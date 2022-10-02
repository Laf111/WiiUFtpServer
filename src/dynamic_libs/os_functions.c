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
#include "os_functions.h"

uint32_t coreinit_handle __attribute__((section(".data"))) = 0;

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! Lib handle functions
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
EXPORT_DECL(int32_t, OSDynLoad_Acquire, const char* rpl, uint32_t *handle);
EXPORT_DECL(int32_t, OSDynLoad_FindExport, uint32_t handle, int32_t isdata, const char *symbol, void *address);

EXPORT_DECL(void, OSDynLoad_Release, uint32_t handle);

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! Security functions
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
EXPORT_DECL(int32_t, OSGetSecurityLevel, void);
EXPORT_DECL(int32_t, OSForceFullRelaunch, void);

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! Thread functions
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
EXPORT_DECL(int32_t, OSCreateThread, OSThread *thread, int32_t (*callback)(int32_t, void*), int32_t argc, void *args, uint32_t stack, uint32_t stack_size, int32_t priority, uint32_t attr);

EXPORT_DECL(void, OSEnableInterrupts, void);
EXPORT_DECL(void, __OSClearAndEnableInterrupt, void);
EXPORT_DECL(int32_t, OSIsInterruptEnabled, void);
EXPORT_DECL(int32_t, OSIsDebuggerPresent, void);
EXPORT_DECL(void, OSRestoreInterrupts, void);
EXPORT_DECL(void, OSSetDABR, int32_t, int32_t, int32_t, int32_t);
EXPORT_DECL(void, OSSetIABR, int32_t, int32_t);

EXPORT_DECL(int32_t, OSResumeThread, OSThread *thread);
EXPORT_DECL(int32_t, OSSuspendThread, OSThread *thread);
EXPORT_DECL(int32_t, OSIsThreadTerminated, OSThread *thread);
EXPORT_DECL(int32_t, OSIsThreadSuspended, OSThread *thread);
EXPORT_DECL(int32_t, OSSetThreadPriority, OSThread * thread, int32_t priority);
EXPORT_DECL(int32_t, OSJoinThread, OSThread * thread, int32_t * ret_val);
EXPORT_DECL(void, OSDetachThread, OSThread * thread);
EXPORT_DECL(OSThread *,OSGetCurrentThread,void);
EXPORT_DECL(const char *,OSGetThreadName,OSThread * thread);
EXPORT_DECL(void ,OSYieldThread, void);
EXPORT_DECL(void ,OSGetActiveThreadLink,OSThread * thread, void* link);
EXPORT_DECL(uint32_t ,OSGetThreadAffinity,OSThread * thread);
EXPORT_DECL(int32_t ,OSGetThreadPriority,OSThread * thread);
EXPORT_DECL(void ,OSSetThreadName,OSThread * thread, const char *name);
EXPORT_DECL(int32_t, OSGetCoreId, void);
EXPORT_DECL(void, OSSleepTicks, uint64_t ticks);
EXPORT_DECL(uint64_t, OSGetTick, void);
EXPORT_DECL(uint64_t, OSGetTime, void);
EXPORT_DECL(uint64_t, OSGetSystemTime, void);
EXPORT_DECL(void, OSTicksToCalendarTime, uint64_t time, OSCalendarTime * calendarTime);

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! Message functions
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
EXPORT_DECL(void,OSInitMessageQueue,OSMessageQueue *queue, OSMessage *messages, int32_t size);
EXPORT_DECL(uint32_t,OSSendMessage,OSMessageQueue *queue, OSMessage *message, int32_t flags);
EXPORT_DECL(uint32_t,OSReceiveMessage,OSMessageQueue *queue, OSMessage *message, int32_t flags);

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! Mutex functions
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
EXPORT_DECL(void, OSInitMutex, void* mutex);
EXPORT_DECL(void, OSLockMutex, void* mutex);
EXPORT_DECL(void, OSUnlockMutex, void* mutex);
EXPORT_DECL(int32_t, OSTryLockMutex, void* mutex);

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! System functions
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
EXPORT_DECL(uint64_t, OSGetTitleID, void);
EXPORT_DECL(void, OSGetArgcArgv, int32_t* argc, char*** argv);
EXPORT_DECL(void, __Exit, void);
EXPORT_DECL(void, OSFatal, const char* msg);
EXPORT_DECL(void *, OSSetExceptionCallback, uint8_t exceptionType, exception_callback newCallback);
EXPORT_DECL(void *, OSSetExceptionCallbackEx, int32_t unkwn, uint8_t exceptionType, exception_callback newCallback);
EXPORT_DECL(void , OSLoadContext, OSContext * context);
EXPORT_DECL(void, DCFlushRange, const void *addr, uint32_t length);
EXPORT_DECL(void, DCStoreRange, const void *addr, uint32_t length);
EXPORT_DECL(void, ICInvalidateRange, const void *addr, uint32_t length);
EXPORT_DECL(void*, OSEffectiveToPhysical, const void*);
EXPORT_DECL(void*, __OSPhysicalToEffectiveUncached, const void*);
EXPORT_DECL(int32_t, __OSValidateAddressSpaceRange, int32_t, void*, int32_t);
EXPORT_DECL(int32_t, __os_snprintf, char* s, int32_t n, const char * format, ...);
EXPORT_DECL(int32_t *, __gh_errno_ptr, void);

EXPORT_DECL(void, OSScreenInit, void);
EXPORT_DECL(void, OSScreenShutdown, void);
EXPORT_DECL(uint32_t, OSScreenGetBufferSizeEx, uint32_t bufferNum);
EXPORT_DECL(int32_t, OSScreenSetBufferEx, uint32_t bufferNum, void * addr);
EXPORT_DECL(int32_t, OSScreenClearBufferEx, uint32_t bufferNum, uint32_t temp);
EXPORT_DECL(int32_t, OSScreenFlipBuffersEx, uint32_t bufferNum);
EXPORT_DECL(int32_t, OSScreenPutFontEx, uint32_t bufferNum, uint32_t posX, uint32_t posY, const char * buffer);
EXPORT_DECL(int32_t, OSScreenEnableEx, uint32_t bufferNum, int32_t enable);
EXPORT_DECL(uint32_t, OSScreenPutPixelEx, uint32_t bufferNum, uint32_t posX, uint32_t posY, uint32_t color);

EXPORT_DECL(void, DisassemblePPCRange, void *, void *, DisasmReport, DisasmGetSym, uint32_t);
EXPORT_DECL(bool, DisassemblePPCOpcode, uint32_t *, char *, uint32_t, DisasmGetSym, uint32_t);
EXPORT_DECL(void*, OSGetSymbolName, uint32_t, uint8_t *, uint32_t);
EXPORT_DECL(void*, OSGetSymbolNameEx, uint32_t, uint8_t *, uint32_t);
EXPORT_DECL(int, OSIsDebuggerInitialized, void);

EXPORT_DECL(bool, OSGetSharedData, uint32_t type, uint32_t unk_r4, uint8_t *addr, uint32_t *size);
EXPORT_DECL(int, OSShutdown, int status);


//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! Memory functions
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
EXPORT_VAR(uint32_t *, pMEMAllocFromDefaultHeapEx);
EXPORT_VAR(uint32_t *, pMEMAllocFromDefaultHeap);
EXPORT_VAR(uint32_t *, pMEMFreeToDefaultHeap);

EXPORT_DECL(void *, MEMAllocFromAllocator, void * allocator, uint32_t size);
EXPORT_DECL(void, MEMFreeToAllocator, void * allocator, void* address);

EXPORT_DECL(int32_t, MEMGetBaseHeapHandle, int32_t mem_arena);
EXPORT_DECL(uint32_t, MEMGetTotalFreeSizeForExpHeap, int32_t heap);
EXPORT_DECL(uint32_t, MEMGetAllocatableSizeForExpHeapEx, int32_t heap, int32_t align);
EXPORT_DECL(uint32_t, MEMGetAllocatableSizeForFrmHeapEx, int32_t heap, int32_t align);
EXPORT_DECL(void *, MEMAllocFromFrmHeapEx, int32_t heap, uint32_t size, int32_t align);
EXPORT_DECL(void, MEMFreeToFrmHeap, int32_t heap, int32_t mode);
EXPORT_DECL(void *, MEMAllocFromExpHeapEx, int32_t heap, uint32_t size, int32_t align);
EXPORT_DECL(int32_t , MEMCreateExpHeapEx, void* address, uint32_t size, unsigned short flags);
EXPORT_DECL(int32_t , MEMCreateFrmHeapEx, void* address, uint32_t size, unsigned short flags);
EXPORT_DECL(void *, MEMDestroyExpHeap, int32_t heap);
EXPORT_DECL(void, MEMFreeToExpHeap, int32_t heap, void* ptr);
EXPORT_DECL(void *, OSAllocFromSystem, uint32_t size, int32_t alignment);
EXPORT_DECL(void, OSFreeToSystem, void *addr);
EXPORT_DECL(int32_t, OSIsAddressValid, const void *ptr);
EXPORT_DECL(int32_t, MEMFindParentHeap, int32_t heap);
EXPORT_DECL(int32_t, OSGetMemBound, int32_t type, uint32_t * startAddress, uint32_t * size);


//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! MCP functions
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
EXPORT_DECL(int32_t, MCP_Open, void);
EXPORT_DECL(int32_t, MCP_Close, int32_t handle);
EXPORT_DECL(int32_t, MCP_TitleCount, int32_t handle);
EXPORT_DECL(int32_t, MCP_TitleList, int32_t handle, int32_t *res, void *data, int32_t count);
EXPORT_DECL(int32_t, MCP_GetOwnTitleInfo, int32_t handle, void *data);
EXPORT_DECL(void*, MCP_GetDeviceId, int32_t handle, uint32_t * id);

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! Loader functions (not real rpl)
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
EXPORT_DECL(int32_t, LiWaitIopComplete, int32_t unknown_syscall_arg_r3, int32_t * remaining_bytes);
EXPORT_DECL(int32_t, LiWaitIopCompleteWithInterrupts, int32_t unknown_syscall_arg_r3, int32_t * remaining_bytes);
EXPORT_DECL(void, addr_LiWaitOneChunk, void);
EXPORT_DECL(void, addr_sgIsLoadingBuffer, void);
EXPORT_DECL(void, addr_gDynloadInitialized, void);

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! Kernel function addresses
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
EXPORT_DECL(void, addr_PrepareTitle_hook, void);

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! Other function addresses
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
EXPORT_DECL(void, DCInvalidateRange, void *buffer, uint32_t length);
EXPORT_DECL(int32_t, OSDynLoad_GetModuleName, int32_t handle, char *name_buffer, int32_t *name_buffer_size);
EXPORT_DECL(int32_t, OSIsHomeButtonMenuEnabled, void);
EXPORT_DECL(void, OSEnableHomeButtonMenu, int32_t);
EXPORT_DECL(int32_t, OSSetScreenCapturePermissionEx, int32_t tvEnabled, int32_t drcEnabled);


//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! Energy Saver functions
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//Burn-in Reduction
EXPORT_DECL(int32_t, IMEnableDim,void);
EXPORT_DECL(int32_t, IMDisableDim,void);
EXPORT_DECL(int32_t, IMIsDimEnabled,int32_t * result);
//Auto power down
EXPORT_DECL(int32_t, IMEnableAPD,void);
EXPORT_DECL(int32_t, IMDisableAPD,void);
EXPORT_DECL(int32_t, IMIsAPDEnabled,int32_t * result);
EXPORT_DECL(int32_t, IMIsAPDEnabledBySysSettings,int32_t * result);

EXPORT_DECL(int32_t, OSSendAppSwitchRequest,int32_t param,void* unknown1,void* unknown2);

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! IOS functions
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------

EXPORT_DECL(int32_t, IOS_Ioctl,int32_t fd, uint32_t request, void *input_buffer,uint32_t input_buffer_len, void *output_buffer, uint32_t output_buffer_len);
EXPORT_DECL(int32_t, IOS_IoctlAsync,int32_t fd, uint32_t request, void *input_buffer,uint32_t input_buffer_len, void *output_buffer, uint32_t output_buffer_len, void *cb, void *cbarg);
EXPORT_DECL(int32_t, IOS_Open,char *path, uint32_t mode);
EXPORT_DECL(int32_t, IOS_Close,int32_t fd);

void _os_find_export(uint32_t handle, const char *funcName, void *funcPointer) {
    OSDynLoad_FindExport(handle, 0, funcName, funcPointer);

    if(!*(uint32_t *)funcPointer) {
        /*
         * This is effectively OSFatal("Function %s is NULL", funcName),
         * but we can't rely on any library functions like snprintf or
         * strcpy at this point.
         *
         * Buffer bounds are not checked. Beware!
         */
        char buf[256], *bufp = buf;
        const char a[] = "Function ", b[] = " is NULL", *p;
        unsigned int i;

        for (i = 0; i < sizeof(a) - 1; i++)
            *bufp++ = a[i];

        for (p = funcName; *p; p++)
            *bufp++ = *p;

        for (i = 0; i < sizeof(b) - 1; i++)
            *bufp++ = b[i];

        *bufp++ = '\0';

        OSFatal(buf);
    }
}

void InitAcquireOS(void) {
    //!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    //! Lib handle functions
    //!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    EXPORT_FUNC_WRITE(OSDynLoad_Acquire, (int32_t (*)(const char*, unsigned *))OS_SPECIFICS->addr_OSDynLoad_Acquire);
    EXPORT_FUNC_WRITE(OSDynLoad_FindExport, (int32_t (*)(uint32_t, int32_t, const char *, void *))OS_SPECIFICS->addr_OSDynLoad_FindExport);

    OSDynLoad_Acquire("coreinit.rpl", &coreinit_handle);
}

void InitOSFunctionPointers(void) {
    uint32_t *funcPointer = 0;

    InitAcquireOS();
    
    OS_FIND_EXPORT(coreinit_handle, OSDynLoad_Release);

    //!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    //! Security functions
    //!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------

    OS_FIND_EXPORT(coreinit_handle, OSGetSecurityLevel);
    OS_FIND_EXPORT(coreinit_handle, OSForceFullRelaunch);
    //!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    //! System functions
    //!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    OS_FIND_EXPORT(coreinit_handle, OSFatal);
    OS_FIND_EXPORT(coreinit_handle, OSGetTitleID);
    OS_FIND_EXPORT(coreinit_handle, OSGetArgcArgv);
    OS_FIND_EXPORT(coreinit_handle, OSSetExceptionCallback);
    OS_FIND_EXPORT(coreinit_handle, OSSetExceptionCallbackEx);
    OS_FIND_EXPORT(coreinit_handle, OSLoadContext);
    OS_FIND_EXPORT(coreinit_handle, DCFlushRange);
    OS_FIND_EXPORT(coreinit_handle, DCStoreRange);
    OS_FIND_EXPORT(coreinit_handle, ICInvalidateRange);
    OS_FIND_EXPORT(coreinit_handle, OSEffectiveToPhysical);
    OS_FIND_EXPORT(coreinit_handle, __OSPhysicalToEffectiveUncached);
    OS_FIND_EXPORT(coreinit_handle, __OSValidateAddressSpaceRange);
    OS_FIND_EXPORT(coreinit_handle, __os_snprintf);
    OS_FIND_EXPORT(coreinit_handle, __gh_errno_ptr);

    OSDynLoad_FindExport(coreinit_handle, 0, "_Exit", &__Exit);

    OS_FIND_EXPORT(coreinit_handle, OSScreenInit);
    OS_FIND_EXPORT(coreinit_handle, OSScreenShutdown);
    OS_FIND_EXPORT(coreinit_handle, OSScreenGetBufferSizeEx);
    OS_FIND_EXPORT(coreinit_handle, OSScreenSetBufferEx);
    OS_FIND_EXPORT(coreinit_handle, OSScreenClearBufferEx);
    OS_FIND_EXPORT(coreinit_handle, OSScreenFlipBuffersEx);
    OS_FIND_EXPORT(coreinit_handle, OSScreenPutFontEx);
    OS_FIND_EXPORT(coreinit_handle, OSScreenEnableEx);
    OS_FIND_EXPORT(coreinit_handle, OSScreenPutPixelEx);

    OS_FIND_EXPORT(coreinit_handle, DisassemblePPCRange);
    OS_FIND_EXPORT(coreinit_handle, DisassemblePPCOpcode);
    OS_FIND_EXPORT(coreinit_handle, OSGetSymbolName);
    OS_FIND_EXPORT(coreinit_handle, OSGetSymbolNameEx);
    OS_FIND_EXPORT(coreinit_handle, OSIsDebuggerInitialized);

    OS_FIND_EXPORT(coreinit_handle, OSGetSharedData);
    OS_FIND_EXPORT(coreinit_handle, OSShutdown);
    
    //!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    //! Thread functions
    //!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    OS_FIND_EXPORT(coreinit_handle, OSEnableInterrupts);
    OS_FIND_EXPORT(coreinit_handle, __OSClearAndEnableInterrupt);
    OS_FIND_EXPORT(coreinit_handle, OSIsInterruptEnabled);
    OS_FIND_EXPORT(coreinit_handle, OSIsDebuggerPresent);
    OS_FIND_EXPORT(coreinit_handle, OSRestoreInterrupts);
    OS_FIND_EXPORT(coreinit_handle, OSSetDABR);
    OS_FIND_EXPORT(coreinit_handle, OSSetIABR);

    OS_FIND_EXPORT(coreinit_handle, OSCreateThread);
    OS_FIND_EXPORT(coreinit_handle, OSResumeThread);
    OS_FIND_EXPORT(coreinit_handle, OSSuspendThread);
    OS_FIND_EXPORT(coreinit_handle, OSIsThreadTerminated);
    OS_FIND_EXPORT(coreinit_handle, OSIsThreadSuspended);
    OS_FIND_EXPORT(coreinit_handle, OSJoinThread);
    OS_FIND_EXPORT(coreinit_handle, OSSetThreadPriority);
    OS_FIND_EXPORT(coreinit_handle, OSDetachThread);
    OS_FIND_EXPORT(coreinit_handle, OSGetCurrentThread);
    OS_FIND_EXPORT(coreinit_handle, OSGetThreadName);
    OS_FIND_EXPORT(coreinit_handle, OSYieldThread);
    OS_FIND_EXPORT(coreinit_handle, OSGetActiveThreadLink);
    OS_FIND_EXPORT(coreinit_handle, OSGetThreadAffinity);
    OS_FIND_EXPORT(coreinit_handle, OSGetThreadPriority);
    OS_FIND_EXPORT(coreinit_handle, OSSetThreadName);
    OS_FIND_EXPORT(coreinit_handle, OSGetCoreId);

    OS_FIND_EXPORT(coreinit_handle, OSSleepTicks);
    OS_FIND_EXPORT(coreinit_handle, OSGetTick);
    OS_FIND_EXPORT(coreinit_handle, OSGetTime);
    OS_FIND_EXPORT(coreinit_handle, OSGetSystemTime);
    OS_FIND_EXPORT(coreinit_handle, OSTicksToCalendarTime);

    //!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    //! Message functions
    //!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    OS_FIND_EXPORT(coreinit_handle, OSInitMessageQueue);
    OS_FIND_EXPORT(coreinit_handle, OSSendMessage);
    OS_FIND_EXPORT(coreinit_handle, OSReceiveMessage);

    //!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    //! Mutex functions
    //!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    OS_FIND_EXPORT(coreinit_handle, OSInitMutex);
    OS_FIND_EXPORT(coreinit_handle, OSLockMutex);
    OS_FIND_EXPORT(coreinit_handle, OSUnlockMutex);
    OS_FIND_EXPORT(coreinit_handle, OSTryLockMutex);
    //!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    //! MCP functions
    //!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    OS_FIND_EXPORT(coreinit_handle, MCP_Open);
    OS_FIND_EXPORT(coreinit_handle, MCP_Close);
    OS_FIND_EXPORT(coreinit_handle, MCP_TitleCount);
    OS_FIND_EXPORT(coreinit_handle, MCP_TitleList);
    OS_FIND_EXPORT(coreinit_handle, MCP_GetOwnTitleInfo);
    OS_FIND_EXPORT(coreinit_handle, MCP_GetDeviceId);

    //!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    //! Memory functions
    //!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    OSDynLoad_FindExport(coreinit_handle, 1, "MEMAllocFromDefaultHeapEx", &pMEMAllocFromDefaultHeapEx);
    OSDynLoad_FindExport(coreinit_handle, 1, "MEMAllocFromDefaultHeap", &pMEMAllocFromDefaultHeap);
    OSDynLoad_FindExport(coreinit_handle, 1, "MEMFreeToDefaultHeap", &pMEMFreeToDefaultHeap);

    OS_FIND_EXPORT(coreinit_handle, MEMAllocFromAllocator);
    OS_FIND_EXPORT(coreinit_handle, MEMFreeToAllocator);
    OS_FIND_EXPORT(coreinit_handle, MEMGetBaseHeapHandle);
    OS_FIND_EXPORT(coreinit_handle, MEMGetTotalFreeSizeForExpHeap);
    OS_FIND_EXPORT(coreinit_handle, MEMGetAllocatableSizeForExpHeapEx);
    OS_FIND_EXPORT(coreinit_handle, MEMGetAllocatableSizeForFrmHeapEx);
    OS_FIND_EXPORT(coreinit_handle, MEMAllocFromFrmHeapEx);
    OS_FIND_EXPORT(coreinit_handle, MEMFreeToFrmHeap);
    OS_FIND_EXPORT(coreinit_handle, MEMAllocFromExpHeapEx);
    OS_FIND_EXPORT(coreinit_handle, MEMCreateExpHeapEx);
    OS_FIND_EXPORT(coreinit_handle, MEMCreateFrmHeapEx);
    OS_FIND_EXPORT(coreinit_handle, MEMDestroyExpHeap);
    OS_FIND_EXPORT(coreinit_handle, MEMFreeToExpHeap);
    OS_FIND_EXPORT(coreinit_handle, OSAllocFromSystem);
    OS_FIND_EXPORT(coreinit_handle, OSFreeToSystem);
    OS_FIND_EXPORT(coreinit_handle, OSIsAddressValid);
    OS_FIND_EXPORT(coreinit_handle, MEMFindParentHeap);
    OS_FIND_EXPORT(coreinit_handle, OSGetMemBound);

    //!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    //! Other function addresses
    //!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    OS_FIND_EXPORT(coreinit_handle, DCInvalidateRange);
    OS_FIND_EXPORT(coreinit_handle, OSDynLoad_GetModuleName);
    OS_FIND_EXPORT(coreinit_handle, OSIsHomeButtonMenuEnabled);
    OS_FIND_EXPORT(coreinit_handle, OSEnableHomeButtonMenu);
    OS_FIND_EXPORT(coreinit_handle, OSSetScreenCapturePermissionEx);

    //!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    //! Energy Saver functions
    //!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    //Burn-in Reduction
    OS_FIND_EXPORT(coreinit_handle, IMEnableDim);
    OS_FIND_EXPORT(coreinit_handle, IMDisableDim);
    OS_FIND_EXPORT(coreinit_handle, IMIsDimEnabled);
    //Auto power down
    OS_FIND_EXPORT(coreinit_handle, IMEnableAPD);
    OS_FIND_EXPORT(coreinit_handle, IMDisableAPD);
    OS_FIND_EXPORT(coreinit_handle, IMIsAPDEnabled);
    OS_FIND_EXPORT(coreinit_handle, IMIsAPDEnabledBySysSettings);

    OS_FIND_EXPORT(coreinit_handle, OSSendAppSwitchRequest);

    //!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    //! IOS functions
    //!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    OS_FIND_EXPORT(coreinit_handle, IOS_Ioctl);
    OS_FIND_EXPORT(coreinit_handle, IOS_IoctlAsync);
    OS_FIND_EXPORT(coreinit_handle, IOS_Open);
    OS_FIND_EXPORT(coreinit_handle, IOS_Close);

    //!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    //! Special non library functions
    //!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
    if(OS_FIRMWARE == 550) {
        EXPORT_FUNC_WRITE(LiWaitIopComplete, (int32_t (*)(int32_t, int32_t *))0x01010180);
        EXPORT_FUNC_WRITE(LiWaitIopCompleteWithInterrupts, (int32_t (*)(int32_t, int32_t *))0x0101006C);
        EXPORT_FUNC_WRITE(addr_LiWaitOneChunk, (int32_t (*)(int32_t, int32_t *))0x0100080C);
        EXPORT_FUNC_WRITE(addr_PrepareTitle_hook, (int32_t (*)(int32_t, int32_t *))0xFFF184E4);

        EXPORT_FUNC_WRITE(addr_sgIsLoadingBuffer, (int32_t (*)(int32_t, int32_t *))0xEFE19E80);
        EXPORT_FUNC_WRITE(addr_gDynloadInitialized, (int32_t (*)(int32_t, int32_t *))0xEFE13DBC);
    } else if(OS_FIRMWARE == 532 || OS_FIRMWARE == 540) {
        EXPORT_FUNC_WRITE(LiWaitIopComplete, (int32_t (*)(int32_t, int32_t *))0x0100FFA4);                // loader.elf
        EXPORT_FUNC_WRITE(LiWaitIopCompleteWithInterrupts, (int32_t (*)(int32_t, int32_t *))0x0100FE90);  // loader.elf
        EXPORT_FUNC_WRITE(addr_LiWaitOneChunk, (int32_t (*)(int32_t, int32_t *))0x010007EC);              // loader.elf
        EXPORT_FUNC_WRITE(addr_PrepareTitle_hook, (int32_t (*)(int32_t, int32_t *))0xFFF18558);           // kernel.elf

        EXPORT_FUNC_WRITE(addr_sgIsLoadingBuffer, (int32_t (*)(int32_t, int32_t *))0xEFE19D00);           // loader.elf
        EXPORT_FUNC_WRITE(addr_gDynloadInitialized, (int32_t (*)(int32_t, int32_t *))0xEFE13C3C);         // loader.elf
    } else if(OS_FIRMWARE == 500 || OS_FIRMWARE == 510) {
        EXPORT_FUNC_WRITE(LiWaitIopComplete, (int32_t (*)(int32_t, int32_t *))0x0100FBC4);
        EXPORT_FUNC_WRITE(LiWaitIopCompleteWithInterrupts, (int32_t (*)(int32_t, int32_t *))0x0100FAB0);
        EXPORT_FUNC_WRITE(addr_LiWaitOneChunk, (int32_t (*)(int32_t, int32_t *))0x010007EC);
        EXPORT_FUNC_WRITE(addr_PrepareTitle_hook, (int32_t (*)(int32_t, int32_t *))0xFFF18534);

        EXPORT_FUNC_WRITE(addr_sgIsLoadingBuffer, (int32_t (*)(int32_t, int32_t *))0xEFE19D00);
        EXPORT_FUNC_WRITE(addr_gDynloadInitialized, (int32_t (*)(int32_t, int32_t *))0xEFE13C3C);
    } else if(OS_FIRMWARE == 410) {
        EXPORT_FUNC_WRITE(LiWaitIopComplete, (int32_t (*)(int32_t, int32_t *))0x0100F78C);
        EXPORT_FUNC_WRITE(LiWaitIopCompleteWithInterrupts, (int32_t (*)(int32_t, int32_t *))0x0100F678);
        EXPORT_FUNC_WRITE(addr_LiWaitOneChunk, (int32_t (*)(int32_t, int32_t *))0x010007F8);
        EXPORT_FUNC_WRITE(addr_PrepareTitle_hook, (int32_t (*)(int32_t, int32_t *))0xFFF166DC);

        EXPORT_FUNC_WRITE(addr_sgIsLoadingBuffer, (int32_t (*)(int32_t, int32_t *))0xEFE19CC0);
        EXPORT_FUNC_WRITE(addr_gDynloadInitialized, (int32_t (*)(int32_t, int32_t *))0xEFE13BFC);
    } else if(OS_FIRMWARE == 400) { //same for 402 and 403
        EXPORT_FUNC_WRITE(LiWaitIopComplete, (int32_t (*)(int32_t, int32_t *))0x0100F78C);
        EXPORT_FUNC_WRITE(LiWaitIopCompleteWithInterrupts, (int32_t (*)(int32_t, int32_t *))0x0100F678);
        EXPORT_FUNC_WRITE(addr_LiWaitOneChunk, (int32_t (*)(int32_t, int32_t *))0x010007F8);
        EXPORT_FUNC_WRITE(addr_PrepareTitle_hook, (int32_t (*)(int32_t, int32_t *))0xFFF15E70);

        EXPORT_FUNC_WRITE(addr_sgIsLoadingBuffer, (int32_t (*)(int32_t, int32_t *))0xEFE19CC0);
        EXPORT_FUNC_WRITE(addr_gDynloadInitialized, (int32_t (*)(int32_t, int32_t *))0xEFE13BFC);
    } else if(OS_FIRMWARE == 310) {
        EXPORT_FUNC_WRITE(LiWaitIopComplete, (int32_t (*)(int32_t, int32_t *))0x0100C4E4);
        EXPORT_FUNC_WRITE(LiWaitIopCompleteWithInterrupts, (int32_t (*)(int32_t, int32_t *))0x0100C3D4);
        EXPORT_FUNC_WRITE(addr_LiWaitOneChunk, (int32_t (*)(int32_t, int32_t *))0x010004D8);
        EXPORT_FUNC_WRITE(addr_PrepareTitle_hook, (int32_t (*)(int32_t, int32_t *))0xFFF15A0C);

        EXPORT_FUNC_WRITE(addr_sgIsLoadingBuffer, (int32_t (*)(int32_t, int32_t *))0xEFE19340);
        EXPORT_FUNC_WRITE(addr_gDynloadInitialized, (int32_t (*)(int32_t, int32_t *))0xEFE1329C);
    } else if(OS_FIRMWARE == 300) {
        EXPORT_FUNC_WRITE(LiWaitIopComplete, (int32_t (*)(int32_t, int32_t *))0x0100C4E4);
        EXPORT_FUNC_WRITE(LiWaitIopCompleteWithInterrupts, (int32_t (*)(int32_t, int32_t *))0x0100C3D4);
        EXPORT_FUNC_WRITE(addr_LiWaitOneChunk, (int32_t (*)(int32_t, int32_t *))0x010004D8);
        EXPORT_FUNC_WRITE(addr_PrepareTitle_hook, (int32_t (*)(int32_t, int32_t *))0xFFF15974);

        EXPORT_FUNC_WRITE(addr_sgIsLoadingBuffer, (int32_t (*)(int32_t, int32_t *))0xEFE19340);
        EXPORT_FUNC_WRITE(addr_gDynloadInitialized, (int32_t (*)(int32_t, int32_t *))0xEFE1329C);
    } else {
        OSFatal("Missing all OS specific addresses.");
    }
}
