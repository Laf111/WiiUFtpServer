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

extern uint32_t vpad_handle;
extern uint32_t vpadbase_handle;

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
    float x,y;
} Vec2D;

typedef struct {
    float x,y,z;
} Vec3D;

typedef struct {
    Vec3D X,Y,Z;
} VPADDir;

typedef struct {
    uint16_t x, y;               /* Touch coordinates */
    uint16_t touched;            /* 1 = Touched, 0 = Not touched */
    uint16_t invalid;            /* 0 = All valid, 1 = X invalid, 2 = Y invalid, 3 = Both invalid? */
} VPADTPData;

typedef struct {
    int16_t offsetX;
    int16_t offsetY;
    float scaleX;
    float scaleY;
} VPADTPCalibrationParam;

typedef struct {
    uint32_t btns_h;                  /* Held buttons */
    uint32_t btns_d;                  /* Buttons that are pressed at that instant */
    uint32_t btns_r;                  /* Released buttons */
    Vec2D lstick, rstick;        /* Each contains 4-byte X and Y components */
    Vec3D acc;                   /* Status of DRC accelerometer */
    float acc_magnitude;           /* Accelerometer magnitude */
    float acc_variation;           /* Accelerometer variation */
    Vec2D acc_vertical;          /* Vertical */
    Vec3D gyro;                  /* Gyro data */
    Vec3D angle;                 /* Angle data */
    int8_t error;                    /* Error */
    VPADTPData tpdata;           /* Normal touchscreen data */
    VPADTPData tpdata1;          /* Modified touchscreen data 1 */
    VPADTPData tpdata2;          /* Modified touchscreen data 2 */
    VPADDir dir;                 /* Orientation in three-dimensional space */
    int32_t headphone;               /* Set to TRUE if headphones are plugged in, FALSE otherwise */
    Vec3D mag;                   /* Magnetometer data */
    uint8_t volume;                   /* 0 to 255 */
    uint8_t battery;                  /* 0 to 6 */
    uint8_t mic;                      /* Microphone status */
    uint8_t unk_volume;               /* One less than volume */
    uint8_t paddings[7];
} VPADData;

void InitVPadFunctionPointers(void);
void InitAcquireVPad(void);

extern void (* VPADInit)(void);
extern void (* VPADShutdown)(void);
extern int32_t (* VPADRead)(int32_t chan, VPADData *buffer, uint32_t buffer_size, int32_t *error);
extern void (* VPADSetAccParam)(int32_t chan, float play_radius, float sensitivity);
extern void (* VPADGetAccParam)(int32_t chan, float *play_radius, float *sensitivity);
extern void (* VPADSetBtnRepeat)(int32_t chan, float delay_sec, float pulse_sec);
extern void (* VPADEnableStickCrossClamp)(int32_t chan);
extern void (* VPADDisableStickCrossClamp)(int32_t chan);
extern void (* VPADSetLStickClampThreshold)(int32_t chan, int32_t max, int32_t min);
extern void (* VPADSetRStickClampThreshold)(int32_t chan, int32_t max, int32_t min);
extern void (* VPADGetLStickClampThreshold)(int32_t chan, int32_t* max, int32_t* min);
extern void (* VPADGetRStickClampThreshold)(int32_t chan, int32_t* max, int32_t* min);
extern void (* VPADSetStickOrigin)(int32_t chan);
extern void (* VPADDisableLStickZeroClamp)(int32_t chan);
extern void (* VPADDisableRStickZeroClamp)(int32_t chan);
extern void (* VPADEnableLStickZeroClamp)(int32_t chan);
extern void (* VPADEnableRStickZeroClamp)(int32_t chan);
extern void (* VPADSetCrossStickEmulationParamsL)(int32_t chan, float rot_deg, float xy_deg, float radius);
extern void (* VPADSetCrossStickEmulationParamsR)(int32_t chan, float rot_deg, float xy_deg, float radius);
extern void (* VPADGetCrossStickEmulationParamsL)(int32_t chan, float* rot_deg, float* xy_deg, float* radius);
extern void (* VPADGetCrossStickEmulationParamsR)(int32_t chan, float* rot_deg, float* xy_deg, float* radius);
extern void (* VPADSetGyroAngle)(int32_t chan, float ax, float ay, float az);
extern void (* VPADSetGyroDirection)(int32_t chan, VPADDir *dir);
extern void (* VPADSetGyroDirectionMag)(int32_t chan, float mag);
extern void (* VPADSetGyroMagnification)(int32_t chan, float pitch, float yaw, float roll);
extern void (* VPADEnableGyroZeroPlay)(int32_t chan);
extern void (* VPADEnableGyroDirRevise)(int32_t chan);
extern void (* VPADEnableGyroAccRevise)(int32_t chan);
extern void (* VPADDisableGyroZeroPlay)(int32_t chan);
extern void (* VPADDisableGyroDirRevise)(int32_t chan);
extern void (* VPADDisableGyroAccRevise)(int32_t chan);
extern float (* VPADIsEnableGyroZeroPlay)(int32_t chan);
extern float (* VPADIsEnableGyroZeroDrift)(int32_t chan);
extern float (* VPADIsEnableGyroDirRevise)(int32_t chan);
extern float (* VPADIsEnableGyroAccRevise)(int32_t chan);
extern void (* VPADSetGyroZeroPlayParam)(int32_t chan, float radius);
extern void (* VPADSetGyroDirReviseParam)(int32_t chan, float revis_pw);
extern void (* VPADSetGyroAccReviseParam)(int32_t chan, float revise_pw, float revise_range);
extern void (* VPADSetGyroDirReviseBase)(int32_t chan, VPADDir *base);
extern void (* VPADGetGyroZeroPlayParam)(int32_t chan, float *radius);
extern void (* VPADGetGyroDirReviseParam)(int32_t chan, float *revise_pw);
extern void (* VPADGetGyroAccReviseParam)(int32_t chan, float *revise_pw, float *revise_range);
extern void (* VPADInitGyroZeroPlayParam)(int32_t chan);
extern void (* VPADInitGyroDirReviseParam)(int32_t chan);
extern void (* VPADInitGyroAccReviseParam)(int32_t chan);
extern void (* VPADInitGyroZeroDriftMode)(int32_t chan);
extern void (* VPADSetGyroZeroDriftMode)(int32_t chan, VPADGyroZeroDriftMode mode);
extern void (* VPADGetGyroZeroDriftMode)(int32_t chan, VPADGyroZeroDriftMode *mode);
extern int16_t (* VPADCalcTPCalibrationParam)(VPADTPCalibrationParam* param, uint16_t rawX1, uint16_t rawY1, uint16_t x1, uint16_t y1, uint16_t rawX2, uint16_t rawY2, uint16_t x2, uint16_t y2);
extern void (* VPADSetTPCalibrationParam)(int32_t chan, const VPADTPCalibrationParam param);
extern void (* VPADGetTPCalibrationParam)(int32_t chan, VPADTPCalibrationParam* param);
extern void (* VPADGetTPCalibratedPoint)(int32_t chan, VPADTPData *disp, const VPADTPData *raw);
extern void (* VPADGetTPCalibratedPointEx)(int32_t chan, VPADTPResolution tpReso, VPADTPData *disp, const VPADTPData *raw);
extern int32_t (* VPADControlMotor)(int32_t chan, uint8_t* pattern, uint8_t length);
extern void (* VPADStopMotor)(int32_t chan);
extern int32_t (* VPADSetLcdMode)(int32_t chan, int32_t lcdmode);
extern int32_t (* VPADGetLcdMode)(int32_t chan, int32_t *lcdmode);
extern int32_t (* VPADBASEGetMotorOnRemainingCount)(int32_t lcdmode);
extern int32_t (* VPADBASESetMotorOnRemainingCount)(int32_t lcdmode, int32_t counter);
extern void (* VPADBASESetSensorBarSetting)(int32_t chan, int8_t setting);
extern void (* VPADBASEGetSensorBarSetting)(int32_t chan, int8_t *setting);
extern int32_t (*VPADSetSensorBar)(int32_t chan, bool on);

typedef void(*samplingCallback)(int32_t  chan);
extern samplingCallback ( *VPADSetSamplingCallback)(int32_t chan, samplingCallback callback);


#ifdef __cplusplus
}
#endif

#endif // __VPAD_FUNCTIONS_H_
