#include <whb/proc.h>
#include <whb/libmanager.h>
#include <coreinit/dynload.h>
#include <coreinit/thread.h>
#include <coreinit/mcp.h>
#include <coreinit/core.h>
#include <coreinit/time.h>
#include <coreinit/energysaver.h>
#include <coreinit/title.h>
#include <padscore/kpad.h>
#include <padscore/wpad.h>
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

#define FTP_PORT    21
/****************************************************************************/
// PARAMETERS
/****************************************************************************/
// iosuhax file descriptor
static int fsaFd = -1;

// mcp_hook_fd
static int mcp_hook_fd = -1;


static OSDynLoad_Module coreinitHandle = NULL;
static int32_t (*OSShutdown)(int32_t status);

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

//--------------------------------------------------------------------------
/****************************************************************************/
// MAIN PROGRAM
/****************************************************************************/
int main()
{
    // returned code :
    // =0 : OK
    // <0 : ERRORS 
    int returnCode = EXIT_SUCCESS;
        
    // Console init
    WHBProcInit();
    WHBLogConsoleInit();
    

    KPADInit();
    
    IMDisableAPD(); // Disable auto-shutdown feature
    
    if (isChannel()) {
        // Initialize OSShutdown and OSForceFullRelaunch functions
        OSDynLoad_Acquire("coreinit.rpl", &coreinitHandle);
        OSDynLoad_FindExport(coreinitHandle, FALSE, "OSShutdown", (void **)&OSShutdown);
        OSDynLoad_Release(coreinitHandle);    
    }
    
    WHBLogPrintf(" -=============================-\n");
    WHBLogPrintf("|    %s     |\n", VERSION_STRING);
    WHBLogPrintf(" -=============================-\n");
    WHBLogPrintf("[Laf111/2021-08/WUT]");
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
    WHBLogPrintf(" ");
    WHBLogConsoleDraw();


    /*--------------------------------------------------------------------------*/
    /* IOSUHAX operations and mounting devices                                  */
    /*--------------------------------------------------------------------------*/
    // Check if a CFW is active
    IOSUHAX_CFW_Family cfw = IOSUHAX_CFW_GetFamily();
    if (cfw == 0) {
        WHBLogPrintf("! ERROR : No running CFW detected");
        returnCode = -10;
        goto exit;
    }
    
    int res = IOSUHAX_Open(NULL);
    if(res < 0)
        res = MCPHookOpen();
    if (res < 0) {
        WHBLogPrintf("! ERROR : IOSUHAX_Open failed.");
        returnCode = -11;
        goto exit;        
    }
    
    fsaFd = IOSUHAX_FSA_Open();
    if (fsaFd < 0) {
        WHBLogPrintf("! ERROR : IOSUHAX_FSA_Open failed.");
        if (mcp_hook_fd >= 0) MCPHookClose();
        else IOSUHAX_Close();
        returnCode = -12;
        goto exit;
    }
    
    setFsaFd(fsaFd);

    int nbDrives=MountVirtualDevices(fsaFd);    
    if (nbDrives == 0) {
        WHBLogPrintf("! ERROR : No virtual devices mounted !");
        returnCode = -20;
        goto exit;
    }
    WHBLogPrintf(" ");
    OSSleepTicks(OSMillisecondsToTicks(2000));

    WHBLogPrintf(" ");
    WHBLogPrintf("FTP client tips :");    
	WHBLogPrintf("- ONLY one simultaneous transfert on UPLOAD (safer)");
	WHBLogPrintf("- 8 slots maximum for DOWNLOAD (2 clients => 4 per clients)");
    WHBLogPrintf(" ");
        
    /*--------------------------------------------------------------------------*/
    /* Starting Network                                                         */
    /*--------------------------------------------------------------------------*/
    WHBInitializeSocketLibrary();
    initialise_network();

    /*--------------------------------------------------------------------------*/
    /* Create FTP server                                                        */
    /*--------------------------------------------------------------------------*/
    int serverSocket = create_server(FTP_PORT);
    if (serverSocket < 0) WHBLogPrintf("! ERROR : when creating server");
    

    
    int network_down = 0;
    WHBLogConsoleDraw();

    /*--------------------------------------------------------------------------*/
    /* FTP loop                                                                 */
    /*--------------------------------------------------------------------------*/    

    // gamepad inputs (needed for channel, WHBProc HOME_BUTTON event is not enought)
    VPADStatus vpadStatus;
    VPADReadError vpadError = -1;

    KPADStatus kpadStatus;
    VPADReadError kpadError = -1;
    bool exitApplication = false;
    while (WHBProcIsRunning() && serverSocket >= 0 && !network_down && !exitApplication)
    {
        network_down = process_ftp_events();
        if(network_down)
            break;

        OSSleepTicks(OSMillisecondsToTicks(100));
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
                    if((kpadStatus.trigger | kpadStatus.hold) & WPAD_BUTTON_HOME)
                        exitApplication = true;
                    break;
                case WPAD_EXT_CLASSIC:
                    if((kpadStatus.trigger | kpadStatus.hold) & WPAD_CLASSIC_BUTTON_HOME)
                        exitApplication = true;
                    break;
                case WPAD_EXT_PRO_CONTROLLER:
                    if((kpadStatus.trigger | kpadStatus.hold) & WPAD_PRO_BUTTON_HOME)
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
    WHBLogPrintf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
    WHBLogPrintf(" ");   
    WHBLogPrintf("Stopping server...");   
    WHBLogPrintf(" "); 

    WHBLogConsoleDraw();  
    
    cleanup_ftp();
    
    if (serverSocket >= 0) network_close(serverSocket);
    finalize_network();
    VPADShutdown();        
    WHBLogPrintf(" "); 

    UmountVirtualDevices();
    
    IOSUHAX_sdio_disc_interface.shutdown();
    IOSUHAX_usb_disc_interface.shutdown();
    
    IOSUHAX_FSA_Close(fsaFd);
    if (mcp_hook_fd >= 0) MCPHookClose();
    else IOSUHAX_Close();
    
exit:
    WHBLogPrintf(" "); 
    if (isChannel()) {
        WHBLogPrintf("Shuting down the Wii-U...");
        
    } else {
        WHBLogPrintf("Returning to HBL Menu...");
    }
    WHBLogPrintf(" ");
    WHBLogConsoleDraw();      
    
    OSSleepTicks(OSMillisecondsToTicks(2000));
            
    WHBLogConsoleFree();
    WHBProcShutdown();    

    // ShutDown when launched with channel
    if (isChannel()) OSShutdown(1);
    
    return returnCode;
}