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
#include <padscore/kpad.h>
#include <padscore/wpad.h>
#include <proc_ui/procui.h>
#include <sysapp/launch.h>
#include <vpad/input.h>
#include <time.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h> 

#include "ftp.h"
#include "virtualpath.h"
#include "net.h"
#include "iosuhax_disc_interface.h"
#include "iosuhax_cfw.h"
#include "receivedFiles.h"

// return code
#define EXIT_SUCCESS        0
#define FTP_PORT            21

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
volatile APP_STATE app = APP_STATE_RUNNING;

static int serverSocket = -99;
// iosuhax file descriptor
static int fsaFd = -1;

// mcp_hook_fd
static int mcp_hook_fd = -1;


//static OSDynLoad_Module coreinitHandle = NULL;
//static int32_t (*OSShutdown)(int32_t status);

// lock to make thread safe the display method
static bool displayLock=false;
// method to output to gamePad and TV (thread safe)
void logLine(const char *line)
{
    while (displayLock) OSSleepTicks(OSMillisecondsToTicks(100));
    
    // set the lock
    displayLock=true;
    
    WHBLogPrintf("%s", line);
    WHBLogConsoleDraw();
    
    // unset the lock
    displayLock=false;
}    
/****************************************************************************/
// LOCAL FUNCTIONS
/****************************************************************************/

//--------------------------------------------------------------------------
//just to be able to call async
void someFunc(IOSError err, void *arg) {
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

static bool isChannel() {
    return OSGetTitleID() == 0x0005000010050421;
}

static void cleanUp() {

    cleanup_ftp();    
    if (serverSocket >= 0) network_close(serverSocket);
    
    finalize_network();

    WHBLogPrintf(" "); 
    UmountVirtualDevices();
    
    IOSUHAX_sdio_disc_interface.shutdown();
    IOSUHAX_usb_disc_interface.shutdown();
    
    IOSUHAX_FSA_Close(fsaFd);
    if (mcp_hook_fd >= 0) MCPHookClose();
    else IOSUHAX_Close();
		
	if (!isChannel()) {
		WHBDeinitializeSocketLibrary();
	    VPADShutdown();
	}
}

bool AppRunning()
{
	if(OSIsMainCore() && app != APP_STATE_STOPPED)
	{
		switch(ProcUIProcessMessages(true))
		{
			case PROCUI_STATUS_EXITING:
				// Being closed, deinit, free, and prepare to exit
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

uint32_t homeButtonCallback(void *dummy)
{
    app = APP_STATE_STOPPING;
	return 0;
}

//--------------------------------------------------------------------------
/****************************************************************************/
// MAIN PROGRAM
/****************************************************************************/
int main()
{
        
    // Console init
    WHBLogUdpInit();
    WHBProcInit();
    WHBLogConsoleInit();
    
    // PAD init
    KPADInit();
    
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

        // set a priority to 1
        OSSetThreadPriority(thread, 1);
    }
    
/*     
    if (isChannel()) {
        // Initialize OSShutdown and OSForceFullRelaunch functions
        OSDynLoad_Acquire("coreinit.rpl", &coreinitHandle);
        OSDynLoad_FindExport(coreinitHandle, FALSE, "OSShutdown", (void **)&OSShutdown);
        OSDynLoad_Release(coreinitHandle);    
    }
 */    
    WHBLogPrintf(" -=============================-\n");
    WHBLogPrintf("|    %s     |\n", VERSION_STRING);
    WHBLogPrintf(" -=============================-\n");
    WHBLogPrintf("[Laf111/2021-09]");
    WHBLogPrintf(" ");
    WHBLogConsoleDraw();
    
    // Get OS time and save it in ftp static variable 
    OSCalendarTime osDateTime;
    struct tm tmTime;
    OSTicksToCalendarTime(OSGetTime(), &osDateTime);

    // tm_mon is in [0,30]
    int mounth=osDateTime.tm_mon+1;
    WHBLogPrintf("Wii-U date (GMT) : %02d/%02d/%04d %02d:%02d:%02d",
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
    logLine(" ");
    logLine(" ");
    logLine(" ");
    WHBLogConsoleDraw();
    logLine(" ");

    /*--------------------------------------------------------------------------*/
    /* IOSUHAX operations and mounting devices                                  */
    /*--------------------------------------------------------------------------*/
    // Check if a CFW is active
    IOSUHAX_CFW_Family cfw = IOSUHAX_CFW_GetFamily();
    if (cfw == 0) {
        WHBLogPrintf("! ERROR : No running CFW detected");
        goto exit;
    }
    
    int res = IOSUHAX_Open(NULL);
    if (res < 0)
        res = MCPHookOpen();
    if (res < 0) {
        WHBLogPrintf("! ERROR : IOSUHAX_Open failed.");
        goto exit;        
    }
    
    fsaFd = IOSUHAX_FSA_Open();
    if (fsaFd < 0) {
        WHBLogPrintf("! ERROR : IOSUHAX_FSA_Open failed.");
        if (mcp_hook_fd >= 0) MCPHookClose();
        else IOSUHAX_Close();
        goto exit;
    }
    
    setFsaFd(fsaFd);
	
    logLine("Please PRESS : (timeout in 10 sec)");
	logLine(" ");	
    if (autoShutDown) {
        logLine("   > DOWN, disable/toggle auto shutdown (currently enabled)");
    }
    logLine("   > UP, disable/toggle verbose mode (OFF by default)");
    logLine("   > A, for only USB and SDCard (default after timeout)");
    logLine("   > B, mount ALL paths (use at your own risk)");
    if (!autoShutDown) {
        logLine(" ");
    }
    
    // gamepad inputs (needed for channel, WHBProc HOME_BUTTON event is not enought)
    VPADStatus vpadStatus;
    VPADReadError vpadError = -1;

    KPADStatus kpadStatus;
    VPADReadError kpadError = -1;    
    
    bool buttonPressed = false;
    bool mountMlc=false;
    bool verbose=false;    
    int cpt=0;
    while (!buttonPressed && (cpt < 10000))
    {

        VPADRead(0, &vpadStatus, 1, &vpadError);
        if ((vpadStatus.trigger | vpadStatus.hold) & VPAD_BUTTON_A) buttonPressed = true;
        if ((vpadStatus.trigger | vpadStatus.hold) & VPAD_BUTTON_B) {
            mountMlc = true;
            buttonPressed = true;
        }
        if ((vpadStatus.trigger | vpadStatus.hold) & VPAD_BUTTON_UP) {
            if (verbose) {
                logLine("(verbose mode OFF)");
                verbose=false;
            } else {
                logLine("(verbose mode ON)");
                verbose=true;
            }
            OSSleepTicks(OSMillisecondsToTicks(500));
        }
        if ((vpadStatus.trigger | vpadStatus.hold) & VPAD_BUTTON_DOWN) {
            if (autoShutDown) {
                logLine("(auto-shutdown OFF)");
                IMDisableAPD(); // Disable auto-shutdown feature
            } else {
                logLine("(auto-shutdown ON)");
                IMEnableAPD(); // Disable auto-shutdown feature
            }
            OSSleepTicks(OSMillisecondsToTicks(500));
        }

        // tr to exit from here
        if (buttonPressed) break;
        
         for (int i = 0; i < 4; i++)
        {
            uint32_t controllerType;
            // check if the controller is connected
            if (WPADProbe(i, &controllerType) != 0)
                continue;

           KPADRead(i, &kpadStatus, kpadError);

            switch (controllerType)
            {
                case WPAD_EXT_CORE:
                    if ((kpadStatus.trigger | kpadStatus.hold) & WPAD_BUTTON_B) {
                        mountMlc = true;
                        buttonPressed = true;
                    }
                    if ((kpadStatus.trigger | kpadStatus.hold) & WPAD_BUTTON_A) {
                        buttonPressed = true;
                    }
                    if ((kpadStatus.trigger | kpadStatus.hold) & WPAD_BUTTON_UP) {
                        if (verbose) {
                            logLine("(verbose mode OFF)");
                            verbose=false;
                        } else {
                            logLine("(verbose mode ON)");
                            verbose=true;
                        }
                        OSSleepTicks(OSMillisecondsToTicks(500));
                    }
                    if ((kpadStatus.trigger | kpadStatus.hold) & WPAD_BUTTON_DOWN) {
                        if (autoShutDown) {
                            logLine("(auto-shutdown feature OFF)");
                            IMDisableAPD(); // Disable auto-shutdown feature
                        } else {
                            logLine("(auto-shutdown feature ON)");
                            IMEnableAPD(); // Enable auto-shutdown feature
                        }
                        OSSleepTicks(OSMillisecondsToTicks(500));
                    }
                    break;
                case WPAD_EXT_CLASSIC:
                    if ((kpadStatus.trigger | kpadStatus.hold) & WPAD_CLASSIC_BUTTON_B) {
                        mountMlc = true;
                        buttonPressed = true;
                    }
                    if ((kpadStatus.trigger | kpadStatus.hold) & WPAD_CLASSIC_BUTTON_A)
                        buttonPressed = true;
                    if ((kpadStatus.trigger | kpadStatus.hold) & WPAD_CLASSIC_BUTTON_UP) {
                        if (verbose) {
                            logLine("(verbose mode OFF)");
                            verbose=false;
                        } else {
                            logLine("(verbose mode ON)");
                            verbose=true;
                        }
                        OSSleepTicks(OSMillisecondsToTicks(500));
                    }
                    if ((kpadStatus.trigger | kpadStatus.hold) & WPAD_CLASSIC_BUTTON_DOWN) {
                        if (autoShutDown) {
                            logLine("(auto-shutdown OFF)");
                            IMDisableAPD(); // Disable auto-shutdown feature
                        } else {
                            logLine("(auto-shutdown ON)");
                            IMEnableAPD(); // Enable auto-shutdown feature
                        }
                        OSSleepTicks(OSMillisecondsToTicks(500));
                    }                    
                    break;
                case WPAD_EXT_PRO_CONTROLLER:
                    if ((kpadStatus.trigger | kpadStatus.hold) & WPAD_PRO_BUTTON_B) {
                        mountMlc = true;
                        buttonPressed = true;
                    }
                    if ((kpadStatus.trigger | kpadStatus.hold) & WPAD_PRO_BUTTON_A)
                        buttonPressed = true;
                    if ((kpadStatus.trigger | kpadStatus.hold) & WPAD_PRO_BUTTON_UP) {
                        if (verbose) {
                            logLine("(verbose mode OFF)");
                            verbose=false;
                        } else {
                            logLine("(verbose mode ON)");
                            verbose=true;
                        }
                        OSSleepTicks(OSMillisecondsToTicks(500));
                    }
                    if ((kpadStatus.trigger | kpadStatus.hold) & WPAD_PRO_BUTTON_DOWN) {
                        if (autoShutDown) {
                            logLine("(auto-shutdown OFF)");
                            IMDisableAPD(); // Disable auto-shutdown feature
                        } else {
                            logLine("(auto-shutdown ON)");
                            IMEnableAPD(); // Enable auto-shutdown feature
                        }
                        OSSleepTicks(OSMillisecondsToTicks(500));
                    }                    
                    break;
            }
            
            if (buttonPressed) break;            
        }
        OSSleepTicks(OSMillisecondsToTicks(1));
        cpt=cpt+1;
        
        // get last autoShutDown status
        IMIsAPDEnabled(&autoShutDown);
    }    

    // verbose mode (disabled by default)
    if (verbose) setVerboseMode(verbose);
    logLine(" ");
    
    int nbDrives=MountVirtualDevices(fsaFd, mountMlc);    
    if (nbDrives == 0) {
        WHBLogPrintf("! ERROR : No virtual devices mounted !");
        goto exit;
    }
	OSSleepTicks(OSMillisecondsToTicks(1000));

    logLine(" ");    
    /*--------------------------------------------------------------------------*/
    /* Starting Network                                                         */
    /*--------------------------------------------------------------------------*/
    WHBInitializeSocketLibrary();
    initialize_network();

    /*--------------------------------------------------------------------------*/
    /* Create FTP server                                                        */
    /*--------------------------------------------------------------------------*/
    serverSocket = create_server(FTP_PORT);
    if (serverSocket < 0) WHBLogPrintf("! ERROR : when creating server");
    
    int network_down = 0;
    WHBLogConsoleDraw();

    
    vpadError = -1;
    kpadError = -1;
    bool exitApplication = false;
    while (WHBProcIsRunning() && serverSocket >= 0 && !network_down && !exitApplication)
    {
        network_down = process_ftp_events();
        if (network_down)
            break;


        WHBLogConsoleDraw();

        VPADRead(0, &vpadStatus, 1, &vpadError);
        if ((vpadStatus.trigger | vpadStatus.hold) & VPAD_BUTTON_HOME) exitApplication = true;

        for (int i = 0; i < 4; i++)
        {
            uint32_t controllerType;
            // check if the controller is connected
            if (WPADProbe(i, &controllerType) != 0)
                continue;

           KPADRead(i, &kpadStatus, kpadError);
            
            switch (kpadError) {
                case VPAD_READ_SUCCESS: {
                    break;
                }
                case VPAD_READ_NO_SAMPLES: {
                    continue;
                }
                case VPAD_READ_INVALID_CONTROLLER: {
                    WHBLogPrint("Controller disconnected!");
                    exitApplication = true;
                    break;
                }
                default: {
                    WHBLogPrintf("Unknown PAD error! %08X", kpadError);
                    exitApplication = true;
                    break;
                }
            }


            switch (controllerType)
            {
                case WPAD_EXT_CORE:
                    if ((kpadStatus.trigger | kpadStatus.hold) & WPAD_BUTTON_HOME)
                        exitApplication = true;
                    break;
                case WPAD_EXT_CLASSIC:
                    if ((kpadStatus.trigger | kpadStatus.hold) & WPAD_CLASSIC_BUTTON_HOME)
                        exitApplication = true;
                    break;
                case WPAD_EXT_PRO_CONTROLLER:
                    if ((kpadStatus.trigger | kpadStatus.hold) & WPAD_PRO_BUTTON_HOME)
                        exitApplication = true;
                    break;
            }

            
            if (exitApplication) break;

        }
    }
        
    WHBLogConsoleDraw();

    /*--------------------------------------------------------------------------*/
    /* Cleanup and exit                                                         */
    /*--------------------------------------------------------------------------*/
    logLine("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
    logLine(" ");   
    if (!isChannel())
        logLine("Stopping server and return to HBL Menu...");
    else
        logLine("Stopping server and return to Wii-U Menu..."); 
    logLine(" "); 

 
	
	cleanUp();
    WHBLogConsoleDraw(); 	
	OSSleepTicks(OSMillisecondsToTicks(2000));
	logLine(" ");
	logLine(" ");

exit:
	
    if (isChannel()) {
        SYSLaunchMenu();
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