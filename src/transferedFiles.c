/****************************************************************************
  * WiiUFtpServer
  * 2021-10-20:Laf111:V6-3
 ***************************************************************************/
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#include "vrt.h"
#include "net.h"
#include "ftp.h"
#include "transferedFiles.h"
#include <iosuhax.h>

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
            writeToLog("Slot %d : fd = %d, %d , %s", n, files[n].fd, files[n].bufferSize, files[n].path);
        else
            writeToLog("Slot %d : fd = %d, NULL , %s", n, files[n].fd, files[n].path);
    }
    writeToLog("-------------------------------------------");
}
#endif


// function fo removing the transfered file corresponding to f
static int removeFile(FILE * f)
{
    int result = -1;
    
    int fd = fileno(f);
    for (int n=0; n<NB_SIMULTANEOUS_TRANSFERS; n++) {
        if (files[n].fd == fd && files[n].f == f && files[n].path != NULL)
        {
            // chmod on file if asked
            if (files[n].changeRights) IOSUHAX_FSA_ChangeMode(fsaFd, files[n].path, 0x644);
            
            // close file
            if (fclose(files[n].f)<0) {
                display("! ERROR : when closing file with fd = %d", files[n].fd);
            }
            
            files[n].f = NULL;            
            files[n].fd = -1;
            
            // free user buffer allocated
            if (files[n].userBuffer != NULL) free(files[n].userBuffer);
            files[n].userBuffer = NULL;
            files[n].bufferSize = -1;
            
            // free path 
            if (files[n].path != NULL) free(files[n].path);
            files[n].path = NULL;

            files[n].changeRights = true;
            
            result = 0;
            break;
        }
    }
#ifdef LOG2FILE
    if (result < 0) display("! ERROR : file with fd = %d not found???", fd);
#endif

    return result;
}


// openFile, mode : 1 = ab, 2 = rb, 3 = wb
// change rights for mode 1 and 3
FILE* openFile(char *cwd, char *path, uint32_t userBufferSize, char *mode) {

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
            if (files[n].path != NULL) if (strcmp(volPath, files[n].path) == 0)  {
                slot=n;         
                break;
            }
            
            // else search for the first free slot
            if (files[n].fd == -1 && files[n].path == NULL && files[n].userBuffer == NULL) {
                slot=n;         
                break;
            }
            
        } else {
            // init
            files[n].fd = -1;
            files[n].f = NULL;
            files[n].path = NULL;
            files[n].changeRights = true;
            files[n].bufferSize = -1;
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
        return NULL;
    }

    files[slot].f = fopen(rPath, mode);
    if (!files[slot].f) {
        display("! ERROR : fopen failed in openFile() for %s", path); 
        display("! ERROR : errno = %d (%s)", errno, strerror(errno));
        free(rPath);
        return NULL;
    }
    free(rPath);

    int fd = -1;
    fd = fileno(files[slot].f);    
        
    if (fd < 0) {
        display("! ERROR : failed to get fd for %s", vPath);
        fclose(files[slot].f);
        #ifdef LOG2FILE
            displayList();    
        #endif        
        return NULL;
    }
    
#ifdef LOG2FILE
    writeToLog("Transfer slot %d opened : fd = %d, path=%s", slot, fd, vPath); 
#endif
    
    // set the file descriptor
    files[slot].fd = fd;

    // allocate compute and store the volume path (ex /vol/storage_usb01/...)
    // needed for IOSUHAX operations    
    files[slot].path = volPath;

    // allocate and set the full volume path to the file
    files[slot].bufferSize = userBufferSize;
    files[slot].userBuffer=NULL;
    
    // init change right to true    
    if (strcmp(mode, "rb") == 0) files[slot].changeRights = false;
    
    return files[slot].f;
}

// return the user buffer length (-1 if errors) and the buffer adress (buf) from file that use fd decriptor
int32_t getUserBuffer(FILE *f, char **buf) {
    int32_t buf_size = -1;

    // reverse loop
    for (int n=0; n<NB_SIMULTANEOUS_TRANSFERS; n++)
    {
        if (files[n].f == f && files[n].path != NULL)
        {
            // first request : allocating and setting file's user buffer
            if (files[n].userBuffer == NULL) {            
                // buf_size
                buf_size = files[n].bufferSize+32;

                // align memory (64bytes = 0x40) when alocating the buffer
                do {
                    buf_size -= 32;
                    if (buf_size < 0) {
                        display("! ERROR : failed to allocate user buffer for %s", files[n].path);
                        return -ENOMEM;
                    }
                    files[n].userBuffer = (char *)memalign(0x40, buf_size);
                    if (files[n].userBuffer) memset(files[n].userBuffer, 0x00, buf_size);
                } while(!files[n].userBuffer);

                if (!files[n].userBuffer) {
                    display("! ERROR : failed to allocate user buffer for %s", files[n].path);
                    return -ENOMEM;
                }
            } else buf_size = files[n].bufferSize;
            
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

    // get the fd
    int fd = -1;
    fd = fileno(f);
    if (fd < 0) {
        display("! ERROR : failed to get fd in closeFile");
        display("! ERROR : errno = %d (%s)", errno, strerror(errno));
        return -1;
    }
    
    int ret = removeFile(f);        
        if (ret < 0)
        {
            display("! ERROR (%d) : when closing the file fd = %d", ret, fd);        
            display("! ERROR : errno = %d (%s)", errno, strerror(errno));
            #ifdef LOG2FILE
                displayList();
            #endif
        }
    
    return ret;
}

