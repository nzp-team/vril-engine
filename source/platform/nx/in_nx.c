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

void IN_PlatformInit(void)
{
	Cvar_SetValue("in_anub_mode", 1);
}

void IN_PlatformShutdown(void)
{

}

void IN_PlatformCommands(void)
{

}

extern PadState pad;
void IN_GetAnalogStick(in_analog_stick_id_t stick, in_analog_stick_t *value)
{
	HidAnalogStickState state = padGetStickPos(&pad, stick == IN_STICK_LEFT ? 0 : 1);
	value->x = (float)state.x / (float)SHRT_MAX;
	value->y = (float)state.y / (float)SHRT_MAX;
}

void IN_PlatformMove(usercmd_t *cmd) { (void)cmd; }


void IN_OpenOSKeyboard(void)
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
