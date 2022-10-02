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
/****************************************************************************
  * WiiUFtpServer
  * 2021-12-05:Laf111:V7-1: complete some TODO left, fix upload file corruption
 ***************************************************************************/
#include <coreinit/filesystem.h>
#include <coreinit/memory.h>
#include <sys/stat.h>

#include "ftp.h"
#include "virtualpath.h"
#include "net.h"
#include "vrt.h"
#include "spinlock.h"

#define UNUSED    __attribute__((unused))

// used to compute transfer rate
#define BUS_SPEED 248625000

// FS global vars
extern FSClient *__wut_devoptab_fs_client;

extern bool verboseMode;
extern bool calculateCrc32;

extern void display(const char *fmt, ...);
extern void writeCRC32(const char way, const char *cwd, const char *name, const int crc32);
#ifdef LOG2FILE
    extern void writeToLog(const char *fmt, ...);
    int nbDataSocketsOpened = 0;
#endif

// FS static vars
static FSCmdBlock cmdBlk;

static bool network_down = false;
static const uint16_t SRC_PORT = 20;
static const int32_t EQUIT = 696969;
static const char *CRLF = "\r\n";
static const uint32_t CRLF_LENGTH = 2;

// number of active connections
static uint32_t activeConnectionsNumber = 0;
// number of active transfers
static uint32_t activeTransfersNumber = 0;
// lock to limit to one access at a time for the display method
static spinlock sdUpLock = false;
static uint32_t activeUploadsToSdCard = 0;
static const uint32_t maxUploadsOnSdCard = 8;

// unique client IP address
static char clientIp[15]="UNKNOWN_CLIENT";

// passive_port : 1024 - 65535
static uint16_t passive_port = 1024;
static char *password = NULL;

// OS time computed in main
static struct tm *timeOs=NULL;
// OS time creation date = 31/12/2009
static const time_t minTime = 1262214000;

static connection_t connections[FTP_NB_SIMULTANEOUS_TRANSFERS];

static int listener = -1;     // listening socket descriptor

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
    strcpy(connection->uploadFilePath, "");
    connection->f = NULL;
    connection->dataTransferOffset = -1;
    connection->speed = 0;
    connection->bytesTransferred = -EAGAIN;
    connection->crc32 = 0;

    
}

// initialize the connection without setting the index data member
static void initConnection(connection_t *connection) {
    
    resetConnection(connection);
    connection->socket = -1;
    

    // allocate the buffer for transfering on the custom heap 
    int32_t buf_size = TRANSFER_BUFFER_SIZE+32;

    // align memory (64bytes = 0x40) when alocating the buffer
    do{
        buf_size -= 32;
        if (buf_size < 0) {
			display("! ERROR when allocating buffer");
            return;
        }
        connection->transferBuffer = (char *)memalign(64, buf_size);
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

    uint32_t ip = network_gethostip();
    
	struct in_addr addr;
	addr.s_addr = ip;
	
    if (strcmp(inet_ntoa(addr),"0.0.0.0") != 0) {

	    char ipText[28 + 15 + 2 + 6 + 2 + 2]; // 28 chars for the text + 15 chars for max length of an IP + 2 for size of port + 6 additional spaces + 2x@ + '\0'
	    sprintf(ipText, "    @ Server IP adress = %u.%u.%u.%u ,port = %u", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF, port);
	    size_t ipSize = strlen(ipText);
	    if(ipSize < 28 + 15 + 2 + 5 + 1)
	        OSBlockSet(ipText + ipSize, ' ', (28 + 15 + 2 + 6 + 1) - ipSize);

	    strcpy(ipText + (28 + 15 + 2 + 5 + 1), " @");

	    display(" ");
	    display("    @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
	    display(ipText);
	    display("    @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
	    uint32_t connection_index;
    
	    for (connection_index = 0; connection_index < FTP_NB_SIMULTANEOUS_TRANSFERS; connection_index++) {
					
		    connections[connection_index].index = connection_index;
		
			initConnection(&connections[connection_index]);
	    }
	}
	
    return listener;
}

#ifdef LOG2FILE
void displayConnectionDetails(uint32_t index) {

    writeToLog("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
    writeToLog("Connection %d ", index);
    writeToLog("------------------------------");

    if (index == 0) {
        writeToLog("socket                     = %d", connections[index]->socket);
        writeToLog("representation_type        = %c", connections[index]->representation_type);
        writeToLog("pending_rename             = %c", connections[index]->pending_rename);
        writeToLog("authenticated              = %d", connections[index]->authenticated);
    }

    writeToLog("cwd                        = %s", connections[index]->cwd);
    writeToLog("offset                     = %d", connections[index]->offset);
    writeToLog("passive_socket             = %d", connections[index]->passive_socket);

    writeToLog("data_socket                = %d", connections[index]->data_socket);
    writeToLog("restart_marker             = %d", connections[index]->restart_marker);
    writeToLog("data_connection_connected  = %d", connections[index]->data_connection_connected);
    writeToLog("data_connection_timer      = %d", connections[index]->data_connection_timer);

    if (connections[index]->f != NULL) {
        writeToLog("filename                   = %s", connections[index]->fileName);
        writeToLog("fd                         = %d", fileno(connections[index]->f));
        writeToLog("filePath                   = %s", connections[index]->uploadFilePath);
        writeToLog("dataTransferOffset         = %d", connections[index]->dataTransferOffset);
        writeToLog("speed                      = %.2f", connections[index]->speed);
        writeToLog("bytesTransferred           = %d", connections[index]->bytesTransferred);

    }

    writeToLog("Number of avg files used   = %d", nbSpeedMeasures);
    writeToLog("Sum of speed rate          = %.2f", sumAvgSpeed);

    writeToLog("Max transfer speed         = %.2f", maxTransferRate);
    writeToLog("Min transfer speed         = %.2f", minTransferRate);

}
#endif

int launchTransfer(int argc UNUSED, const char **argv)
{
        
    int32_t result = -101;
    connection_t* activeConnection = (connection_t*) argv;

    #ifdef LOG2FILE
        writeToLog("C[%d] launching transfer for %s", activeConnection.index+1, activeConnection.fileName);
    #endif

    
    if (strlen(activeConnection->uploadFilePath) == 0) {
        result = send_from_file(activeConnection->data_socket, activeConnection);
        
    } else {
        
        result = recv_to_file(activeConnection->data_socket, activeConnection);

    }
    // set a priority to 1 to priorize the termination of the thread
    OSSetThreadPriority(&activeConnection->transferThread, 1);
    return result;
}

// launch and monitor the transfer
static int32_t transfer(int32_t data_socket UNUSED, connection_t *connection) {
    int32_t result = -EAGAIN;

    // on the very first call
    if (connection->dataTransferOffset == -1) {
        
        // hard limit simultaneous uploads on SDCard
        if ((strlen(connection->uploadFilePath) != 0) && (strstr(connection->cwd, "/sd/") != NULL)) {
            if (activeUploadsToSdCard >= maxUploadsOnSdCard) return -EAGAIN;
            spinLock(sdUpLock);
            activeUploadsToSdCard++;
            spinReleaseLock(sdUpLock);
        }
        activeTransfersNumber++;

        // init bytes counter
        connection->dataTransferOffset = 0;

        // init speed to 0
        connection->speed = 0;

		#ifdef LOG2FILE
		    writeToLog("Using data_socket (%d) of C[%d] to transfer %s", data_socket, connection->index+1, connection->fileName);
		    displayConnectionDetails(connection->index);
		#endif
        
    
        // Dispatching connections over CPUs :
        
        // connection->index+1 = 1,4,7 => CPU2
        enum OS_THREAD_ATTRIB cpu = OS_THREAD_ATTRIB_AFFINITY_CPU2;                
        // connection->index+1 = 2,5,8 => CPU0
        if (connection->index == 1 || connection->index == 4 || connection->index == 7)  cpu = OS_THREAD_ATTRIB_AFFINITY_CPU0;
        // connection->index+1 = 3,6   => CPU1
        if (connection->index == 2 || connection->index == 5) cpu = OS_THREAD_ATTRIB_AFFINITY_CPU1;
    

        // set thread priority : 
    
        // activeTransfersNumber <= 3 : prio = 8
        int priority = 8;
        // activeTransfersNumber > 3 & activeTransfersNumber < 7 : prio = 4
        if (activeTransfersNumber > 3 && activeTransfersNumber < 7) priority = 4;
        // activeTransfersNumber > 6 : prio = 2
        if (activeTransfersNumber > 6 ) priority = 2;
   
        // launching transfer thread
        if (!OSCreateThread(&connection->transferThread, launchTransfer, 1, (char *)connection, connection->transferThreadStack + FTP_TRANSFER_STACK_SIZE, FTP_TRANSFER_STACK_SIZE, priority, cpu)) {
            display("! ERROR : when launching transferThread with priority = %u", priority);
            return -105;
        }

        #ifdef LOG2FILE
            OSSetThreadStackUsage(&connection->transferThread);
        #endif

        OSResumeThread(&connection->transferThread);

    } else {

/*     #ifdef LOG2FILE
        writeToLog("C[%d] Transfer thread stack size = %d", connection->index+1, OSCheckThreadStackUsage(&connection->transferThread));
    #endif 
*/
        // join the thread here only in case of error or the transfer is finished 
        if (connection->bytesTransferred <= 0) {
            OSJoinThread(&connection->transferThread, &result);
            #ifdef LOG2FILE
                writeToLog("Transfer thread on C[%d] ended successfully", connection->index+1);
            #endif
        }
    }

    return result;
}

// this method is called on transfer success but also on transfer failure
static int32_t closeTransferredFile(connection_t *connection) {
    int32_t result = 0;

    activeTransfersNumber--;    
    
    
    // hard limit simultaneous transfers on SDCard
    if ((strlen(connection->uploadFilePath) != 0) && (strstr(connection->cwd, "/sd/") != NULL)) {
        spinLock(sdUpLock);
        activeUploadsToSdCard--;            
        spinReleaseLock(sdUpLock);
    }
    #ifdef LOG2FILE
        writeToLog("CloseTransferredFile for C[%d] (file=%s)", connection->index+1, connection->fileName);
    #endif


    // close file if needed
    if (connection->f != NULL) {
        fclose(connection->f);
    }	   
    // on success, if crc32 calc is enabled
    if (connection->bytesTransferred == 0 && calculateCrc32) {
        
        if (strlen(connection->uploadFilePath) != 0) {
             // for all files except WiiUFtpServer_crc32_report.sfv
            if ( strcmp(connection->fileName, "WiiUFtpServer_crc32_report.sfv") != 0 ) {
                
                #ifdef LOG2FILE
                    display("C[%d] writing CRC32 after uploading %s", connection->index+1, connection->fileName);
                #endif
                
                // add the CRC32 value to the SFV file
                writeCRC32('<', connection->cwd, connection->fileName, connection->crc32);
            }
        } else {
            // for all files except WiiUFtpServer_crc32_report.sfv
            if ( strcmp(connection->fileName, "WiiUFtpServer_crc32_report.sfv") != 0 ) {
                
                #ifdef LOG2FILE
                    display("C[%d] writing CRC32 after downloading %s", connection->index+1, connection->fileName);
                #endif
                
                // add the CRC32 value to the SFV file
                writeCRC32('>', connection->cwd, connection->fileName, connection->crc32);
            }
        }
    }
    
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
    
    #ifdef LOG2FILE
        writeToLog("secureAndSplitPath cwd=%s", cwd);
        writeToLog("secureAndSplitPath path=%s", path);
    #endif
    
    if ( (strcmp(cwd, "/") == 0) && (path &&((strcmp(path, ".") == 0) || (strcmp(path, "/") == 0))) ) {
        
        // allocate and copy folder with cwd
        *folder = (char *)malloc (strlen(cwd)+1);
        strcpy(*folder, cwd);

        // allocate and copy fileName with .
        *fileName = (char *)malloc (strlen(path)+1);
        strcpy(*fileName, ".");        
        
    #ifdef LOG2FILE
        writeToLog("secureAndSplitPath folder0=%s fileName0=%s", *folder, *fileName);
    #endif
        
    } else {

        char *cwdNoSlash = NULL;
        // allocate and copy fileName with path
        cwdNoSlash = (char *)malloc (strlen(cwd)+1);
        strcpy(cwdNoSlash, cwd);

        // remove any trailing slash when cwd != "/"
        removeTrailingSlash(&cwdNoSlash);

    #ifdef LOG2FILE
        writeToLog("secureAndSplitPath cwd=%s, cwdNoSlash=%s", cwd, cwdNoSlash);
    #endif
        
        // fix #10 : cyberduck support
        if (path) {
            // path is given

    #ifdef LOG2FILE
            writeToLog("secureAndSplitPath path given=%s", path);
    #endif

            // if first char is a slash
            if ((path[0] == '/') && (strlen(path) > 1)) {
                // path gives the whole full path

                // get the folder from path
                *fileName = getLastItemOfPath(path);
                pos = strrchr(path, '/');
                // folder is allocated by strndup
                *folder = strndup(path, strlen(path)-strlen(pos));

    #ifdef LOG2FILE
                writeToLog("secureAndSplitPath fileName1=%s", *fileName);
    #endif
                
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
                        
    #ifdef LOG2FILE
                        writeToLog("secureAndSplitPath fileName2=%s", *fileName);
    #endif
                    } else {

                        if ( strcmp(cwdNoSlash,"") != 0 ) {

                            // allocate and copy fileName with path
                            *fileName = (char *)malloc (strlen(path)+1);
                            strcpy(*fileName, path);
                            // allocate and copy folder with cwd
                            *folder = (char *)malloc (strlen(cwdNoSlash)+1);
                            strcpy(*folder, cwdNoSlash);

    #ifdef LOG2FILE
                            writeToLog("secureAndSplitPath fileName3=%s", *fileName);
    #endif
                            
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
    #ifdef LOG2FILE
                            writeToLog("secureAndSplitPath fileName4=%s", *fileName);
    #endif
                        }
                    }
                } else {
                    if (strlen(path) == 1) {
                        if (strcmp(path, "/") == 0) {
                            
                            *folder = (char *)malloc (2);
                            strcpy(*folder, "/");
                            *fileName = (char *)malloc (2);
                            strcpy(*fileName, ".");
                            
                        } else {
                            if (strcmp(path, ".") != 0) {
                                *folder = (char *)malloc (2);
                                strcpy(*folder, cwd);
                                *fileName = (char *)malloc (2);
                                strcpy(*fileName, path);    
                        	}
                        }    
                        // else should be '.'
                    } else {
                    
                        // path is not given, cwd gives the whole full path
                        if (strcmp(cwd, "/") != 0) {

        #ifdef LOG2FILE
                            writeToLog("secureAndSplitPath cwd=%s", cwd);
        #endif
                        
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
                            
        #ifdef LOG2FILE
                            writeToLog("secureAndSplitPath fileName4=%s", *fileName);
        #endif
                            
                        }
                    }
                }
            }
            
    #ifdef LOG2FILE
            writeToLog("secureAndSplitPath folder1=%s", *folder);
    #endif            
            
        } else {

    #ifdef LOG2FILE
            writeToLog("secureAndSplitPath cwd given=%s", cwd);
    #endif
        
            // path is not given, cwd gives the whole full path
            if (strcmp(cwd, "/") != 0) {
                
                
                // path is not given, cwd gives the whole full path
                // get path from cwd
                path = getLastItemOfPath(cwdNoSlash);
                *fileName = (char *)malloc (strlen(path)+1);
                strcpy(*fileName, path);

    #ifdef LOG2FILE
            writeToLog("secureAndSplitPath fineName5=%s", *fileName);
    #endif
                
                pos = strrchr(cwdNoSlash, '/');
                // folder is allocated by strndup
                *folder = strndup(cwdNoSlash, strlen(path)-strlen(pos));

                // update cwd
                strcpy(cwd, *folder);
                strcat(cwd, "/");
            }
    #ifdef LOG2FILE
            writeToLog("secureAndSplitPath folder2=%s", *folder);
    #endif            
            
        }
    }
    
}

static int32_t ftp_PWD(connection_t *connection, char *rest UNUSED) {
    char msg[MAXPATHLEN + 40] = "";

#ifdef LOG2FILE
    writeToLog("C[%d] ftp_PWD previous dir = %s", connection->index+1, connection->cwd);
#endif

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
    #ifdef LOG2FILE
        writeToLog("C[%d] ftp_PWD new dir = %s", connection->index+1, connection->cwd);
        writeToLog("%s", msg);
    #endif
        
        if (parentFolder != NULL) free(parentFolder);
    }
    return write_reply(connection, 257, msg);

}

static int32_t ftp_CWD(connection_t *connection, char *path) {
    int32_t result = 0;

#ifdef LOG2FILE
    writeToLog("C[%d] ftp_CWD previous dir cwd=%s, path=%s", connection->index+1, connection->cwd, path);
#endif

    char *folder = NULL;
    char *baseName = NULL;

    secureAndSplitPath(connection->cwd, path, &folder, &baseName);
#ifdef LOG2FILE
    writeToLog("C[%d] ftp_CWD folder=%s, baseName=%s", connection->index+1, folder, baseName);
#endif
    
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
            
    #ifdef LOG2FILE
            display("%s", msg);
    #endif
            
            write_reply(connection, 550, msg);
        }
        
    #ifdef LOG2FILE
        writeToLog("C[%d] ftp_CWD new dir = %s", connection->index+1, connection->cwd);
    #endif

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

#ifdef LOG2FILE
    writeToLog("C[%d] ftp_DELE cwd=%s, path=%s", connection->index+1, connection->cwd, path);
#endif
    
    char *folder = NULL;
    char *baseName = NULL;

    secureAndSplitPath(connection->cwd, path, &folder, &baseName);
	strcpy(connection->cwd, folder);
	if (strcmp(connection->cwd, "/") != 0) strcat(connection->cwd, "/");

#ifdef LOG2FILE
    writeToLog("C[%d] ftp_DELE on %s in %s", connection->index+1, baseName, folder);
#endif
    
    if (!vrt_unlink(connection->cwd, baseName)) {
        char msg[MAXPATHLEN + 40] = "";
        sprintf(msg, "C[%d] %s removed", connection->index+1, baseName);
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

#ifdef LOG2FILE
    writeToLog("C[%d] ftp_RMD cwd=%s, path=%s", connection->index+1, connection->cwd, path);
#endif
    
    char *folder = NULL;
    char *baseName = NULL;

    secureAndSplitPath(connection->cwd, path, &folder, &baseName);
	strcpy(connection->cwd, folder);
	if (strcmp(connection->cwd, "/") != 0) strcat(connection->cwd, "/");

#ifdef LOG2FILE
    writeToLog("C[%d] ftp_RMD on %s in %s", connection->index+1, baseName, folder);
#endif
        
    if (!vrt_unlink(connection->cwd, baseName)) {
        char msg[MAXPATHLEN + 40] = "";
        sprintf(msg, "C[%d] %s removed", connection->index+1, baseName);
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

#ifdef LOG2FILE
    writeToLog("C[%d] ftp_MKD on %s in %s", connection->index+1, path, connection->cwd);
#endif
    
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
#ifdef LOG2FILE
        writeToLog("ftp_MKD on C[%d] %s already exist", connection->index+1, baseName);
#endif

    } else {

        if (!vrt_mkdir(connection->cwd, baseName, 0775)) {
            msgCode = 250;
            sprintf(msg, "directory %s created in %s", baseName, connection->cwd);

#ifdef LOG2FILE
        writeToLog("ftp_MKD on C[%d] folder %s was created", connection->index+1, baseName);
        writeToLog("ftp_MKD on C[%d] current dir = %s", connection->index+1, connection->cwd);
#endif

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

#ifdef LOG2FILE
	writeToLog("ftp_RNTO pending_rename = %s", connection->pending_rename);
	writeToLog("len pending_rename = %d", strlen(connection->pending_rename));
#endif
	
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

#ifdef LOG2FILE
    writeToLog("cwd=%s, path=%s, pending_rename=%s", connection->cwd, path, connection->pending_rename);
    writeToLog("folder=%s, baseName=%s", folder, baseName);
#endif

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

    static const int retriesNumber = 2*NB_SIMULTANEOUS_TRANSFERS;
    close_passive_socket(connection);
    // leave this sleep to avoid error on client console

    int nbTries=0;
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
        OSSleepTicks(OSMillisecondsToTicks(retriesNumber*10));
			
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
        OSSleepTicks(OSMillisecondsToTicks(retriesNumber*10));
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
        OSSleepTicks(OSMillisecondsToTicks(retriesNumber*10));
    }
    char reply[49+2+16] = "";
    uint16_t port = bindAddress.sin_port;
    uint32_t ip = network_gethostip();
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
    
    static const int retriesNumber = 2*NB_SIMULTANEOUS_TRANSFERS;
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
        OSSleepTicks(OSMillisecondsToTicks(retriesNumber*10));

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
        OSSleepTicks(OSMillisecondsToTicks(retriesNumber*10));
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
            connection->data_connection_timer = OSGetTime() + (OSTime)(FTP_CONNECTION_TIMEOUT)*1000000;
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
            
            // set permissions for symlinks
            if S_ISLNK(st.st_mode) strcpy(permissions, "lwxr-xr-x");
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
    
#ifdef LOG2FILE
    writeToLog("C[%d] ftp_LIST : listing cwd=%s path=%s", connection->index+1, connection->cwd, path); 
#endif            
    
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


// You must free the result if result is non-NULL.
static char *str_replace(char *orig, char *rep, char *with) {
    char *result; // the return string
    char *ins;    // the next insert point
    char *tmp;    // varies
    int len_rep;  // length of rep (the string to remove)
    int len_with; // length of with (the string to replace rep with)
    int len_front; // distance between rep and end of last rep
    int count;    // number of replacements

    // sanity checks and initialization
    if (!orig || !rep)
        return NULL;
    len_rep = strlen(rep);
    if (len_rep == 0)
        return NULL; // empty rep causes infinite loop during count
    if (!with)
        with = "";
    len_with = strlen(with);

    // count the number of replacements needed
    ins = orig;
    for (count = 0; (tmp = strstr(ins, rep)); ++count) {
        ins = tmp + len_rep;
    }

    tmp = result = malloc(strlen(orig) + (len_with - len_rep) * count + 1);

    if (!result)
        return NULL;

    // first time through the loop, all the variable are set correctly
    // from here on,
    //    tmp points to the end of the result string
    //    ins points to the next occurrence of rep in orig
    //    orig points to the remainder of orig after "end of rep"
    while (count--) {
        ins = strstr(orig, rep);
        len_front = ins - orig;
        tmp = strncpy(tmp, orig, len_front) + len_front;
        tmp = strcpy(tmp, with) + len_with;
        orig += len_front + len_rep; // move to next "end of rep"
    }
    strcpy(tmp, orig);
    return result;
}

static int32_t ftp_RETR(connection_t *connection, char *path) {

    char *folder = NULL;
    char *fileName = NULL;

#ifdef LOG2FILE
    writeToLog("C[%d] ftp_RETR cwd=%s, path=%s", connection->index+1, connection->cwd, path);
#endif

    secureAndSplitPath(connection->cwd, path, &folder, &fileName);
    strcat(folder, "/");
    char *filePath = to_real_path(folder, fileName);
    
    // file or symlink ?
    struct stat st;
    stat(filePath, &st);
    
    if S_ISLNK(st.st_mode) {
        
#ifdef LOG2FILE
        writeToLog("C[%d] ftp_RETR : symlink detected = %s", connection->index+1, filePath); 
#endif            
        // symlink
        char *resolved = NULL;
        resolved = str_replace(folder, "0005000e", "00050000");
        if (resolved == NULL) resolved = str_replace(folder, "0005000c", "00050000"); 
        if (resolved != NULL) {
            strcpy(folder, resolved);
#ifdef LOG2FILE
            writeToLog("C[%d] ftp_RETR : resolved = %s", connection->index+1, resolved); 
#endif
            free(resolved);
        }
        
        // check if the resolved path exist
        free(filePath);
        filePath = to_real_path(folder, fileName);
        if (access(filePath, F_OK) != 0) {
            display("! ERROR : C[%d] ftp_RETR resolution failed on %s", connection->index+1, path);
            display("! ERROR : resolved = %s", filePath);
            display("! ERROR : err = %s", strerror(errno));
            char msg[MAXPATHLEN + 40] = "";
            sprintf(msg, "Error when RETR cwd=%s path=%s : err=%s", connection->cwd, path, strerror(errno));
            free(filePath);
            return write_reply(connection, 550, msg);
        }
    }    
    free(filePath);
    
#ifdef LOG2FILE
    writeToLog("C[%d] cwd=(%s), path=(%s)", connection->index+1, connection->cwd, path);
    writeToLog("C[%d] secured path folder=(%s), fileName=(%s)", connection->index+1, folder, fileName);
#endif

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
    strcpy(connection->uploadFilePath, "");

	// set the internal file's buffer to connection->transferBuffer 	
	setvbuf(connection->f, connection->transferBuffer, _IOFBF, TRANSFER_BUFFER_SIZE);
    
    int32_t result = prepare_data_connection(connection, transfer, connection, closeTransferredFile);

    if (result < 0) {
        if (result == -ENOMEM) {
            display("! ERROR : C[%d] prepare_data_connection failed in ftp_RETR for %s", connection->index+1, path);
            display("! ERROR : out of memory in ftp_RETR");
        }
    }

    return result;
}

static int32_t stor_or_append(connection_t *connection, char *path, char mode[3]) {

    // compute volume path
    char *folder = NULL;
    char *fileName = NULL;

#ifdef LOG2FILE
    writeToLog("C[%d] stor_or_append cwd=(%s), path=(%s)", connection->index+1, connection->cwd, path);
#endif

    secureAndSplitPath(connection->cwd, path, &folder, &fileName);
    strcpy(connection->fileName, fileName);
	strcpy(connection->cwd, folder);
    if (strcmp(connection->cwd, "/") != 0) strcat(connection->cwd, "/");
    
#ifdef LOG2FILE
    writeToLog("C[%d] cwd=(%s), path=(%s)", connection->index+1, connection->cwd, path);
    writeToLog("C[%d] secured path folder=(%s), fileName=(%s)", connection->index+1, folder, fileName);
#endif

    sprintf(connection->uploadFilePath, "%s/%s", folder, fileName);

#ifdef LOG2FILE
    writeToLog("C[%d] connection->uploadFilePath=(%s)", connection->index+1, connection->uploadFilePath);
#endif

    char *folderName = getLastItemOfPath(folder);
    char *pos = strrchr (folder, '/');
    char *parentFolder = strndup(folder, strlen(folder)-strlen(pos)+1);

#ifdef LOG2FILE
    writeToLog("C[%d] check/create %s in %s", connection->index+1, folderName, parentFolder);
#endif

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

	// set the internal file's buffer to connection->transferBuffer 	
	setvbuf(connection->f, connection->transferBuffer, _IOFBF, TRANSFER_BUFFER_SIZE);
    
    display("> C[%d] receiving %s...", connection->index+1, connection->fileName);
    
    int32_t result = prepare_data_connection(connection, transfer, connection, closeTransferredFile);
    if (result < 0) {
        display("! ERROR : C[%d] prepare_data_connection failed in stor_or_append for %s", connection->index+1, path);
        if (result == -ENOMEM) {
            display("! ERROR : out of memory in stor_or_append");
        }
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
    FSChangeMode(__wut_devoptab_fs_client, &cmdBlk, connection->cwd, (FSMode) 0x666, (FSMode) 0x777, FS_ERROR_FLAG_ALL);

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

#ifdef LOG2FILE
        writeToLog("C[%d] closing (data) socket %d", connection->index+1, connection->data_socket);
#endif
    }
    connection->data_socket = -1;
    connection->data_connection_connected = false;
    connection->data_connection_timer = 0;
    connection->dataTransferOffset = -1;
    // transfer call back
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
        display("------------------------------------------------------------");
    }
}

static void cleanup_connection(connection_t *connection) {

#ifdef LOG2FILE
    writeToLog("cleanup C[%d]", connection->index);
#endif

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
        if (connections[connection_index].socket != -1) return &connections[connection_index];
}
    return NULL;
}

void cleanup_ftp() {

    if (listener != -1) {

#ifdef LOG2FILE
        display("Entering in cleanup_ftp()");
        writeToLog("total data sockets opened = %d", nbDataSocketsOpened);
        
        display("Try to warn client from closing connections");
#endif
        connection_t *firstAvailable = getFirstConnectionAvailable();
        if (firstAvailable != NULL) write_reply(firstAvailable, 421, "Closing remaining active connections connection");

#ifdef LOG2FILE
        display("Loop on opened connections");
#endif

        uint32_t connection_index;
        for (connection_index = 0; connection_index < FTP_NB_SIMULTANEOUS_TRANSFERS; connection_index++) {
            connection_t *connection = &connections[connection_index];
            if (connection->data_socket != -1) {


	#ifdef LOG2FILE
	            display("C[%d] in use, check transfer thread", connection_index+1);
	#endif
	            if (!OSIsThreadTerminated(&connection->transferThread)) {
	#ifdef LOG2FILE
	                display("C[%d] in transfer, try to cancel the thread", connection_index+1);
	#endif
	                OSCancelThread(&connection->transferThread);
	                OSTestThreadCancel();
	                #ifdef LOG2FILE
	                    writeToLog("Cancel transfer thread of C[%d]", connection->index+1);
	                #endif
	            }
			}
            cleanup_connection(connection);
            
            // free transfer buffer if needed
            if (connection->transferBuffer != NULL) free(connection->transferBuffer);
            // free transfer threads if needed
            
            // display only if a client was connected
            if (strcmp(clientIp, "UNKNOWN_CLIENT") != 0) display("- %s connection C[%d] closed", clientIp, connection_index+1);        
                
        }
        if (password != NULL) free(password);  

        if (lastSumAvgSpeed != sumAvgSpeed) displayTransferSpeedStats();
    }
    
}

static bool processConnections() {
    
    // if the max connections number is not reached, treat incomming connections
    if (activeConnectionsNumber <= NB_SIMULTANEOUS_TRANSFERS) {

        int32_t peer;
        struct sockaddr_in client_address;
        int32_t addrlen = sizeof(client_address);

        while ((peer = network_accept(listener, (struct sockaddr *)&client_address, &addrlen)) != -EAGAIN) {
            if (peer < 0) {
                char msg[FTP_MSG_BUFFER_SIZE];
                sprintf(msg, "Error accepting connection: err=%d (%s)", -peer, strerror(-peer));
                display("! ERROR : %s", msg);
                OSSleepTicks(OSMillisecondsToTicks(5000));
                connection_t *last = getFirstConnectionAvailable();
                if (last) return write_reply(last, 520, msg);

                // if cannot inform client, stop server
                return false;
            }

            if (strcmp(clientIp, "UNKNOWN_CLIENT") == 0) strcpy(clientIp,inet_ntoa(client_address.sin_addr));
            if (strcmp(clientIp, inet_ntoa(client_address.sin_addr)) !=0 ) {

                display("~ WARNING : Sorry %s, %s is already connected ! close all his connections first !", inet_ntoa(client_address.sin_addr), clientIp);
                network_close(peer);
            } else {

                resetConnection(&connections[activeConnectionsNumber]);
                connections[activeConnectionsNumber].socket = peer;
                memcpy(&connections[activeConnectionsNumber].address, &client_address, sizeof(client_address));

                if (write_reply(&connections[activeConnectionsNumber], 220, "---------====={ WiiUFtpServer }=====---------") < 0) {
                    display("! ERROR : Error writing greeting");
                    network_close(peer);
                    connections[activeConnectionsNumber].socket = -1;
					return false;
                } else {
                    #ifdef LOG2FILE
                        writeToLog("Greetings sent sucessfully");
                    #endif

                    activeConnectionsNumber++;

            		display("- %s use connection C[%d]", clientIp, activeConnectionsNumber);
//                            #ifdef LOG2FILE
//                                displayConnectionDetails(connection_index);
//                            #endif
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
			#ifdef LOG2FILE
			    writeToLog("C[%d] using passive_socket (%d)", connection->index+1, connection->passive_socket);
			#endif
            struct sockaddr_in data_peer_address;
            int32_t addrlen = sizeof(data_peer_address);

            result = network_accept(connection->passive_socket, (struct sockaddr *)&data_peer_address ,&addrlen);
            if (result >= 0) {
                connection->data_socket = result;
                connection->data_connection_connected = true;
            } else {
				if (result != -EAGAIN) {
	                char msg[FTP_MSG_BUFFER_SIZE];
	                sprintf(msg, "Error accepting C[%d] %d (%s)",  connection->index+1, errno, strerror(errno));
	                display("~ WARNING : %s", msg);
	                write_reply(connection, 425, msg);
				}
            }

        } else {

			#ifdef LOG2FILE
			    writeToLog("C[%d] using data_socket (%d) for transferring %s", connection->index+1, connection->data_socket, connection->fileName);
			#endif
            // retry 2 times if can't connect before exiting
            int nbTries=0;
            try_again:
            if ((result = network_connect(connection->data_socket, (struct sockaddr *)&connection->address, sizeof(connection->address))) < 0) {
                if (result == -EINPROGRESS || result == -EALREADY) {
                    nbTries++;
                    if (nbTries <= NB_SIMULTANEOUS_TRANSFERS) goto try_again;
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
                #ifdef LOG2FILE
                    writeToLog("Opened connections = %d / %d", activeConnectionsNumber, NB_SIMULTANEOUS_TRANSFERS);
                #endif
                if (result > 0) return;
            }
        }

        if (!connection->data_connection_connected) {
            if (OSGetTime() > connection->data_connection_timer) {
	            // timeout only for connections who try to connect
	            result = -99;
	            char msg[MAXPATHLEN] = "";
	            sprintf(msg, "C[%d] timed out when connecting", connection->index+1);
	            display("~ WARNING : %s", msg);
	            write_reply(connection, 520, msg);
	        }
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

                    if (strlen(connection->uploadFilePath) == 0)
                        display("> C[%d] %s sent at %.2f MB/s (%d bytes)", connection->index+1, connection->fileName, connection->speed, connection->dataTransferOffset);
                    else
                        display("> C[%d] %s received at %.2f MB/s (%d bytes)", connection->index+1, connection->fileName, connection->speed, connection->dataTransferOffset);

                }
            }
        }

        #ifdef LOG2FILE
            if (result != -EAGAIN) writeToLog("C[%d] data_callback using socket %d returned %d", connection->index+1, connection->data_socket, result);
        #endif

        // check errors
        if (result < 0 && result != -EAGAIN) {
            display("! ERROR : C[%d] data transfer callback using socket %d failed , socket error = %d", connection->index+1, connection->data_socket, result);
        }
    }

    if (result <= 0 && result != -EAGAIN && result != -EISCONN) {
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
            if (connections[connection_index].socket != -1) {
                if (connections[connection_index].data_callback) {
                    process_data_events(&connections[connection_index]);
                    // total is the sum of speed computed for each connection
                    if (connections[connection_index].speed) totalSpeedMBs += connections[connection_index].speed;
                } else {
                    process_control_events(&connections[connection_index]);
                }
            }
        }
        if (totalSpeedMBs) {
            // increment nbAvgFiles and take speed into account for mean calculation
            nbSpeedMeasures += 1;
            
            // fix double rate errors
			int maxSpeedPerTransfer = 7;
			int nbt = (int)(totalSpeedMBs / maxSpeedPerTransfer);
			if (nbt > 1) totalSpeedMBs = totalSpeedMBs / nbt;
            
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