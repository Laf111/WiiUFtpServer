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
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dir.h>
#include <sys/param.h>
#include <unistd.h>
#include "common/common.h"
#include "dynamic_libs/os_functions.h"
#include "dynamic_libs/socket_functions.h"
#include "iosuhax/iosuhax.h"

#define errno                   geterrno()

#include "ftp.h"
#include "virtualpath.h"
#include "net.h"
#include "vrt.h"

#define UNUSED    __attribute__((unused))

#define FTP_MSG_BUFFER_SIZE MIN_NET_BUFFER_SIZE/4

#define FTP_STACK_SIZE 0x2000

extern void display(const char *format, ...);

static const u16 SRC_PORT = 20;
static const s32 EQUIT = 696969;
static const char *CRLF = "\r\n";
static const u32 CRLF_LENGTH = 2;

static u32 nbConnections = 0;
static char clientIp[15]="";
static u16 passive_port = 1024;
static char *password = NULL;

// OS time computed in main
static struct tm *timeOs=NULL;
// OS time computed in main
static time_t tsOs=0;

// IOSUHAX fd
static int fsaFd = -1;

typedef s32 (*data_connection_callback)(s32 data_socket, void *arg);

struct client_struct {
    s32 socket;
    char representation_type;
    s32 passive_socket;
    s32 data_socket;
    char cwd[MAXPATHLEN];
    char pending_rename[MAXPATHLEN];
    off_t restart_marker;
    struct sockaddr_in address;
    bool authenticated;
    char buf[FTP_MSG_BUFFER_SIZE];
    s32 offset;
    bool data_connection_connected;
    data_connection_callback data_callback;
    void *data_connection_callback_arg;
    void (*data_connection_cleanup)(void *arg);
    u64 data_connection_timer;
};

typedef struct client_struct client_t;

static client_t *clients[NB_SIMULTANEOUS_CONNECTIONS] = { NULL };
static s32 listener=-1;     // listening socket descriptor


// FTP thread on CPU2
static OSThread *ftpThread=NULL;
static u32 *ftpThreadStack;

static int ftpThreadMain(int argc, void *argv)
{
    s32 socket = network_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (socket < 0)
        display("! ERROR : network_socket failed and return %d", socket);        

    // Set to non-blocking I/O 
    set_blocking(socket, false);
    
    u32 enable = 1;
    setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
    
    return socket;
}

s32 create_server(u16 port) {

    // create a thread on CPU1
    ftpThread = OSAllocFromSystem(sizeof(OSThread), 8); 
    if (ftpThread == NULL) {
        display("! ERROR : when allocating ftpThread!");        
        return -1;
    }
     
    ftpThreadStack = OSAllocFromSystem(FTP_STACK_SIZE, 8);
    if (ftpThreadStack == NULL) {
        display("! ERROR : when allocating ftpThreadStack!");        
        return -1;
    }
        
    // on CPU2
    if (!OSCreateThread(ftpThread, ftpThreadMain, 0, NULL, (u32)ftpThreadStack + FTP_STACK_SIZE, FTP_STACK_SIZE, 1, OS_THREAD_ATTR_AFFINITY_CORE2)) {
        display("! ERROR : when creating ftpThread!");        
        return -1;
    }
    // set name    
    OSSetThreadName(ftpThread, "FTP and network thread on CPU2");
    // launch thread
    OSResumeThread(ftpThread);
            
	OSJoinThread(ftpThread, &listener);
    
    // check that the listener is created
    if ( listener < 0 ) {
        display("! ERROR: listener not created!");
    }
    
    struct sockaddr_in bindAddress;
    memset(&bindAddress, 0, sizeof(bindAddress));
    bindAddress.sin_family = AF_INET;
    bindAddress.sin_port = htons(port);
    bindAddress.sin_addr.s_addr = htonl(INADDR_ANY);

    s32 ret;
    if ((ret = network_bind(listener, (struct sockaddr *)&bindAddress, sizeof(bindAddress))) < 0) {
        network_close(listener);
        return ret;
    }
    
    if ((ret = network_listen(listener, NB_SIMULTANEOUS_CONNECTIONS)) < 0) {
        network_close(listener);
        return ret;
    }
    
    uint32_t ip = network_gethostip();     
    display(" ");
    display("Listening on %u.%u.%u.%u:%i", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF, port);
    
    return listener;
}

// vPath : /storage_usb/path/saveinfo.xml -> vlPath : /vol/storage_usb01/path/saveinfo.xml
// This function allocate the memory returned.
// The caller must take care of freeing it
static char* virtualToVolPath(char *vPath) {

    if (vPath) {
        if (strcmp(vPath,"/") ==0) return "/vol";

        int dimv=strlen(vPath);
        int dimm=dimv+6+1;
        
        // output
        char *vlPath = NULL;
        
        // allocate vlPath
        vlPath=(char *) malloc(sizeof(char)*dimm);
        if (!vlPath) {
            display("! ERROR : When allocation vlPath");
        } else {
        
            char volume[30]="";
            if (strncmp(strchr(vPath, '_'), "_usb", 4) == 0) {
                strcpy(volume,"/vol/storage_usb01");
            } else if (strncmp(strchr(vPath, '_'), "_mlc", 4) == 0) {
                strcpy(volume,"/vol/storage_mlc01");
            } else if (strncmp(strchr(vPath, '_'), "_slccmpt", 8) == 0) {
                strcpy(volume,"/vol/storage_slccmpt01");
            } else if (strncmp(strchr(vPath, '_'), "_odd_tickets", 12) == 0) {
                strcpy(volume,"/vol/storage_odd_tickets");
            } else if (strncmp(strchr(vPath, '_'), "_odd_updates", 12) == 0) {
                strcpy(volume,"/vol/storage_odd_updates");
            } else if (strncmp(strchr(vPath, '_'), "_odd_content", 12) == 0) {
                strcpy(volume,"/vol/storage_odd_content");
            } else if (strncmp(strchr(vPath, '_'), "_odd_content2", 13) == 0) {
                strcpy(volume,"/vol/storage_odd_content2");
            } else if (strncmp(strchr(vPath, '_'), "_sdcard", 7) == 0) {
                strcpy(volume,"/vol/storage_sdcard");
            } else if (strncmp(strchr(vPath, '_'), "_slc", 4) == 0) {
                strcpy(volume,"/vol/system");
            } else {
                display("! ERROR : No volume found for %s", vPath);
            }
            
            strcpy(vlPath, volume);
            
            char str[dimm];            
            char *token="";
            strcpy(str,vPath);
            
            token=strtok(str, "/");
            if (token != NULL) {
                token=strtok(NULL, "/");
                while (token != NULL) {            
                    strcat(vlPath, "/");
                    strcat(vlPath, token);
                    token=strtok(NULL, "/");
                }
            }
            if (vPath[dimv-1] == '/') strcat(vlPath, "/");
            return vlPath;
            
        }        
    }

    return "";
}

// transform a date from J1980 with microseconds to J1970 (UNIX epoch)
// GetStat can return 0 with faulty time stamp ! -> add check on time stamp value
static struct tm getDateEpoch(time_t ts1980) {
    // output 
    struct tm *time=NULL;
    // initialized with timeOs
    if (timeOs == NULL) return *(localtime(NULL));

    if (ts1980 == 0) return (*timeOs);
    time=timeOs;
    
    // display("DEBUG : ts1980 = %" PRId64, ts1980);
    
    // check ts1980 value (1980 epoch with micro seconds)
    // 01/01/2010 00:00:00 -> 946771200,  with us = 946771200000000
    // 01/01/2038 00:00:00 -> 1830384000, with us = 1830384000000000
    time_t tsMax = tsOs + 86400*2;
    
    if ( ts1980 <= 946771200000000 || ts1980 >= tsMax) return *time;
    
    // compute the timestamp in J1970 Epoch (1972 and 1976 got 366 days)
    time_t ts1970 = ts1980/1000000 + 86400*((366*2) + (365*8));
    
    // get the corresponding tm
    time=localtime(&ts1970);
    
    return (*time);
    
}    
static void set_ftp_password(char *new_password) {
    if (password) free(password);
    if (new_password) {
        password = malloc(strlen(new_password) + 1);
        if (!password)
            return;

        strcpy((char *)password, new_password);
    } else {
        password = NULL;
    }
}

static bool compare_ftp_password(char *password_attempt) {
    return !password || !strcmp((char *)password, password_attempt);
}

/*
    TODO: support multi-line reply
*/
static s32 write_reply(client_t *client, u16 code, char *msg) {
    u32 msglen = 4 + strlen(msg) + CRLF_LENGTH;
    char msgbuf[msglen + 1];
    if (msgbuf == NULL) return -ENOMEM;
    sprintf(msgbuf, "%u %s\r\n", code, msg);
    display("> %s", msgbuf);
    return send_exact(client->socket, msgbuf, msglen);
}

static void close_passive_socket(client_t *client) {
    if (client->passive_socket >= 0) {
        network_close_blocking(client->passive_socket);
        client->passive_socket = -1;
    }
}

/*
    result must be able to hold up to maxsplit+1 null-terminated strings of length strlen(s)
    returns the number of strings stored in the result array (up to maxsplit+1)
*/
static u32 split(char *s, char sep, u32 maxsplit, char *result[]) {
    u32 num_results = 0;
    u32 result_pos = 0;
    u32 trim_pos = 0;
    bool in_word = false;
    for (; *s; s++) {
        if (*s == sep) {
            if (num_results <= maxsplit) {
                in_word = false;
                continue;
            } else if (!trim_pos) {
                trim_pos = result_pos;
            }
        } else if (trim_pos) {
            trim_pos = 0;
        }
        if (!in_word) {
            in_word = true;
            if (num_results <= maxsplit) {
                num_results++;
                result_pos = 0;
            }
        }
        result[num_results - 1][result_pos++] = *s;
        result[num_results - 1][result_pos] = '\0';
    }
    if (trim_pos) {
        result[num_results - 1][trim_pos] = '\0';
    }
    u32 i = num_results;
    for (i = num_results; i <= maxsplit; i++) {
        result[i][0] = '\0';
    }
    return num_results;
}

static s32 ftp_USER(client_t *client, char *username UNUSED) {
    return write_reply(client, 331, "User name okay, need password.");
}

static s32 ftp_PASS(client_t *client, char *password_attempt) {
    if (compare_ftp_password(password_attempt)) {
        client->authenticated = true;
        return write_reply(client, 230, "User logged in, proceed.");
    } else {
        return write_reply(client, 530, "Login incorrect.");
    }
}

static s32 ftp_REIN(client_t *client, char *rest UNUSED) {
    close_passive_socket(client);
    strcpy(client->cwd, "/");
    client->representation_type = 'A';
    client->authenticated = false;
    return write_reply(client, 220, "Service ready for new user.");
}

static s32 ftp_QUIT(client_t *client, char *rest UNUSED) {
    // TODO: dont quit if xfer in progress
    s32 result = write_reply(client, 221, "Service closing control connection.");
    return result < 0 ? result : -EQUIT;
}

static s32 ftp_SYST(client_t *client, char *rest UNUSED) {
    return write_reply(client, 215, "UNIX Type: L8 Version: WiiUFtpServer");
}

static s32 ftp_TYPE(client_t *client, char *rest) {
    char representation_type[FTP_MSG_BUFFER_SIZE], param[FTP_MSG_BUFFER_SIZE];
    char *args[] = { representation_type, param };
    u32 num_args = split(rest, ' ', 1, args);
    if (num_args == 0) {
        return write_reply(client, 501, "Syntax error in parameters.");
    } else if ((!strcasecmp("A", representation_type) && (!*param || !strcasecmp("N", param))) ||
               (!strcasecmp("I", representation_type) && num_args == 1)) {
        client->representation_type = *representation_type;
    } else {
        return write_reply(client, 501, "Syntax error in parameters.");
    }
    char msg[FTP_MSG_BUFFER_SIZE+21] = "";
    sprintf(msg, "Type set to %s.", representation_type);
    return write_reply(client, 200, msg);
}

static s32 ftp_MODE(client_t *client, char *rest) {
    if (!strcasecmp("S", rest)) {
        return write_reply(client, 200, "Mode S ok.");
    } else {
        return write_reply(client, 501, "Syntax error in parameters.");
    }
}

static s32 ftp_PWD(client_t *client, char *rest UNUSED) {
    char msg[MAXPATHLEN + 24] = "";
    // TODO: escape double-quotes
    sprintf(msg, "\"%s\" is current directory.", client->cwd);
    return write_reply(client, 257, msg);
}

static s32 ftp_CWD(client_t *client, char *path) {
    s32 result;
    if (!vrt_chdir(client->cwd, path)) {
        result = write_reply(client, 250, "CWD command successful.");
    } else  {
        result = write_reply(client, 550, strerror(errno));
    }
    return result;
}

static s32 ftp_CDUP(client_t *client, char *rest UNUSED) {
    s32 result;
    if (!vrt_chdir(client->cwd, "..")) {
        result = write_reply(client, 250, "CDUP command successful.");
    } else  {
        result = write_reply(client, 550, strerror(errno));
    }
    return result;
}

static s32 ftp_DELE(client_t *client, char *path) {
    if (!vrt_unlink(client->cwd, path)) {
        return write_reply(client, 250, "File or directory removed.");
    } else {
        return write_reply(client, 550, strerror(errno));
    }
}

static s32 ftp_MKD(client_t *client, char *path) {
    if (!*path) {
        return write_reply(client, 501, "Syntax error in parameters.");
    }
    if (!vrt_mkdir(client->cwd, path, 0777)) {
        char msg[MAXPATHLEN + 21] = "";
        char abspath[MAXPATHLEN] = "";
        strcpy(abspath, client->cwd);
        vrt_chdir(abspath, path); // TODO: error checking
        // TODO: escape double-quotes
        sprintf(msg, "\"%s\" directory created.", abspath);
        
        // compute virtual path /usb/... in a string allocate on the stack
        char vPath[MAXPATHLEN+1] = "";
        if (path) sprintf(vPath, "%s%s", client->cwd, path);
        else sprintf(vPath, "%s", client->cwd);
        
        char *volPath = NULL;
        volPath = virtualToVolPath(vPath); 
    
        // chmod on folder
        IOSUHAX_FSA_ChangeMode(fsaFd, volPath, 0x666);        
        free(volPath);
        
        return write_reply(client, 257, msg);
    } else {
        return write_reply(client, 550, strerror(errno));
    }
}

static s32 ftp_RNFR(client_t *client, char *path) {
    strcpy(client->pending_rename, path);
    return write_reply(client, 350, "Ready for RNTO.");
}

static s32 ftp_RNTO(client_t *client, char *path) {
    if (!*client->pending_rename) {
        return write_reply(client, 503, "RNFR required first.");
    }
    s32 result;
    if (!vrt_rename(client->cwd, client->pending_rename, path)) {
        result = write_reply(client, 250, "Rename successful.");
    } else {
        result = write_reply(client, 550, strerror(errno));
    }
    *client->pending_rename = '\0';
    return result;
}

static s32 ftp_SIZE(client_t *client, char *path) {
    struct stat st;
    if (!vrt_stat(client->cwd, path, &st)) {
        char size_buf[12] = "";
        sprintf(size_buf, "%llu", st.st_size);
        return write_reply(client, 213, size_buf);
    } else {
        return write_reply(client, 550, strerror(errno));
    }
}

static s32 ftp_PASV(client_t *client, char *rest UNUSED) {
    close_passive_socket(client);
    client->passive_socket = network_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (client->passive_socket < 0) {
        return write_reply(client, 520, "Unable to create listening socket.");
    }
    set_blocking(client->passive_socket, false);
    struct sockaddr_in bindAddress;
    memset(&bindAddress, 0, sizeof(bindAddress));
    bindAddress.sin_family = AF_INET;
    bindAddress.sin_port = htons(passive_port++); // XXX: BUG: This will overflow eventually, with interesting results...
    bindAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    s32 result;
    if ((result = network_bind(client->passive_socket, (struct sockaddr *)&bindAddress, sizeof(bindAddress))) < 0) {
        close_passive_socket(client);
        return write_reply(client, 520, "Unable to bind listening socket.");
    }
    if ((result = network_listen(client->passive_socket, 1)) < 0) {
        close_passive_socket(client);
        return write_reply(client, 520, "Unable to listen on socket.");
    }
    char reply[49+2] = "";
    u16 port = bindAddress.sin_port;
    u32 ip = network_gethostip();
    struct in_addr addr;
    addr.s_addr = ip;
    display("  Listening for data connections at %s:%u...\n", inet_ntoa(addr), port);
    sprintf(reply, "  Entering Passive Mode (%u,%u,%u,%u,%u,%u).", (ip >> 24) & 0xff, (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff, (port >> 8) & 0xff, port & 0xff);
    return write_reply(client, 227, reply);
}

static s32 ftp_PORT(client_t *client, char *portspec) {
    u32 h1, h2, h3, h4, p1, p2;
    if (sscanf(portspec, "%3u,%3u,%3u,%3u,%3u,%3u", &h1, &h2, &h3, &h4, &p1, &p2) < 6) {
        return write_reply(client, 501, "Syntax error in parameters.");
    }
    char addr_str[44];
    sprintf(addr_str, "%u.%u.%u.%u", h1, h2, h3, h4);
    struct in_addr sin_addr;
    if (!inet_aton(addr_str, &sin_addr)) {
        return write_reply(client, 501, "Syntax error in parameters.");
    }
    close_passive_socket(client);
    u16 port = ((p1 &0xff) << 8) | (p2 & 0xff);
    client->address.sin_addr = sin_addr;
    client->address.sin_port = htons(port);
    display("  Set client address to %s:%u\n", addr_str, port);
    return write_reply(client, 200, "PORT command successful.");
}

typedef s32 (*data_connection_handler)(client_t *client, data_connection_callback callback, void *arg);

static s32 prepare_data_connection_active(client_t *client, data_connection_callback callback UNUSED, void *arg UNUSED) {
    s32 data_socket = network_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (data_socket < 0) return data_socket;
    set_blocking(data_socket, false);
    struct sockaddr_in bindAddress;
    memset(&bindAddress, 0, sizeof(bindAddress));
    bindAddress.sin_family = AF_INET;
    bindAddress.sin_port = htons(SRC_PORT);
    bindAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    s32 result;
    if ((result = network_bind(data_socket, (struct sockaddr *)&bindAddress, sizeof(bindAddress))) < 0) {
        network_close(data_socket);
        return result;
    }

    client->data_socket = data_socket;
    display("  Attempting to connect to client at %s:%u\n", inet_ntoa(client->address.sin_addr), client->address.sin_port);
    return 0;
}

static s32 prepare_data_connection_passive(client_t *client, data_connection_callback callback UNUSED, void *arg UNUSED) {
    client->data_socket = client->passive_socket;
    display("  Waiting for data connections...\n");
    return 0;
}

static s32 prepare_data_connection(client_t *client, void *callback, void *arg, void *cleanup) {
    s32 result = write_reply(client, 150, "Transferring data.");
    if (result >= 0) {
        data_connection_handler handler = prepare_data_connection_active;
        if (client->passive_socket >= 0) handler = prepare_data_connection_passive;
        result = handler(client, (data_connection_callback)callback, arg);
        if (result < 0) {
            result = write_reply(client, 520, "Closing data connection, error occurred during transfer.");
        } else {
            client->data_connection_connected = false;
            client->data_callback = callback;
            client->data_connection_callback_arg = arg;
            client->data_connection_cleanup = cleanup;
            client->data_connection_timer = OSGetTick() + SECS_TO_TICKS(10);
        }
    }
    return result;
}

static s32 send_nlst(s32 data_socket, DIR_P *iter) {
    s32 result = 0;
    char filename[MAXPATHLEN] = "";
    struct dirent *dirent = NULL;
    while ((dirent = vrt_readdir(iter)) != 0) {
        size_t end_index = strlen(dirent->d_name);
        if(end_index + 2 >= MAXPATHLEN)
            continue;
        strcpy(filename, dirent->d_name);
        filename[end_index] = CRLF[0];
        filename[end_index + 1] = CRLF[1];
        filename[end_index + 2] = '\0';
        if ((result = send_exact(data_socket, filename, strlen(filename))) < 0) {
            break;
        }
    }
    return result < 0 ? result : 0;
}

static s32 send_list(s32 data_socket, DIR_P *iter) {
    s32 result = 0;
    char filename[MAXPATHLEN] = "";
    char line[MAXPATHLEN + 56 + CRLF_LENGTH + 1];
    struct dirent *dirent = NULL;
    while ((dirent = vrt_readdir(iter)) != 0) {

        snprintf(filename, sizeof(filename), "%s/%s", iter->path, dirent->d_name);
        struct stat st;
        stat(filename, &st);
        // compute date to display : transform a date from J1980 with microseconds to J1970 (UNIX epoch)
        struct tm timeinfo = getDateEpoch((time_t)st.st_mtime);
        
        // dim = 13
        char timestamp[13]="";
        strftime(timestamp, sizeof(timestamp), "%b %d  %Y", &timeinfo);                        
        snprintf(line, sizeof(line), "%crwxr-xr-x    1 0        0     %10llu %s %s\r\n", (dirent->d_type & DT_DIR) ? 'd' : '-', st.st_size, timestamp, dirent->d_name);
        if ((result = send_exact(data_socket, line, strlen(line))) < 0) {
            break;
        }
    }
    return result < 0 ? result : 0;
}

static s32 ftp_NLST(client_t *client, char *path) {
    if (!*path) {
        path = ".";
    }

    DIR_P *dir = vrt_opendir(client->cwd, path);
    if (dir == NULL) {
        return write_reply(client, 550, strerror(errno));
    }

    s32 result = prepare_data_connection(client, send_nlst, dir, vrt_closedir);
    if (result < 0) vrt_closedir(dir);
    return result;
}

static s32 ftp_LIST(client_t *client, char *path) {
    char rest[FTP_MSG_BUFFER_SIZE] = "";
    if (*path == '-') {
        // handle buggy clients that use "LIST -aL" or similar, at the expense of breaking paths that begin with '-'
        char flags[FTP_MSG_BUFFER_SIZE];
        char *args[] = { flags, rest };
        split(path, ' ', 1, args);
        path = rest;
    }
    if (!*path) {
        path = ".";
    }

      if(path && client->cwd) if(strcmp(path, ".") == 0 && strcmp(client->cwd, "/") == 0) ResetVirtualPaths(); 

    DIR_P *dir = vrt_opendir(client->cwd, path);
    if (dir == NULL) {
        return write_reply(client, 550, strerror(errno));
    }

    s32 result = prepare_data_connection(client, send_list, dir, vrt_closedir);
    if (result < 0) vrt_closedir(dir);
    return result;
}

static s32 ftp_RETR(client_t *client, char *path) {
    FILE *f = vrt_fopen(client->cwd, path, "rb");
    if (!f) {
        return write_reply(client, 550, strerror(errno));
    }

    int fd = fileno(f);
    if (client->restart_marker && lseek(fd, client->restart_marker, SEEK_SET) != client->restart_marker) {
        s32 lseek_error = errno;
        fclose(f);
        client->restart_marker = 0;
        return write_reply(client, 550, strerror(lseek_error));
    }
    client->restart_marker = 0;

    s32 result = prepare_data_connection(client, send_from_file, f, fclose);
    if (result < 0) fclose(f);
    return result;
}

static s32 stor_or_append(client_t *client, char *path, FILE *f) {
    if (!f) {
        return write_reply(client, 550, strerror(errno));
    }
    
    // compute virtual path /usb/... in a string allocate on the stack
    char vPath[MAXPATHLEN+1] = "";
    if (path) sprintf(vPath, "%s%s", client->cwd, path);
    else sprintf(vPath, "%s", client->cwd);
    
    // allocate compute and store the volume path (/vol/storage_usb01) 
    // needed for IOSUHAX operations
    char *volPath = NULL;
    volPath = virtualToVolPath(vPath);    
    SetVolPath(volPath, fileno(f));
    free(volPath);
    s32 result = prepare_data_connection(client, recv_to_file, f, fclose);
    if (result < 0) fclose(f);
    return result;
}

static s32 ftp_STOR(client_t *client, char *path) {
    FILE *f = vrt_fopen(client->cwd, path, "wb");
    int fd;
    if (f) fd = fileno(f);
    if (f && client->restart_marker && lseek(fd, client->restart_marker, SEEK_SET) != client->restart_marker) {
        s32 lseek_error = errno;
        fclose(f);
        client->restart_marker = 0;
        return write_reply(client, 550, strerror(lseek_error));
    }
    client->restart_marker = 0;

    return stor_or_append(client, path, f);
}

static s32 ftp_APPE(client_t *client, char *path) {
    return stor_or_append(client, path, vrt_fopen(client->cwd, path, "ab"));
}

static s32 ftp_REST(client_t *client, char *offset_str) {
    off_t offset;
    if (sscanf(offset_str, "%lli", &offset) < 1 || offset < 0) {
        return write_reply(client, 501, "Syntax error in parameters.");
    }
    client->restart_marker = offset;
    char msg[FTP_MSG_BUFFER_SIZE];
    sprintf(msg, "Restart position accepted (%lli).", offset);
    return write_reply(client, 350, msg);
}

static s32 ftp_SITE_LOADER(client_t *client, char *rest UNUSED) {
    s32 result = write_reply(client, 200, "Exiting to loader.");
    //set_reset_flag();
    return result;
}

static s32 ftp_SITE_CLEAR(client_t *client, char *rest UNUSED) {
    s32 result = write_reply(client, 200, "Cleared.");
    u32 i;
    for (i = 0; i < 18; i++) display("\n");
    //display("\x1b[2;0H");
    return result;
}

/*
    This is implemented as a no-op to prevent some FTP clients
    from displaying skip/abort/retry type prompts.
*/
static s32 ftp_SITE_CHMOD(client_t *client, char *rest UNUSED) {
    // compute virtual path /usb/... in a string allocate on the stack
    char vPath[MAXPATHLEN+1] = "";
    sprintf(vPath, "%s", client->cwd);
    
    char *volPath = NULL;
    volPath = virtualToVolPath(vPath); 

    // chmod on folder
    IOSUHAX_FSA_ChangeMode(fsaFd, volPath, 0x666);        
    free(volPath);
    
    return write_reply(client, 250, "SITE CHMOD command ok.");
}

static s32 ftp_SITE_PASSWD(client_t *client, char *new_password) {
    set_ftp_password(new_password);
    return write_reply(client, 200, "Password changed.");
}

static s32 ftp_SITE_NOPASSWD(client_t *client, char *rest UNUSED) {
    set_ftp_password(NULL);
    return write_reply(client, 200, "Authentication disabled.");
}

static s32 ftp_SITE_EJECT(client_t *client, char *rest UNUSED) {
    //if (dvd_eject()) return write_reply(client, 550, "Unable to eject DVD.");
    return write_reply(client, 200, "DVD ejected.");
}

static s32 ftp_SITE_MOUNT(client_t *client, char *path UNUSED) {
    //if (!mount_virtual(path)) return write_reply(client, 550, "Unable to mount.");
    return write_reply(client, 250, "Mounted.");
}

static s32 ftp_SITE_UNMOUNT(client_t *client, char *path UNUSED) {
    //if (!unmount_virtual(path)) return write_reply(client, 550, "Unable to unmount.");
    return write_reply(client, 250, "Unmounted.");
}

static s32 ftp_SITE_UNKNOWN(client_t *client, char *rest UNUSED) {
    return write_reply(client, 501, "Unknown SITE command.");
}

static s32 ftp_SITE_LOAD(client_t *client, char *path UNUSED) {
 //   FILE *f = vrt_fopen(client->cwd, path, "rb");
 //   if (!f) return write_reply(client, 550, strerror(errno));
 //   char *real_path = to_real_path(client->cwd, path);
 //   if (!real_path) goto end;
 //   load_from_file(f, real_path);
 //   free(real_path);
 //   end:
 //   fclose(f);
    return write_reply(client, 500, "Unable to load.");
}

typedef s32 (*ftp_command_handler)(client_t *client, char *args);

static s32 dispatch_to_handler(client_t *client, char *cmd_line, const char **commands, const ftp_command_handler *handlers) {
    char cmd[FTP_MSG_BUFFER_SIZE], rest[FTP_MSG_BUFFER_SIZE];
    char *args[] = { cmd, rest };
    split(cmd_line, ' ', 1, args);
    s32 i;
    for (i = 0; commands[i]; i++) {
        if (!strcasecmp(commands[i], cmd)) break;
    }
    return handlers[i](client, rest);
}

static const char *site_commands[] = { "LOADER", "CLEAR", "CHMOD", "PASSWD", "NOPASSWD", "EJECT", "MOUNT", "UNMOUNT", "LOAD", NULL };
static const ftp_command_handler site_handlers[] = { ftp_SITE_LOADER, ftp_SITE_CLEAR, ftp_SITE_CHMOD, ftp_SITE_PASSWD, ftp_SITE_NOPASSWD, ftp_SITE_EJECT, ftp_SITE_MOUNT, ftp_SITE_UNMOUNT, ftp_SITE_LOAD, ftp_SITE_UNKNOWN };

static s32 ftp_SITE(client_t *client, char *cmd_line) {
    return dispatch_to_handler(client, cmd_line, site_commands, site_handlers);
}

static s32 ftp_NOOP(client_t *client, char *rest UNUSED) {
    return write_reply(client, 200, "NOOP command successful.");
}

static s32 ftp_SUPERFLUOUS(client_t *client, char *rest UNUSED) {
    return write_reply(client, 202, "Command not implemented, superfluous at this site.");
}

static s32 ftp_NEEDAUTH(client_t *client, char *rest UNUSED) {
    return write_reply(client, 530, "Please login with USER and PASS.");
}

static s32 ftp_UNKNOWN(client_t *client, char *rest UNUSED) {
    return write_reply(client, 502, "Command not implemented.");
}

static const char *unauthenticated_commands[] = { "USER", "PASS", "QUIT", "REIN", "NOOP", NULL };
static const ftp_command_handler unauthenticated_handlers[] = { ftp_USER, ftp_PASS, ftp_QUIT, ftp_REIN, ftp_NOOP, ftp_NEEDAUTH };

static const char *authenticated_commands[] = {
    "USER", "PASS", "LIST", "PWD", "CWD", "CDUP",
    "SIZE", "PASV", "PORT", "TYPE", "SYST", "MODE",
    "RETR", "STOR", "APPE", "REST", "DELE", "MKD",
    "RMD", "RNFR", "RNTO", "NLST", "QUIT", "REIN",
    "SITE", "NOOP", "ALLO", NULL
};
static const ftp_command_handler authenticated_handlers[] = {
    ftp_USER, ftp_PASS, ftp_LIST, ftp_PWD, ftp_CWD, ftp_CDUP,
    ftp_SIZE, ftp_PASV, ftp_PORT, ftp_TYPE, ftp_SYST, ftp_MODE,
    ftp_RETR, ftp_STOR, ftp_APPE, ftp_REST, ftp_DELE, ftp_MKD,
    ftp_DELE, ftp_RNFR, ftp_RNTO, ftp_NLST, ftp_QUIT, ftp_REIN,
    ftp_SITE, ftp_NOOP, ftp_SUPERFLUOUS, ftp_UNKNOWN
};

/*
    returns negative to signal an error that requires closing the connection
*/
static s32 process_command(client_t *client, char *cmd_line) {
    if (strlen(cmd_line) == 0) {
        return 0;
    }

    display("< %s\n", cmd_line);

    const char **commands = unauthenticated_commands;
    const ftp_command_handler *handlers = unauthenticated_handlers;

    if (client->authenticated) {
        commands = authenticated_commands;
        handlers = authenticated_handlers;
    }

    return dispatch_to_handler(client, cmd_line, commands, handlers);
}

static void cleanup_data_resources(client_t *client) {
    if (client->data_socket >= 0 && client->data_socket != client->passive_socket) {
        network_close_blocking(client->data_socket);
    }
    client->data_socket = -1;
    client->data_connection_connected = false;
    client->data_callback = NULL;
    if (client->data_connection_cleanup) {
        client->data_connection_cleanup(client->data_connection_callback_arg);
    }
    client->data_connection_callback_arg = NULL;
    client->data_connection_cleanup = NULL;
    client->data_connection_timer = 0;
}

static void cleanup_client(client_t *client) {
    network_close_blocking(client->socket);
    cleanup_data_resources(client);
    close_passive_socket(client);
    int client_index;
    for (client_index = 0; client_index < NB_SIMULTANEOUS_CONNECTIONS; client_index++) {
        if (clients[client_index] == client) {
            clients[client_index] = NULL;
            break;
        }
    }
    free(client);
    nbConnections--;
    display("  Client disconnected.\n");
}

void cleanup_ftp() {
    int client_index;
    for (client_index = 0; client_index < NB_SIMULTANEOUS_CONNECTIONS; client_index++) {
        client_t *client = clients[client_index];
        if (client) {
            write_reply(client, 421, "  Closing remaining active clients connection.");
            cleanup_client(client);
        }
    }
    if (ftpThreadStack != NULL) OSFreeToSystem(ftpThreadStack);
    if (ftpThread != NULL) OSFreeToSystem(ftpThread);
}

static bool process_getClients() {
    s32 peer;
    struct sockaddr_in client_address;
    s32 addrlen = sizeof(client_address);
    while ((peer = network_accept(listener, (struct sockaddr *)&client_address, &addrlen)) != -EAGAIN) {
        if (peer < 0) {
            display("! ERROR : Error accepting connection: [%i] %s\n", -peer, strerror(-peer));
            return false;
        }

        if (nbConnections == 0) strcpy(clientIp,inet_ntoa(client_address.sin_addr));
        if (strcmp(clientIp,inet_ntoa(client_address.sin_addr)) !=0) { 
            
            display("! WARNING : %d already connected, close all his connections before trying from %d", clientIp, inet_ntoa(client_address.sin_addr));
            network_close(peer);
            return true;
        }
        display("  Accepted connection from %s!\n", inet_ntoa(client_address.sin_addr));
        
        if (nbConnections == NB_SIMULTANEOUS_CONNECTIONS) {
            display("! WARNING : Maximum connections number reached (%d), retry after ends one current transfert", NB_SIMULTANEOUS_CONNECTIONS);
            network_close(peer);
            return true;
        }

        client_t *client = malloc(sizeof(client_t));
        if (!client) {
            display("! ERROR : Could not allocate memory for client state, not accepting client.\n");
            network_close(peer);
            return true;
        }
        client->socket = peer;
        client->representation_type = 'A';
        client->passive_socket = -1;
        client->data_socket = -1;
        strcpy(client->cwd, "/");
        *client->pending_rename = '\0';
        client->restart_marker = 0;
        client->authenticated = false;
        client->offset = 0;
        client->data_connection_connected = false;
        client->data_callback = NULL;
        client->data_connection_callback_arg = NULL;
        client->data_connection_cleanup = NULL;
        client->data_connection_timer = 0;
        memcpy(&client->address, &client_address, sizeof(client_address));
        u32 client_index;
        if (write_reply(client, 220, "WiiUFtpServer") < 0) {
            display("! ERROR : Error writing greeting.");
            network_close_blocking(peer);
            free(client);
        } else {
            for (client_index = 0; client_index < NB_SIMULTANEOUS_CONNECTIONS; client_index++) {
                if (!clients[client_index]) {
                    clients[client_index] = client;
                    break;
                }
            }
            nbConnections++;
        }
    }
    return true;
}

static void process_data_events(client_t *client) {
    s32 result;
    if (!client->data_connection_connected) {
        if (client->passive_socket >= 0) {
            struct sockaddr_in data_peer_address;
            s32 addrlen = sizeof(data_peer_address);
            result = network_accept(client->passive_socket, (struct sockaddr *)&data_peer_address ,&addrlen);
            if (result >= 0) {
                client->data_socket = result;
                client->data_connection_connected = true;
            }
        } else {
            if ((result = network_connect(client->data_socket, (struct sockaddr *)&client->address, sizeof(client->address))) < 0) {
                if (result == -EINPROGRESS || result == -EALREADY) result = -EAGAIN;
                if ((result != -EAGAIN) && (result != -EISCONN))
                {
                    display("! ERROR : Unable to connect to client: [%i] %s\n", -result, strerror(-result));
                }
            }
             if (result >= 0 || result == -EISCONN) {
                client->data_connection_connected = true;
            }
        }
        if (client->data_connection_connected) {
            result = 1;
            display("  Connected to client!  Transferring data...\n");
        } else if (OSGetTick() > client->data_connection_timer) {
            result = -2;
            display("! ERROR : Timed out waiting for data connection.\n");
        }
    } else {
        result = client->data_callback(client->data_socket, client->data_connection_callback_arg);
    }

    if (result <= 0 && result != -EAGAIN) {
        cleanup_data_resources(client);
        if (result < 0) {
            result = write_reply(client, 520, "Closing data connection, error occurred during transfer.");
        } else {
            result = write_reply(client, 226, "Closing data connection, transfer successful.");
        }
        if (result < 0) {
            cleanup_client(client);
        }
    }
}

static void process_control_events(client_t *client) {
    s32 bytes_read;
    while (client->offset < (FTP_MSG_BUFFER_SIZE - 1)) {
        if (client->data_callback) {
            return;
        }
        char *offset_buf = client->buf + client->offset;
        if ((bytes_read = network_read(client->socket, offset_buf, FTP_MSG_BUFFER_SIZE - 1 - client->offset)) < 0) {
            if (bytes_read != -EAGAIN) {
                display("! ERROR : Read error %i occurred, closing client.\n", bytes_read);
                goto recv_loop_end;
            }
            return;
        } else if (bytes_read == 0) {
            goto recv_loop_end; // EOF from client
        }
        client->offset += bytes_read;
        client->buf[client->offset] = '\0';

        if (strchr(offset_buf, '\0') != (client->buf + client->offset)) {
            display("! WNG : Received a null byte from client, closing connection ;-)\n"); // i have decided this isn't allowed =P
            goto recv_loop_end;
        }

        char *next;
        char *end;
        for (next = client->buf; (end = strstr(next, CRLF)) && !client->data_callback; next = end + CRLF_LENGTH) {
            *end = '\0';
            if (strchr(next, '\n')) {
                display("! WNG : Received a line-feed from client without preceding carriage return, closing connection ;-)\n"); // i have decided this isn't allowed =P
                goto recv_loop_end;
            }

            if (*next) {
                s32 result;
                if ((result = process_command(client, next)) < 0) {
                    if (result != -EQUIT) {
                        display("! ERROR : Closing connection due to error while processing command: %s\n", next);
                    }
                    goto recv_loop_end;
                }
            }

        }

        if (next != client->buf) { // some lines were processed
            client->offset = strlen(next);
            char tmp_buf[client->offset];
            memcpy(tmp_buf, next, client->offset);
            memcpy(client->buf, tmp_buf, client->offset);
        }
    }
    display("! ERROR : Received line longer than %u bytes, closing client.\n", FTP_MSG_BUFFER_SIZE - 1);

    recv_loop_end:
    cleanup_client(client);
}

bool process_ftp_events() {
    
    if (timeOs != NULL && tsOs == 0) {
        time_t ts1970=mktime(timeOs);
        // compute the timestamp in J1980 Epoch (1972 and 1976 got 366 days)
        tsOs = ts1970*1000000 + 86400*((366*2) + (365*8) +1);        
    }        
    
    bool network_down = !process_getClients();    
    u32 client_index;
    for (client_index = 0; client_index < nbConnections; client_index++) {
        client_t *client = clients[client_index];
        if (client) {
            if (client->data_callback) {
                process_data_events(client);
            } else {
                process_control_events(client);
            }
        }
    }
    return network_down;
}

void setOsTime(struct tm *tmTime) {
    if (!timeOs) timeOs=tmTime;
}

void setFsaFd(int hfd) {
    fsaFd = hfd;
}    
int getFsaFd() {
    return fsaFd;
}    