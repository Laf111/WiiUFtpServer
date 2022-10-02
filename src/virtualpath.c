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

#include "vrt.h"
#include "virtualpath.h"

#include <mocha/mocha.h>
#include <mocha/disc_interface.h>

#define O_OPEN_UNENCRYPTED 0x4000000

extern void display(const char *fmt, ...);

// mounting flags
extern bool mountMlc;
static bool diskInserted = false;

bool sd = false;
static bool storage_slccmpt = false;
static bool storage_mlc = false;
static bool storage_usb[4] = {false, false, false, false};    
static char usbLabel[4][14] = {"storage_usb01", "storage_usb02", "storage_usb03", "storage_usb04"};
static bool storage_odd_tickets = false;
static bool storage_odd_updates = false;
static bool storage_odd_content = false;
static bool storage_odd_content2 = false;
static bool storage_slc = false;

static int nbDevices = 0;

// storage_usb+port_number
char storage_usb_found[14] = "";    

uint8_t MAX_VIRTUAL_PARTITIONS = 0;
VIRTUAL_PARTITION * VIRTUAL_PARTITIONS = NULL;

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

static void UnmountVirtualPath(char * name)
{
    uint32_t i = 0;
    for(i = 0; i < MAX_VIRTUAL_PARTITIONS; i++)
    {
        if (strcmp(VIRTUAL_PARTITIONS[i].name, name) == 0) {
            free(VIRTUAL_PARTITIONS[i].name);
                
            if (VIRTUAL_PARTITIONS[i].alias)
                free(VIRTUAL_PARTITIONS[i].alias);
            if (VIRTUAL_PARTITIONS[i].prefix)
                free(VIRTUAL_PARTITIONS[i].prefix);
            MAX_VIRTUAL_PARTITIONS--;
        }
    }
}

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

        if (sd)                   VirtualMountDevice("sd:/");
        if (storage_slccmpt)      VirtualMountDevice("storage_slccmpt:/");
        if (storage_mlc)          VirtualMountDevice("storage_mlc:/");
        uint32_t i = -1;
        // Loop on all available USB ports     
        for (i = 0; i < 4; i++) {
            if (storage_usb[i]) {
                VirtualMountDevice(storage_usb_found);
            }
        }
        if (storage_slc)          VirtualMountDevice("storage_slc:/");
        if (storage_odd_tickets)  VirtualMountDevice("storage_odd_tickets:/");
        if (storage_odd_updates)  VirtualMountDevice("storage_odd_updates:/");
        if (storage_odd_content)  VirtualMountDevice("storage_odd_content:/");
        if (storage_odd_content2) VirtualMountDevice("storage_odd_content2:/");

    }
}

//--------------------------------------------------------------------------
int MountVirtualDevices(bool mountMlc) {

	Mocha_sdio_disc_interface.startup();
//    if (fatMountSimple("sd", &Mocha_sdio_disc_interface)) {
    if (Mocha_MountFS("sd", NULL, "/vol/external01") == MOCHA_RESULT_SUCCESS) {

        sd = true;
        display("Mounting sd...");
        VirtualMountDevice("sd:/");
        nbDevices++;
    }
	
    // USB    
	Mocha_usb_disc_interface.startup();
    uint32_t i = -1;
    // Loop on all available USB ports     
    for (i = 0; i < 4; i++) {
        
        char usbVolPath[19] = "/vol/";
        strcat(usbVolPath, usbLabel[i]);
        
		if (Mocha_MountFS(usbLabel[i], NULL, usbVolPath) == MOCHA_RESULT_SUCCESS) {
	        char usbVirtPath[16] = "";
	        strcat(usbVirtPath, usbLabel[i]);
	        strcat(usbVirtPath, ":/");
        
	        VirtualMountDevice(usbVirtPath);

	        char path[15] = "";
	        strcat(path, "/");
	        strcat(path, usbLabel[i]);
	        strcat(path, "/");
                
	        if (vrt_checkdir(path, "usr") >= 0) {
	            storage_usb[i] = true;
                strcpy(storage_usb_found, usbLabel[i]);
	        } 
	        UnmountVirtualPath(usbLabel[i]);
	        Mocha_UnmountFS(usbLabel[i]);
		}
    }
	
    if (strlen(storage_usb_found) != 0 ) {
        
        // mount the right path        
        char usbVolPath[19] = "/vol/";
        strcat(usbVolPath, storage_usb_found);

        Mocha_MountFS("storage_usb", NULL, usbVolPath);
        display("Mounting storage_usb (%s)...", storage_usb_found);
        
        VirtualMountDevice("storage_usb:/");
        nbDevices++;
    }    

    // MLC Paths
    if (mountMlc) {

        if (Mocha_MountFS("storage_mlc", NULL, "/vol/storage_mlc01") == MOCHA_RESULT_SUCCESS) {
            display("Mounting storage_mlc...");
            storage_mlc = true;
            VirtualMountDevice("storage_mlc:/");
            nbDevices++;
        }

        if (Mocha_MountFS("storage_slc", "/dev/slc01", "/vol/storage_slc01") == MOCHA_RESULT_SUCCESS) {
            display("Mounting storage_slc...");
            storage_slc = true;
            VirtualMountDevice("storage_slc:/");
            nbDevices++;
        }

        if (Mocha_MountFS("storage_slccmpt", "/dev/slccmpt01", "/vol/storage_slccmpt01") == MOCHA_RESULT_SUCCESS) {
            display("Mounting storage_slccmpt...");
            storage_slccmpt = true;
            VirtualMountDevice("storage_slccmpt:/");
            nbDevices++;
        }
        mountMlc = true;
    }

    if (vrt_checkdir("/dev", "odd01") >= 0) {
        if (Mocha_MountFS("storage_odd_tickets", "/dev/odd01", "/vol/storage_odd_tickets") == MOCHA_RESULT_SUCCESS) {
            display("Mounting storage_odd_tickets...");
            storage_odd_tickets = true;
            diskInserted = true;
            VirtualMountDevice("storage_odd_tickets:/");
            nbDevices++;
        }
    }

    if (vrt_checkdir("/dev", "odd02") >= 0) {
        if (Mocha_MountFS("storage_odd_updates", "/dev/odd02", "/vol/storage_odd_updates") == MOCHA_RESULT_SUCCESS) {
            display("Mounting storage_odd_updates...");
            storage_odd_updates = true;
            diskInserted = true;
            VirtualMountDevice("storage_odd_updates:/");
            nbDevices++;
        }
    }

    if (vrt_checkdir("/dev", "odd03") >= 0) {
        if (Mocha_MountFS("storage_odd_content", "/dev/odd03", "/vol/storage_odd_content") == MOCHA_RESULT_SUCCESS) {
            display("Mounting storage_odd_content...");
            storage_odd_content = true;
            diskInserted = true;
            VirtualMountDevice("storage_odd_content:/");
            nbDevices++;
        }
    }

    if (vrt_checkdir("/dev", "odd04") >= 0) {
        if (Mocha_MountFS("storage_odd_content2", "/dev/odd04", "/vol/storage_odd_content2") == MOCHA_RESULT_SUCCESS) {
            display("Mounting storage_odd_content2...");
            storage_odd_content2 = true;
            diskInserted = true;
            VirtualMountDevice("storage_odd_content2:/");
            nbDevices++;
        }
    }

    return nbDevices;

}

//--------------------------------------------------------------------------
void UnmountVirtualDevices() {

    UnmountVirtualPaths();

    if (sd) {
		if (Mocha_UnmountFS("sd") == MOCHA_RESULT_SUCCESS) {
        	display("Unmounting sd...");
//        fatUnmount("sd");
        	sd = false;
		}
		Mocha_sdio_disc_interface.shutdown();
    }

    uint32_t i = -1;
    // Loop on all available USB ports     
    for (i = 0; i < 4; i++) {
        if (storage_usb[i]) {
            Mocha_UnmountFS(usbLabel[i]);
	        display("Unmounting storage_usb (%s)...", storage_usb_found);
        }
    }

    if (mountMlc) {

        if (storage_slccmpt && Mocha_UnmountFS("storage_slccmpt") == MOCHA_RESULT_SUCCESS) {
            storage_slccmpt = false;
            display("Unmounting storage_slccmpt...");
        }

        if (storage_mlc && Mocha_UnmountFS("storage_mlc") == MOCHA_RESULT_SUCCESS) {
            storage_mlc = false;
            display("Unmounting storage_mlc...");
        }

        if (storage_slc && Mocha_UnmountFS("storage_slc") == MOCHA_RESULT_SUCCESS) {
            storage_slc = false;
            display("Unmounting storage_slc...");
        }
        mountMlc = false;
    }


    if (diskInserted) {

        if (storage_odd_tickets && Mocha_UnmountFS("storage_odd_tickets") == MOCHA_RESULT_SUCCESS) {
            storage_odd_tickets = false;
            display("Unmounting storage_odd_tickets...");
        }
        if (storage_odd_updates && Mocha_UnmountFS("storage_odd_updates") == MOCHA_RESULT_SUCCESS) {
            storage_odd_updates = false;
            display("Unmounting storage_odd_updates...");
        }
        if (storage_odd_content && Mocha_UnmountFS("storage_odd_content") == MOCHA_RESULT_SUCCESS) {
            storage_odd_content = false;
            display("Unmounting storage_odd_content...");
        }
        if (storage_odd_content2 && Mocha_UnmountFS("storage_odd_content2") == MOCHA_RESULT_SUCCESS) {
            storage_odd_content2 = false;
            display("Unmounting storage_odd_content2...");
        }
    	Mocha_usb_disc_interface.shutdown();    
        diskInserted = false;
    }
}

