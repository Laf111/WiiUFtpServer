/****************************************************************************
  * WiiUFtpServer
 ***************************************************************************/
#include <padscore/kpad.h>
#include <padscore/wpad.h>
#include <vpad/input.h>

#ifndef _CONTROLLERS_H_
#define _CONTROLLERS_H_

void listenControlerEvent(VPADStatus *vpadStatus);
bool readUserAnswer(VPADStatus *vpadStatus);
bool checkController(VPADStatus *vpadStatus);

#endif


