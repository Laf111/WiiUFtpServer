/****************************************************************************
  * WiiUFtpServer
  * 2021-12-05:Laf111:V7-0
 ***************************************************************************/
#include <locale.h>
#include <time.h>
#include <sys/param.h>
#include <sys/stat.h>
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
#include <coreinit/thread.h>
#include <coreinit/mcp.h>
#include <coreinit/core.h>
#include <coreinit/time.h>
#include <coreinit/energysaver.h>
#include <coreinit/foreground.h>
#include <coreinit/title.h>
#include <proc_ui/procui.h>
#include <sysapp/launch.h>
#include <nsysnet/_socket.h>

#include <iosuhax_disc_interface.h>
#include <iosuhax_cfw.h>

#include "ftp.h"
#include "virtualpath.h"
#include "net.h"
#include "nandBackup.h"
#include "controllers.h"

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
// PARAMETERS
/****************************************************************************/

// path used to check if a NAND backup exists on SDCard
const char backupCheck[FS_MAX_LOCALPATH_SIZE] = "/vol/storage_sdcard/wiiu/apps/WiiuFtpServer/NandBackup/storage_slc/proc/prefs/nn.xml";

// gamepad inputs
static VPADStatus vpadStatus;

volatile APP_STATE app = APP_STATE_RUNNING;

static int serverSocket = -99;
// iosuhax file descriptor
static int fsaFd = -1;

// mcp_hook_fd
static int mcp_hook_fd = -1;

// lock to limit to one acess at the time for the display method
static bool displayLocked = false;

#ifdef LOG2FILE
// log file
static char logFilePath[FS_MAX_LOCALPATH_SIZE]="wiiu/apps/WiiuFtpServer/WiiuFtpServer.log";
static char previous[FS_MAX_LOCALPATH_SIZE]="wiiu/apps/WiiuFtpServer/WiiuFtpServer.old";
static FILE * logFile=NULL;

// lock to limit to one acess at the time for the loggin method
static bool logLocked = false;

#endif

/****************************************************************************/
// LOCAL FUNCTIONS
/****************************************************************************/

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
    
	while (logLocked == true);
    logLocked = true;
    
    if (logFile == NULL) logFile = fopen(logFilePath, "a");
    if (logFile == NULL) {
        WHBLogPrintf("! ERROR : Unable to reopen log file?");
        WHBLogConsoleDraw();
    } else {
 
	    fprintf(logFile, "%s\n", buf);        

	    fclose(logFile);
	    logFile = NULL;
	}
    
    logLocked = false;
}
#endif

// method to output a message to gamePad and TV (thread safe)
void display(const char *fmt, ...)
{
    char buf[MAXPATHLEN];
	va_list va;
	va_start(va, fmt);
    vsprintf(buf, fmt, va);
    va_end(va);
    
	while (displayLocked == true);
    displayLocked = true;
      
    WHBLogPrintf(buf);    
#ifdef LOG2FILE       
    writeToLog(buf);
#endif 
   
    WHBLogConsoleDraw();
    displayLocked = false;
}

//--------------------------------------------------------------------------
//just to be able to call asyn
void someFunc(IOSError err UNUSED, void *arg) {
    (void)arg;
}

//--------------------------------------------------------------------------
int MCPHookOpen() {
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
void MCPHookClose() {
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
static void cleanUp() {

    cleanup_ftp();
    if (serverSocket >= 0) network_close(serverSocket);

    finalize_network();

    display(" ");
    UmountVirtualDevices();

    IOSUHAX_sdio_disc_interface.shutdown();
    IOSUHAX_usb_disc_interface.shutdown();

    IOSUHAX_FSA_Close(fsaFd);
    if (mcp_hook_fd >= 0) MCPHookClose();
    else IOSUHAX_Close();

    WPADShutdown();
    VPADShutdown();
}

//--------------------------------------------------------------------------
bool AppRunning()
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
uint32_t homeButtonCallback(void *dummy UNUSED)
{
    app = APP_STATE_STOPPING;
	return 0;
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
    
    // TODO : check readonly SDCard (NAND backup + logFile)
    
#ifdef LOG2FILE
    // if log file exists
    if (access(logFilePath, F_OK) == 0) {
        // file exists

        // check if second log file exists
        if (access(previous, F_OK) == 0) {
            // remove previous
            if (remove(previous) != 0) {
                WHBLogPrintf("! ERROR : Failed to remove old log file");
            }
        }

        // backup : log -> previous
        if (rename(logFilePath, previous) != 0) {
            WHBLogPrintf("! ERROR : Failed to rename log file");
            
        }
    }
    WHBLogConsoleDraw();  
#endif    
    
    // *PAD init
    KPADInit();
    WPADInit();
    // enable Universal Remote Console Communication Protocol
    WPADEnableURCC(1);
    // enable Wiimote
    WPADEnableWiiRemote(1);

    ProcUIInit(&OSSavesDone_ReadyToRelease);
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

    display(" -=============================-");
    display("|    %s     |", VERSION_STRING);
    display(" -=============================-");
    display("[Laf111/2021-12]");
    display(" ");


    // Get OS time and save it in ftp static variable
    OSCalendarTime osDateTime;
    struct tm tmTime;
    OSTicksToCalendarTime(OSGetTime(), &osDateTime);

    // tm_mon is in [0,30]
    int mounth=osDateTime.tm_mon+1;
    display("Wii-U date (GMT) : %02d/%02d/%04d %02d:%02d:%02d",
            osDateTime.tm_mday, mounth, osDateTime.tm_year,
            osDateTime.tm_hour, osDateTime.tm_min, osDateTime.tm_sec);      
            
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
    // Check if a CFW is active
    IOSUHAX_CFW_Family cfw = IOSUHAX_CFW_GetFamily();
    if (cfw == 0) {
        display("! ERROR : No running CFW detected");
        goto exit;
    }

    int res = IOSUHAX_Open(NULL);
    if (res < 0)
        res = MCPHookOpen();
    if (res < 0) {
        display("! ERROR : IOSUHAX_Open failed.");
        goto exit;
    }

    fsaFd = IOSUHAX_FSA_Open();
    if (fsaFd < 0) {
        display("! ERROR : IOSUHAX_FSA_Open failed.");
        if (mcp_hook_fd >= 0) MCPHookClose();
        else IOSUHAX_Close();
        goto exit;
    }

    setFsaFd(fsaFd);

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
        display("   > DOWN, disable/toggle auto shutdown (currently enabled)");
    }
    display("   > UP, disable/toggle verbose mode (OFF by default)");
    display("   > A, for only USB and SDCard (default after timeout)");
    display("   > B, mount ALL paths (use at your own risk)");
    if (!autoShutDown) {
        display(" ");
    }

    bool buttonPressed = false;
    bool mountMlc=false;
    bool verbose=false;
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
            if (verbose) {
                display("(verbose mode OFF)");
                verbose=false;
            } else {
                display("(verbose mode ON)");
                verbose=true;
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
        cpt=cpt+1;

    }

    // verbose mode (disabled by default)
    if (verbose) setVerboseMode(verbose);
    display(" ");

    int nbDrives=MountVirtualDevices(fsaFd, mountMlc);
    if (nbDrives == 0) {
        display("! ERROR : No virtual devices mounted !");
        goto exit;
    }
    display(" ");
    display("Starting network and create server....");
    display(" ");
    
    // if mountMlc, check that a NAND backup exists, ask to create one otherwise
    setFsaFdCopyFiles(fsaFd);
    int backupExist = checkEntry(backupCheck);
    if (mountMlc) {
        if (backupExist != 1) {
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
            display("  in order to start WiiuFtpServer");
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
                display("the Wii-U network in order to start WiiuFtpServer");
                display("");
                display("It is highly recommended to create a FULL backup");
                display("on your own");
            }
            
            display("");                
            display("Press A or B button to continue");
            display("");                
            readUserAnswer(&vpadStatus);
            cls();
        }
    }
    
    /*--------------------------------------------------------------------------*/
    /* Starting Network                                                         */
    /*--------------------------------------------------------------------------*/
    
    initialize_network();
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
        if (backupExist != 1) {
            display("Do you need to restore the partial NAND backup?");
            display(" ");
            display("Press A for YES, B for NO ");
            display("");
            if (readUserAnswer(&vpadStatus)) {
                display("NAND backup will be restored, please confirm");
                display("");
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
    bool userExitRequest = false;
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

	OSSleepTicks(OSMillisecondsToTicks(1000));
	display(" ");
	display(" ");
#ifdef LOG2FILE
    if (logFile != NULL) fclose(logFile);
#endif
    if (isChannel()) {
        SYSLaunchMenu();
        // loop to exit to the Wii-U Menu
		while (app != APP_STATE_STOPPED) {
	        AppRunning();
	    }
    }

    WHBLogConsoleFree();
    WHBProcShutdown();
    WHBLogUdpDeinit();
    
    ProcUIShutdown();
    return EXIT_SUCCESS;
}