/****************************************************************************
  * WiiUFtpServer
  * 2021/04/05:V1.0.0:Laf111: import ftp-everywhere code
  * 2021/05/08:V2.2.0:Laf111: decrease buffer with MIN_NET_BUFFER_SIZE steps
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

#define BUFFER_SIZE_MAX (MAX_NET_BUFFER_SIZE*2)+32-64
#define NET_STACK_SIZE 0x2000


extern void logLine(const char *line);

static uint32_t hostIpAddress = 0;

// main thread (default thread of CPU0)
static OSThread *netThread=NULL;
static int *netThreadStack=NULL;


static bool initDone = false;


int getsocketerrno()
{
    int res = socketlasterr();
    if (res == NSN_EAGAIN)
    {
        res = EAGAIN;
    }
    return res;
}


int netThreadMain(int argc, const char **argv)
{


    socket_lib_init();

    return 0;
}

void initialise_network()
{
    unsigned int nn_startupid;

    ACInitialize();
    ACGetStartupId(&nn_startupid);
    ACConnectWithConfigId(nn_startupid);
    ACGetAssignedAddress(&hostIpAddress);

        
    // use default thread on Core 0
    netThread=OSGetDefaultThread(0);
    if (netThread == NULL) {
        logLine("! ERROR : when getting netThread!");
        return;
    }
    
    OSSetThreadName(netThread, "network thread on CPU0");
    OSRunThread(netThread, netThreadMain, 0, NULL);    
    
    if (!OSSetThreadPriority(netThread, 0))
        logLine("! WNG: Error changing net thread priority!");

    OSResumeThread(netThread);

    int ret;
    OSJoinThread(netThread, &ret);

}

void finalize_network()
{
    socket_lib_finish();

    if (netThreadStack != NULL) MEMFreeToDefaultHeap(netThreadStack);

}

int32_t network_socket(uint32_t domain,uint32_t type,uint32_t protocol)
{
    int sock = socket(domain, type, protocol);
    if(sock < 0)
    {
        int err = -getsocketerrno();
        return (err < 0) ? err : sock;
    }
    if (type == SOCK_STREAM)
    {
        if (!initDone) logLine("-------------- socket optimizations --------------");
        int enable = 1;
        
        // Activate WinScale
        if (setsockopt(sock, SOL_SOCKET, SO_WINSCALE, &enable, sizeof(enable))==0) 
            {if (!initDone) logLine("> WinScale enabled");}
        else 
            {if (!initDone) logLine("OPT : ERROR WinScale activation failed !");}
        
        // Activate TCP SAck
        if (setsockopt(sock, SOL_SOCKET, SO_TCPSACK, &enable, sizeof(enable))==0) 
            {if (!initDone) logLine("> TCP SAck enabled");}
        else 
            {if (!initDone) logLine("! ERROR : TCP SAck activation failed !");}
 
        /* Disable the Nagle (TCP No Delay) algorithm */
        int flag = 1;
        if (setsockopt( sock, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(flag))==0)
            {if (!initDone) logLine("> Nagle disabled");}
        else 
            {if (!initDone) logLine("! ERROR : disabling the Nagle failed !");}    
 
          // minimize default I/O buffers size
        int bufferSize = MIN_NET_BUFFER_SIZE;
        if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &bufferSize, sizeof(bufferSize))==0) 
            {if (!initDone) WHBLogPrintf("> Reserve RCV socket buffer");}
        else 
            {if (!initDone) logLine("! ERROR : RCVBUF failed !");}
        
        if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &bufferSize, sizeof(bufferSize))==0)
            {if (!initDone) WHBLogPrintf("> Reserve SND socket buffer");}
        else 
            {if (!initDone) logLine("! ERROR : SNDBUF failed !");}

        
        if (!initDone) {
            initDone = true;
            WHBLogPrintf("--------------------------------------------------");
        }
        
    }
    return sock;
}

int32_t network_bind(int32_t s,struct sockaddr *name,int32_t namelen)
{
    int res = bind(s, name, namelen);
    if(res < 0)
    {
        int err = -getsocketerrno();
        return (err < 0) ? err : res;
    }
    return res;
}

int32_t network_listen(int32_t s,uint32_t backlog)
{
    int res = listen(s, backlog);
    if(res < 0)
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
    if(res < 0)
    {
        int err = -getsocketerrno();
        return (err < 0) ? err : res;
    }
    return res;
}

int32_t network_connect(int32_t s,struct sockaddr *addr, int32_t addrlen)
{
    int res = connect(s, addr, addrlen);
    if(res < 0)
    {
        int err = -getsocketerrno();
        return (err < 0) ? err : res;
    }
    return res;
}

int32_t network_read(int32_t s,void *mem,int32_t len)
{
    int res = recv(s, mem, len, 0);
    if(res < 0)
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

    while(len)
    {
        int ret = send(s, mem, len, 0);
        if(ret < 0)
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
    if(s < 0)
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
    int buf_size = BUFFER_SIZE_MAX;
	int32_t result = 0;
    int32_t remaining = length;
    int32_t bytes_transferred;
    set_blocking(s, true);
    while (remaining) {
        try_again_with_smaller_buffer:
        bytes_transferred = network_write(s, buf, MIN(remaining, (int) buf_size));
        if (bytes_transferred > 0) {
            remaining -= bytes_transferred;
            buf += bytes_transferred;
            
            // restore the whole buffer after a successfully network read
            if (buf_size < BUFFER_SIZE_MAX) buf_size = BUFFER_SIZE_MAX;        
            
        } else if (bytes_transferred < 0) {
            if (buf_size > MIN_NET_BUFFER_SIZE) {
                buf_size = buf_size - MIN_NET_BUFFER_SIZE;
                OSSleepTicks(OSMillisecondsToTicks(500));
                goto try_again_with_smaller_buffer;
            }

            result = bytes_transferred;
            break;
        } else {
            result = -ENODATA;
            break;
        }
    }
    set_blocking(s, false);
    return result;
}

int32_t send_from_file(int32_t s, FILE *f) {
    int buf_size = BUFFER_SIZE_MAX;
    char * buf = NULL;
    buf = MEMAllocFromDefaultHeapEx(buf_size, 64);
    if(!buf)
        return -1;

    int bufferSize=MAX_NET_BUFFER_SIZE;
    if (setsockopt(s, SOL_SOCKET, SO_SNDBUF, &bufferSize, sizeof(bufferSize))!=0)
        {logLine("! ERROR : SNDBUF failed !");}
 
    int32_t bytes_read;
    int32_t result = 0;

    bytes_read = fread(buf, 1, buf_size, f);
    if (bytes_read > 0) {
        result = send_exact(s, buf, bytes_read);
        if (result < 0) goto end;
    }
    if (bytes_read < (int32_t) buf_size) {
        result = -!feof(f);
        goto end;
    }

    free(buf);
    return -EAGAIN;
    
end:
    free(buf);
    return result;
}

int32_t recv_to_file(int32_t s, FILE *f) {
    
    int buf_size = BUFFER_SIZE_MAX;
    char * buf = NULL;
    buf = MEMAllocFromDefaultHeapEx(buf_size, 64);
    if(!buf)
        return -1;

    int bufferSize=MAX_NET_BUFFER_SIZE;
    if (setsockopt(s, SOL_SOCKET, SO_RCVBUF, &bufferSize, sizeof(bufferSize))!=0)
        {logLine("! ERROR : RCVBUF failed !");}
    
    int32_t bytes_read;
    while (1) {
        try_again_with_smaller_buffer:
        // network_read call recv() that return the number of bytes read
        bytes_read = network_read(s, buf, buf_size);
        if (bytes_read < 0) {
            if (buf_size > MIN_NET_BUFFER_SIZE) {
                buf_size = buf_size - MIN_NET_BUFFER_SIZE;
                OSSleepTicks(OSMillisecondsToTicks(500));
                goto try_again_with_smaller_buffer;
            }

            free(buf);
            return bytes_read;
        } else if (bytes_read == 0) {
            // get the fd 
            int fd=fileno(f);
            ChmodFile(fd);
            free(buf);
            return 0;
        }
        
        int32_t bytes_written = fwrite(buf, 1, bytes_read, f);
        if (bytes_written < bytes_read)
        {
            free(buf);
            return -1;
        }
        // restore the whole buffer after a successfully network read
        if (buf_size < BUFFER_SIZE_MAX) buf_size = BUFFER_SIZE_MAX;
    }
    return -1;
}

