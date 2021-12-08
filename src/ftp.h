/*

ftpii -- an FTP server for the Wii

Copyright (C) 2008 Joseph Jordan <joe.ftpii@psychlaw.com.au>

This software is provided 'as-is', without any express or implied warranty.
In no event will the authors be held liable for any damages arising from
the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

1.The origin of this software must not be misrepresented; you must not
claim that you wrote the original software. If you use this software in a
product, an acknowledgment in the product documentation would be
appreciated but is not required.

2.Altered source versions must be plainly marked as such, and must not be
misrepresented as being the original software.

3.This notice may not be removed or altered from any source distribution.

*/
/****************************************************************************
  * WiiUFtpServer
  * 2021-12-05:Laf111:V7-0: complete the some TODO left
 ***************************************************************************/

#ifndef _FTP_H_
#define _FTP_H_

#include <whb/log.h>
#include <whb/log_console.h>

#include <time.h>
#include <stdint.h>
#include <stdbool.h>

#define FTP_PORT            21

// Number max of simultaneous connections from the client : 
// 1 for communication with the client + NB_SIMULTANEOUS_TRANSFERS
#define FTP_NB_SIMULTANEOUS_TRANSFERS 1+NB_SIMULTANEOUS_TRANSFERS

#ifdef __cplusplus
extern "C"{
#endif

void    setVerboseMode(bool flag);
int32_t create_server(uint16_t port);
bool    process_ftp_events();
void    cleanup_ftp();

void    setOsTime(struct tm *tmTime);
void    setFsaFd(int hfd);
int     getFsaFd();

#ifdef __cplusplus
}
#endif

#endif /* _FTP_H_ */
