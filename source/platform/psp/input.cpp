/*
Copyright (C) 2007 Peter Mackay and Chris Swindle.

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

#include <pspctrl.h>

extern "C"
{
#include "../../nzportable_def.h"
}

namespace quake
{
	namespace input
	{
		// A map from button mask to Quake key.
		static const unsigned int	buttonCount	= sizeof(unsigned int) * 8;
        typedef int	ButtonToKeyMap[buttonCount];
		static ButtonToKeyMap		buttonToGameKeyMap;
		static ButtonToKeyMap		buttonToConsoleKeyMap;
		static ButtonToKeyMap		buttonToMessageKeyMap;
		static ButtonToKeyMap		buttonToMenuKeyMap;

		// The previous key state (for checking if things changed).
		static SceCtrlData		lastPad;
		static bool				readyToBindKeys	= false;

		static unsigned int buttonMaskToShift(unsigned int mask)
		{
			// Bad mask?
			if (!mask)
				return 0;

			// Shift right until we hit a 1.
			unsigned int shift = 0;
			while ((mask & 1) == 0)
			{
				mask >>= 1;
				++shift;
			}
			return shift;
		}
	}
}

// Quake globals.
// INPUT TODO
int bind_grab = 0;

using namespace quake;
using namespace quake::input;

extern bool croshhairmoving;
extern float crosshair_opacity;

extern cvar_t in_anub_mode;
extern cvar_t in_mlook; //Heffo - mlook cvar

extern bool system_has_right_stick;

void IN_Init (void)
{
	// Set up the controller.
	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

	// Japanese users would prefer to have X as cancel and O as OK.
   	//unsigned int okButton		= PSP_CTRL_CIRCLE;
	//unsigned int cancelButton	= PSP_CTRL_CROSS;
	unsigned int okButton		= PSP_CTRL_CROSS;
	unsigned int cancelButton	= PSP_CTRL_CIRCLE;


	// Build the button to Quake key maps.
	// Common keys:
    buttonToGameKeyMap[buttonMaskToShift(PSP_CTRL_SELECT)]	= K_SELECT;
	buttonToGameKeyMap[buttonMaskToShift(PSP_CTRL_START)]	= K_ESCAPE;
	buttonToGameKeyMap[buttonMaskToShift(PSP_CTRL_HOME)]	= K_HOME;
	buttonToGameKeyMap[buttonMaskToShift(PSP_CTRL_NOTE)]	= K_NOTE;
	buttonToGameKeyMap[buttonMaskToShift(PSP_CTRL_SCREEN)]	= K_SCREEN;
	buttonToGameKeyMap[buttonMaskToShift(PSP_CTRL_UP)]		= K_UPARROW;
	buttonToGameKeyMap[buttonMaskToShift(PSP_CTRL_RIGHT)]	= K_RIGHTARROW;
	buttonToGameKeyMap[buttonMaskToShift(PSP_CTRL_DOWN)]	= K_DOWNARROW;
	buttonToGameKeyMap[buttonMaskToShift(PSP_CTRL_LEFT)]	= K_LEFTARROW;
	memcpy_vfpu(buttonToConsoleKeyMap, buttonToGameKeyMap, sizeof(ButtonToKeyMap));
	memcpy_vfpu(buttonToMessageKeyMap, buttonToGameKeyMap, sizeof(ButtonToKeyMap));
	memcpy_vfpu(buttonToMenuKeyMap, buttonToGameKeyMap, sizeof(ButtonToKeyMap));

	// Game keys:
	buttonToGameKeyMap[buttonMaskToShift(PSP_CTRL_LTRIGGER)]	= K_AUX1;
	buttonToGameKeyMap[buttonMaskToShift(PSP_CTRL_RTRIGGER)]	= K_AUX2;
	buttonToGameKeyMap[buttonMaskToShift(PSP_CTRL_TRIANGLE)]	= K_JOY1;
	buttonToGameKeyMap[buttonMaskToShift(PSP_CTRL_CIRCLE)]		= K_JOY2;
	buttonToGameKeyMap[buttonMaskToShift(PSP_CTRL_CROSS)]		= K_JOY3;
	buttonToGameKeyMap[buttonMaskToShift(PSP_CTRL_SQUARE)]		= K_JOY4;

	// Console keys:
	buttonToConsoleKeyMap[buttonMaskToShift(PSP_CTRL_LTRIGGER)]	= K_PGUP;
	buttonToConsoleKeyMap[buttonMaskToShift(PSP_CTRL_RTRIGGER)]	= K_PGDN;
	buttonToConsoleKeyMap[buttonMaskToShift(okButton)]			= K_ENTER;
	buttonToConsoleKeyMap[buttonMaskToShift(cancelButton)]		= K_ESCAPE;
	buttonToConsoleKeyMap[buttonMaskToShift(PSP_CTRL_TRIANGLE)]	= K_DEL;
	buttonToConsoleKeyMap[buttonMaskToShift(PSP_CTRL_SQUARE)]	= K_INS;

	// Message keys:
	memcpy_vfpu(buttonToMessageKeyMap, buttonToConsoleKeyMap, sizeof(ButtonToKeyMap));

	// Menu keys:
	buttonToMenuKeyMap[buttonMaskToShift(PSP_CTRL_SQUARE)]	= K_INS;
	buttonToMenuKeyMap[buttonMaskToShift(cancelButton)]		= K_ESCAPE;
	buttonToMenuKeyMap[buttonMaskToShift(okButton)]			= K_ENTER;
	buttonToMenuKeyMap[buttonMaskToShift(PSP_CTRL_TRIANGLE)]	= K_DEL;
	buttonToMenuKeyMap[buttonMaskToShift(PSP_CTRL_LTRIGGER)]	= K_AUX1;
	buttonToMenuKeyMap[buttonMaskToShift(PSP_CTRL_RTRIGGER)]	= K_AUX2;
}

void IN_Shutdown (void)
{

}

void IN_Commands (void)
{
	// Changed in or out of key binding mode?
	if ((bind_grab != 0) != readyToBindKeys)
	{
		// Was in key binding mode?
		if (readyToBindKeys)
		{
			// Just left key binding mode.
			// Release all keys which are pressed.
			for (unsigned int button = 0; button < buttonCount; ++button)
			{
				// Was the button pressed?
				if (lastPad.Buttons & (1 << button))
				{
					// Is the button in the map?
					const int key = buttonToGameKeyMap[button];
					if (key)
					{
						// Send a release event.
						Key_Event(key, false);
					}
				}
			}

			// We're not ready to bind keys any more.
			readyToBindKeys = false;
		}
		else
		{
			// Entering key binding mode.
			// Release all keys which are pressed.
			for (unsigned int button = 0; button < buttonCount; ++button)
			{
				// Was the button pressed?
				if (lastPad.Buttons & (1 << button))
				{
					// Is the button in the map?
					const int key = buttonToMenuKeyMap[button];
					if (key)
					{
						// Send a release event.
						Key_Event(key, false);
					}
				}
			}

			// We're now ready to bind keys.
			readyToBindKeys = true;
		}
	}

	// Use a different key mapping depending on where inputs are going to go.
	const ButtonToKeyMap* buttonToKeyMap = 0;
	static const ButtonToKeyMap* lastKeyMap = 0; 
	switch (key_dest)
	{
	case key_game:
		buttonToKeyMap = &buttonToGameKeyMap;
		break;

	case key_console:
		buttonToKeyMap = &buttonToConsoleKeyMap;
		break;

	case key_message:
		buttonToKeyMap = &buttonToMessageKeyMap;
		break;

	case key_menu:
	case key_menu_pause:
		// Binding keys?
		if (readyToBindKeys)
		{
			buttonToKeyMap = &buttonToGameKeyMap;
		}
		else
		{
			buttonToKeyMap = &buttonToMenuKeyMap;
		}
		break;

	default:
		Sys_Error("Unhandled key destination %d", key_dest);
		return;
	}

	// Read the pad state.
	SceCtrlData pad;
	sceCtrlPeekBufferPositive(&pad, 1);

	// Find out which buttons have changed.
	SceCtrlData deltaPad;
	deltaPad.Buttons	= pad.Buttons ^ lastPad.Buttons;
	deltaPad.Lx			= pad.Lx - lastPad.Lx;
	deltaPad.Ly			= pad.Ly - lastPad.Ly;
	deltaPad.TimeStamp	= pad.TimeStamp	- lastPad.TimeStamp;

	// Handle buttons which have changed.
	for (unsigned int button = 0; button < buttonCount; ++button)
	{
		// Has the button changed?
		const unsigned int buttonMask = 1 << button;

		if (deltaPad.Buttons & buttonMask)
		{
			// Is the button in the map?
			const int key = (*buttonToKeyMap)[button];
			if (key)
			{
				// Send an event.
				const qboolean	state	= (pad.Buttons & buttonMask) ? true : false;
				Key_Event(key, state);
			}
		}

		// shpuld: Following block is to emit key-up signal when keymap changes and button changes its key.
		// first check if keymap changed
		if (lastKeyMap && (lastKeyMap != buttonToKeyMap))
		{
			// was the button held down previously
			if (lastPad.Buttons & buttonMask)
			{
				int key = (*buttonToKeyMap)[button];
				int key_in_previous = (*lastKeyMap)[button];
				// did the current button actually change its key
				if (key_in_previous != key)
				{
					// Emit Key_Event with down=false
					Key_Event(key_in_previous, false);
				}
			}
		}
	}

	// Remember the pad state for next time.
	lastPad = pad;
	lastKeyMap = buttonToKeyMap;
}

float IN_CalcInput(int axis, float speed, float tolerance, float acceleration) {

	float value = ((float) axis / 128.0f) - 1.0f;

	if (value == 0.0f) {
		return 0.0f;
	}

	float abs_value = fabs(value);

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
void IN_Move (usercmd_t *cmd)
{
	V_StopPitchDrift();

	// Read the pad state.
	SceCtrlData pad;
	sceCtrlPeekBufferPositive(&pad, 1);

	// Convert the inputs to floats in the range [-1, 1].
	// Implement the dead zone.
	float speed;
	float deadZone = in_tolerance.value;
	float acceleration = in_acceleration.value;
	float look_x, look_y;

	//
	// Analog look tweaks
	//
	speed = in_sensitivity.value;

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
		look_x = IN_CalcInput(pad.Lx, speed, deadZone, acceleration);
		look_y = IN_CalcInput(pad.Ly, speed, deadZone, acceleration) * -1;
	} else { // Right
		look_x = IN_CalcInput(pad.Rsrv[0], speed, deadZone, acceleration);
		look_y = IN_CalcInput(pad.Rsrv[1], speed, deadZone, acceleration) * -1;

		if (!system_has_right_stick) {
			look_x = look_y = 0;
		}
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
		input_x = pad.Lx;
		input_y = 255 - pad.Ly;
	} else {
		input_x = pad.Rsrv[0];
		input_y = 255 - pad.Rsrv[1];

		if (!system_has_right_stick) {
			input_x = input_y = 128;
		}
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
	if (cmd->forwardmove == 0.0 && cmd->sidemove == 0.0 && cl.onground) {
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
