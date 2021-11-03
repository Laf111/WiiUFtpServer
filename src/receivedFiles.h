/****************************************************************************
  * WiiUFtpServer
  * 2021-10-20:Laf111:V6-3
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
};

void   SetVolPath(char *volPath, int fd);
int    ChmodFile(int fd);

#ifdef __cplusplus
}
#endif

#endif /* _RECEIVEDFILES_H_ */
