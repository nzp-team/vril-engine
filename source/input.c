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
#ifdef PLATFORM_INPUT_GAMEPAD
extern cvar_t in_anub_mode;

static float IN_Clamp(float value, float minimum, float maximum)
{
	if (value < minimum)
		return minimum;
	if (value > maximum)
		return maximum;
	return value;
}
#endif

void IN_Init(void)
{
#ifdef PLATFORM_INPUT_KBM
	IN_SetMouseToRelative(true);
#elif defined(PLATFORM_INPUT_GAMEPAD)
	IN_PlatformInit();
#endif
}

void IN_Shutdown(void)
{
#ifdef PLATFORM_INPUT_KBM
	IN_SetMouseToRelative(false);
#elif defined(PLATFORM_INPUT_GAMEPAD)
	IN_PlatformShutdown();
#endif
}

#if !defined(PLATFORM_INPUT_GAMEPAD) && !defined(PLATFORM_INPUT_KBM)
void IN_Move(usercmd_t *cmd)
{
	(void)cmd;
}
#endif

void IN_Commands(void)
{
#ifdef PLATFORM_INPUT_GAMEPAD
	IN_PlatformCommands();
#endif
}

void IN_ClearPendingInput(void)
{
	Key_ClearStates();
#ifdef PLATFORM_INPUT_KBM
	IN_PlatformClearPendingInput();
#endif
}

#ifdef PLATFORM_INPUT_GAMEPAD
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

	IN_GetAnalogStick(IN_STICK_LEFT, &left);
	IN_GetAnalogStick(IN_STICK_RIGHT, &right);
	IN_PlatformMove(cmd);

	if (in_anub_mode.value) {
		move_stick = left;
		look_stick = right;
	} else {
		move_stick = right;
		look_stick = left;
	}

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
#endif
