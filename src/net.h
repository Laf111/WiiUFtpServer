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

#ifdef __cplusplus
extern "C"{
#endif

#define IOSUHAX_BUFFER_SIZE 				0x8020
#define IOSUHAX_BUFFER_SIZE_STEPS           0x20
#define IOSUHAX_BUS_SPEED                   OSGetSystemInfo()->busClockSpeed

// define here buffers size (same as IOSUHAX use)
#define SO_SNDBUF       0x8021      // send buffer size
#define SO_RCVBUF       0x8022      // receive buffer size

// max MTU size even on fatest networks ~9000
// #define SO_SNDBUF       0x2001      // send buffer size = 1024*8+1 = 8193
// #define SO_RCVBUF       0x2002      // receive buffer size = 1024*8+2 = 8194

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <nsysnet/socket.h>
// define : add #indef 
//define SO_SNDBUF       0x1001      // send buffer size
//define SO_RCVBUF       0x1002      // receive buffer size

#include "iosuhax.h"
#include "iosuhax_devoptab.h"
#include "iosuhax_disc_interface.h"


void initialise_network();
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

int32_t create_server(uint16_t port);

int32_t send_exact(int32_t s, char *buf, int32_t length);

int32_t send_from_file(int32_t s, FILE *f);

int32_t recv_to_file(int32_t s, FILE *f);

// access to iosuhax file descriptors
void setFSAFD(int fd);
int getFSAFD();
int FSAR(int result);

#ifdef __cplusplus
}
#endif

#endif /* _NET_H_ */
