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
  * 2021-10-20:Laf111:V6-3
 ***************************************************************************/
 
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>

#include <whb/log.h>
#include <whb/log_console.h>

#include <errno.h>
#include <sys/fcntl.h>
#include <coreinit/memdefaultheap.h>

#include <nn/ac.h>
#include "vrt.h"
#include "net.h"

#define SOCKLIB_BUFSIZE DEFAULT_NET_BUFFER_SIZE*12

#define SOCKET_MOPT_STACK_SIZE DEFAULT_NET_BUFFER_SIZE

extern int somemopt (int req_type, char* mem, unsigned int memlen, int flags);
extern void display(const char *fmt, ...);

static uint32_t hostIpAddress = 0;

static const int retriesNumber = 20;

static OSThread *socketThread=NULL;

static OSThread socketOptThread;
static uint8_t *socketOptThreadStack=NULL;

static bool initDone = false;

int32_t getsocketerrno()
{
    int res = socketlasterr();
    if (res == NSN_EAGAIN)
    {
        res = EAGAIN;
    }
    return res;
}

int socketOptThreadMain(int argc, const char **argv)
{
    void *buf = MEMAllocFromDefaultHeapEx(SOCKLIB_BUFSIZE, 64);
    if (buf == NULL)
    {
        display("Socket optimizer: OUT OF MEMORY!");
        return 1;
    }

    if (somemopt(0x01, buf, SOCKLIB_BUFSIZE, 0) == -1 && socketlasterr() != 50)
    {
        display("somemopt failed!");
        return 1;
    }

    MEMFreeToDefaultHeap(buf);

    return 0;
}

int socketThreadMain(int argc, const char **argv)
{
    socket_lib_init();

    socketOptThreadStack = MEMAllocFromDefaultHeapEx(SOCKET_MOPT_STACK_SIZE, 8);

    if (socketOptThreadStack == NULL || !OSCreateThread(&socketOptThread, socketOptThreadMain, 0, NULL, socketOptThreadStack + SOCKET_MOPT_STACK_SIZE, SOCKET_MOPT_STACK_SIZE, 0, OS_THREAD_ATTRIB_AFFINITY_CPU0))
        return 1;

    OSSetThreadName(&socketOptThread, "Socket optimizer socket");
    OSResumeThread(&socketOptThread);

    return 0;
}

void initialize_network()
{
    unsigned int nn_startupid;

    ACInitialize();
    ACGetStartupId(&nn_startupid);
    ACConnectWithConfigId(nn_startupid);
    ACGetAssignedAddress(&hostIpAddress);

    // use default thread on Core 0
    socketThread=OSGetDefaultThread(0);
    if (socketThread == NULL) {
        display("! ERROR : when getting socketThread!");
        return;
    }
    if (!OSSetThreadPriority(socketThread, 1))
        display("! WARNING: Error changing net thread priority!");

    OSSetThreadName(socketThread, "Network thread on CPU0");
    OSRunThread(socketThread, socketThreadMain, 0, NULL);

    OSResumeThread(socketThread);

    int ret;
    OSJoinThread(socketThread, &ret);

}

void finalize_network()
{
    if (socketOptThreadStack != NULL) socket_lib_finish();

    int ret;
    OSJoinThread(&socketOptThread, &ret);
    if (socketOptThreadStack != NULL) MEMFreeToDefaultHeap(socketOptThreadStack);

}

int32_t network_socket(uint32_t domain,uint32_t type,uint32_t protocol)
{
    int sock = socket(domain, type, protocol);
    if (sock < 0)
    {
        int err = -getsocketerrno();
        return (err < 0) ? err : sock;
    }

    // common optimizations for default socket creation
    if (type == SOCK_STREAM)
    {
        int enable = 1;
            
        // Reuse opened buffers (avoid TIME_WAIT)
        if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable))!=0) 
            {if (!initDone) display("! ERROR : SO_REUSEADDR activation failed !");}
        
        // Activate WinScale
        if (setsockopt(sock, SOL_SOCKET, SO_WINSCALE, &enable, sizeof(enable))!=0) 
            {if (!initDone) display("! ERROR : WinScale activation failed !");}

        // SO_NOSLOWSTART
        if (setsockopt(sock, SOL_SOCKET, SO_NOSLOWSTART, &enable, sizeof(enable))!=0) 
            {if (!initDone) display("! ERROR : Disable slow start feature failed !");}    
        
        // socket memory optimization
        int nbTries=0;
        try_again_someopt:
        if (setsockopt(sock, SOL_SOCKET, SO_RUSRBUF, &enable, sizeof(enable))!=0) 
        {
            OSSleepTicks(OSMillisecondsToTicks(200));
            nbTries++;
            if (nbTries <= retriesNumber) goto try_again_someopt;
            else if (!initDone) display("! ERROR : Socket memory optimization failed !");
        }
        
        if (!initDone) {
            initDone = true;
            display("Limited to 1 client and 1 slot for up/download !");
        }
        
    }

    return sock;
}

int32_t network_bind(int32_t s,struct sockaddr *name,int32_t namelen)
{
    int res = bind(s, name, namelen);
    if (res < 0)
    {
        int err = -getsocketerrno();
        return (err < 0) ? err : res;
    }
    return res;
}

int32_t network_listen(int32_t s,uint32_t backlog)
{
    int res = listen(s, backlog);
    if (res < 0)
    {
        int err = -getsocketerrno();
        return (err < 0) ? err : res;
    }
    return res;
}

int32_t network_accept(int32_t s,struct sockaddr *addr,int32_t *addrlen)
{
    socklen_t addrl=(socklen_t) *addrlen;
    int res = accept(s, addr, &addrl);
    if (res < 0)
    {
        int err = -getsocketerrno();
        return (err < 0) ? err : res;
    }
    return res;
}

int32_t network_connect(int32_t s,struct sockaddr *addr, int32_t addrlen)
{
    int res = connect(s, addr, addrlen);
    if (res < 0)
    {
        int err = -getsocketerrno();
        return (err < 0) ? err : res;
    }
    return res;
}

int32_t network_read(int32_t s,void *mem,int32_t len)
{
    int res = recv(s, mem, len, 0);
    if (res < 0)
    {
        int err = -getsocketerrno();
        return (err < 0) ? err : res;
    }
    return res;
}

uint32_t network_gethostip()
{
    return hostIpAddress;
}

int32_t network_write(int32_t s, const void *mem, int32_t len)
{
    int32_t transfered = 0;
    
    while (len)
    {
        int ret = send(s, mem, len, 0);
        if (ret < 0)
        {
            int err = -getsocketerrno();
            transfered = (err < 0) ? err : ret;
            break;
        }

        mem += ret;
        transfered += ret;
        len -= ret;
    }
    return transfered;
}

int32_t network_close(int32_t s)
{
    if (s < 0)
        return -1;

    return socketclose(s);
}

int32_t set_blocking(int32_t s, bool blocking) {
    int32_t block = !blocking;
    setsockopt(s, SOL_SOCKET, SO_NONBLOCK, &block, sizeof(block));
    return 0;
}

int32_t network_close_blocking(int32_t s) {
    set_blocking(s, true);
    return network_close(s);
}


int32_t send_exact(int32_t s, char *buf, int32_t length) {
    int buf_size = length;
    int32_t result = 0;
    int32_t remaining = length;
    int32_t bytes_transferred;
	set_blocking(s, true);
    while (remaining) {

        bytes_transferred = network_write(s, buf, MIN(remaining, (int) buf_size));
        
        if (bytes_transferred > 0) {
            remaining -= bytes_transferred;
            buf += bytes_transferred;

        } else if (bytes_transferred < 0) {

            if (bytes_transferred == -ENOMEM) {
                display("! ERROR : out of memory in send_exact");
            }
            // ERROR
            result = bytes_transferred;
            break;
        } else {
            // Should never happen
            result = -ENODATA;
			display("DEBUG : send_exact return ENODATA!");
            break;
        }
    }
	set_blocking(s, false);
    return result;
}

int32_t send_from_file(int32_t s, FILE *f) {
    // return code
    int32_t result = 0;
    
    // compute the file size
    int fd = fileno(f);
    
    // get the current position in the file
    int current = ftell(f);

    // to the end of file, gives the remaining bytes to send
    int fsize = lseek(fd, 0, SEEK_END);
    
    if (current != 0) {
        fsize = fsize - current;
    }
    lseek(fd, current, SEEK_SET);
        
    int buf_size = fsize+32;
    char * buf=NULL;

    // align memory (64bytes = 0x40) when alocating the buffer
    do {
        buf_size -= 32;
        if (buf_size < 0) {
			display("! ERROR : failed to allocate buf!");
            return -ENOMEM;
        }
        buf = (char *)memalign(0x40, buf_size);
        if (buf) memset(buf, 0x00, buf_size);
    } while(!buf);

    if (!buf) {
		display("! ERROR : failed to allocate buf!");
        return -ENOMEM;
    }
    // fail over DEFAULT_NET_BUFFER_SIZE*2
    int bufferSize = DEFAULT_NET_BUFFER_SIZE*2;
    if (setsockopt(s, SOL_SOCKET, SO_SNDBUF, &bufferSize, sizeof(bufferSize))!=0)
        {display("! ERROR : SNDBUF failed !");}
    
    setvbuf(f, NULL, _IOFBF, buf_size);
	int32_t bytes_read = fread(buf, 1, buf_size, f);
    
    if (bytes_read  < buf_size) {
        // end of file ? or failed to read f ?
        if (!feof(f)) {
            display("! ERROR : failed to read file!");
            display("! ERROR : fread = %d and bytes = %d", bytes_read, buf_size);
            display("! ERROR : errno = %d", errno);        
            return-100;
        }
    }
    
    // will loop on sending network packets to reach buf_size data sent
    int32_t remaining = bytes_read;
    set_blocking(s, true);
    while (remaining) {

        int32_t bytes_transferred = network_write(s, buf, remaining);

        if (bytes_transferred > 0) {
            remaining -= bytes_transferred;
            buf += bytes_transferred;

        } else if (bytes_transferred < 0) {

            if (bytes_transferred == -ENOMEM) {
                display("! ERROR : out of memory in send_exact");
            }
            // ERROR
            result = bytes_transferred;
            break;
        } 
    }
    set_blocking(s, false);
    if (!buf) free(buf);
    return result;
}


int32_t recv_to_file(int32_t s, FILE *f) {
    // return code
    int32_t result = 0;
    
    // buf_size is set to max data network recv size
    int buf_size = DEFAULT_NET_BUFFER_SIZE*10+32;
    char * buf=NULL;

    // align memory (64bytes = 0x40) when alocating the buffer
    do {
        buf_size -= 32;
        if (buf_size < 0) {
            display("! ERROR : failed to allocate buf!");
            return -ENOMEM;
        }
        buf = (char *)memalign(0x40, buf_size);
        if (buf) memset(buf, 0x00, buf_size);
    } while(!buf);

    if (!buf) {
        display("! ERROR : failed to allocate buf!");
        return -ENOMEM;
    }
    
    // some optmizations that affect upload speed (and lower download one)
    int enable = 1;
     // Activate TCP SAck
    if (setsockopt(s, SOL_SOCKET, SO_TCPSACK, &enable, sizeof(enable))!=0) 
        {display("! ERROR : TCP SAck activation failed !");}
    
    // SO_OOBINLINE
    if (setsockopt(s, SOL_SOCKET, SO_OOBINLINE, &enable, sizeof(enable))!=0) 
        {display("! ERROR : Force to leave received OOB data in line failed !");}
    
    // TCP_NODELAY 
    if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable))!=0) 
        {display("! ERROR : Disable Nagle's algorithm failed !");}

    // Suppress delayed ACKs
    if (setsockopt(s, IPPROTO_TCP, TCP_NOACKDELAY, &enable, sizeof(enable))!=0)
        {display("! ERROR : Suppress delayed ACKs failed !");}
        
    // set the max buffer recv size (system will double this value => so = buf_size)
    int bufferSize = DEFAULT_NET_BUFFER_SIZE*5;    
    if (setsockopt(s, SOL_SOCKET, SO_RCVBUF, &bufferSize, sizeof(bufferSize))!=0)
        {display("! ERROR : RCVBUF failed !");}

    int32_t bytes_received = buf_size;
    setvbuf(f, NULL, _IOFBF, bufferSize);
    set_blocking(s, true);

    // this while loop hard limiting to 1 slot for upload on client side
    while (bytes_received) {
        
        bytes_received = network_read(s, buf, buf_size);       
        if (bytes_received < 0) {

            if (bytes_received == -ENOMEM) {
                display("! ERROR : out of memory in recv_to_file");
            }

            // still fail, exit code
            // error
            result = bytes_received;
            
        } else if (bytes_received == 0) {
            // connection is closed
            // get the fd
            int fd=fileno(f);
            ChmodFile(fd);
            // FINISHED : file sucessfully uploaded and chmoded
            result = 0;
        }

        // SUCESS : network_read success : write bytes_received to f
        int32_t bytes_written = fwrite(buf, 1, bytes_received, f);
        if (bytes_written < bytes_received)
        {
            // error when writing f
            display("! ERROR : failed to write file!");
            display("! ERROR : fwrite = %d and bytes=%d", bytes_written, bytes_received);
            display("! ERROR : errno = %d", errno);

            result = -100;
        }
    }
    set_blocking(s, false);
    if (!buf) free(buf);
    return result;
}
