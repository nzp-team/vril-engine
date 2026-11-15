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
#include <io/pad.h>
#include "ps3_local.h"

static padData pad;
static int port = -1, ready;
static unsigned capabilities, ui_dpad;
static double rumble_end;

static float gyro_bias = 512;
static unsigned gyro_samples;
static cvar_t ps3_motion_pitch = { "ps3_motion_pitch", "1", true };
static float pitch_angle, pitch_delta, yaw_delta;
static double motion_time;
static int motion_ready;

static double pitch_time;
static int pitch_ready;

static void
ResetMotion(void)
{
    gyro_samples = 0;
    gyro_bias    = 512;
    pitch_angle  = pitch_delta = yaw_delta = 0;
    pitch_time   = motion_time = 0;
    pitch_ready  = motion_ready = 0;
}

static void
UpdatePitch(double now)
{
    if (key_dest != key_game) {
        pitch_ready = 0;
        pitch_delta = 0;
        return;
    }

    float ax = 512 - (int) pad.button[20], ay = 512 - (int) pad.button[21],
      az         = 512 - (int) pad.button[22];
    float length = sqrtf(ax * ax + ay * ay + az * az);
    double dt    = now - pitch_time;

    if (length < 75 || length > 155 || ay * ay + az * az < 2500) {
        pitch_ready = 0;
        pitch_delta = 0;
        return;
    }

    float angle = atan2f(ay, az);

    if (!pitch_ready || dt <= 0 || dt > 0.25) {
        pitch_angle = angle;
        pitch_time  = now;
        pitch_ready = 1;
        return;
    }

    float delta = angle - pitch_angle;

    while (delta > 3.141592654f)
        delta -= 6.283185307f;

    while (delta < -3.141592654f)
        delta += 6.283185307f;

    pitch_angle  = angle;
    pitch_time   = now;
    pitch_delta += delta;
}

static const struct {
    unsigned word, mask;
    int      key, pressure;
} buttons[] = {
    { 2, PAD_CTRL_SELECT, K_SELECT, -1 },
    { 2, PAD_CTRL_START, K_START, -1 },
    { 2, PAD_CTRL_L3, K_LTHUMB, -1 },
    { 2, PAD_CTRL_R3, K_RTHUMB, -1 },
    { 2, PAD_CTRL_UP, K_UPARROW, 10 },
    { 2, PAD_CTRL_DOWN, K_DOWNARROW, 11 },
    { 2, PAD_CTRL_LEFT, K_LEFTARROW, 9 },
    { 2, PAD_CTRL_RIGHT, K_RIGHTARROW, 8 },
    { 3, PAD_CTRL_CROSS, K_BOTTOMFACE, 14 },
    { 3, PAD_CTRL_CIRCLE, K_RIGHTFACE, 13 },
    { 3, PAD_CTRL_SQUARE, K_LEFTFACE, 15 },
    { 3, PAD_CTRL_TRIANGLE, K_TOPFACE, 12 },
    { 3, PAD_CTRL_L1, K_LTRIGGER, 16 },
    { 3, PAD_CTRL_R1, K_RTRIGGER, 17 },
    { 3, PAD_CTRL_L2, K_ZLTRIGGER, 18 },
    { 3, PAD_CTRL_R2, K_ZRTRIGGER, 19 }
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
    if (port >= 0)
        ioPadClearBuf(port);

    pitch_delta = yaw_delta = 0;
    pitch_ready = motion_ready = 0;
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
Rumble(unsigned low, unsigned high)
{
    padActParam action = { 0 };

    action.motor[0] = high != 0;
    action.motor[1] = low / 257;

    if (port >= 0 && (capabilities & (1u << 4)))
        ioPadSetActDirect(port, &action);
}

static void
Release(void)
{
    size_t i;

    for (i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i)
        Key_PressureEvent(buttons[i].key, false, -1);

    static const int ui_keys[] = { K_DPAD_UP, K_DPAD_DOWN, K_DPAD_LEFT, K_DPAD_RIGHT };

    for (i = 0; i < 4; ++i)
        if (ui_dpad & (1u << i))
            Key_Event(ui_keys[i], false);

    ui_dpad = 0;
    Rumble(0, 0);
    rumble_end   = 0;
    port         = -1;
    capabilities = 0;
    ResetMotion();
    memset(&pad, 0, sizeof(pad));
}

void
IN_PlatformInit(void)
{
    extern cvar_t cl_controllerglyphs;

    Cvar_Set("cl_controllerglyphs", "sony");
    cl_controllerglyphs.flags |= CVAR_ROM;
    Cvar_RegisterVariable(&ps3_motion_pitch);
    Cmd_AddCommand("ps3_motion_calibrate", ResetMotion);
    Cmd_AddCommand("osk", IN_OpenOSKeyboard);
    Cmd_AddCommand("osk_cancel", PS3_OSKShutdown);

    if (ioPadInit(7) != 0)
        Sys_Error("PS3: ioPadInit failed");

    ready = 1;
}

void
IN_PlatformShutdown(void)
{
    if (!ready)
        return;

    PS3_OSKShutdown();
    Release();
    ioPadEnd();
    ready = 0;
}

void
Sys_SendKeyEvents(void)
{
    pitch_delta = yaw_delta = 0;
    padInfo2 info;
    padData next = { 0 };
    size_t i;

    if (!ready)
        return;

    if (PS3_OSKActive())
        return;

    if (ioPadGetInfo2(&info) != 0 || (info.info & 1)) {
        Release();
        return;
    }

    if (port >= 0 && !(info.port_status[port] & 1))
        Release();

    if (port < 0) {
        for (i = 0; i < 7; ++i)
            if (info.port_status[i] & 1) {
                unsigned setting = 0;
                port         = i;
                capabilities = info.device_capability[i];

                if (capabilities & 2)
                    setting |= PAD_SETTINGS_PRESS_ON;

                if (capabilities & 4)
                    setting |= PAD_SETTINGS_SENSOR_ON;

                if (ioPadSetPortSetting(port, setting) != 0)
                    Con_Printf("PS3: could not enable extended controller data\n");

                break;
            }
    }

    if (port < 0)
        return;

    if (ioPadGetData(port, &next) != 0) {
        Release();
        return;
    }

    if (next.len > 0)
        pad = next;

    static int select_down;
    int selected      = (pad.button[2] & PAD_CTRL_SELECT) != 0;
    int open_keyboard = selected && !select_down;
    select_down = selected;

    if (open_keyboard) {
        IN_OpenOSKeyboard();

        if (PS3_OSKActive())
            return;
    }

    for (i = 0; i < sizeof(buttons) / sizeof(buttons[0]); ++i) {
        int pressure = -1;

        if ((capabilities & 2) && buttons[i].pressure >= 0 && pad.len > buttons[i].pressure)
            pressure = pad.button[buttons[i].pressure];

        qboolean down = (pad.button[buttons[i].word] & buttons[i].mask) != 0;

        if (i >= 4 && i < 8) {
            static const int ui_keys[] = { K_DPAD_UP, K_DPAD_DOWN, K_DPAD_LEFT, K_DPAD_RIGHT };
            unsigned bit     = 1u << (i - 4);
            qboolean ui_down = down && key_dest != key_game;
            Key_PressureEvent(buttons[i].key, down && key_dest == key_game, pressure);

            if (ui_down != ((ui_dpad & bit) != 0))
                Key_Event(ui_keys[i - 4], ui_down);

            if (ui_down)
                ui_dpad |= bit;
            else
                ui_dpad &= ~bit;
        } else
            Key_PressureEvent(buttons[i].key, down, pressure);
    }

    if (next.len > 0 && (capabilities & 4) && pad.len > PAD_BUTTON_OFFSET_SENSOR_G &&
      gyro_samples < 64)
    {
        float sample = pad.button[PAD_BUTTON_OFFSET_SENSOR_G];

        if (fabsf(sample - 512) < 40) {
            gyro_bias += (sample - gyro_bias) / (++gyro_samples);
        }
    }

    if ((capabilities & 4) && pad.len > PAD_BUTTON_OFFSET_SENSOR_G) {
        double now = Sys_FloatTime(), dt = now - motion_time;

        if (motion_ready && dt > 0 && dt <= 0.25 && key_dest == key_game && gyro_samples >= 64)
            yaw_delta = (gyro_bias - pad.button[PAD_BUTTON_OFFSET_SENSOR_G])
              * (0.01745329252f * 90.0f / 123.0f) * dt;

        motion_time  = now;
        motion_ready = 1;

        if (next.len > 0)
            UpdatePitch(now);
        else if (key_dest != key_game || now - pitch_time > 0.25)
            pitch_ready = 0;
    }
}

void
IN_GetAnalogStick(in_analog_stick_id_t which, in_analog_stick_t * value)
{
    unsigned offset = which == IN_STICK_LEFT ? 6 : 4;

    value->x = value->y = 0;

    if (PS3_OSKActive() || port < 0 || pad.len < 8)
        return;

    int x = pad.button[offset] - 128, y = pad.button[offset + 1] - 128;
    value->x = x / (x < 0 ? 128.0f : 127.0f);
    value->y = -y / (y < 0 ? 128.0f : 127.0f);
}

qboolean
IN_PlatformGetGyro(float * x, float * y)
{
    *x = *y = 0;

    if (PS3_OSKActive() || port < 0 || !(capabilities & 4) ||
      pad.len <= PAD_BUTTON_OFFSET_SENSOR_G || gyro_samples < 64)
        return false;

    if (host_frametime > 0.000001f && key_dest == key_game) {
        *y = yaw_delta / host_frametime;

        if (ps3_motion_pitch.value)
            *x = pitch_delta / host_frametime;
    }

    pitch_delta = yaw_delta = 0;
    return true;
}

void
IN_PlatformRumble(unsigned short low, unsigned short high, unsigned int duration)
{
    if (PS3_OSKActive())
        duration = 0;

    Rumble(duration ? min(65535u, (unsigned) low * 16u) : 0,
      duration ? min(65535u, (unsigned) high * 16u) : 0);
    rumble_end = duration ? Sys_FloatTime() + duration / 1000.0 : 0;
}

void
IN_PlatformCommands(void)
{
    if (rumble_end && Sys_FloatTime() >= rumble_end) {
        Rumble(0, 0);
        rumble_end = 0;
    }
}
