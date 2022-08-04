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
#ifndef __OS_FUNCTIONS_H_
#define __OS_FUNCTIONS_H_

#include "common/types.h"
#include "common/common.h"

#ifdef __cplusplus
extern "C" {
#endif


/* Disassembler */
typedef void (*DisasmReport)(char *outputBuffer, ...);

typedef void *(*DisasmGetSym)(u32 addr, u8 *symbolName, u32 nameBufSize);

#define PPC_DISASM_MAX_BUFFER 64

#define PPC_DISASM_DEFAULT     0x00000000  // use defaults
#define PPC_DISASM_SIMPLIFY    0x00000001  // use simplified mnemonics
#define PPC_DISASM_REG_SPACES  0x00000020  // emit spaces between registers
#define PPC_DISASM_EMIT_DISASM 0x00000040  // emit only disassembly
#define PPC_DISASM_EMIT_ADDR   0x00000080  // emit only addresses + disassembly
#define PPC_DISASM_EMIT_FUNCS  0x00000100  // emit function names before and during disassembly

/* zlib */

/*#define Z_NO_COMPRESSION         0
#define Z_BEST_SPEED             1
#define Z_BEST_COMPRESSION       9
#define Z_DEFAULT_COMPRESSION  (-1)
#define Z_OK            0
#define Z_STREAM_END    1
#define Z_NEED_DICT     2
#define Z_ERRNO        (-1)
#define Z_STREAM_ERROR (-2)
#define Z_DATA_ERROR   (-3)
#define Z_MEM_ERROR    (-4)
#define Z_BUF_ERROR    (-5)
#define Z_VERSION_ERROR (-6)*/

#define BUS_SPEED                       248625000
#define SECS_TO_TICKS(sec)              (((unsigned long long)(sec)) * (BUS_SPEED/4))
#define MILLISECS_TO_TICKS(msec)        (SECS_TO_TICKS(msec) / 1000)
#define MICROSECS_TO_TICKS(usec)        (SECS_TO_TICKS(usec) / 1000000)

//To avoid conflicts with the unistd.h
#define usleep(usecs)                OSSleepTicks(MICROSECS_TO_TICKS(usecs))
#define sleep(secs)                  OSSleepTicks(SECS_TO_TICKS(secs))

#define FLUSH_DATA_BLOCK(addr)          asm volatile("dcbf 0, %0; sync" : : "r"(((addr) & ~31)))
#define INVAL_DATA_BLOCK(addr)          asm volatile("dcbi 0, %0; sync" : : "r"(((addr) & ~31)))

#define EXPORT_DECL(res, func, ...)     res (* func)(__VA_ARGS__) __attribute__((section(".data"))) = 0;
#define EXPORT_VAR(type, var)           type var __attribute__((section(".data")));


#define EXPORT_FUNC_WRITE(func, val)    *(u32*)(((u32)&func) + 0) = (u32)val

#define OS_FIND_EXPORT(handle, func)    _os_find_export(handle, # func, &funcPointer);                                  \
                                        EXPORT_FUNC_WRITE(func, funcPointer);

#define OS_FIND_EXPORT_EX(handle, func, func_p)                                                                         \
                                        _os_find_export(handle, # func, &funcPointer);                                  \
                                        EXPORT_FUNC_WRITE(func_p, funcPointer);

#define OS_MUTEX_SIZE                   44

/* Handle for coreinit */
extern u32 coreinit_handle;
extern void _os_find_export(u32 handle, const char *funcName, void *funcPointer);
extern void InitAcquireOS(void);
extern void InitOSFunctionPointers(void);

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! Lib handle functions
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
extern s32 (* OSDynLoad_Acquire)(const char* rpl, u32 *handle);
extern s32 (* OSDynLoad_FindExport)(u32 handle, s32 isdata, const char *symbol, void *address);
extern void (* OSDynLoad_Release)(u32 handle);

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! Security functions
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
extern s32 (* OSGetSecurityLevel)(void);
extern s32 (* OSForceFullRelaunch)(void);

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! Thread functions
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
extern s32 (* OSCreateThread)(OSThread *thread, s32 (*callback)(s32, void*), s32 argc, void *args, u32 stack, u32 stack_size, s32 priority, u32 attr);

extern void (*OSEnableInterrupts)(void);
extern void (*__OSClearAndEnableInterrupt)(void);
extern s32 (*OSIsInterruptEnabled)(void);
extern s32 (*OSIsDebuggerPresent)(void);

extern void (*OSRestoreInterrupts)(void);
extern void (*OSSetDABR)(s32, s32, s32, s32);
extern void (*OSSetIABR)(s32, s32);

extern s32 (* OSResumeThread)(OSThread *thread);
extern s32 (* OSSuspendThread)(OSThread *thread);
extern s32 (* OSIsThreadTerminated)(OSThread *thread);
extern s32 (* OSIsThreadSuspended)(OSThread *thread);
extern s32 (* OSJoinThread)(OSThread * thread, s32 * ret_val);
extern s32 (* OSSetThreadPriority)(OSThread * thread, s32 priority);
extern void (* OSDetachThread)(OSThread * thread);
extern OSThread * (* OSGetCurrentThread)(void);
extern const char * (* OSGetThreadName)(OSThread * thread);
extern void (* OSYieldThread)(void);

extern void (* OSGetActiveThreadLink)(OSThread * thread, void* link);
extern u32 (* OSGetThreadAffinity)(OSThread * thread);
extern s32 (* OSGetThreadPriority)(OSThread * thread);
extern void (* OSSetThreadName)(OSThread * thread, const char *name);
extern s32 (* OSGetCoreId)(void);

extern void (* OSSleepTicks)(u64 ticks);
extern u64 (* OSGetTick)(void);
extern u64 (* OSGetTime)(void);
extern u64 (* OSGetSystemTime)(void);
extern void (*OSTicksToCalendarTime)(u64 time, OSCalendarTime *calendarTime);

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! Message functions
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
extern void(* OSInitMessageQueue)(OSMessageQueue *queue, OSMessage *messages, s32 size);
extern u32(* OSSendMessage)(OSMessageQueue *queue, OSMessage *message, s32 flags);
extern u32(* OSReceiveMessage)(OSMessageQueue *queue, OSMessage *message, s32 flags);

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! Mutex functions
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
extern void (* OSInitMutex)(void* mutex);
extern void (* OSLockMutex)(void* mutex);
extern void (* OSUnlockMutex)(void* mutex);
extern s32 (* OSTryLockMutex)(void* mutex);

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! System functions
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
extern u64 (* OSGetTitleID)(void);
extern void (* OSGetArgcArgv)(s32* argc, char*** argv);
extern void (* __Exit)(void);
extern void (* OSFatal)(const char* msg);
extern void (* DCFlushRange)(const void *addr, u32 length);
extern void (* DCStoreRange)(const void *addr, u32 length);
extern void (* ICInvalidateRange)(const void *addr, u32 length);
extern void* (* OSEffectiveToPhysical)(const void*);
extern void* (* __OSPhysicalToEffectiveUncached)(const void*);
extern s32 (* __OSValidateAddressSpaceRange)(s32, void*, s32);
extern s32 (* __os_snprintf)(char* s, s32 n, const char * format, ...);
extern s32 * (* __gh_errno_ptr)(void);

extern void (*OSScreenInit)(void);
extern void (*OSScreenShutdown)(void);
extern u32 (*OSScreenGetBufferSizeEx)(u32 bufferNum);
extern s32 (*OSScreenSetBufferEx)(u32 bufferNum, void * addr);
extern s32 (*OSScreenClearBufferEx)(u32 bufferNum, u32 temp);
extern s32 (*OSScreenFlipBuffersEx)(u32 bufferNum);
extern s32 (*OSScreenPutFontEx)(u32 bufferNum, u32 posX, u32 posY, const char * buffer);
extern s32 (*OSScreenEnableEx)(u32 bufferNum, s32 enable);
extern u32 (*OSScreenPutPixelEx)(u32 bufferNum, u32 posX, u32 posY, u32 color);

typedef unsigned char (*exception_callback)(OSContext * interruptedContext);
extern void * (* OSSetExceptionCallback)(u8 exceptionType, exception_callback newCallback);
extern void * (* OSSetExceptionCallbackEx)(s32 unknwn,u8 exceptionType, exception_callback newCallback);
extern void (* OSLoadContext)(OSContext * context);

extern void (*DisassemblePPCRange)(void *rangeStart, void *rangeEnd, DisasmReport disasmReport, DisasmGetSym disasmGetSym, u32 disasmOptions);
extern bool (*DisassemblePPCOpcode)(u32 *opcode, char *outputBuffer, u32 bufferSize, DisasmGetSym disasmGetSym, u32 disasmOptions);
extern void *(*OSGetSymbolName)(u32 addr, u8 *symbolName, u32 nameBufSize);
extern void *(*OSGetSymbolNameEx)(u32 addr, u8 *symbolName, u32 nameBufSize);
extern int (*OSIsDebuggerInitialized)(void);

extern bool (*OSGetSharedData)(u32 type, u32 unk_r4, u8 *addr, u32 *size);
extern int (*OSShutdown)(int status);

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! Memory functions
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
extern u32 *pMEMAllocFromDefaultHeapEx;
extern u32 *pMEMAllocFromDefaultHeap;
extern u32 *pMEMFreeToDefaultHeap;

extern void* (* MEMAllocFromAllocator) (void * allocator, u32 size);
extern void (* MEMFreeToAllocator) (void * allocator, void* address);
extern s32 (* MEMGetBaseHeapHandle)(s32 mem_arena);
extern u32 (* MEMGetTotalFreeSizeForExpHeap)(s32 heap);
extern u32 (* MEMGetAllocatableSizeForExpHeapEx)(s32 heap, s32 align);
extern u32 (* MEMGetAllocatableSizeForFrmHeapEx)(s32 heap, s32 align);
extern void* (* MEMAllocFromFrmHeapEx)(s32 heap, u32 size, s32 align);
extern void (* MEMFreeToFrmHeap)(s32 heap, s32 mode);
extern void *(* MEMAllocFromExpHeapEx)(s32 heap, u32 size, s32 align);
extern s32 (* MEMCreateExpHeapEx)(void* address, u32 size, unsigned short flags);
extern s32 (* MEMCreateFrmHeapEx)(void* address, u32 size, unsigned short flags);
extern void *(* MEMDestroyExpHeap)(s32 heap);
extern void (* MEMFreeToExpHeap)(s32 heap, void* ptr);
extern void* (* OSAllocFromSystem)(u32 size, s32 alignment);
extern void (* OSFreeToSystem)(void *addr);
extern s32 (* OSIsAddressValid)(const void *ptr);
extern s32 (* MEMFindParentHeap)(s32 heap);
extern s32 (* OSGetMemBound)(s32 type, u32 * startAddress, u32 * size);

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! MCP functions
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
extern s32 (* MCP_Open)(void);
extern s32 (* MCP_Close)(s32 handle);
extern s32 (* MCP_TitleCount)(s32 handle);
extern s32 (* MCP_TitleList)(s32 handle, s32 *res, void *data, s32 count);
extern s32 (* MCP_GetOwnTitleInfo)(s32 handle, void *data);
extern void* (* MCP_GetDeviceId)(s32 handle, u32 * id);

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! LOADER functions
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
extern s32 (* LiWaitIopComplete)(s32 unknown_syscall_arg_r3, s32 * remaining_bytes);
extern s32 (* LiWaitIopCompleteWithInterrupts)(s32 unknown_syscall_arg_r3, s32 * remaining_bytes);
extern void (* addr_LiWaitOneChunk)(void);
extern void (* addr_sgIsLoadingBuffer)(void);
extern void (* addr_gDynloadInitialized)(void);

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! Kernel function addresses
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
extern void (* addr_PrepareTitle_hook)(void);

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! Other function addresses
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
extern void (*DCInvalidateRange)(void *buffer, u32 length);
extern s32 (*OSDynLoad_GetModuleName)(s32 handle, char *name_buffer, s32 *name_buffer_size);
extern s32 (*OSIsHomeButtonMenuEnabled) (void);
extern void (*OSEnableHomeButtonMenu) (s32);
extern s32 (*OSSetScreenCapturePermissionEx) (s32 tvEnabled, s32 drcEnabled);

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! Energy Saver functions
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
////Burn-in Reduction
extern s32 (*IMEnableDim)(void);
extern s32 (*IMDisableDim)(void);
extern s32 (*IMIsDimEnabled)(s32 * result);
//Auto power down
extern s32 (*IMEnableAPD)(void);
extern s32 (*IMDisableAPD)(void);
extern s32 (*IMIsAPDEnabled)(s32 * result);
extern s32 (*IMIsAPDEnabledBySysSettings)(s32 * result);

extern s32 (*OSSendAppSwitchRequest)(s32 param,void* unknown1,void* unknown2);

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! IOS functions
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------

extern s32 (*IOS_Ioctl)(s32 fd, u32 request, void *input_buffer,u32 input_buffer_len, void *output_buffer, u32 output_buffer_len);
extern s32 (*IOS_IoctlAsync)(s32 fd, u32 request, void *input_buffer,u32 input_buffer_len, void *output_buffer, u32 output_buffer_len, void *cb, void *cbarg);
extern s32 (*IOS_Open)(char *path, u32 mode);
extern s32 (*IOS_Close)(s32 fd);

#ifdef __cplusplus
}
#endif

#endif // __OS_FUNCTIONS_H_
