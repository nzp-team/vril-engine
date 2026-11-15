/*
 * Copyright (C) 1996-1997 Id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#include "../../nzportable_def.h"
#include "ps3_local.h"
#include <sys/memory.h>
#include <sysutil/osk.h>
#include <sysutil/sysutil.h>

#define OSK_LIMIT 255
static int osk_active, callback_ready;
static sys_mem_container_t container;
static int container_ready;
static u16 message[] = { 'C', 'o', 'n', 's', 'o', 'l', 'e', ' ',
                         'c', 'o', 'm', 'm', 'a', 'n', 'd', 0 };
static u16 initial[1], text[OSK_LIMIT + 1];
static oskParam parameters;
static oskInputFieldInfo field;
static oskCallbackReturnParam result;

static void
FreeContainer(void)
{
    if (container_ready) {
        sysMemContainerDestroy(container);
        container_ready = 0;
    }
}

static void
KeyboardCallback(u64 status, u64 parameter, void * data)
{
    (void) parameter;
    (void) data;

    if (status == SYSUTIL_OSK_DONE && osk_active == 1) {
        osk_active = 2;
        int error = oskUnloadAsync(&result);

        if (error < 0) {
            osk_active = 1;
            Con_Printf("PS3: keyboard unload failed: %08x\n", error);
        }
    } else if (status == SYSUTIL_OSK_UNLOADED && osk_active) {
        osk_active = 0;
        FreeContainer();
        Con_Printf("PS3: keyboard closed (result %d)\n", result.res);
        Key_ClearStates();
        IN_PlatformClearPendingInput();

        if (result.res == OSK_OK) {
            char command[OSK_LIMIT + 1];
            size_t i;

            for (i = 0; i < OSK_LIMIT && text[i]; ++i) {
                if (text[i] < 32 || text[i] > 126) {
                    Con_Printf("Console commands require basic Latin characters.\n");
                    return;
                }

                command[i] = (char) text[i];
            }

            command[i] = 0;

            if (i) {
                Cbuf_AddText(command);
                Cbuf_AddText("\n");
            }
        }
    }
}

int
PS3_OSKActive(void)
{
    return osk_active != 0;
}

void
IN_OpenOSKeyboard(void)
{
    if (osk_active)
        return;

    if (!callback_ready) {
        int error = sysUtilRegisterCallback(1, KeyboardCallback, NULL);

        if (error < 0) {
            Con_Printf("PS3: keyboard callback failed: %08x\n", error);
            return;
        }

        callback_ready = 1;
    }

    int error = oskSetKeyLayoutOption(OSK_10KEY_PANEL | OSK_FULLKEY_PANEL);

    if (error < 0) {
        Con_Printf("PS3: oskSetKeyLayoutOption failed: %08x\n", error);
        return;
    }

    error = oskSetInitialKeyLayout(OSK_INITIAL_FULLKEY_PANEL);

    if (error < 0) {
        Con_Printf("PS3: oskSetInitialKeyLayout failed: %08x\n", error);
        return;
    }

    error = sysMemContainerCreate(&container, 8 * 1024 * 1024);

    if (error < 0) {
        Con_Printf("PS3: keyboard memory failed: %08x\n", error);
        return;
    }

    container_ready = 1;
    memset(text, 0, sizeof(text));
    memset(&parameters, 0, sizeof(parameters));
    parameters.allowedPanels =
      OSK_PANEL_TYPE_ALPHABET | OSK_PANEL_TYPE_NUMERAL | OSK_PANEL_TYPE_ENGLISH;
    parameters.firstViewPanel = OSK_PANEL_TYPE_ENGLISH;
    parameters.prohibitFlags  = OSK_PROHIBIT_RETURN;
    field.message   = message;
    field.startText = initial;
    field.maxLength = OSK_LIMIT;
    result.res      = OSK_CANCELED;
    result.len      = OSK_LIMIT;
    result.str      = text;
    osk_active      = 1;
    error = oskLoadAsync(container, &parameters, &field);

    if (error < 0) {
        osk_active = 0;
        FreeContainer();
        Con_Printf("PS3: keyboard could not open: %08x\n", error);
        return;
    }

    Con_Printf("PS3: keyboard opened\n");
    Key_ClearStates();
    IN_PlatformClearPendingInput();
    IN_PlatformRumble(0, 0, 0);
}

void
PS3_OSKShutdown(void)
{
    if (osk_active)
        oskAbort();
}
