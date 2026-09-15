/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2026 NZ:P Team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
*/
// input.c -- platform-independent input handling

#include "nzportable_def.h"

extern qboolean croshhairmoving;
extern float crosshair_opacity;
extern cvar_t in_anub_mode;
static in_device_t in_active_device = IN_DEVICE_KEYBOARD_MOUSE;

#ifdef PLATFORM_SUPPORTS_GYRO
cvar_t in_gyro_mode = {"in_gyro_mode", "0", true};
cvar_t in_gyro_sensitivity_x = {"in_gyro_sensitivity_x", "1.0", true};
cvar_t in_gyro_sensitivity_y = {"in_gyro_sensitivity_y", "1.0", true};
cvar_t in_gyro_zoom_scaling = {"in_gyro_zoom_scaling", "1", true};
#endif
#ifdef PLATFORM_SUPPORTS_RUMBLE
cvar_t in_rumble = {"in_rumble", "1", true};
#endif
#ifdef PLATFORM_SUPPORTS_LIGHTBAR
cvar_t in_lightbar = {"in_lightbar", "1", true};
static double in_lightbar_muzzleflash_time;
static byte in_lightbar_muzzleflash_color[3];
#endif

static float IN_Clamp(float value, float minimum, float maximum)
{
	if (value < minimum)
		return minimum;
	if (value > maximum)
		return maximum;
	return value;
}

void IN_SetActiveDevice(in_device_t device) { in_active_device = device; }
in_device_t IN_GetActiveDevice(void) { return in_active_device; }

qboolean IN_KeyMatchesDevice(int key, in_device_t device)
{
	qboolean gamepad_key =
		(key >= K_BOTTOMFACE && key <= K_RTHUMB) ||
		(key >= K_DPAD_UP && key <= K_DPAD_RIGHT) ||
		(key >= K_JOY1 && key <= K_JOY4);
	return device == IN_DEVICE_GAMEPAD ? gamepad_key : !gamepad_key;
}

qboolean IN_KeyMatchesActiveDevice(int key)
{
	return IN_KeyMatchesDevice(key, in_active_device);
}

void IN_Init(void)
{
#ifdef PLATFORM_SUPPORTS_GYRO
	Cvar_RegisterVariable(&in_gyro_mode);
	Cvar_RegisterVariable(&in_gyro_sensitivity_x);
	Cvar_RegisterVariable(&in_gyro_sensitivity_y);
	Cvar_RegisterVariable(&in_gyro_zoom_scaling);
#endif
#ifdef PLATFORM_SUPPORTS_RUMBLE
	Cvar_RegisterVariable(&in_rumble);
#endif
#ifdef PLATFORM_SUPPORTS_LIGHTBAR
	Cvar_RegisterVariable(&in_lightbar);
#endif
	if (IN_PlatformHasGamepad() && !IN_PlatformHasMouse())
		IN_SetActiveDevice(IN_DEVICE_GAMEPAD);
	if (IN_PlatformHasMouse()) IN_SetMouseToRelative(true);
	if (IN_PlatformHasGamepad()) IN_PlatformInit();
}

#ifdef PLATFORM_SUPPORTS_RUMBLE
void IN_StartRumble(int low_frequency, int high_frequency, int duration)
{
	if (!in_rumble.value || duration <= 0)
		return;
	IN_PlatformRumble((unsigned short)IN_Clamp(low_frequency, 0, 65535),
		(unsigned short)IN_Clamp(high_frequency, 0, 65535),
		(unsigned int)duration);
}
#endif

#ifdef PLATFORM_SUPPORTS_LIGHTBAR
void IN_TriggerLightbarMuzzleFlash(int red, int green, int blue)
{
	if (red == 255 && green == 255 && blue == 255) {
		red = 255;
		green = 160;
		blue = 32;
	}
	in_lightbar_muzzleflash_color[0] = (byte)IN_Clamp(red, 0, 255);
	in_lightbar_muzzleflash_color[1] = (byte)IN_Clamp(green, 0, 255);
	in_lightbar_muzzleflash_color[2] = (byte)IN_Clamp(blue, 0, 255);
	in_lightbar_muzzleflash_time = realtime + 0.1;
}

void IN_UpdateLightbar(void)
{
	int red, green, blue;
	int player = cl.viewentity > 0 ? cl.viewentity - 1 : 0;
	float damage_mix = 0.0f;

	if (!in_lightbar.value) {
		IN_PlatformSetLightbar(0, 0, 0);
		return;
	}
	if (realtime < in_lightbar_muzzleflash_time) {
		IN_PlatformSetLightbar(in_lightbar_muzzleflash_color[0],
			in_lightbar_muzzleflash_color[1], in_lightbar_muzzleflash_color[2]);
		return;
	}

	CL_PlayerColor(player, &red, &green, &blue);
	if (cl.stats[STAT_HEALTH] > 0 && cl.stats[STAT_HEALTH] < 100) {
		int pulse_value, pulse_add;
		if (cl.stats[STAT_HEALTH] < 50) {
			pulse_value = abs(((int)(realtime * 100) & 100) - 50);
			pulse_add = 50;
		} else {
			pulse_value = abs(((int)(realtime * 50) & 20) - 10);
			pulse_add = 10;
		}
		damage_mix = (200.0f - ((cl.stats[STAT_HEALTH] + pulse_value)
			/ (100.0f + pulse_add)) * 255.0f) / 255.0f;
		damage_mix = IN_Clamp(damage_mix, 0.0f, 1.0f);
	}
	red = (int)(red + (255 - red) * damage_mix);
	green = (int)(green * (1.0f - damage_mix));
	blue = (int)(blue * (1.0f - damage_mix));
	IN_PlatformSetLightbar((byte)red, (byte)green, (byte)blue);
}
#endif

void IN_Shutdown(void)
{
	if (IN_PlatformHasMouse()) IN_SetMouseToRelative(false);
	if (IN_PlatformHasGamepad()) IN_PlatformShutdown();
}

void IN_Commands(void)
{
	if (IN_PlatformHasGamepad()) IN_PlatformCommands();
#ifdef PLATFORM_SUPPORTS_LIGHTBAR
	IN_UpdateLightbar();
#endif
}

void IN_ClearPendingInput(void)
{
	Key_ClearStates();
	if (IN_PlatformHasMouse()) IN_PlatformClearPendingInput();
}

static float IN_ShapeAxis(float value, float speed, float tolerance, float acceleration)
{
	float magnitude;

	value = IN_Clamp(value, -1.0f, 1.0f);
	magnitude = fabsf(value);
	tolerance = IN_Clamp(tolerance, 0.0f, 0.99f);
	if (magnitude <= tolerance)
		return 0.0f;

	magnitude = (magnitude - tolerance) / (1.0f - tolerance);
	magnitude = powf(magnitude, acceleration) * speed;
	return value < 0.0f ? -magnitude : magnitude;
}

void IN_Move(usercmd_t *cmd)
{
	in_analog_stick_t left, right;
	in_analog_stick_t move_stick, look_stick;
	float speed, look_x, look_y, move_x, move_y;
	float tolerance = in_tolerance.value;
	float acceleration = in_acceleration.value;

	if (key_dest != key_game || cl.paused)
		return;

	left.x = left.y = right.x = right.y = 0.0f;
	if (IN_PlatformHasGamepad()) {
		IN_GetAnalogStick(IN_STICK_LEFT, &left);
		IN_GetAnalogStick(IN_STICK_RIGHT, &right);
		IN_PlatformMove(cmd);
	}
	if (IN_PlatformHasMouse()) IN_PlatformMouseMove(cmd);

#ifdef PLATFORM_SUPPORTS_GYRO
	if ((int)in_gyro_mode.value == 1 || ((int)in_gyro_mode.value == 2 && (cl.stats[STAT_ZOOM] == 1 || cl.stats[STAT_ZOOM] == 2))) {
		float gyro_x, gyro_y;

		if (IN_PlatformGetGyro(&gyro_x, &gyro_y)) {
			const float radians_to_degrees = 57.2957795131f;
			float gyro_scale = 1.0f;

			if (in_gyro_zoom_scaling.value) {
				if (cl.stats[STAT_ZOOM] == 1)
					gyro_scale = 0.5f;
				else if (cl.stats[STAT_ZOOM] == 2)
					gyro_scale = 0.25f;
			}

			V_StopPitchDrift();
			cl.viewangles[YAW] += gyro_y * radians_to_degrees * in_gyro_sensitivity_x.value * gyro_scale * (float)host_frametime;
			cl.viewangles[PITCH] -= gyro_x * radians_to_degrees * in_gyro_sensitivity_y.value * (m_pitch.value > 0 ? -1.0f : 1.0f) * gyro_scale * (float)host_frametime;
			cl.viewangles[PITCH] = IN_Clamp(cl.viewangles[PITCH], -70.0f, 80.0f);
		}
	}
#endif

#ifdef PLATFORM_HAS_ONE_ANALOG_STICK
	if (in_anub_mode.value) {
		move_stick = left;
		look_stick = right;
	} else {
		move_stick = right;
		look_stick = left;
	}
#else
	move_stick = left;
	look_stick = right;
#endif

	speed = sensitivity.value;
	if (IN_GetActiveDevice() == IN_DEVICE_GAMEPAD && in_aimassist.value &&
		sv_player->v.facingenemy == 1 && cl.stats[STAT_CURRENTMAG] > 0)
		speed *= 0.5f;
	if (cl.stats[STAT_ZOOM] == 1)
		speed *= 0.5f;
	else if (cl.stats[STAT_ZOOM] == 2)
		speed *= 0.25f;

	look_x = IN_ShapeAxis(look_stick.x, speed, tolerance, acceleration);
	look_y = IN_ShapeAxis(look_stick.y, speed, tolerance, acceleration);
	V_StopPitchDrift();
	cl.viewangles[YAW] -= 30.0f * look_x * (float)host_frametime;
	cl.viewangles[PITCH] += 30.0f * (m_pitch.value > 0 ? 1.0f : -1.0f)
		* look_y * (float)host_frametime;
	cl.viewangles[PITCH] = IN_Clamp(cl.viewangles[PITCH], -70.0f, 80.0f);

	cl_backspeed = cl_forwardspeed = cl_sidespeed = sv_player->v.maxspeed;
	cl_sidespeed *= 0.8f;
	cl_backspeed *= 0.7f;
	move_x = IN_ShapeAxis(move_stick.x, cl_sidespeed, tolerance, acceleration);
	move_y = IN_ShapeAxis(move_stick.y,
		move_stick.y >= 0.0f ? cl_forwardspeed : cl_backspeed,
		tolerance, acceleration);
	if (move_x != 0.0f || move_y != 0.0f) {
		cmd->sidemove = move_x;
		cmd->forwardmove = move_y;
	}

	if (cmd->forwardmove == 0.0f && cmd->sidemove == 0.0f && cl.onground) {
		croshhairmoving = false;
		crosshair_opacity = IN_Clamp(crosshair_opacity + 22.0f, 0.0f, 255.0f);
	} else {
		croshhairmoving = true;
		crosshair_opacity = IN_Clamp(crosshair_opacity - 8.0f, 128.0f, 255.0f);
	}
}
