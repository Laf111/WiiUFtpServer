/****************************************************************************
 * Copyright (C) 2015
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
 ***************************************************************************/
#include "fs_functions.h"
#include "os_functions.h"

EXPORT_DECL(int32_t, FSInit, void);
EXPORT_DECL(int32_t, FSShutdown, void);
EXPORT_DECL(int32_t, FSAddClient, void *pClient, int32_t errHandling);
EXPORT_DECL(int32_t, FSAddClientEx, void *pClient, int32_t unk_zero_param, int32_t errHandling);
EXPORT_DECL(int32_t, FSDelClient, void *pClient);
EXPORT_DECL(void, FSInitCmdBlock, void *pCmd);
EXPORT_DECL(void *, FSGetCurrentCmdBlock, void *pClient);
EXPORT_DECL(int32_t, FSGetMountSource, void *pClient, void *pCmd, int32_t type, void *source, int32_t errHandling);

EXPORT_DECL(int32_t, FSMount, void *pClient, void *pCmd, void *source, char *target, uint32_t bytes, int32_t errHandling);
EXPORT_DECL(int32_t, FSUnmount, void *pClient, void *pCmd, const char *target, int32_t errHandling);

EXPORT_DECL(int32_t, FSGetStat, void *pClient, void *pCmd, const char *path, FSStat *stats, int32_t errHandling);
EXPORT_DECL(int32_t, FSGetStatAsync, void *pClient, void *pCmd, const char *path, void *stats, int32_t error, void *asyncParams);
EXPORT_DECL(int32_t, FSRename, void *pClient, void *pCmd, const char *oldPath, const char *newPath, int32_t error);
EXPORT_DECL(int32_t, FSRenameAsync, void *pClient, void *pCmd, const char *oldPath, const char *newPath, int32_t error, void *asyncParams);
EXPORT_DECL(int32_t, FSRemove, void *pClient, void *pCmd, const char *path, int32_t error);
EXPORT_DECL(int32_t, FSRemoveAsync, void *pClient, void *pCmd, const char *path, int32_t error, void *asyncParams);
EXPORT_DECL(int32_t, FSFlushQuota, void *pClient, void *pCmd, const char* path, int32_t error);
EXPORT_DECL(int32_t, FSFlushQuotaAsync, void *pClient, void *pCmd, const char *path, int32_t error, void *asyncParams);
EXPORT_DECL(int32_t, FSGetFreeSpaceSize, void *pClient, void *pCmd, const char *path, uint64_t *returnedFreeSize, int32_t error);
EXPORT_DECL(int32_t, FSGetFreeSpaceSizeAsync, void *pClient, void *pCmd, const char *path, uint64_t *returnedFreeSize, int32_t error, void *asyncParams);
EXPORT_DECL(int32_t, FSRollbackQuota, void *pClient, void *pCmd, const char *path, int32_t error);
EXPORT_DECL(int32_t, FSRollbackQuotaAsync, void *pClient, void *pCmd, const char *path, int32_t error, void *asyncParams);

EXPORT_DECL(int32_t, FSOpenDir, void *pClient, void *pCmd, const char *path, int32_t *dh, int32_t errHandling);
EXPORT_DECL(int32_t, FSOpenDirAsync, void *pClient, void* pCmd, const char *path, int32_t *handle, int32_t error, void *asyncParams);
EXPORT_DECL(int32_t, FSReadDir, void *pClient, void *pCmd, int32_t dh, FSDirEntry *dir_entry, int32_t errHandling);
EXPORT_DECL(int32_t, FSRewindDir, void *pClient, void *pCmd, int32_t dh, int32_t errHandling);
EXPORT_DECL(int32_t, FSCloseDir, void *pClient, void *pCmd, int32_t dh, int32_t errHandling);
EXPORT_DECL(int32_t, FSChangeDir, void *pClient, void *pCmd, const char *path, int32_t errHandling);
EXPORT_DECL(int32_t, FSChangeDirAsync, void *pClient, void *pCmd, const char *path, int32_t error, void *asyncParams);
EXPORT_DECL(int32_t, FSMakeDir, void *pClient, void *pCmd, const char *path, int32_t errHandling);
EXPORT_DECL(int32_t, FSMakeDirAsync, void *pClient, void *pCmd, const char *path, int32_t error, void *asyncParams);

EXPORT_DECL(int32_t, FSOpenFile, void *pClient, void *pCmd, const char *path, const char *mode, int32_t *fd, int32_t errHandling);
EXPORT_DECL(int32_t, FSOpenFileAsync, void *pClient, void *pCmd, const char *path, const char *mode, int32_t *handle, int32_t error, const void *asyncParams);
EXPORT_DECL(int32_t, FSReadFile, void *pClient, void *pCmd, void *buffer, int32_t size, int32_t count, int32_t fd, int32_t flag, int32_t errHandling);
EXPORT_DECL(int32_t, FSCloseFile, void *pClient, void *pCmd, int32_t fd, int32_t errHandling);

EXPORT_DECL(int32_t, FSFlushFile, void *pClient, void *pCmd, int32_t fd, int32_t error);
EXPORT_DECL(int32_t, FSTruncateFile, void *pClient, void *pCmd, int32_t fd, int32_t error);
EXPORT_DECL(int32_t, FSGetStatFile, void *pClient, void *pCmd, int32_t fd, void *buffer, int32_t error);
EXPORT_DECL(int32_t, FSSetPosFile, void *pClient, void *pCmd, int32_t fd, uint32_t pos, int32_t error);
EXPORT_DECL(int32_t, FSWriteFile, void *pClient, void *pCmd, const void *source, int32_t block_size, int32_t block_count, int32_t fd, int32_t flag, int32_t error);

EXPORT_DECL(int32_t, FSBindMount, void *pClient, void *pCmd, char *source, char *target, int32_t error);
EXPORT_DECL(int32_t, FSBindUnmount, void *pClient, void *pCmd, char *target, int32_t error);

EXPORT_DECL(int32_t, FSMakeQuota, void *pClient, void *pCmd, const char *path,uint32_t mode, uint64_t size, int32_t errHandling);
EXPORT_DECL(int32_t, FSMakeQuotaAsync ,void *pClient, void *pCmd, const char *path,uint32_t mode, uint64_t size, int32_t errHandling,const void  *asyncParams);

EXPORT_DECL(int32_t, FSGetCwd,void * client,void * block,char * buffer,uint32_t bufferSize,uint32_t flags);

void InitFSFunctionPointers(void) {
    if(coreinit_handle == 0) {
        InitAcquireOS();
    };
    uint32_t *funcPointer = 0;

    OS_FIND_EXPORT(coreinit_handle, FSInit);
    OS_FIND_EXPORT(coreinit_handle, FSShutdown);
    OS_FIND_EXPORT(coreinit_handle, FSAddClient);
    OS_FIND_EXPORT(coreinit_handle, FSAddClientEx);
    OS_FIND_EXPORT(coreinit_handle, FSDelClient);
    OS_FIND_EXPORT(coreinit_handle, FSInitCmdBlock);
    OS_FIND_EXPORT(coreinit_handle, FSGetCurrentCmdBlock);
    OS_FIND_EXPORT(coreinit_handle, FSGetMountSource);

    OS_FIND_EXPORT(coreinit_handle, FSMount);
    OS_FIND_EXPORT(coreinit_handle, FSUnmount);

    OS_FIND_EXPORT(coreinit_handle, FSGetStat);
    OS_FIND_EXPORT(coreinit_handle, FSGetStatAsync);
    OS_FIND_EXPORT(coreinit_handle, FSRename);
    OS_FIND_EXPORT(coreinit_handle, FSRenameAsync);
    OS_FIND_EXPORT(coreinit_handle, FSRemove);
    OS_FIND_EXPORT(coreinit_handle, FSRemoveAsync);
    OS_FIND_EXPORT(coreinit_handle, FSFlushQuota);
    OS_FIND_EXPORT(coreinit_handle, FSFlushQuotaAsync);
    OS_FIND_EXPORT(coreinit_handle, FSGetFreeSpaceSize);
    OS_FIND_EXPORT(coreinit_handle, FSGetFreeSpaceSizeAsync);
    OS_FIND_EXPORT(coreinit_handle, FSRollbackQuota);
    OS_FIND_EXPORT(coreinit_handle, FSRollbackQuotaAsync);

    OS_FIND_EXPORT(coreinit_handle, FSOpenDir);
    OS_FIND_EXPORT(coreinit_handle, FSOpenDirAsync);
    OS_FIND_EXPORT(coreinit_handle, FSReadDir);
    OS_FIND_EXPORT(coreinit_handle, FSRewindDir);
    OS_FIND_EXPORT(coreinit_handle, FSCloseDir);
    OS_FIND_EXPORT(coreinit_handle, FSChangeDir);
    OS_FIND_EXPORT(coreinit_handle, FSChangeDirAsync);
    OS_FIND_EXPORT(coreinit_handle, FSMakeDir);
    OS_FIND_EXPORT(coreinit_handle, FSMakeDirAsync);


    OS_FIND_EXPORT(coreinit_handle, FSOpenFile);
    OS_FIND_EXPORT(coreinit_handle, FSOpenFileAsync);
    OS_FIND_EXPORT(coreinit_handle, FSReadFile);
    OS_FIND_EXPORT(coreinit_handle, FSCloseFile);

    OS_FIND_EXPORT(coreinit_handle, FSFlushFile);
    OS_FIND_EXPORT(coreinit_handle, FSTruncateFile);
    OS_FIND_EXPORT(coreinit_handle, FSGetStatFile);
    OS_FIND_EXPORT(coreinit_handle, FSSetPosFile);
    OS_FIND_EXPORT(coreinit_handle, FSWriteFile);

    OS_FIND_EXPORT(coreinit_handle, FSBindMount);
    OS_FIND_EXPORT(coreinit_handle, FSBindUnmount);

    OS_FIND_EXPORT(coreinit_handle, FSMakeQuota);
    OS_FIND_EXPORT(coreinit_handle, FSMakeQuotaAsync);

    OS_FIND_EXPORT(coreinit_handle, FSGetCwd);
}
