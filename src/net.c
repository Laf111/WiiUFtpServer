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
#define SOCKLIB_BUFSIZE MAX_NET_BUFFER_SIZE*4 // For send & receive + double buffering

#define NET_STACK_SIZE 0x2000

extern int somemopt (int req_type, char* mem, unsigned int memlen, int flags); 
extern void logLine(const char *line);

static uint32_t hostIpAddress = 0;

// main thread (default thread of CPU0)
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
		logLine("Socket optimizer: OUT OF MEMORY!");
		return 1;
	}
	
	if (somemopt(0x01, buf, SOCKLIB_BUFSIZE, 0) == -1 && socketlasterr() != 50)
	{
		logLine("somemopt failed!");
		return 1;
	}
	
	MEMFreeToDefaultHeap(buf);
	
    return 0;
}

int socketThreadMain(int argc, const char **argv)
{
    socket_lib_init();
    
	socketOptThreadStack = MEMAllocFromDefaultHeapEx(NET_STACK_SIZE, 8);
	
	if (socketOptThreadStack == NULL || !OSCreateThread(&socketOptThread, socketOptThreadMain, 0, NULL, socketOptThreadStack + NET_STACK_SIZE, NET_STACK_SIZE, 0, OS_THREAD_ATTRIB_AFFINITY_ANY))
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
        logLine("! ERROR : when getting socketThread!");
        return;
    }
    if (!OSSetThreadPriority(socketThread, 1))
        logLine("! WARNING: Error changing net thread priority!");
    
    OSSetThreadName(socketThread, "Network thread on CPU0");
    OSRunThread(socketThread, socketThreadMain, 0, NULL);    

    OSResumeThread(socketThread);

    int ret;
    OSJoinThread(socketThread, &ret);
    
}

void finalize_network()
{
    socket_lib_finish();

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
    if (type == SOCK_STREAM)
    {
        int enable = 1;
        
        // Activate WinScale
        if (setsockopt(sock, SOL_SOCKET, SO_WINSCALE, &enable, sizeof(enable))!=0) 
            {if (!initDone) logLine("! ERROR : WinScale activation failed !");}        

        // Activate TCP SAck
        if (setsockopt(sock, SOL_SOCKET, SO_TCPSACK, &enable, sizeof(enable))!=0) 
            {if (!initDone) logLine("! ERROR : TCP SAck activation failed !");}
        
        // SO_OOBINLINE
        if (setsockopt(sock, SOL_SOCKET, SO_OOBINLINE, &enable, sizeof(enable))!=0) 
            {if (!initDone) logLine("! ERROR : Force to leave received OOB data in line failed !");}
        
        // TCP_NODELAY 
        if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable))!=0) 
            {if (!initDone) logLine("! ERROR : Disable Nagle's algorithm failed !");}

        // Suppress delayed ACKs
        if (setsockopt(sock, IPPROTO_TCP, TCP_NOACKDELAY, &enable, sizeof(enable))!=0)
            {if (!initDone) logLine("! ERROR : Suppress delayed ACKs failed !");}
        
        // SO_NOSLOWSTART
        if (setsockopt(sock, SOL_SOCKET, SO_NOSLOWSTART, &enable, sizeof(enable))!=0) 
            {if (!initDone) logLine("! ERROR : Disable slow start feature failed !");}

/* Those ones fail
        // SO_BIGCWND
        if (setsockopt(sock, SOL_SOCKET, SO_BIGCWND, &enable, sizeof(enable))==0) 
            {if (!initDone) logLine("> SO_BIGCWND enabled");}
        else 
            {if (!initDone) logLine("! ERROR : SO_BIGCWND activation failed !");}
         
        // SO_TIMESTAMP
         if (setsockopt(sock, SOL_SOCKET, SO_TIMESTAMP, &enable, sizeof(enable))==0) 
            {if (!initDone) logLine("> SO_TIMESTAMP enabled");}
        else 
            {if (!initDone) display("! ERROR : SO_TIMESTAMP activation failed !");}
*/      
        
         // Minimize default I/O buffers size (system double the value set)
        int bufferSize = MIN_NET_BUFFER_SIZE/4; // 1024
        setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &bufferSize, sizeof(bufferSize));
        setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &bufferSize, sizeof(bufferSize));
      
        if (!initDone) {
            initDone = true;
            logLine("Limited to 1 client and 1 slot for up/download !");
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
    int buf_size = MAX_NET_BUFFER_SIZE*2;
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
            
        } else if (bytes_transferred < 0) {
            if (buf_size > MIN_NET_BUFFER_SIZE) {
                buf_size = buf_size - MIN_NET_BUFFER_SIZE;
                OSSleepTicks(OSMillisecondsToTicks(100));
                goto try_again_with_smaller_buffer;
            }
			if (bytes_transferred == -EINPROGRESS || bytes_transferred == -EALREADY || bytes_transferred == -EISCONN) goto try_again_with_smaller_buffer;  
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
    int buf_size = MAX_NET_BUFFER_SIZE*2;
    char * buf = NULL;
    buf = MEMAllocFromDefaultHeapEx(buf_size, 64);
    if (!buf)
        return -99;
    
    int bufferSize = MAX_NET_BUFFER_SIZE;
    if (setsockopt(s, SOL_SOCKET, SO_SNDBUF, &bufferSize, sizeof(bufferSize))!=0)
        {logLine("! ERROR : SNDBUF failed !");}
    
    int32_t bytes_read = buf_size;
    int32_t result = 0;

    while (bytes_read) {  
        bytes_read = fread(buf, 1, buf_size, f);
        if (bytes_read > 0) {
            result = send_exact(s, buf, bytes_read);
            if (result < 0) goto end; 
	    } else if (bytes_read < 0) {
	        // fail to read file f
	        if (!buf) MEMFreeToDefaultHeap(buf);
	        return bytes_read;
	    }
	    if (bytes_read < buf_size) {
	        // end of file ? or failed to read f ?
	        return -!feof(f);
	    }
	}
    // RUNNING : 
    if (!buf) MEMFreeToDefaultHeap(buf);
    return -EAGAIN;
    
    end:
    if (!buf) free(buf);
    return result;
}

int32_t recv_to_file(int32_t s, FILE *f) {
    
    int buf_size = MAX_NET_BUFFER_SIZE*2;
    char * buf = NULL;
    buf = MEMAllocFromDefaultHeapEx(buf_size, 64);
    if (!buf)
        return -99;

    int bufferSize = MAX_NET_BUFFER_SIZE;
    if (setsockopt(s, SOL_SOCKET, SO_RCVBUF, &bufferSize, sizeof(bufferSize))!=0)
        {logLine("! ERROR : RCVBUF failed !");}
    
    int32_t bytes_received = buf_size;

    while (bytes_received) {    
        try_again_with_smaller_buffer:
		set_blocking(s, true);
        bytes_received = network_read(s, buf, buf_size);
        set_blocking(s, false);
        if (bytes_received < 0) {
            
            // failure !
            if (buf_size > MIN_NET_BUFFER_SIZE) {
                // try to reduce the buffer size
                buf_size = buf_size - MIN_NET_BUFFER_SIZE;
                OSSleepTicks(OSMillisecondsToTicks(100));
                goto try_again_with_smaller_buffer;
            }
            if (bytes_received == -EINPROGRESS || bytes_received == -EALREADY || bytes_received == -EISCONN) goto try_again_with_smaller_buffer;
            // still fail, exit code
            if (!buf) MEMFreeToDefaultHeap(buf);
            // error
            return bytes_received;        
        } else if (bytes_received == 0) {
            // connection is closed
        
            // get the fd 
            int fd=fileno(f);
            ChmodFile(fd);
            if (!buf) MEMFreeToDefaultHeap(buf);
            // FINISHED : file sucessfully uploaded and chmoded
            return 0;
        }

        // SUCESS : network_read success : write bytes_received to f
        int32_t bytes_written = fwrite(buf, 1, bytes_received, f);
        if (bytes_written < bytes_received)
        {   
            // error when writing f
            if (!buf) MEMFreeToDefaultHeap(buf);
            return -100;
        }
    }
    if (!buf) MEMFreeToDefaultHeap(buf);
    return -1;
}
