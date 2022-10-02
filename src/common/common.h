#ifndef COMMON_H
#define    COMMON_H

#ifdef __cplusplus
extern "C" {
#endif

#include "types.h"
#include "os_defs.h"

#define CAFE_OS_SD_PATH             "/vol/external01"
#define SD_PATH                     "storage_sdcard:"
#define WIIU_PATH                   "/wiiu"

#ifndef MEM_BASE
#define MEM_BASE                    (0x00800000)
#endif

#define ELF_DATA_ADDR               (*(volatile unsigned int*)(MEM_BASE + 0x1300 + 0x00))
#define ELF_DATA_SIZE               (*(volatile unsigned int*)(MEM_BASE + 0x1300 + 0x04))
#define MAIN_ENTRY_ADDR             (*(volatile unsigned int*)(MEM_BASE + 0x1400 + 0x00))
#define OS_FIRMWARE                 (*(volatile unsigned int*)(MEM_BASE + 0x1400 + 0x04))

#define OS_SPECIFICS                ((OsSpecifics*)(MEM_BASE + 0x1500))

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS                0
#endif
#define EXIT_HBL_EXIT               0xFFFFFFFE
#define EXIT_RELAUNCH_ON_LOAD       0xFFFFFFFD


#define OS_MESSAGE_NOBLOCK 0
#define OS_MESSAGE_BLOCK 1

#define OS_EXCEPTION_DSI 2
#define OS_EXCEPTION_ISI 3
#define OS_EXCEPTION_PROGRAM 6
#define OS_EXCEPTION_MODE_THREAD 1
#define OS_EXCEPTION_MODE_GLOBAL_ALL_CORES 4

#define OS_THREAD_ATTR_AFFINITY_NONE    0x0007u        // affinity to run on every core
#define OS_THREAD_ATTR_AFFINITY_CORE0   0x0001u        // run only on core0
#define OS_THREAD_ATTR_AFFINITY_CORE1   0x0002u        // run only on core1
#define OS_THREAD_ATTR_AFFINITY_CORE2   0x0004u        // run only on core2
#define OS_THREAD_ATTR_DETACH           0x0008u        // detached
#define OS_THREAD_ATTR_PINNED_AFFINITY  0x0010u        // pinned (affinitized) to a single core
#define OS_THREAD_ATTR_CHECK_STACK_USE  0x0040u        // check for stack usage
#define OS_THREAD_ATTR_NAME_SENT        0x0080u        // debugger has seen the name
#define OS_THREAD_ATTR_LAST (OS_THREAD_ATTR_DETACH | OS_THREAD_ATTR_PINNED_AFFINITY | OS_THREAD_ATTR_AFFINITY_NONE)

typedef struct OSThread_ OSThread;

typedef struct OSThreadLink_ {
    OSThread *next;
    OSThread *prev;
}  OSThreadLink;

typedef struct OSThreadQueue_ {
    OSThread *head;
    OSThread *tail;
    void *parentStruct;
    uint32_t reserved;
} OSThreadQueue;

typedef struct OSMessage_ {
    uint32_t message;
    uint32_t data0;
    uint32_t data1;
    uint32_t data2;
} OSMessage;

typedef struct OSMessageQueue_ {
    uint32_t tag;
    char *name;
    uint32_t reserved;

    OSThreadQueue sendQueue;
    OSThreadQueue recvQueue;
    OSMessage *messages;
    int msgCount;
    int firstIndex;
    int usedCount;
} OSMessageQueue;

typedef struct OSContext_ {
    char tag[8];

    uint32_t gpr[32];

    uint32_t cr;
    uint32_t lr;
    uint32_t ctr;
    uint32_t xer;

    uint32_t srr0;
    uint32_t srr1;

    uint32_t ex0;
    uint32_t ex1;

    uint32_t exception_type;
    uint32_t reserved;

    double fpscr;
    double fpr[32];

    uint16_t spinLockCount;
    uint16_t state;

    uint32_t gqr[8];
    uint32_t pir;
    double psf[32];

    uint64_t coretime[3];
    uint64_t starttime;

    uint32_t error;
    uint32_t attributes;

    uint32_t pmc1;
    uint32_t pmc2;
    uint32_t pmc3;
    uint32_t pmc4;
    uint32_t mmcr0;
    uint32_t mmcr1;
} OSContext;

typedef enum OSExceptionType {
    OS_EXCEPTION_TYPE_SYSTEM_RESET         = 0,
    OS_EXCEPTION_TYPE_MACHINE_CHECK        = 1,
    OS_EXCEPTION_TYPE_DSI                  = 2,
    OS_EXCEPTION_TYPE_ISI                  = 3,
    OS_EXCEPTION_TYPE_EXTERNAL_INTERRUPT   = 4,
    OS_EXCEPTION_TYPE_ALIGNMENT            = 5,
    OS_EXCEPTION_TYPE_PROGRAM              = 6,
    OS_EXCEPTION_TYPE_FLOATING_POINT       = 7,
    OS_EXCEPTION_TYPE_DECREMENTER          = 8,
    OS_EXCEPTION_TYPE_SYSTEM_CALL          = 9,
    OS_EXCEPTION_TYPE_TRACE                = 10,
    OS_EXCEPTION_TYPE_PERFORMANCE_MONITOR  = 11,
    OS_EXCEPTION_TYPE_BREAKPOINT           = 12,
    OS_EXCEPTION_TYPE_SYSTEM_INTERRUPT     = 13,
    OS_EXCEPTION_TYPE_ICI                  = 14,
} OSExceptionType;

typedef int (*ThreadFunc)(int argc, void *argv);

struct OSThread_ {
    OSContext context;

    uint32_t txtTag;
    uint8_t state;
    uint8_t attr;

    short threadId;
    int suspend;
    int priority;

    char _[0x394 - 0x330 - sizeof(OSThreadLink)];
    OSThreadLink linkActive;

    void *stackBase;
    void *stackEnd;

    ThreadFunc entryPoint;

    char _3A0[0x6A0 - 0x3A0];
};

typedef struct _OSCalendarTime {
    int sec;
    int min;
    int hour;
    int mday;
    int mon;
    int year;
    int wday;
    int yday;
    int msec;
    int usec;
} OSCalendarTime;


typedef struct MCPTitleListType {
    uint64_t titleId;
    uint8_t unknwn[4];
    int8_t path[56];
    uint32_t appType;
    uint8_t unknwn1[0x54 - 0x48];
    uint8_t device;
    uint8_t unknwn2;
    int8_t indexedDevice[10];
    uint8_t unk0x60;
} MCPTitleListType;

#ifdef __cplusplus
}
#endif

#endif    /* COMMON_H */

