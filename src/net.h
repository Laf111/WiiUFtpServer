/*

Copyright (C) 2008 Joseph Jordan <joe.ftpii@psychlaw.com.au>

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from
the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1.The origin of this software must not be misrepresented; you must not
claim that you wrote the original software. If you use this software in a
product, an acknowledgment in the product documentation would be
appreciated but is not required.

2.Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original software.

3.This notice may not be removed or altered from any source distribution.

*/
#ifndef _NET_H_
#define _NET_H_

#include <sys/param.h>
#include "ftp.h"
#include "dynamic_libs/socket_functions.h"

#ifdef __cplusplus
extern "C"{
#endif

#include <stdio.h>

#define UNUSED    __attribute__((unused))

#ifndef MIN
    #define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif
#ifndef MAX
    #define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

#define NET_RETRY_TIME_STEP_MILLISECS 3900
#define SO_RUSRBUF      0x10000     // enable userspace socket buffer
#define SO_NOSLOWSTART  0x4000      // suppress slowstart
#define TCP_NOACKDELAY  0x2002      // suppress delayed ACKs
#define TCP_MAXSEG      0x2003      // set maximum segment size
#define TCP_NODELAY     0x2004      // suppress TCP delay
#define UNSCALED_BUFFER_SIZE (8*1024) 

#define SOCKET_BUFFER_SIZE (16*UNSCALED_BUFFER_SIZE) 

// socket memory buffer size = (2*sndBuffSize+2*rcvBuffSize)
#define SOMEMOPT_BUFFER_SIZE (4*SOCKET_BUFFER_SIZE)

// perallocated transfer buffer per connections (FTP_NB_SIMULTANEOUS_TRANSFERS
#define TRANSFER_BUFFER_SIZE (4*SOCKET_BUFFER_SIZE*16)

typedef int32_t (*data_connection_callback)(int32_t data_socket, void *arg);
struct connection_struct {
    int32_t socket;
    char representation_type;
    int32_t passive_socket;
    int32_t data_socket;
    char cwd[MAXPATHLEN];
    char pending_rename[MAXPATHLEN];
    off_t restart_marker;
    struct sockaddr_in address;
    bool authenticated;
    char buf[FTP_MSG_BUFFER_SIZE];
    int32_t offset;
    bool data_connection_connected;
    data_connection_callback data_callback;
    void *data_connection_callback_arg;
    void (*data_connection_cleanup)(void *arg);
    uint32_t index;
    // file to transfer
    FILE *f;    
    // for file transferring
    char fileName[MAXPATHLEN];
    // volume path to the file
    char *volPath;
    // thread for transfering
    OSThread *transferThread;
    // preallocated transfer thread stack
    u32 transferThreadStack[FTP_TRANSFER_STACK_SIZE];
	// buffer for transferring files
    char *transferBuffer;
    // for data transfer tracking
    int32_t dataTransferOffset;
    // last speed computed in MB/s
    float speed;	
    // return code of send/recv functions
    int32_t bytesTransferred;
    uint64_t data_connection_timer;
};


typedef struct connection_struct connection_t;

void initialise_network();
void finalize_network();
s32 network_socket(u32 domain,u32 type,u32 protocol);

s32 network_bind(s32 s,struct sockaddr *name,s32 namelen);

s32 network_listen(s32 s,u32 backlog);

s32 network_accept(s32 s,struct sockaddr *addr,s32 *addrlen);

s32 network_connect(s32 s,struct sockaddr *,s32);

s32 network_read(s32 s,char *mem,s32 len);

s32 network_close(s32 s);

u32 network_gethostip();

s32 set_blocking(s32 s, bool blocking);

s32 network_close_blocking(s32 s);

s32 send_exact(s32 s, char *buf, s32 length);

s32 send_from_file(s32 data_socket, connection_t* connection);

s32 recv_to_file(s32 data_socket, connection_t* connection);


#ifdef __cplusplus
}
#endif

#endif /* _NET_H_ */
