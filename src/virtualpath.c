 /****************************************************************************
 * Copyright (C) 2008
 * Joseph Jordan <joe.ftpii@psychlaw.com.au>
 *
 * Copyright (C) 2010
 * by Dimok
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you
 * must not claim that you wrote the original software. If you use
 * this software in a product, an acknowledgment in the product
 * documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and
 * must not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 *
 * for WiiXplorer 2010
 ***************************************************************************/
 
 /****************************************************************************
  * WiiUFtpServer
  * 2021-12-05:Laf111:V7-0
 ***************************************************************************/
// Wii U libraries will give us paths that use /vol/storage_mlc01/file.txt, but posix uses the mounted drive paths like storage_mlc01:/file.txt
// this module handle posix path with virtual mounted drives (wrapping)
#include <malloc.h>
#include <string.h>
#include <whb/log.h>
#include <whb/log_console.h>

#include <fat.h>

#include <iosuhax_disc_interface.h>

#include "virtualpath.h"

extern void display(const char *fmt, ...);

// iosuhax fd
extern int fsaFd;

// mounting flags
static int sd=0;
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

//--------------------------------------------------------------------------
static void AddVirtualPath(const char *name, const char *alias, const char *prefix)
{
    if (!VIRTUAL_PARTITIONS)
        VIRTUAL_PARTITIONS = (VIRTUAL_PARTITION *) malloc(sizeof(VIRTUAL_PARTITION));

    VIRTUAL_PARTITION * tmp = realloc(VIRTUAL_PARTITIONS, sizeof(VIRTUAL_PARTITION)*(MAX_VIRTUAL_PARTITIONS+1));
    if (!tmp)
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

//--------------------------------------------------------------------------
void VirtualMountDevice(const char * path)
{
    if (!path)
        return;

    int i = 0;
    char name[255] = "";
    char alias[255] = "";
    char prefix[255] = "";
    bool namestop = false;

    alias[0] = '/';

    do
    {
        if (path[i] == ':')
            namestop = true;

        if (!namestop)
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
    while (path[i-1] != '/');
    AddVirtualPath(name, alias, prefix);
}

//--------------------------------------------------------------------------
void UnmountVirtualPaths()
{
    uint32_t i = 0;
    for(i = 0; i < MAX_VIRTUAL_PARTITIONS; i++)
    {
        if (VIRTUAL_PARTITIONS[i].name)
            free(VIRTUAL_PARTITIONS[i].name);
        if (VIRTUAL_PARTITIONS[i].alias)
            free(VIRTUAL_PARTITIONS[i].alias);
        if (VIRTUAL_PARTITIONS[i].prefix)
            free(VIRTUAL_PARTITIONS[i].prefix);
    }

    if (VIRTUAL_PARTITIONS)
        free(VIRTUAL_PARTITIONS);
    VIRTUAL_PARTITIONS = NULL;
    MAX_VIRTUAL_PARTITIONS = 0;
}

//--------------------------------------------------------------------------
void ResetVirtualPaths()
{
    if (nbDevices > 0) {
        UnmountVirtualPaths();

        if (sd == 1)                   VirtualMountDevice("sd:/");
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
int MountVirtualDevices(bool mountMlc) {

    // SDCard : use libFat or it will cripple performances on SDCard
    if (fatMountSimple("sd", &IOSUHAX_sdio_disc_interface)) {
        display("Mounting sd...");
        sd=1;
        VirtualMountDevice("sd:/");
        nbDevices++;
    }

    // USB
    if (mount_fs("storage_usb", fsaFd, NULL, "/vol/storage_usb01") >=0) {
        display("Mounting storage_usb...");

        storage_usb=1;
        VirtualMountDevice("storage_usb:/");
        nbDevices++;
    }
    
    // MLC Paths
    if (mountMlc) {
        if (mount_fs("storage_slccmpt", fsaFd, "/dev/slccmpt01", "/vol/storage_slccmpt01") >=0) {
            display("Mounting storage_slccmpt...");

            storage_slccmpt=1;
            VirtualMountDevice("storage_slccmpt:/");
            nbDevices++;
        }
        if (mount_fs("storage_mlc", fsaFd, NULL, "/vol/storage_mlc01") >= 0) {
            display("Mounting storage_mlc...");

            storage_mlc=1;
            VirtualMountDevice("storage_mlc:/");
            nbDevices++;
        }
        if (mount_fs("storage_slc", fsaFd, NULL, "/vol/system") >= 0) {
            display("Mounting storage_slc...");

            storage_slc=1;
            VirtualMountDevice("storage_slc:/");
            nbDevices++;
        }
    }
    if (mount_fs("storage_odd_tickets", fsaFd, "/dev/odd01", "/vol/storage_odd_tickets") >= 0) {
        display("Mounting storage_odd_tickets...");

        storage_odd_tickets=1;
        VirtualMountDevice("storage_odd_tickets:/");
        nbDevices++;
    }
    if (mount_fs("storage_odd_updates", fsaFd, "/dev/odd02", "/vol/storage_odd_updates") >=0) {
        display("Mounting storage_odd_updates...");

        storage_odd_updates=1;
        VirtualMountDevice("storage_odd_updates:/");
        nbDevices++;
    }
    if (mount_fs("storage_odd_content", fsaFd, "/dev/odd03", "/vol/storage_odd_content") >= 0) {
        display("Mounting storage_odd_content...");

        storage_odd_content=1;
        VirtualMountDevice("storage_odd_content:/");
        nbDevices++;
    }
    if (mount_fs("storage_odd_content2", fsaFd, "/dev/odd04", "/vol/storage_odd_content2") >= 0) {
        display("Mounting storage_odd_content2...");

        storage_odd_content2=1;
        VirtualMountDevice("storage_odd_content2:/");
        nbDevices++;
    }
    return nbDevices;

}

//--------------------------------------------------------------------------
void UmountVirtualDevices() {

    UnmountVirtualPaths();

    if (sd == 1) {
        fatUnmount("sd");
        display("Unmounting sd...");
        sd = 0;
    }
    if (storage_slccmpt == 1) {
        unmount_fs("storage_slccmpt");
        display("Unmounting storage_slccmpt...");

        IOSUHAX_FSA_FlushVolume(fsaFd, "/vol/storage_slccmpt01");
        storage_slccmpt = 0;
    }
    if (storage_mlc == 1) {
        unmount_fs("storage_mlc");
        display("Unmounting storage_mlc...");

        IOSUHAX_FSA_FlushVolume(fsaFd, "/vol/storage_mlc01");
        storage_mlc = 0;
    }
    if (storage_usb == 1) {
        unmount_fs("storage_usb");
        display("Unmounting storage_usb...");

        IOSUHAX_FSA_FlushVolume(fsaFd, "/vol/storage_usb01");
        storage_usb = 0;
    }
    if (storage_odd_tickets == 1) {
        unmount_fs("storage_odd_tickets");
        display("Unmounting storage_odd_tickets...");

        IOSUHAX_FSA_FlushVolume(fsaFd, "/vol/storage_odd_tickets");
        storage_odd_tickets = 0;
    }
    if (storage_odd_updates == 1) {
        unmount_fs("storage_odd_updates");
        display("Unmounting storage_odd_updates...");

        IOSUHAX_FSA_FlushVolume(fsaFd, "/vol/storage_odd_updates");
        storage_odd_updates = 0;
    }
    if (storage_odd_content == 1) {
        unmount_fs("storage_odd_content");
        display("Unmounting storage_odd_content...");

        IOSUHAX_FSA_FlushVolume(fsaFd, "/vol/storage_odd_content");
        storage_odd_content = 0;
    }
    if (storage_odd_content2 == 1) {
        unmount_fs("storage_odd_content2");
        display("Unmounting storage_odd_content2...");

        IOSUHAX_FSA_FlushVolume(fsaFd, "/vol/storage_odd_content2");
        storage_odd_content2 = 0;
    }
    if (storage_slc == 1) {
        unmount_fs("storage_slc");
        display("Unmounting storage_slc...");

        IOSUHAX_FSA_FlushVolume(fsaFd, "/vol/storage_slc");
        storage_slc = 0;
    }
}

