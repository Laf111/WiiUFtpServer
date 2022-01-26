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
#include <coreinit/memory.h>
 
#include "ftp.h"
#include "virtualpath.h"
#include "net.h"
#include "vrt.h"

#define UNUSED    __attribute__((unused))

// used to compute transfer rate
#define BUS_SPEED 248625000

extern void display(const char *fmt, ...);

#ifdef LOG2FILE
    extern void writeToLog(const char *fmt, ...);
    int nbDataSocketsOpened = 0;
#endif

static bool verboseMode=false;
static const uint16_t SRC_PORT = 20;
static const int32_t EQUIT = 696969;
static const char *CRLF = "\r\n";
static const uint32_t CRLF_LENGTH = 2;

// number of active connections
static uint32_t activeConnectionsNumber = 0;

// unique client IP address
static char clientIp[15]="UNKNOWN_CLIENT";

// passive_port : 1024 - 65535
static uint16_t passive_port = 1024;
static char *password = NULL;

// OS time computed in main
static struct tm *timeOs=NULL;
// OS time computed in main
static time_t tsOs=0;

// IOSUHAX fd
static int fsaFd = -1;

static connection_t *connections[FTP_NB_SIMULTANEOUS_TRANSFERS] = { NULL };
static void *transferBuffers[FTP_NB_SIMULTANEOUS_TRANSFERS] = { NULL };

static int listener = -1;     // listening socket descriptor

// max and min transfer rate speeds in MBs
static float maxTransferRate = -9999;
static float minTransferRate = 9999;

// sum of average speeds
static float sumAvgSpeed = 0;
// number of measures used for average computation
static uint32_t nbSpeedMeasures = 0;

// FTP thread on CPU2
static OSThread ftpThread;
static uint8_t *ftpThreadStack=NULL;

static int ftpThreadMain(int argc UNUSED, const char **argv UNUSED)
{

    int32_t socket = network_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (socket < 0)
        display("! ERROR : network_socket failed and return %d", socket);   
        
    return socket;
}

// function to toggle verbose mode
void setVerboseMode(bool flag) {
    verboseMode=flag;
}

int32_t create_server(uint16_t port) {

    ftpThreadStack = MEMAllocFromDefaultHeapEx(FTP_STACK_SIZE, 8);
    if (ftpThreadStack == NULL) {
        display("! ERROR : when allocating ftpThreadStack!");
        return -ENOMEM;
    }

    if (!OSCreateThread(&ftpThread, ftpThreadMain, 0, NULL, ftpThreadStack + FTP_STACK_SIZE, FTP_STACK_SIZE, FTP_NB_SIMULTANEOUS_TRANSFERS+2, OS_THREAD_ATTRIB_AFFINITY_CPU2)) {
        display("! ERROR : when creating ftpThread!");        
        return -ENOMEM;
    }
    #ifdef LOG2FILE    
        OSSetThreadStackUsage(&ftpThread);
    #endif

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
    if ((ret = network_listen(listener, FTP_NB_SIMULTANEOUS_TRANSFERS)) < 0) {
        network_close(listener);
        return ret;
    }

    uint32_t ip = network_gethostip();

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
    display(" ");
    if (timeOs != NULL && tsOs == 0) {
        time_t ts1970=mktime(timeOs);
        tsOs = ts1970*1000000 + 86400*((366*2) + (365*8) +1);        
    }       
    
    return listener;
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
                return NULL;
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

#ifdef LOG2FILE
void displayData(uint32_t index) {

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
        writeToLog("fileFolder                 = %s", connections[index]->fileFolder);
        writeToLog("fd                         = %d", fileno(connections[index]->f));
        writeToLog("volPath                    = %s", connections[index]->volPath);
        writeToLog("dataTransferOffset         = %d", connections[index]->dataTransferOffset);
        writeToLog("speed                      = %.2f", connections[index]->speed);
        writeToLog("bytesTransfered            = %d", connections[index]->bytesTransfered);
            
    }
    writeToLog("FTP Thread stack size      = %d", OSCheckThreadStackUsage(&ftpThread));        
         
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
        display("C[%d] launching transfer for %s", activeConnection->index+1, activeConnection->fileName);
    #endif
    
    if (activeConnection->volPath == NULL) {
        result = send_from_file(activeConnection->data_socket, activeConnection);
    } else {        
        result = recv_to_file(activeConnection->data_socket, activeConnection);

        // change rights on file
        int rc = IOSUHAX_FSA_ChangeMode(fsaFd, activeConnection->volPath, 0x664);
        
        if (rc < 0 ) {
            display("~ WARNING : when settings file's rights, rc = %d !", rc);
            display("~ WARNING : file = %s", activeConnection->fileName);        		
        }
        free(activeConnection->volPath);
        activeConnection->volPath = NULL;
        
    }
    
    #ifdef LOG2FILE    
        display("C[%d] transfer launched", activeConnection->index+1);
    #endif

    return result;
}

static int32_t transfer(int32_t data_socket UNUSED, connection_t *connection) {
    int32_t result = -EAGAIN;    
                
    if (connection->dataTransferOffset == -1) {

        // init bytes counter
        connection->dataTransferOffset = 0;
        
        // init speed to 0
        connection->speed = 0;
               
		#ifdef LOG2FILE    
		    writeToLog("Using data_socket (%d) of C[%d] to transfer %s", data_socket, connection->index+1, connection->fileName);
		    displayData(connection->index);
		#endif

        // resize internal file's buffer to MIN_TRANSFER_CHUNK_SIZE (max size of one recv chunk in net.c)
        if (setvbuf(connection->f, NULL, _IOFBF, MIN_TRANSFER_CHUNK_SIZE) != 0) {
            display("! WARNING : setvbuf failed for uploading  = %s", connection->fileName);
            display("! WARNING : errno = %d (%s)", errno, strerror(errno));          
        }

        // priorize the last connections launched
        if (!OSCreateThread(&connection->transferThread, launchTransfer, 1, (char *)connection, connection->transferThreadStack + FTP_TRANSFER_STACK_SIZE, FTP_TRANSFER_STACK_SIZE, (FTP_NB_SIMULTANEOUS_TRANSFERS-activeConnectionsNumber), OS_THREAD_ATTRIB_AFFINITY_ANY)) {
            display("! ERROR : when creating transferThread!");        
            return -105;
        }
        
        #ifdef LOG2FILE    
            OSSetThreadStackUsage(&connection->transferThread);
        #endif
        
        OSResumeThread(&connection->transferThread);
                    
    } else {
        
        result = connection->bytesTransfered;
        
/*     #ifdef LOG2FILE
        writeToLog("C[%d] Transfer thread stack size = %d", connection->index+1, OSCheckThreadStackUsage(&connection->transferThread));   
    #endif */
        
        if (result <= 0) {
            OSJoinThread(&connection->transferThread, &result);
            #ifdef LOG2FILE    
                writeToLog("Transfer thread on C[%d] ended successfully", connection->index+1);
            #endif            
        }
    }
       
    return result;
}

static int32_t closeTransferedFile(connection_t *connection) {
    int32_t result = -120;    

    #ifdef LOG2FILE    
        writeToLog("CloseTransferedFile for C[%d] (file=%s)", connection->index+1, connection->fileName);
    #endif         
            
    if (!OSIsThreadTerminated(&connection->transferThread)) {
        OSCancelThread(&connection->transferThread);
        OSTestThreadCancel();        
        #ifdef LOG2FILE    
            writeToLog("Cancel transfer thread of C[%d]", connection->index+1);
        #endif                     
    }
    
    if (connection->f != NULL) result = fclose(connection->f);
    connection->f = NULL;

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
	if (msgbuf == NULL) return -ENOMEM;
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

static int32_t ftp_PWD(connection_t *connection, char *rest UNUSED) {
    char msg[MAXPATHLEN + 24] = "";
    
    // check if folder exist    
    char *parentFolder = (char *)malloc (strlen(connection->cwd)+1);
    strcpy(parentFolder, connection->cwd);
    char *pos = strrchr(parentFolder, '/');  
    char *folder = strdup (pos + 1);

    if (vrt_checkdir(parentFolder, folder) == 0) {

        if (strrchr(connection->cwd, '"'))
            sprintf(msg, "C[%d] %s is current directory", connection->index+1, connection->cwd);
        else
            sprintf(msg, "C[%d] \"%s\" is current directory", connection->index+1, connection->cwd);
    } else {
        display("! ERROR : C[%d] failed to PWD to %s (does not exist)", connection->index+1, connection->cwd);
        
    }
    return write_reply(connection, 257, msg);
}

static int32_t ftp_CWD(connection_t *connection, char *path) {
    int32_t result = 0;    
    
// #ifdef LOG2FILE
//         writeToLog("ftp_CWD on C[%d] previous dir = %s", connection->inde+1x, connection->cwd);
// #endif
                        
    if (!vrt_chdir(connection->cwd, path)) {
        
// #ifdef LOG2FILE
//         writeToLog("ftp_CWD on C[%d] current dir = %s", connection->index+1, connection->cwd);
// #endif                        
        char msg[MAXPATHLEN + 60] = "";
        sprintf(msg, "C[%d] CWD successful to %s", connection->index+1, connection->cwd); 
        write_reply(connection, 250, msg);
    } else  {
//        display("~ WARNING : error in vrt_chdir in ftp_CWD : %s+%s", connection->cwd, path);            
//        display("~ WARNING : errno = %d (%s)", errno, strerror(errno));         

        char msg[MAXPATHLEN + 40] = "";
        sprintf(msg, "Error when CWD to %s%s : err = %s", connection->cwd, path, strerror(errno)); 
        write_reply(connection, 550, msg);
    }
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
    char vPath[MAXPATHLEN+1] = "";

    char *folder = NULL;
    char *fileName = NULL;
    
    // check if folder exist, creates it if needed    
    if (path[0] == '/') {
        sprintf(vPath, "%s", path);
        // get the folder from path        
        folder = (char *)malloc (strlen(path)+1);
        strcpy(folder, path);
        char *pos = strrchr(folder, '/');  
        fileName = strdup (pos + 1);
    } else {  
        // folder = connection_cwd
        sprintf(vPath, "%s/%s", connection->cwd, path);
        folder = (char *)malloc (strlen(connection->cwd)+1);
        strcpy (folder, connection->cwd);
        fileName = (char *)malloc (strlen(path)+1);
        strcpy (fileName, path);
    }
    
    char *volPath = NULL;
    volPath = virtualToVolPath(vPath);

    // chmod 
    IOSUHAX_FSA_ChangeMode(fsaFd, volPath, 0x664);
    free(volPath);
    
    if (!vrt_unlink(folder, fileName)) {
        char msg[MAXPATHLEN + 40] = "";
        sprintf(msg, "C[%d] File or directory removed", connection->index+1); 
        
        return write_reply(connection, 250, msg);
    } else {
        display("~ WARNING : error from vrt_unlink in ftp_DELE : %s", strerror(errno));                            
        char msg[MAXPATHLEN + 40] = "";
        sprintf(msg, "Error when DELE %s%s : err = %s", connection->cwd, path, strerror(errno)); 
        return write_reply(connection, 550, msg);
    }
}

static int32_t ftp_MKD(connection_t *connection, char *path) {
    if (!*path) {
        return write_reply(connection, 501, "Syntax error in parameters");
    }

    char msg[MAXPATHLEN + 60] = "";
	int msgCode = 550;
        
    if (vrt_checkdir(connection->cwd, path) == 0) {
		msgCode = 257;
		strcpy(msg, "folder already exist");
#ifdef LOG2FILE
        writeToLog("ftp_MKD on C[%d] %s already exist", connection->index+1, path);
#endif                        
        
    } else {
        
        if (!vrt_mkdir(connection->cwd, path, 0775)) {
            msgCode = 250;               
            sprintf(msg, "%s%s directory created", connection->cwd, path);
            					
#ifdef LOG2FILE
        writeToLog("ftp_MKD on C[%d] folder %s was created", connection->index+1, path);
        writeToLog("ftp_MKD on C[%d] current dir = %s", connection->index+1, connection->cwd);
#endif                        

        } else {
            display("! ERROR : error from vrt_mkdir in ftp_MKD : %s", strerror(errno));            
            sprintf(msg, "Error in MKD when cd to %s%s : err = %s", connection->cwd, path, strerror(errno)); 
            return write_reply(connection, msgCode, strerror(errno));
        }
    }
	
    return write_reply(connection, msgCode, msg);
}

static int32_t ftp_RNFR(connection_t *connection, char *path) {
    strcpy(connection->pending_rename, path);
    return write_reply(connection, 350, "Ready for RNTO");
}

static int32_t ftp_RNTO(connection_t *connection, char *path) {
    if (!*connection->pending_rename) {
        return write_reply(connection, 503, "RNFR required first");
    }
    int32_t result;
    if (!vrt_rename(connection->cwd, connection->pending_rename, path)) {
        result = write_reply(connection, 250, "Rename successful");
    } else {
        display("! ERROR : error from vrt_rename in ftp_RNTO : %s", strerror(errno));             
        result = write_reply(connection, 550, strerror(errno));
    }
    *connection->pending_rename = '\0';
    return result;
}

static int32_t ftp_SIZE(connection_t *connection, char *path) {
    struct stat st;

    FILE *f = vrt_fopen(connection->cwd, path, "rb");
    if (f) {
        fclose(f);
        int ret = 0;
        if ((ret = vrt_stat(connection->cwd, path, &st)) == 0) {
            char size_buf[12] = "";
            sprintf(size_buf, "%llu", st.st_size);
            return write_reply(connection, 213, size_buf);
        } else {
            display("! ERROR : C[%d], error from vrt_stat in ftp_SIZE, ret = %d", connection->index+1, ret);  
            display("! C[%d], cwd=%s, path=%s", connection->index+1, connection->cwd, path);  

            char msg[MAXPATHLEN + 40] = "";
            sprintf(msg, "Error SIZE on %s%s : err = %s", connection->cwd, path, strerror(errno)); 
            return write_reply(connection, 550, msg);
        }
    } else {
        display("! ERROR : C[%d], error from vrt_stat in ftp_SIZE on %s%s", connection->index+1, connection->cwd, path);  
        display("! failed to open %s%s", connection->cwd, path);  

        char msg[MAXPATHLEN + 40] = "";
        sprintf(msg, "Error SIZE on %s%s : err = %s", connection->cwd, path, strerror(errno)); 
        return write_reply(connection, 550, msg);
    }
}

static int32_t ftp_PASV(connection_t *connection, char *rest UNUSED) {
    
    static const int retriesNumber = (int) ((float)(FTP_CONNECTION_TIMEOUT) / ((float)NET_RETRY_TIME_STEP_MILLISECS/1000.0));
    close_passive_socket(connection);
    // leave this sleep to avoid error on client console
    OSSleepTicks(OSMillisecondsToTicks(NB_SIMULTANEOUS_TRANSFERS*4));
    
    int nbTries=0;
    while (1)
    {
        connection->passive_socket = network_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
        if (connection->passive_socket >= 0)
            break;

        OSSleepTicks(OSMillisecondsToTicks(NET_RETRY_TIME_STEP_MILLISECS));
        if (++nbTries > retriesNumber)
            return write_reply(connection, 520, "Unable to create listening socket");
    }
#ifdef LOG2FILE
        writeToLog("C[%d] opening passive socket %d", connection->index+1, connection->passive_socket);        
#endif

    struct sockaddr_in bindAddress;
    memset(&bindAddress, 0, sizeof(bindAddress));
    bindAddress.sin_family = AF_INET;
    // reset passive_port to avoid overflow
    if (passive_port == 65534) {
        passive_port = 1024;
#ifdef LOG2FILE
        display("Passive port overflow !, reset to 1024");
#endif                
    }
    bindAddress.sin_port = htons(passive_port++);
    bindAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    int32_t result;
    if ((result = network_bind(connection->passive_socket, (struct sockaddr *)&bindAddress, sizeof(bindAddress))) < 0) {
	    display("! ERROR : failed to bind passive socket %s", connection->passive_socket);        
        close_passive_socket(connection);
        return write_reply(connection, 520, "Unable to bind listening socket");
    }
    if ((result = network_listen(connection->passive_socket, 1)) < 0) {
        close_passive_socket(connection);
        return write_reply(connection, 520, "Unable to listen on socket");
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

    static const int retriesNumber = (int) ((float)(FTP_CONNECTION_TIMEOUT) / ((float)NET_RETRY_TIME_STEP_MILLISECS/1000.0));
    int nbTries=0;
    try_again:

    int32_t data_socket = network_socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (data_socket < 0) {
        OSSleepTicks(OSMillisecondsToTicks(NET_RETRY_TIME_STEP_MILLISECS));
        nbTries++;
        if (nbTries <= retriesNumber) goto try_again;
        return data_socket;
    }
    
#ifdef LOG2FILE
        nbDataSocketsOpened+=1;
        writeToLog("opening data socket = %d", data_socket);
        writeToLog("total sockets opened = %d", nbDataSocketsOpened);        
#endif        
    
    struct sockaddr_in bindAddress;
    memset(&bindAddress, 0, sizeof(bindAddress));
    bindAddress.sin_family = AF_INET;
    bindAddress.sin_port = htons(SRC_PORT);
    bindAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    int32_t result;
    if ((result = network_bind(data_socket, (struct sockaddr *)&bindAddress, sizeof(bindAddress))) < 0) {
        network_close(data_socket);
        
        char msg[FTP_MSG_BUFFER_SIZE];
        sprintf(msg, "failed to bind active socket %d of C[%d] %d (%s)", connection->data_socket, connection->index+1, errno, strerror(errno));                
        display("~ WARNING : %s", msg);
        OSSleepTicks(OSMillisecondsToTicks(NET_RETRY_TIME_STEP_MILLISECS));
        write_reply(connection, 421, msg);
                
        return result;
    }

    connection->data_socket = data_socket;
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
    if (strcmp(connection->fileName, "."))
        sprintf(msgStatus, "C[%d] transferring data...", connection->index+1); 
    else
        sprintf(msgStatus, "C[%d] transferring %s...", connection->index+1, connection->fileName); 
        
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

static int32_t ftp_NLST(connection_t *connection, char *path) {
    if (!*path) {
        path = ".";
    }

    DIR_P *dir = vrt_opendir(connection->cwd, path);
    if (dir == NULL) {
        display("! ERROR : error from vrt_opendir in ftp_NLST : %s", strerror(errno));
        
        char msg[MAXPATHLEN + 40] = "";
        sprintf(msg, "Error when NLIST %s%s : err = %s", connection->cwd, path, strerror(errno)); 
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

    if (path && connection->cwd) if (strcmp(path, ".") == 0 && strcmp(connection->cwd, "/") == 0) {
#ifdef LOG2FILE    
        writeToLog("ResetVirtualPaths from ftp_LIST");
#endif        
        ResetVirtualPaths();
    }    
    DIR_P *dir = vrt_opendir(connection->cwd, path);
    if (dir == NULL) {
        display("! ERROR : C[%d] vrt_opendir failed in ftp_LIST() on %s", connection->index+1, path);
        
        char msg[MAXPATHLEN + 40] = "";
        sprintf(msg, "Error when LIST %s%s : err = %s", connection->cwd, path, strerror(errno)); 
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
    
    // compute fileName    
    if (path[0] == '/') {
        // get the folder from path        
        char *folder = (char *)malloc (strlen(path)+1);
        strcpy(folder, path);
	    char *pos = strrchr(folder, '/');  
	    char *fileName = strdup (pos + 1);
	    strcpy(connection->fileName, fileName);
    } else {  
        // folder = connection_cwd
		strcpy(connection->fileName, path);
    }
        
    display("> C[%d] sending %s...", connection->index+1, connection->fileName);
    
    connection->f = vrt_fopen(connection->cwd, path, "rb");
    if (!connection->f) {
        display("! ERROR : ftp_RETR failed to open %s", path);            
        display("! ERROR : err = %s", strerror(errno));            
        char msg[MAXPATHLEN + 40] = "";
        sprintf(msg, "Error when RETR %s%s : err = %s", connection->cwd, path, strerror(errno)); 
        return write_reply(connection, 550, msg);
    }
    
    int fd = fileno(connection->f);
    if (connection->restart_marker && lseek(fd, connection->restart_marker, SEEK_SET) != connection->restart_marker) {
        int32_t lseek_error = errno;
        fclose(connection->f);
        connection->restart_marker = 0;
        return write_reply(connection, 550, strerror(lseek_error));
    }
    connection->restart_marker = 0;
    connection->volPath = NULL;
    
    int32_t result = prepare_data_connection(connection, transfer, connection, closeTransferedFile);

    if (result < 0) {
        if (result == -ENOMEM) {
            display("! ERROR : C[%d] prepare_data_connection failed in ftp_RETR for %s", connection->index+1, path);
            display("! ERROR : out of memory in ftp_RETR");
        }
        closeTransferedFile(connection);
    }

    return result;
}

static int32_t stor_or_append(connection_t *connection, char *path) {
    
    connection->restart_marker = 0;
    display("> C[%d] receiving %s...", connection->index+1, connection->fileName);

    int32_t result = prepare_data_connection(connection, transfer, connection, closeTransferedFile);
    if (result < 0) {
        display("! ERROR : C[%d] prepare_data_connection failed in stor_or_append for %s", connection->index+1, path);
        if (result == -ENOMEM) {
            display("! ERROR : out of memory in stor_or_append");
        }
        closeTransferedFile(connection);
    }
    return result;
}

static int32_t ftp_STOR(connection_t *connection, char *path) {

    // compute volume path
    char vPath[MAXPATHLEN*2] = "";
    
    char *folder = NULL;
    char *pos = NULL;
	
    // check if folder exist, creates it if needed    
    if (path[0] == '/') {
        sprintf(vPath, "%s", path);
        // get the folder from path        
        folder = (char *)malloc (strlen(path)+1);
        strcpy(folder, path);
	    pos = strrchr(folder, '/');  
	    char *fileName = strdup (pos + 1);
	    strcpy(connection->fileName, fileName);
    } else {  
        sprintf(vPath, "%s/%s", connection->cwd, path);
        // folder = connection_cwd
        folder = (char *)malloc (strlen(connection->cwd)+1);
        strcpy (folder, connection->cwd);
		strcpy(connection->fileName, path);
	    pos = strrchr(folder, '/');  
    }
    
    char *parentFolder = (char *)malloc (strlen(folder)+1);
    strcpy(parentFolder, folder);
    pos =  strrchr (parentFolder, '/');
    char *folderName = strdup(pos + 1);
    
    if (vrt_checkdir(parentFolder, folderName)) {       
        if (vrt_mkdir(parentFolder, folderName, 0775)!=0) {
            display("! ERROR : C[%d] error from stor_or_append when creating %s/%s", connection->index+1, parentFolder, folderName);            
        }
    }
    connection->volPath = virtualToVolPath(vPath);
    
	connection->f = vrt_fopen(connection->cwd, path, "wb");    
    if (!connection->f) {
        display("! ERROR : ftp_STOR failed to open %s", path);            
        display("! ERROR : err = %s", strerror(errno));            
        char msg[MAXPATHLEN + 40] = "";
        sprintf(msg, "Error storing %s%s : err = %s", connection->cwd, path, strerror(errno)); 
        return write_reply(connection, 550, msg);
    }
    
    return stor_or_append(connection, path);
}

static int32_t ftp_APPE(connection_t *connection, char *path) {

    // compute volume path
    char vPath[MAXPATHLEN*2] = "";
    
    char *folder = NULL;
    char *pos = NULL;
	
    // check if folder exist, creates it if needed    
    if (path[0] == '/') {
        sprintf(vPath, "%s", path);
        // get the folder from path        
        folder = (char *)malloc (strlen(path)+1);
        strcpy(folder, path);
	    pos = strrchr(folder, '/');  
	    char *fileName = strdup (pos + 1);
	    strcpy(connection->fileName, fileName);
    } else {  
        sprintf(vPath, "%s/%s", connection->cwd, path);
        // folder = connection_cwd
        folder = (char *)malloc (strlen(connection->cwd)+1);
        strcpy (folder, connection->cwd);
		strcpy(connection->fileName, path);
	    pos = strrchr(folder, '/');  
    }
    
    char *parentFolder = (char *)malloc (strlen(folder)+1);
    strcpy(parentFolder, folder);
    pos =  strrchr (parentFolder, '/');
    char *folderName = strdup(pos + 1);
    
    if (vrt_checkdir(parentFolder, folderName)) {       
        if (vrt_mkdir(parentFolder, folderName, 0775)!=0) {
            display("! ERROR : C[%d] error from stor_or_append when creating %s/%s", connection->index+1, parentFolder, folderName);            
        }
    }
    connection->volPath = virtualToVolPath(vPath);

    connection->f = vrt_fopen(connection->cwd, path, "ab");
    if (!connection->f) {
        display("! ERROR : ftp_APPE failed to open %s", path);            
        display("! ERROR : err = %s", strerror(errno));            
        char msg[MAXPATHLEN + 40] = "";
        sprintf(msg, "Error storing %s%s : err = %s", connection->cwd, path, strerror(errno)); 
        return write_reply(connection, 550, msg);
    }
    
    return stor_or_append(connection, path);
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

    // chmod on folder
    IOSUHAX_FSA_ChangeMode(fsaFd, volPath, 0x664);
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
    ftp_SITE, ftp_NOOP, ftp_SUPERFLUOUS, ftp_UNKNOWN, ftp_MDTM
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
        writeToLog("C[%d] closing socket %d", connection->index+1, connection->data_socket);        
#endif
    }
    connection->data_socket = -1;
    connection->data_connection_connected = false;
    

    if (connection->data_connection_cleanup) {        
        connection->data_connection_cleanup(connection->data_connection_callback_arg);
    }    
    connection->data_callback = NULL;        
    connection->data_connection_callback_arg = NULL;
    connection->data_connection_cleanup = NULL;
    
    connection->data_connection_timer = 0;
    connection->dataTransferOffset = -1;
    connection->volPath = NULL;
    connection->f = NULL;
                
}

static void cleanup_connection(connection_t *connection) {
    
    network_close(connection->socket);    
    cleanup_data_resources(connection);
    // volontary set to -EAGAIN here and not in cleanup_data_resources for msg 226 to connection
    connection->bytesTransfered = -EAGAIN;
    close_passive_socket(connection);
	                
    uint32_t connection_index;
    for (connection_index = 0; connection_index < FTP_NB_SIMULTANEOUS_TRANSFERS; connection_index++) {
        if (connections[connection_index]) {
            if (connections[connection_index] == connection) {
                connections[connection_index] = NULL;
                break;
            }
        }
    }
    // free transfer thread stack
    if (connection->transferThreadStack != NULL) MEMFreeToDefaultHeap(connection->transferThreadStack);
    connection->transferThreadStack = NULL;

    // set pointer to transferBuffer to null (transferBuffers[connection_index] is not deallocated here but only in cleanup_ftp when stopping the server)
    connection->transferBuffer = NULL;
    
    free(connection);
    activeConnectionsNumber--;
    display("- %s connection C[%d] closed", clientIp, connection_index+1);
}


static connection_t* getFirstConnectionAvailable() {
    
    uint32_t connection_index;

    for (connection_index = 0; connection_index < FTP_NB_SIMULTANEOUS_TRANSFERS; connection_index++) {
        
        connection_t *connection = connections[connection_index];        
        if (connection) return connection;
    }
    
    return NULL;
}

void cleanup_ftp() {
    
    if (listener != -1) {
        
#ifdef LOG2FILE
        writeToLog("total data sockets opened = %d", nbDataSocketsOpened);
#endif
        connection_t *firstAvailable = getFirstConnectionAvailable();
        if (firstAvailable) write_reply(firstAvailable, 421, "Closing remaining active connections connection");

        uint32_t connection_index;
        for (connection_index = 0; connection_index < FTP_NB_SIMULTANEOUS_TRANSFERS; connection_index++) {
            connection_t *connection = connections[connection_index];
            
            if (connection) {
                
                if (!OSIsThreadTerminated(&connection->transferThread)) {
                    OSCancelThread(&connection->transferThread);
                    OSTestThreadCancel();        
                    #ifdef LOG2FILE    
                        writeToLog("Cancel transfer thread of C[%d]", connection->index+1);
                    #endif                     
                }                
                cleanup_connection(connection);				
            }
            
            // free user's buffer file
            if (transferBuffers[connection_index] != NULL) MEMFreeToDefaultHeap(transferBuffers[connection_index]);
            transferBuffers[connection_index] = NULL;
        }
/*         #ifdef LOG2FILE    
            OSClearThreadStackUsage(&ftpThread);
        #endif  */           
        
        if (ftpThreadStack != NULL) MEMFreeToDefaultHeap(ftpThreadStack);        
        
        if (nbSpeedMeasures != 0) {
            display(" ");    
            display("------------------------------------------------------------");
            display("  Speed (MB/s) [min = %.2f, mean = %.2f, max = %.2f]", minTransferRate, sumAvgSpeed/(float)nbSpeedMeasures, maxTransferRate);
        }        
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
                    
                // Allocate a new connection
                connection_t *connection = malloc(sizeof(connection_t));
                if (!connection) {
                    display("! ERROR : Could not allocate memory for a new connection, not accepting connection");
                    network_close(peer);
                    return true;
                }
                connection->socket = peer;
                connection->representation_type = 'A';
                connection->passive_socket = -1;
                connection->data_socket = -1;
                strcpy(connection->cwd, "/");        
                *connection->pending_rename = '\0';
                connection->restart_marker = 0;
                connection->authenticated = false;
                connection->offset = 0;
                connection->data_connection_connected = false;
                connection->data_callback = NULL;
                connection->data_connection_callback_arg = NULL;
                connection->data_connection_cleanup = NULL;
                connection->data_connection_timer = 0;
                connection->index = -1;
                strcpy(connection->fileName, "");
                connection->volPath = NULL;
                connection->f = NULL;
                
                connection->transferThreadStack = NULL;                        
                // pre-allocate transfer thread stack
                connection->transferThreadStack = MEMAllocFromDefaultHeapEx(FTP_TRANSFER_STACK_SIZE, 8);
                if (connection->transferThreadStack == NULL) {
                    display("! ERROR : when allocating transferThreadStack!");
                    network_close(peer);
                    free(connection);
                    return false;
                }  
                        
                connection->dataTransferOffset = -1;
                connection->speed = 0;
                connection->bytesTransfered = -EAGAIN;

                memcpy(&connection->address, &client_address, sizeof(client_address));
                uint32_t connection_index;
                if (write_reply(connection, 220, "---------====={ WiiUFtpServer }=====---------") < 0) {
                    display("! ERROR : Error writing greeting");
                    network_close(peer);
                    MEMFreeToDefaultHeap(connection->transferThreadStack);
                    free(connection);
                } else {
                    #ifdef LOG2FILE
                        writeToLog("Greetings sent sucessfully");
                    #endif

                    for (connection_index = 0; connection_index < FTP_NB_SIMULTANEOUS_TRANSFERS; connection_index++) {
                        if (!connections[connection_index]) {
                            connection->index = connection_index;
                            connections[connection_index] = connection;
                             
                            if (transferBuffers[connection_index] != NULL) 
                                // already allocated, use it
                                connection->transferBuffer = transferBuffers[connection_index];
                            else {
                                // pre-allocate user buffer for transfering with connection[connection_index]
                                transferBuffers[connection_index] = MEMAllocFromDefaultHeapEx(TRANSFER_BUFFER_SIZE, 64);
                                if (!transferBuffers[connection_index]) {
                                    display("! ERROR : failed to allocate user buffer for the c[%d]", connection_index+1);
                                    network_close(peer);
                                    MEMFreeToDefaultHeap(connection->transferThreadStack);
                                    free(connection);
                                    return false;
                                }
                                
                                connection->transferBuffer = transferBuffers[connection_index];
                            }
                            
                            activeConnectionsNumber++;
                                    
                            display("- %s opening connection C[%d]", clientIp, connection_index+1);
                            #ifdef LOG2FILE    
                                displayData(connection_index);
                            #endif                        
                            return true;
                        }
                    }
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

			#ifdef LOG2FILE
			    writeToLog("C[%d] using data_socket (%d) for transferring %s", connection->index+1, connection->data_socket, connection->fileName);
			#endif
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
                #ifdef LOG2FILE    
                    display("Opened connections = %d / %d", activeConnectionsNumber, NB_SIMULTANEOUS_TRANSFERS);
                #endif
                if (result > 0) return;
            }
        }
        
        if (connection->data_connection_connected) {            
            return;
        } else if (OSGetTime() > connection->data_connection_timer) {
            
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
        if (connection->bytesTransfered != -EAGAIN && result == 0 && connection->dataTransferOffset > 0) {
            
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
        
        #ifdef LOG2FILE    
            if (result != -EAGAIN) writeToLog("C[%d] data_callback using socket %d returned %d", connection->index+1, connection->data_socket, result);
        #endif
        
        // check errors
        if (result < 0 && result != -EAGAIN) {
            display("! ERROR : C[%d] data transfer callback using sokcet %d failed , socket error = %d", connection->index+1, connection->data_socket, result);
        }
    }

    if (result <= 0 && result != -EAGAIN) {
        cleanup_data_resources(connection);
        if (result < 0) {
            if (result != -99) {
                char msg[MAXPATHLEN] = "";
                sprintf(msg, "C[%d] closed, error occurred during transfer (%s)", connection->index+1, strerror(errno)); 
                display("! ERROR : %s", msg);                
                write_reply(connection, 520, msg);
            } 
        } else {
            
            char msg[MAXPATHLEN + 80] = "";
            if (connection->bytesTransfered == 0 && (strcmp(connection->fileName,"") != 0)) {
                if (connection->speed != 0)
                    sprintf(msg, "C[%d] %s Transfered sucessfully %.0fKB/s", connection->index+1, connection->fileName, connection->speed*1000); 
                else
                    sprintf(msg, "C[%d] %s Transfered sucessfully", connection->index+1, connection->fileName); 
            }
            else 
                sprintf(msg, "C[%d] command executed sucessfully", connection->index+1); 
            
            write_reply(connection, 226, msg);
        }
        // reset connection->fileName here
        strcpy(connection->fileName, "");

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
    
    bool network_down = !processConnections();
    
    if (!network_down) {
        uint32_t connection_index;
        float totalSpeedMBs = 0;
        
        for (connection_index = 0; connection_index < FTP_NB_SIMULTANEOUS_TRANSFERS; connection_index++) {
            connection_t *connection = connections[connection_index];
            if (connection) {
                if (connection->data_callback) {
                    process_data_events(connection);
                    if (connection->speed) totalSpeedMBs += connection->speed;
                } else {
                    process_control_events(connection);
                }
            }
        }
        if (totalSpeedMBs) {
            // increment nbAvgFiles and take speed into account for mean calculation
            nbSpeedMeasures += 1;
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

void setFsaFdInFtp(int hfd) {
    fsaFd = hfd;
}
