/***************************************************************************
 * Copyright (C) 2020
 * by Ash Logan <ash@heyquark.com>
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
#ifndef _LIB_IOSUHAX_CFW_H_
#define _LIB_IOSUHAX_CFW_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef enum IOSUHAX_CFW_Family {
    IOSUHAX_CFW_NO_CFW = 0,
    IOSUHAX_CFW_MOCHA,
    IOSUHAX_CFW_HAXCHIFW,
} IOSUHAX_CFW_Family;

typedef enum IOSUHAX_CFW_Variant {
    IOSUHAX_CFW_VARIANT_STOCK = 0,
    IOSUHAX_CFW_VARIANT_MOCHALITE,
    IOSUHAX_CFW_VARIANT_MOCHA_RPX,
} IOSUHAX_CFW_Variant;

typedef enum IOSUHAX_CFW_RPXStyle {
    IOSUHAX_CFW_RPX_STYLE_NONE = 0,
    IOSUHAX_CFW_RPX_STYLE_IPC,
    IOSUHAX_CFW_RPX_STYLE_RAW_PATH,
} IOSUHAX_CFW_RPXStyle;

//! Whether iosuhax, and therefore the FSA, SVC and memory APIs are available
int IOSUHAX_CFW_Available();

//! Whether IOS-MCP APIs are available - this is variable on HaxchiFW.
int IOSUHAX_CFW_MCPAvailable();

//! Get the CFW family (no cfw, mocha, haxchi) of the current environment.
IOSUHAX_CFW_Family IOSUHAX_CFW_GetFamily();
//! Get the running CFW's variant.
IOSUHAX_CFW_Variant IOSUHAX_CFW_GetVariant();
//! Get the IOSU-side RPX loading style, if any.
IOSUHAX_CFW_RPXStyle IOSUHAX_CFW_GetRPXStyle();

#ifdef __cplusplus
}
#endif

#endif //_LIB_IOSUHAX_CFW_H_
