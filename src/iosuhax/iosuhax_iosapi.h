#ifndef _IOSUHAX_IOS_API_H_
#define _IOSUHAX_IOS_API_H_

#ifdef __cplusplus
extern "C" {
#endif

#define IOCTL_MEM_WRITE             0x00
#define IOCTL_MEM_READ              0x01
#define IOCTL_SVC                   0x02
#define IOCTL_MEMCPY                0x04
#define IOCTL_REPEATED_WRITE        0x05
#define IOCTL_KERN_READ32           0x06
#define IOCTL_KERN_WRITE32          0x07

#define IOCTL_FSA_OPEN              0x40
#define IOCTL_FSA_CLOSE             0x41
#define IOCTL_FSA_MOUNT             0x42
#define IOCTL_FSA_UNMOUNT           0x43
#define IOCTL_FSA_GETDEVICEINFO     0x44
#define IOCTL_FSA_OPENDIR           0x45
#define IOCTL_FSA_READDIR           0x46
#define IOCTL_FSA_CLOSEDIR          0x47
#define IOCTL_FSA_MAKEDIR           0x48
#define IOCTL_FSA_OPENFILE          0x49
#define IOCTL_FSA_READFILE          0x4A
#define IOCTL_FSA_WRITEFILE         0x4B
#define IOCTL_FSA_STATFILE          0x4C
#define IOCTL_FSA_CLOSEFILE         0x4D
#define IOCTL_FSA_SETFILEPOS        0x4E
#define IOCTL_FSA_GETSTAT           0x4F
#define IOCTL_FSA_REMOVE            0x50
#define IOCTL_FSA_REWINDDIR         0x51
#define IOCTL_FSA_CHDIR             0x52
#define IOCTL_FSA_RENAME            0x53
#define IOCTL_FSA_RAW_OPEN          0x54
#define IOCTL_FSA_RAW_READ          0x55
#define IOCTL_FSA_RAW_WRITE         0x56
#define IOCTL_FSA_RAW_CLOSE         0x57
#define IOCTL_FSA_CHANGEMODE        0x58
//HaxchiFW exclusive
#define IOCTL_FSA_FLUSHVOLUME       0x59
//HaxchiFW exclusive
#define IOCTL_CHECK_IF_IOSUHAX      0x5B
#define IOSUHAX_MAGIC_WORD          0x4E696365

//MochaLite exclusive/ioctl100
#define IPC_CUSTOM_LOG_STRING               0xFF
#define IPC_CUSTOM_META_XML_SWAP_REQUIRED   0xFE
#define IPC_CUSTOM_MEN_RPX_HOOK_COMPLETED   0xFD
#define IPC_CUSTOM_LOAD_CUSTOM_RPX          0xFC
#define IPC_CUSTOM_META_XML_READ            0xFB

#ifdef __cplusplus
}
#endif

#endif //_IOSUHAX_IOS_API_H_
