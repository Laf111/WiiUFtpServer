/****************************************************************************
  * WiiUFtpServer
  * 2021-12-05:Laf111:V7-0
 ***************************************************************************/
#include <stdbool.h>
#include <string.h>

#include <whb/log.h>
#include <whb/log_console.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
 
#include "controllers.h"

extern void display(const char *fmt, ...);


static bool checkButton(VPADStatus *vpadStatus, int button) {
    bool status = false;
    bool exitWhile = false;
    
    int cpt=0;
    while (!exitWhile && (cpt < 5000))
    {
        listenControlerEvent(vpadStatus);

        // check button pressed and/or hold
        if ((vpadStatus->trigger | vpadStatus->hold) & button) {
            status = true;
            exitWhile = true;
            break;
        }
        OSSleepTicks(OSMillisecondsToTicks(1));
        cpt=cpt+1;
    }
    if (cpt >= 5000) display("~ WARNING : controller A BUTTON check timed out !");
    if (!exitWhile) display("! ERROR : controller A BUTTON check failed !");
    
    return status;
}    



//--------------------------------------------------------------------------
void listenControlerEvent (VPADStatus *vpadStatus) {

    VPADReadError vpadError = -1;
    VPADRead(0, vpadStatus, 1, &vpadError);
    switch (vpadError) {
        case VPAD_READ_SUCCESS: {
            break;
        }
        case VPAD_READ_NO_SAMPLES: {
            break;
        }
        case VPAD_READ_INVALID_CONTROLLER: {
            display("Controller disconnected!");
            break;
        }
        default: {
            display("Unknown PAD error! %08X", vpadError);
            return;
        }
    }
	// do not remove this draw ! 
    WHBLogConsoleDraw();
    for (int i = 0; i < 4; i++)
    {
        uint32_t controllerType;
        // check if the controller is connected
        if (WPADProbe(i, &controllerType) != 0)
            continue;

        KPADStatus kpadStatus;
        memset(&kpadStatus, 0, sizeof(kpadStatus));

        if (KPADRead(i, &kpadStatus, 1) < 0)  {
                display("Unknown KPAD error!");
                return;
        } else {

            // switch on controler type, map wpadStatus, kpadStatus to vpadStatus
            switch (controllerType) {

                case WPAD_EXT_CLASSIC:
                {
                    if ((kpadStatus.classic.trigger | kpadStatus.classic.hold) & WPAD_CLASSIC_BUTTON_A) {
                        vpadStatus->trigger = VPAD_BUTTON_A;
                        vpadStatus->hold = VPAD_BUTTON_A;
                    }
                    if ((kpadStatus.classic.trigger | kpadStatus.classic.hold) & WPAD_CLASSIC_BUTTON_B) {
                        vpadStatus->trigger = VPAD_BUTTON_B;
                        vpadStatus->hold = VPAD_BUTTON_B;
                    }
                    if ((kpadStatus.classic.trigger | kpadStatus.classic.hold) & WPAD_CLASSIC_BUTTON_UP) {
                        vpadStatus->trigger = VPAD_BUTTON_UP;
                        vpadStatus->hold = VPAD_BUTTON_UP;
                    }
                    if ((kpadStatus.classic.trigger | kpadStatus.classic.hold) & WPAD_CLASSIC_BUTTON_DOWN) {
                        vpadStatus->trigger = VPAD_BUTTON_DOWN;
                        vpadStatus->hold = VPAD_BUTTON_DOWN;
                    }
                    if ((kpadStatus.classic.trigger | kpadStatus.classic.hold) & WPAD_CLASSIC_BUTTON_LEFT) {
                        vpadStatus->trigger = VPAD_BUTTON_LEFT;
                        vpadStatus->hold = VPAD_BUTTON_LEFT;
                    }
                    if ((kpadStatus.classic.trigger | kpadStatus.classic.hold) & WPAD_CLASSIC_BUTTON_RIGHT) {
                        vpadStatus->trigger = VPAD_BUTTON_RIGHT;
                        vpadStatus->hold = VPAD_BUTTON_RIGHT;
                    }
                    if ((kpadStatus.classic.trigger | kpadStatus.classic.hold) & WPAD_CLASSIC_BUTTON_HOME) {
                        vpadStatus->trigger = WPAD_CLASSIC_BUTTON_HOME;
                        vpadStatus->hold = WPAD_CLASSIC_BUTTON_HOME;
                    }
                    break;
                }
                case WPAD_EXT_PRO_CONTROLLER:
                {
                    if ((kpadStatus.pro.trigger | kpadStatus.pro.hold) & WPAD_PRO_BUTTON_A) {
                        vpadStatus->trigger = VPAD_BUTTON_A;
                        vpadStatus->hold = VPAD_BUTTON_A;
                    }
                    if ((kpadStatus.pro.trigger | kpadStatus.pro.hold) & WPAD_PRO_BUTTON_B) {
                        vpadStatus->trigger = VPAD_BUTTON_B;
                        vpadStatus->hold = VPAD_BUTTON_B;
                    }
                    if ((kpadStatus.pro.trigger | kpadStatus.pro.hold) & WPAD_PRO_BUTTON_UP) {
                        vpadStatus->trigger = VPAD_BUTTON_UP;
                        vpadStatus->hold = VPAD_BUTTON_UP;
                    }
                    if ((kpadStatus.pro.trigger | kpadStatus.pro.hold) & WPAD_PRO_BUTTON_DOWN) {
                        vpadStatus->trigger = VPAD_BUTTON_DOWN;
                        vpadStatus->hold = VPAD_BUTTON_DOWN;
                    }
                    if ((kpadStatus.pro.trigger | kpadStatus.pro.hold) & WPAD_PRO_BUTTON_HOME) {
                        vpadStatus->trigger = VPAD_BUTTON_HOME;
                        vpadStatus->hold = VPAD_BUTTON_HOME;
                    }
                    if ((kpadStatus.pro.trigger | kpadStatus.pro.hold) & WPAD_PRO_BUTTON_LEFT) {
                        vpadStatus->trigger = VPAD_BUTTON_LEFT;
                        vpadStatus->hold = VPAD_BUTTON_LEFT;
                    }
                    if ((kpadStatus.pro.trigger | kpadStatus.pro.hold) & WPAD_PRO_BUTTON_RIGHT) {
                        vpadStatus->trigger = VPAD_BUTTON_RIGHT;
                        vpadStatus->hold = VPAD_BUTTON_RIGHT;
                    }
                    break;
                }

                case WPAD_EXT_CORE:
                case WPAD_EXT_MPLUS:
                case WPAD_EXT_MPLUS_CLASSIC:
                case WPAD_EXT_MPLUS_NUNCHUK:
                {
                    if ((kpadStatus.trigger | kpadStatus.hold) & WPAD_BUTTON_A) {
                        vpadStatus->trigger = VPAD_BUTTON_A;
                        vpadStatus->hold = VPAD_BUTTON_A;
                    }
                    if ((kpadStatus.trigger | kpadStatus.hold) & WPAD_BUTTON_B) {
                        vpadStatus->trigger = VPAD_BUTTON_B;
                        vpadStatus->hold = VPAD_BUTTON_B;
                    }
                    if ((kpadStatus.trigger | kpadStatus.hold) & WPAD_BUTTON_UP) {
                        vpadStatus->trigger = VPAD_BUTTON_UP;
                        vpadStatus->hold = VPAD_BUTTON_UP;
                    }
                    if ((kpadStatus.trigger | kpadStatus.hold) & WPAD_BUTTON_DOWN) {
                        vpadStatus->trigger = VPAD_BUTTON_DOWN;
                        vpadStatus->hold = VPAD_BUTTON_DOWN;
                    }
                    if ((kpadStatus.trigger | kpadStatus.hold) & WPAD_BUTTON_LEFT) {
                        vpadStatus->trigger = VPAD_BUTTON_LEFT;
                        vpadStatus->hold = VPAD_BUTTON_LEFT;
                    }
                    if ((kpadStatus.trigger | kpadStatus.hold) & WPAD_BUTTON_RIGHT) {
                        vpadStatus->trigger = VPAD_BUTTON_RIGHT;
                        vpadStatus->hold = VPAD_BUTTON_RIGHT;
                    }
                    if ((kpadStatus.trigger | kpadStatus.hold) & WPAD_BUTTON_HOME) {
                        vpadStatus->trigger = VPAD_BUTTON_HOME;
                        vpadStatus->hold = VPAD_BUTTON_HOME;
                    }
                    break;
                }
                
                case WPAD_EXT_NUNCHUK:
                {
                    if ((kpadStatus.nunchuck.trigger | kpadStatus.nunchuck.hold) & WPAD_BUTTON_A) {
                        vpadStatus->trigger = VPAD_BUTTON_A;
                        vpadStatus->hold = VPAD_BUTTON_A;
                    }
                    if ((kpadStatus.nunchuck.trigger | kpadStatus.nunchuck.hold) & WPAD_BUTTON_B) {
                        vpadStatus->trigger = VPAD_BUTTON_B;
                        vpadStatus->hold = VPAD_BUTTON_B;
                    }
                    if ((kpadStatus.nunchuck.trigger | kpadStatus.nunchuck.hold) & WPAD_BUTTON_UP) {
                        vpadStatus->trigger = VPAD_BUTTON_UP;
                        vpadStatus->hold = VPAD_BUTTON_UP;
                    }
                    if ((kpadStatus.nunchuck.trigger | kpadStatus.nunchuck.hold) & WPAD_BUTTON_DOWN) {
                        vpadStatus->trigger = VPAD_BUTTON_DOWN;
                        vpadStatus->hold = VPAD_BUTTON_DOWN;
                    }
                    if ((kpadStatus.nunchuck.trigger | kpadStatus.nunchuck.hold) & WPAD_BUTTON_LEFT) {
                        vpadStatus->trigger = VPAD_BUTTON_LEFT;
                        vpadStatus->hold = VPAD_BUTTON_LEFT;
                    }
                    if ((kpadStatus.nunchuck.trigger | kpadStatus.nunchuck.hold) & WPAD_BUTTON_RIGHT) {
                        vpadStatus->trigger = VPAD_BUTTON_RIGHT;
                        vpadStatus->hold = VPAD_BUTTON_RIGHT;
                    }
                    if ((kpadStatus.nunchuck.trigger | kpadStatus.nunchuck.hold) & WPAD_BUTTON_HOME) {
                        vpadStatus->trigger = VPAD_BUTTON_HOME;
                        vpadStatus->hold = VPAD_BUTTON_HOME;
                    }
                    break;
                }
                default:
                {
                    display("! ERROR : controller not recognized");
                    break;
                }
            }
        }
    }
}

//--------------------------------------------------------------------------
bool readUserAnswer(VPADStatus *vpadStatus) {
    bool answer = false;

    while (1)
    {
        listenControlerEvent(vpadStatus);

        // check button pressed and/or hold
        if ((vpadStatus->trigger | vpadStatus->hold) & VPAD_BUTTON_A) {
            answer=true;
            break;
        }
        if ((vpadStatus->trigger | vpadStatus->hold) & VPAD_BUTTON_B) {
            answer = false;
            break;
        }
        OSSleepTicks(OSMillisecondsToTicks(100));
    }
    return answer;
}

//--------------------------------------------------------------------------
bool checkController(VPADStatus *vpadStatus) {
    bool status = false;
    bool globalStatus = true;
    
    // A BUTTTON
    display("Please PRESS A button...");
    status = checkButton(vpadStatus, VPAD_BUTTON_A);
    if (!status) {
        globalStatus = false;
        display("> KO !!!");
    } else display("> OK");

    // B BUTTTON
    display("Please PRESS B button...");
    status = checkButton(vpadStatus, VPAD_BUTTON_B);
    if (!status) {
        globalStatus = false;
        display("> KO !!!");
    } else display("> OK");
    
    // UP BUTTTON
    display("Please PRESS UP button...");
    status = checkButton(vpadStatus, VPAD_BUTTON_UP);
    if (!status) {
        globalStatus = false;
        display("> KO !!!");
    } else display("> OK");
    
    // DOWN BUTTTON
    display("Please PRESS DOWN button...");
    status = checkButton(vpadStatus, VPAD_BUTTON_DOWN);
    if (!status) {
        globalStatus = false;
        display("> KO !!!");
    } else display("> OK");

    // HOME BUTTTON
    display("Please PRESS HOME button...");
    status = checkButton(vpadStatus, VPAD_BUTTON_HOME);
    if (!status) {
        globalStatus = false;
        display("> KO !!!");
    } else display("> OK");
    
    // RIGHT BUTTTON
    display("Please PRESS RIGHT button...");
    status = checkButton(vpadStatus, VPAD_BUTTON_RIGHT);
    if (!status) {
        globalStatus = false;
        display("> KO !!!");
    } else display("> OK");
    
    // LEFT BUTTTON
    display("Please PRESS LEFT button...");
    status = checkButton(vpadStatus, VPAD_BUTTON_LEFT);
    if (!status) {
        globalStatus = false;
        display("> KO !!!");
    } else display("> OK");
            
    if (globalStatus) display("Current controller check ends successfully");
    else  display("! ERROR : Current controller check failed !");
    
    return globalStatus;
}


