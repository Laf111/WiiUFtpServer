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
#include "padscore_functions.h"

uint32_t padscore_handle __attribute__((section(".data"))) = 0;

EXPORT_DECL(void, KPADInit, void);
EXPORT_DECL(void, WPADInit, void);
EXPORT_DECL(int32_t, WPADProbe, int32_t chan, uint32_t * pad_type);
EXPORT_DECL(int32_t, WPADSetDataFormat, int32_t chan, int32_t format);
EXPORT_DECL(void, WPADEnableURCC, int32_t enable);
EXPORT_DECL(void, WPADRead, int32_t chan, void * data);
EXPORT_DECL(int32_t, KPADRead, int32_t chan, KPADData * data, uint32_t size);
EXPORT_DECL(int32_t, KPADReadEx, int32_t chan, KPADData * data, uint32_t size, int32_t *error);
EXPORT_DECL(void,WPADSetAutoSleepTime,uint8_t minute);
EXPORT_DECL(void,WPADDisconnect,int32_t chan);

void InitAcquirePadScore(void) {
    if(coreinit_handle == 0) {
        InitAcquireOS();
    };
    OSDynLoad_Acquire("padscore.rpl", &padscore_handle);
}

void InitPadScoreFunctionPointers(void) {
    uint32_t *funcPointer = 0;
    InitAcquirePadScore();

    OS_FIND_EXPORT(padscore_handle, WPADInit);
    OS_FIND_EXPORT(padscore_handle, KPADInit);
    OS_FIND_EXPORT(padscore_handle, WPADProbe);
    OS_FIND_EXPORT(padscore_handle, WPADSetDataFormat);
    OS_FIND_EXPORT(padscore_handle, WPADEnableURCC);
    OS_FIND_EXPORT(padscore_handle, WPADRead);
    OS_FIND_EXPORT(padscore_handle, KPADRead);
    OS_FIND_EXPORT(padscore_handle, KPADReadEx);
    OS_FIND_EXPORT(padscore_handle, WPADSetAutoSleepTime);
    OS_FIND_EXPORT(padscore_handle, WPADDisconnect);

    KPADInit();
    WPADEnableURCC(1);
}