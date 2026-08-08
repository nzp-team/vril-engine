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
#include <GL/picaGL.h>
#include <3ds.h>

#include "circle_pad_pro.h"

extern int bind_grab;

extern bool croshhairmoving;
extern float crosshair_opacity;

extern cvar_t in_mlook; //Heffo - mlook cvar
extern cvar_t in_anub_mode;

void IN_PlatformInit(void)
{
	if (new3ds_flag) {
		Cvar_SetValue("in_anub_mode", 1);
	}
	else{
		if(cppGetConnected()){
			circlepadpro_flag = true;
			Cvar_SetValue ("in_anub_mode", 1);
		}
		else{
			circlepadpro_flag = false;
			cppExit();
		}
	}
}

void IN_PlatformShutdown(void)
{

}

void IN_PlatformCommands(void)
{

}


void IN_GetAnalogStick(in_analog_stick_id_t stick, in_analog_stick_t *value)
{
	circlePosition state;
	if (stick == IN_STICK_LEFT)
		hidCircleRead(&state);
	else if (circlepadpro_flag)
		cppCircleRead(&state);
	else
		hidCstickRead(&state);
	value->x = (float)state.dx / 154.0f;
	value->y = (float)state.dy / 154.0f;
}

void IN_PlatformMove(usercmd_t *cmd)
{
	(void)cmd;
}

//
// ctr software keyboard courtesy of libctru samples
//
void IN_OpenOSKeyboard(void)
{
	static SwkbdState swkbd;
	static char console_buffer[64];

	swkbdInit(&swkbd, SWKBD_TYPE_QWERTY, 2, -1);
	swkbdSetInitialText(&swkbd, console_buffer);
	swkbdSetHintText(&swkbd, "Enter Quake console command");
	swkbdSetButton(&swkbd, SWKBD_BUTTON_RIGHT, "Send", true);
	swkbdInputText(&swkbd, console_buffer, sizeof(console_buffer));

	Cbuf_AddText(va("%s\n", console_buffer));
}
