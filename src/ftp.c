/**************************************************************************
* WiiUFtpServer (fork of FTP everywhere by Laf111@2021)                   *
***************************************************************************/
#include <malloc.h>
#include <stdlib.h>
#include <string.h>
#include <sys/dir.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>
#include <coreinit/time.h>
#include <coreinit/memdefaultheap.h>
#include <time.h>

#include "ftp.h"
#include "virtualpath.h"
#include "net.h"
#include "vrt.h"

#define UNUSED    __attribute__((unused))

// IPV6 header is 60 bytes long
#define FTP_MSG_BUFFER_SIZE MIN_NET_BUFFER_SIZE-60

#define FTP_STACK_SIZE 0x2000

extern void logLine(const char *line);

static bool verboseMode=false;
static const uint16_t SRC_PORT = 20;
static const int32_t EQUIT = 696969;
static const char *CRLF = "\r\n";
static const uint32_t CRLF_LENGTH = 2;

static uint32_t nbConnections = 0;
static char clientIp[15]="";
// passive_port : 1024 - 65535
static uint16_t passive_port = 1024;
static char *password = NULL;

// OS time computed in main
static struct tm *timeOs=NULL;
// OS time computed in main
static time_t tsOs=0;

// IOSUHAX fd
static int fsaFd = -1;

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
    uint64_t data_connection_timer;
};

typedef struct connection_struct connection_t;

static connection_t *connections[NB_SIMULTANEOUS_CONNECTIONS] = { NULL };
static int listener=-1;     // listening socket descriptor


// FTP thread on CPU2
static OSThread ftpThread;
static uint8_t *ftpThreadStack;

static int ftpThreadMain(int argc, const char **argv)
{

    int32_t socket = network_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (socket < 0)
        WHBLogPrintf("! ERROR : network_socket failed and return %d", socket);        

    // Set to non-blocking I/O 
    set_blocking(socket, false);
        
    uint32_t enable = 1;
    setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
    
    return socket;
}

// function to toggle verbose mode
void setVerboseMode(bool flag) {
    verboseMode=flag;
}

int32_t create_server(uint16_t port) {
    
//    logLine("DEBUG : allocate ftpThreadStack");    
    
        
    ftpThreadStack = MEMAllocFromDefaultHeapEx(FTP_STACK_SIZE, 8);
    if (ftpThreadStack == NULL) {
        WHBLogPrintf("! ERROR : when allocating ftpThreadStack!");        
        return -1;
    }

    if (!OSCreateThread(&ftpThread, ftpThreadMain, 0, NULL, ftpThreadStack + FTP_STACK_SIZE, FTP_STACK_SIZE, 1, OS_THREAD_ATTRIB_AFFINITY_CPU2)) {
        WHBLogPrintf("! ERROR : when creating ftpThread!");        
        return -1;
    }
    
    OSSetThreadName(&ftpThread, "ftp thread on CPU2");
    
    OSResumeThread(&ftpThread);
    
    OSJoinThread(&ftpThread, &listener);
    if (listener < 0)
        return -1;

    struct sockaddr_in bindAddress;
    memset(&bindAddress, 0, sizeof(bindAddress));
    bindAddress.sin_family = AF_INET;
    bindAddress.sin_port = htons(port);
    bindAddress.sin_addr.s_addr = htonl(INADDR_ANY);


    int32_t ret;
    if ((ret = network_bind(listener, (struct sockaddr *)&bindAddress, sizeof(bindAddress))) < 0) {
        network_close(listener);
        return ret;
    }
    if ((ret = network_listen(listener, NB_SIMULTANEOUS_CONNECTIONS)) < 0) {
        network_close(listener);
        return ret;
    }

    uint32_t ip = network_gethostip();
    
    WHBLogPrintf(" ");     
    WHBLogPrintf("    @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
    WHBLogPrintf("    @   Server IP adress = %u.%u.%u.%u ,port = %i   @", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF, port);
    WHBLogPrintf("    @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
    WHBLogPrintf(" ");    
    WHBLogConsoleDraw();
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
            WHBLogPrintf("! ERROR : When allocation vlPath");
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
                WHBLogPrintf("! ERROR : No volume found for %s", vPath);
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
static int32_t write_reply(connection_t *client, uint16_t code, char *msg) {
    uint32_t msglen = 4 + strlen(msg) + CRLF_LENGTH;
    char msgbuf[msglen + 1];
    if (msgbuf == NULL) return -ENOMEM;
    sprintf(msgbuf, "%u %s\r\n", code, msg);
    if (verboseMode) WHBLogPrintf("> %s", msgbuf);

    return send_exact(client->socket, msgbuf, msglen);
}

static void close_passive_socket(connection_t *client) {
    if (client->passive_socket >= 0) {
        network_close_blocking(client->passive_socket);
        client->passive_socket = -1;
    }
}

/*
    result must be able to hold up to maxsplit+1 null-terminated strings of length strlen(s)
    returns the number of strings stored in the result array (up to maxsplit+1)
*/
static uint32_t split(char *s, char sep, uint32_t maxsplit, char *result[]) {
    uint32_t num_results = 0;
    uint32_t result_pos = 0;
    uint32_t trim_pos = 0;
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
    uint32_t i = num_results;
    for (i = num_results; i <= maxsplit; i++) {
        result[i][0] = '\0';
    }
    return num_results;
}

static int32_t ftp_USER(connection_t *client, char *username UNUSED) {
    return write_reply(client, 331, "User name okay, need password.");
}

static int32_t ftp_PASS(connection_t *client, char *password_attempt) {
    if (compare_ftp_password(password_attempt)) {
        client->authenticated = true;
        return write_reply(client, 230, "User logged in, proceed.");
    } else {
        return write_reply(client, 530, "Login incorrect.");
    }
}

static int32_t ftp_REIN(connection_t *client, char *rest UNUSED) {
    close_passive_socket(client);
    strcpy(client->cwd, "/");
    client->representation_type = 'A';
    client->authenticated = false;
    return write_reply(client, 220, "Service ready for new user.");
}

static int32_t ftp_QUIT(connection_t *client, char *rest UNUSED) {
    // TODO: dont quit if xfer in progress
    int32_t result = write_reply(client, 221, "Service closing control connection.");
    return result < 0 ? result : -EQUIT;
}

static int32_t ftp_SYST(connection_t *client, char *rest UNUSED) {
    return write_reply(client, 215, "UNIX Type: L8 Version: WiiUFtpServer");
}

static int32_t ftp_TYPE(connection_t *client, char *rest) {
    char representation_type[FTP_MSG_BUFFER_SIZE] = "", param[FTP_MSG_BUFFER_SIZE] = "";
    char *args[] = { representation_type, param };
    uint32_t num_args = split(rest, ' ', 1, args);
    if (num_args == 0) {
        return write_reply(client, 501, "Syntax error in parameters.");
    } else if ((!strcasecmp("A", representation_type) && (!*param || !strcasecmp("N", param))) ||
               (!strcasecmp("I", representation_type) && num_args == 1)) {
        client->representation_type = *representation_type;
    } else {
        return write_reply(client, 501, "Syntax error in parameters.");
    }
    char msg[FTP_MSG_BUFFER_SIZE+21] = "";
    sprintf(msg,"Type set to %s.", representation_type);
    return write_reply(client, 200, msg);
}

static int32_t ftp_MODE(connection_t *client, char *rest) {
    if (!strcasecmp("S", rest)) {
        return write_reply(client, 200, "Mode S ok.");
    } else {
        return write_reply(client, 501, "Syntax error in parameters.");
    }
}

static int32_t ftp_PWD(connection_t *client, char *rest UNUSED) {
    char msg[MAXPATHLEN + 24] = "";
    // TODO: escape double-quotes
    sprintf(msg, "\"%s\" is current directory.", client->cwd);
    return write_reply(client, 257, msg);
}

static int32_t ftp_CWD(connection_t *client, char *path) {
    int32_t result;
    if (!vrt_chdir(client->cwd, path)) {
        result = write_reply(client, 250, "CWD command successful.");
    } else  {
        result = write_reply(client, 550, strerror(errno));
    }
    return result;
}

static int32_t ftp_CDUP(connection_t *client, char *rest UNUSED) {
    int32_t result;
    if (!vrt_chdir(client->cwd, "..")) {
        result = write_reply(client, 250, "CDUP command successful.");
    } else  {
        result = write_reply(client, 550, strerror(errno));
    }
    return result;
}

static int32_t ftp_DELE(connection_t *client, char *path) {
    if (!vrt_unlink(client->cwd, path)) {
        return write_reply(client, 250, "File or directory removed.");
    } else {
        return write_reply(client, 550, strerror(errno));
    }
}

static int32_t ftp_MKD(connection_t *client, char *path) {
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

static int32_t ftp_RNFR(connection_t *client, char *path) {
    strcpy(client->pending_rename, path);
    return write_reply(client, 350, "Ready for RNTO.");
}

static int32_t ftp_RNTO(connection_t *client, char *path) {
    if (!*client->pending_rename) {
        return write_reply(client, 503, "RNFR required first.");
    }
    int32_t result;
    if (!vrt_rename(client->cwd, client->pending_rename, path)) {
        result = write_reply(client, 250, "Rename successful.");
    } else {
        result = write_reply(client, 550, strerror(errno));
    }
    *client->pending_rename = '\0';
    return result;
}

static int32_t ftp_SIZE(connection_t *client, char *path) {
    struct stat st;
    if (!vrt_stat(client->cwd, path, &st)) {
        char size_buf[12] = "";
        sprintf(size_buf, "%llu", st.st_size);
        return write_reply(client, 213, size_buf);
    } else {
        return write_reply(client, 550, strerror(errno));
    }
}

static int32_t ftp_PASV(connection_t *client, char *rest UNUSED) {
    close_passive_socket(client);
    client->passive_socket = network_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (client->passive_socket < 0) {
        return write_reply(client, 520, "Unable to create listening socket.");
    }
    set_blocking(client->passive_socket, false);
    struct sockaddr_in bindAddress;
    memset(&bindAddress, 0, sizeof(bindAddress));
    bindAddress.sin_family = AF_INET;
    // reset passive_port to avoid overflow
    if (passive_port == 65535) passive_port = 1024;
    bindAddress.sin_port = htons(passive_port++);
    bindAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    int32_t result;
    if ((result = network_bind(client->passive_socket, (struct sockaddr *)&bindAddress, sizeof(bindAddress))) < 0) {
        close_passive_socket(client);
        return write_reply(client, 520, "Unable to bind listening socket.");
    }
    if ((result = network_listen(client->passive_socket, 1)) < 0) {
        close_passive_socket(client);
        return write_reply(client, 520, "Unable to listen on socket.");
    }
    char reply[49+2] = "";
    uint16_t port = bindAddress.sin_port;
    uint32_t ip = network_gethostip();
	if (verboseMode) {
	    struct in_addr addr;
	    addr.s_addr = ip;
	    WHBLogPrintf("Listening for data connections at %s : %lu...", inet_ntoa(addr), port);
	}
    sprintf(reply, "Entering Passive Mode (%d,%d,%d,%d,%"PRIu16",%"PRIu16").", (ip >> 24) & 0xff, (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff, (port >> 8) & 0xff, port & 0xff);
    return write_reply(client, 227, reply);
}

static int32_t ftp_PORT(connection_t *client, char *portspec) {
    uint32_t h1, h2, h3, h4, p1, p2;
    if (sscanf(portspec, "%3d,%3d,%3d,%3d,%3d,%3d", &h1, &h2, &h3, &h4, &p1, &p2) < 6) {
        
        return write_reply(client, 501, "Syntax error in parameters.");
    }
    char addr_str[44] = "";
    sprintf(addr_str, "%d.%d.%d.%d", h1, h2, h3, h4);
    struct in_addr sin_addr;
    if (!inet_aton(addr_str, &sin_addr)) {
        return write_reply(client, 501, "Syntax error in parameters.");
    }
    close_passive_socket(client);
    uint16_t port = ((p1 &0xff) << 8) | (p2 & 0xff);
    client->address.sin_addr = sin_addr;
    client->address.sin_port = htons(port);
    if (verboseMode) WHBLogPrintf("Sending server address to %s on %lu port", addr_str, port);
    return write_reply(client, 200, "PORT command successful.");
}

typedef int32_t (*data_connection_handler)(connection_t *client, data_connection_callback callback, void *arg);

static int32_t prepare_data_connection_active(connection_t *client, data_connection_callback callback UNUSED, void *arg UNUSED) {
    
    int32_t data_socket = network_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (data_socket < 0) return data_socket;
    set_blocking(data_socket, false);
    
    int enable = 1;
    if (!setsockopt(data_socket, SOL_SOCKET, SO_USERBUF, &enable, sizeof(enable))==0)
        {logLine("! ERROR : Socket memory optimization failed !");}
        
    struct sockaddr_in bindAddress;
    memset(&bindAddress, 0, sizeof(bindAddress));
    bindAddress.sin_family = AF_INET;
    bindAddress.sin_port = htons(SRC_PORT);
    bindAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    int32_t result;
    if ((result = network_bind(data_socket, (struct sockaddr *)&bindAddress, sizeof(bindAddress))) < 0) {
        network_close(data_socket);
        return result;
    }

    client->data_socket = data_socket;
    if (verboseMode) WHBLogPrintf("Attempting to connect to client through %s : %u", inet_ntoa(client->address.sin_addr), client->address.sin_port);
    return 0;
}

static int32_t prepare_data_connection_passive(connection_t *client, data_connection_callback callback UNUSED, void *arg UNUSED) {
    client->data_socket = client->passive_socket;
    if (verboseMode) WHBLogPrintf("Waiting for data connections...");
    return 0;
}

static int32_t prepare_data_connection(connection_t *client, void *callback, void *arg, void *cleanup) {
    int32_t result = write_reply(client, 150, "Transferring data.");
    if (result >= 0) {
        data_connection_handler handler = prepare_data_connection_active;
        if (client->passive_socket >= 0) handler = prepare_data_connection_passive;
        result = handler(client, (data_connection_callback)callback, arg);
        if (result < 0) {
            WHBLogPrintf("! WARNING : data transfer handler failed , socket error = %d", result);            
            result = write_reply(client, 520, "Closing data connection, error occurred during transfer.");
        } else {
            client->data_connection_connected = false;
            client->data_callback = callback;
            client->data_connection_callback_arg = arg;
            client->data_connection_cleanup = cleanup;
            client->data_connection_timer = OSGetTick() + OSSecondsToTicks(1200);
        }
    }
    return result;
}

static int32_t send_nlst(int32_t data_socket, DIR_P *iter) {
    int32_t result = 0;
    char filename[MAXPATHLEN] = "";
    struct dirent *dirent = NULL;
    while ((dirent = vrt_readdir(iter)) != 0) {
        size_t end_index = strlen(dirent->d_name);
        if (end_index + 2 >= MAXPATHLEN)
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

static int32_t send_list(int32_t data_socket, DIR_P *iter) {

    int32_t result = 0;

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

static int32_t ftp_NLST(connection_t *client, char *path) {
    if (!*path) {
        path = ".";
    }

    DIR_P *dir = vrt_opendir(client->cwd, path);
    if (dir == NULL) {
        return write_reply(client, 550, strerror(errno));
    }

    int32_t result = prepare_data_connection(client, send_nlst, dir, vrt_closedir);
    if (result < 0) vrt_closedir(dir);
    return result;
}

static int32_t ftp_LIST(connection_t *client, char *path) {
    char rest[FTP_MSG_BUFFER_SIZE] = "";
    if (*path == '-') {
        // handle buggy clients that use "LIST -aL" or similar, at the expense of breaking paths that begin with '-'
        char flags[FTP_MSG_BUFFER_SIZE] = "";
        char *args[] = { flags, rest };
        split(path, ' ', 1, args);
        path = rest;
    }
    if (!*path) {
        path = ".";
    }

    if (path && client->cwd) if (strcmp(path, ".") == 0 && strcmp(client->cwd, "/") == 0) ResetVirtualPaths(); 

    DIR_P *dir = vrt_opendir(client->cwd, path);
    if (dir == NULL) {
        return write_reply(client, 550, strerror(errno));
    }

    int32_t result = prepare_data_connection(client, send_list, dir, vrt_closedir);
    if (result < 0) vrt_closedir(dir);
    return result;
}

static int32_t ftp_RETR(connection_t *client, char *path) {
    FILE *f = vrt_fopen(client->cwd, path, "rb");
    if (!f) {
        return write_reply(client, 550, strerror(errno));
    }

    int fd = fileno(f);
    if (client->restart_marker && lseek(fd, client->restart_marker, SEEK_SET) != client->restart_marker) {
        int32_t lseek_error = errno;
        fclose(f);
        client->restart_marker = 0;
        return write_reply(client, 550, strerror(lseek_error));
    }
    client->restart_marker = 0;

    int32_t result = prepare_data_connection(client, send_from_file, f, fclose);
    if (result < 0) fclose(f);
    return result;
}

static int32_t stor_or_append(connection_t *client, char *path, FILE *f) {
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
    
    int32_t result = prepare_data_connection(client, recv_to_file, f, fclose);
    
    if (result < 0) fclose(f);
    return result;
}

static int32_t ftp_STOR(connection_t *client, char *path) {
    FILE *f = vrt_fopen(client->cwd, path, "wb");
    int fd;
    if (f) fd = fileno(f);
    if (f && client->restart_marker && lseek(fd, client->restart_marker, SEEK_SET) != client->restart_marker) {
        int32_t lseek_error = errno;
        fclose(f);
        client->restart_marker = 0;
        return write_reply(client, 550, strerror(lseek_error));
    }
    client->restart_marker = 0;

    return stor_or_append(client, path, f);
}

static int32_t ftp_APPE(connection_t *client, char *path) {
    
    return stor_or_append(client, path, vrt_fopen(client->cwd, path, "ab"));
}

static int32_t ftp_REST(connection_t *client, char *offset_str) {
    off_t offset;
    if (sscanf(offset_str, "%lli", &offset) < 1 || offset < 0) {
        return write_reply(client, 501, "Syntax error in parameters.");
    }
    client->restart_marker = offset;
    char msg[FTP_MSG_BUFFER_SIZE] = "";
    sprintf(msg, "Restart position accepted (%lli).", offset);
    return write_reply(client, 350, msg);
}

static int32_t ftp_SITE_LOADER(connection_t *client, char *rest UNUSED) {
    int32_t result = write_reply(client, 200, "Exiting to loader.");
    //set_reset_flag();
    return result;
}

static int32_t ftp_SITE_CLEAR(connection_t *client, char *rest UNUSED) {
    int32_t result = write_reply(client, 200, "Cleared.");
    uint32_t i;
    for (i = 0; i < 18; i++) WHBLogPrintf("\n");
    //WHBLogPrintf("\x1b[2;0H");
    return result;
}

/*
    This is implemented as a no-op to prevent some FTP clients
    from displaying skip/abort/retry type prompts.
*/
static int32_t ftp_SITE_CHMOD(connection_t *client, char *rest UNUSED) {
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

static int32_t ftp_SITE_PASSWD(connection_t *client, char *new_password) {
    set_ftp_password(new_password);
    return write_reply(client, 200, "Password changed.");
}

static int32_t ftp_SITE_NOPASSWD(connection_t *client, char *rest UNUSED) {
    set_ftp_password(NULL);
    return write_reply(client, 200, "Authentication disabled.");
}

static int32_t ftp_SITE_EJECT(connection_t *client, char *rest UNUSED) {
    //if (dvd_eject()) return write_reply(client, 550, "Unable to eject DVD.");
    return write_reply(client, 200, "DVD ejected.");
}

static int32_t ftp_SITE_MOUNT(connection_t *client, char *path UNUSED) {
    //if (!mount_virtual(path)) return write_reply(client, 550, "Unable to mount.");
    return write_reply(client, 250, "Mounted.");
}

static int32_t ftp_SITE_UNMOUNT(connection_t *client, char *path UNUSED) {
    //if (!unmount_virtual(path)) return write_reply(client, 550, "Unable to unmount.");
    return write_reply(client, 250, "Unmounted.");
}

static int32_t ftp_SITE_UNKNOWN(connection_t *client, char *rest UNUSED) {
    return write_reply(client, 501, "Unknown SITE command.");
}

static int32_t ftp_SITE_LOAD(connection_t *client, char *path UNUSED) {
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

typedef int32_t (*ftp_command_handler)(connection_t *client, char *args);

static int32_t dispatch_to_handler(connection_t *client, char *cmd_line, const char **commands, const ftp_command_handler *handlers) {
    char cmd[FTP_MSG_BUFFER_SIZE] = "", rest[FTP_MSG_BUFFER_SIZE] = "";
    char *args[] = { cmd, rest };
    split(cmd_line, ' ', 1, args);
    int32_t i;
    for (i = 0; commands[i]; i++) {
        if (!strcasecmp(commands[i], cmd)) break;
    }
    return handlers[i](client, rest);
}

static const char *site_commands[] = { "LOADER", "CLEAR", "CHMOD", "PASSWD", "NOPASSWD", "EJECT", "MOUNT", "UNMOUNT", "LOAD", NULL };
static const ftp_command_handler site_handlers[] = { ftp_SITE_LOADER, ftp_SITE_CLEAR, ftp_SITE_CHMOD, ftp_SITE_PASSWD, ftp_SITE_NOPASSWD, ftp_SITE_EJECT, ftp_SITE_MOUNT, ftp_SITE_UNMOUNT, ftp_SITE_LOAD, ftp_SITE_UNKNOWN };

static int32_t ftp_SITE(connection_t *client, char *cmd_line) {
    return dispatch_to_handler(client, cmd_line, site_commands, site_handlers);
}

static int32_t ftp_NOOP(connection_t *client, char *rest UNUSED) {
    return write_reply(client, 200, "NOOP command successful.");
}

static int32_t ftp_SUPERFLUOUS(connection_t *client, char *rest UNUSED) {
    return write_reply(client, 202, "Command not implemented, superfluous at this site.");
}

static int32_t ftp_NEEDAUTH(connection_t *client, char *rest UNUSED) {
    return write_reply(client, 530, "Please login with USER and PASS.");
}

static int32_t ftp_UNKNOWN(connection_t *client, char *rest UNUSED) {
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
static int32_t process_command(connection_t *client, char *cmd_line) {
    if (strlen(cmd_line) == 0) {
        return 0;
    }

    if (verboseMode) WHBLogPrintf("< %s", cmd_line);

    const char **commands = unauthenticated_commands;
    const ftp_command_handler *handlers = unauthenticated_handlers;

    if (client->authenticated) {
        commands = authenticated_commands;
        handlers = authenticated_handlers;
    }

    return dispatch_to_handler(client, cmd_line, commands, handlers);
}

static void cleanup_data_resources(connection_t *client) {
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

static void cleanup_client(connection_t *client) {
    network_close_blocking(client->socket);
    cleanup_data_resources(client);
    close_passive_socket(client);
    int client_index;
    for (client_index = 0; client_index < NB_SIMULTANEOUS_CONNECTIONS; client_index++) {
        if (connections[client_index] == client) {
            connections[client_index] = NULL;
            break;
        }
    }
    free(client);
    nbConnections--;
    WHBLogPrintf("Client %s disconnected.", clientIp);
}

void cleanup_ftp() {
    int client_index;
    for (client_index = 0; client_index < NB_SIMULTANEOUS_CONNECTIONS; client_index++) {
        connection_t *client = connections[client_index];
        if (client) {
            write_reply(client, 421, "Closing remaining active connections connection.");
            cleanup_client(client);
        }
    }
    if (ftpThreadStack != NULL) MEMFreeToDefaultHeap(ftpThreadStack);
}

static bool process_getClients() {
    int32_t peer;
    struct sockaddr_in client_address;
    int32_t addrlen = sizeof(client_address);
    while ((peer = network_accept(listener, (struct sockaddr *)&client_address, &addrlen)) != -EAGAIN) {
        if (peer < 0) {
            WHBLogPrintf("ERROR : Error accepting connection: [%i] %s", -peer, strerror(-peer));
            return false;
        }

        if (nbConnections == 0) strcpy(clientIp,inet_ntoa(client_address.sin_addr));
        if (strcmp(clientIp,inet_ntoa(client_address.sin_addr)) !=0) { 
            
            WHBLogPrintf("! WARNING : %s already connected, close all his connections", clientIp);
            network_close(peer);
            return true;
        }
        WHBLogPrintf("Accepted connection from %s!\n", inet_ntoa(client_address.sin_addr));

        if (nbConnections == NB_SIMULTANEOUS_CONNECTIONS) {
            WHBLogPrintf("! WARNING : Maximum connections number reached (%d), retry after ends one current transfert", NB_SIMULTANEOUS_CONNECTIONS);
            network_close(peer);
            return true;
        }

        connection_t *client = malloc(sizeof(connection_t));
        if (!client) {
            WHBLogPrintf("! ERROR : Could not allocate memory for client state, not accepting client.\n");
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
        int client_index;
        if (write_reply(client, 220, "WiiUFtpServer") < 0) {
            WHBLogPrintf("! ERROR : Error writing greeting.");
            network_close_blocking(peer);
            free(client);
        } else {
            
            if (nbConnections == NB_SIMULTANEOUS_CONNECTIONS) {
                char msg[FTP_MSG_BUFFER_SIZE];                
                sprintf(msg, "Maximum connections number reached (%d), retry after ends one current transfert", NB_SIMULTANEOUS_CONNECTIONS);
                write_reply(client, 520, msg);
                WHBLogPrintf("! WARNING : %s", msg);
            } else {
                
                for (client_index = 0; client_index < NB_SIMULTANEOUS_CONNECTIONS; client_index++) {
                    if (!connections[client_index]) {
                        connections[client_index] = client;
                        break;
                    }
                }
                nbConnections++;
            }
        }
    }
    return true;
}

static void process_data_events(connection_t *client) {
    int32_t result;
    if (!client->data_connection_connected) {
        if (client->passive_socket >= 0) {
            struct sockaddr_in data_peer_address;
            int32_t addrlen = sizeof(data_peer_address);
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
                    WHBLogPrintf("! ERROR : Unable to connect to client: [%i] %s", -result, strerror(-result));
                }
            }
             if (result >= 0 || result == -EISCONN) {
                client->data_connection_connected = true;
            }
        }
        if (client->data_connection_connected) {
            result = 1;
            WHBLogPrintf("Transferring data to %s...", clientIp);
        } else if (OSGetTick() > (int) client->data_connection_timer) {
            result = -2;
            WHBLogPrintf("! ERROR : Timed out waiting for data connection.");
        }
    } else {
        result = client->data_callback(client->data_socket, client->data_connection_callback_arg);
        if (result < 0 && result != -EAGAIN && result != -ENODATA) {
            WHBLogPrintf("! WARNING : data transfer callback failed , socket error = %d", result);
        }
    }

    if (result <= 0 && result != -EAGAIN && result != -ENODATA) {
        cleanup_data_resources(client);
        if (result < 0) {
            WHBLogPrintf("! ERROR : transfer failed, socket error = %d", result);            
            result = write_reply(client, 520, "Closing data connection, error occurred during transfer.");
        } else {
            result = write_reply(client, 226, "Closing data connection, transfer successful.");
        }
        if (result < 0) {
            cleanup_client(client);
        }
    }
}

static void process_control_events(connection_t *client) {
    int32_t bytes_read;
    while (client->offset < (FTP_MSG_BUFFER_SIZE - 1)) {
        if (client->data_callback) {
            return;
        }
        char *offset_buf = client->buf + client->offset;
        if ((bytes_read = network_read(client->socket, offset_buf, FTP_MSG_BUFFER_SIZE - 1 - client->offset)) < 0) {
            if (bytes_read != -EAGAIN) {
                WHBLogPrintf("! ERROR : Read error %i occurred, closing client.", bytes_read);
                goto recv_loop_end;
            }
            return;
        } else if (bytes_read == 0) {
            goto recv_loop_end; // EOF from client
        }
        client->offset += bytes_read;
        client->buf[client->offset] = '\0';

        if (strchr(offset_buf, '\0') != (client->buf + client->offset)) {
            WHBLogPrintf("! WARNING : Received a null byte from client, closing connection ;-)"); // i have decided this isn't allowed =P
            goto recv_loop_end;
        }

        char *next;
        char *end;
        for (next = client->buf; (end = strstr(next, CRLF)) && !client->data_callback; next = end + CRLF_LENGTH) {
            *end = '\0';
            if (strchr(next, '\n')) {
                WHBLogPrintf("! WARNING : Received a line-feed from client without preceding carriage return, closing connection ;-)"); // i have decided this isn't allowed =P
                goto recv_loop_end;
            }

            if (*next) {
                int32_t result;
                if ((result = process_command(client, next)) < 0) {
                    if (result != -EQUIT) {
                        WHBLogPrintf("! ERROR : Closing connection due to error while processing command: %s", next);
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
    WHBLogPrintf("! ERROR : Received line longer than %lu bytes, closing client.", FTP_MSG_BUFFER_SIZE - 1);

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
    uint32_t client_index;
    for (client_index = 0; client_index < nbConnections; client_index++) {
        connection_t *client = connections[client_index];
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