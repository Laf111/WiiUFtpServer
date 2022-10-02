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

uint32_t vpad_handle __attribute__((section(".data"))) = 0;
uint32_t vpadbase_handle __attribute__((section(".data"))) = 0;

EXPORT_DECL(void, VPADInit, void);
EXPORT_DECL(void, VPADShutdown, void);
EXPORT_DECL(int32_t, VPADRead, int32_t chan, VPADData *buffer, uint32_t buffer_size, int32_t *error);
EXPORT_DECL(void, VPADSetAccParam, int32_t chan, float play_radius, float sensitivity);
EXPORT_DECL(void, VPADGetAccParam, int32_t chan, float *play_radius, float *sensitivity);
EXPORT_DECL(void, VPADSetBtnRepeat, int32_t chan, float delay_sec, float pulse_sec);
EXPORT_DECL(void, VPADEnableStickCrossClamp, int32_t chan);
EXPORT_DECL(void, VPADDisableStickCrossClamp, int32_t chan);
EXPORT_DECL(void, VPADSetLStickClampThreshold, int32_t chan, int32_t max, int32_t min);
EXPORT_DECL(void, VPADSetRStickClampThreshold, int32_t chan, int32_t max, int32_t min);
EXPORT_DECL(void, VPADGetLStickClampThreshold, int32_t chan, int32_t* max, int32_t* min);
EXPORT_DECL(void, VPADGetRStickClampThreshold, int32_t chan, int32_t* max, int32_t* min);
EXPORT_DECL(void, VPADSetStickOrigin, int32_t chan);
EXPORT_DECL(void, VPADDisableLStickZeroClamp, int32_t chan);
EXPORT_DECL(void, VPADDisableRStickZeroClamp, int32_t chan);
EXPORT_DECL(void, VPADEnableLStickZeroClamp, int32_t chan);
EXPORT_DECL(void, VPADEnableRStickZeroClamp, int32_t chan);
EXPORT_DECL(void, VPADSetCrossStickEmulationParamsL, int32_t chan, float rot_deg, float xy_deg, float radius);
EXPORT_DECL(void, VPADSetCrossStickEmulationParamsR, int32_t chan, float rot_deg, float xy_deg, float radius);
EXPORT_DECL(void, VPADGetCrossStickEmulationParamsL, int32_t chan, float* rot_deg, float* xy_deg, float* radius);
EXPORT_DECL(void, VPADGetCrossStickEmulationParamsR, int32_t chan, float* rot_deg, float* xy_deg, float* radius);
EXPORT_DECL(void, VPADSetGyroAngle, int32_t chan, float ax, float ay, float az);
EXPORT_DECL(void, VPADSetGyroDirection, int32_t chan, VPADDir *dir);
EXPORT_DECL(void, VPADSetGyroDirectionMag, int32_t chan, float mag);
EXPORT_DECL(void, VPADSetGyroMagnification, int32_t chan, float pitch, float yaw, float roll);
EXPORT_DECL(void, VPADEnableGyroZeroPlay, int32_t chan);
EXPORT_DECL(void, VPADEnableGyroDirRevise, int32_t chan);
EXPORT_DECL(void, VPADEnableGyroAccRevise, int32_t chan);
EXPORT_DECL(void, VPADDisableGyroZeroPlay, int32_t chan);
EXPORT_DECL(void, VPADDisableGyroDirRevise, int32_t chan);
EXPORT_DECL(void, VPADDisableGyroAccRevise, int32_t chan);
EXPORT_DECL(float, VPADIsEnableGyroZeroPlay, int32_t chan);
EXPORT_DECL(float, VPADIsEnableGyroZeroDrift, int32_t chan);
EXPORT_DECL(float, VPADIsEnableGyroDirRevise, int32_t chan);
EXPORT_DECL(float, VPADIsEnableGyroAccRevise, int32_t chan);
EXPORT_DECL(void, VPADSetGyroZeroPlayParam, int32_t chan, float radius);
EXPORT_DECL(void, VPADSetGyroDirReviseParam, int32_t chan, float revis_pw);
EXPORT_DECL(void, VPADSetGyroAccReviseParam, int32_t chan, float revise_pw, float revise_range);
EXPORT_DECL(void, VPADSetGyroDirReviseBase, int32_t chan, VPADDir *base);
EXPORT_DECL(void, VPADGetGyroZeroPlayParam, int32_t chan, float *radius);
EXPORT_DECL(void, VPADGetGyroDirReviseParam, int32_t chan, float *revise_pw);
EXPORT_DECL(void, VPADGetGyroAccReviseParam, int32_t chan, float *revise_pw, float *revise_range);
EXPORT_DECL(void, VPADInitGyroZeroPlayParam, int32_t chan);
EXPORT_DECL(void, VPADInitGyroDirReviseParam, int32_t chan);
EXPORT_DECL(void, VPADInitGyroAccReviseParam, int32_t chan);
EXPORT_DECL(void, VPADInitGyroZeroDriftMode, int32_t chan);
EXPORT_DECL(void, VPADSetGyroZeroDriftMode, int32_t chan, VPADGyroZeroDriftMode mode);
EXPORT_DECL(void, VPADGetGyroZeroDriftMode, int32_t chan, VPADGyroZeroDriftMode *mode);
EXPORT_DECL(int16_t, VPADCalcTPCalibrationParam, VPADTPCalibrationParam* param, uint16_t rawX1, uint16_t rawY1, uint16_t x1, uint16_t y1, uint16_t rawX2, uint16_t rawY2, uint16_t x2, uint16_t y2);
EXPORT_DECL(void, VPADSetTPCalibrationParam, int32_t chan, const VPADTPCalibrationParam param);
EXPORT_DECL(void, VPADGetTPCalibrationParam, int32_t chan, VPADTPCalibrationParam* param);
EXPORT_DECL(void, VPADGetTPCalibratedPoint, int32_t chan, VPADTPData *disp, const VPADTPData *raw);
EXPORT_DECL(void, VPADGetTPCalibratedPointEx, int32_t chan, VPADTPResolution tpReso, VPADTPData *disp, const VPADTPData *raw);
EXPORT_DECL(int32_t, VPADControlMotor, int32_t chan, uint8_t* pattern, uint8_t length);
EXPORT_DECL(void, VPADStopMotor, int32_t chan);
EXPORT_DECL(int32_t, VPADSetLcdMode, int32_t chan, int32_t lcdmode);
EXPORT_DECL(int32_t, VPADGetLcdMode, int32_t chan, int32_t *lcdmode);
EXPORT_DECL(int32_t, VPADBASEGetMotorOnRemainingCount, int32_t lcdmode);
EXPORT_DECL(int32_t, VPADBASESetMotorOnRemainingCount, int32_t lcdmode, int32_t counter);
EXPORT_DECL(void, VPADBASESetSensorBarSetting, int32_t chan, int8_t setting);
EXPORT_DECL(void, VPADBASEGetSensorBarSetting, int32_t chan, int8_t *setting);
EXPORT_DECL(int32_t, VPADSetSensorBar, int32_t chan, bool on);
EXPORT_DECL(samplingCallback, VPADSetSamplingCallback, int32_t chan, samplingCallback callbackn);

void InitAcquireVPad(void) {
    if(coreinit_handle == 0) {
        InitAcquireOS();
    };
    OSDynLoad_Acquire("vpad.rpl", &vpad_handle);
    OSDynLoad_Acquire("vpadbase.rpl", &vpadbase_handle);
}

void InitVPadFunctionPointers(void) {
    uint32_t *funcPointer = 0;

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
