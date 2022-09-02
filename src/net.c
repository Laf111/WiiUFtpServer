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


extern void display(const char *format, ...);
extern int fsaFd;

extern bool verboseMode;

static const int retriesNumber = (int) ((float)(FTP_CONNECTION_TIMEOUT) / ((float)NET_RETRY_TIME_STEP_MILLISECS/1000.0));
static bool initDone = false;

int getsocketerrno()
{
    int res = socketlasterr();
    return res;

}

static bool retry(int32_t socketError) {
    bool status = false;
    
    // retry
    if (socketError == -EINPROGRESS ||
        socketError == -EALREADY ||        
        socketError == -EBUSY ||
        socketError == -ETIME ||
        socketError == -ECONNREFUSED ||
        socketError == -ECONNABORTED ||
        socketError == -ETIMEDOUT ||
        socketError == -EMFILE ||
        socketError == -ENFILE ||
        socketError == -EHOSTUNREACH ||
        socketError == -EISCONN) status = true;

    if (verboseMode) {    
        if (status) display("~ WARNING : Retrying transfer after = %d (%s)", socketError, strerror(socketError));
    }
    return status;
}
s32 network_socket(u32 domain,u32 type,u32 protocol)
{
    int s = socket(domain, type, protocol);
    if (s < 0)
    {
        int err = -getsocketerrno();
        return (err < 0) ? err : s;
    }
    if (type == SOCK_STREAM)
    {
        if (!initDone) display("-------------- socket optimizations --------------");
        s32 enable = 1;

		// Reuse socket
	    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable))!=0) 
	        {if (!initDone) display("! ERROR : setsockopt / SO_REUSEADDR failed !");}
			     
        
        // Activate WinScale
        if (setsockopt(s, SOL_SOCKET, SO_WINSCALE, &enable, sizeof(enable))!=0) 
            {if (!initDone) display("! ERROR : setsockopt / winScale activation failed !");}
        
        // Set to non-blocking I/O
        set_blocking(s, false); 
        if (!initDone) {
            initDone = true;
            display("  1 client only using a max of %d slots for up/download!", NB_SIMULTANEOUS_TRANSFERS);
            display("   (set your client's timeout greater than %d seconds)", FTP_CONNECTION_TIMEOUT+5);
        }
	}
    return s;
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
    if (res < 0)
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

static s32 network_readChunk(s32 s, char *mem, s32 len) {

    s32 received = 0;
    s32 ret = -1;

    // while buffer is not full (len>0)
    while (len>0)
    {
        // max ret value is 2*SOCKET_BUFFER_SIZE
        ret = recv(s, mem, len, 0);        
        if (ret == 0) {
            // client EOF detected
            break;
        } else if (ret < 0 && getsocketerrno() != EAGAIN) {
            int err = -getsocketerrno();
            received = (err < 0) ? err : ret;
            break;
        } else {
            if (ret > 0) {
                received += ret;
                len -= ret;
                mem += ret;
            }
        }
    }
    // here len could be < 0 and so more than len bytes are read
    // received > len and mem up to date
    return received;
}


s32 network_write(s32 s, const char *mem,s32 len)
{
    s32 transfered = 0;

    while(len)
    {
        int ret = send(s, mem, len, 0);
        if (ret < 0 && getsocketerrno() != EAGAIN)
        {
            int err = -getsocketerrno();
            transfered = (err < 0) ? err : ret;
            break;
        } else {
            if (ret > 0) {
                mem += ret;
                transfered += ret;
                len -= ret;
            }
        }
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
    int buf_size = length;
    int32_t result = 0;
    int32_t remaining = length;
    int32_t bytes_transferred;
    
    uint32_t retryNumber = 0;

    set_blocking(s, true);
    while (remaining) {

        retry:
        bytes_transferred = network_write(s, buf, MIN(remaining, (int) buf_size));
        
        if (bytes_transferred > 0) {
            remaining -= bytes_transferred;
            buf += bytes_transferred;
        } else if (bytes_transferred < 0) {

            if (retry(bytes_transferred)) {
                usleep(NET_RETRY_TIME_STEP_MILLISECS);
                retryNumber++;
                if (retryNumber <= retriesNumber) goto retry;
            }
            
    if (verboseMode) { 
            display("! ERROR : network_write failed = %d afer %d attempts", bytes_transferred, retriesNumber);
            display("! ERROR : errno = %d (%s)", getsocketerrno(), strerror(getsocketerrno()));            
    }
            result = bytes_transferred;
            break;
        } else {
            // result = bytes_transferred = 0
            result = bytes_transferred;                            
            break;
        }

    }
    set_blocking(s, false);
    
    return result;
}



int32_t send_from_file(int32_t s, connection_t* connection) {
    // return code
    int32_t result = 0;
    

    // max value for SNDBUF = SOCKET_BUFFER_SIZE (the system double the value set)
    int sndBuffSize = SOCKET_BUFFER_SIZE/2;
    if (setsockopt(s, SOL_SOCKET, SO_SNDBUF, &sndBuffSize, sizeof(sndBuffSize))!=0)
        {display("! ERROR : setsockopt / SNDBUF failed !");
    }
	
    if (verboseMode) {    
        display("C[%d] sending %d bytes of %s on socket %d", connection->index+1, 2*SOCKET_BUFFER_SIZE, connection->fileName,s);
    }


    int32_t bytes_read = TRANSFER_BUFFER_SIZE;        
	while (bytes_read) {

        bytes_read = fread(connection->transferBuffer, 1, TRANSFER_BUFFER_SIZE, connection->f);
        if (bytes_read == 0) {
            // SUCCESS, no more to write                  
            result = 0;
            break;
        }        
        if (bytes_read > 0) {
        
            uint32_t retryNumber = 0;
            int32_t remaining = bytes_read;            

            // Let buffer on file be larger than socket one for checking performances scenarii 
            while (remaining) {
            
                send_again:
                result = network_write(s, connection->transferBuffer, MIN(remaining, TRANSFER_BUFFER_SIZE));

                if (verboseMode) {    
                    display("C[%d] sent %d bytes of %s", connection->index+1, result, connection->fileName);
                }
                
                if (result < 0) {
                    if (retry(result)) {
                        usleep(NET_RETRY_TIME_STEP_MILLISECS);
                        retryNumber++;
                        if (retryNumber <= retriesNumber) goto send_again;
                    }            
                    // result = error, connection will be closed
                    break;
                } else {
                

                    // data block sent sucessfully, continue
                    connection->dataTransferOffset += result;
                    connection->bytesTransferred = result;
                    remaining -= result;                    
                                        
                }
            }        
		}
        if (result >=0) {
                
            // check bytes read (now because on the last sending, data is already sent here = result)
            if (bytes_read < TRANSFER_BUFFER_SIZE) {
                    
            	if (bytes_read < 0 || feof(connection->f) == 0 || ferror(connection->f) != 0) {
                    // ERROR : not on eof file or read error, or error on stream => ERROR
                    display("! ERROR : failed to read file!");
                    display("! ERROR : fread = %d and bytes = %d", bytes_read, TRANSFER_BUFFER_SIZE);
                    display("! ERROR : errno = %d (%s)", getsocketerrno(), strerror(getsocketerrno())); 
                    result = -103;
                    break;
                }
            }
            
            // result = 0 and EOF
            if ((feof(connection->f) != 0) && (result == 0)) {
                // SUCESS : eof file, last data bloc sent
                break;
            }
        }
    }
    connection->bytesTransferred = result;

    return result;
}


int32_t recv_to_file(int32_t s, connection_t* connection) {
    // return code
    int32_t result = 0;
    
    // (the system double the value set)
    int rcvBuffSize = SOCKET_BUFFER_SIZE;
    if (setsockopt(s, SOL_SOCKET, SO_RCVBUF, &rcvBuffSize, sizeof(rcvBuffSize))!=0)
        {display("! ERROR : setsockopt / RCVBUF failed !");}
	
    if (verboseMode) {    
        display("C[%d] receiving %s on socket %d", connection->index+1, connection->fileName, s);
    }


    uint32_t retryNumber = 0;
	// network_readChunk can overflow but less than (rcvBuffSize*2) bytes
	// considering a buffer size of TRANSFER_BUFFER_SIZE - (rcvBuffSize*2) to handle the overflow
	uint32_t chunckSize = TRANSFER_BUFFER_SIZE - (SOCKET_BUFFER_SIZE*2);
    int32_t bytes_received = chunckSize;
    while (bytes_received) {
                        
        read_again:        
        bytes_received = network_readChunk(s, connection->transferBuffer, chunckSize);
        
        if (bytes_received == 0) {
            // SUCCESS, no more to write to file
            result = 0;
                        
        } else if (bytes_received < 0) {

            if (retry(bytes_received)) {
                usleep(NET_RETRY_TIME_STEP_MILLISECS);
                retryNumber++;
                if (retryNumber <= retriesNumber) goto read_again;
            }    
            display("! ERROR : network_read failed = %d afer %d attempts", bytes_received, retriesNumber);
            display("! ERROR : errno = %d (%s)", getsocketerrno(), strerror(getsocketerrno()));
            // result = error, connection will be closed
            result = bytes_received;
            break;
        } else {
            // bytes_received > 0
 
            // write bytes_received to f
            result = fwrite(connection->transferBuffer, 1, bytes_received, connection->f);
            
            if ((result < 0 && result < bytes_received) || ferror(connection->f) != 0) {
                // error when writing f
                display("! ERROR : failed to write file!");
                display("! ERROR : fwrite = %d and bytes=%d", result, bytes_received);
                display("! ERROR : errno = %d (%s)", getsocketerrno(), strerror(getsocketerrno()));
                result = -100;
                break;
            } else {
                connection->dataTransferOffset += result;
                connection->bytesTransferred = result;
                
            }
        }
    }
    connection->bytesTransferred = result;
    
    return result;
}

