/****************************************************************************
  * WiiUFtpServer
  * 2021/04/22:V1.1.0:Laf111: creation
 ***************************************************************************/
#ifndef _RECEIVEDFILES_H_
#define _RECEIVEDFILES_H_

#ifdef __cplusplus
extern "C"{
#endif

struct ENTRY {
    // volume path for IOSUHAX /vol/storage_usb01/...
    char *path;
    // file descriptor 
    int fd;
    // optimal buffer size
    int optBufSize;
};

void   SetVolPath(char *volPath, int fd);
int    ChmodFile(int fd);

#ifdef __cplusplus
}
#endif

#endif /* _RECEIVEDFILES_H_ */
