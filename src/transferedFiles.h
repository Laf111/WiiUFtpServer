/****************************************************************************
  * WiiUFtpServer
  * 2021-10-20:Laf111:V6-3
 ***************************************************************************/
#ifndef _RECEIVEDFILES_H_
#define _RECEIVEDFILES_H_

#ifdef __cplusplus
extern "C"{
#endif

typedef struct transferedFile transferedFile;
struct transferedFile {
    
    // volume path for IOSUHAX /vol/storage_usb01/...
    char *path;
    
    // user buffer for IO operations on the file
    char *userBuffer;
    
    uint32_t bufferSize;
    
    bool changeRights;
    
    // file descriptor
    int fd;
    
    // file stream pointer
    FILE * f;
    
};

// vPath : /storage_usb/path/saveinfo.xml -> vlPath : /vol/storage_usb01/path/saveinfo.xml
// This function allocate the memory returned.
// The caller must take care of freeing it
char*    virtualToVolPath(char *vPath);

// openFile : open the file, add file to the list with filling the data but WITHOUT ALLOCATING the user buffer's file
// mode : ab, rb, wb (change rights for modes ab and wb)
FILE*    openFile(char *cwd, char *path, uint32_t userBufferSize, char* mode);

// return the user buffer length (-1 if errors) and the buffer adress (buf) from file that use fd decriptor
int32_t    getUserBuffer(FILE *f, char **buf);

int32_t  closeFile(FILE *f);


#ifdef __cplusplus
}
#endif

#endif /* _RECEIVEDFILES_H_ */
