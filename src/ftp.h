/****************************************************************************
  * WiiUFtpServer
  * 2021/04/05:V1.0.0:Laf111: import ftp-everywhere code
 ***************************************************************************/
#ifndef _FTP_H_
#define _FTP_H_

#include <whb/log.h>
#include <whb/log_console.h>

#include <time.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C"{
#endif

#define gettime()               OSGetTime()
void accept_ftp_client(int32_t server);
void set_ftp_password(char *new_password);
bool process_ftp_events(int32_t server);
void cleanup_ftp();

void setOsTime(struct tm *tmTime);
void setFsaFd(int hfd);
int getFsaFd();

#ifdef __cplusplus
}
#endif

#endif /* _FTP_H_ */
