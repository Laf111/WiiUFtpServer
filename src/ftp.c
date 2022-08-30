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
#include <inttypes.h>
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

// global vraiables from main.c
extern int fsaFd;
extern bool verboseMode;
extern void display(const char *fmt, ...);

// global vraiables from virtualpath.c
extern char storage_usb_found[14];

// global vraiables from dynamic_libs/socket_functions.c
extern u32 hostIpAddress;

static bool network_down = false;
static const uint16_t SRC_PORT = 20;
static const int32_t EQUIT = 696969;
static const char *CRLF = "\r\n";
static const uint32_t CRLF_LENGTH = 2;

// number of active connections
static uint32_t activeConnectionsNumber = 0;
static uint32_t activeTransfersNumber = 0;

// unique client IP address
static char clientIp[15]="UNKNOWN_CLIENT";

// passive_port : 1024 - 65535
static uint16_t passive_port = 1024;
static char *password = NULL;

// OS time computed in main
static struct tm *timeOs=NULL;
// OS time creation date = 31/12/2009
static const time_t minTime = 1262214000;

static connection_t *connections[FTP_NB_SIMULTANEOUS_TRANSFERS] = { NULL };

static s32 listener = -1;     // listening socket descriptor

// max and min transfer rate speeds in MBs
static float maxTransferRate = -9999;
static float minTransferRate = 9999;

// sum of average speeds
static float sumAvgSpeed = 0;
// last sum of average speeds
static float lastSumAvgSpeed = 0;
// number of measures used for average computation
static uint32_t nbSpeedMeasures = 0;

static void resetConnection(connection_t *connection) {
    
    connection->representation_type = 'A';
    connection->passive_socket = -1;
    connection->data_socket = -1;
    strcpy(connection->cwd, "/");
    *connection->pending_rename = '\0';
    strcpy(connection->buf, "");
    connection->restart_marker = 0;
    connection->authenticated = false;
    connection->offset = 0;
    connection->data_connection_connected = false;
    connection->data_callback = NULL;
    connection->data_connection_callback_arg = NULL;
    connection->data_connection_cleanup = NULL;
    connection->data_connection_timer = 0;
    strcpy(connection->fileName, "");
    connection->volPath = NULL;
    connection->f = NULL;
    connection->dataTransferOffset = -1;
    connection->speed = 0;
    connection->bytesTransferred = -EAGAIN;

    
}

// initialize the connection without setting the index data member
static void initConnection(connection_t *connection) {
    
    resetConnection(connection);
    connection->socket = -1;
    
	// allocate the transfer thread and buffer
    connection->transferThread = OSAllocFromSystem(sizeof(OSThread), 32);

    // allocate the buffer for transfering
    s32 buf_size = TRANSFER_BUFFER_SIZE+32;
    connection->transferBuffer = NULL;

    // align memory (64bytes = 0x40) when alocating the buffer
    do{
        buf_size -= 32;
        if (buf_size < 0) {
            return;
        }
        connection->transferBuffer = (char *)memalign(0x40, buf_size);
    } while (!connection->transferBuffer);    
}

int32_t create_server(uint16_t port) {

    listener = network_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listener < 0) {
        display("! ERROR : network_socket failed to create listener %d", listener);
		return listener;
    }
    struct sockaddr_in bindAddress;
    memset(&bindAddress, 0, sizeof(bindAddress));
    bindAddress.sin_family = AF_INET;
    bindAddress.sin_port = htons(port);
    bindAddress.sin_addr.s_addr = htonl(INADDR_ANY);

    int32_t ret = -1;
    if ((ret = network_bind(listener, (struct sockaddr *)&bindAddress, sizeof(bindAddress))) < 0) {
        display("! ERROR : network_bind failed with %d (%s)", errno, strerror(errno));
		network_close(listener);
        return ret;
    }
    if ((ret = network_listen(listener, FTP_NB_SIMULTANEOUS_TRANSFERS)) < 0) {
        display("! ERROR : network_listen failed with %d (%s)", errno, strerror(errno));
        network_close(listener);
        return ret;
    }
    
    uint32_t ip = hostIpAddress;;     
    display(" ");
    display("Listening on %u.%u.%u.%u:%i", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF, port);

    uint32_t connection_index;
    
    for (connection_index = 0; connection_index < FTP_NB_SIMULTANEOUS_TRANSFERS; connection_index++) {
		
		connections[connection_index] = malloc(sizeof(connection_t));
        if (!connections[connection_index]) {
            display("! ERROR : Could not allocate memory for connection %d", connection_index+1);
            return -ENOMEM;
        }
	    connections[connection_index]->index = connection_index;
		
		initConnection(connections[connection_index]);
    }
    
    return listener;
}

// vPath : /storage_usb/path/saveinfo.xml -> vlPath : /vol/storage_usb01/path/saveinfo.xml
// This function allocate the memory returned.
// The caller must take care of freeing it
char* virtualToVolPath(char *vPath) {

    if (vPath) {
        if (strcmp(vPath,"/") ==0) return "/vol";

        int dimv=strlen(vPath);
        int dimm=dimv+6+1;

        // output
        char *vlPath = NULL;

        // allocate vlPath : caller will have to free it
        vlPath=(char *) malloc(sizeof(char)*dimm);
        if (!vlPath) {
            display("! ERROR : When allocation vlPath");
            return NULL;
        } else {

            char volume[30]="";
            if (strstr(vPath, "/storage_usb") != NULL) {
                char usbVolPath[19] = "/vol/";
                strcat(usbVolPath, storage_usb_found);        
                strcpy(volume, usbVolPath);
            } else if (strstr(vPath, "/storage_mlc") != NULL) {
                strcpy(volume,"/vol/storage_mlc01");
            } else if (strstr(vPath, "/storage_slccmpt") != NULL) {
                strcpy(volume,"/vol/storage_slccmpt01");
            } else if (strstr(vPath, "/storage_odd_tickets") != NULL) {
                strcpy(volume,"/vol/storage_odd_tickets");
            } else if (strstr(vPath, "/storage_odd_updates") != NULL) {
                strcpy(volume,"/vol/storage_odd_updates");
            } else if (strstr(vPath, "/storage_odd_content2") != NULL) {
                strcpy(volume,"/vol/storage_odd_content2");
            } else if (strstr(vPath, "/storage_odd_content") != NULL) {
                strcpy(volume,"/vol/storage_odd_content");
            } else if (strstr(vPath, "/storage_slc") != NULL) {
                strcpy(volume,"/vol/system");
            } else {
                strcpy(volume,"/sd");
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


void displayConnectionDetails(uint32_t index) {

    if (verboseMode) {
        display("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
        display("Connection %d ", index);
        display("------------------------------");

        if (index == 0) {
            display("socket                     = %d", connections[index]->socket);
            display("representation_type        = %c", connections[index]->representation_type);
            display("pending_rename             = %c", connections[index]->pending_rename);
            display("authenticated              = %d", connections[index]->authenticated);
        }

        display("cwd                        = %s", connections[index]->cwd);
        display("offset                     = %d", connections[index]->offset);
        display("passive_socket             = %d", connections[index]->passive_socket);

        display("data_socket                = %d", connections[index]->data_socket);
        display("restart_marker             = %d", connections[index]->restart_marker);
        display("data_connection_connected  = %d", connections[index]->data_connection_connected);
        display("data_connection_timer      = %d", connections[index]->data_connection_timer);

        if (connections[index]->f != NULL) {
            display("filename                   = %s", connections[index]->fileName);
            display("fd                         = %d", fileno(connections[index]->f));
            display("volPath                    = %s", connections[index]->volPath);
            display("dataTransferOffset         = %d", connections[index]->dataTransferOffset);
            display("speed                      = %.2f", connections[index]->speed);
            display("bytesTransferred            = %d", connections[index]->bytesTransferred);

        }

        display("Number of avg files used   = %d", nbSpeedMeasures);
        display("Sum of speed rate          = %.2f", sumAvgSpeed);

        display("Max transfer speed         = %.2f", maxTransferRate);
        display("Min transfer speed         = %.2f", minTransferRate);
    }
}

int launchTransfer(s32 argc UNUSED, void *argv)

{
        
    int32_t result = -101;
    connection_t* activeConnection = (connection_t*) argv;

    if (verboseMode) {
        display("C[%d] launching transfer for %s", activeConnection->index+1, activeConnection->fileName);
    }
    
    if (activeConnection->volPath == NULL) {
        result = send_from_file(activeConnection->data_socket, activeConnection);
        
    } else {
        
        result = recv_to_file(activeConnection->data_socket, activeConnection);
        
        // if cwd does not contain /sd/
        if ((result == 0) && (strstr(activeConnection->cwd, "/sd/") == NULL)) {        

            // change rights on file
            int rc = IOSUHAX_FSA_ChangeMode(fsaFd, activeConnection->volPath, 0x664);
            if (rc < 0 && rc != -EAGAIN) {
                display("~ WARNING : when settings file's rights, rc = %d !", rc);
                display("~ WARNING : err = %d (%s)", errno, strerror(errno));
                display("~ WARNING : file = %s", activeConnection->fileName);
            }            
        }            
    }
    
    return result;
}

// launch and monitor the transfer
static int32_t transfer(int32_t data_socket UNUSED, connection_t *connection) {
    int32_t result = -EAGAIN;

    // on the very first call
    if (connection->dataTransferOffset == -1) {
                
        activeTransfersNumber++;

        // init bytes counter
        connection->dataTransferOffset = 0;

        // init speed to 0
        connection->speed = 0;

		if (verboseMode) {
		    display("Using data_socket (%d) of C[%d] to transfer %s", data_socket, connection->index+1, connection->fileName);
//		    displayConnectionDetails(connection->index);
		}
        
    
        // Dispatching connections over CPUs :
        
        // activeTransfersNumber = 1,4,7 => CPU2
        s32 cpu = OS_THREAD_ATTR_AFFINITY_CORE2;                
        // activeTransfersNumber = 2,5,8 => CPU0
        if (activeTransfersNumber == 2 || activeTransfersNumber == 5 || activeTransfersNumber == 8) cpu = OS_THREAD_ATTR_AFFINITY_CORE0;
        // activeTransfersNumber = 3,6   => CPU1
        if (activeTransfersNumber == 3 || activeTransfersNumber == 6) cpu = OS_THREAD_ATTR_AFFINITY_CORE1;
    

        // set thread priority : 
    
        // activeTransfersNumber = 1,2,3 => prio = 2*NB_SIMULTANEOUS_TRANSFERS
        u32 priority = 2*NB_SIMULTANEOUS_TRANSFERS;        
        // activeTransfersNumber = 4,5,6 => prio = NB_SIMULTANEOUS_TRANSFERS
        if (activeTransfersNumber > 3 || activeTransfersNumber <= 6) priority = NB_SIMULTANEOUS_TRANSFERS;
        // activeTransfersNumber = 7,8 => prio = 1
        if (activeTransfersNumber > 6 ) priority = 1;
   
        // launching transfer thread
        if (!OSCreateThread(connection->transferThread, launchTransfer, 1, (char *)connection, (u32)connection->transferThreadStack + FTP_TRANSFER_STACK_SIZE, FTP_TRANSFER_STACK_SIZE, priority, cpu)) {
            display("! ERROR : when creating transferThread!");
            return -105;
        }

        OSResumeThread(connection->transferThread);

    } else {

/*     if (verboseMode) {
        display("C[%d] Transfer thread stack size = %d", connection->index+1, OSCheckThreadStackUsage(connection->transferThread));
        }
*/
        // join the thread here only in case of error or the transfer is finished 
        if (connection->bytesTransferred <= 0) {
            OSJoinThread(connection->transferThread, &result);
            if (verboseMode) {
                display("Transfer thread on C[%d] ended successfully", connection->index+1);
            }
        }
    }

    return result;
}

// this method is called on transfer success but also on transfer failure
static int32_t closeTransferredFile(connection_t *connection) {
    int32_t result = 0;

    activeTransfersNumber--;    
    
    if (verboseMode) {
        display("CloseTransferredFile for C[%d] (file=%s)", connection->index+1, connection->fileName);
    }

    // close file if needed
    if (connection->f != NULL) {
        fclose(connection->f);
    }	

    // if needed, free connection->volPath
    if (connection->volPath != NULL) free(connection->volPath);
    
    return result;
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
static int32_t write_reply(connection_t *connection, uint16_t code, char *msg) {
    uint32_t msglen = 4 + strlen(msg) + CRLF_LENGTH;

    char msgbuf[msglen + 1];
    sprintf(msgbuf, "%u %s\r\n", code, msg);
    if (verboseMode) display("> %s", msgbuf);

    return send_exact(connection->socket, msgbuf, msglen);
}

static void close_passive_socket(connection_t *connection) {
    if (connection->passive_socket >= 0) {
        network_close(connection->passive_socket);
        connection->passive_socket = -1;
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

static int32_t ftp_USER(connection_t *connection, char *username UNUSED) {
    return write_reply(connection, 331, "User name okay, need password");
}

static int32_t ftp_PASS(connection_t *connection, char *password_attempt) {
    if (compare_ftp_password(password_attempt)) {
        connection->authenticated = true;
        return write_reply(connection, 230, "User logged in, proceed");
    } else {
        return write_reply(connection, 530, "Login incorrect");
    }
}

static int32_t ftp_REIN(connection_t *connection, char *rest UNUSED) {
    close_passive_socket(connection);
    strcpy(connection->cwd, "/");
    connection->representation_type = 'A';
    connection->authenticated = false;
    return write_reply(connection, 220, "Service ready for new user");
}

static int32_t ftp_QUIT(connection_t *connection, char *rest UNUSED) {
    // TODO: dont quit if xfer in progress
    int32_t result = write_reply(connection, 221, "Service closing control connection");
    return result < 0 ? result : -EQUIT;
}

static int32_t ftp_SYST(connection_t *connection, char *rest UNUSED) {
    return write_reply(connection, 215, "UNIX Type: L8 Version: WiiUFtpServer");
}

static int32_t ftp_TYPE(connection_t *connection, char *rest) {
    char representation_type[FTP_MSG_BUFFER_SIZE] = "", param[FTP_MSG_BUFFER_SIZE] = "";
    char *args[] = { representation_type, param };
    uint32_t num_args = split(rest, ' ', 1, args);
    if (num_args == 0) {
        return write_reply(connection, 501, "Syntax error in parameters");
    } else if ((!strcasecmp("A", representation_type) && (!*param || !strcasecmp("N", param))) ||
               (!strcasecmp("I", representation_type) && num_args == 1)) {
        connection->representation_type = *representation_type;
    } else {
        return write_reply(connection, 501, "Syntax error in parameters");
    }
    char msg[FTP_MSG_BUFFER_SIZE+30] = "";
    sprintf(msg, "C[%d] Type set to %s", connection->index+1, representation_type);
    return write_reply(connection, 200, msg);
}

static int32_t ftp_MODE(connection_t *connection, char *rest) {
    if (!strcasecmp("S", rest)) {
        return write_reply(connection, 200, "Mode S ok");
    } else {
        return write_reply(connection, 501, "Syntax error in parameters");
    }
}


static void removeTrailingSlash(char **cwd) {
    if (strcmp(*cwd, ".") == 0) strcpy(*cwd, "/");
    else {
        if ( (strlen(*cwd) > 0) && (strcmp(*cwd, "/") != 0) ) {
            char *path = (char *)malloc (strlen(*cwd)+1);
            strcpy(path, *cwd);
            char *pos = strrchr(path, '/');
            if (strcmp(pos,"/") == 0) {
                // folder is allocated by strndup
                char *folder = strndup(*cwd, strlen(path)-strlen(pos));
                // update cwd
                strcpy (*cwd, folder);
                free(folder);
            }
            free(path);
        }
    }
}

// caller must free the returned string
static char* getLastItemOfPath(char *cwd) {
    char *final = NULL;
    if ( (strlen(cwd) > 0) && (strcmp(cwd, "/") != 0) ) {
        char *path = (char *)malloc (strlen(cwd)+1);
        strcpy(path, cwd);
        char *pos = strrchr(path, '/');
        // final is allocated by strdup
        final = strdup(pos+1);
        free(path);
    }
    return final;
}

// caller must free folder and fileName strings
static void secureAndSplitPath(char *cwd, char* path, char **folder, char **fileName) {

    *fileName = NULL;
    *folder = NULL;
    char *pos = NULL;
        
    if ( (strcmp(cwd, "/") == 0) && (path &&((strcmp(path, ".") == 0) || (strcmp(path, "/") == 0))) ) {
        
        // allocate and copy folder with cwd
        *folder = (char *)malloc (strlen(cwd)+1);
        strcpy(*folder, cwd);

        // allocate and copy fileName with .
        *fileName = (char *)malloc (strlen(path)+1);
        strcpy(*fileName, ".");        
        
    } else {

        char *cwdNoSlash = NULL;
        // allocate and copy fileName with path
        cwdNoSlash = (char *)malloc (strlen(cwd)+1);
        strcpy(cwdNoSlash, cwd);

        // remove any trailing slash when cwd != "/"
        removeTrailingSlash(&cwdNoSlash);

        // fix #10 : cyberduck support
        if (path) {
            // path is given

            // if first char is a slash
            if ((path[0] == '/') && (strlen(path) > 1)) {
                // path gives the whole full path

                // get the folder from path
                *fileName = getLastItemOfPath(path);
                pos = strrchr(path, '/');
                // folder is allocated by strndup
                *folder = strndup(path, strlen(path)-strlen(pos));
                
            } else {

                if (strlen(path) > 1) {

                    // path gives file's name

                    // check if cwd contains path (cyberduck)
                    if (strstr(cwdNoSlash, path) != NULL) {
                        // remove file's name from the path

                        // get the fileName from cwd
                        *fileName = getLastItemOfPath(cwdNoSlash);
                        pos = strrchr(cwdNoSlash, '/');
                        // folder is allocated by strndup
                        *folder = strndup(cwdNoSlash, strlen(cwdNoSlash)-strlen(pos));

                        // update cwd
                        strcpy(cwd, *folder);
                        strcat(cwd, "/");
                        
                    } else {

                        if ( strcmp(cwdNoSlash,"") != 0 ) {

                            // allocate and copy fileName with path
                            *fileName = (char *)malloc (strlen(path)+1);
                            strcpy(*fileName, path);
                            // allocate and copy folder with cwd
                            *folder = (char *)malloc (strlen(cwdNoSlash)+1);
                            strcpy(*folder, cwdNoSlash);
                            
                        } else {

                            // get the folder from path
                            *fileName = getLastItemOfPath(path);
                            pos = strrchr(path, '/');
                            // folder is allocated by strndup
                            *folder = strndup(path, strlen(path)-strlen(pos));

                            // update cwd
                            strcpy(cwd, *folder);
                            strcat(cwd, "/");
                            // update path
                            strcpy(path, *fileName);
                        }
                    }
                } else {
                    if (strlen(path) == 1) {
                        if (strcmp(path, "/") == 0) {
                            
                            *folder = (char *)malloc (2);
                            strcpy(*folder, "/");
                            *fileName = (char *)malloc (2);
                            strcpy(*fileName, ".");
                            
                        }
                        // else should be '.'
                    } else {
                    
                        // path is not given, cwd gives the whole full path
                        if (strcmp(cwd, "/") != 0) {
                        
                            // get path from cwd
                            path = getLastItemOfPath(cwdNoSlash);
                            *fileName = (char *)malloc (strlen(path)+1);
                            strcpy(*fileName, path);

                            pos = strrchr(cwdNoSlash, '/');
                            // folder is allocated by strndup
                            *folder = strndup(cwdNoSlash, strlen(cwdNoSlash)-strlen(pos));

                            // update cwd
                            strcpy(cwd, *folder);
                            strcat(cwd, "/");
                        }
                    }
                }
            }
            
        } else {
        
            // path is not given, cwd gives the whole full path
            if (strcmp(cwd, "/") != 0) {
                
                
                // path is not given, cwd gives the whole full path
                // get path from cwd
                path = getLastItemOfPath(cwdNoSlash);
                *fileName = (char *)malloc (strlen(path)+1);
                strcpy(*fileName, path);

                pos = strrchr(cwdNoSlash, '/');
                // folder is allocated by strndup
                *folder = strndup(cwdNoSlash, strlen(path)-strlen(pos));

                // update cwd
                strcpy(cwd, *folder);
                strcat(cwd, "/");
            }
        }
    }
    
}

static int32_t ftp_PWD(connection_t *connection, char *rest UNUSED) {
    char msg[MAXPATHLEN + 40] = "";

    if (verboseMode) {
        display("C[%d] ftp_PWD previous dir = %s", connection->index+1, connection->cwd);
    }

    // fix regression Cyberduck connection failure #15 
    // FTP client like cyberduck bugg get the path used for CWD (next command after PWD when login) from ftp_CWD's response
    // => the msg to be sent to FTP client must begin with connection->cwd !
    if (strrchr(connection->cwd, '"')) {
        sprintf(msg, "%s is current directory", connection->cwd);
    } else {
        sprintf(msg, "\"%s\" is current directory", connection->cwd);
    }
    
    if (strcmp(connection->cwd, "/") != 0) {
        // check if folder exist 
        char *parentFolder = NULL;
        parentFolder = (char *)malloc (strlen(connection->cwd)+1);
        strcpy(parentFolder, connection->cwd);
        char *pos = strrchr(parentFolder, '/');
        
        char *folder = NULL;
        folder = strdup (pos + 1);

        if (folder != NULL) {
            if (vrt_checkdir(parentFolder, folder) < 0) {
                display("! ERROR : C[%d] failed to PWD to %s (does not exist)", connection->index+1, connection->cwd);
            }
            free(folder);
        }
        if (verboseMode) {
            display("C[%d] ftp_PWD new dir = %s", connection->index+1, connection->cwd);
            display("%s", msg);
        }        
        if (parentFolder != NULL) free(parentFolder);
    }
    return write_reply(connection, 257, msg);

}

static int32_t ftp_CWD(connection_t *connection, char *path) {
    int32_t result = 0;

    if (verboseMode) {
        display("C[%d] ftp_CWD previous dir cwd=%s, path=%s", connection->index+1, connection->cwd, path);
    }

    char *folder = NULL;
    char *baseName = NULL;

    secureAndSplitPath(connection->cwd, path, &folder, &baseName);
    if (verboseMode) {
        display("C[%d] ftp_CWD folder=%s, baseName=%s", connection->index+1, folder, baseName);
    }
    
	strcpy(connection->cwd, folder);
	if (strcmp(connection->cwd, "/") != 0) strcat(connection->cwd, "/");


    if (baseName != NULL) {
        
        if (!vrt_chdir(connection->cwd, baseName)) {

            char msg[MAXPATHLEN + 60] = "";
            sprintf(msg, "C[%d] CWD successful to %s", connection->index+1, connection->cwd);
            write_reply(connection, 250, msg);
        } else  {
    //        display("~ WARNING : error in vrt_chdir in ftp_CWD : %s+%s", folder, baseName);
    //        display("~ WARNING : errno = %d (%s)", errno, strerror(errno));

            char msg[MAXPATHLEN + 40] = "";
            sprintf(msg, "Error when CWD to cwd=%s path=%s : err=%s", connection->cwd, baseName, strerror(errno));
            
            if (verboseMode) {
                display("%s", msg);
            }            
            write_reply(connection, 550, msg);
        }
        
        if (verboseMode) {
            display("C[%d] ftp_CWD new dir = %s", connection->index+1, connection->cwd);
        }

        free(baseName);
    }
    if (folder != NULL) free(folder);
    
    // always return 0 on server side
    // - when connection needs to create a folder tree on server side, connection try CWD until it do not fail before launching the MKD command)
    // - note that when ftp_CWD fails, an error is sent to the connection with the 550 error code
    return result;
}

static int32_t ftp_CDUP(connection_t *connection, char *rest UNUSED) {
    int32_t result;
    if (!vrt_chdir(connection->cwd, "..")) {
        char msg[MAXPATHLEN + 40] = "";
        sprintf(msg, "C[%d] CDUP command successful", connection->index+1);
        return write_reply(connection, 250, msg);
    } else  {
        display("! ERROR : error in vrt_chdir in ftp_CDUP : %s/..", connection->cwd);
        display("! ERROR : errno = %d (%s)", errno, strerror(errno));
        char msg[MAXPATHLEN + 40] = "";
        sprintf(msg, "Error when CDUP to %s : err = %s", connection->cwd, strerror(errno));
        result = write_reply(connection, 550, msg);
    }
    return result;
}

static int32_t ftp_DELE(connection_t *connection, char *path) {

    // compute volume path
    char vPath[2*MAXPATHLEN+1] = "";

    if (verboseMode) {
        display("C[%d] ftp_DELE cwd=%s, path=%s", connection->index+1, connection->cwd, path);
    }
        
    char *folder = NULL;
    char *baseName = NULL;

    secureAndSplitPath(connection->cwd, path, &folder, &baseName);
	strcpy(connection->cwd, folder);
	if (strcmp(connection->cwd, "/") != 0) strcat(connection->cwd, "/");

    if (verboseMode) {
        display("C[%d] ftp_DELE on %s in %s", connection->index+1, baseName, folder);
    }
    
    sprintf(vPath, "%s/%s", folder, baseName);
    
    char *volPath = NULL;
    volPath = virtualToVolPath(vPath);

    // chmod
    IOSUHAX_FSA_ChangeMode(fsaFd, volPath, 0x664);
    free(volPath);
    
    if (!vrt_unlink(connection->cwd, baseName)) {
        char msg[MAXPATHLEN + 40] = "";
        sprintf(msg, "C[%d] File or directory removed", connection->index+1);
        if (baseName != NULL) free(baseName);
        if (folder != NULL) free(folder);
        display("%s", msg);
        return write_reply(connection, 250, msg);
    } else {
        display("~ WARNING : error from vrt_unlink in ftp_DELE : %s", strerror(errno));
        char msg[MAXPATHLEN + 40] = "";
        sprintf(msg, "Error when DELE %s/%s : err = %s", connection->cwd, baseName, strerror(errno));
        if (baseName != NULL) free(baseName);
        if (folder != NULL) free(folder);
        
        return write_reply(connection, 550, msg);
    }
}

static int32_t ftp_RMD(connection_t *connection, char *path) {

    // compute volume path
    char vPath[2*MAXPATHLEN+1] = "";

    if (verboseMode) {
        display("C[%d] ftp_DELE cwd=%s, path=%s", connection->index+1, connection->cwd, path);
    }
        
    char *folder = NULL;
    char *baseName = NULL;

    secureAndSplitPath(connection->cwd, path, &folder, &baseName);
	strcpy(connection->cwd, folder);
	if (strcmp(connection->cwd, "/") != 0) strcat(connection->cwd, "/");

    if (verboseMode) {
        display("C[%d] ftp_RMD on %s in %s", connection->index+1, baseName, folder);
    }
        
    sprintf(vPath, "%s/%s", folder, baseName);
    
    char *volPath = NULL;
    volPath = virtualToVolPath(vPath);

    // chmod
    IOSUHAX_FSA_ChangeMode(fsaFd, volPath, 0x664);
    free(volPath);
    
    if (!vrt_unlink(connection->cwd, baseName)) {
        char msg[MAXPATHLEN + 40] = "";
        sprintf(msg, "C[%d] File or directory removed", connection->index+1);
        if (baseName != NULL) free(baseName);
        if (folder != NULL) free(folder);

        return write_reply(connection, 250, msg);
    } else {
        display("~ WARNING : error from vrt_unlink in ftp_RMD : %s", strerror(errno));
        char msg[MAXPATHLEN + 40] = "";
        sprintf(msg, "Error when DELE %s/%s : err = %s", connection->cwd, baseName, strerror(errno));
        if (baseName != NULL) free(baseName);
        if (folder != NULL) free(folder);
        
        return write_reply(connection, 550, msg);
    }
    
}

static int32_t ftp_MKD(connection_t *connection, char *path) {
    if (!*path) {
        return write_reply(connection, 501, "Syntax error in parameters");
    }

    if (verboseMode) {
        display("C[%d] ftp_MKD on %s in %s", connection->index+1, path, connection->cwd);
    }
    
    char *folder = NULL;
    char *baseName = NULL;

    secureAndSplitPath(connection->cwd, path, &folder, &baseName);    
	strcpy(connection->cwd, folder);
    if (strcmp(connection->cwd, "/") != 0) strcat(connection->cwd, "/");
    
    char msg[MAXPATHLEN + 60] = "";
	int msgCode = 550;

    if (vrt_checkdir(connection->cwd, baseName) >= 0) {
		msgCode = 257;
		strcpy(msg, "folder already exist");
        if (verboseMode) {
            display("ftp_MKD on C[%d] %s already exist", connection->index+1, baseName);
        }

    } else {

        if (!vrt_mkdir(connection->cwd, baseName, 0775)) {
            msgCode = 250;
            sprintf(msg, "directory %s created in %s", baseName, connection->cwd);

            if (verboseMode) {
                    display("ftp_MKD on C[%d] folder %s was created", connection->index+1, baseName);
                    display("ftp_MKD on C[%d] current dir = %s", connection->index+1, connection->cwd);
            }
        } else {
            display("! ERROR : error from vrt_mkdir in ftp_MKD : %s", strerror(errno));
            sprintf(msg, "Error in MKD when cd to cwd=%s, path=%s : err = %s", connection->cwd, baseName, strerror(errno));
            if (baseName != NULL) free(baseName);
            if (folder != NULL) free(folder);
    
            return write_reply(connection, msgCode, strerror(errno));
        }
    }

    if (baseName != NULL) free(baseName);
    if (folder != NULL) free(folder);
    return write_reply(connection, msgCode, msg);
}

static int32_t ftp_RNFR(connection_t *connection, char *path) {

    char *folder = NULL;
    char *fileName = NULL;

    secureAndSplitPath(connection->cwd, path, &folder, &fileName);
    strcpy(connection->fileName, fileName);
	strcpy(connection->cwd, folder);
    if (strcmp(connection->cwd, "/") != 0) strcat(connection->cwd, "/");

    strcpy(connection->pending_rename, fileName);
    char msg[MAXPATHLEN + 24] = "";
    sprintf(msg, "C[%d] Ready for RNTO for %s", connection->index+1, fileName);

    if (fileName != NULL) free(fileName);
    if (folder != NULL) free(folder);
	
    return write_reply(connection, 350, msg);
}

static int32_t ftp_RNTO(connection_t *connection, char *path) {
    char msg[MAXPATHLEN + 60] = "";

    if (!*connection->pending_rename) {
        sprintf(msg, "C[%d] RNFR required first", connection->index+1);
        return write_reply(connection, 503, msg);
    }
	
    int32_t result;
	
    char *folder = NULL;
    char *baseName = NULL;

    secureAndSplitPath(connection->cwd, path, &folder, &baseName);
	strcpy(connection->cwd, folder);
    if (strcmp(connection->cwd, "/") != 0) strcat(connection->cwd, "/");

    if (!vrt_rename(connection->cwd, connection->pending_rename, baseName)) {
        sprintf(msg, "C[%d] Rename %s to %s successfully", connection->index+1, connection->pending_rename, path);
        display("%s", msg);
        result = write_reply(connection, 250, msg);
    } else {
        sprintf(msg, "C[%d] failed to rename %s to %s : err = %s", connection->index+1, connection->pending_rename, path, strerror(errno));
        display("! ERROR : %s", msg);

        result = write_reply(connection, 550, msg);
    }
    *connection->pending_rename = '\0';
    
    if (baseName != NULL) free(baseName);
    if (folder != NULL) free(folder);
	
    return result;
}

static int32_t ftp_SIZE(connection_t *connection, char *path) {
    struct stat st;

    char *folder = NULL;
    char *fileName = NULL;

    secureAndSplitPath(connection->cwd, path, &folder, &fileName);
	strcpy(connection->cwd, folder);
    if (strcmp(connection->cwd, "/") != 0) strcat(connection->cwd, "/");
    
    FILE *f = vrt_fopen(connection->cwd, fileName, "rb");
    if (f) {
        fclose(f);
        int ret = 0;
        if ((ret = vrt_stat(connection->cwd, fileName, &st)) == 0) {
            char size_buf[12] = "";
            sprintf(size_buf, "%llu", st.st_size);
            if (fileName != NULL) free(fileName);
		    if (folder != NULL) free(folder);    
            return write_reply(connection, 213, size_buf);
        } else {
            display("! ERROR : C[%d], error from vrt_stat in ftp_SIZE, ret = %d", connection->index+1, ret);
            display("! C[%d], cwd=%s, path=%s", connection->index+1, connection->cwd, fileName);

            char msg[MAXPATHLEN + 40] = "";
            sprintf(msg, "Error SIZE on %s in %s : err=%s", fileName, connection->cwd, strerror(errno));
		    if (fileName != NULL) free(fileName);
		    if (folder != NULL) free(folder);
            return write_reply(connection, 550, msg);
        }
    } else {
        display("! ERROR : C[%d], error from vrt_stat in ftp_SIZE on %s in %s", connection->index+1, fileName, folder);
        display("! failed to open cwd=%s fileName=%s", connection->cwd, fileName);

        char msg[MAXPATHLEN + 40] = "";
        sprintf(msg, "Error SIZE on %s in %s : err=%s", fileName, connection->cwd, strerror(errno));
	    if (fileName != NULL) free(fileName);
	    if (folder != NULL) free(folder);
        return write_reply(connection, 550, msg);
    }
    if (fileName != NULL) free(fileName);
    if (folder != NULL) free(folder);
	
}

static int32_t ftp_PASV(connection_t *connection, char *rest UNUSED) {

    static const int retriesNumber = NB_SIMULTANEOUS_TRANSFERS;
    close_passive_socket(connection);
    
    int nbTries = 0;
    while (1)
    {
	
        connection->passive_socket = network_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (connection->passive_socket >= 0)
            break;

        if (++nbTries > retriesNumber) {
            char msg[FTP_MSG_BUFFER_SIZE];
            sprintf(msg, "C[%d] failed to create passive socket (%d), err = %d (%s)", connection->index+1, connection->passive_socket, errno, strerror(errno));
            display("~ WARNING : %s", msg);
            return write_reply(connection, 421, msg);
		}
			
	    // leave this sleep to avoid error on client console
//	    usleep((connection->index+1)*NB_SIMULTANEOUS_TRANSFERS*2*1000);
    }
    if (verboseMode) {
        display("C[%d] opening passive socket %d", connection->index+1, connection->passive_socket);
    }

    struct sockaddr_in bindAddress;
    memset(&bindAddress, 0, sizeof(bindAddress));
    bindAddress.sin_family = AF_INET;
    // reset passive_port to avoid overflow
    if (passive_port == 65534) {
        passive_port = 1024;
        if (verboseMode) {
                display("Passive port overflow !, reset to 1024");
        }
    }
    bindAddress.sin_port = htons(passive_port++);
    bindAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    
    nbTries = 0;
    while (1) {
        int32_t result;
        result = network_bind(connection->passive_socket, (struct sockaddr *)&bindAddress, sizeof(bindAddress));
        if (result >= 0)
            break;
            
        if (++nbTries > retriesNumber) {
            char msg[FTP_MSG_BUFFER_SIZE];
            sprintf(msg, "C[%d] failed to bind passive socket (%d), err = %d (%s)", connection->index+1, connection->passive_socket, errno, strerror(errno));
            display("~ WARNING : %s", msg);
            close_passive_socket(connection);
            return write_reply(connection, 421, msg);
        }
    }
    
    nbTries = 0;
    while (1) {
        int32_t result;
        result = network_listen(connection->passive_socket, 1);
        
        if (result >= 0)
            break;
            
        if (++nbTries > retriesNumber) {
            char msg[FTP_MSG_BUFFER_SIZE];
            sprintf(msg, "C[%d] failed to listen on passive socket (%d), err = %d (%s)", connection->index+1, connection->passive_socket, errno, strerror(errno));
            display("~ WARNING : %s", msg);
            close_passive_socket(connection);
            return write_reply(connection, 421, msg);
        }
    }
    char reply[49+2+16] = "";
    uint16_t port = bindAddress.sin_port;
    uint32_t ip = hostIpAddress;
	if (verboseMode) {
	    struct in_addr addr;
	    addr.s_addr = ip;
	    display("- Listening for data connections at %s : %lu...", inet_ntoa(addr), port);
	}
    sprintf(reply, "C[%d] entering in passive mode (%d,%d,%d,%d,%"PRIu16",%"PRIu16")", connection->index+1, (ip >> 24) & 0xff, (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff, (port >> 8) & 0xff, port & 0xff);
    return write_reply(connection, 227, reply);
}

static int32_t ftp_PORT(connection_t *connection, char *portspec) {
    uint32_t h1, h2, h3, h4, p1, p2;
    if (sscanf(portspec, "%3d,%3d,%3d,%3d,%3d,%3d", &h1, &h2, &h3, &h4, &p1, &p2) < 6) {

        return write_reply(connection, 501, "Syntax error in parameters");
    }
    char addr_str[44] = "";
    sprintf(addr_str, "%d.%d.%d.%d", h1, h2, h3, h4);
    struct in_addr sin_addr;
    if (!inet_aton(addr_str, &sin_addr)) {
        return write_reply(connection, 501, "Syntax error in parameters");
    }
    close_passive_socket(connection);

    uint16_t port = ((p1 &0xff) << 8) | (p2 & 0xff);
    connection->address.sin_addr = sin_addr;
    connection->address.sin_port = htons(port);
    if (verboseMode) display("- Sending server address to %s on %lu port", addr_str, port);
    return write_reply(connection, 200, "PORT command successful");
}

typedef int32_t (*data_connection_handler)(connection_t *connection, data_connection_callback callback, void *arg);

static int32_t prepare_data_connection_active(connection_t *connection, data_connection_callback callback UNUSED, void *arg UNUSED) {
    
    static const int retriesNumber = 4;
    int nbTries=0;
	
    while (1)
    {
        connection->data_socket = network_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (connection->data_socket >= 0)
            break;

        if (++nbTries > retriesNumber) {
            char msg[FTP_MSG_BUFFER_SIZE];
            sprintf(msg, "C[%d] failed to create data socket (%d), err = %d (%s)", connection->index+1, connection->data_socket, errno, strerror(errno));
            display("~ WARNING : %s", msg);
            return write_reply(connection, 421, msg);
		}
		
	    // leave this sleep to avoid error on client console
//	    usleep((connection->index+1)*NB_SIMULTANEOUS_TRANSFERS*2*1000);
    }
	

    if (verboseMode) {
        display("opening data socket = %d", connection->data_socket);
    }

    struct sockaddr_in bindAddress;
    memset(&bindAddress, 0, sizeof(bindAddress));
    bindAddress.sin_family = AF_INET;
    bindAddress.sin_port = htons(SRC_PORT);
    bindAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    
    nbTries = 0;
    while (1) {
        int32_t result;
        result = network_bind(connection->data_socket, (struct sockaddr *)&bindAddress, sizeof(bindAddress));
        if (result >= 0)
            break;
            
        if (++nbTries > retriesNumber) {
            char msg[FTP_MSG_BUFFER_SIZE];
            sprintf(msg, "C[%d] failed to bind data socket (%d), err = %d (%s)", connection->index+1, connection->data_socket, errno, strerror(errno));
            display("~ WARNING : %s", msg);
            network_close(connection->data_socket);
            return write_reply(connection, 421, msg);
        }
    }

    if (verboseMode) display("- Attempting to connect to connection through %s : %u", inet_ntoa(connection->address.sin_addr), connection->address.sin_port);
    return 0;
}

static int32_t prepare_data_connection_passive(connection_t *connection, data_connection_callback callback UNUSED, void *arg UNUSED) {
    connection->data_socket = connection->passive_socket;
    if (verboseMode) display("- Waiting for data connections...");
    return 0;
}

static int32_t prepare_data_connection(connection_t *connection, void *callback, void *arg, void *cleanup) {

    char msgStatus[MAXPATHLEN + 60] = "";
    sprintf(msgStatus, "C[%d] connected", connection->index+1);

    int32_t result = write_reply(connection, 150, msgStatus);
    if (result >= 0) {
        data_connection_handler handler = prepare_data_connection_active;
        if (connection->passive_socket >= 0) handler = prepare_data_connection_passive;
        result = handler(connection, (data_connection_callback)callback, arg);
        if (result < 0) {
            display("! ERROR : transfer handler failed , socket error = %d", result);
            display("! ERROR : error = %s", strerror(errno));
            display("! ERROR : file = %s", connection->fileName);
            char msg[MAXPATHLEN + 50] = "";
            sprintf(msg, "Closing C[%d], transfer failed (%s)", connection->index+1, connection->fileName);
            result = write_reply(connection, 520, msg);
        } else {
            connection->data_connection_connected = false;
            connection->data_callback = callback;
            connection->data_connection_callback_arg = arg;
            connection->data_connection_cleanup = cleanup;
            connection->data_connection_timer = OSGetTime() + (FTP_CONNECTION_TIMEOUT)*1000000;
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

    // compute dates intervals for checks
    // min t = J2012
    time_t min = minTime;
    // max t = timeOS(GMT)+24H
    time_t max = mktime(timeOs) + 86400.0;
        
    char fullPath[MAXPATHLEN] = "";
    char line[2*MAXPATHLEN + 56 + CRLF_LENGTH + 1];
    struct dirent *dirent = NULL;

    while ((dirent = vrt_readdir(iter)) != 0) {

        snprintf(fullPath, sizeof(fullPath), "%s/%s", iter->path, dirent->d_name);
        
        struct stat st;
         
        // initialize to session date
        struct tm *timeInfo = timeOs;
        uint64_t size = 0;
        
        // permissions for regular file (dim+1 needed)
        char permissions[10] = "rwxr-xr-x";

        // check the code returned for setting the modified date
        if (stat(fullPath, &st) == 0) {
            // modified time
            time_t mtime = st.st_mtime;
            
            // check modified date returned
            if (mtime > min && mtime < max) { 
                timeInfo = localtime(&mtime);
            }
            
            // size
            size = st.st_size;
            
        }
        
        // dim = 13
        char timestamp[13]="";
        strftime(timestamp, sizeof(timestamp), "%b %d  %Y", timeInfo);
        
        char fileType = 'd';
        if (dirent->d_type == DT_REG) fileType = '-';
        
        snprintf(line, sizeof(line), "%c%s    1 USER     WII-U %10llu %s %s\r\n", fileType, permissions, size, timestamp, dirent->d_name);
        if ((result = send_exact(data_socket, line, strlen(line))) < 0) {
            break;
        }
    }

    return result < 0 ? result : 0;
}

static int32_t ftp_NLST(connection_t *connection, char *path) {
    if (!*path) {
        path = ".";
    }

    DIR_P *dir = vrt_opendir(connection->cwd, path);
    if (dir == NULL) {
        display("! ERROR : error from vrt_opendir in ftp_NLST : %s", strerror(errno));

        char msg[MAXPATHLEN + 40] = "";
        sprintf(msg, "Error when NLIST cwd=%s path=%s : err = %s", connection->cwd, path, strerror(errno));
        return write_reply(connection, 550, msg);
    }
    
    int32_t result = prepare_data_connection(connection, send_nlst, dir, vrt_closedir);
    if (result < 0) {
        display("! ERROR : prepare_data_connection failed in ftp_NLST for %s", path);

        vrt_closedir(dir);
        if (result == -ENOMEM) {
            display("! ERROR : out of memory in prepare_data_connection");
        }
    }
    return result;

}

static int32_t ftp_LIST(connection_t *connection, char *path) {
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
    
    if (verboseMode) {
        display("C[%d] ftp_LIST : listing cwd=%s path=%s", connection->index+1, connection->cwd, path); 
    }            
    
    DIR_P *dir = vrt_opendir(connection->cwd, path);
    if (dir == NULL) {
        display("! ERROR : C[%d] vrt_opendir failed in ftp_LIST() on %s", connection->index+1, path);

        char msg[MAXPATHLEN + 40] = "";
        sprintf(msg, "Error when LIST cwd=%s path=%s : err = %s", connection->cwd, path, strerror(errno));
        return write_reply(connection, 550, msg);
    }

    int32_t result = prepare_data_connection(connection, send_list, dir, vrt_closedir);
    if (result < 0) {
        vrt_closedir(dir);
        if (result == -ENOMEM) {
            display("! ERROR : out of memory in ftp_LIST");
        }
    }
    return result;
}

static int32_t ftp_RETR(connection_t *connection, char *path) {

    char *folder = NULL;
    char *fileName = NULL;

    if (verboseMode) {
        display("C[%d] ftp_RETR cwd=%s, path=%s", connection->index+1, connection->cwd, path);
    }

    secureAndSplitPath(connection->cwd, path, &folder, &fileName);
    strcat(folder, "/");

    strcpy(connection->fileName, fileName);
	strcpy(connection->cwd, folder);
    
    if (fileName != NULL) free(fileName);
    if (folder != NULL) free(folder);

    display("> C[%d] sending %s...", connection->index+1, connection->fileName);
    
    connection->f = vrt_fopen(connection->cwd, path, "rb");
    if (!connection->f) {
        display("! ERROR : C[%d] ftp_RETR failed to open %s", connection->index+1, path);
        display("! ERROR : err = %s", strerror(errno));
        char msg[MAXPATHLEN + 40] = "";
        sprintf(msg, "Error when RETR cwd=%s path=%s : err=%s", connection->cwd, path, strerror(errno));
        return write_reply(connection, 550, msg);
    }     

    int fd = fileno(connection->f);
    if (connection->restart_marker && lseek(fd, connection->restart_marker, SEEK_SET) != connection->restart_marker) {
        int32_t lseek_error = errno;
        fclose(connection->f);
        connection->restart_marker = 0;
        
        return write_reply(connection, 550, strerror(lseek_error));
    }
    connection->volPath = NULL;
    
    int32_t result = prepare_data_connection(connection, transfer, connection, closeTransferredFile);

    if (result < 0) {
        if (result == -ENOMEM) {
            display("! ERROR : C[%d] prepare_data_connection failed in ftp_RETR for %s", connection->index+1, path);
            display("! ERROR : out of memory in ftp_RETR");
        }
        closeTransferredFile(connection);
    }

    return result;
}

static int32_t stor_or_append(connection_t *connection, char *path, char mode[3]) {

    // compute volume path
    char vPath[MAXPATHLEN*2] = "";
    char *folder = NULL;
    char *fileName = NULL;

    if (verboseMode) {
        display("C[%d] stor_or_append cwd=(%s), path=(%s)", connection->index+1, connection->cwd, path);
    }

    secureAndSplitPath(connection->cwd, path, &folder, &fileName);
    strcpy(connection->fileName, fileName);
	strcpy(connection->cwd, folder);
    if (strcmp(connection->cwd, "/") != 0) strcat(connection->cwd, "/");
    
    if (verboseMode) {
        display("C[%d] cwd=(%s), path=(%s)", connection->index+1, connection->cwd, path);
        display("C[%d] secured path folder=(%s), fileName=(%s)", connection->index+1, folder, fileName);
    }
    sprintf(vPath, "%s/%s", folder, fileName);
    connection->volPath = virtualToVolPath(vPath);

    if (verboseMode) {
        display("C[%d] vol path=(%s)", connection->index+1, connection->volPath);
    }

    char *folderName = getLastItemOfPath(folder);
    char *pos = strrchr (folder, '/');
    char *parentFolder = strndup(folder, strlen(folder)-strlen(pos)+1);

    if (verboseMode) {
        display("C[%d] check/create %s in %s", connection->index+1, folderName, parentFolder);
    }
    if (vrt_checkdir(parentFolder, folderName) < 0) {
        if (vrt_mkdir(parentFolder, folderName, 0775)!=0) {
            display("! ERROR : C[%d] error from stor_or_append when creating %s in %s", connection->index+1, folderName, parentFolder);
        }
    }

    if (fileName != NULL) free(fileName);
    if (folder != NULL) free(folder);
    if (parentFolder != NULL) free(parentFolder);
    if (folderName != NULL) free(folderName);    
    
	connection->f = vrt_fopen(connection->cwd, path, mode);
    if (!connection->f) {
        display("! ERROR : ftp_STOR failed to open %s", path);
        display("! ERROR : err = %s", strerror(errno));
        char msg[MAXPATHLEN + 40] = "";
        sprintf(msg, "Error storing cwd=%s path=%s : err=%s", connection->cwd, path, strerror(errno));

        return write_reply(connection, 550, msg);
    }
    
    display("> C[%d] receiving %s...", connection->index+1, connection->fileName);
    
    int32_t result = prepare_data_connection(connection, transfer, connection, closeTransferredFile);
    if (result < 0) {
        display("! ERROR : C[%d] prepare_data_connection failed in stor_or_append for %s", connection->index+1, path);
        if (result == -ENOMEM) {
            display("! ERROR : out of memory in stor_or_append");
        }
        closeTransferredFile(connection);
    }

    return result;
}

static int32_t ftp_STOR(connection_t *connection, char *path) {

    connection->restart_marker = 0;
    return stor_or_append(connection, path, "wb");
}

static int32_t ftp_APPE(connection_t *connection, char *path) {

    return stor_or_append(connection, path, "ab");
}

static int32_t ftp_REST(connection_t *connection, char *offset_str) {
    off_t offset;
    if (sscanf(offset_str, "%lli", &offset) < 1 || offset < 0) {
        return write_reply(connection, 501, "Syntax error in parameters");
    }
    connection->restart_marker = offset;

    display("> C[%d] restarting %s from %u...", connection->index+1, connection->fileName, offset);

    char msg[MAXPATHLEN+100] = "";
    sprintf(msg, "C[%d] restart position accepted (%lli) for %s", connection->index+1, offset, connection->fileName);
    return write_reply(connection, 350, msg);
}

static int32_t ftp_SITE_LOADER(connection_t *connection, char *rest UNUSED) {
    int32_t result = write_reply(connection, 200, "Exiting to loader");
    return result;
}

static int32_t ftp_SITE_CLEAR(connection_t *connection, char *rest UNUSED) {
    int32_t result = write_reply(connection, 200, "Cleared");
    uint32_t i;
    for (i = 0; i < 18; i++) display("\n");
    //display("\x1b[2;0H");
    return result;
}

/*
    This is implemented as a no-op to prevent some FTP clients
    from displaying skip/abort/retry type prompts.
*/
static int32_t ftp_SITE_CHMOD(connection_t *connection, char *rest UNUSED) {
    // compute virtual path /usb/... in a string allocate on the stack
    char vPath[MAXPATHLEN+1] = "";
    sprintf(vPath, "%s", connection->cwd);

    char *volPath = NULL;
    volPath = virtualToVolPath(vPath);

    // chmod
    int rc = IOSUHAX_FSA_ChangeMode(fsaFd, volPath, 0x664);
    if (rc < 0 && rc != -EAGAIN) {
        display("~ WARNING : C[%d] ftp_DELE error when settings file's rights, rc = %d !", connection->index+1, rc);
        display("~ WARNING : err = %d (%s)", errno, strerror(errno));
    }

    free(volPath);

    char msg[MAXPATHLEN+50] = "";
    sprintf(msg, "C[%d] CHMOD %s sucessfully", connection->index+1, connection->cwd);

    return write_reply(connection, 250, "SITE CHMOD command ok");
}

static int32_t ftp_SITE_PASSWD(connection_t *connection, char *new_password) {
    set_ftp_password(new_password);
    return write_reply(connection, 200, "Password changed");
}

static int32_t ftp_SITE_NOPASSWD(connection_t *connection, char *rest UNUSED) {
    set_ftp_password(NULL);
    return write_reply(connection, 200, "Authentication disabled");
}

static int32_t ftp_SITE_EJECT(connection_t *connection, char *rest UNUSED) {
    //if (dvd_eject()) return write_reply(connection, 550, "Unable to eject DVD");
    return write_reply(connection, 200, "DVD ejected");
}

static int32_t ftp_SITE_MOUNT(connection_t *connection, char *path UNUSED) {
    //if (!mount_virtual(path)) return write_reply(connection, 550, "Unable to mount");
    return write_reply(connection, 250, "Mounted");
}

static int32_t ftp_SITE_UNMOUNT(connection_t *connection, char *path UNUSED) {
    //if (!unmount_virtual(path)) return write_reply(connection, 550, "Unable to unmount");
    return write_reply(connection, 250, "Unmounted");
}

static int32_t ftp_SITE_UNKNOWN(connection_t *connection, char *rest UNUSED) {
    return write_reply(connection, 501, "Unknown SITE command");
}

static int32_t ftp_SITE_LOAD(connection_t *connection, char *path UNUSED) {
 //   FILE *f = vrt_fopen(connection->cwd, path, "rb");
 //   if (!f) return write_reply(connection, 550, strerror(errno));
 //   char *real_path = to_real_path(connection->cwd, path);
 //   if (!real_path) goto end;
 //   load_from_file(f, real_path);
 //   free(real_path);
 //   end:
 //   fclose(f);
    return write_reply(connection, 500, "Unable to load");
}

typedef int32_t (*ftp_command_handler)(connection_t *connection, char *args);

static int32_t dispatch_to_handler(connection_t *connection, char *cmd_line, const char **commands, const ftp_command_handler *handlers) {
    char cmd[FTP_MSG_BUFFER_SIZE] = "", rest[FTP_MSG_BUFFER_SIZE] = "";
    char *args[] = { cmd, rest };
    split(cmd_line, ' ', 1, args);
    
    int32_t i;
    for (i = 0; commands[i]; i++) {
        if (!strcasecmp(commands[i], cmd)) break;
    }
    return handlers[i](connection, rest);
}

static const char *site_commands[] = { "LOADER", "CLEAR", "CHMOD", "PASSWD", "NOPASSWD", "EJECT", "MOUNT", "UNMOUNT", "LOAD", NULL };
static const ftp_command_handler site_handlers[] = { ftp_SITE_LOADER, ftp_SITE_CLEAR, ftp_SITE_CHMOD, ftp_SITE_PASSWD, ftp_SITE_NOPASSWD, ftp_SITE_EJECT, ftp_SITE_MOUNT, ftp_SITE_UNMOUNT, ftp_SITE_LOAD, ftp_SITE_UNKNOWN };

static int32_t ftp_SITE(connection_t *connection, char *cmd_line) {
    return dispatch_to_handler(connection, cmd_line, site_commands, site_handlers);
}

static int32_t ftp_NOOP(connection_t *connection, char *rest UNUSED) {
    return write_reply(connection, 200, "NOOP command successful");
}

static int32_t ftp_SUPERFLUOUS(connection_t *connection, char *rest UNUSED) {
    return write_reply(connection, 202, "Command not implemented, superfluous at this site");
}

static int32_t ftp_NEEDAUTH(connection_t *connection, char *rest UNUSED) {
    char msg[FTP_MSG_BUFFER_SIZE+50] = "";
    sprintf(msg, "C[%d] Please login with USER and PASS", connection->index+1);

    return write_reply(connection, 530, msg);
}

static int32_t ftp_UNKNOWN(connection_t *connection, char *rest UNUSED) {
    return write_reply(connection, 502, "Command not implemented");
}

static int32_t ftp_MDTM(connection_t *connection, char *rest UNUSED) {
    return write_reply(connection, 202, "Command not implemented");
}

static int32_t ftp_XCRC(connection_t *connection, char *path UNUSED) {
    return write_reply(connection, 202, "Command not implemented");
}

static const char *unauthenticated_commands[] = { "USER", "PASS", "QUIT", "REIN", "NOOP", NULL };
static const ftp_command_handler unauthenticated_handlers[] = { ftp_USER, ftp_PASS, ftp_QUIT, ftp_REIN, ftp_NOOP, ftp_NEEDAUTH };

static const char *authenticated_commands[] = {
    "USER", "PASS", "LIST", "PWD", "CWD", "CDUP",
    "SIZE", "PASV", "PORT", "TYPE", "SYST", "MODE",
    "RETR", "STOR", "APPE", "REST", "DELE", "RMD", "MKD",
    "RNFR", "RNTO", "NLST", "QUIT", "REIN", "XCRC", "MDTM",
    "SITE", "NOOP", "ALLO", NULL
};
static const ftp_command_handler authenticated_handlers[] = {
    ftp_USER, ftp_PASS, ftp_LIST, ftp_PWD, ftp_CWD, ftp_CDUP,
    ftp_SIZE, ftp_PASV, ftp_PORT, ftp_TYPE, ftp_SYST, ftp_MODE,
    ftp_RETR, ftp_STOR, ftp_APPE, ftp_REST, ftp_DELE, ftp_RMD, ftp_MKD,
    ftp_RNFR, ftp_RNTO, ftp_NLST, ftp_QUIT, ftp_REIN, ftp_XCRC, ftp_MDTM,
    ftp_SITE, ftp_NOOP, ftp_SUPERFLUOUS, ftp_UNKNOWN
};

/*
    returns negative to signal an error that requires closing the connection
*/
static int32_t process_command(connection_t *connection, char *cmd_line) {
    if (strlen(cmd_line) == 0) {
        return 0;
    }

    if (verboseMode) display("< C[%d] %s", connection->index+1, cmd_line);

    const char **commands = unauthenticated_commands;
    const ftp_command_handler *handlers = unauthenticated_handlers;

    if (connection->authenticated) {
        commands = authenticated_commands;
        handlers = authenticated_handlers;
    }

    return dispatch_to_handler(connection, cmd_line, commands, handlers);
}

static void cleanup_data_resources(connection_t *connection) {

    // first close the data_socket
    if (connection->data_socket >= 0 && connection->data_socket != connection->passive_socket) {
        network_close(connection->data_socket);

        if (verboseMode) {
            display("C[%d] closing (data) socket %d", connection->index+1, connection->data_socket);
        }
    }
    connection->data_socket = -1;
    connection->data_connection_connected = false;
    connection->data_connection_timer = 0;
    connection->dataTransferOffset = -1;

    if (connection->data_connection_cleanup) {
        connection->data_connection_cleanup(connection->data_connection_callback_arg);
    }
    
    connection->data_callback = NULL;
    connection->data_connection_callback_arg = NULL;
    connection->data_connection_cleanup = NULL;
    connection->f = NULL;
}

static void displayTransferSpeedStats() {
    if (nbSpeedMeasures != 0) {
        display(" ");
        display("------------------------------------------------------------");
        display("  Speed (MB/s) [min = %.2f, mean = %.2f, max = %.2f]", minTransferRate, sumAvgSpeed/(float)nbSpeedMeasures, maxTransferRate);
    }
}

static void cleanup_connection(connection_t *connection) {

    if (verboseMode) {
        display("cleanup C[%d]", connection->index);
    }

    network_close(connection->socket);
    cleanup_data_resources(connection);
    // volontary set to -EAGAIN here and not in cleanup_data_resources for msg 226 to connection
    connection->bytesTransferred = -EAGAIN;
    close_passive_socket(connection);

    resetConnection(connection);
    connection->socket =-1;

    if (activeConnectionsNumber >= 1) activeConnectionsNumber--;
    
    // if only a browse connection is active
    if (activeConnectionsNumber == 1) {
        if (activeTransfersNumber == 0) {
            if ((!network_down) && (lastSumAvgSpeed != sumAvgSpeed)) {
                displayTransferSpeedStats();
                lastSumAvgSpeed = sumAvgSpeed;
            }
        }
    }

}


static connection_t* getFirstConnectionAvailable() {

    uint32_t connection_index;

    for (connection_index = 0; connection_index < FTP_NB_SIMULTANEOUS_TRANSFERS; connection_index++) {
        if (connections[connection_index]->socket != -1) return connections[connection_index];
    }
    return NULL;
}

void cleanup_ftp() {

    if (listener != -1) {

        if (verboseMode) {
            display("Entering in cleanup_ftp()");
            
            display("Try to warn client from closing connections");
        }
        connection_t *firstAvailable = getFirstConnectionAvailable();
        if (firstAvailable != NULL) write_reply(firstAvailable, 421, "Closing remaining active connections connection");

        if (verboseMode) {
            display("Loop on opened connections");
        }
        uint32_t connection_index;
        for (connection_index = 0; connection_index < FTP_NB_SIMULTANEOUS_TRANSFERS; connection_index++) {
            connection_t *connection = connections[connection_index];
            cleanup_connection(connection);
            
            // free transfer buffer if needed
            if (connection->transferBuffer != NULL) free(connection->transferBuffer);
            // free transfer threads if needed
            if (connection->transferThread != NULL) OSFreeToSystem(connection->transferThread);
            free(connection);
            
            // display only if a client was connected
            if (strcmp(clientIp, "UNKNOWN_CLIENT") != 0) display("- %s connection C[%d] closed", clientIp, connection_index+1);        
        }
        if (password != NULL) free(password);  

        if (lastSumAvgSpeed != sumAvgSpeed) displayTransferSpeedStats();
    }
    
}

static bool processConnections() {
    
	
    // if the max connections number is not reached, treat incomming connections
    if (activeConnectionsNumber < FTP_NB_SIMULTANEOUS_TRANSFERS) {

        int32_t peer;
        struct sockaddr_in client_address;
        int32_t addrlen = sizeof(client_address);

        while ((peer = network_accept(listener, (struct sockaddr *)&client_address, &addrlen)) != -EAGAIN) {
            if (peer < 0) {
                char msg[FTP_MSG_BUFFER_SIZE];
                sprintf(msg, "Error accepting connection: err=%d (%s)", -peer, strerror(-peer));
                display("! ERROR : %s", msg);
                sleep(5);
                connection_t *last = getFirstConnectionAvailable();
                if (last) return write_reply(last, 520, msg);

                // if cannot inform client, stop server
                return false;
            }

            if (strcmp(clientIp, "UNKNOWN_CLIENT") == 0) strcpy(clientIp,inet_ntoa(client_address.sin_addr));
            if (strcmp(clientIp, inet_ntoa(client_address.sin_addr)) != 0 ) {
                
                display("~ WARNING : Sorry %s, %s is already connected ! close all his connections first !", inet_ntoa(client_address.sin_addr), clientIp);
                network_close(peer);
            } else {

                resetConnection(connections[activeConnectionsNumber]);
                connections[activeConnectionsNumber]->socket = peer;
                memcpy(&connections[activeConnectionsNumber]->address, &client_address, sizeof(client_address));

                if (write_reply(connections[activeConnectionsNumber], 220, "---------====={ WiiUFtpServer }=====---------") < 0) {
                    display("! ERROR : Error writing greeting");
                    network_close(peer);
                    connections[activeConnectionsNumber]->socket = -1;
					return false;
                } else {
                    if (verboseMode) {
                        display("Greetings sent sucessfully");
                    }

                    activeConnectionsNumber++;

            		display("- %s use connection C[%d]", clientIp, activeConnectionsNumber);
//                            if (verboseMode) {
//                                displayConnectionDetails(connection_index);
//                            }
                    return true;
                }
            }
        }
    }
    return true;
}

static void process_data_events(connection_t *connection) {
    int32_t result;

    if (!connection->data_connection_connected) {

        if (connection->passive_socket >= 0) {
			if (verboseMode) {
			    display("C[%d] using passive_socket (%d)", connection->index+1, connection->passive_socket);
			}
            struct sockaddr_in data_peer_address;
            int32_t addrlen = sizeof(data_peer_address);

            result = network_accept(connection->passive_socket, (struct sockaddr *)&data_peer_address ,&addrlen);
            if (result >= 0) {
                connection->data_socket = result;
                connection->data_connection_connected = true;
                if (result > 0) return;
            } else {
				if (result != -EAGAIN) {
	                char msg[FTP_MSG_BUFFER_SIZE];
	                sprintf(msg, "Error accepting C[%d] %d (%s)",  connection->index+1, errno, strerror(errno));
	                display("~ WARNING : %s", msg);
	                write_reply(connection, 550, msg);
				}
            }

        } else {

			if (verboseMode) {
			    display("C[%d] using data_socket (%d) for transferring %s", connection->index+1, connection->data_socket, connection->fileName);
			}
            // retry 2 times if can't connect before exiting
            int nbTries=0;
            try_again:
            if ((result = network_connect(connection->data_socket, (struct sockaddr *)&connection->address, sizeof(connection->address))) < 0) {
                if (result == -EINPROGRESS || result == -EALREADY) {
                    nbTries++;
                    if (nbTries <= 2) goto try_again;
                    // no need to set to -EAGAIN, exit
                    return;
                }

                if ((result != -EAGAIN) && (result != -EISCONN))
                {
                    display("! ERROR : C[%d] unable to connect to client: rc=%d, err=%s", connection->index+1, -result, strerror(-result));
                }
            }
            if (result >= 0 || result == -EISCONN) {
                connection->data_connection_connected = true;
                if (verboseMode) {
                    display("Opened connections = %d / %d", activeConnectionsNumber, NB_SIMULTANEOUS_TRANSFERS);
                }
                if (result > 0) return;
            }
        }

        if (connection->data_connection_connected) {
            return;
        } else if (OSGetTime() > connection->data_connection_timer && connection->dataTransferOffset == 0) {
            // do not timeout anymore once the connection was estblished a first time
            result = -99;
            char msg[MAXPATHLEN] = "";
            sprintf(msg, "C[%d] timed out when connecting", connection->index+1);
            display("~ WARNING : %s", msg);
            write_reply(connection, 520, msg);
        }
        // here result = 1 or -99

    } else {

        result = connection->data_callback(connection->data_socket, connection->data_connection_callback_arg);
        // file transfer finished
        if (connection->bytesTransferred != -EAGAIN && result == 0 && connection->dataTransferOffset > 0) {

            // compute transfer speed
            uint64_t duration = (OSGetTime() - (connection->data_connection_timer - FTP_CONNECTION_TIMEOUT*1000000)) * 4000ULL / BUS_SPEED;
            if (duration != 0) {

                // set a threshold on file size to consider file for average calculation
                // take only files larger than the network buffer used

                if (connection->dataTransferOffset >= 2*SOCKET_BUFFER_SIZE) {
                    connection->speed = (float)(connection->dataTransferOffset) / (float)(duration*1000);

                    if (connection->volPath == NULL)
                        display("> C[%d] %s sent at %.2f MB/s (%d bytes)", connection->index+1, connection->fileName, connection->speed, connection->dataTransferOffset);
                    else
                        display("> C[%d] %s received at %.2f MB/s (%d bytes)", connection->index+1, connection->fileName, connection->speed, connection->dataTransferOffset);

                }
            }
        }

        if (verboseMode) {
            if (result != -EAGAIN) display("C[%d] data_callback using socket %d returned %d", connection->index+1, connection->data_socket, result);
        }

        // check errors
        if (result < 0 && result != -EAGAIN) {
            display("! ERROR : C[%d] data transfer callback using socket %d failed , socket error = %d", connection->index+1, connection->data_socket, result);
        }
    }

    if (result <= 0 && result != -EAGAIN) {
        cleanup_data_resources(connection);
        if (result < 0) {
            if (result != -99) {
                char msg[MAXPATHLEN] = "";
                sprintf(msg, "C[%d] to be closed : error = %d (%s)", connection->index+1, result, strerror(result));
                display("! ERROR : %s", msg);
                write_reply(connection, 520, msg);
            }
        } else {

            char msg[MAXPATHLEN + 80] = "";
            // usually set to -EAGAIN when not transferring a file
            if (connection->bytesTransferred == 0) {
                if (connection->speed != 0)
                    sprintf(msg, "C[%d] %s Transferred sucessfully %.0fKB/s", connection->index+1, connection->fileName, connection->speed*1000);
                else
                    sprintf(msg, "C[%d] %s Transferred sucessfully", connection->index+1, connection->fileName);
            }
            else
                sprintf(msg, "C[%d] command executed sucessfully", connection->index+1);

            write_reply(connection, 226, msg);
        }

        if (result < 0) {
            cleanup_connection(connection);
        }
    }
}

static void process_control_events(connection_t *connection) {
    int32_t bytes_read;
    while (connection->offset < (FTP_MSG_BUFFER_SIZE - 1)) {
        if (connection->data_callback) {
            return;
        }
        char *offset_buf = connection->buf + connection->offset;
        if ((bytes_read = network_read(connection->socket, offset_buf, FTP_MSG_BUFFER_SIZE - 1 - connection->offset)) < 0) {
            if (bytes_read != -EAGAIN) {
                display("! ERROR : C[%d] read error %i occurred, closing connection", connection->index+1, bytes_read);
                goto recv_loop_end;
            }
            return;
        } else if (bytes_read == 0) {
            goto recv_loop_end; // EOF from client
        }
        connection->offset += bytes_read;
        connection->buf[connection->offset] = '\0';

        if (strchr(offset_buf, '\0') != (connection->buf + connection->offset)) {
            display("~ WARNING : C[%d] Received a null byte from client, closing connection ;-)", connection->index+1); // i have decided this isn't allowed =P
            goto recv_loop_end;
        }

        char *next;
        char *end;
        for (next = connection->buf; (end = strstr(next, CRLF)) && !connection->data_callback; next = end + CRLF_LENGTH) {
            *end = '\0';
            if (strchr(next, '\n')) {
                display("~ WARNING : C[%d] a line-feed from client without preceding carriage return, closing connection ;-)", connection->index+1); // i have decided this isn't allowed =P
                goto recv_loop_end;
            }

            if (*next) {
                int32_t result;
                if ((result = process_command(connection, next)) < 0) {
                    if (result != -EQUIT) {
                        display("! ERROR : C[%d] closed due to error while processing command: %s", connection->index+1, next);
                    }
                    goto recv_loop_end;
                }
            }

        }

        if (next != connection->buf) { // some lines were processed
            connection->offset = strlen(next);
            char tmp_buf[connection->offset];
            memcpy(tmp_buf, next, connection->offset);
            memcpy(connection->buf, tmp_buf, connection->offset);
        }
    }
    display("! ERROR : C[%d] close because of a line longer than %lu bytes", connection->index+1, FTP_MSG_BUFFER_SIZE - 1);

    recv_loop_end:
    cleanup_connection(connection);
}

bool process_ftp_events() {

    network_down = !processConnections();

    if (!network_down) {
        uint32_t connection_index;
        float totalSpeedMBs = 0;

        for (connection_index = 0; connection_index < FTP_NB_SIMULTANEOUS_TRANSFERS; connection_index++) {
            // if connection is in use
            if (connections[connection_index]->socket != -1) {
                if (connections[connection_index]->data_callback) {
                    process_data_events(connections[connection_index]);
                    // total is the sum of speed computed for each connection
                    if (connections[connection_index]->speed) totalSpeedMBs += connections[connection_index]->speed;
                } else {
                    process_control_events(connections[connection_index]);
                }
            }
        }
        if (totalSpeedMBs) {
            // increment nbAvgFiles and take speed into account for mean calculation
            nbSpeedMeasures += 1;
            
            // fix double rate errors
            if (totalSpeedMBs > 8.0) totalSpeedMBs = totalSpeedMBs / 2.0; 
            
            if (totalSpeedMBs > maxTransferRate) maxTransferRate = totalSpeedMBs;
            if (totalSpeedMBs < minTransferRate) minTransferRate = totalSpeedMBs;
            sumAvgSpeed += totalSpeedMBs;
        }
	}

    return network_down;
}

void setOsTime(struct tm *tmTime) {
    if (!timeOs) timeOs=tmTime;
}

uint32_t getActiveTransfersNumber() {
    return activeTransfersNumber;
}    