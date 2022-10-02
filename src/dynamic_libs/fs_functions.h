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
#ifndef __FS_FUNCTIONS_H_
#define __FS_FUNCTIONS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "common/fs_defs.h"

void InitFSFunctionPointers(void);

extern int32_t (* FSInit)(void);
extern int32_t (* FSShutdown)(void);
extern int32_t (* FSAddClient)(void *pClient, int32_t errHandling);
extern int32_t (* FSAddClientEx)(void *pClient, int32_t unk_zero_param, int32_t errHandling);
extern int32_t (* FSDelClient)(void *pClient);
extern void (* FSInitCmdBlock)(void *pCmd);
extern void *(* FSGetCurrentCmdBlock)(void *pClient);
extern int32_t (* FSGetMountSource)(void *pClient, void *pCmd, int32_t type, void *source, int32_t errHandling);

extern int32_t (* FSMount)(void *pClient, void *pCmd, void *source, char *target, uint32_t bytes, int32_t errHandling);
extern int32_t (* FSUnmount)(void *pClient, void *pCmd, const char *target, int32_t errHandling);
extern int32_t (* FSRename)(void *pClient, void *pCmd, const char *oldPath, const char *newPath, int32_t error);
extern int32_t (* FSRenameAsync)(void *pClient, void *pCmd, const char *oldPath, const char *newPath, int32_t error, void *asyncParams);
extern int32_t (* FSRemove)(void *pClient, void *pCmd, const char *path, int32_t error);
extern int32_t (* FSRemoveAsync)(void *pClient, void *pCmd, const char *path, int32_t error, void *asyncParams);

extern int32_t (* FSGetStat)(void *pClient, void *pCmd, const char *path, FSStat *stats, int32_t errHandling);
extern int32_t (* FSGetStatAsync)(void *pClient, void *pCmd, const char *path, void *stats, int32_t error, void *asyncParams);
extern int32_t (* FSRename)(void *pClient, void *pCmd, const char *oldPath, const char *newPath, int32_t error);
extern int32_t (* FSRenameAsync)(void *pClient, void *pCmd, const char *oldPath, const char *newPath, int32_t error, void *asyncParams);
extern int32_t (* FSRemove)(void *pClient, void *pCmd, const char *path, int32_t error);
extern int32_t (* FSRemoveAsync)(void *pClient, void *pCmd, const char *path, int32_t error, void *asyncParams);
extern int32_t (* FSFlushQuota)(void *pClient, void *pCmd, const char* path, int32_t error);
extern int32_t (* FSFlushQuotaAsync)(void *pClient, void *pCmd, const char *path, int32_t error, void *asyncParams);
extern int32_t (* FSGetFreeSpaceSize)(void *pClient, void *pCmd, const char *path, uint64_t *returnedFreeSize, int32_t error);
extern int32_t (* FSGetFreeSpaceSizeAsync)(void *pClient, void *pCmd, const char *path, uint64_t *returnedFreeSize, int32_t error, void *asyncParams);
extern int32_t (* FSRollbackQuota)(void *pClient, void *pCmd, const char *path, int32_t error);
extern int32_t (* FSRollbackQuotaAsync)(void *pClient, void *pCmd, const char *path, int32_t error, void *asyncParams);

extern int32_t (* FSOpenDir)(void *pClient, void *pCmd, const char *path, int32_t *dh, int32_t errHandling);
extern int32_t (* FSOpenDirAsync)(void *pClient, void* pCmd, const char *path, int32_t *handle, int32_t error, void *asyncParams);
extern int32_t (* FSReadDir)(void *pClient, void *pCmd, int32_t dh, FSDirEntry *dir_entry, int32_t errHandling);
extern int32_t (* FSRewindDir)(void *pClient, void *pCmd, int32_t dh, int32_t errHandling);
extern int32_t (* FSCloseDir)(void *pClient, void *pCmd, int32_t dh, int32_t errHandling);
extern int32_t (* FSChangeDir)(void *pClient, void *pCmd, const char *path, int32_t errHandling);
extern int32_t (* FSChangeDirAsync)(void *pClient, void *pCmd, const char *path, int32_t error, void *asyncParams);
extern int32_t (* FSMakeDir)(void *pClient, void *pCmd, const char *path, int32_t errHandling);
extern int32_t (* FSMakeDirAsync)(void *pClient, void *pCmd, const char *path, int32_t error, void *asyncParams);

extern int32_t (* FSOpenFile)(void *pClient, void *pCmd, const char *path, const char *mode, int32_t *fd, int32_t errHandling);
extern int32_t (* FSOpenFileAsync)(void *pClient, void *pCmd, const char *path, const char *mode, int32_t *handle, int32_t error, const void *asyncParams);
extern int32_t (* FSReadFile)(void *pClient, void *pCmd, void *buffer, int32_t size, int32_t count, int32_t fd, int32_t flag, int32_t errHandling);
extern int32_t (* FSCloseFile)(void *pClient, void *pCmd, int32_t fd, int32_t errHandling);

extern int32_t (* FSFlushFile)(void *pClient, void *pCmd, int32_t fd, int32_t error);
extern int32_t (* FSTruncateFile)(void *pClient, void *pCmd, int32_t fd, int32_t error);
extern int32_t (* FSGetStatFile)(void *pClient, void *pCmd, int32_t fd, void *buffer, int32_t error);
extern int32_t (* FSSetPosFile)(void *pClient, void *pCmd, int32_t fd, uint32_t pos, int32_t error);
extern int32_t (* FSWriteFile)(void *pClient, void *pCmd, const void *source, int32_t block_size, int32_t block_count, int32_t fd, int32_t flag, int32_t error);

extern int32_t (* FSBindMount)(void *pClient, void *pCmd, char *source, char *target, int32_t error);
extern int32_t (* FSBindUnmount)(void *pClient, void *pCmd, char *target, int32_t error);

extern int32_t (* FSMakeQuota)( void *pClient, void *pCmd, const char *path,uint32_t mode, uint64_t size, int32_t errHandling);
extern int32_t (* FSMakeQuotaAsync)(void *pClient, void *pCmd, const char *path,uint32_t mode, uint64_t size, int32_t errHandling,const void  *asyncParams);

extern int32_t (* FSGetCwd)(void * client,void * block,char * buffer,uint32_t bufferSize,uint32_t flags);


#ifdef __cplusplus
}
#endif

#endif // __FS_FUNCTIONS_H_
