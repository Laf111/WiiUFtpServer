/****************************************************************************
  * WiiUFtpServer
  * 2021/04/05:V1.0.0:Laf111: import ftp-everywhere code
 ***************************************************************************/
#ifndef _VIRTUALPATH_H_
#define _VIRTUALPATH_H_

#ifdef __cplusplus
extern "C"{
#endif

#include <stdint.h>
#include <stdbool.h>

#include "iosuhax.h"
#include "iosuhax_devoptab.h"

typedef struct {
    char *name;
    char *alias;
    char *prefix;
    bool inserted;
} VIRTUAL_PARTITION;

extern VIRTUAL_PARTITION * VIRTUAL_PARTITIONS;
extern uint8_t MAX_VIRTUAL_PARTITIONS;

int    MountVirtualDevices();
void UnmountVirtualPaths();
void UmountVirtualDevices();

void ResetVirtualPaths();

#ifdef __cplusplus
}
#endif

#endif /* _VIRTUALPART_H_ */
