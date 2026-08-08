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

void IN_PlatformInit(void)
{
	// Set up the controller.
	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);

	// Build the button to Quake key maps.
	// Common keys:
    buttonToGameKeyMap[buttonMaskToShift(PSP_CTRL_SELECT)]	= K_SELECT;
	buttonToGameKeyMap[buttonMaskToShift(PSP_CTRL_START)]	= K_START;
	buttonToGameKeyMap[buttonMaskToShift(PSP_CTRL_UP)]		= K_UPARROW;
	buttonToGameKeyMap[buttonMaskToShift(PSP_CTRL_RIGHT)]	= K_RIGHTARROW;
	buttonToGameKeyMap[buttonMaskToShift(PSP_CTRL_DOWN)]	= K_DOWNARROW;
	buttonToGameKeyMap[buttonMaskToShift(PSP_CTRL_LEFT)]	= K_LEFTARROW;
	buttonToGameKeyMap[buttonMaskToShift(PSP_CTRL_LTRIGGER)]	= K_LTRIGGER;
	buttonToGameKeyMap[buttonMaskToShift(PSP_CTRL_RTRIGGER)]	= K_RTRIGGER;
	buttonToGameKeyMap[buttonMaskToShift(PSP_CTRL_TRIANGLE)]	= K_TOPFACE;
	buttonToGameKeyMap[buttonMaskToShift(PSP_CTRL_CIRCLE)]		= K_RIGHTFACE;
	buttonToGameKeyMap[buttonMaskToShift(PSP_CTRL_CROSS)]		= K_BOTTOMFACE;
	buttonToGameKeyMap[buttonMaskToShift(PSP_CTRL_SQUARE)]		= K_LEFTFACE;
	memcpy_vfpu(buttonToConsoleKeyMap, buttonToGameKeyMap, sizeof(ButtonToKeyMap));
	memcpy_vfpu(buttonToMessageKeyMap, buttonToGameKeyMap, sizeof(ButtonToKeyMap));
	memcpy_vfpu(buttonToMenuKeyMap, buttonToGameKeyMap, sizeof(ButtonToKeyMap));
	
	// Message keys:
	memcpy_vfpu(buttonToMessageKeyMap, buttonToConsoleKeyMap, sizeof(ButtonToKeyMap));
}

void IN_PlatformShutdown(void)
{

}

void IN_PlatformCommands(void)
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


void IN_GetAnalogStick(in_analog_stick_id_t stick, in_analog_stick_t *value)
{
	SceCtrlData pad;
	sceCtrlPeekBufferPositive(&pad, 1);
	if (stick == IN_STICK_LEFT) {
		value->x = ((float)pad.Lx - 127.5f) / 127.5f;
		value->y = (127.5f - (float)pad.Ly) / 127.5f;
	} else if (system_has_right_stick) {
		value->x = ((float)pad.Rsrv[0] - 127.5f) / 127.5f;
		value->y = (127.5f - (float)pad.Rsrv[1]) / 127.5f;
	} else {
		value->x = value->y = 0.0f;
	}
}

void IN_PlatformMove(usercmd_t *cmd)
{
	(void)cmd;
}
