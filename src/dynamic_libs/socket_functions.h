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
#ifndef __SOCKET_FUNCTIONS_H_
#define __SOCKET_FUNCTIONS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

#include "common/types.h"

// number max of concurrents transfer slots 
#define NB_SIMULTANEOUS_TRANSFERS 8


#define INADDR_ANY      0

#define AF_INET         2

#define IPPROTO_IP      0
#define IPPROTO_TCP     6
#define IPPROTO_UDP     17

#define TCP_NODELAY     0x2004

#define SOL_SOCKET      -1
#define SOCK_STREAM     1
#define SOCK_DGRAM      2

/*
 * Option flags per-socket.
 */
#define SO_DEBUG        0x0001      /* turn on debugging info recording */
#define SO_ACCEPTCONN   0x0002      /* socket has had listen() */
#define SO_REUSEADDR    0x0004
#define SO_KEEPALIVE    0x0008      /* keep connections alive */
#define SO_DONTROUTE    0x0010      /* just use interface addresses */
#define SO_BROADCAST    0x0020      // broadcast
#define SO_USELOOPBACK  0x0040      /* bypass hardware when possible */

#define SO_LINGER       0x0080      // linger (no effect?)
#define SO_OOBINLINE    0x0100      // out-of-band data inline (no effect?)
#define SO_TCPSACK      0x0200      // set tcp selective acknowledgment
#define SO_WINSCALE     0x0400      // set tcp window scaling

#define TCP_NOACKDELAY  0x2002    /* suppress delayed ACKs    */
#define TCP_MAXSEG      0x2003    /* set maximum segment size    */

#define SO_USERBUF      0x10000     // enable userspace buffer
/*
 * Additional options, not kept in so_options.
 */
#define SO_SNDBUF       0x1001      // send buffer size
#define SO_RCVBUF       0x1002      // receive buffer size
#define SO_SNDLOWAT     0x1003      // send low-water mark (no effect?)
#define SO_RCVLOWAT     0x1004      // receive low-water mark
#define SO_SNDTIMEO     0x1005      /* send timeout */
#define SO_RCVTIMEO     0x1006      /* receive timeout */
                        
#define SO_TYPE         0x1008      // get socket type
                        
#define SO_NBIO         0x1014      // set socket to NON-blocking mode
#define SO_BIO          0x1015      // set socket to blocking mode
#define SO_NONBLOCK     0x1016
#define SO_MYADDR       0x1013

// From https://wiiubrew.org/wiki/Nsysnet.rpl
// SO functions return codes
#define SO_SUCCESS      0           // The operation were successful
#define SO_ETIMEDOUT    2           // The connection timed out
#define SO_ECONNREFUSED 7           // The connection were refused
#define SO_ERROR        41          // The operation hit a generic error and were not successful
#define SO_ELIBNOTREADY 43          // The library is not ready, did you forget to initialize it? 

#define ENODATA         1
#define ENOENT          2
#define EISCONN         3
#define EWOULDBLOCK     6
#define EALREADY        10
#define EAGAIN          EWOULDBLOCK
#define EINVAL          11
#define ENOMEM          18
#define ENODEV          19
#define EINPROGRESS     22
#define	EBUSY 16		/* Device or resource busy */
#define ENOMEM          18
#define ENODEV          19
#define EINPROGRESS     22
#define	ENFILE 23	/* Too many open files in system */
#define	EMFILE 24	/* File descriptor value too large */
#define MSG_DONTWAIT    32
#define ETIME 62		/* Stream ioctl timeout */
#define ECONNREFUSED 111	/* Connection refused */
#define ECONNABORTED 113	/* Software caused connection abort */
#define ETIMEDOUT 116		/* Connection timed out */
#define EHOSTUNREACH 118	/* Host is unreachable */
#define MSG_DONTWAIT    32

#define htonl(x) x
#define htons(x) x
#define ntohl(x) x
#define ntohs(x) x

#define geterrno()  (socketlasterr())

struct in_addr {
    unsigned int s_addr;
};
struct sockaddr_in {
    short sin_family;
    unsigned short sin_port;
    struct in_addr sin_addr;
    char sin_zero[8];
};

struct sockaddr
{
   unsigned short sa_family;
   char sa_data[14];
};

void InitSocketFunctionPointers(void);
void FreeSocketFunctionPointers(void);
void InitAcquireSocket(void);

extern int32_t (*socket_lib_init)(void);
extern int32_t (*socket_lib_finish)(void);
extern int32_t (*socket)(int32_t domain, int32_t type, int32_t protocol);
extern int32_t (*socketclose)(int32_t s);
extern int32_t (*shutdown)(int32_t s, int32_t how);
extern int32_t (*connect)(int32_t s, void *addr, int32_t addrlen);
extern int32_t (*bind)(int32_t s,struct sockaddr *name,int32_t namelen);
extern int32_t (*listen)(int32_t s,uint32_t backlog);
extern int32_t (*accept)(int32_t s,struct sockaddr *addr,int32_t *addrlen);
extern int32_t (*send)(int32_t s, const void *buffer, int32_t size, int32_t flags);
extern int32_t (*recv)(int32_t s, void *buffer, int32_t size, int32_t flags);
extern int32_t (*recvfrom)(int32_t sockfd, void *buf, int32_t len, int32_t flags,struct sockaddr *src_addr, int32_t *addrlen);
extern int32_t (*socketlasterr)(void);

extern int32_t (*sendto)(int32_t s, const void *buffer, int32_t size, int32_t flags, const struct sockaddr *dest, int32_t dest_len);
extern int32_t (*setsockopt)(int32_t s, int32_t level, int32_t optname, void *optval, int32_t optlen);
extern int32_t (*getsockopt)(int32_t s, int32_t level, int32_t optname, void *optval, int32_t optlen);
extern int32_t (*somemopt) (int type, void *buf, size_t bufsize, int unk);

extern char * (*inet_ntoa)(struct in_addr in);
extern int32_t (*inet_aton)(const char *cp, struct in_addr *inp);
extern const char * (*inet_ntop)(int32_t af, const void *src, char *dst, int32_t size);
extern int32_t (*inet_pton)(int32_t af, const char *src, void *dst);

#ifdef __cplusplus
}
#endif

#endif // __SOCKET_FUNCTIONS_H_
