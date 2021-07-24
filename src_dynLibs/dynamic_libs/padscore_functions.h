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

extern u32 padscore_handle;

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
    u32 btns_h;
    u32 btns_d;
    u32 btns_r;
    f32 acc_x;
    f32 acc_y;
    f32 acc_z;
    f32 acc_value;
    f32 acc_speed;
    f32 pos_x;
    f32 pos_y;
    f32 vec_x;
    f32 vec_y;
    f32 speed;
    f32 angle_x;
    f32 angle_y;
    f32 angle_vec_x;
    f32 angle_vec_y;
    f32 angle_speed;
    f32 dist;
    f32 dist_vec;
    f32 dist_speed;
    f32 acc_vertical_x;
    f32 acc_vertical_y;
    u8 device_type;
    s8 wpad_error;
    s8 pos_valid;
    u8 format;

    union {
        struct {
            f32 stick_x;
            f32 stick_y;
        } nunchuck;

        struct {
            u32 btns_h;
            u32 btns_d;
            u32 btns_r;
            f32 lstick_x;
            f32 lstick_y;
            f32 rstick_x;
            f32 rstick_y;
            f32 ltrigger;
            f32 rtrigger;
        } classic;

        struct {
            u32 btns_h;
            u32 btns_d;
            u32 btns_r;
            f32 lstick_x;
            f32 lstick_y;
            f32 rstick_x;
            f32 rstick_y;
            s32 charging;
            s32 wired;
        } pro;

        u32 unused_6[20];
    };
    u32 unused_7[16];
} KPADData;

typedef struct WPADReadData_ {
    u8 unknown[40];
    u8 dev;
    u8 err;
    u8 unknown1[2];
    u32 buttons;
    s16 l_stick_x;
    s16 l_stick_y;
    s16 r_stick_x;
    s16 r_stick_y;
    u8 unknown2[8];
    u8 fmt;
} WPADReadData;

typedef WPADReadData KPADUnifiedWpadData;

void InitPadScoreFunctionPointers(void);
void InitAcquirePadScore(void);

typedef void (* wpad_sampling_callback_t)(s32 chan);
typedef void (* wpad_extension_callback_t)(s32 chan, s32 status);
typedef void (* wpad_connect_callback_t)(s32 chan, s32 status);

extern void (* KPADInit)(void);
extern void (* WPADInit)(void);
extern s32 (* WPADProbe)(s32 chan, u32 * pad_type);
extern s32 (* WPADSetDataFormat)(s32 chan, s32 format);
extern void (* WPADEnableURCC)(s32 enable);
extern void (* WPADRead)(s32 chan, void * data);
extern s32 (* KPADRead)(s32 chan, KPADData * data, u32 size);
extern s32 (* KPADReadEx)(s32 chan, KPADData * data, u32 size, s32 *error);
extern void (*WPADSetAutoSleepTime)(u8 time);
extern void (*WPADDisconnect)( s32 chan );

#ifdef __cplusplus
}
#endif

#endif // __PAD_SCORE_FUNCTIONS_H_