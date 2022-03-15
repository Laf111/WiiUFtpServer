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
  * 2021-12-05:Laf111:V7-0
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
#include <nsysnet/_socket.h>
#include <coreinit/memdefaultheap.h>

#include <nn/ac.h>

#include <iosuhax.h>

#include "vrt.h"
#include "ftp.h"
#include "net.h"

// // Functions from https://github.com/devkitPro/wut/blob/master/libraries/wutsocket/wut_socket_common.c
void __attribute__((weak)) __init_wut_socket();
void __attribute__((weak))__fini_wut_socket();

#define SOCKET_MOPT_STACK_SIZE 0x2000

extern int somemopt (int req_type, char* mem, unsigned int memlen, int flags);

extern void display(const char *fmt, ...);
extern int fsaFd;
extern bool calculateCrc32;

static uint32_t nbFilesDL = 0;
static uint32_t nbFilesUL = 0;

#ifdef LOG2FILE
    extern void writeToLog(const char *fmt, ...);
#endif

static uint32_t hostIpAddress = 0;

static const int retriesNumber = (int) ((float)(FTP_CONNECTION_TIMEOUT) / ((float)NET_RETRY_TIME_STEP_MILLISECS/1000.0));

static OSThread socketOptThread;
static uint8_t *socketOptThreadStack=NULL;

static bool initDone = false;

static const unsigned long crcTable[256] = {
0x00000000,0x77073096,0xEE0E612C,0x990951BA,0x076DC419,0x706AF48F,0xE963A535,
0x9E6495A3,0x0EDB8832,0x79DCB8A4,0xE0D5E91E,0x97D2D988,0x09B64C2B,0x7EB17CBD,
0xE7B82D07,0x90BF1D91,0x1DB71064,0x6AB020F2,0xF3B97148,0x84BE41DE,0x1ADAD47D,
0x6DDDE4EB,0xF4D4B551,0x83D385C7,0x136C9856,0x646BA8C0,0xFD62F97A,0x8A65C9EC,
0x14015C4F,0x63066CD9,0xFA0F3D63,0x8D080DF5,0x3B6E20C8,0x4C69105E,0xD56041E4,
0xA2677172,0x3C03E4D1,0x4B04D447,0xD20D85FD,0xA50AB56B,0x35B5A8FA,0x42B2986C,
0xDBBBC9D6,0xACBCF940,0x32D86CE3,0x45DF5C75,0xDCD60DCF,0xABD13D59,0x26D930AC,
0x51DE003A,0xC8D75180,0xBFD06116,0x21B4F4B5,0x56B3C423,0xCFBA9599,0xB8BDA50F,
0x2802B89E,0x5F058808,0xC60CD9B2,0xB10BE924,0x2F6F7C87,0x58684C11,0xC1611DAB,
0xB6662D3D,0x76DC4190,0x01DB7106,0x98D220BC,0xEFD5102A,0x71B18589,0x06B6B51F,
0x9FBFE4A5,0xE8B8D433,0x7807C9A2,0x0F00F934,0x9609A88E,0xE10E9818,0x7F6A0DBB,
0x086D3D2D,0x91646C97,0xE6635C01,0x6B6B51F4,0x1C6C6162,0x856530D8,0xF262004E,
0x6C0695ED,0x1B01A57B,0x8208F4C1,0xF50FC457,0x65B0D9C6,0x12B7E950,0x8BBEB8EA,
0xFCB9887C,0x62DD1DDF,0x15DA2D49,0x8CD37CF3,0xFBD44C65,0x4DB26158,0x3AB551CE,
0xA3BC0074,0xD4BB30E2,0x4ADFA541,0x3DD895D7,0xA4D1C46D,0xD3D6F4FB,0x4369E96A,
0x346ED9FC,0xAD678846,0xDA60B8D0,0x44042D73,0x33031DE5,0xAA0A4C5F,0xDD0D7CC9,
0x5005713C,0x270241AA,0xBE0B1010,0xC90C2086,0x5768B525,0x206F85B3,0xB966D409,
0xCE61E49F,0x5EDEF90E,0x29D9C998,0xB0D09822,0xC7D7A8B4,0x59B33D17,0x2EB40D81,
0xB7BD5C3B,0xC0BA6CAD,0xEDB88320,0x9ABFB3B6,0x03B6E20C,0x74B1D29A,0xEAD54739,
0x9DD277AF,0x04DB2615,0x73DC1683,0xE3630B12,0x94643B84,0x0D6D6A3E,0x7A6A5AA8,
0xE40ECF0B,0x9309FF9D,0x0A00AE27,0x7D079EB1,0xF00F9344,0x8708A3D2,0x1E01F268,
0x6906C2FE,0xF762575D,0x806567CB,0x196C3671,0x6E6B06E7,0xFED41B76,0x89D32BE0,
0x10DA7A5A,0x67DD4ACC,0xF9B9DF6F,0x8EBEEFF9,0x17B7BE43,0x60B08ED5,0xD6D6A3E8,
0xA1D1937E,0x38D8C2C4,0x4FDFF252,0xD1BB67F1,0xA6BC5767,0x3FB506DD,0x48B2364B,
0xD80D2BDA,0xAF0A1B4C,0x36034AF6,0x41047A60,0xDF60EFC3,0xA867DF55,0x316E8EEF,
0x4669BE79,0xCB61B38C,0xBC66831A,0x256FD2A0,0x5268E236,0xCC0C7795,0xBB0B4703,
0x220216B9,0x5505262F,0xC5BA3BBE,0xB2BD0B28,0x2BB45A92,0x5CB36A04,0xC2D7FFA7,
0xB5D0CF31,0x2CD99E8B,0x5BDEAE1D,0x9B64C2B0,0xEC63F226,0x756AA39C,0x026D930A,
0x9C0906A9,0xEB0E363F,0x72076785,0x05005713,0x95BF4A82,0xE2B87A14,0x7BB12BAE,
0x0CB61B38,0x92D28E9B,0xE5D5BE0D,0x7CDCEFB7,0x0BDBDF21,0x86D3D2D4,0xF1D4E242,
0x68DDB3F8,0x1FDA836E,0x81BE16CD,0xF6B9265B,0x6FB077E1,0x18B74777,0x88085AE6,
0xFF0F6A70,0x66063BCA,0x11010B5C,0x8F659EFF,0xF862AE69,0x616BFFD3,0x166CCF45,
0xA00AE278,0xD70DD2EE,0x4E048354,0x3903B3C2,0xA7672661,0xD06016F7,0x4969474D,
0x3E6E77DB,0xAED16A4A,0xD9D65ADC,0x40DF0B66,0x37D83BF0,0xA9BCAE53,0xDEBB9EC5,
0x47B2CF7F,0x30B5FFE9,0xBDBDF21C,0xCABAC28A,0x53B39330,0x24B4A3A6,0xBAD03605,
0xCDD70693,0x54DE5729,0x23D967BF,0xB3667A2E,0xC4614AB8,0x5D681B02,0x2A6F2B94,
0xB40BBE37,0xC30C8EA1,0x5A05DF1B,0x2D02EF8D
};

int socketOptThreadMain(int argc UNUSED, const char **argv UNUSED)
{
    void *buf = MEMAllocFromDefaultHeapEx(SOMEMOPT_BUFFER_SIZE, 64);
    if (buf == NULL)
    {
        display("! ERROR : Socket optimizer: OUT OF MEMORY!");
        return -ENOMEM;
    }

    if (somemopt(0x01, buf, SOMEMOPT_BUFFER_SIZE, 0) == -1 && errno != 50)
    {if (!initDone) display("! ERROR : somemopt failed !");}

    MEMFreeToDefaultHeap(buf);

    return 0;
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

    #ifdef LOG2FILE    
        if (status) display("~ WARNING : Retrying transfer after = %d (%s)", socketError, strerror(socketError));
    #endif
    return status;
}


int32_t initialize_network()
{
    ACGetAssignedAddress(&hostIpAddress);

    socketOptThreadStack = MEMAllocFromDefaultHeapEx(SOCKET_MOPT_STACK_SIZE, 8);
    
    if (socketOptThreadStack == NULL || !OSCreateThread(&socketOptThread, socketOptThreadMain, 0, NULL, socketOptThreadStack + SOCKET_MOPT_STACK_SIZE, SOCKET_MOPT_STACK_SIZE, FTP_NB_SIMULTANEOUS_TRANSFERS, OS_THREAD_ATTRIB_AFFINITY_CPU0)) {
        display("! ERROR : failed to create socket memory optimization thread!");
        return -1;
    }
    OSSetThreadName(&socketOptThread, "Socket memory optimizer thread");
    OSResumeThread(&socketOptThread);

    #ifdef LOG2FILE    
        writeToLog("socketThreadMain ready");
    #endif
        
    #ifdef LOG2FILE      
        writeToLog("--------------- MAIN SETTINGS ----------------");
        writeToLog("NB_SIMULTANEOUS_TRANSFERS     = %d", NB_SIMULTANEOUS_TRANSFERS);
        writeToLog("NET_RETRY_TIME_STEP_MILLISECS = %d", NET_RETRY_TIME_STEP_MILLISECS);
        writeToLog("network retries number        = %d", retriesNumber);
        writeToLog("SOCKET_BUFFER_SIZE            = %d", SOCKET_BUFFER_SIZE);
        writeToLog("TRANSFER_BUFFER_SIZE          = %d", TRANSFER_BUFFER_SIZE);
        writeToLog("FTP_CONNECTION_TIMEOUT        = %d", FTP_CONNECTION_TIMEOUT);
        writeToLog("----------------------------------------------");
   #endif
   
   return 0;
}

void finalize_network()
{
    display("  Files received : %d", nbFilesUL);
    display("  Files sent     : %d", nbFilesDL);
    display("------------------------------------------------------------");
    
    if (socketOptThreadStack != NULL) socket_lib_finish();
    int ret;
    OSJoinThread(&socketOptThread, &ret);
    
    if (socketOptThreadStack != NULL) MEMFreeToDefaultHeap(socketOptThreadStack);

}

int32_t network_socket(uint32_t domain,uint32_t type,uint32_t protocol)
{
    
    int s = socket(domain, type, protocol);
    if(s < 0)
    {
        int err = -errno;
        return (err < 0) ? err : s;
    }
    
    if (type == SOCK_STREAM)
    {
        int enable = 1;
		
		// Reuse socket
	    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable))!=0) 
	        {if (!initDone) display("! ERROR : setsockopt / SO_REUSEADDR failed !");}        

        // SO_NOSLOWSTART
        if (setsockopt(s, SOL_SOCKET, SO_NOSLOWSTART, &enable, sizeof(enable))!=0) 
            {if (!initDone) display("! ERROR : setsockopt / Disable slow start feature failed !");}    
        
        // Activate TCP SAck
        if (setsockopt(s, SOL_SOCKET, SO_TCPSACK, &enable, sizeof(enable))!=0) 
            {if (!initDone) display("! ERROR : setsockopt / TCP SAck activation failed !");}
        
        // Disabling Nagle's algorithm
        if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable))!=0) 
            {if (!initDone) display("! ERROR : setsockopt / Disabling Nagle's algorithm failed !");}

        int disable = 0;
        
        // Enable delayed ACKs
        if (setsockopt(s, IPPROTO_TCP, TCP_NOACKDELAY, &disable, sizeof(disable))!=0)
            {if (!initDone) display("! ERROR : setsockopt / Enabling delayed ACKs failed !");}
        
        // Activate WinScale
        if (setsockopt(s, SOL_SOCKET, SO_WINSCALE, &enable, sizeof(enable))!=0) 
            {display("! ERROR : setsockopt / winScale activation failed !");}
        
        // socket memory optimization
        int nbTries=0;
        while (1)
        {
            if (setsockopt(s, SOL_SOCKET, SO_RUSRBUF, &enable, sizeof(enable))==0)
                break;

            OSSleepTicks(OSMillisecondsToTicks(NET_RETRY_TIME_STEP_MILLISECS));
            if (++nbTries > retriesNumber)
            {
                if (!initDone) display("! ERROR : setsockopt / Socket memory optimization failed !");
                break;
            }
        }
        
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


int32_t network_bind(int32_t s,struct sockaddr *name,int32_t namelen)
{
    int res = bind(s, name, namelen);
    if (res < 0)
    {
        int err = -errno;
        return (err < 0) ? err : res;
    }
    return res;
}

int32_t network_listen(int32_t s,uint32_t backlog)
{
    
    int res = listen(s, backlog);
    if (res < 0)
    {
        int err = -errno;
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
        int err = -errno;
        return (err < 0) ? err : res;
    }
    
    
    return res;
}

int32_t network_connect(int32_t s,struct sockaddr *addr, int32_t addrlen)
{
    
    int res = connect(s, addr, addrlen);
    if (res < 0)
    {
        int err = -errno;
        return (err < 0) ? err : res;
    }
    return res;
}

int32_t network_read(int32_t s,void *mem,int32_t len)
{
    int res = recv(s, mem, len, 0);
    if (res < 0)
    {
        int err = -errno;
        return (err < 0) ? err : res;
    }
    return res;
}

static int32_t network_readChunk(int32_t s, void *mem, int32_t len) {

    int32_t received = 0;
    int ret = -1;
    
    // while buffer is not full (len>0)
    while (len>0)
    {
        // max ret value is 2*SOCKET_BUFFER_SIZE
        ret = recv(s, mem, len, 0);        
        if (ret == 0) {
            // client EOF detected
            break;
        } else if (ret < 0 && errno != EAGAIN) {
            int err = -errno;
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

uint32_t network_gethostip()
{
    return hostIpAddress;
}

int32_t network_write(int32_t s, const void *mem, int32_t len)
{
    int32_t transferred = 0;
    
    while (len)
    {
        int ret = send(s, mem, len, 0);
        if (ret < 0 && errno != EAGAIN)
        {
            int err = -errno;
            transferred = (err < 0) ? err : ret;
            break;
        } else {
            if (ret > 0) {
                mem += ret;
                transferred += ret;
                len -= ret;
            }
        }
    }
    return transferred;
}

int32_t network_close(int32_t s)
{
    if (s < 0)
        return -1;
    return close(s);
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
                OSSleepTicks(OSMillisecondsToTicks(NET_RETRY_TIME_STEP_MILLISECS));
                retryNumber++;
                if (retryNumber <= retriesNumber) goto retry;
            }
            
    #ifdef LOG2FILE 
            display("! ERROR : network_write failed = %d afer %d attempts", bytes_transferred, retriesNumber);
            display("! ERROR : errno = %d (%s)", errno, strerror(errno));            
    #endif
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

static unsigned long getCrc32(unsigned long inCrc32, const void *buf, size_t bufLen) {
    
    unsigned long crc32;
    unsigned char *byteBuf;
    size_t i;

    // accumulate crc32 for buffer
    crc32 = inCrc32 ^ 0xFFFFFFFF;
    byteBuf = (unsigned char*) buf;
    for (i = 0; i < bufLen; i++) {
        crc32 = (crc32 >> 8) ^ crcTable[(crc32 ^ byteBuf[i]) & 0xFF];
    }
    return crc32 ^ 0xFFFFFFFF;
}


int32_t send_from_file(int32_t s, connection_t* connection) {
    // return code
    int32_t result = 0;
    
    // CRC-32 value
    if (calculateCrc32) connection->crc32 = getCrc32(0, NULL, 0);

    #ifdef LOG2FILE    
        writeToLog("C[%d] sending %d bytes of %s on socket %d", connection->index+1, 2*SOCKET_BUFFER_SIZE, connection->fileName,s);
    #endif

    // max value for SNDBUF = SOCKET_BUFFER_SIZE (the system double the value set)
    int sndBuffSize = SOCKET_BUFFER_SIZE;
    if (setsockopt(s, SOL_SOCKET, SO_SNDBUF, &sndBuffSize, sizeof(sndBuffSize))!=0)
        {display("! ERROR : setsockopt / SNDBUF failed !");
    }

    int32_t downloadBufferSize = TRANSFER_CHUNK_SIZE*4;

    // if less than 4 transfers are running, sleep just an instant to let other connections start (only 3 cores are available)
    int nbt = getActiveTransfersNumber();
    if ( nbt < 4 ) OSSleepTicks(OSMillisecondsToTicks(40));    
    
    bool prioLowered = false;
    int32_t bytes_read = downloadBufferSize;        
	while (bytes_read) {

        bytes_read = fread(connection->transferBuffer, 1, downloadBufferSize, connection->f);
        if (bytes_read == 0) {
            // SUCCESS, no more to write                  
            nbFilesDL++;
            result = 0;
            break;
        }        
        if (bytes_read > 0) {
            
            // after the first chunk received, lower the priority to 2*NB_SIMULTANEOUS_TRANSFERS
            if (!prioLowered) {
                OSSetThreadPriority(&connection->transferThread, 2*NB_SIMULTANEOUS_TRANSFERS);
                prioLowered = true;
            }
        
            uint32_t retryNumber = 0;
            int32_t remaining = bytes_read;            

            // Let buffer on file be larger than socket one for checking performances scenarii 
            while (remaining) {
                
                send_again:
                result = network_write(s, connection->transferBuffer, MIN(remaining, downloadBufferSize));

                #ifdef LOG2FILE    
                    writeToLog("C[%d] sent %d bytes of %s", connection->index+1, result, connection->fileName);
                #endif
                
                if (result < 0) {
                    if (retry(result)) {
                        OSSleepTicks(OSMillisecondsToTicks(NET_RETRY_TIME_STEP_MILLISECS));
                        retryNumber++;
                        if (retryNumber <= retriesNumber) goto send_again;
                    }            
                    // result = error, connection will be closed
                    break;
                } else {
                    // add buffer contribution to the the CRC32 computation
                    if (calculateCrc32) connection->crc32 = getCrc32(connection->crc32, connection->transferBuffer, bytes_read);            

                    // data block sent sucessfully, continue
                    connection->dataTransferOffset += result;
                    connection->bytesTransferred = result;
                    remaining -= result;
                }
            }        
		}
        if (result >=0) {
                
            // check bytes read (now because on the last sending, data is already sent here = result)
            if (bytes_read < downloadBufferSize) {
                    
            	if (bytes_read < 0 || feof(connection->f) == 0 || ferror(connection->f) != 0) {
                    // ERROR : not on eof file or read error, or error on stream => ERROR
                    display("! ERROR : failed to read file!");
                    display("! ERROR : fread = %d and bytes = %d", bytes_read, TRANSFER_BUFFER_SIZE);
                    display("! ERROR : errno = %d (%s)", errno, strerror(errno)); 
                    result = -103;
                    break;
                }
            }
            
            // result = 0 and EOF
            if ((feof(connection->f) != 0) && (result == 0)) {
                // SUCESS : eof file, last data bloc sent
                nbFilesDL++;
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
    // CRC-32 value
    if (calculateCrc32) connection->crc32 = getCrc32(0, NULL, 0);
    
    // (the system double the value set)
    int rcvBuffSize = SOCKET_BUFFER_SIZE;
    if (setsockopt(s, SOL_SOCKET, SO_RCVBUF, &rcvBuffSize, sizeof(rcvBuffSize))!=0)
        {display("! ERROR : setsockopt / RCVBUF failed !");}
    
    #ifdef LOG2FILE    
        writeToLog("C[%d] receiving %s on socket %d", connection->index+1, connection->fileName, s);
    #endif

    // network_readChunk() overflow cannot exceed 2*SOCKET_BUFFER_SIZE bytes after setsockopt(RCVBUF)
    // use a buffer with twice the size to handle the bytes overflow
    int32_t uploadBufferSize = TRANSFER_BUFFER_SIZE - (2*SOCKET_BUFFER_SIZE);
    
    // if less than 4 transfers are running, lower the priority to let other connections start (only 3 cores are available)
    bool prioLowered = false;
    int nbt = getActiveTransfersNumber();
    if ( nbt < 4 ) {
        // lower the priority to 2*NB_SIMULTANEOUS_TRANSFERS
        OSSetThreadPriority(&connection->transferThread, 2*NB_SIMULTANEOUS_TRANSFERS);  
        prioLowered = true;
    }
    
    uint32_t retryNumber = 0;

    int32_t bytes_received = uploadBufferSize;
    while (bytes_received) {
                        
        read_again:
        bytes_received = network_readChunk(s, connection->transferBuffer, uploadBufferSize);
        if (bytes_received == 0) {
            // SUCCESS, no more to write to file
            nbFilesUL++;
            result = 0;
                        
        } else if (bytes_received < 0) {

            if (retry(bytes_received)) {
                OSSleepTicks(OSMillisecondsToTicks(NET_RETRY_TIME_STEP_MILLISECS));
                retryNumber++;
                if (retryNumber <= retriesNumber) goto read_again;
            }    
            display("! ERROR : network_read failed = %d afer %d attempts", bytes_received, retriesNumber);
            display("! ERROR : errno = %d (%s)", errno, strerror(errno));
            // result = error, connection will be closed
            result = bytes_received;
            break;
        } else {
            // bytes_received > 0

            if (!prioLowered) {
                OSSetThreadPriority(&connection->transferThread, 2*NB_SIMULTANEOUS_TRANSFERS);
                prioLowered = true;
            }
            
            // write bytes_received to f
            result = fwrite(connection->transferBuffer, 1, bytes_received, connection->f);
            
            if ((result < 0 && result < bytes_received) || ferror(connection->f) != 0) {
                // error when writing f
                display("! ERROR : failed to write file!");
                display("! ERROR : fwrite = %d and bytes=%d", result, bytes_received);
                display("! ERROR : errno = %d (%s)", errno, strerror(errno));
                result = -100;
                break;
            } else {
                // add buffer contribution to the the CRC32 computation
                if (calculateCrc32) {
                    connection->crc32 = getCrc32(connection->crc32, connection->transferBuffer, bytes_received);            
                }
                connection->dataTransferOffset += result;
                connection->bytesTransferred = result;                
            }
        }
    }
    
	connection->bytesTransferred = result;
      
    return result;
}

uint32_t getNbFilesTransferred() {
    return nbFilesDL + nbFilesUL;
}