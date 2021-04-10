#include <coreinit/thread.h>
#include <coreinit/mcp.h>
#include <coreinit/time.h>
#include <time.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h> 

#include "ftp.h"
#include "virtualpath.h"
#include "net.h"
#include "iosuhax_cfw.h"

#define FTP_PORT	21

/****************************************************************************/
// PARAMETERS
/****************************************************************************/
// iosuhax file descriptor
static int fsaFd = -1;
// mcp_hook_fd
static int mcp_hook_fd = -1;

/****************************************************************************/
// LOCAL FUNCTIONS
/****************************************************************************/

//just to be able to call async
void someFunc(IOSError err, void *arg) {
    (void)arg;
}

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

void MCPHookClose() {
    if (mcp_hook_fd < 0) return;
    //close down wupserver, return control to mcp
    IOSUHAX_Close();
    //wait for mcp to return
    OSSleepTicks(OSMillisecondsToTicks(1000));
    MCP_Close(mcp_hook_fd);
    mcp_hook_fd = -1;
}

/****************************************************************************/
// MAIN PROGRAM
/****************************************************************************/
int main()
{
    // returned code :
    // =0 : OK
    // >0 : ERRORS 
    int returnCode = 0;
    
    // Console init
    WHBProcInit();
    WHBLogConsoleInit();

    WHBLogPrintf(" -=============================-\n");
    WHBLogPrintf("|    %s     |\n", VERSION_STRING);
    WHBLogPrintf(" -=============================-\n");
    WHBLogPrintf("                 Laf111(2021/03)");
    
    // Get OS time and save it in ftp static variable 
    OSCalendarTime osDateTime;
    struct tm tmTime;
    OSTicksToCalendarTime(OSGetTime(), &osDateTime);

    // tm_mon is in [0,30]
    int mounth=osDateTime.tm_mon+1;
    WHBLogPrintf("Wii-U OS date : %02d/%02d/%04d %02d:%02d:%02d",
                      osDateTime.tm_mday, mounth, osDateTime.tm_year,
                      osDateTime.tm_hour, osDateTime.tm_min, osDateTime.tm_sec);
    WHBLogPrintf(" ");
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
    int iosuhax = IOSUHAX_CFW_Available();
    if (iosuhax == 0) {
        WHBLogPrintf("You're not running any CFW, exiting...");
        OSSleepTicks(OSMillisecondsToTicks(5000));
        returnCode = 1;
        goto exit;
    }
    
    /*--------------------------------------------------------------------------*/
    /* IOSUHAX operations                                                       */
    /*--------------------------------------------------------------------------*/
    
    int res = IOSUHAX_Open(NULL);
    if (res < 0) {
        res = MCPHookOpen();
    }
    if (res < 0) {
        WHBLogPrintf("IOSUHAX_Open failed.");
        returnCode = 2;
        goto exit;        
    }

    fsaFd = IOSUHAX_FSA_Open();
    if (fsaFd < 0) {
        WHBLogPrintf("IOSUHAX_FSA_Open failed.");
        if (mcp_hook_fd >= 0) MCPHookClose();
        else IOSUHAX_Close();
        returnCode = 3;
        goto exit;
    }
	
    setFSAFD(fsaFd);
    IOSUHAX_FSA_Mount(fsaFd, "/dev/sdcard01", "/vol/storage_sdcard", 2, (void*)0, 0);
    mount_fs("sd", fsaFd, NULL, "/vol/storage_sdcard");
    mount_fs("slccmpt01", fsaFd, "/dev/slccmpt01", "/vol/storage_slccmpt01");
    mount_fs("storage_mlc", fsaFd, NULL, "/vol/storage_mlc01");
    mount_fs("storage_usb", fsaFd, NULL, "/vol/storage_usb01");
    mount_fs("storage_odd", fsaFd, "/dev/odd03", "/vol/storage_odd_content");    
    
    WHBLogConsoleDraw();

	MountVirtualDevices();    
    
    /*--------------------------------------------------------------------------*/
    /* Starting Network                                                         */
    /*--------------------------------------------------------------------------*/
    
	initialise_network();
    uint32_t ip = network_gethostip();                    
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
    
    while(WHBProcIsRunning() && serverSocket >= 0 && !network_down)
    {
        network_down = process_ftp_events(serverSocket);
        if(network_down)
            break;
        OSSleepTicks(OSMillisecondsToTicks(100));
        WHBLogConsoleDraw();        
    }
    
    /*--------------------------------------------------------------------------*/
    /* Cleanup and exit                                                         */
    /*--------------------------------------------------------------------------*/
    
    WHBLogPrintf("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
    WHBLogPrintf("Exiting... have a nice day!");
    
    WHBLogConsoleDraw();
    OSSleepTicks(OSMillisecondsToTicks(3000));

    cleanup_ftp();
    if (serverSocket >= 0) network_close(serverSocket);
	finalize_network();

    // Flushing volumes    
    IOSUHAX_FSA_FlushVolume(fsaFd, "/vol/storage_usb01");
    IOSUHAX_FSA_FlushVolume(fsaFd, "/vol/storage_mlc01");
    IOSUHAX_FSA_FlushVolume(fsaFd, "/vol/storage_slccmpt01");
    IOSUHAX_FSA_FlushVolume(fsaFd, "/vol/storage_sdcard");
        
    IOSUHAX_sdio_disc_interface.shutdown();
    IOSUHAX_usb_disc_interface.shutdown();

    unmount_fs("slccmpt01");
    unmount_fs("storage_slc");
    unmount_fs("storage_mlc");
    unmount_fs("storage_usb");

    IOSUHAX_FSA_Close(fsaFd);
    if (mcp_hook_fd >= 0) MCPHookClose();
    else IOSUHAX_Close();

	UnmountVirtualPaths();

    
exit:    
    WHBLogConsoleFree();
    WHBProcShutdown();
    return returnCode;
}