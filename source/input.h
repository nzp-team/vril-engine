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
// input.h -- external (non-keyboard) input devices

void IN_Init (void);

void IN_Shutdown (void);

void IN_Commands (void);
// oportunity for devices to stick commands on the script buffer

void IN_Move (usercmd_t *cmd);
// add additional movement on top of the keyboard move cmd

void IN_ClearPendingInput(void);

#ifdef PLATFORM_INPUT_KBM
void IN_SetMouseToRelative(bool relative);
void IN_PlatformClearPendingInput(void);
#endif

#ifdef PLATFORM_INPUT_GAMEPAD
typedef enum {
	IN_STICK_LEFT,
	IN_STICK_RIGHT
} in_analog_stick_id_t;

typedef struct {
	float x;
	float y;
} in_analog_stick_t;

// Platform input backends normalize both axes to [-1, 1], with positive Y up.
void IN_GetAnalogStick(in_analog_stick_id_t stick, in_analog_stick_t *value);
void IN_PlatformInit(void);
void IN_PlatformShutdown(void);
void IN_PlatformCommands(void);
void IN_PlatformMove(usercmd_t *cmd);
#endif

#ifdef PLATFORM_KEYBOARD_SYSTEM
void IN_OpenOSKeyboard (void);
#endif
