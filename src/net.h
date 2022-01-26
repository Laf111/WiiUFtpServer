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
/****************************************************************************
  * WiiUFtpServer
  * 2021-12-05:Laf111:V7-0
 ***************************************************************************/

#ifndef _NET_H_
#define _NET_H_

#include <coreinit/thread.h>

#include "ftp.h"

#ifdef __cplusplus
extern "C"{
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <sys/socket.h>

#define UNUSED    __attribute__((unused))

#ifndef MIN
    #define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif
#ifndef MAX
    #define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

// number max of concurrents transfer slots 
#define NB_SIMULTANEOUS_TRANSFERS 8

// Timeouts for retrying the calls to the network API
#define NET_RETRY_TIME_STEP_MILLISECS 3900

// socket extra definitions -------------------------------------------------------------------------

#define SO_RUSRBUF      0x10000     // enable userspace socket buffer
#define SO_NOSLOWSTART  0x4000      // suppress slowstart
#define TCP_NOACKDELAY  0x2002      // suppress delayed ACKs
#define TCP_MAXSEG      0x2003      // set maximum segment size
#define TCP_NODELAY     0x2004      // suppress TCP delay

// --------------------------------------------------------------------------------------------------

// user buffers (socket and file) -------------------------------------------------------------------

#define UNSCALED_BUFFER_SIZE (8*1024) 

// setsockopt send buffer size (the value will be doubled by the system when setsockopt)
// 131072 bytes (128KB = 128 x 1024) = 16*(8*1024)
#define SOCKET_BUFFER_SIZE (16*UNSCALED_BUFFER_SIZE) 
#define SND_BUFFER_SIZE SOCKET_BUFFER_SIZE 


// SOCKET MEMORY_OPTIMIZATION : use the max amount allowed = 3MB
// MAX_SOMEMOPT_BUFFER_SIZE=3*1024*1024=2*[(2*SNDBUF)+(2*RCVBUF)] (double buffering)
// MAX_RCVBUF=(MAX_SOMEMOPT_BUFFER_SIZE/4)-SNDBUF=3*1024*256-128*1024=5*128*1024
#define RCV_BUFFER_SIZE 5*SOCKET_BUFFER_SIZE 

// socket memory buffer size = (SND+RCV) double buffered (x2) = MAX_SOMEMOPT_BUFFER_SIZE
#define SOMEMOPT_BUFFER_SIZE (2*(SND_BUFFER_SIZE + RCV_BUFFER_SIZE))

// recv can sent a max of 2*RCV_BUFFER_SIZE at one time
#define MIN_TRANSFER_CHUNK_SIZE (2*RCV_BUFFER_SIZE)

// number of chunks (blocs) to send/receive per network operations
// low values lower openning connection time because of setvbuf resizing and favorize the share of the network bandwith between transfer sockets
// too large may slow down openning connection time because of setvbuf resizing but will give better transfer rate on single file operation

#define NB_TRANSFER_CHUNKS 4

// need at least the double of MIN_TRANSFER_CHUNK_SIZE = 2.6MB
// 4*(2*(5*(128*1024))) : 5.2MB
// size of the pre-allocated transfer buffer size (5.2MB)
#define TRANSFER_BUFFER_SIZE (NB_TRANSFER_CHUNKS*MIN_TRANSFER_CHUNK_SIZE)

// --------------------------------------------------------------------------------------------------

int32_t initialize_network();

void finalize_network();

int32_t network_socket(uint32_t domain,uint32_t type,uint32_t protocol);

int32_t network_bind(int32_t s,struct sockaddr *name,int32_t namelen);

int32_t network_listen(int32_t s,uint32_t backlog);

int32_t network_accept(int32_t s,struct sockaddr *addr,int32_t *addrlen);

int32_t network_connect(int32_t s,struct sockaddr *,int32_t);

int32_t network_read(int32_t s,void *mem,int32_t len);

int32_t network_close(int32_t s);

uint32_t network_gethostip();

int32_t set_blocking(int32_t s, bool blocking);

int32_t network_close_blocking(int32_t s);

int32_t send_exact(int32_t s, char *buf, int32_t length);

int32_t send_from_file(int32_t data_socket, connection_t* connection);

int32_t recv_to_file(int32_t data_socket, connection_t* connection);

void    setFsaFdInNet(int hfd);

#ifdef __cplusplus
}
#endif

#endif /* _NET_H_ */
