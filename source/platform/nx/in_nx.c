/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// in_ctr.c -- for the Nintendo 3DS

#include "../../nzportable_def.h"
#include <switch.h>
#include <limits.h>

extern int bind_grab;

extern bool croshhairmoving;
extern float crosshair_opacity;

extern cvar_t in_mlook; //Heffo - mlook cvar
extern cvar_t in_anub_mode;

void IN_Init (void)
{
	Cvar_SetValue("in_anub_mode", 1);
}

void IN_Shutdown (void)
{

}

void IN_Commands (void)
{

}

float IN_CalcInput(int axis, float speed, float tolerance, float acceleration) {

	float value = ((float) axis / (SHRT_MAX/2));

	if (value == 0.0f) {
		return 0.0f;
	}

	float abs_value = fabsf(value);

	if (abs_value < tolerance) {
		return 0.0f;
	}

	abs_value -= tolerance;
	abs_value /= (1.0f - tolerance);
	abs_value = powf(abs_value, acceleration);
	abs_value *= speed;

	if (value < 0.0f) {
		value = -abs_value;
	} else {
		value = abs_value;
	}
	return value;
}

extern cvar_t scr_fov;
extern int original_fov, final_fov;
extern PadState pad;
void IN_Move (usercmd_t *cmd)
{
	V_StopPitchDrift();

	// Read the pad states
	HidAnalogStickState left = padGetStickPos(&pad, 0);
	HidAnalogStickState right = padGetStickPos(&pad, 1);

	// Convert the inputs to floats in the range [-1, 1].
	// Implement the dead zone.
	float speed;
	float deadZone = in_tolerance.value;
	float acceleration = in_acceleration.value;
	float look_x, look_y;

	// 
	// Analog look tweaks
	//
	speed = sensitivity.value;

	if (!in_anub_mode.value)
		speed -= 2;
	else
		speed += 8;

	// cut look speed in half when facing enemy, unless mag is empty
	if ((in_aimassist.value) && (sv_player->v.facingenemy == 1) && cl.stats[STAT_CURRENTMAG] > 0) {
		speed *= 0.5f;
	}
	// additionally, slice look speed when ADS/scopes
	if (cl.stats[STAT_ZOOM] == 1)
		speed *= 0.5f;
	else if (cl.stats[STAT_ZOOM] == 2)
		speed *= 0.25f;
	
	// Are we using the left or right stick for looking?
	if (!in_anub_mode.value) { // Left
		look_x = IN_CalcInput(left.x, speed, deadZone, acceleration);
		look_y = IN_CalcInput(left.y, speed, deadZone, acceleration);
	} else { // Right
		look_x = IN_CalcInput(right.x, speed, deadZone, acceleration);
		look_y = IN_CalcInput(right.y, speed, deadZone, acceleration);
	}

	const float yawScale = 30.0f;
	cl.viewangles[YAW] -= yawScale * look_x * (float)host_frametime;

	// Set the pitch.
	const bool invertPitch = m_pitch.value < 0;
	const float pitchScale = yawScale * (invertPitch ? 1 : -1);

	cl.viewangles[PITCH] += pitchScale * look_y * (float)host_frametime;

	// Don't look too far up or down.
	if (cl.viewangles[PITCH] > 80.0f)
		cl.viewangles[PITCH] = 80.0f;
	if (cl.viewangles[PITCH] < -70.0f)
		cl.viewangles[PITCH] = -70.0f;

	// Ability to move with the left nub on NEW model systems
	float move_x, move_y;
	float input_x, input_y;

	if (in_anub_mode.value) {
		input_x = left.x;
		input_y = left.y;
	} else {
		input_x = right.x;
		input_y = right.y;
	}

	cl_backspeed = cl_forwardspeed = cl_sidespeed = sv_player->v.maxspeed;
	cl_sidespeed *= 0.8f;
	cl_backspeed *= 0.7f;

	move_x = IN_CalcInput(input_x, cl_sidespeed, deadZone, acceleration);

	if (input_y > 0)
		move_y = IN_CalcInput(input_y, cl_forwardspeed, deadZone, acceleration);
	else
		move_y = IN_CalcInput(input_y, cl_backspeed, deadZone, acceleration);

	// cypress -- explicitly setting instead of adding so we always prioritize
	// analog movement over standard bindings if both are at play
	if (move_x != 0 || move_y != 0) {
		cmd->sidemove = move_x;
		cmd->forwardmove = move_y;
	} 

	// crosshair stuff
	if (input_x < 50 && input_x > -50 && input_y < 50 && input_y > -50) {
		croshhairmoving = false;

		crosshair_opacity += 22;

		if (crosshair_opacity >= 255)
			crosshair_opacity = 255;
	} else {
		croshhairmoving = true;
		crosshair_opacity -= 8;
		if (crosshair_opacity <= 128)
			crosshair_opacity = 128;
	}
}

//
// ctr software keyboard courtesy of libctru samples
//
void IN_SwitchKeyboard(void)
{
	static SwkbdConfig swkbd;
	static char console_buffer[64];

	swkbdCreate(&swkbd, 0);
	swkbdConfigMakePresetDefault(&swkbd);
	swkbdConfigSetInitialText(&swkbd, console_buffer);
	swkbdConfigSetGuideText(&swkbd, "Enter Quake console command");
	swkbdConfigSetOkButtonText(&swkbd, "Send");
	Result rc = swkbdShow(&swkbd, console_buffer, sizeof(console_buffer));
    if (R_SUCCEEDED(rc))
		Cbuf_AddText(va("%s\n", console_buffer));
	swkbdClose(&swkbd);
}