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

#include <stdio.h>

#include <sys/param.h>

#include "dynamic_libs/socket_functions.h"
#include "ftp.h"

#ifdef __cplusplus
extern "C"{
#endif

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

// preallocated transfer buffer per connections
// (using a small transfer buffer for UL reduce the number of simultaneuous transfers)
// (using a large buffer for DL has the same effect)
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
    uint32_t transferThreadStack[FTP_TRANSFER_STACK_SIZE];
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
int32_t network_socket(uint32_t domain,uint32_t type,uint32_t protocol);

int32_t network_bind(int32_t s,struct sockaddr *name,int32_t namelen);

int32_t network_listen(int32_t s,uint32_t backlog);

int32_t network_accept(int32_t s,struct sockaddr *addr,int32_t *addrlen);

int32_t network_connect(int32_t s,struct sockaddr *,int32_t);

int32_t network_read(int32_t s,char *mem,int32_t len);

int32_t network_close(int32_t s);

uint32_t network_gethostip();

int32_t set_blocking(int32_t s, bool blocking);

int32_t network_close_blocking(int32_t s);

int32_t send_exact(int32_t s, char *buf, int32_t length);

int32_t send_from_file(int32_t data_socket, connection_t* connection);

int32_t recv_to_file(int32_t data_socket, connection_t* connection);


#ifdef __cplusplus
}
#endif

#endif /* _NET_H_ */
