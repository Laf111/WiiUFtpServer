/****************************************************************************
  * WiiUFtpServer
  * 2021/04/05:V1.0.0:Laf111: import ftp-everywhere code
 ***************************************************************************/
#ifndef _VRT_H_
#define _VRT_H_

#ifdef __cplusplus
extern "C"{
#endif

#include <stdio.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/dirent.h>

typedef struct
{
    DIR *dir;
    char *path;
    uint8_t virt_root;
} DIR_P;

char *to_real_path(char *virtual_cwd, char *virtual_path);

FILE *vrt_fopen(char *cwd, char *path, char *mode);
int vrt_stat(char *cwd, char *path, struct stat *st);
int vrt_chdir(char *cwd, char *path);
int vrt_unlink(char *cwd, char *path);
int vrt_mkdir(char *cwd, char *path, mode_t mode);
int vrt_rename(char *cwd, char *from_path, char *to_path);
DIR_P *vrt_opendir(char *cwd, char *path);
struct dirent *vrt_readdir(DIR_P *iter);
int vrt_closedir(DIR_P *iter);

#ifdef __cplusplus
}
#endif

#endif /* _VRT_H_ */
