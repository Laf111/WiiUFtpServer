/****************************************************************************
  * WiiUFtpServer
 ***************************************************************************/
#ifndef _FTP_H_
#define _FTP_H_

#include <whb/log.h>
#include <whb/log_console.h>

#include <time.h>
#include <stdint.h>
#include <stdbool.h>

#define FTP_PORT            21

#ifdef __cplusplus
extern "C"{
#endif

void setVerboseMode(bool flag);
int32_t create_server(uint16_t port);
bool process_ftp_events();
void cleanup_ftp();

void setOsTime(struct tm *tmTime);
void setFsaFd(int hfd);
int getFsaFd();

#ifdef __cplusplus
}
#endif

#endif /* _FTP_H_ */
