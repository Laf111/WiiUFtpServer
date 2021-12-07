/****************************************************************************
  * WiiUFtpServer
  * 2021-12-05:Laf111:V7-0
 ***************************************************************************/
#ifndef _NANDBACKUP_H_
#define _NANDBACKUP_H_

#include <padscore/kpad.h>
#include <padscore/wpad.h>
#include <vpad/input.h>

/* FS defines and types */
#define FS_MAX_LOCALPATH_SIZE               511
#define FS_MAX_MOUNTPATH_SIZE               128
#define FS_MAX_FULLPATH_SIZE                (FS_MAX_LOCALPATH_SIZE + FS_MAX_MOUNTPATH_SIZE)

#define FSA_STATUS_OK                       0
#define FSA_STATUS_END_OF_DIRECTORY         -4
#define FSA_STATUS_END_OF_FILE              -5
#define FSA_STATUS_ALREADY_EXISTS           -22
#define FSA_STATUS_NOT_FOUND                -23
#define FSA_STATUS_NOT_EMPTY                -24

#define DIR_ENTRY_IS_DIRECTORY              0x80000000

void setFsaFdCopyFiles(int fd);

uint32_t folderEmpty(const char * fPath);
uint32_t checkEntry(const char * fPath);

uint32_t copyFile(char* srcFilePath, const char* targetFilePath);
uint32_t copyFolder(char* srcFolderPath, const char* targetFolderPath);

uint32_t createNandBackup(bool fullFlag);
uint32_t restoreNandBackup();

#endif
