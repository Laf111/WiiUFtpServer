#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <malloc.h>
#include <time.h>

#include "common/types.h"
#include "iosuhax/iosuhax.h"
#include "iosuhax/iosuhax_cfw.h"
#include "iosuhax/iosuhax_devoptab.h"
#include "iosuhax/iosuhax_disc_interface.h"
#include "dynamic_libs/os_functions.h"
#include "dynamic_libs/fs_functions.h"
#include "dynamic_libs/vpad_functions.h"
#include "dynamic_libs/padscore_functions.h"
#include "dynamic_libs/socket_functions.h"
#include "system/memory.h"
#include "common/common.h"
#include "ftp.h"
#include "virtualpath.h"
#include "net.h"

#define FTP_PORT                21

#define MAX_CONSOLE_LINES_TV    27
#define MAX_CONSOLE_LINES_DRC   18

/****************************************************************************/
// PARAMETERS
/****************************************************************************/
// iosuhax file descriptor

static int fsaFd = -1;

// mcp_hook_fd
static int mcp_hook_fd = -1;

static char * consoleArrayTv[MAX_CONSOLE_LINES_TV];
static char * consoleArrayDrc[MAX_CONSOLE_LINES_DRC];


// lock to make thread safe the display method
static bool displayLock=false;
// method to output to gamePad and TV (thread safe)
void display(const char *format, ...)
{
    while (displayLock) usleep(100);
    // set the lock
    displayLock=true;
    
    char * tmp = NULL;

    va_list va;
    va_start(va, format);
    if((vasprintf(&tmp, format, va) >= 0) && tmp)
    {
        if(consoleArrayTv[0])
            free(consoleArrayTv[0]);
        if(consoleArrayDrc[0])
            free(consoleArrayDrc[0]);

        for(int i = 1; i < MAX_CONSOLE_LINES_TV; i++)
            consoleArrayTv[i-1] = consoleArrayTv[i];

        for(int i = 1; i < MAX_CONSOLE_LINES_DRC; i++)
            consoleArrayDrc[i-1] = consoleArrayDrc[i];

        if(strlen(tmp) > 79)
            tmp[79] = 0;

        consoleArrayTv[MAX_CONSOLE_LINES_TV-1] = (char*)malloc(strlen(tmp) + 1);
        if(consoleArrayTv[MAX_CONSOLE_LINES_TV-1])
            strcpy(consoleArrayTv[MAX_CONSOLE_LINES_TV-1], tmp);

        consoleArrayDrc[MAX_CONSOLE_LINES_DRC-1] = (tmp);
    }
    va_end(va);

    // Clear screens
//    OSScreenClearBufferEx(0, 0x993333FF);
//    OSScreenClearBufferEx(1, 0x993333FF);
// TODO    
    OSScreenClearBufferEx(0, 0x00000000);
    OSScreenClearBufferEx(1, 0x00000000);


    for(int i = 0; i < MAX_CONSOLE_LINES_TV; i++)
    {
        if(consoleArrayTv[i])
            OSScreenPutFontEx(0, 0, i, consoleArrayTv[i]);
    }

    for(int i = 0; i < MAX_CONSOLE_LINES_DRC; i++)
    {
        if(consoleArrayDrc[i])
            OSScreenPutFontEx(1, 0, i, consoleArrayDrc[i]);
    }

    OSScreenFlipBuffersEx(0);
    OSScreenFlipBuffersEx(1);
    
    // unset the lock
    displayLock=false;    
}
/****************************************************************************/
// LOCAL FUNCTIONS
/****************************************************************************/

//--------------------------------------------------------------------------
//just to be able to call async
void someFunc(s32 err, void *arg)
{
    (void)arg;
}
    
int MCPHookOpen() {
    //take over mcp thread
    mcp_hook_fd = MCP_Open();
    if (mcp_hook_fd < 0) return -1;
    IOS_IoctlAsync(mcp_hook_fd, 0x62, (void*)0, 0, (void*)0, 0, someFunc, (void*)0);
    //let wupserver start up
    sleep(1);
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
    sleep(1);
    MCP_Close(mcp_hook_fd);
    mcp_hook_fd = -1;
}

//--------------------------------------------------------------------------
/****************************************************************************/
// MAIN PROGRAM
/****************************************************************************/
int __entry_menu(int argc, char **argv)
{
    // returned code :
    // =0 : OK
    // <0 : ERRORS 
    int returnCode = EXIT_SUCCESS;
    
    // Init functions
    InitOSFunctionPointers();
    InitFSFunctionPointers();
    memoryInitialize();
    InitVPadFunctionPointers();
    InitPadScoreFunctionPointers();    

    WPADInit();

    // Init screen and screen buffers
    for(int i = 0; i < MAX_CONSOLE_LINES_TV; i++)
        consoleArrayTv[i] = NULL;

    for(int i = 0; i < MAX_CONSOLE_LINES_DRC; i++)
        consoleArrayDrc[i] = NULL;
    
    // Prepare screen
    int screen_buf0_size = 0;

    OSScreenInit();
    screen_buf0_size = OSScreenGetBufferSizeEx(0);
    OSScreenSetBufferEx(0, (void *)0xF4000000);
    OSScreenSetBufferEx(1, (void *)(0xF4000000 + screen_buf0_size));

    // Enable TV
    OSScreenEnableEx(0, 1);
    // Enable GamePad
    OSScreenEnableEx(1, 1);

    // Clear screens
//    OSScreenClearBufferEx(0, 0x993333FF);
//    OSScreenClearBufferEx(1, 0x993333FF);
// TODO    
    OSScreenClearBufferEx(0, 0x00000000);
    OSScreenClearBufferEx(1, 0x00000000);

    // Flip buffers
    OSScreenFlipBuffersEx(0);
    OSScreenFlipBuffersEx(1);
    
    // Init network
    InitSocketFunctionPointers();   
    
    IMDisableAPD(); // Disable auto-shutdown feature


    OSThread *thread = NULL;
    // get the current thread (on CPU1)
    thread = OSGetCurrentThread();
    
    if (thread != NULL) {
        // set the name 
        OSSetThreadName(thread, "WiiUFtpServer thread on CPU1");

        // set a priority to 0
        OSSetThreadPriority(thread, 0);
    }
    
    display(" -=============================-\n");
    display("|    %s     |\n", VERSION_STRING);
    display(" -=============================-\n");
    display("[Laf111/2021-08/dynamic_libs]");
    display(" ");
    
    // Get OS time and save it in ftp static variable 
    OSCalendarTime osDateTime;
    struct tm tmTime;
    OSTicksToCalendarTime(OSGetTime(), &osDateTime);

    // tm_mon is in [0,30]
    int mounth=osDateTime.mon+1;
    display("Wii-U date (GMT) : %02d/%02d/%04d %02d:%02d:%02d",
                      osDateTime.mday, mounth, osDateTime.year,
                      osDateTime.hour, osDateTime.min, osDateTime.sec);
    
    tmTime.tm_sec   =   osDateTime.sec;
    tmTime.tm_min   =   osDateTime.min;
    tmTime.tm_hour  =   osDateTime.hour;
    tmTime.tm_mday  =   osDateTime.mday;
    tmTime.tm_mon   =   osDateTime.mon;
    // tm struct : year is based from 1900
    tmTime.tm_year  =   osDateTime.year-1900;
    tmTime.tm_wday  =   osDateTime.wday;
    tmTime.tm_yday  =   osDateTime.yday;
    
    // save GMT OS Time in ftp.c
    setOsTime(&tmTime);
    display(" ");
    
    /*--------------------------------------------------------------------------*/
    /* IOSUHAX operations and mounting devices                                  */
    /*--------------------------------------------------------------------------*/
    IOSUHAX_CFW_Family cfw = IOSUHAX_CFW_GetFamily();
    if (cfw == 0) {
        display("! ERROR : No running CFW detected");
        returnCode = -10;
        goto exit;
    }
    
    int res = IOSUHAX_Open(NULL);
    if(res < 0)
        res = MCPHookOpen();
    if (res < 0) {
        display("! ERROR : IOSUHAX_Open failed.");
        returnCode = -11;
        goto exit;        
    }

    fsaFd = IOSUHAX_FSA_Open();
    if (fsaFd < 0) {
        display("! ERROR : IOSUHAX_FSA_Open failed.");
        if (mcp_hook_fd >= 0) MCPHookClose();
        else IOSUHAX_Close();
        returnCode = -12;
        goto exit;
    }
    
    setFsaFd(fsaFd);
    
    int nbDrives=MountVirtualDevices(fsaFd);    
    if (nbDrives == 0) {
        display("! ERROR : No virtual devices mounted !");
        returnCode = -20;
        goto exit;
    }
    display(" ");
    sleep(2);

	
    display(" ");
    display("FTP client tips :");    
	display("- ONLY one simultaneous transfert on UPLOAD (safer)");
	display("- 8 slots maximum for DOWNLOAD (2 clients => 4 per clients)");
    display(" ");
		
    
    /*--------------------------------------------------------------------------*/
    /* Create FTP server                                                        */
    /*--------------------------------------------------------------------------*/

    int serverSocket = create_server(FTP_PORT);
    if (serverSocket < 0) display("! ERROR : when creating server");
    int network_down = 0;

    /*--------------------------------------------------------------------------*/
    /* FTP loop                                                                 */
    /*--------------------------------------------------------------------------*/ 

    // gamepad inputs
    int vpadError = -1;
    VPADData vpad;

    KPADData kpad;

    // set everything to 0 because some vars will stay uninitialized on first read
    memset(&kpad, 0, sizeof(kpad));

    bool exitApplication = false;
    while(serverSocket >= 0 && !network_down && !exitApplication)
    {
        network_down = process_ftp_events();
        if(network_down)
            break;

        VPADRead(0, &vpad, 1, &vpadError);

        if(vpadError == 0 && ((vpad.btns_d | vpad.btns_h) & VPAD_BUTTON_HOME))
            break;

        for (int i = 0; i < 4; i++)
        {
            u32 controllerType;
            // check if the controller is connected
            if (WPADProbe(i, &controllerType) != 0)
                continue;

            KPADRead(i, &kpad, 1);

            switch (controllerType)
            {
                case WPAD_EXT_CORE:
                    if((kpad.btns_h | kpad.btns_d) & WPAD_BUTTON_HOME)
                        exitApplication = true;
                    break;
                case WPAD_EXT_CLASSIC:
                    if((kpad.classic.btns_h | kpad.classic.btns_d) & WPAD_CLASSIC_BUTTON_HOME)
                        exitApplication = true;
                    break;
                case WPAD_EXT_PRO_CONTROLLER:
                    if((kpad.pro.btns_h | kpad.pro.btns_d) & WPAD_PRO_BUTTON_HOME)
                        exitApplication = true;
                    break;
            }
            
        if (exitApplication) break;            
        }
    }
        
    /*--------------------------------------------------------------------------*/
    /* Cleanup and exit                                                         */
    /*--------------------------------------------------------------------------*/
    display("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
    display(" ");   
    display("Stopping server...");   
    display(" "); 
    
    cleanup_ftp();
    if (serverSocket >= 0) network_close(serverSocket);
    FreeSocketFunctionPointers();
    VPADShutdown();        
    display(" "); 
    UmountVirtualDevices();
    
    IOSUHAX_sdio_disc_interface.shutdown();
    IOSUHAX_usb_disc_interface.shutdown();

    IOSUHAX_FSA_Close(fsaFd);
    if (mcp_hook_fd >= 0) MCPHookClose();
    else IOSUHAX_Close();
    
exit:
    display(" "); 
    display("Returning to HBL Menu...");
    display(" ");

     for(int i = 0; i < MAX_CONSOLE_LINES_TV; i++)
    {
        if(consoleArrayTv[i])
            free(consoleArrayTv[i]);
    }
    for(int i = 0; i < MAX_CONSOLE_LINES_DRC; i++)
    {
        if(consoleArrayDrc[i])
            free(consoleArrayDrc[i]);
    }
    usleep(2000);
    memoryRelease();
    
    return returnCode;
}

