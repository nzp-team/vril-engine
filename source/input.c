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
	if (IN_PlatformHasGamepad() && !IN_PlatformHasMouse())
		IN_SetActiveDevice(IN_DEVICE_GAMEPAD);
	if (IN_PlatformHasMouse()) IN_SetMouseToRelative(true);
	if (IN_PlatformHasGamepad()) IN_PlatformInit();
}

void IN_Shutdown(void)
{
	if (IN_PlatformHasMouse()) IN_SetMouseToRelative(false);
	if (IN_PlatformHasGamepad()) IN_PlatformShutdown();
}

void IN_Commands(void)
{
	if (IN_PlatformHasGamepad()) IN_PlatformCommands();
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
	if (in_aimassist.value && sv_player->v.facingenemy == 1 && cl.stats[STAT_CURRENTMAG] > 0)
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
