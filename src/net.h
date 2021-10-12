/****************************************************************************
  * WiiUFtpServer
  * 2021/04/05:V1.0.0:Laf111: import ftp-everywhere code
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

/****************************************************************************
  * WiiUFtpServer
 ***************************************************************************/
#include <nsysnet/socket.h>
#include "receivedFiles.h"

#ifndef MIN
    #define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

#define DEFAULT_NET_BUFFER_SIZE 64*1024

// max data connections number : listener + passive_socket + data_socket*2 (two ways)
#define NB_SIMULTANEOUS_CONNECTIONS 4

void initialize_network();
void finalize_network();

int32_t network_socket(uint32_t domain,uint32_t type,uint32_t protocol);
int32_t network_data_socket(uint32_t domain,uint32_t type,uint32_t protocol);
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
