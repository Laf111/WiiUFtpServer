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
#ifndef __VPAD_FUNCTIONS_H_
#define __VPAD_FUNCTIONS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <common/types.h>

extern u32 vpad_handle;
extern u32 vpadbase_handle;

#define VPAD_BUTTON_A        0x8000
#define VPAD_BUTTON_B        0x4000
#define VPAD_BUTTON_X        0x2000
#define VPAD_BUTTON_Y        0x1000
#define VPAD_BUTTON_LEFT     0x0800
#define VPAD_BUTTON_RIGHT    0x0400
#define VPAD_BUTTON_UP       0x0200
#define VPAD_BUTTON_DOWN     0x0100
#define VPAD_BUTTON_ZL       0x0080
#define VPAD_BUTTON_ZR       0x0040
#define VPAD_BUTTON_L        0x0020
#define VPAD_BUTTON_R        0x0010
#define VPAD_BUTTON_PLUS     0x0008
#define VPAD_BUTTON_MINUS    0x0004
#define VPAD_BUTTON_HOME     0x0002
#define VPAD_BUTTON_SYNC     0x0001
#define VPAD_BUTTON_STICK_R  0x00020000
#define VPAD_BUTTON_STICK_L  0x00040000
#define VPAD_BUTTON_TV       0x00010000

#define VPAD_STICK_R_EMULATION_LEFT     0x04000000
#define VPAD_STICK_R_EMULATION_RIGHT    0x02000000
#define VPAD_STICK_R_EMULATION_UP       0x01000000
#define VPAD_STICK_R_EMULATION_DOWN     0x00800000

#define VPAD_STICK_L_EMULATION_LEFT     0x40000000
#define VPAD_STICK_L_EMULATION_RIGHT    0x20000000
#define VPAD_STICK_L_EMULATION_UP       0x10000000
#define VPAD_STICK_L_EMULATION_DOWN     0x08000000

//! Own definitions
#define VPAD_BUTTON_TOUCH               0x00080000
#define VPAD_MASK_EMULATED_STICKS       0x7F800000
#define VPAD_MASK_BUTTONS               ~VPAD_MASK_EMULATED_STICKS

typedef enum VPADTPResolution {
    VPAD_TP_1920x1080,
    VPAD_TP_1280x720,
    VPAD_TP_854x480
} VPADTPResolution;

typedef enum VPADGyroZeroDriftMode {
    VPAD_GYRO_ZERODRIFT_LOOSE,
    VPAD_GYRO_ZERODRIFT_STANDARD,
    VPAD_GYRO_ZERODRIFT_TIGHT
} VPADGyroZeroDriftMode;

typedef struct {
    f32 x,y;
} Vec2D;

typedef struct {
    f32 x,y,z;
} Vec3D;

typedef struct {
    Vec3D X,Y,Z;
} VPADDir;

typedef struct {
    u16 x, y;               /* Touch coordinates */
    u16 touched;            /* 1 = Touched, 0 = Not touched */
    u16 invalid;            /* 0 = All valid, 1 = X invalid, 2 = Y invalid, 3 = Both invalid? */
} VPADTPData;

typedef struct {
    s16 offsetX;
    s16 offsetY;
    f32 scaleX;
    f32 scaleY;
} VPADTPCalibrationParam;

typedef struct {
    u32 btns_h;                  /* Held buttons */
    u32 btns_d;                  /* Buttons that are pressed at that instant */
    u32 btns_r;                  /* Released buttons */
    Vec2D lstick, rstick;        /* Each contains 4-byte X and Y components */
    Vec3D acc;                   /* Status of DRC accelerometer */
    f32 acc_magnitude;           /* Accelerometer magnitude */
    f32 acc_variation;           /* Accelerometer variation */
    Vec2D acc_vertical;          /* Vertical */
    Vec3D gyro;                  /* Gyro data */
    Vec3D angle;                 /* Angle data */
    s8 error;                    /* Error */
    VPADTPData tpdata;           /* Normal touchscreen data */
    VPADTPData tpdata1;          /* Modified touchscreen data 1 */
    VPADTPData tpdata2;          /* Modified touchscreen data 2 */
    VPADDir dir;                 /* Orientation in three-dimensional space */
    s32 headphone;               /* Set to TRUE if headphones are plugged in, FALSE otherwise */
    Vec3D mag;                   /* Magnetometer data */
    u8 volume;                   /* 0 to 255 */
    u8 battery;                  /* 0 to 6 */
    u8 mic;                      /* Microphone status */
    u8 unk_volume;               /* One less than volume */
    u8 paddings[7];
} VPADData;

void InitVPadFunctionPointers(void);
void InitAcquireVPad(void);

extern void (* VPADInit)(void);
extern void (* VPADShutdown)(void);
extern s32 (* VPADRead)(s32 chan, VPADData *buffer, u32 buffer_size, s32 *error);
extern void (* VPADSetAccParam)(s32 chan, f32 play_radius, f32 sensitivity);
extern void (* VPADGetAccParam)(s32 chan, f32 *play_radius, f32 *sensitivity);
extern void (* VPADSetBtnRepeat)(s32 chan, f32 delay_sec, f32 pulse_sec);
extern void (* VPADEnableStickCrossClamp)(s32 chan);
extern void (* VPADDisableStickCrossClamp)(s32 chan);
extern void (* VPADSetLStickClampThreshold)(s32 chan, s32 max, s32 min);
extern void (* VPADSetRStickClampThreshold)(s32 chan, s32 max, s32 min);
extern void (* VPADGetLStickClampThreshold)(s32 chan, s32* max, s32* min);
extern void (* VPADGetRStickClampThreshold)(s32 chan, s32* max, s32* min);
extern void (* VPADSetStickOrigin)(s32 chan);
extern void (* VPADDisableLStickZeroClamp)(s32 chan);
extern void (* VPADDisableRStickZeroClamp)(s32 chan);
extern void (* VPADEnableLStickZeroClamp)(s32 chan);
extern void (* VPADEnableRStickZeroClamp)(s32 chan);
extern void (* VPADSetCrossStickEmulationParamsL)(s32 chan, f32 rot_deg, f32 xy_deg, f32 radius);
extern void (* VPADSetCrossStickEmulationParamsR)(s32 chan, f32 rot_deg, f32 xy_deg, f32 radius);
extern void (* VPADGetCrossStickEmulationParamsL)(s32 chan, f32* rot_deg, f32* xy_deg, f32* radius);
extern void (* VPADGetCrossStickEmulationParamsR)(s32 chan, f32* rot_deg, f32* xy_deg, f32* radius);
extern void (* VPADSetGyroAngle)(s32 chan, f32 ax, f32 ay, f32 az);
extern void (* VPADSetGyroDirection)(s32 chan, VPADDir *dir);
extern void (* VPADSetGyroDirectionMag)(s32 chan, f32 mag);
extern void (* VPADSetGyroMagnification)(s32 chan, f32 pitch, f32 yaw, f32 roll);
extern void (* VPADEnableGyroZeroPlay)(s32 chan);
extern void (* VPADEnableGyroDirRevise)(s32 chan);
extern void (* VPADEnableGyroAccRevise)(s32 chan);
extern void (* VPADDisableGyroZeroPlay)(s32 chan);
extern void (* VPADDisableGyroDirRevise)(s32 chan);
extern void (* VPADDisableGyroAccRevise)(s32 chan);
extern f32 (* VPADIsEnableGyroZeroPlay)(s32 chan);
extern f32 (* VPADIsEnableGyroZeroDrift)(s32 chan);
extern f32 (* VPADIsEnableGyroDirRevise)(s32 chan);
extern f32 (* VPADIsEnableGyroAccRevise)(s32 chan);
extern void (* VPADSetGyroZeroPlayParam)(s32 chan, f32 radius);
extern void (* VPADSetGyroDirReviseParam)(s32 chan, f32 revis_pw);
extern void (* VPADSetGyroAccReviseParam)(s32 chan, f32 revise_pw, f32 revise_range);
extern void (* VPADSetGyroDirReviseBase)(s32 chan, VPADDir *base);
extern void (* VPADGetGyroZeroPlayParam)(s32 chan, f32 *radius);
extern void (* VPADGetGyroDirReviseParam)(s32 chan, f32 *revise_pw);
extern void (* VPADGetGyroAccReviseParam)(s32 chan, f32 *revise_pw, f32 *revise_range);
extern void (* VPADInitGyroZeroPlayParam)(s32 chan);
extern void (* VPADInitGyroDirReviseParam)(s32 chan);
extern void (* VPADInitGyroAccReviseParam)(s32 chan);
extern void (* VPADInitGyroZeroDriftMode)(s32 chan);
extern void (* VPADSetGyroZeroDriftMode)(s32 chan, VPADGyroZeroDriftMode mode);
extern void (* VPADGetGyroZeroDriftMode)(s32 chan, VPADGyroZeroDriftMode *mode);
extern s16 (* VPADCalcTPCalibrationParam)(VPADTPCalibrationParam* param, u16 rawX1, u16 rawY1, u16 x1, u16 y1, u16 rawX2, u16 rawY2, u16 x2, u16 y2);
extern void (* VPADSetTPCalibrationParam)(s32 chan, const VPADTPCalibrationParam param);
extern void (* VPADGetTPCalibrationParam)(s32 chan, VPADTPCalibrationParam* param);
extern void (* VPADGetTPCalibratedPoint)(s32 chan, VPADTPData *disp, const VPADTPData *raw);
extern void (* VPADGetTPCalibratedPointEx)(s32 chan, VPADTPResolution tpReso, VPADTPData *disp, const VPADTPData *raw);
extern s32 (* VPADControlMotor)(s32 chan, u8* pattern, u8 length);
extern void (* VPADStopMotor)(s32 chan);
extern s32 (* VPADSetLcdMode)(s32 chan, s32 lcdmode);
extern s32 (* VPADGetLcdMode)(s32 chan, s32 *lcdmode);
extern s32 (* VPADBASEGetMotorOnRemainingCount)(s32 lcdmode);
extern s32 (* VPADBASESetMotorOnRemainingCount)(s32 lcdmode, s32 counter);
extern void (* VPADBASESetSensorBarSetting)(s32 chan, s8 setting);
extern void (* VPADBASEGetSensorBarSetting)(s32 chan, s8 *setting);
extern s32 (*VPADSetSensorBar)(s32 chan, bool on);

typedef void(*samplingCallback)(s32  chan);
extern samplingCallback ( *VPADSetSamplingCallback)(s32 chan, samplingCallback callback);


#ifdef __cplusplus
}
#endif

#endif // __VPAD_FUNCTIONS_H_
