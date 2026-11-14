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
#include <input/input.h>
#include <usb/usbmain.h>
#include <stddef.h>

static struct controller_data_s pad;
static double rumble_end;
static unsigned previous_buttons;
static const struct {
    size_t offset;
    int key;
} buttons[] = {
    { offsetof(struct controller_data_s, start), K_START },
    { offsetof(struct controller_data_s, back), K_SELECT },
    { offsetof(struct controller_data_s, a), K_BOTTOMFACE },
    { offsetof(struct controller_data_s, b), K_RIGHTFACE },
    { offsetof(struct controller_data_s, x), K_LEFTFACE },
    { offsetof(struct controller_data_s, y), K_TOPFACE },
    { offsetof(struct controller_data_s, lb), K_LTRIGGER },
    { offsetof(struct controller_data_s, rb), K_RTRIGGER },
    { offsetof(struct controller_data_s, s1_z), K_LTHUMB },
    { offsetof(struct controller_data_s, s2_z), K_RTHUMB },
    { offsetof(struct controller_data_s, up), K_UPARROW },
    { offsetof(struct controller_data_s, down), K_DOWNARROW },
    { offsetof(struct controller_data_s, left), K_LEFTARROW },
    { offsetof(struct controller_data_s, right), K_RIGHTARROW }
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

void
IN_PlatformInit(void)
{
    extern cvar_t cl_controllerglyphs;

    Cvar_Set("cl_controllerglyphs", "xbox");
    cl_controllerglyphs.flags |= CVAR_ROM;
}

void
IN_PlatformShutdown(void)
{
    set_controller_rumble(0, 0, 0);
    memset(&pad, 0, sizeof(pad));
    previous_buttons = 0;
    rumble_end = 0;
}

void
Sys_SendKeyEvents(void)
{
    struct controller_data_s next;
    unsigned current = 0;
    size_t i;

    usb_do_poll();
    if (get_controller_data(&next, 0)) {
        pad = next;
    }
    for (i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i) {
        if (*(const int *) ((const byte *) &pad + buttons[i].offset))
            current |= 1u << i;
        if ((current ^ previous_buttons) & (1u << i))
            Key_Event(buttons[i].key, (current & (1u << i)) != 0);
    }
    if (pad.lt > 32)
        current |= 1u << i;
    if ((current ^ previous_buttons) & (1u << i))
        Key_Event(K_ZLTRIGGER, pad.lt > 32);
    ++i;
    if (pad.rt > 32)
        current |= 1u << i;
    if ((current ^ previous_buttons) & (1u << i))
        Key_Event(K_ZRTRIGGER, pad.rt > 32);
    previous_buttons = current;
}

static float
StickAxis(short value)
{
    return value < 0 ? value / 32768.0f : value / 32767.0f;
}

void
IN_GetAnalogStick(in_analog_stick_id_t which, in_analog_stick_t * value)
{
    value->x = StickAxis(which == IN_STICK_LEFT ? pad.s1_x : pad.s2_x);
    value->y = StickAxis(which == IN_STICK_LEFT ? pad.s1_y : pad.s2_y);
}

void
IN_PlatformRumble(unsigned short low, unsigned short high, unsigned int duration)
{
    unsigned low_strength = min(65535u, (unsigned) low * 16u);
    unsigned high_strength = min(65535u, (unsigned) high * 16u);

    set_controller_rumble(0, duration ? low_strength / 257 : 0, duration ? high_strength / 257 : 0);
    rumble_end = duration ? Sys_FloatTime() + duration / 1000.0 : 0;
}

void
IN_PlatformCommands(void)
{
    if (rumble_end && Sys_FloatTime() >= rumble_end) {
        set_controller_rumble(0, 0, 0);
        rumble_end = 0;
    }
}
