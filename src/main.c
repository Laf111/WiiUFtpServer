/****************************************************************************
  * WiiUFtpServer_dl
  * 2021/04/05:V1.0.0:Laf111: import ftp-everywhere code
 ***************************************************************************/
#include <coreinit/thread.h>
#include <coreinit/mcp.h>
#include <coreinit/time.h>
#include <coreinit/energysaver.h>
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


// TODO : remove checks -----------------------------------------------------------------------

int FSAR(int result) {
	if ((result & 0xFFFF0000) == 0xFFFC0000)
		return (result & 0xFFFF) | 0xFFFF0000;
	else
		return result;
}

void displayTs(time_t t) {
    if ((time_t)-1 > 0) {
        // time_t is an unsigned type
        WHBLogPrintf("TS (unsigned) = %ju\n", (uintmax_t)t);
    }
    else if ((time_t)1 / 2 > 0) {
        // time_t is a signed integer type
        WHBLogPrintf("%TS (signed) = %jd\n", (intmax_t)t);
    }
    else {
        // time_t is a floating-point type (I've never seen this)
        WHBLogPrintf("TS (floating-point) = %Lf\n", (long double)t);
    }
}
// TODO : remove checks -----------------------------------------------------------------------





//--------------------------------------------------------------------------
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

    IMDisableAPD(); // Disable auto-shutdown feature
    
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
        returnCode = 1;
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
        returnCode = 2;
        goto exit;        
    }
    
    fsaFd = IOSUHAX_FSA_Open();
    if (fsaFd < 0) {
        WHBLogPrintf("ERROR IOSUHAX_FSA_Open failed.");
        if (mcp_hook_fd >= 0) MCPHookClose();
        else IOSUHAX_Close();
        returnCode = 3;
        goto exit;
    }
    
    setFsaFd(fsaFd);
    WHBLogPrintf(" ");

	int nbDrives=MountVirtualDevices(fsaFd);    
    if (nbDrives == 0) {
        WHBLogPrintf("ERROR No virtual devices mounted !");
        returnCode = 4;
        goto exit;
    }

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
    WHBLogConsoleDraw();
    OSSleepTicks(OSMillisecondsToTicks(3000));
    
    WHBLogConsoleFree();
    WHBProcShutdown();
    return returnCode;
}