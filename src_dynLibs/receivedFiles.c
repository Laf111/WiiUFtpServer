/****************************************************************************
  * WiiUFtpServer
  * 2021/04/22:V1.1.0:Laf111: creation this THREAD SAFE module
 ***************************************************************************/
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <dynamic_libs/os_functions.h>

#include "ftp.h"
#include "receivedFiles.h"
#include "iosuhax/iosuhax.h"

// iosuhax fd
static int fsaFd = -1;

// thread lock
static int _threadLocked = 0;

// array of ENTRY struct
static struct ENTRY **files = NULL;
static int nbFiles = 0;

// thread safety functions
static void lockThread() {
    while (_threadLocked) usleep(10);
    _threadLocked = 1;
}    
static void unlockThread() {
    _threadLocked = 0;
}    
// module functions
void SetVolPath(char *volPath, int fd) {
    if (volPath != NULL) {
        lockThread();
        if (files == NULL) {
            fsaFd = getFsaFd();
            files = (struct ENTRY **)malloc(sizeof(struct ENTRY *));
        } else {
            files = (struct ENTRY **)realloc(files, (nbFiles + 1) * sizeof(struct ENTRY *));
        }
        
        files[nbFiles] = (struct ENTRY*) malloc(sizeof(struct ENTRY));
        
        // set the file descriptor
        files[nbFiles]->fd = fd;

        
        // allocate and set the full volume path to the file        
        int len = strlen(volPath)+1;        
        files[nbFiles]->path = (char *) malloc(sizeof(char)*len);

        strcpy(files[nbFiles]->path, volPath);

        nbFiles=nbFiles+1;
        unlockThread();

    }
}    


int ChmodFile(int fd) {

    if (files != NULL) {
        // backward loop on the array elements
        int i=0;
        lockThread();
        for (i=nbFiles-1; i>=0; i--) {
            // if fd is found
            if (files[i]->fd == fd) {
                
                // chmod on file
                IOSUHAX_FSA_ChangeMode(fsaFd, files[i]->path, 0x666);

                // free the ENTRY
                
                // free path first
                free(files[i]->path);
                // free ENTRY
                free(files[i]);
                if (nbFiles > 0) nbFiles=nbFiles-1;
                else free(files);
                
                unlockThread();
                return 0;
            }
        }
        unlockThread();
    }
    return -1;
}    
