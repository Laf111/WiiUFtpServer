#ifndef FS_DEFS_H
#define FS_DEFS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "common/common.h"

/* FS defines and types */
#define FS_MAX_LOCALPATH_SIZE           511
#define FS_MAX_MOUNTPATH_SIZE           128
#define FS_MAX_FULLPATH_SIZE            (FS_MAX_LOCALPATH_SIZE + FS_MAX_MOUNTPATH_SIZE)
#define FS_MAX_ARGPATH_SIZE             FS_MAX_FULLPATH_SIZE

#define FS_STATUS_OK                    0
#define FS_STATUS_EOF                   -2
#define FS_STATUS_FATAL_ERROR           -0x400
#define FS_RET_UNSUPPORTED_CMD          0x0400
#define FS_RET_NO_ERROR                 0x0000
#define FS_RET_ALL_ERROR                (u32)(-1)

#define FS_IO_BUFFER_ALIGN              64

#define FS_STAT_FLAG_IS_DIRECTORY       0x80000000

/* max length of file/dir name */
#define FS_MAX_ENTNAME_SIZE             256

#define FS_SOURCETYPE_EXTERNAL          0
#define FS_SOURCETYPE_HFIO              1

#define FS_MOUNT_SOURCE_SIZE            0x300
#define FS_CLIENT_SIZE                  0x1700
#define FS_CMD_BLOCK_SIZE               0xA80

typedef struct FSClient_ {
    u8 buffer[FS_CLIENT_SIZE];
} FSClient;

typedef struct FSCmdBlock_ {
    u8 buffer[FS_CMD_BLOCK_SIZE];
} FSCmdBlock;

typedef struct {
    u32 flag;
    u32 permission;
    u32 owner_id;
    u32 group_id;
    u32 size;
    u32 alloc_size;
    u64 quota_size;
    u32 ent_id;
    u64 ctime;
    u64 mtime;
    u8 attributes[48];
} __attribute__((packed)) FSStat;

typedef struct {
    FSStat      stat;
    char        name[FS_MAX_ENTNAME_SIZE];
} FSDirEntry;

typedef void (*FSAsyncCallback)(FSClient * pClient, FSCmdBlock * pCmd, s32 result, void *context);
typedef struct {
    FSAsyncCallback userCallback;
    void            *userContext;
    OSMessageQueue  *ioMsgQueue;
} FSAsyncParams;

typedef struct {
    void*               data; // pointer to a FSAsyncResult;
    u32                 unkwn1;
    u32                 unkwn2;
    u32                 unkwn3; // always 0x08
} __attribute__((packed)) FSMessage;

typedef struct FSAsyncResult_ {
    FSAsyncParams       userParams;
    FSMessage           ioMsg;

    FSClient *          client;
    FSCmdBlock *        block;
    u32                 result;
} FSAsyncResult;

#ifdef __cplusplus
}
#endif

#endif  /* FS_DEFS_H */

