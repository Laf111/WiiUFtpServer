/****************************************************************************
  * WiiUFtpServer
  * 2021-12-05:Laf111:V7-0: files handler (asynchronous safe) with user buffer space optimization
 ***************************************************************************/
#ifndef _RECEIVEDFILES_H_
#define _RECEIVEDFILES_H_

#ifdef __cplusplus
extern "C"{
#endif

typedef struct transferedFile transferedFile;
struct transferedFile {
    
    // volume path for IOSUHAX /vol/storage_usb01/...
    // NULL for downloads !!!
    char *path;
    
    // user buffer for IO operations on the file
    char *userBuffer;
    
    uint32_t bufferSize;
    
    bool changeRights;
    
    // file stream pointer
    FILE * f;
    
};

// vPath : /storage_usb/path/saveinfo.xml -> vlPath : /vol/storage_usb01/path/saveinfo.xml
// This function allocate the memory returned.
// The caller must take care of freeing it
char*    virtualToVolPath(char *vPath);

// openFile : open the file, add file to the list with filling the data but WITHOUT ALLOCATING the user buffer's file
// mode : ab, rb, wb (change rights for modes ab and wb)
FILE*    openFile(char *cwd, char *path, char* mode);

// return the user buffer length (-1 if errors) and the buffer adress (buf) from file that use fd decriptor
int32_t    getUserBuffer(int32_t s, FILE *f, char **buf);

int32_t  closeFile(FILE *f);


#ifdef __cplusplus
}
#endif

#endif /* _RECEIVEDFILES_H_ */
