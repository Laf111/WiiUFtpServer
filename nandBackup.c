/****************************************************************************
  * WiiUFtpServer
  * 2021-12-05:Laf111:V7-0
 ***************************************************************************/
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/dirent.h>

#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <whb/log.h>
#include <whb/log_console.h>

#include "nandBackup.h"
#include <iosuhax.h>
#include <iosuhax_devoptab.h>
#include <iosuhax_disc_interface.h>

#define BUFFER_SIZE 				0x8020
#define BUFFER_SIZE_STEPS           0x20

extern void display(const char *fmt, ...);

// paths on SDCard and Wii-U FS for NAND backups
const char localRootPath[FS_MAX_LOCALPATH_SIZE] = "/vol/storage_sdcard/wiiu/apps/WiiuFtpServer/NandBackup";
const char localSubFolders[17][FS_MAX_LOCALPATH_SIZE] = {"storage_slc/proc",
                                                         "storage_slc/config",
                                                         "storage_slc/security",
                                                         "storage_slc/rights",
                                                         "storage_slc/title/00050010/100040ff",
                                                         "storage_slc/title/00050010/1000400a",
                                                         "storage_slc/title/00050010/10004000",
                                                         "storage_slc/title/00050010/10004001",
                                                         "storage_slc/title/00050010/10004009", 
                                                         "storage_slccmpt",
                                                         "storage_mlc/usr/save/system",
                                                         "storage_mlc/usr/save/00050030/1006d200",
                                                         "storage_mlc/usr/save/00050030/10012200",
                                                         "storage_mlc/usr/save/00050030/10014200",
                                                         "storage_mlc/usr/save/00050030/10015200",
                                                         "storage_mlc/usr/boss/00050010/10066000",
                                                         "storage_mlc/sys/title/00050030/1001a10a"};
const char remoteRootPath[FS_MAX_LOCALPATH_SIZE] = "/vol";
const char remoteSubFolders[17][FS_MAX_LOCALPATH_SIZE] = {"system/proc",
                                                          "system/config",
                                                          "system/security",
                                                          "system/rights",
                                                          "system/title/00050010/100040ff",
                                                          "system/title/00050010/1000400a",
                                                          "system/title/00050010/10004000",
                                                          "system/title/00050010/10004001",
                                                          "system/title/00050010/10004009",
                                                          "storage_slccmpt01",
                                                          "storage_mlc01/usr/save/system",
                                                          "storage_mlc01/usr/save/00050030/1006d200",
                                                          "storage_mlc01/usr/save/00050030/10012200",
                                                          "storage_mlc01/usr/save/00050030/10014200",
                                                          "storage_mlc01/usr/save/00050030/10015200",
                                                          "storage_mlc01/usr/boss/00050010/10066000",
                                                          "storage_mlc01/sys/title/00050030/1001a10a"};
                                                          
static int fsaFd = -1;

void setFsaFdCopyFiles(int fd) {
	fsaFd = fd;
}

static int FSAR(int result) {
	if ((result & 0xFFFF0000) == 0xFFFC0000)
		return (result & 0xFFFF) | 0xFFFF0000;
	else
		return result;
}

uint32_t checkEntry(const char * fPath) {
	IOSUHAX_FSA_Stat fStat;
	int ret = FSAR(IOSUHAX_FSA_GetStat(fsaFd, fPath, &fStat));

	if (ret == FSA_STATUS_NOT_FOUND) return 0;
	else if (ret < 0) return -1;

	if (fStat.flags & DIR_ENTRY_IS_DIRECTORY) return 2;
	return 1;
}

uint32_t folderEmpty(const char * fPath) {
	int dirH;

	if (IOSUHAX_FSA_OpenDir(fsaFd, fPath, &dirH) >= 0) {
		IOSUHAX_FSA_DirectoryEntry data;
		int ret = FSAR(IOSUHAX_FSA_ReadDir(fsaFd, dirH, &data));
		IOSUHAX_FSA_CloseDir(fsaFd, dirH);
		if (ret == FSA_STATUS_END_OF_DIRECTORY)
			return 1;
	} else return -1;
	return 0;
}


uint32_t copyFile(char* srcFilePath, const char* targetFilePath) {
	int srcFd = -1, destFd = -1;
	int ret = 0;
	int buf_size = BUFFER_SIZE;
 	uint8_t * pBuffer;

	do{
		buf_size -= BUFFER_SIZE_STEPS;
		if (buf_size < 0) {
			display("! ERROR : when allocating Buffer.");
			return -99;
		}
		pBuffer = (uint8_t *)memalign(0x40, buf_size);
		if (pBuffer) memset(pBuffer, 0x00, buf_size);
	}while(!pBuffer);

	ret = IOSUHAX_FSA_OpenFile(fsaFd, srcFilePath, "rb", &srcFd);
	if (ret >= 0) {
		IOSUHAX_FSA_Stat fStat;
		IOSUHAX_FSA_StatFile(fsaFd, srcFd, &fStat);
		if ((ret = IOSUHAX_FSA_OpenFile(fsaFd, targetFilePath, "wb", &destFd)) >= 0) {

			int result = 0, fwrite = 0;
			while ((result = IOSUHAX_FSA_ReadFile(fsaFd, pBuffer, 0x01, buf_size, srcFd, 0)) > 0) {
				if ((fwrite = IOSUHAX_FSA_WriteFile(fsaFd, pBuffer, 0x01, result, destFd, 0)) < 0) {
					IOSUHAX_FSA_CloseFile(fsaFd, destFd);
					IOSUHAX_FSA_CloseFile(fsaFd, srcFd);
					free(pBuffer);
					return -1;
				}
			}
		} else {
			display("! ERROR : Failed to write rc=%d,%s", ret, targetFilePath);
			IOSUHAX_FSA_CloseFile(fsaFd, srcFd);
			free(pBuffer);
			return -1;
		}
		IOSUHAX_FSA_CloseFile(fsaFd, destFd);
		IOSUHAX_FSA_CloseFile(fsaFd, srcFd);
		IOSUHAX_FSA_ChangeMode(fsaFd, targetFilePath, 0x644);
		free(pBuffer);
	} else {
		display("! ERROR : Failed to open file rc=%d,%s", ret, srcFilePath);
		free(pBuffer);
		return -1;
	}
	return 0;
}

uint32_t copyFolder(char* srcFolderPath, const char* targetFolderPath) {
    int dirH;

	if (IOSUHAX_FSA_OpenDir(fsaFd, srcFolderPath, &dirH) < 0) return -1;
	IOSUHAX_FSA_MakeDir(fsaFd, targetFolderPath, 0x644);

    int nbFilesCopied = 0;
    while (1) {
		IOSUHAX_FSA_DirectoryEntry data;
		int ret = IOSUHAX_FSA_ReadDir(fsaFd, dirH, &data);
		if (ret != 0)
			break;

        if (strcmp(data.name, "..") == 0 || strcmp(data.name, ".") == 0) continue;

        int len = strlen(srcFolderPath);
        snprintf(srcFolderPath + len, FS_MAX_FULLPATH_SIZE - len, "/%s", data.name);

        if (data.info.flags & DIR_ENTRY_IS_DIRECTORY) {
            char* targetPath = (char*)malloc(FS_MAX_FULLPATH_SIZE);
            snprintf(targetPath, FS_MAX_FULLPATH_SIZE, "%s/%s", targetFolderPath, data.name);

            IOSUHAX_FSA_MakeDir(fsaFd, targetPath, 0x644);
            if (copyFolder(srcFolderPath, targetPath) != 0) {
                
                display("! ERROR : %s copy failed", data.name);
                IOSUHAX_FSA_CloseDir(fsaFd, dirH);
                return -2;
            }

            free(targetPath);
        } else {
            char* targetPath = (char*)malloc(FS_MAX_FULLPATH_SIZE);
            snprintf(targetPath, FS_MAX_FULLPATH_SIZE, "%s/%s", targetFolderPath, data.name);

            if (copyFile(srcFolderPath, targetPath) != 0) {
                IOSUHAX_FSA_CloseDir(fsaFd, dirH);
                return -3;
            }

            free(targetPath);
            nbFilesCopied += 1;
        }

        srcFolderPath[len] = 0;
    
    }

    IOSUHAX_FSA_CloseDir(fsaFd, dirH);

    if (nbFilesCopied != 0) display("  %d files copied sucessfully", nbFilesCopied);
    return 0;
}

//--------------------------------------------------------------------------
uint32_t createNandBackup(bool fullFlag) {

    // global return code
    uint32_t grc = 0;

    uint32_t nbPaths = 1;
    if (fullFlag) nbPaths = 17;
    
    for (uint32_t i=0; i<nbPaths; i++) {

        display("> Treating %s...", remoteSubFolders[i]);

        char srcFolder[FS_MAX_FULLPATH_SIZE]="";
        strcpy(srcFolder, remoteRootPath);
        strcat(srcFolder, "/");
        strcat(srcFolder, remoteSubFolders[i]);

        char tgtFolder[FS_MAX_FULLPATH_SIZE]="";
        strcpy(tgtFolder, localRootPath);
        strcat(tgtFolder, "/");
        strcat(tgtFolder, localSubFolders[i]);

        // if both folders exists
        if ( checkEntry(srcFolder) == 2 ) {
            
            // if tgtFolder does not exist, it will be created
            int rc = copyFolder(srcFolder, tgtFolder);
            if (rc != 0) {
                display("! ERROR : copyFolder returned %d", rc);
            
                OSSleepTicks(OSMillisecondsToTicks(2000));
                grc = rc;
            }
        }
    }

    if (grc == 0) display("Backup created in wiiu/apps/WiiuFtpServer/NandBackup");
    else display("! ERROR : failed to create NAND backup");
    display(" ");

    return grc;
}

//--------------------------------------------------------------------------
uint32_t restoreNandBackup() {
    // global return code
    uint32_t grc = 0;

    // Restore only storage_slc/proc when network is down.
    for (int i=0; i<1; i++) {

        display("> Treating %s...", localSubFolders[i]);

        char srcFolder[FS_MAX_FULLPATH_SIZE]="";
        strcpy(srcFolder, localRootPath);
        strcat(srcFolder, "/");
        strcat(srcFolder, localSubFolders[i]);

        char tgtFolder[FS_MAX_FULLPATH_SIZE]="";
        strcpy(tgtFolder, remoteRootPath);
        strcat(tgtFolder, "/");
        strcat(tgtFolder, remoteSubFolders[i]);

        // if both folders exists
        if ( checkEntry(srcFolder) == 2 ) {
            // if tgtFolder does not exist, it will be created
            int rc = copyFolder(srcFolder, tgtFolder);
            if (rc != 0) {
                display("! ERROR : copyFolder returned %d", rc);
            
                OSSleepTicks(OSMillisecondsToTicks(2000));
                grc = rc;
            }
        }
    }

    if (grc == 0) display("NAND network files restored successfully !");
    else display("! ERROR : failed to restore NAND backup");
    display(" ");

    return grc;
}


