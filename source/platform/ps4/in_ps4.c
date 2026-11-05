/*
 * Copyright (C) 2026 NZ:P Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

// DualShock 4 backend. Stick shaping and gyro sensitivity live in input.c.
#include "../../nzportable_def.h"
#define scePadSetMotionSensorState OpenOrbis_UnprototypedMotionSensorState
#include <orbis/Pad.h>
#undef scePadSetMotionSensorState
#include "ps4_local.h"
#include <orbis/Sysmodule.h>
#include <orbis/ImeDialog.h>
#include <orbis/CommonDialog.h>

static int pad_handle = -1;
static OrbisPadData pad;
static unsigned int previous_buttons;
static double rumble_end;

// Supply the typed signature missing from OpenOrbis Pad.h.
extern int
scePadSetMotionSensorState(int, bool);

static const struct {
    unsigned int mask;
    int          key;
} buttons[] = {
    { ORBIS_PAD_BUTTON_OPTIONS   , K_START     },
    { ORBIS_PAD_BUTTON_TOUCH_PAD , K_SELECT    },
    { ORBIS_PAD_BUTTON_UP        , K_UPARROW   },
    { ORBIS_PAD_BUTTON_DOWN      , K_DOWNARROW },
    { ORBIS_PAD_BUTTON_LEFT      , K_LEFTARROW },
    { ORBIS_PAD_BUTTON_RIGHT     , K_RIGHTARROW },
    { ORBIS_PAD_BUTTON_CROSS     , K_BOTTOMFACE },
    { ORBIS_PAD_BUTTON_CIRCLE    , K_RIGHTFACE },
    { ORBIS_PAD_BUTTON_SQUARE    , K_LEFTFACE  },
    { ORBIS_PAD_BUTTON_TRIANGLE  , K_TOPFACE   },
    { ORBIS_PAD_BUTTON_L1        , K_LTRIGGER  },
    { ORBIS_PAD_BUTTON_R1        , K_RTRIGGER  },
    { ORBIS_PAD_BUTTON_L2        , K_ZLTRIGGER },
    { ORBIS_PAD_BUTTON_R2        , K_ZRTRIGGER },
    { ORBIS_PAD_BUTTON_L3        , K_LTHUMB    },
    { ORBIS_PAD_BUTTON_R3        , K_RTHUMB    }
};

qboolean
IN_PlatformHasMouse(void)
{
    return false;
}

qboolean
IN_PlatformHasGamepad(void)
{
    return true;
}

void
IN_SetMouseToRelative(bool relative)
{
    (void) relative;
}

void
IN_PlatformClearPendingInput(void)
{
}

void
IN_PlatformMouseMove(usercmd_t * cmd)
{
    (void) cmd;
}

void
IN_PlatformMove(usercmd_t * cmd)
{
    (void) cmd;
}

static void
SetRumble(byte low, byte high)
{
    OrbisPadVibeParam vibration = { low, high };

    if (pad_handle >= 0)
        scePadSetVibration(pad_handle, &vibration);
}

void
IN_PlatformInit(void)
{
    Cmd_AddCommand("osk", IN_OpenOSKeyboard);

    if (PS4_LoadModule(ORBIS_SYSMODULE_INTERNAL_PAD) < 0)
        Sys_Error("PS4: controller module could not be loaded");

    if (scePadInit() < 0)
        Sys_Error("PS4: scePadInit failed");

    pad_handle = scePadOpen(ps4_user, ORBIS_PAD_PORT_TYPE_STANDARD, 0, NULL);

    if (pad_handle < 0)
        Sys_Error("PS4: cannot open controller (%d)", pad_handle);

    if (scePadSetMotionSensorState(pad_handle, true) < 0)
        Con_Printf("PS4: controller motion sensor could not be enabled.\n");
}

void
IN_PlatformShutdown(void)
{
    PS4_OSKShutdown();

    if (pad_handle < 0)
        return;

    SetRumble(0, 0);
    scePadResetLightBar(pad_handle);
    scePadSetMotionSensorState(pad_handle, false);
    scePadClose(pad_handle);
    pad_handle = -1;
    memset(&pad, 0, sizeof(pad));
    previous_buttons = 0;
    rumble_end       = 0;
}

void
Sys_SendKeyEvents(void)
{
    OrbisPadData next = { 0 };
    unsigned int current;
    size_t i;

    if (pad_handle < 0 || scePadReadState(pad_handle, &next) < 0 || !next.connected) {
        memset(&next, 0, sizeof(next));

        if (pad.connected) {
            SetRumble(0, 0);
            rumble_end = 0;
        }
    }

    if (next.connected && !pad.connected)
        scePadSetMotionSensorState(pad_handle, true);

    int keyboard_was_active = PS4_OSKActive();

    PS4_OSKPoll();
    pad     = next;
    current = pad.connected ? pad.buttons : 0;

    if (keyboard_was_active || PS4_OSKActive()) {
        previous_buttons = current;

        return;
    }

    for (i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i)
        if ((current ^ previous_buttons) & buttons[i].mask)
            Key_Event(buttons[i].key, (current & buttons[i].mask) != 0);


    previous_buttons = current;
}

void
IN_GetAnalogStick(in_analog_stick_id_t which, in_analog_stick_t * value)
{
    stick state = which == IN_STICK_LEFT ? pad.leftStick : pad.rightStick;

    value->x = value->y = 0;

    if (!pad.connected || PS4_OSKActive())
        return;

    // 128 is the controller's neutral value. Keep it exactly zero.
    value->x = ((int) state.x - 128) / (state.x < 128 ? 128.0f : 127.0f);
    value->y = ((int) state.y - 128) / (state.y < 128 ? 128.0f : 127.0f);

    if (which == IN_STICK_LEFT)
        value->y = -value->y;
}

qboolean
IN_PlatformGetGyro(float * x, float * y)
{
    *x = *y = 0;

    if (!pad.connected || PS4_OSKActive())
        return false;

    // OpenOrbis 0.5.4 names the angular-velocity ABI field acell.
    *x = -pad.acell.x;
    *y = pad.acell.y;

    return true;
}

void
IN_PlatformRumble(unsigned short low, unsigned short high, unsigned int duration)
{
    SetRumble(duration ? low / 257 : 0, duration ? high / 257 : 0);
    rumble_end = duration ? Sys_FloatTime() + duration / 1000.0 : 0;
}

void
IN_PlatformCommands(void)
{
    if (rumble_end && Sys_FloatTime() >= rumble_end) {
        SetRumble(0, 0);
        rumble_end = 0;
    }
}

void
IN_PlatformSetLightbar(byte red, byte green, byte blue)
{
    OrbisPadColor color = { red, green, blue, 0 };

    if (pad_handle >= 0 && pad.connected)
        scePadSetLightBar(pad_handle, &color);
}

// The system IME uses UTF-16 even though this SDK declares wchar_t pointers.
static uint16_t text[256];
static const uint16_t title[] = { 'C', 'o', 'n', 's', 'o', 'l', 'e', ' ', 'c', 'o', 'm', 'm', 'a', 'n', 'd', 0 };
static int active, ready;
static OrbisImeDialogSetting settings;

int
PS4_OSKActive(void)
{
    return active;
}

void
IN_OpenOSKeyboard(void)
{
    if (active)
        return;

    if (!ready) {
        int result = PS4_LoadModule(ORBIS_SYSMODULE_INTERNAL_COMMON_DIALOG);

        if (result >= 0)
            result = sceSysmoduleLoadModule(ORBIS_SYSMODULE_IME_DIALOG);

        if (result >= 0)
            result = sceCommonDialogInitialize();

        if (result < 0) {
            Con_Printf("PS4 keyboard initialization failed: %08x\n", result);

            return;
        }

        ready = 1;
    }

    memset(&settings, 0, sizeof(settings));
    settings.userId          = ps4_user;
    settings.type            = ORBIS_TYPE_BASIC_LATIN;
    settings.enterLabel      = ORBIS_BUTTON_LABEL_SEND;
    settings.maxTextLength   = 255;
    settings.inputTextBuffer = (wchar_t *) text;
    settings.title           = (const wchar_t *) title;
    int result = sceImeDialogInit(&settings, NULL);

    if (result < 0) {
        Con_Printf("PS4 keyboard could not open: %08x\n", result);

        return;
    }

    active = 1;
    Key_ClearStates();
}

void
PS4_OSKPoll(void)
{
    if (!active || sceImeDialogGetStatus() != ORBIS_DIALOG_STATUS_STOPPED)
        return;

    OrbisDialogResult result = { 0 };
    int status = sceImeDialogGetResult(&result);

    if (status < 0)
        return;  // Never terminate a dialog whose result is not ready.

    sceImeDialogTerm();
    active = 0;

    if (status >= 0 && result.endstatus == ORBIS_DIALOG_OK) {
        char command[256];
        size_t i;

        for (i = 0; i < 255 && text[i]; ++i) {
            // Quake command input is byte based; do not truncate UTF-16 into commands.
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

void
PS4_OSKShutdown(void)
{
    if (active) {
        sceImeDialogAbort();
        sceImeDialogTerm();
        active = 0;
    }
}
