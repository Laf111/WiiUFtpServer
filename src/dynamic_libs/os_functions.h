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

typedef void *(*DisasmGetSym)(uint32_t addr, uint8_t *symbolName, uint32_t nameBufSize);

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


#define EXPORT_FUNC_WRITE(func, val)    *(uint32_t*)(((uint32_t)&func) + 0) = (uint32_t)val

#define OS_FIND_EXPORT(handle, func)    _os_find_export(handle, # func, &funcPointer);                                  \
                                        EXPORT_FUNC_WRITE(func, funcPointer);

#define OS_FIND_EXPORT_EX(handle, func, func_p)                                                                         \
                                        _os_find_export(handle, # func, &funcPointer);                                  \
                                        EXPORT_FUNC_WRITE(func_p, funcPointer);

#define OS_MUTEX_SIZE                   44

/* Handle for coreinit */
extern uint32_t coreinit_handle;
extern void _os_find_export(uint32_t handle, const char *funcName, void *funcPointer);
extern void InitAcquireOS(void);
extern void InitOSFunctionPointers(void);

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! Lib handle functions
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
extern int32_t (* OSDynLoad_Acquire)(const char* rpl, uint32_t *handle);
extern int32_t (* OSDynLoad_FindExport)(uint32_t handle, int32_t isdata, const char *symbol, void *address);
extern void (* OSDynLoad_Release)(uint32_t handle);

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! Security functions
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
extern int32_t (* OSGetSecurityLevel)(void);
extern int32_t (* OSForceFullRelaunch)(void);

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! Thread functions
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
extern int32_t (* OSCreateThread)(OSThread *thread, int32_t (*callback)(int32_t, void*), int32_t argc, void *args, uint32_t stack, uint32_t stack_size, int32_t priority, uint32_t attr);

extern void (*OSEnableInterrupts)(void);
extern void (*__OSClearAndEnableInterrupt)(void);
extern int32_t (*OSIsInterruptEnabled)(void);
extern int32_t (*OSIsDebuggerPresent)(void);

extern void (*OSRestoreInterrupts)(void);
extern void (*OSSetDABR)(int32_t, int32_t, int32_t, int32_t);
extern void (*OSSetIABR)(int32_t, int32_t);

extern int32_t (* OSResumeThread)(OSThread *thread);
extern int32_t (* OSSuspendThread)(OSThread *thread);
extern int32_t (* OSIsThreadTerminated)(OSThread *thread);
extern int32_t (* OSIsThreadSuspended)(OSThread *thread);
extern int32_t (* OSJoinThread)(OSThread * thread, int32_t * ret_val);
extern int32_t (* OSSetThreadPriority)(OSThread * thread, int32_t priority);
extern void (* OSDetachThread)(OSThread * thread);
extern OSThread * (* OSGetCurrentThread)(void);
extern const char * (* OSGetThreadName)(OSThread * thread);
extern void (* OSYieldThread)(void);

extern void (* OSGetActiveThreadLink)(OSThread * thread, void* link);
extern uint32_t (* OSGetThreadAffinity)(OSThread * thread);
extern int32_t (* OSGetThreadPriority)(OSThread * thread);
extern void (* OSSetThreadName)(OSThread * thread, const char *name);
extern int32_t (* OSGetCoreId)(void);

extern void (* OSSleepTicks)(uint64_t ticks);
extern uint64_t (* OSGetTick)(void);
extern uint64_t (* OSGetTime)(void);
extern uint64_t (* OSGetSystemTime)(void);
extern void (*OSTicksToCalendarTime)(uint64_t time, OSCalendarTime *calendarTime);

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! Message functions
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
extern void(* OSInitMessageQueue)(OSMessageQueue *queue, OSMessage *messages, int32_t size);
extern uint32_t(* OSSendMessage)(OSMessageQueue *queue, OSMessage *message, int32_t flags);
extern uint32_t(* OSReceiveMessage)(OSMessageQueue *queue, OSMessage *message, int32_t flags);

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! Mutex functions
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
extern void (* OSInitMutex)(void* mutex);
extern void (* OSLockMutex)(void* mutex);
extern void (* OSUnlockMutex)(void* mutex);
extern int32_t (* OSTryLockMutex)(void* mutex);

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! System functions
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
extern uint64_t (* OSGetTitleID)(void);
extern void (* OSGetArgcArgv)(int32_t* argc, char*** argv);
extern void (* __Exit)(void);
extern void (* OSFatal)(const char* msg);
extern void (* DCFlushRange)(const void *addr, uint32_t length);
extern void (* DCStoreRange)(const void *addr, uint32_t length);
extern void (* ICInvalidateRange)(const void *addr, uint32_t length);
extern void* (* OSEffectiveToPhysical)(const void*);
extern void* (* __OSPhysicalToEffectiveUncached)(const void*);
extern int32_t (* __OSValidateAddressSpaceRange)(int32_t, void*, int32_t);
extern int32_t (* __os_snprintf)(char* s, int32_t n, const char * format, ...);
extern int32_t * (* __gh_errno_ptr)(void);

extern void (*OSScreenInit)(void);
extern void (*OSScreenShutdown)(void);
extern uint32_t (*OSScreenGetBufferSizeEx)(uint32_t bufferNum);
extern int32_t (*OSScreenSetBufferEx)(uint32_t bufferNum, void * addr);
extern int32_t (*OSScreenClearBufferEx)(uint32_t bufferNum, uint32_t temp);
extern int32_t (*OSScreenFlipBuffersEx)(uint32_t bufferNum);
extern int32_t (*OSScreenPutFontEx)(uint32_t bufferNum, uint32_t posX, uint32_t posY, const char * buffer);
extern int32_t (*OSScreenEnableEx)(uint32_t bufferNum, int32_t enable);
extern uint32_t (*OSScreenPutPixelEx)(uint32_t bufferNum, uint32_t posX, uint32_t posY, uint32_t color);

typedef unsigned char (*exception_callback)(OSContext * interruptedContext);
extern void * (* OSSetExceptionCallback)(uint8_t exceptionType, exception_callback newCallback);
extern void * (* OSSetExceptionCallbackEx)(int32_t unknwn,uint8_t exceptionType, exception_callback newCallback);
extern void (* OSLoadContext)(OSContext * context);

extern void (*DisassemblePPCRange)(void *rangeStart, void *rangeEnd, DisasmReport disasmReport, DisasmGetSym disasmGetSym, uint32_t disasmOptions);
extern bool (*DisassemblePPCOpcode)(uint32_t *opcode, char *outputBuffer, uint32_t bufferSize, DisasmGetSym disasmGetSym, uint32_t disasmOptions);
extern void *(*OSGetSymbolName)(uint32_t addr, uint8_t *symbolName, uint32_t nameBufSize);
extern void *(*OSGetSymbolNameEx)(uint32_t addr, uint8_t *symbolName, uint32_t nameBufSize);
extern int (*OSIsDebuggerInitialized)(void);

extern bool (*OSGetSharedData)(uint32_t type, uint32_t unk_r4, uint8_t *addr, uint32_t *size);
extern int (*OSShutdown)(int status);

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! Memory functions
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
extern uint32_t *pMEMAllocFromDefaultHeapEx;
extern uint32_t *pMEMAllocFromDefaultHeap;
extern uint32_t *pMEMFreeToDefaultHeap;

extern void* (* MEMAllocFromAllocator) (void * allocator, uint32_t size);
extern void (* MEMFreeToAllocator) (void * allocator, void* address);
extern int32_t (* MEMGetBaseHeapHandle)(int32_t mem_arena);
extern uint32_t (* MEMGetTotalFreeSizeForExpHeap)(int32_t heap);
extern uint32_t (* MEMGetAllocatableSizeForExpHeapEx)(int32_t heap, int32_t align);
extern uint32_t (* MEMGetAllocatableSizeForFrmHeapEx)(int32_t heap, int32_t align);
extern void* (* MEMAllocFromFrmHeapEx)(int32_t heap, uint32_t size, int32_t align);
extern void (* MEMFreeToFrmHeap)(int32_t heap, int32_t mode);
extern void *(* MEMAllocFromExpHeapEx)(int32_t heap, uint32_t size, int32_t align);
extern int32_t (* MEMCreateExpHeapEx)(void* address, uint32_t size, unsigned short flags);
extern int32_t (* MEMCreateFrmHeapEx)(void* address, uint32_t size, unsigned short flags);
extern void *(* MEMDestroyExpHeap)(int32_t heap);
extern void (* MEMFreeToExpHeap)(int32_t heap, void* ptr);
extern void* (* OSAllocFromSystem)(uint32_t size, int32_t alignment);
extern void (* OSFreeToSystem)(void *addr);
extern int32_t (* OSIsAddressValid)(const void *ptr);
extern int32_t (* MEMFindParentHeap)(int32_t heap);
extern int32_t (* OSGetMemBound)(int32_t type, uint32_t * startAddress, uint32_t * size);

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! MCP functions
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
extern int32_t (* MCP_Open)(void);
extern int32_t (* MCP_Close)(int32_t handle);
extern int32_t (* MCP_TitleCount)(int32_t handle);
extern int32_t (* MCP_TitleList)(int32_t handle, int32_t *res, void *data, int32_t count);
extern int32_t (* MCP_GetOwnTitleInfo)(int32_t handle, void *data);
extern void* (* MCP_GetDeviceId)(int32_t handle, uint32_t * id);

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! LOADER functions
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
extern int32_t (* LiWaitIopComplete)(int32_t unknown_syscall_arg_r3, int32_t * remaining_bytes);
extern int32_t (* LiWaitIopCompleteWithInterrupts)(int32_t unknown_syscall_arg_r3, int32_t * remaining_bytes);
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
extern void (*DCInvalidateRange)(void *buffer, uint32_t length);
extern int32_t (*OSDynLoad_GetModuleName)(int32_t handle, char *name_buffer, int32_t *name_buffer_size);
extern int32_t (*OSIsHomeButtonMenuEnabled) (void);
extern void (*OSEnableHomeButtonMenu) (int32_t);
extern int32_t (*OSSetScreenCapturePermissionEx) (int32_t tvEnabled, int32_t drcEnabled);

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! Energy Saver functions
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
////Burn-in Reduction
extern int32_t (*IMEnableDim)(void);
extern int32_t (*IMDisableDim)(void);
extern int32_t (*IMIsDimEnabled)(int32_t * result);
//Auto power down
extern int32_t (*IMEnableAPD)(void);
extern int32_t (*IMDisableAPD)(void);
extern int32_t (*IMIsAPDEnabled)(int32_t * result);
extern int32_t (*IMIsAPDEnabledBySysSettings)(int32_t * result);

extern int32_t (*OSSendAppSwitchRequest)(int32_t param,void* unknown1,void* unknown2);

//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
//! IOS functions
//!----------------------------------------------------------------------------------------------------------------------------------------------------------------------------

extern int32_t (*IOS_Ioctl)(int32_t fd, uint32_t request, void *input_buffer,uint32_t input_buffer_len, void *output_buffer, uint32_t output_buffer_len);
extern int32_t (*IOS_IoctlAsync)(int32_t fd, uint32_t request, void *input_buffer,uint32_t input_buffer_len, void *output_buffer, uint32_t output_buffer_len, void *cb, void *cbarg);
extern int32_t (*IOS_Open)(char *path, uint32_t mode);
extern int32_t (*IOS_Close)(int32_t fd);

#ifdef __cplusplus
}
#endif

#endif // __OS_FUNCTIONS_H_
