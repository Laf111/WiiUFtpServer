/****************************************************************************
  * WiiUFtpServer
  * 2021-12-05:Laf111:V7-0
 ***************************************************************************/
#include <locale.h>
#include <time.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <malloc.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>

#include <whb/log_udp.h>
#include <whb/proc.h>
#include <whb/libmanager.h>
#include <coreinit/dynload.h>
#include <coreinit/ios.h>
#include <coreinit/thread.h>
#include <coreinit/mcp.h>
#include <coreinit/core.h>
#include <coreinit/time.h>
#include <coreinit/energysaver.h>
#include <coreinit/foreground.h>
#include <coreinit/filesystem.h>
#include <coreinit/title.h>
#include <proc_ui/procui.h>
#include <sysapp/launch.h>
#include <nsysnet/_socket.h>

#include <fat.h>

#include <iosuhax.h>
#include <iosuhax_disc_interface.h>

#include "ftp.h"
#include "virtualpath.h"
#include "net.h"
#include "nandBackup.h"
#include "controllers.h"
#include "spinlock.h"

// return code
#define EXIT_SUCCESS        0

typedef enum
{
	APP_STATE_STOPPING = 0,
	APP_STATE_STOPPED,
	APP_STATE_RUNNING,
	APP_STATE_BACKGROUND,
	APP_STATE_RETURNING,

} APP_STATE;

/****************************************************************************/
// STATIC VARS
/****************************************************************************/

// Wii-U date (GMT) %02d/%02d/%04d %02d:%02d:%02d
static char sessionDate[40] = "";

// path used to check if a NAND backup exists on SDCard
static const char backupCheck[FS_MAX_LOCALPATH_SIZE] = "sd:/wiiu/apps/WiiUFtpServer/NandBackup/storage_slc/proc/prefs/nn.xml";

// gamepad inputs
static VPADStatus vpadStatus;

static volatile APP_STATE app = APP_STATE_RUNNING;

static int serverSocket = -99;

// mcp_hook_fd
static int mcp_hook_fd = -1;

// MLC vol mounted flag
static bool mountMlc = false;

// lock to limit to one access at a time for the display method
static spinlock displayLock = false;

#ifdef LOG2FILE
    // log files
	static char logFilePath[FS_MAX_LOCALPATH_SIZE]="wiiu/apps/WiiUFtpServer/WiiUFtpServer.log";
    static const char previousLogFilePath[FS_MAX_LOCALPATH_SIZE] = "wiiu/apps/WiiUFtpServer/WiiUFtpServer.old";
    static FILE * logFile = NULL;

    // lock to limit to one access at a time for the loggin method
    static spinlock logLock = false;
#endif


// 2 level CRC32 SFV report 
static char sfvFilePath[FS_MAX_LOCALPATH_SIZE] = "sd:/wiiu/apps/WiiUFtpServer/CrcChecker/WiiUFtpServer_crc32_report.sfv";
static const char previousSfvFilePath[FS_MAX_LOCALPATH_SIZE] = "sd:/wiiu/apps/WiiUFtpServer/CrcChecker/WiiUFtpServer_crc32_report.old";
static FILE * sfvFile = NULL;

// lock to limit to one access at a time for the CRC32 SFV file
static spinlock sfvLock = false;
    
    
/****************************************************************************/
// GLOBAL VARS
/****************************************************************************/
// iosuhax file descriptor
int fsaFd = -1;

// verbose mode for server
bool verboseMode = false;

// flag to enable CRC32 calculation
bool calculateCrc32 = false;


/****************************************************************************/
// GLOBAL FUNCTIONS
/****************************************************************************/

//--------------------------------------------------------------------------
void lockDisplay() {
    spinLock(displayLock);
}

void unlockDisplay() {
    spinReleaseLock(displayLock);
}

//--------------------------------------------------------------------------
#ifdef LOG2FILE
    // method to output a message to gamePad and TV (thread safe)
    void writeToLog(const char *fmt, ...)
    {
        char buf[MAXPATHLEN];
        va_list va;
        va_start(va, fmt);
        vsprintf(buf, fmt, va);
        va_end(va);
        
        spinLock(logLock);
        
        if (logFile == NULL) logFile = fopen(logFilePath, "a");
        if (logFile == NULL) {
            WHBLogPrintf("! ERROR : Unable to reopen log file?");
            OSSleepTicks(OSMillisecondsToTicks(5000));            
        } else {
     
            fprintf(logFile, "%s\n", buf);
            fclose(logFile);
            logFile = NULL;
        }    
        spinReleaseLock(logLock);
    }        
#endif

//--------------------------------------------------------------------------
// method to output a message to gamePad and TV (thread safe)
void display(const char *fmt, ...)
{
    char buf[MAXPATHLEN];
	va_list va;
	va_start(va, fmt);
    vsprintf(buf, fmt, va);
    va_end(va);
    
	spinLock(displayLock);
      
    WHBLogPrintf(buf);    
#ifdef LOG2FILE       
    writeToLog(buf);
#endif    
    WHBLogConsoleDraw();  

    spinReleaseLock(displayLock);
}

//--------------------------------------------------------------------------
void writeCRC32(const char way, const char *cwd, const char *name, const int crc32)
{
    spinLock(sfvLock);
    
    if (sfvFile == NULL) sfvFile = fopen(sfvFilePath, "a");
    if (sfvFile == NULL) {
        display("! ERROR : Unable to reopen crc report file?");
        OSSleepTicks(OSMillisecondsToTicks(5000));            
    } else {
        
        fprintf(sfvFile, "%c'%s%s' %08" PRIX32 "\n", way, cwd, name, crc32);            
        fclose(sfvFile);
        sfvFile = NULL;

    }    
    spinReleaseLock(sfvLock);
}



/****************************************************************************/
// LOCAL FUNCTIONS
/****************************************************************************/

//--------------------------------------------------------------------------
//just to be able to call asyn
static void someFunc(IMError err UNUSED, void *arg) {
    (void)arg;
}

//--------------------------------------------------------------------------
static int MCPHookOpen() {
    //take over mcp thread
    mcp_hook_fd = MCP_Open();
    if (mcp_hook_fd < 0) return -1;
    IOS_IoctlAsync(mcp_hook_fd, 0x62, (void*)0, 0, (void*)0, 0, someFunc, (void*)0);
    //let wupserver start up
    OSSleepTicks(OSMillisecondsToTicks(1000));
    if (IOSUHAX_Open("/dev/mcp") < 0) {
        MCP_Close(mcp_hook_fd);
        mcp_hook_fd = -1;
        return -1;
    }
    return 0;
}

//--------------------------------------------------------------------------
static void MCPHookClose() {
    if (mcp_hook_fd < 0) return;
    //close down wupserver, return control to mcp
    IOSUHAX_Close();
    //wait for mcp to return
    OSSleepTicks(OSMillisecondsToTicks(1000));
    MCP_Close(mcp_hook_fd);
    mcp_hook_fd = -1;
}

//--------------------------------------------------------------------------
static void cls() {
    for (int i=0; i<20; i++) display(" ");

}

//--------------------------------------------------------------------------
static bool isChannel() {
    return OSGetTitleID() == 0x0005000010050421;
}

//--------------------------------------------------------------------------
static void displayCrcWarning() {
    cls();
    display("!!!!!!!!!!!!!!!!!!!!!!!! WARNING !!!!!!!!!!!!!!!!!!!!!!!!!");
    display("");
    display("You have chosen to mount system paths, and CRC32 calculation");
    display("is disabled!");
    display("");
    display("If you transfer critical files (such as system ones), you'd");
    display("better verify the integrity of the files transferred using");
    display("a CRC check espcially if you're using WIFI and not Ethernet");
    display("");
    display("You can toggle it at anytime during the session with 'X'");
    display("");
    display("Then use the CrcChecker to check your files afterward.");
    display("(available in WiiUFtpServer HBL app subfolder on the SDcard)");
    display("");
    display("");

    display("Press A or B button to continue (timeout in 10 sec)");
	
    bool buttonPressed = false;
    int cpt = 0;
    while (!buttonPressed && (cpt < 300))
    {
        listenControlerEvent(&vpadStatus);
        
        // check A/B button pressed and/or hold
        if ( ((vpadStatus.trigger | vpadStatus.hold) & VPAD_BUTTON_A) | \
             ((vpadStatus.trigger | vpadStatus.hold) & VPAD_BUTTON_B) ) buttonPressed = true;
			 
        OSSleepTicks(OSMillisecondsToTicks(1));
		cpt+=1;
	}	
    cls();
}

//--------------------------------------------------------------------------
static void cleanUp() {

    cleanup_ftp();
    if (serverSocket >= 0) network_close(serverSocket);

    finalize_network();

    display(" ");
    UmountVirtualDevices();

	OSSleepTicks(OSMillisecondsToTicks(1000));
    
    IOSUHAX_FSA_Close(fsaFd);
    if (mcp_hook_fd >= 0) MCPHookClose();
    else IOSUHAX_Close();

    WPADShutdown();
    VPADShutdown();
}

//--------------------------------------------------------------------------
static bool AppRunning()
{
	if(OSIsMainCore() && app != APP_STATE_STOPPED)
	{
		switch(ProcUIProcessMessages(true))
		{
			case PROCUI_STATUS_EXITING:
				// Being closed, prepare to exit
				app = APP_STATE_STOPPED;
				break;
			case PROCUI_STATUS_RELEASE_FOREGROUND:
				// Free up MEM1 to next foreground app, deinit screen, etc.
				ProcUIDrawDoneRelease();
				if(app != APP_STATE_STOPPING)
					app = APP_STATE_BACKGROUND;
				break;
			case PROCUI_STATUS_IN_FOREGROUND:
				// Executed while app is in foreground
				if(app == APP_STATE_STOPPING)
					break;
				if(app == APP_STATE_BACKGROUND)
				{
					app = APP_STATE_RETURNING;
				}
				else
					app = APP_STATE_RUNNING;

				break;
			case PROCUI_STATUS_IN_BACKGROUND:
				if(app != APP_STATE_STOPPING)
					app = APP_STATE_BACKGROUND;
				break;
		}
	}

	return app;
}

//--------------------------------------------------------------------------
static uint32_t homeButtonCallback(void *dummy UNUSED)
{
    app = APP_STATE_STOPPING;
	return 0;
}

//--------------------------------------------------------------------------
static void writeSfvHeader() {
    
    if (sfvFile == NULL) sfvFile = fopen(sfvFilePath, "w");
    if (sfvFile == NULL) {
        display("! ERROR : Unable to reopen crc report file?");
        OSSleepTicks(OSMillisecondsToTicks(5000));            
    } else {
        
        char sfvHeader[128*5] = "";
        strcpy(sfvHeader, ";============================================================================================\n"); 
        strcat(sfvHeader, "; WiiUFtpServer CRC-32 report of FTP session on "); 
        strcat(sfvHeader, sessionDate);
        strcat(sfvHeader, "\n");
        strcat(sfvHeader, ";--------------------------------------------------------------------------------------------\n"); 
        strcat(sfvHeader, "; prefix '<' means file was received by server (client upload), '>' for sent (client download)\n"); 
        strcat(sfvHeader, ";============================================================================================\n"); 
        fprintf(sfvFile, "%s", sfvHeader);            
        fclose(sfvFile);
        sfvFile = NULL;
    }    
}



/****************************************************************************/
// MAIN PROGRAM
/****************************************************************************/
int main()
{
    setlocale(LC_ALL, "");
        
    // Console init
    WHBLogUdpInit();
    WHBProcInit();
    WHBLogConsoleInit();
    
    // enable Universal Remote Console Communication Protocol
    WPADEnableURCC(1);
    // enable Wiimote
    WPADEnableWiiRemote(1);
    
	ProcUIRegisterCallback(PROCUI_CALLBACK_HOME_BUTTON_DENIED, &homeButtonCallback, NULL, 100);
	OSEnableHomeButtonMenu(false);

    // get the energy saver mode status
    uint32_t autoShutDown=0;
    // from settings
    IMIsAPDEnabledBySysSettings(&autoShutDown);

    // get the current thread (on CPU1)
    OSThread *thread = NULL;
    thread = OSGetCurrentThread();

    if (thread != NULL) {
        // set the name
        OSSetThreadName(thread, "WiiUFtpServer thread on CPU1");

        // set a priority to 0
        OSSetThreadPriority(thread, 0);
    }

    #ifdef LOG2FILE
        
        // if log file exists
        if (access(logFilePath, F_OK) == 0) {
            // file exists

            // check if second log file exists
            if (access(previousLogFilePath, F_OK) == 0) {
                // remove previousLogFilePath
                if (remove(previousLogFilePath) != 0) {
                    WHBLogPrintf("! ERROR : Failed to remove old log file");
                }
            }

            // backup : log -> previous
            if (rename(logFilePath, previousLogFilePath) != 0) {
                WHBLogPrintf("! ERROR : Failed to rename log file");
                
            }
            WHBLogConsoleDraw();  
        }
        
    #endif    
    
    display(" -=============================-");
    display("|    %s     |", VERSION_STRING);
    display(" -=============================-");
    display("[Laf111/2022-01]");
    display(" ");


    // Get OS time and save it in ftp static variable
    OSCalendarTime osDateTime;
    struct tm tmTime;
    OSTicksToCalendarTime(OSGetTime(), &osDateTime);

    // tm_mon is in [0,30]
    int mounth=osDateTime.tm_mon+1;
    
    sprintf(sessionDate, "%02d/%02d/%04d %02d:%02d:%02d",
            osDateTime.tm_mday, mounth, osDateTime.tm_year,
            osDateTime.tm_hour, osDateTime.tm_min, osDateTime.tm_sec);
            
    display("Wii-U date (GMT) : %s",sessionDate);      
            
    tmTime.tm_sec   =   osDateTime.tm_sec;
    tmTime.tm_min   =   osDateTime.tm_min;
    tmTime.tm_hour  =   osDateTime.tm_hour;
    tmTime.tm_mday  =   osDateTime.tm_mday;
    tmTime.tm_mon   =   osDateTime.tm_mon;
    // tm struct : year is based from 1900
    tmTime.tm_year  =   osDateTime.tm_year-1900;
    tmTime.tm_wday  =   osDateTime.tm_wday;
    tmTime.tm_yday  =   osDateTime.tm_yday;

    // save GMT OS Time in ftp.c
    setOsTime(&tmTime);
	
	OSSleepTicks(OSMillisecondsToTicks(1200));
    display(" ");
    display(" ");
    display(" ");
    display(" ");

    /*--------------------------------------------------------------------------*/
    /* IOSUHAX operations and mounting devices                                  */
    /*--------------------------------------------------------------------------*/

    int res = IOSUHAX_Open(NULL);
    if (res < 0)
        res = MCPHookOpen();
    if (res < 0) {
        display("! ERROR : IOSUHAX_Open failed.");
        goto exitCFW;
    }
    
    fsaFd = IOSUHAX_FSA_Open();
    if (fsaFd < 0) {
        display("! ERROR : IOSUHAX_FSA_Open failed.");
        if (mcp_hook_fd >= 0) MCPHookClose();
        else IOSUHAX_Close();
        goto exitCFW;
    }
        
    // *PAD init
    KPADInit();
    WPADInit();
    
#ifdef CHECK_CONTROLLER
    // Check your controller
    display(" ");
    display("Check the current controller : ");
	display(" ");
    if (!checkController(&vpadStatus)) {
        display("This controller is not supported, exiting in 10 sec");
        OSSleepTicks(OSMillisecondsToTicks(10000));
        
        goto exit;
    }
#endif

    cls();
    display("Please PRESS : (timeout in 10 sec)");
	display(" ");
    if (autoShutDown) {
        display("   > DOWN, toggle auto shutdown (currently enabled)");
    }
    display("   > UP, toggle verbose mode (OFF by default)");
    display("   > A, for only USB and SDCard (default after timeout)");
    display("   > B, mount ALL paths");
    display("   > X, toogle CRC32-C calculation (OFF by default)");
    if (!autoShutDown) {
        display(" ");
    }

    bool buttonPressed = false;
    int cpt=0;
    while (!buttonPressed && (cpt < 400))
    {
        listenControlerEvent(&vpadStatus);

        // check button pressed and/or hold
        if ((vpadStatus.trigger | vpadStatus.hold) & VPAD_BUTTON_A) buttonPressed = true;
        if ((vpadStatus.trigger | vpadStatus.hold) & VPAD_BUTTON_B) {
            mountMlc = true;
            buttonPressed = true;
        }
        if ((vpadStatus.trigger | vpadStatus.hold) & VPAD_BUTTON_UP) {
            if (verboseMode) {
                display("(verbose mode OFF)");
                verboseMode = false;
            } else {
                display("(verbose mode ON)");
                verboseMode = true;
            }
            OSSleepTicks(OSMillisecondsToTicks(500));
        }
        if ((vpadStatus.trigger | vpadStatus.hold) & VPAD_BUTTON_X) {
            if (calculateCrc32) {
                display("(disable CRC32 computation)");
                calculateCrc32 = false;
            } else {
                display("(enable CRC32 computation)");
                calculateCrc32 = true;
            }
            OSSleepTicks(OSMillisecondsToTicks(500));
        }
        if ((vpadStatus.trigger | vpadStatus.hold) & VPAD_BUTTON_DOWN) {
            if (autoShutDown) {
                display("(auto-shutdown OFF)");
                IMDisableAPD(); // Disable auto-shutdown feature
				autoShutDown = 0;
            } else {
                display("(auto-shutdown ON)");
                IMEnableAPD(); // Disable auto-shutdown feature
				autoShutDown = 1;
            }
            OSSleepTicks(OSMillisecondsToTicks(500));
        }

        OSSleepTicks(OSMillisecondsToTicks(1));
        cpt = cpt+1;

    }

    display(" ");
    int nbDrives=MountVirtualDevices(mountMlc);
    if (nbDrives == 0) {
        display("! ERROR : No virtual devices mounted !");
        goto exit;
    }

    // if SFV file exists
    if (access(sfvFilePath, F_OK) == 0) {
        // file exists

        // check if second SFV file exists
        if (access(previousSfvFilePath, F_OK) == 0) {
            // remove previousLogFilePath
            if (remove(previousSfvFilePath) != 0) {
                display("! ERROR : Failed to remove old SFV file");
            }
        }

        // backup : log -> previousLogFilePath
        if (rename(sfvFilePath, previousSfvFilePath) != 0) {
            display("! ERROR : Failed to rename SFV file");
        }
    }
        
    display(" ");
    display("Starting network and create server...");
    display(" ");

    // if mountMlc, check that a NAND backup exists, ask to create one otherwise
    bool backupExist = (access(backupCheck, F_OK) == 0);
    if (mountMlc) {
        if (!backupExist) {
            fatUnmount("sd");            
            IOSUHAX_FSA_Mount(fsaFd, "/dev/sdcard01", "/vol/storage_sdcard", 2, (void*)0, 0);

            mount_fs("storage_sdcard", fsaFd, NULL, "/vol/storage_sdcard");
            
            cls();
            display("!!!!!!!!!!!!!!!!!!!!!!!! WARNING !!!!!!!!!!!!!!!!!!!!!!!!!");
            display(" ");
            display("No NAND backup was found !");
            display(" ");
            display("There's always a risk of brick");
            display("(specially if you edit system files from your FTP client)");
            display(" ");
            display(" ");
            display("Create a complete system (press A) or partial (B) backup?");
            display(" ");
            display("- COMPLETE system requires 500MB free space on SD card !");
            display("- PARTIAL one will be only used to unbrick the Wii-U network");
            display("  in order to start WiiUFtpServer");
            display(" ");
            if (readUserAnswer(&vpadStatus)) {
                display("Creating FULL NAND backup...");
                createNandBackup(1);
                display("");
                display("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
            } else {
                display("Creating partial NAND backup...");
                createNandBackup(0);
                display("");
                display("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
                display("This backup is only a PARTIAL one used to unbrick");
                display("the Wii-U network in order to start WiiUFtpServer");
                display("");
                display("It is highly recommended to create a FULL backup");
                display("on your own");
            }

            display("");
            display("Press A or B button to continue");
            display("");
            readUserAnswer(&vpadStatus);
            cls();
            unmount_fs("storage_sdcard");
            IOSUHAX_FSA_FlushVolume(fsaFd, "/vol/storage_sdcard");
            
            fatMountSimple("sd", &IOSUHAX_sdio_disc_interface);
            VirtualMountDevice("sd:/");            
        }
        if (!calculateCrc32) displayCrcWarning();
    }

    /*--------------------------------------------------------------------------*/
    /* Starting Network                                                         */
    /*--------------------------------------------------------------------------*/

    if (initialize_network() < 0) {
        display("! ERROR : when initializing network");
        OSSleepTicks(OSMillisecondsToTicks(5000));
        goto exit;
    }
    bool networkDown = false;

#ifdef LOG2FILE
    writeToLog("Network initialized");
#endif
    /*--------------------------------------------------------------------------*/
    /* Create FTP server                                                        */
    /*--------------------------------------------------------------------------*/
    serverSocket = create_server(FTP_PORT);
    if (serverSocket < 0) display("! ERROR : when creating server");

    // check that network availability
    struct in_addr addr;
	addr.s_addr = network_gethostip();

    if (strcmp(inet_ntoa(addr),"0.0.0.0") == 0 && !isChannel()) {
        networkDown = true;
        cls();
        display(" ");
        display("! ERROR : network is OFF on the wii-U, FTP is impossible");
        display(" ");
        if (backupExist) {
            display("If you have already checked the Network connection to the WIi-U");
            display("Do you want to restore the partial NAND backup?");
            display(" ");
            display("Press A for YES, B for NO ");
            display("");
            if (readUserAnswer(&vpadStatus)) {
                display("NAND backup will be restored, please confirm");
                display("");
                fatUnmount("sd");            
                IOSUHAX_FSA_Mount(fsaFd, "/dev/sdcard01", "/vol/storage_sdcard", 2, (void*)0, 0);

                mount_fs("storage_sdcard", fsaFd, NULL, "/vol/storage_sdcard");                
                if (readUserAnswer(&vpadStatus)) restoreNandBackup();
                display("");
                // reboot
                display("Shutdowning...");
                OSSleepTicks(OSMillisecondsToTicks(2000));
                OSDynLoad_Module coreinitHandle = NULL;
                int32_t (*OSShutdown)(int32_t status);
                OSDynLoad_Acquire("coreinit.rpl", &coreinitHandle);
                OSDynLoad_FindExport(coreinitHandle, FALSE, "OSShutdown", (void **)&OSShutdown);
                OSDynLoad_Release(coreinitHandle);
                OSShutdown(1);
                goto exit;
            }
        } else {
            display("! ERROR : Can't start the FTP server, exiting");
        }
        display("");
    }

    // check network and server creation, wait for user aknowledgement
    if (networkDown | (serverSocket < 0)) {
        display("");
        display("Press A or B button to continue");
        display("");
        readUserAnswer(&vpadStatus);
    }

#ifdef LOG2FILE
    writeToLog("Server created, adress = %s", inet_ntoa(addr));
#endif

    if (autoShutDown) display(" (auto shutdown is ON , use DOWN button to toggle state)");
    else display(" (auto shutdown is OFF, use DOWN button to toggle state)");

    bool sflHeaderWritten = false;
    // write Sfv header
    if (calculateCrc32) {
        writeSfvHeader();
        sflHeaderWritten = true;
    }
    
    bool userExitRequest = false;
    cpt=0;
    while (!networkDown && !userExitRequest)
    {
        networkDown = process_ftp_events();
        if (networkDown) {
            display("! ERROR : FTP server stopped !");
            display("! ERROR : errno = %d (%s)", errno, strerror(errno));
            display("");
            display(" > Press A or B to exit...");
            display("");

            readUserAnswer(&vpadStatus);
            break;
        }

        listenControlerEvent(&vpadStatus);

        // add the possibility to switch auto-shutdown during the session
        if ((vpadStatus.trigger | vpadStatus.hold) & VPAD_BUTTON_DOWN) {
            if (autoShutDown) {
                display("- auto-shutdown OFF");
                IMDisableAPD(); // Disable auto-shutdown feature
				autoShutDown = 0;
            } else {
                display("- auto-shutdown ON");
                IMEnableAPD(); // Disable auto-shutdown feature
				autoShutDown = 1;
            }
            OSSleepTicks(OSMillisecondsToTicks(1000));
        }

        if ((vpadStatus.trigger | vpadStatus.hold) & VPAD_BUTTON_X) {
            if (calculateCrc32) {
                display("(disable CRC32 computation)");
                calculateCrc32 = false;
            } else {
                display("(enable CRC32 computation)");
                calculateCrc32 = true;
                // write Sfv header
                if (!sflHeaderWritten) {
                    writeSfvHeader();
                    sflHeaderWritten = true;
                }
            }
            OSSleepTicks(OSMillisecondsToTicks(500));
        }
        
        // check button pressed and/or hold
        if ((vpadStatus.trigger | vpadStatus.hold) & VPAD_BUTTON_HOME) userExitRequest = true;
        if (userExitRequest) break;
        
    }

    /*--------------------------------------------------------------------------*/
    /* Cleanup and exit                                                         */
    /*--------------------------------------------------------------------------*/
    display("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
    display(" ");
    if (!isChannel())
        display("Stopping server and return to HBL Menu...");
    else
        display("Stopping server and return to Wii-U Menu...");
    display(" ");

exit:

	cleanUp();
    
#ifdef LOG2FILE
    if (logFile != NULL) fclose(logFile);
#endif
    if (sfvFile != NULL) fclose(sfvFile);
    
    if (isChannel()) {
        SYSLaunchMenu();
        // loop to exit to the Wii-U Menu
		while (app != APP_STATE_STOPPED) {
	        AppRunning();
	    }
    }
    
exitCFW:

    WHBLogConsoleFree();
    WHBProcShutdown();
    WHBLogUdpDeinit();
    
    ProcUIShutdown();
    return EXIT_SUCCESS;
}
