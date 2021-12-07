/****************************************************************************
  * WiiUFtpServer
  * 2021-12-05:Laf111:V7-0: files handler (asynchronous safe) with user buffer space optimization
 ***************************************************************************/
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <coreinit/memdefaultheap.h>
#include <nsysnet/_socket.h>
#include <iosuhax.h>

#include "vrt.h"
#include "net.h"
#include "ftp.h"
#include "transferedFiles.h"

#define UNUSED    __attribute__((unused))

// iosuhax fd
static int fsaFd = -1;

extern void display(const char *fmt, ...);

#ifdef LOG2FILE
    extern void writeToLog(const char *fmt, ...);
#endif

    
// array of files to transfer
static transferedFile files[NB_SIMULTANEOUS_TRANSFERS];

static bool initDone = false;
            
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

#ifdef LOG2FILE
static void displayList()
{
    int n=1; 
    writeToLog("--------- Transfered files list -----------");
    for (n=0; n<NB_SIMULTANEOUS_TRANSFERS; n++) {
        if (files[n].userBuffer != NULL) 
            writeToLog("Slot %d : fd = %d, %d , %s", n, fileno(files[n].f), files[n].bufferSize, files[n].path);
        else
            if (files[n].f != NULL) 
                writeToLog("Slot %d : fd = %d, NULL , %s", n, fileno(files[n].f), files[n].path);
            else        
                writeToLog("Slot %d : fd = -1, NULL , %s", n, files[n].path);
    }
    writeToLog("-------------------------------------------");
}
#endif


// openFile : open the file, add file to the list with filling the data but WITHOUT ALLOCATING the user buffer's file
// mode : ab, rb, wb (change rights for modes ab and wb)
FILE* openFile(char *cwd, char *path, char *mode) {

    // compute virtual path /usb/... in a string allocate on the stack
    char vPath[MAXPATHLEN+26] = "";
    if (path) sprintf(vPath, "%s%s", cwd, path);
    else sprintf(vPath, "%s", cwd);

    char *volPath = NULL; 
    volPath = virtualToVolPath(vPath);
    if (volPath == NULL) {
        display("! ERROR : failed to allocate volpath for %s", vPath);
        return NULL;
    }
    
    int slot=-1;
    for (int n=0; n<NB_SIMULTANEOUS_TRANSFERS; n++) {
        if (initDone == true) {
            
            // search if this path is already saved 
            if (files[n].path != NULL) {
				if (strcmp(volPath, files[n].path) == 0)  {
	                slot=n;         
	                break;
	            }
			} else {
            
				// first free slot
                slot=n;         
                break;
			}
            
        } else {
            // init
            files[n].f = NULL;
            files[n].path = NULL;
			// init for DL
            files[n].changeRights = false;
            files[n].bufferSize = DL_USER_BUFFER_SIZE;
            files[n].userBuffer = NULL; 
            slot = 0;
        }
    }
    initDone = true;
    
    if (slot == -1) {
        display("! ERROR : no free slot found for %s !!!", path); 
        display("! ERROR : errno = %s", strerror(errno));
        #ifdef LOG2FILE
            displayList();
        #endif
        return NULL;
    }
    char *rPath = NULL; 
    rPath = to_real_path(cwd, path);
    if (!rPath) {
        display("! ERROR : to_real_path failed in openFile() for %s", path); 
        display("! ERROR : errno = %d (%s)", errno, strerror(errno));
	    free(volPath);
        return NULL;
    }
    
    files[slot].f = fopen(rPath, mode);
    if (!files[slot].f) {
        display("! ERROR : fopen failed in openFile() for %s", path); 
        display("! ERROR : errno = %d (%s)", errno, strerror(errno));
        free(volPath);
	    free(rPath);
        return NULL;
    }
    free(rPath);

    // allocate compute and store the volume path (ex /vol/storage_usb01/...)
    // needed for IOSUHAX operations    
    files[slot].path = volPath;
    
    // Upload : change rights + update buffer size
    if (strcmp(mode, "rb") != 0) {                
        
        // update buffer size
        files[slot].bufferSize = UL_USER_BUFFER_SIZE;
		
		files[slot].changeRights = true;
    }
    
#ifdef LOG2FILE
    writeToLog("Transfer slot %d opened : fd = %d, path=%s", slot, fileno(files[slot].f), path); 
#endif

    
#ifdef LOG2FILE
    displayList();    
#endif        
    
    return files[slot].f;
}

static void setExtraSocketOptimizations(int32_t s)
{
    int enable = 1;
     // Activate TCP SAck
    if (setsockopt(s, SOL_SOCKET, SO_TCPSACK, &enable, sizeof(enable))!=0) 
        {display("! ERROR : TCP SAck activation failed !");}
        
    // SO_OOBINLINE
    if (setsockopt(s, SOL_SOCKET, SO_OOBINLINE, &enable, sizeof(enable))!=0) 
        {display("! ERROR : Force to leave received OOB data in line failed !");}
        
    // TCP_NODELAY 
    if (setsockopt(s, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable))!=0) 
        {display("! ERROR : Disable Nagle's algorithm failed !");}

    // Suppress delayed ACKs
    if (setsockopt(s, IPPROTO_TCP, TCP_NOACKDELAY, &enable, sizeof(enable))!=0)
        {display("! ERROR : Suppress delayed ACKs failed !");}
}

// return the user buffer length (-1 if errors) and the buffer adress (buf) from file that use fd decriptor
int32_t getUserBuffer(int32_t s, FILE *f, char **buf) {
    int32_t buf_size = -1;

    // reverse loop
    for (int n=0; n<NB_SIMULTANEOUS_TRANSFERS; n++)
    {
        if (files[n].f == f && files[n].path != NULL)
        {
            buf_size = files[n].bufferSize;
            if (files[n].userBuffer == NULL) {
                // first call : set socket options, allocate the user's buffer file                

                // allocate user's buffer           
                files[n].userBuffer = MEMAllocFromDefaultHeapEx(files[n].bufferSize, 64);
                if (!files[n].userBuffer) {
                    display("! ERROR : failed to allocate user buffer for fd = %d", fileno(f));
                    return -ENOMEM;
                }
                
                // (the system double the value set)
                int sockbuf_size = buf_size/2;
            	if (!files[n].changeRights) {        
			        if (setsockopt(s, SOL_SOCKET, SO_SNDBUF, &sockbuf_size, sizeof(sockbuf_size))!=0)
			            {display("! ERROR : SNDBUF failed !");}

				} else {
                    setExtraSocketOptimizations(s);
			        if (setsockopt(s, SOL_SOCKET, SO_RCVBUF, &sockbuf_size, sizeof(sockbuf_size))!=0)
			            {display("! ERROR : RCVBUF failed !");}
                }
                
                // set user's file buffer
                if (setvbuf(files[n].f, files[n].userBuffer, _IOFBF, files[n].bufferSize) != 0) {
                    display("! WARNING : setvbuf failed for fd = %d", fileno(f));
                    display("! WARNING : errno = %d (%s)", errno, strerror(errno));          
                }
            }
            
            *buf = files[n].userBuffer;
            break;
        }
    }

    if (buf_size == -1) {
		display("! ERROR : failed to get user buffer for %d", fileno(f));
        #ifdef LOG2FILE
            displayList();
        #endif
	}		
    return buf_size;
}

int32_t closeFile(FILE *f) {

    int result = -1;
    
    for (int n=0; n<NB_SIMULTANEOUS_TRANSFERS; n++) {
        if (files[n].f == f && files[n].path != NULL)
        {
            // chmod on file if asked
            if (files[n].changeRights) IOSUHAX_FSA_ChangeMode(fsaFd, files[n].path, 0x644);
            
            // close file
            if (fclose(files[n].f)<0) {
                display("! ERROR : when closing file with fd = %d", fileno(f));
            }
            
            files[n].f = NULL;            
            
            // free user buffer allocated
            if (files[n].userBuffer != NULL) MEMFreeToDefaultHeap(files[n].userBuffer);

            files[n].userBuffer = NULL;
            files[n].bufferSize = -1;
            
            // free path 
            if (files[n].path != NULL) free(files[n].path);
            files[n].path = NULL;
			
			files[n].changeRights = false;
			files[n].bufferSize = DL_USER_BUFFER_SIZE;

            result = 0;
            break;
        }
    }
#ifdef LOG2FILE
    if (result < 0) display("! ERROR : file f = %d not found???", fileno(f));
#endif
    
    return result;
}

