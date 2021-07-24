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
#include "os_functions.h"
#include "vpad_functions.h"

u32 vpad_handle __attribute__((section(".data"))) = 0;
u32 vpadbase_handle __attribute__((section(".data"))) = 0;

EXPORT_DECL(void, VPADInit, void);
EXPORT_DECL(void, VPADShutdown, void);
EXPORT_DECL(s32, VPADRead, s32 chan, VPADData *buffer, u32 buffer_size, s32 *error);
EXPORT_DECL(void, VPADSetAccParam, s32 chan, f32 play_radius, f32 sensitivity);
EXPORT_DECL(void, VPADGetAccParam, s32 chan, f32 *play_radius, f32 *sensitivity);
EXPORT_DECL(void, VPADSetBtnRepeat, s32 chan, f32 delay_sec, f32 pulse_sec);
EXPORT_DECL(void, VPADEnableStickCrossClamp, s32 chan);
EXPORT_DECL(void, VPADDisableStickCrossClamp, s32 chan);
EXPORT_DECL(void, VPADSetLStickClampThreshold, s32 chan, s32 max, s32 min);
EXPORT_DECL(void, VPADSetRStickClampThreshold, s32 chan, s32 max, s32 min);
EXPORT_DECL(void, VPADGetLStickClampThreshold, s32 chan, s32* max, s32* min);
EXPORT_DECL(void, VPADGetRStickClampThreshold, s32 chan, s32* max, s32* min);
EXPORT_DECL(void, VPADSetStickOrigin, s32 chan);
EXPORT_DECL(void, VPADDisableLStickZeroClamp, s32 chan);
EXPORT_DECL(void, VPADDisableRStickZeroClamp, s32 chan);
EXPORT_DECL(void, VPADEnableLStickZeroClamp, s32 chan);
EXPORT_DECL(void, VPADEnableRStickZeroClamp, s32 chan);
EXPORT_DECL(void, VPADSetCrossStickEmulationParamsL, s32 chan, f32 rot_deg, f32 xy_deg, f32 radius);
EXPORT_DECL(void, VPADSetCrossStickEmulationParamsR, s32 chan, f32 rot_deg, f32 xy_deg, f32 radius);
EXPORT_DECL(void, VPADGetCrossStickEmulationParamsL, s32 chan, f32* rot_deg, f32* xy_deg, f32* radius);
EXPORT_DECL(void, VPADGetCrossStickEmulationParamsR, s32 chan, f32* rot_deg, f32* xy_deg, f32* radius);
EXPORT_DECL(void, VPADSetGyroAngle, s32 chan, f32 ax, f32 ay, f32 az);
EXPORT_DECL(void, VPADSetGyroDirection, s32 chan, VPADDir *dir);
EXPORT_DECL(void, VPADSetGyroDirectionMag, s32 chan, f32 mag);
EXPORT_DECL(void, VPADSetGyroMagnification, s32 chan, f32 pitch, f32 yaw, f32 roll);
EXPORT_DECL(void, VPADEnableGyroZeroPlay, s32 chan);
EXPORT_DECL(void, VPADEnableGyroDirRevise, s32 chan);
EXPORT_DECL(void, VPADEnableGyroAccRevise, s32 chan);
EXPORT_DECL(void, VPADDisableGyroZeroPlay, s32 chan);
EXPORT_DECL(void, VPADDisableGyroDirRevise, s32 chan);
EXPORT_DECL(void, VPADDisableGyroAccRevise, s32 chan);
EXPORT_DECL(f32, VPADIsEnableGyroZeroPlay, s32 chan);
EXPORT_DECL(f32, VPADIsEnableGyroZeroDrift, s32 chan);
EXPORT_DECL(f32, VPADIsEnableGyroDirRevise, s32 chan);
EXPORT_DECL(f32, VPADIsEnableGyroAccRevise, s32 chan);
EXPORT_DECL(void, VPADSetGyroZeroPlayParam, s32 chan, f32 radius);
EXPORT_DECL(void, VPADSetGyroDirReviseParam, s32 chan, f32 revis_pw);
EXPORT_DECL(void, VPADSetGyroAccReviseParam, s32 chan, f32 revise_pw, f32 revise_range);
EXPORT_DECL(void, VPADSetGyroDirReviseBase, s32 chan, VPADDir *base);
EXPORT_DECL(void, VPADGetGyroZeroPlayParam, s32 chan, f32 *radius);
EXPORT_DECL(void, VPADGetGyroDirReviseParam, s32 chan, f32 *revise_pw);
EXPORT_DECL(void, VPADGetGyroAccReviseParam, s32 chan, f32 *revise_pw, f32 *revise_range);
EXPORT_DECL(void, VPADInitGyroZeroPlayParam, s32 chan);
EXPORT_DECL(void, VPADInitGyroDirReviseParam, s32 chan);
EXPORT_DECL(void, VPADInitGyroAccReviseParam, s32 chan);
EXPORT_DECL(void, VPADInitGyroZeroDriftMode, s32 chan);
EXPORT_DECL(void, VPADSetGyroZeroDriftMode, s32 chan, VPADGyroZeroDriftMode mode);
EXPORT_DECL(void, VPADGetGyroZeroDriftMode, s32 chan, VPADGyroZeroDriftMode *mode);
EXPORT_DECL(s16, VPADCalcTPCalibrationParam, VPADTPCalibrationParam* param, u16 rawX1, u16 rawY1, u16 x1, u16 y1, u16 rawX2, u16 rawY2, u16 x2, u16 y2);
EXPORT_DECL(void, VPADSetTPCalibrationParam, s32 chan, const VPADTPCalibrationParam param);
EXPORT_DECL(void, VPADGetTPCalibrationParam, s32 chan, VPADTPCalibrationParam* param);
EXPORT_DECL(void, VPADGetTPCalibratedPoint, s32 chan, VPADTPData *disp, const VPADTPData *raw);
EXPORT_DECL(void, VPADGetTPCalibratedPointEx, s32 chan, VPADTPResolution tpReso, VPADTPData *disp, const VPADTPData *raw);
EXPORT_DECL(s32, VPADControlMotor, s32 chan, u8* pattern, u8 length);
EXPORT_DECL(void, VPADStopMotor, s32 chan);
EXPORT_DECL(s32, VPADSetLcdMode, s32 chan, s32 lcdmode);
EXPORT_DECL(s32, VPADGetLcdMode, s32 chan, s32 *lcdmode);
EXPORT_DECL(s32, VPADBASEGetMotorOnRemainingCount, s32 lcdmode);
EXPORT_DECL(s32, VPADBASESetMotorOnRemainingCount, s32 lcdmode, s32 counter);
EXPORT_DECL(void, VPADBASESetSensorBarSetting, s32 chan, s8 setting);
EXPORT_DECL(void, VPADBASEGetSensorBarSetting, s32 chan, s8 *setting);
EXPORT_DECL(s32, VPADSetSensorBar, s32 chan, bool on);
EXPORT_DECL(samplingCallback, VPADSetSamplingCallback, s32 chan, samplingCallback callbackn);

void InitAcquireVPad(void) {
    if(coreinit_handle == 0) {
        InitAcquireOS();
    };
    OSDynLoad_Acquire("vpad.rpl", &vpad_handle);
    OSDynLoad_Acquire("vpadbase.rpl", &vpadbase_handle);
}

void InitVPadFunctionPointers(void) {
    u32 *funcPointer = 0;

    InitAcquireVPad();

    OS_FIND_EXPORT(vpad_handle, VPADInit);
    OS_FIND_EXPORT(vpad_handle, VPADShutdown);
    OS_FIND_EXPORT(vpad_handle, VPADRead);
    OS_FIND_EXPORT(vpad_handle, VPADSetAccParam);
    OS_FIND_EXPORT(vpad_handle, VPADGetAccParam);
    OS_FIND_EXPORT(vpad_handle, VPADSetBtnRepeat);
    OS_FIND_EXPORT(vpad_handle, VPADEnableStickCrossClamp);
    OS_FIND_EXPORT(vpad_handle, VPADDisableStickCrossClamp);
    OS_FIND_EXPORT(vpad_handle, VPADSetLStickClampThreshold);
    OS_FIND_EXPORT(vpad_handle, VPADSetRStickClampThreshold);
    OS_FIND_EXPORT(vpad_handle, VPADGetLStickClampThreshold);
    OS_FIND_EXPORT(vpad_handle, VPADGetRStickClampThreshold);
    OS_FIND_EXPORT(vpad_handle, VPADSetStickOrigin);
    OS_FIND_EXPORT(vpad_handle, VPADDisableLStickZeroClamp);
    OS_FIND_EXPORT(vpad_handle, VPADDisableRStickZeroClamp);
    OS_FIND_EXPORT(vpad_handle, VPADEnableLStickZeroClamp);
    OS_FIND_EXPORT(vpad_handle, VPADEnableRStickZeroClamp);
    OS_FIND_EXPORT(vpad_handle, VPADSetCrossStickEmulationParamsL);
    OS_FIND_EXPORT(vpad_handle, VPADSetCrossStickEmulationParamsR);
    OS_FIND_EXPORT(vpad_handle, VPADGetCrossStickEmulationParamsL);
    OS_FIND_EXPORT(vpad_handle, VPADGetCrossStickEmulationParamsR);
    OS_FIND_EXPORT(vpad_handle, VPADSetGyroAngle);
    OS_FIND_EXPORT(vpad_handle, VPADSetGyroDirection);
    OS_FIND_EXPORT(vpad_handle, VPADSetGyroDirectionMag);
    OS_FIND_EXPORT(vpad_handle, VPADSetGyroMagnification);
    OS_FIND_EXPORT(vpad_handle, VPADEnableGyroZeroPlay);
    OS_FIND_EXPORT(vpad_handle, VPADEnableGyroDirRevise);
    OS_FIND_EXPORT(vpad_handle, VPADEnableGyroAccRevise);
    OS_FIND_EXPORT(vpad_handle, VPADDisableGyroZeroPlay);
    OS_FIND_EXPORT(vpad_handle, VPADDisableGyroDirRevise);
    OS_FIND_EXPORT(vpad_handle, VPADDisableGyroAccRevise);
    OS_FIND_EXPORT(vpad_handle, VPADIsEnableGyroZeroPlay);
    OS_FIND_EXPORT(vpad_handle, VPADIsEnableGyroZeroDrift);
    OS_FIND_EXPORT(vpad_handle, VPADIsEnableGyroDirRevise);
    OS_FIND_EXPORT(vpad_handle, VPADIsEnableGyroAccRevise);
    OS_FIND_EXPORT(vpad_handle, VPADSetGyroZeroPlayParam);
    OS_FIND_EXPORT(vpad_handle, VPADSetGyroDirReviseParam);
    OS_FIND_EXPORT(vpad_handle, VPADSetGyroAccReviseParam);
    OS_FIND_EXPORT(vpad_handle, VPADSetGyroDirReviseBase);
    OS_FIND_EXPORT(vpad_handle, VPADGetGyroZeroPlayParam);
    OS_FIND_EXPORT(vpad_handle, VPADGetGyroDirReviseParam);
    OS_FIND_EXPORT(vpad_handle, VPADGetGyroAccReviseParam);
    OS_FIND_EXPORT(vpad_handle, VPADInitGyroZeroPlayParam);
    OS_FIND_EXPORT(vpad_handle, VPADInitGyroDirReviseParam);
    OS_FIND_EXPORT(vpad_handle, VPADInitGyroAccReviseParam);
    OS_FIND_EXPORT(vpad_handle, VPADInitGyroZeroDriftMode);
    OS_FIND_EXPORT(vpad_handle, VPADSetGyroZeroDriftMode);
    OS_FIND_EXPORT(vpad_handle, VPADGetGyroZeroDriftMode);
    OS_FIND_EXPORT(vpad_handle, VPADCalcTPCalibrationParam);
    OS_FIND_EXPORT(vpad_handle, VPADSetTPCalibrationParam);
    OS_FIND_EXPORT(vpad_handle, VPADGetTPCalibrationParam);
    OS_FIND_EXPORT(vpad_handle, VPADGetTPCalibratedPoint);
    OS_FIND_EXPORT(vpad_handle, VPADGetTPCalibratedPointEx);
    OS_FIND_EXPORT(vpad_handle, VPADControlMotor);
    OS_FIND_EXPORT(vpad_handle, VPADStopMotor);
    OS_FIND_EXPORT(vpad_handle, VPADSetLcdMode);
    OS_FIND_EXPORT(vpad_handle, VPADGetLcdMode);
    OS_FIND_EXPORT(vpad_handle, VPADSetSensorBar);
    OS_FIND_EXPORT(vpad_handle, VPADSetSamplingCallback);
    OS_FIND_EXPORT(vpadbase_handle, VPADBASEGetMotorOnRemainingCount);
    OS_FIND_EXPORT(vpadbase_handle, VPADBASESetMotorOnRemainingCount);
    OS_FIND_EXPORT(vpadbase_handle, VPADBASESetSensorBarSetting);
    OS_FIND_EXPORT(vpadbase_handle, VPADBASEGetSensorBarSetting);
}
