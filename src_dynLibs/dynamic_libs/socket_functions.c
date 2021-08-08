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
#include <string.h>
#include <malloc.h>

#include "os_functions.h"
#include "socket_functions.h"

#define SOCKLIB_BUFSIZE (MAX_NET_BUFFER_SIZE * 4) // For send & receive + double buffering

extern void display(const char *format, ...);

u32 hostIpAddress = 0;

u32 nsysnet_handle __attribute__((section(".data"))) = 0;

EXPORT_DECL(s32, socket_lib_init, void);
EXPORT_DECL(s32, socket_lib_finish, void);
EXPORT_DECL(s32, socket, s32 domain, s32 type, s32 protocol);
EXPORT_DECL(s32, socketclose, s32 s);
EXPORT_DECL(s32, shutdown, s32 s, s32 how);
EXPORT_DECL(s32, connect, s32 s, void *addr, s32 addrlen);
EXPORT_DECL(s32, bind, s32 s,struct sockaddr *name,s32 namelen);
EXPORT_DECL(s32, listen, s32 s,u32 backlog);
EXPORT_DECL(s32, accept, s32 s,struct sockaddr *addr,s32 *addrlen);
EXPORT_DECL(s32, send, s32 s, const void *buffer, s32 size, s32 flags);
EXPORT_DECL(s32, recv, s32 s, void *buffer, s32 size, s32 flags);
EXPORT_DECL(s32, recvfrom,s32 sockfd, void *buf, s32 len, s32 flags,struct sockaddr *src_addr, s32 *addrlen);
EXPORT_DECL(s32, sendto, s32 s, const void *buffer, s32 size, s32 flags, const struct sockaddr *dest, s32 dest_len);
EXPORT_DECL(s32, setsockopt, s32 s, s32 level, s32 optname, void *optval, s32 optlen);
EXPORT_DECL(s32, getsockopt, s32 s, s32 level, s32 optname, void *optval, s32 optlen);
EXPORT_DECL(s32, somemopt, int type, void *buf, size_t bufsize, int unk);

EXPORT_DECL(char *, inet_ntoa, struct in_addr in);
EXPORT_DECL(s32, inet_aton, const char *cp, struct in_addr *inp);
EXPORT_DECL(const char *, inet_ntop, s32 af, const void *src, char *dst, s32 size);
EXPORT_DECL(s32, inet_pton, s32 af, const char *src, void *dst);
EXPORT_DECL(s32, socketlasterr, void);
        
static OSThread *socketThread=NULL;
static u32 *socketThreadStack=NULL;

/*
static OSThread *socketOptThread=NULL;
static u32 *socketOptThreadStack=NULL;

// Socket memory optimization

    Don't forget to enable it when creating a socket
    
    // Activate userspace buffer
    int enable = 1;
    setsockopt(sock, SOL_SOCKET, SO_USERBUF, &enable, sizeof(enable));


void socketOptThreadMain(s32 argc, void *argv)
{

    int buf_size = SOCKLIB_BUFSIZE;
    char * buf=NULL;

    // align memory (64bytes = 0x40) when alocating the buffer
    do{
        buf_size -= 32;

        buf = (char *)memalign(0x40, buf_size);
        if (buf) memset(buf, 0x00, buf_size);
    }while(!buf);
    
    // always fail....    
    if (somemopt(0x01, buf, SOCKLIB_BUFSIZE, 0) == -1)
    {
        display("! ERROR : somemopt failed!");
    }
   
    display("DEBUG : somemopt ends now");

    free(buf);
    
    ((void (*)())0x01041D6C)(); // OSExitThread()
}
*/
void socketThreadMain(s32 argc, void *argv)
{
    u32 *funcPointer = 0;

    InitAcquireSocket();

    u32 nn_ac_handle;
    s32(*ACInitialize)();
    s32(*ACGetStartupId) (u32 *id);
    s32(*ACConnectWithConfigId) (u32 id);
    s32(*ACGetAssignedAddress) (u32 * ip);
    OSDynLoad_Acquire("nn_ac.rpl", &nn_ac_handle);
    OSDynLoad_FindExport(nn_ac_handle, 0, "ACInitialize", &ACInitialize);
    OSDynLoad_FindExport(nn_ac_handle, 0, "ACGetStartupId", &ACGetStartupId);
    OSDynLoad_FindExport(nn_ac_handle, 0, "ACConnectWithConfigId",&ACConnectWithConfigId);
    OSDynLoad_FindExport(nn_ac_handle, 0, "ACGetAssignedAddress",&ACGetAssignedAddress);

    OS_FIND_EXPORT(nsysnet_handle, socket_lib_init);
    OS_FIND_EXPORT(nsysnet_handle, socket_lib_finish);
    OS_FIND_EXPORT(nsysnet_handle, socketlasterr);
    OS_FIND_EXPORT(nsysnet_handle, socket);
    OS_FIND_EXPORT(nsysnet_handle, socketclose);
    OS_FIND_EXPORT(nsysnet_handle, shutdown);
    OS_FIND_EXPORT(nsysnet_handle, connect);
    OS_FIND_EXPORT(nsysnet_handle, bind);
    OS_FIND_EXPORT(nsysnet_handle, listen);
    OS_FIND_EXPORT(nsysnet_handle, accept);
    OS_FIND_EXPORT(nsysnet_handle, send);
    OS_FIND_EXPORT(nsysnet_handle, recv);
    OS_FIND_EXPORT(nsysnet_handle, recvfrom);
    OS_FIND_EXPORT(nsysnet_handle, sendto);
    OS_FIND_EXPORT(nsysnet_handle, setsockopt);
    OS_FIND_EXPORT(nsysnet_handle, getsockopt);
    OS_FIND_EXPORT(nsysnet_handle, somemopt);
    OS_FIND_EXPORT(nsysnet_handle, inet_ntoa);
    OS_FIND_EXPORT(nsysnet_handle, inet_aton);
    OS_FIND_EXPORT(nsysnet_handle, inet_ntop);
    OS_FIND_EXPORT(nsysnet_handle, inet_pton);
    
    u32 nn_startupid;
    ACInitialize();
    ACGetStartupId(&nn_startupid);
    ACConnectWithConfigId(nn_startupid);
    ACGetAssignedAddress(&hostIpAddress);
   
    socket_lib_init();
    /*
    // create a thread on CPU0
    socketOptThread = OSAllocFromSystem(sizeof(OSThread), 8); 
    if (socketOptThread != NULL) {
     
        socketOptThreadStack = OSAllocFromSystem(NET_STACK_SIZE, 8);
        if (socketOptThreadStack != NULL) {
    
            // on CPU0
            OSCreateThread(socketOptThread, (void*)socketOptThreadMain, 0, NULL, (u32)socketOptThreadStack+NET_STACK_SIZE, NET_STACK_SIZE, 0, OS_THREAD_ATTR_AFFINITY_CORE0);
            // set name    
            OSSetThreadName(socketOptThread, "Socket optimizer thread on CPU0");
            
            // launch the thread
            OSResumeThread(socketOptThread);
            sleep(3);
        }
    }    
    */
    ((void (*)())0x01041D6C)(); // OSExitThread()
}

void InitAcquireSocket(void) {
    if(coreinit_handle == 0) {
        InitAcquireOS();
    };
    OSDynLoad_Acquire("nsysnet.rpl", &nsysnet_handle);
}

void InitSocketFunctionPointers(void) {

    // create a thread on CPU0
    socketThread = OSAllocFromSystem(sizeof(OSThread), 8); 
    if (socketThread != NULL) {
     
        socketThreadStack = OSAllocFromSystem(NET_STACK_SIZE, 8);
        if (socketThreadStack != NULL) {
    
            // on CPU0, prio = 0
            OSCreateThread(socketThread, (void*)socketThreadMain, 0, NULL, (u32)socketThreadStack+NET_STACK_SIZE, NET_STACK_SIZE, 0, OS_THREAD_ATTR_AFFINITY_CORE0);
            // set name    
            OSSetThreadName(socketThread, "Socket lib thread on CPU0");
            
            // launch the thread
            OSResumeThread(socketThread);
        }
    }
}    

void FreeSocketFunctionPointers(void) {

    socket_lib_finish();

    s32 ret;
/*    
    OSJoinThread(socketOptThread, &ret);
    
    if (socketOptThreadStack != NULL) OSFreeToSystem(socketOptThreadStack);
    if (socketOptThread != NULL) OSFreeToSystem(socketOptThread);       
*/
    OSJoinThread(socketThread, &ret);
    
    if (socketThreadStack != NULL) OSFreeToSystem(socketThreadStack);
    if (socketThread != NULL) OSFreeToSystem(socketThread); 
    
}