/****************************************************************************
  * WiiUFtpServer
  * 2021/04/05:V1.0.0:Laf111: import ftp-everywhere code
  * 2021/04/05:V1.2.0:Laf111: add msg when unmonting device
 ***************************************************************************/
#include <malloc.h>
#include <string.h>
#include <whb/log.h>
#include <whb/log_console.h>
#include "virtualpath.h"

// iosuhax fd
static int fsaFd = -1;

// mounting flags
static int storage_sdcard=0;
static int storage_slccmpt=0;
static int storage_mlc=0;
static int storage_usb=0;    
static int storage_odd_tickets=0;    
static int storage_odd_updates=0;
static int storage_odd_content=0; 
static int storage_odd_content2=0;
static int storage_slc=0;
static int nbDevices=0;

uint8_t MAX_VIRTUAL_PARTITIONS = 0;
VIRTUAL_PARTITION * VIRTUAL_PARTITIONS = NULL;

static void AddVirtualPath(const char *name, const char *alias, const char *prefix)
{
	if(!VIRTUAL_PARTITIONS)
		VIRTUAL_PARTITIONS = (VIRTUAL_PARTITION *) malloc(sizeof(VIRTUAL_PARTITION));

	VIRTUAL_PARTITION * tmp = realloc(VIRTUAL_PARTITIONS, sizeof(VIRTUAL_PARTITION)*(MAX_VIRTUAL_PARTITIONS+1));
	if(!tmp)
	{
		free(VIRTUAL_PARTITIONS);
		MAX_VIRTUAL_PARTITIONS = 0;
		return;
	}

	VIRTUAL_PARTITIONS = tmp;

	VIRTUAL_PARTITIONS[MAX_VIRTUAL_PARTITIONS].name = strdup(name);
	VIRTUAL_PARTITIONS[MAX_VIRTUAL_PARTITIONS].alias = strdup(alias);
	VIRTUAL_PARTITIONS[MAX_VIRTUAL_PARTITIONS].prefix = strdup(prefix);
	VIRTUAL_PARTITIONS[MAX_VIRTUAL_PARTITIONS].inserted = true;

	MAX_VIRTUAL_PARTITIONS++;
}

static void VirtualMountDevice(const char * path)
{
	if(!path)
		return;

	int i = 0;
	char name[255] = "";
	char alias[255] = "";
	char prefix[255] = "";
	bool namestop = false;

	alias[0] = '/';

	do
	{
		if(path[i] == ':')
			namestop = true;

		if(!namestop)
		{
			name[i] = path[i];
			name[i+1] = '\0';
			alias[i+1] = path[i];
			alias[i+2] = '\0';
		}

		prefix[i] = path[i];
		prefix[i+1] = '\0';
		i++;
	}
	while(path[i-1] != '/');
	AddVirtualPath(name, alias, prefix);
}

void UnmountVirtualPaths()
{
	uint32_t i = 0;
	for(i = 0; i < MAX_VIRTUAL_PARTITIONS; i++)
	{
		if(VIRTUAL_PARTITIONS[i].name)
			free(VIRTUAL_PARTITIONS[i].name);
		if(VIRTUAL_PARTITIONS[i].alias)
			free(VIRTUAL_PARTITIONS[i].alias);
		if(VIRTUAL_PARTITIONS[i].prefix)
			free(VIRTUAL_PARTITIONS[i].prefix);
	}

	if(VIRTUAL_PARTITIONS)
		free(VIRTUAL_PARTITIONS);
	VIRTUAL_PARTITIONS = NULL;
	MAX_VIRTUAL_PARTITIONS = 0;
}

void ResetVirtualPaths()
{
    if (nbDevices > 0) {
        UnmountVirtualPaths();
        
        if (storage_sdcard == 1)       VirtualMountDevice("storage_sdcard:/");
        if (storage_slccmpt == 1)      VirtualMountDevice("storage_slccmpt:/");
        if (storage_mlc == 1)          VirtualMountDevice("storage_mlc:/");
        if (storage_usb == 1)          VirtualMountDevice("storage_usb:/");
        if (storage_slc == 1)          VirtualMountDevice("storage_slc:/");
        
        if (storage_odd_tickets == 1)  VirtualMountDevice("storage_odd_tickets:/");
        if (storage_odd_updates == 1)  VirtualMountDevice("storage_odd_updates:/");
        if (storage_odd_content == 1)  VirtualMountDevice("storage_odd_content:/");
        if (storage_odd_content2 == 1) VirtualMountDevice("storage_odd_content2:/");
        
    }
}       
//--------------------------------------------------------------------------
int	MountVirtualDevices(int hfd) {
    
    fsaFd = hfd;
    IOSUHAX_FSA_Mount(fsaFd, "/dev/sdcard01", "/vol/storage_sdcard", 2, (void*)0, 0);
    
    if (mount_fs("storage_sdcard", fsaFd, NULL, "/vol/storage_sdcard") >=0) {
        WHBLogPrintf(">mounting storage_sdcard...");
        WHBLogConsoleDraw();
        storage_sdcard=1;
        VirtualMountDevice("storage_sdcard:/");
        nbDevices++;
    }
    if (mount_fs("storage_slccmpt", fsaFd, "/dev/slccmpt01", "/vol/storage_slccmpt01") >=0) {
        WHBLogPrintf(">mounting storage_slccmpt...");
        WHBLogConsoleDraw();
        storage_slccmpt=1;
        VirtualMountDevice("storage_slccmpt:/");
        nbDevices++;
    }
    if (mount_fs("storage_mlc", fsaFd, NULL, "/vol/storage_mlc01") >= 0) {
        WHBLogPrintf(">mounting storage_mlc...");
        WHBLogConsoleDraw();
        storage_mlc=1;
        VirtualMountDevice("storage_mlc:/");
        nbDevices++;
    }
    if (mount_fs("storage_usb", fsaFd, NULL, "/vol/storage_usb01") >=0) {
        WHBLogPrintf(">mounting storage_usb...");
        WHBLogConsoleDraw();
        storage_usb=1;
        VirtualMountDevice("storage_usb:/");
        nbDevices++;
    }
    if (mount_fs("storage_slc", fsaFd, NULL, "/vol/system") >= 0) {
        WHBLogPrintf(">mounting storage_slc...");
        WHBLogConsoleDraw();
        storage_slc=1;
        VirtualMountDevice("storage_slc:/");
        nbDevices++;
    }    
    if (mount_fs("storage_odd_tickets", fsaFd, "/dev/odd01", "/vol/storage_odd_tickets") >= 0) {
        WHBLogPrintf(">mounting storage_odd_tickets...");
        WHBLogConsoleDraw();
        storage_odd_tickets=1;
        VirtualMountDevice("storage_odd_tickets:/");
        nbDevices++;
    }
    if (mount_fs("storage_odd_updates", fsaFd, "/dev/odd02", "/vol/storage_odd_updates") >=0) {
        WHBLogPrintf(">mounting storage_odd_updates...");
        WHBLogConsoleDraw();
        storage_odd_updates=1;
        VirtualMountDevice("storage_odd_updates:/");
        nbDevices++;
    }
    if (mount_fs("storage_odd_content", fsaFd, "/dev/odd03", "/vol/storage_odd_content") >= 0) {
        WHBLogPrintf(">mounting storage_odd_content...");
        WHBLogConsoleDraw();
        storage_odd_content=1;
        VirtualMountDevice("storage_odd_content:/");
        nbDevices++;
    }
    if (mount_fs("storage_odd_content2", fsaFd, "/dev/odd04", "/vol/storage_odd_content2") >= 0) {
        WHBLogPrintf(">mounting storage_odd_content2...");
        WHBLogConsoleDraw();
        storage_odd_content2=1; 
        VirtualMountDevice("storage_odd_content2:/");
        nbDevices++;
    }
    return nbDevices;

}

//--------------------------------------------------------------------------
void UmountVirtualDevices() {
    
    UnmountVirtualPaths();
    
    if (storage_sdcard == 1) {
        unmount_fs("storage_sdcard");
        WHBLogPrintf(">unmounting storage_sdcard...");
        WHBLogConsoleDraw();        
        IOSUHAX_FSA_FlushVolume(fsaFd, "/vol/storage_sdcard");
        storage_sdcard = 0;
    }
    if (storage_slccmpt == 1) {
        unmount_fs("storage_slccmpt");
        WHBLogPrintf(">unmounting storage_slccmpt...");
        WHBLogConsoleDraw();
        IOSUHAX_FSA_FlushVolume(fsaFd, "/vol/storage_slccmpt01");
        storage_slccmpt = 0;
    }
    if (storage_mlc == 1) {
        unmount_fs("storage_mlc");
        WHBLogPrintf(">unmounting storage_mlc...");
        WHBLogConsoleDraw();
        IOSUHAX_FSA_FlushVolume(fsaFd, "/vol/storage_mlc01");
        storage_mlc = 0;
    }
    if (storage_usb == 1) {
        unmount_fs("storage_usb");
        WHBLogPrintf(">unmounting storage_usb...");
        WHBLogConsoleDraw();
        IOSUHAX_FSA_FlushVolume(fsaFd, "/vol/storage_usb01");
        storage_usb = 0;
    }
    if (storage_odd_tickets == 1) {
        unmount_fs("storage_odd_tickets");
        WHBLogPrintf(">unmounting storage_odd_tickets...");
        WHBLogConsoleDraw();
        IOSUHAX_FSA_FlushVolume(fsaFd, "/vol/storage_odd_tickets");
        storage_odd_tickets = 0;
    }
    if (storage_odd_updates == 1) {
        unmount_fs("storage_odd_updates");
        WHBLogPrintf(">unmounting storage_odd_updates...");
        WHBLogConsoleDraw();
        IOSUHAX_FSA_FlushVolume(fsaFd, "/vol/storage_odd_updates");
        storage_odd_updates = 0;
    }
    if (storage_odd_content == 1) {
        unmount_fs("storage_odd_content");
        WHBLogPrintf(">unmounting storage_odd_content...");
        WHBLogConsoleDraw();
        IOSUHAX_FSA_FlushVolume(fsaFd, "/vol/storage_odd_content");
        storage_odd_content = 0;
    }
    if (storage_odd_content2 == 1) {
        unmount_fs("storage_odd_content2");
        WHBLogPrintf(">unmounting storage_odd_content2...");
        WHBLogConsoleDraw();
        IOSUHAX_FSA_FlushVolume(fsaFd, "/vol/storage_odd_content2");
        storage_odd_content2 = 0;
    }
    if (storage_slc == 1) {
        unmount_fs("storage_slc");
        WHBLogPrintf(">unmounting storage_slc...");
        WHBLogConsoleDraw();
        IOSUHAX_FSA_FlushVolume(fsaFd, "/vol/storage_slc");
        storage_slc = 0;
    }
}