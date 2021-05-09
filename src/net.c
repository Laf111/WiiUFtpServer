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

#include <errno.h>
#include <sys/fcntl.h>

#include <nn/ac.h>
#include "vrt.h"
#include "net.h"

#ifndef MIN
    #define MIN(x, y) ((x) < (y) ? (x) : (y))
#endif

static uint32_t NET_BUFFER_SIZE = MAX_NET_BUFFER_SIZE;

uint32_t hostIpAddress = 0;

void initialise_network()
{
    unsigned int nn_startupid;

    ACInitialize();
    ACGetStartupId(&nn_startupid);
    ACConnectWithConfigId(nn_startupid);
    ACGetAssignedAddress(&hostIpAddress);

	socket_lib_init();
}

void finalize_network()
{
	socket_lib_finish();
}

int getsocketerrno()
{
	int res = socketlasterr();
    if (res == NSN_EAGAIN)
    {
		res = EAGAIN;
    }
	return res;
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
        int tcpsack = 1, winscale = 1, rcvbuf = 128 * 1024;
        setsockopt(sock, SOL_SOCKET, SO_TCPSACK, &tcpsack, sizeof(tcpsack));
        setsockopt(sock, SOL_SOCKET, SO_WINSCALE, &winscale, sizeof(winscale));
        setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof(rcvbuf));
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

int32_t create_server(uint16_t port) {
	int32_t server = network_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
	if (server < 0)
		return -1;


	set_blocking(server, false);
    uint32_t enable = 1;
	setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

	struct sockaddr_in bindAddress;
	memset(&bindAddress, 0, sizeof(bindAddress));
	bindAddress.sin_family = AF_INET;
	bindAddress.sin_port = htons(port);
	bindAddress.sin_addr.s_addr = htonl(INADDR_ANY);

	int32_t ret;
	if ((ret = network_bind(server, (struct sockaddr *)&bindAddress, sizeof(bindAddress))) < 0) {
		network_close(server);
		//gxprintf("Error binding socket: [%i] %s\n", -ret, strerror(-ret));
		return ret;
	}
	if ((ret = network_listen(server, 3)) < 0) {
		network_close(server);
		//gxprintf("Error listening on socket: [%i] %s\n", -ret, strerror(-ret));
		return ret;
	}

	return server;
}

typedef int32_t (*transferrer_type)(int32_t s, void *mem, int32_t len);

static int32_t transfer_exact(int32_t s, char *buf, int32_t length, transferrer_type transferrer) {
	int32_t result = 0;
    
	int32_t remaining = length;
	int32_t bytes_transferred;
	set_blocking(s, true);
	while (remaining) {
		try_again_with_smaller_buffer:
		bytes_transferred = transferrer(s, buf, MIN(remaining, (int) NET_BUFFER_SIZE));
		if (bytes_transferred > 0) {
			remaining -= bytes_transferred;
			buf += bytes_transferred;
		} else if (bytes_transferred < 0) {
			if (NET_BUFFER_SIZE > MIN_NET_BUFFER_SIZE) {
				NET_BUFFER_SIZE = NET_BUFFER_SIZE - MIN_NET_BUFFER_SIZE;
				OSSleepTicks(OSMillisecondsToTicks(100));
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

int32_t send_exact(int32_t s, char *buf, int32_t length) {
	return transfer_exact(s, buf, length, (transferrer_type)network_write);
}

int32_t send_from_file(int32_t s, FILE *f) {
	char * buf = (char *) malloc(FREAD_BUFFER_SIZE);
	if(!buf)
		return -1;

	int32_t bytes_read;
	int32_t result = 0;

	bytes_read = fread(buf, 1, FREAD_BUFFER_SIZE, f);
	if (bytes_read > 0) {
		result = send_exact(s, buf, bytes_read);
		if (result < 0) goto end;
	}
	if (bytes_read < FREAD_BUFFER_SIZE) {
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
    
	int buf_size = NET_BUFFER_SIZE+32;
 	uint8_t * buf=NULL;

    // align memory (64bytes = 0x40) when alocating the buffer
	do{
		buf_size -= 32;
		if (buf_size < 0) {
            WHBLogPrintf("ERROR failed to allocate buf"); 
            WHBLogConsoleDraw();
			return -5;
		}
		buf = (uint8_t *)memalign(0x40, buf_size);
		if (buf) memset(buf, 0x00, buf_size);
	}while(!buf);    
	int32_t bytes_read;
	while (1) {
		try_again_with_smaller_buffer:
        // network_read call recv() that return the number of bytes read
		bytes_read = network_read(s, buf, NET_BUFFER_SIZE);
		if (bytes_read < 0) {
			if (NET_BUFFER_SIZE > MIN_NET_BUFFER_SIZE) {
				NET_BUFFER_SIZE = NET_BUFFER_SIZE - MIN_NET_BUFFER_SIZE;
				OSSleepTicks(OSMillisecondsToTicks(100));
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
	}
	return -1;
}

