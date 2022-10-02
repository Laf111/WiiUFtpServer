/****************************************************************************
 * Copyright (C) 2015
 * by Dimok
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you
 * must not claim that you wrote the original software. If you use
 * this software in a product, an acknowledgment in the product
 * documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and
 * must not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 ***************************************************************************/
#ifndef __PAD_SCORE_FUNCTIONS_H_
#define __PAD_SCORE_FUNCTIONS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "vpad_functions.h"

extern uint32_t padscore_handle;

#define WPAD_EXT_CORE           0
#define WPAD_EXT_NUNCHUK        1
#define WPAD_EXT_CLASSIC        2
#define WPAD_EXT_MPLUS          5
#define WPAD_EXT_MPLUS_NUNCHUK  6
#define WPAD_EXT_MPLUS_CLASSIC  7
#define WPAD_EXT_PRO_CONTROLLER 31

#define WPAD_FMT_PRO_CONTROLLER 22

#define WPAD_BUTTON_LEFT                    0x0001
#define WPAD_BUTTON_RIGHT                   0x0002
#define WPAD_BUTTON_DOWN                    0x0004
#define WPAD_BUTTON_UP                      0x0008
#define WPAD_BUTTON_PLUS                    0x0010
#define WPAD_BUTTON_2                       0x0100
#define WPAD_BUTTON_1                       0x0200
#define WPAD_BUTTON_B                       0x0400
#define WPAD_BUTTON_A                       0x0800
#define WPAD_BUTTON_MINUS                   0x1000
#define WPAD_BUTTON_Z                       0x2000
#define WPAD_BUTTON_C                       0x4000
#define WPAD_BUTTON_HOME                    0x8000

#define WPAD_CLASSIC_BUTTON_UP              0x0001
#define WPAD_CLASSIC_BUTTON_LEFT            0x0002
#define WPAD_CLASSIC_BUTTON_ZR              0x0004
#define WPAD_CLASSIC_BUTTON_X               0x0008
#define WPAD_CLASSIC_BUTTON_A               0x0010
#define WPAD_CLASSIC_BUTTON_Y               0x0020
#define WPAD_CLASSIC_BUTTON_B               0x0040
#define WPAD_CLASSIC_BUTTON_ZL              0x0080
#define WPAD_CLASSIC_BUTTON_R               0x0200
#define WPAD_CLASSIC_BUTTON_PLUS            0x0400
#define WPAD_CLASSIC_BUTTON_HOME            0x0800
#define WPAD_CLASSIC_BUTTON_MINUS           0x1000
#define WPAD_CLASSIC_BUTTON_L               0x2000
#define WPAD_CLASSIC_BUTTON_DOWN            0x4000
#define WPAD_CLASSIC_BUTTON_RIGHT           0x8000

#define WPAD_PRO_BUTTON_UP             0x00000001
#define WPAD_PRO_BUTTON_LEFT           0x00000002
#define WPAD_PRO_TRIGGER_ZR            0x00000004
#define WPAD_PRO_BUTTON_X              0x00000008
#define WPAD_PRO_BUTTON_A              0x00000010
#define WPAD_PRO_BUTTON_Y              0x00000020
#define WPAD_PRO_BUTTON_B              0x00000040
#define WPAD_PRO_TRIGGER_ZL            0x00000080
#define WPAD_PRO_RESERVED              0x00000100
#define WPAD_PRO_TRIGGER_R             0x00000200
#define WPAD_PRO_BUTTON_PLUS           0x00000400
#define WPAD_PRO_BUTTON_HOME           0x00000800
#define WPAD_PRO_BUTTON_MINUS          0x00001000
#define WPAD_PRO_TRIGGER_L             0x00002000
#define WPAD_PRO_BUTTON_DOWN           0x00004000
#define WPAD_PRO_BUTTON_RIGHT          0x00008000
#define WPAD_PRO_BUTTON_STICK_R        0x00010000
#define WPAD_PRO_BUTTON_STICK_L        0x00020000

#define WPAD_PRO_STICK_L_EMULATION_UP        0x00200000
#define WPAD_PRO_STICK_L_EMULATION_DOWN      0x00100000
#define WPAD_PRO_STICK_L_EMULATION_LEFT      0x00040000
#define WPAD_PRO_STICK_L_EMULATION_RIGHT     0x00080000

#define WPAD_PRO_STICK_R_EMULATION_UP        0x02000000
#define WPAD_PRO_STICK_R_EMULATION_DOWN      0x01000000
#define WPAD_PRO_STICK_R_EMULATION_LEFT      0x00400000
#define WPAD_PRO_STICK_R_EMULATION_RIGHT     0x00800000

typedef struct _KPADData {
    uint32_t btns_h;
    uint32_t btns_d;
    uint32_t btns_r;
    float acc_x;
    float acc_y;
    float acc_z;
    float acc_value;
    float acc_speed;
    float pos_x;
    float pos_y;
    float vec_x;
    float vec_y;
    float speed;
    float angle_x;
    float angle_y;
    float angle_vec_x;
    float angle_vec_y;
    float angle_speed;
    float dist;
    float dist_vec;
    float dist_speed;
    float acc_vertical_x;
    float acc_vertical_y;
    uint8_t device_type;
    int8_t wpad_error;
    int8_t pos_valid;
    uint8_t format;

    union {
        struct {
            float stick_x;
            float stick_y;
        } nunchuck;

        struct {
            uint32_t btns_h;
            uint32_t btns_d;
            uint32_t btns_r;
            float lstick_x;
            float lstick_y;
            float rstick_x;
            float rstick_y;
            float ltrigger;
            float rtrigger;
        } classic;

        struct {
            uint32_t btns_h;
            uint32_t btns_d;
            uint32_t btns_r;
            float lstick_x;
            float lstick_y;
            float rstick_x;
            float rstick_y;
            int32_t charging;
            int32_t wired;
        } pro;

        uint32_t unused_6[20];
    };
    uint32_t unused_7[16];
} KPADData;

typedef struct WPADReadData_ {
    uint8_t unknown[40];
    uint8_t dev;
    uint8_t err;
    uint8_t unknown1[2];
    uint32_t buttons;
    int16_t l_stick_x;
    int16_t l_stick_y;
    int16_t r_stick_x;
    int16_t r_stick_y;
    uint8_t unknown2[8];
    uint8_t fmt;
} WPADReadData;

typedef WPADReadData KPADUnifiedWpadData;

void InitPadScoreFunctionPointers(void);
void InitAcquirePadScore(void);

typedef void (* wpad_sampling_callback_t)(int32_t chan);
typedef void (* wpad_extension_callback_t)(int32_t chan, int32_t status);
typedef void (* wpad_connect_callback_t)(int32_t chan, int32_t status);

extern void (* KPADInit)(void);
extern void (* WPADInit)(void);
extern int32_t (* WPADProbe)(int32_t chan, uint32_t * pad_type);
extern int32_t (* WPADSetDataFormat)(int32_t chan, int32_t format);
extern void (* WPADEnableURCC)(int32_t enable);
extern void (* WPADRead)(int32_t chan, void * data);
extern int32_t (* KPADRead)(int32_t chan, KPADData * data, uint32_t size);
extern int32_t (* KPADReadEx)(int32_t chan, KPADData * data, uint32_t size, int32_t *error);
extern void (*WPADSetAutoSleepTime)(uint8_t time);
extern void (*WPADDisconnect)( int32_t chan );

#ifdef __cplusplus
}
#endif

#endif // __PAD_SCORE_FUNCTIONS_H_