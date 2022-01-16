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

#define TRANSFER_THREAD_PRIORITY 0

// user buffers (socket and file) -------------------------------------------------------------------

#define UNSCALED_BUFFER_SIZE (8*1024)

// setsockopt send buffer size (the value will be doubled by the system when setsockopt)
// 131072 bytes (128KB = 128 x 1024) = 16*(8*1024)
#define SOCKET_BUFFER_SIZE (16*UNSCALED_BUFFER_SIZE)

// size of the socket memory buffer size (= max, 3*1024*1024 bytes)
#define SOMEMOPT_BUFFER_SIZE (4*SOCKET_BUFFER_SIZE)

// preallocated transfer buffer size (allocated when openning connection and stored in transferBuffers[] until the end of the session)
// NOTE : that if the size of the file transfered is lower than that, his size will be used as buffer size (internal file's buffer
//  size, plus for download SNDBUF's one if the size is lower than 2*SOCKET_BUFFER_SIZE)
// 50MB
#define TRANSFER_BUFFER_SIZE (50*SOCKET_BUFFER_SIZE)

// --------------------------------------------------------------------------------------------------

void initialize_network();

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

#ifdef __cplusplus
}
#endif

#endif /* _NET_H_ */
