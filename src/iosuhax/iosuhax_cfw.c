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

#include <malloc.h>

#include "iosuhax.h"
#include "iosuhax_iosapi.h"
#include "iosuhax_cfw.h"

static int cfw_identified = 0;
static IOSUHAX_CFW_Family cfw_family = IOSUHAX_CFW_NO_CFW;
static IOSUHAX_CFW_Variant cfw_variant = IOSUHAX_CFW_VARIANT_STOCK;
static IOSUHAX_CFW_RPXStyle cfw_rpxstyle = IOSUHAX_CFW_RPX_STYLE_NONE;
static int mcp_available = 1;
static int iosuhax_available = 0;

static void IOSUHAX_CFW_Identify() {
    int mcpFd = IOS_Open("/dev/mcp", 0);
    if (mcpFd < 0) return;

    int iosuhaxFd = IOS_Open("/dev/iosuhax", 0);
    if (iosuhaxFd >= 0) {
    /*  Mocha-style CFW */
        cfw_family = IOSUHAX_CFW_MOCHA;
        iosuhax_available = 1;
        IOS_Close(iosuhaxFd);

        int ioctl100_ret = 0;

    /*  Detect ioctl100/modern RPX loading
        Important: the size of dummy must be more than 0x280-1 to make Mocha RPX
        fail on it. */
        const int dummy_size = 0x300;
        void* dummy = memalign(0x20, 0x300);
        *(uint32_t*)dummy = 0; //this must *not* be a valid IPC_CUSTOM command
        IOS_Ioctl(mcpFd, 100, dummy, dummy_size, &ioctl100_ret, sizeof(ioctl100_ret));
        free(dummy);

        if (ioctl100_ret == 1) {
        /*  "Mocha RPX" */
            cfw_variant = IOSUHAX_CFW_VARIANT_MOCHA_RPX;
            cfw_rpxstyle = IOSUHAX_CFW_RPX_STYLE_RAW_PATH;
        } else if (ioctl100_ret == 2) {
        /*  MochaLite */
            cfw_variant = IOSUHAX_CFW_VARIANT_MOCHALITE;
            cfw_rpxstyle = IOSUHAX_CFW_RPX_STYLE_IPC;
        } else {
        /*  Mocha */
            cfw_rpxstyle = IOSUHAX_CFW_RPX_STYLE_NONE;
        }
        cfw_identified = 1;
        IOS_Close(mcpFd);
        return;
    }

/*  Try to detect Haxchi FW - MCP hook open */
    uint32_t* haxchi_magic = memalign(0x20, 0x100);
    IOS_Ioctl(mcpFd, 0x5B, NULL, 0, haxchi_magic, sizeof(*haxchi_magic));
    if (*haxchi_magic == IOSUHAX_MAGIC_WORD) {
        cfw_family = IOSUHAX_CFW_HAXCHIFW;
        mcp_available = 0;

        cfw_identified = 1;
        IOS_Close(mcpFd);
        free(haxchi_magic);
        return;
    }
    free(haxchi_magic);

/*  Try to detect Haxchi FW - no MCP hook */
    struct {
        int major;
        int minor;
        int patch;
        char region[4];
    } *mcp_version = memalign(0x20, 0x100);
    IOS_Ioctl(mcpFd, 0x89, NULL, 0, mcp_version, sizeof(*mcp_version));
    if (mcp_version->major == 99 &&
        mcp_version->minor == 99 &&
        mcp_version->patch == 99) {
        cfw_family = IOSUHAX_CFW_HAXCHIFW;
        iosuhax_available = 1;

        cfw_identified = 1;
        IOS_Close(mcpFd);
        free(mcp_version);
        return;
    }
    free(mcp_version);

/*  Alright, we got nothing */
    cfw_identified = 1;
    IOS_Close(mcpFd);
    return;
}

static void IOSUHAX_CFW_CheckMCP() {
    if (!cfw_identified) {
        IOSUHAX_CFW_Identify();
    } else {
    /*  State might have changed since last call */
        if (cfw_family == IOSUHAX_CFW_HAXCHIFW) {
            int mcpFd = IOS_Open("/dev/mcp", 0);
            if (mcpFd < 0) {
                mcp_available = 0;
                return;
            }

            uint32_t haxchi_magic = 0;
            IOS_Ioctl(mcpFd, IOCTL_CHECK_IF_IOSUHAX, NULL, 0, &haxchi_magic, sizeof(haxchi_magic));
            if (haxchi_magic == IOSUHAX_MAGIC_WORD) {
                mcp_available = 0;
                iosuhax_available = 1;
            } else {
                mcp_available = 1;
                iosuhax_available = 0;
            }
            IOS_Close(mcpFd);
        }
    }
}

IOSUHAX_CFW_Family IOSUHAX_CFW_GetFamily() {
    if (!cfw_identified) {
        IOSUHAX_CFW_Identify();
    }

    return cfw_family;
}

IOSUHAX_CFW_Variant IOSUHAX_CFW_GetVariant() {
    if (!cfw_identified) {
        IOSUHAX_CFW_Identify();
    }

    return cfw_variant;
}

IOSUHAX_CFW_RPXStyle IOSUHAX_CFW_GetRPXStyle() {
    if (!cfw_identified) {
        IOSUHAX_CFW_Identify();
    }

    return cfw_rpxstyle;
}

int IOSUHAX_CFW_MCPAvailable() {
    IOSUHAX_CFW_CheckMCP();

    return mcp_available;
}

int IOSUHAX_CFW_Available() {
    IOSUHAX_CFW_CheckMCP();

    return iosuhax_available;
}
