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
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <unistd.h>
#include <sys/fcntl.h>

#include "common/types.h"
#include "dynamic_libs/os_functions.h"
#include "dynamic_libs/socket_functions.h"
#include "net.h"

#define BUFFER_SIZE_MAX (MAX_NET_BUFFER_SIZE*2)+32-64

extern u32 hostIpAddress;
extern void display(const char *format, ...);

static bool initDone = false;

int getsocketerrno()
{
    int res = socketlasterr();
    return res;
}

s32 network_socket(u32 domain,u32 type,u32 protocol)
{
    int sock = socket(domain, type, protocol);
    if(sock < 0)
    {
        int err = -getsocketerrno();
        return (err < 0) ? err : sock;
    }
    if (type == SOCK_STREAM)
    {
        if (!initDone) display("-------------- socket optimizations --------------");
        int enable = 1;

        // Activate WinScale
        if (setsockopt(sock, SOL_SOCKET, SO_WINSCALE, &enable, sizeof(enable))==0) 
            {if (!initDone) display("> WinScale enabled");}
        else 
            {if (!initDone) display("! ERROR : WinScale activation failed !");}
        
        // Activate TCP SAck
        if (setsockopt(sock, SOL_SOCKET, SO_TCPSACK, &enable, sizeof(enable))==0) 
            {if (!initDone) display("> TCP SAck enabled");}
        else 
            {if (!initDone) display("! ERROR : TCP SAck activation failed !");}
 
        /* Disable the Nagle (TCP No Delay) algorithm */
        if (setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char *)&enable, sizeof(enable))==0)
            {if (!initDone) display("> Nagle disabled");}
        else 
            {if (!initDone) display("! ERROR : disabling the Nagle failed !");}    
 
         // minimize default I/O buffers size
        int bufferSize = MIN_NET_BUFFER_SIZE/4;
        if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &bufferSize, sizeof(bufferSize))==0) 
            {if (!initDone) display("> Reserve RCV socket buffer");}
        else 
            {if (!initDone) display("! ERROR : RCVBUF failed !");}
        
        if (setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &bufferSize, sizeof(bufferSize))==0)
            {if (!initDone) display("> Reserve SND socket buffer");}
        else 
            {if (!initDone) display("! ERROR : SNDBUF failed !");}
        /*
        // Socket memory optimization : always fail...
        
        if (setsockopt(sock, SOL_SOCKET, SO_USERBUF, &enable, sizeof(enable))==0)
            {if (!initDone) display("> Socket memory option activated");}
        else 
            {if (!initDone) display("! ERROR : Socket memory option failed !");}
        */
        if (!initDone) {
            initDone = true;
            display("--------------------------------------------------");
        }
        
    }
    return sock;
}

s32 network_bind(s32 s,struct sockaddr *name,s32 namelen)
{
    int res = bind(s, name, namelen);
    if(res < 0)
    {
        int err = -getsocketerrno();
        return (err < 0) ? err : res;
    }
    return res;
}

s32 network_listen(s32 s,u32 backlog)
{
    int res = listen(s, backlog);
    if(res < 0)
    {
        int err = -getsocketerrno();
        return (err < 0) ? err : res;
    }
    return res;
}

s32 network_accept(s32 s,struct sockaddr *addr,s32 *addrlen)
{
    int res = accept(s, addr, addrlen);
    if(res < 0)
    {
        int err = -getsocketerrno();
        return (err < 0) ? err : res;
    }
    return res;
}

s32 network_connect(s32 s,struct sockaddr *addr, s32 addrlen)
{
    int res = connect(s, addr, addrlen);
    if(res < 0)
    {
        int err = -getsocketerrno();
        return (err < 0) ? err : res;
    }
    return res;
}

s32 network_read(s32 s, char *mem,s32 len)
{
    int res = recv(s, mem, len, 0);
    if(res < 0)
    {
        int err = -getsocketerrno();
        return (err < 0) ? err : res;
    }
    return res;
}

u32 network_gethostip()
{
    return hostIpAddress;
}

s32 network_write(s32 s, const char *mem,s32 len)
{
    s32 transfered = 0;

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

s32 network_close(s32 s)
{
    if(s < 0)
        return -1;

    return socketclose(s);
}

s32 set_blocking(s32 s, bool blocking) {
    s32 block = !blocking;
    setsockopt(s, SOL_SOCKET, SO_NONBLOCK, &block, sizeof(block));
    return 0;
}

s32 network_close_blocking(s32 s) {
    set_blocking(s, true);
    return network_close(s);
}

s32 send_exact(s32 s, char *buf, s32 length) {
    int buf_size = BUFFER_SIZE_MAX;
    s32 result = 0;
    s32 remaining = length;
    s32 bytes_transferred;
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
                usleep(500);
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

s32 send_from_file(s32 s, FILE *f) {
    int buf_size = BUFFER_SIZE_MAX;
     char * buf=NULL;

    // align memory (64bytes = 0x40) when alocating the buffer
    do{
        buf_size -= 32;
        if (buf_size < 0) {
            return -5;
        }
        buf = (char *)memalign(0x40, buf_size);
        if (buf) memset(buf, 0x00, buf_size);
    }while(!buf);
    
    int bufferSize=MAX_NET_BUFFER_SIZE;
    if (setsockopt(s, SOL_SOCKET, SO_SNDBUF, &bufferSize, sizeof(bufferSize))!=0)
        {display("! ERROR : SNDBUF failed !");}
         
    s32 bytes_read;
    s32 result = 0;

    bytes_read = fread(buf, 1, buf_size, f);
    if (bytes_read > 0) {
        result = send_exact(s, buf, bytes_read);
        if (result < 0) goto end;
    }
    if (bytes_read < (s32) buf_size) {
        result = -!feof(f);
        goto end;
    }
    free(buf);
    return -EAGAIN;
    end:
    free(buf);
    return result;
}

s32 recv_to_file(s32 s, FILE *f) {
    
    int buf_size = BUFFER_SIZE_MAX;
    char * buf=NULL;

    // align memory (64bytes = 0x40) when alocating the buffer
    do{
        buf_size -= 32;
        if (buf_size < 0) {
            return -5;
        }
        buf = (char *)memalign(0x40, buf_size);
        if (buf) memset(buf, 0x00, buf_size);
    }while(!buf);
    
    int bufferSize=MAX_NET_BUFFER_SIZE;
    if (setsockopt(s, SOL_SOCKET, SO_RCVBUF, &bufferSize, sizeof(bufferSize))!=0)
        {display("! ERROR : RCVBUF failed !");}
        
    s32 bytes_read;
    while (1) {
        try_again_with_smaller_buffer:
        // network_read call recv() that return the number of bytes read
        bytes_read = network_read(s, buf, buf_size);
        if (bytes_read < 0) {
            if (buf_size > MIN_NET_BUFFER_SIZE) {
                buf_size = buf_size - MIN_NET_BUFFER_SIZE;
                usleep(500);
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
        
        s32 bytes_written = fwrite(buf, 1, bytes_read, f);
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
