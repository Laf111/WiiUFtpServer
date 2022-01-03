/*

ftpii -- an FTP server for the Wii

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
  * 2021-12-05:Laf111:V7-0: complete the some TODO left
 ***************************************************************************/

#ifndef _FTP_H_
#define _FTP_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/dir.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>
#include <time.h>

#include <coreinit/time.h>
#include <coreinit/thread.h>
#include <coreinit/memdefaultheap.h>
#include <nsysnet/_socket.h>

#include <whb/log.h>
#include <whb/log_console.h>

#define FTP_PORT            21

#define FTP_CONNECTION_TIMEOUT NET_TIMEOUT*NB_NET_TIME_OUT

#define FTP_MSG_BUFFER_SIZE 1024

// Number max of simultaneous connections from the client : 
// 1 for communication with the client + NB_SIMULTANEOUS_TRANSFERS
#define FTP_NB_SIMULTANEOUS_TRANSFERS 1+NB_SIMULTANEOUS_TRANSFERS

#define FTP_STACK_SIZE 32*1024


#ifdef __cplusplus
extern "C"{
#endif

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
    // index of the connection
    uint32_t index;
    // file if a file is transfered using this connection
    FILE *f;
    char fileName[MAXPATHLEN];
    // volume path to the file
    char *volPath;
    // user's buffer for file
    char *userBuffer;
    // attributes for data transfer tracking
    int32_t dataTransferOffset;
    // return code of send/recv functions
    int32_t bytesTransfered;
    OSTime data_connection_timer;
};

typedef struct connection_struct connection_t;

void    setVerboseMode(bool flag);
int32_t create_server(uint16_t port);
bool    process_ftp_events();
void    cleanup_ftp();

void    setOsTime(struct tm *tmTime);
void    setFsaFd(int hfd);
int     getFsaFd();

#ifdef __cplusplus
}
#endif

#endif /* _FTP_H_ */
