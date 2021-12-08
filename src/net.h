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

#ifdef __cplusplus
extern "C"{
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include <sys/socket.h>
#include "transferedFiles.h"

#define UNUSED    __attribute__((unused))

#ifndef MIN
    #define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif
#ifndef MAX
    #define MAX(x, y) ((x) > (y) ? (x) : (y))
#endif

// number max of concurrents transfer slots 
#define NB_SIMULTANEOUS_TRANSFERS 6

// Timeout for retrying the calls to the network API
#define NET_TIMEOUT 4
#define NB_NET_TIME_OUT 9
#define NET_RETRY_TIME_STEP_MILLISECS 2400

// socket extra definitions
#define SO_RUSRBUF      0x10000     // enable userspace socket buffer
#define SO_NOSLOWSTART  0x4000      // suppress slowstart
#define TCP_NOACKDELAY  0x2002      // suppress delayed ACKs
#define TCP_NODELAY     0x2004      // suppress TCP delay

// user buffers (file and IO) defs
#define UNSCALED_BUFFER_SIZE 8*1024
// 131072 bytes (128 x 1024) = 2*64*1024
#define SOMEMOPT_MIN_BUFFER_SIZE UNSCALED_BUFFER_SIZE*16

#define DL_USER_BUFFER_SIZE SOMEMOPT_MIN_BUFFER_SIZE*2
#define UL_USER_BUFFER_SIZE SOMEMOPT_MIN_BUFFER_SIZE*5

// close to the max (3MB) : 4*(5*128*1024) = 2 621 440 bytes
#define SOMEMOPT_BUFFER_SIZE 4*MAX(UL_USER_BUFFER_SIZE, DL_USER_BUFFER_SIZE)

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

int32_t send_from_file(int32_t s, FILE *f);

int32_t recv_to_file(int32_t s, FILE *f);

#ifdef __cplusplus
}
#endif

#endif /* _NET_H_ */
