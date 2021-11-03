/****************************************************************************
  * WiiUFtpServer
  * 2021-10-20:Laf111:V6-3
 ***************************************************************************/
#include <malloc.h>
#include <string.h>
#include <unistd.h>
#include <coreinit/thread.h>
#include <coreinit/fastmutex.h>

#include "ftp.h"
#include "receivedFiles.h"
#include <iosuhax.h>

// iosuhax fd
static int fsaFd = -1;

OSFastMutex rfMutex;

// array of ENTRY struct
static struct ENTRY **files = NULL;
static int nbFiles = 0;

// module functions

// TODO : implement a mutex , use another way for allocating ? mutex also for display ? 
void SetVolPath(char *volPath, int fd) {
    
    if (volPath != NULL) {
        while(!OSFastMutex_TryLock(&rfMutex));
        if (files == NULL) {
            fsaFd = getFsaFd();
            
            OSFastMutex_Init(&rfMutex, "Set received file path mutex");
            OSFastMutex_Unlock(&rfMutex);
    
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
        OSFastMutex_Unlock(&rfMutex);

    }
}


int ChmodFile(int fd) {

    if (files != NULL) {
        // backward loop on the array elements
        int i=0;
        while(!OSFastMutex_TryLock(&rfMutex));
        for (i=nbFiles-1; i>=0; i--) {
            // if fd is found
            if (files[i]->fd == fd) {

                // chmod on file
                IOSUHAX_FSA_ChangeMode(fsaFd, files[i]->path, 0x644);

                // free the ENTRY

                // free path first
                if (files[i]->path != NULL) free(files[i]->path);
                // free ENTRY
                if (files[i] != NULL) free(files[i]);
                if (nbFiles > 0) nbFiles=nbFiles-1;
                else if (files != NULL) free(files);

                OSFastMutex_Unlock(&rfMutex);
                return 0;
            }
        }
        OSFastMutex_Unlock(&rfMutex);
    }
    return -1;
}
