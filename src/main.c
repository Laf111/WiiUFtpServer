/****************************************************************************
  * WiiUFtpServer
  * 2021/04/05:V1.0.0:Laf111: import ftp-everywhere code
  * 2021/04/30:V2.0.0:Laf111: code for channel
  * 2021/05/06:V2.1.0:Laf111: add other controller than gamePad (request/issue #1)
  * 2021/05/07:V2.2.0:Laf111: (request/issue #1) reopen, not work for HBL version
 ***************************************************************************/
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
#include <whb/proc.h>
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

#define FTP_PORT	21
/****************************************************************************/
// PARAMETERS
/****************************************************************************/
// iosuhax file descriptor
static int fsaFd = -1;

// mcp_hook_fd
static int mcp_hook_fd = -1;

// gamepad inputs (needed for channel, WHBProc HOME_BUTTON event is not enought)
VPADStatus vpadStatus;
VPADReadError vpadError;
KPADStatus kpadStatus;
VPADReadError kpadError;
bool pad_fatal = false;

static OSDynLoad_Module coreinitHandle = NULL;
static int32_t (*OSShutdown)(int32_t status);

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
    int returnCode = 0;
        
    // Console init
    WHBProcInit();
    WHBLogConsoleInit();
    
    VPADInit();
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
    WHBLogPrintf("[Laf111:2021-04]");
    WHBLogPrintf(" ");

    // Get OS time and save it in ftp static variable 
    OSCalendarTime osDateTime;
    struct tm tmTime;
    OSTicksToCalendarTime(OSGetTime(), &osDateTime);

    // tm_mon is in [0,30]
    int mounth=osDateTime.tm_mon+1;
    WHBLogPrintf("Wii-U OS date : %02d/%02d/%04d %02d:%02d:%02d",
                      osDateTime.tm_mday, mounth, osDateTime.tm_year,
                      osDateTime.tm_hour, osDateTime.tm_min, osDateTime.tm_sec);
    WHBLogConsoleDraw();
    
    tmTime.tm_sec   =   osDateTime.tm_sec;
    tmTime.tm_min   =   osDateTime.tm_min;
    tmTime.tm_hour  =   osDateTime.tm_hour;
    tmTime.tm_mday  =   osDateTime.tm_mday;
    tmTime.tm_mon   =   osDateTime.tm_mon;
    // tm struct : year is based from 1900
    tmTime.tm_year  =   osDateTime.tm_year-1900;
    tmTime.tm_wday  =   osDateTime.tm_wday;
    tmTime.tm_yday  =   osDateTime.tm_yday;
    
    setOsTime(&tmTime);

     // Check if a CFW is active
    IOSUHAX_CFW_Family cfw = IOSUHAX_CFW_GetFamily();
    if (cfw == 0) {
        WHBLogPrintf("ERROR No running CFW detected");
        returnCode = -10;
        goto exit;
    }
    
    /*--------------------------------------------------------------------------*/
    /* IOSUHAX operations and mounting devices                                  */
    /*--------------------------------------------------------------------------*/
    int res = IOSUHAX_Open(NULL);
    if (res < 0) {
        res = MCPHookOpen();
    }
    if (res < 0) {
        WHBLogPrintf("ERROR IOSUHAX_Open failed.");
        returnCode = -11;
        goto exit;        
    }
    
    fsaFd = IOSUHAX_FSA_Open();
    if (fsaFd < 0) {
        WHBLogPrintf("ERROR IOSUHAX_FSA_Open failed.");
        if (mcp_hook_fd >= 0) MCPHookClose();
        else IOSUHAX_Close();
        returnCode = -12;
        goto exit;
    }
    
    setFsaFd(fsaFd);
    WHBLogPrintf(" ");

	int nbDrives=MountVirtualDevices(fsaFd);    
    if (nbDrives == 0) {
        WHBLogPrintf("ERROR No virtual devices mounted !");
        returnCode = -20;
        goto exit;
    }
    
 	OSThread *mainThread = OSGetCurrentThread();
	OSSetThreadName(mainThread, "WiiUFtpServer");

	if(!OSSetThreadPriority(mainThread, 1))
		WHBLogPrintf("WARNING: Error changing main thread priority!");
    
    /*--------------------------------------------------------------------------*/
    /* Starting Network                                                         */
    /*--------------------------------------------------------------------------*/
	initialise_network();
    uint32_t ip = network_gethostip();     
    WHBLogPrintf(" ");               
    WHBLogPrintf("Listening on %u.%u.%u.%u:%i", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF, FTP_PORT);
    WHBLogConsoleDraw();

    /*--------------------------------------------------------------------------*/
    /* Create FTP server                                                        */
    /*--------------------------------------------------------------------------*/
    int serverSocket = create_server(FTP_PORT);
    int network_down = 0;

    /*--------------------------------------------------------------------------*/
    /* FTP loop                                                                 */
    /*--------------------------------------------------------------------------*/    

    bool exitApplication = false;
    while (WHBProcIsRunning() && serverSocket >= 0 && !network_down)
    {
        network_down = process_ftp_events(serverSocket);
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

            if (exitApplication) break;         

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
        
        if (exitApplication) break;            
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
    WHBLogConsoleDraw();    
    OSSleepTicks(OSMillisecondsToTicks(1000));
        
    WHBLogConsoleDraw();
    WHBLogConsoleFree();
    WHBProcShutdown();    

    // ShutDown when launched with channel
    if (isChannel()) OSShutdown(1);
    
    return returnCode;
}