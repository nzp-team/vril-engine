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

#include "../../nzportable_def.h"
#include <vitasdk.h>

cvar_t m_filter = {"m_filter", "0", true};
cvar_t pstv_rumble = {"pstv_rumble", "1", true};
cvar_t retrotouch = {"retrotouch", "0", true};
cvar_t psvita_touchmode = {"psvita_touchmode", "0", true};
cvar_t psvita_front_sensitivity_x = {"psvita_front_sensitivity_x", "1", true};
cvar_t psvita_front_sensitivity_y = {"psvita_front_sensitivity_y", "0.5", true};
cvar_t psvita_back_sensitivity_x = {"psvita_back_sensitivity_x", "1", true};
cvar_t psvita_back_sensitivity_y = {"psvita_back_sensitivity_y", "0.5", true};
cvar_t motioncam = {"motioncam", "0", true};
cvar_t motion_horizontal_sensitivity = {"motion_horizontal_sensitivity", "0", true};
cvar_t motion_vertical_sensitivity = {"motion_vertical_sensitivity", "0", true};

extern void Log (const char *format, ...);

#define lerp(value, from_max, to_max) ((((value*10) * (to_max*10))/(from_max*10))/10)

extern bool croshhairmoving;
extern float crosshair_opacity;

uint64_t rumble_tick = 0;
SceCtrlData oldanalogs, analogs;
SceMotionState motionstate;

void IN_PlatformInit(void)
{
  Cvar_SetValue("in_anub_mode", 1);
  Cvar_RegisterVariable (&m_filter);
  Cvar_RegisterVariable (&retrotouch);
  Cvar_RegisterVariable (&pstv_rumble);
  Cvar_RegisterVariable(&psvita_touchmode);

  Cvar_RegisterVariable (&motioncam);
  Cvar_RegisterVariable (&motion_horizontal_sensitivity);
  Cvar_RegisterVariable (&motion_vertical_sensitivity);

  //Touchscreen sensitivity
  Cvar_RegisterVariable(&psvita_front_sensitivity_x);
  Cvar_RegisterVariable(&psvita_front_sensitivity_y);
  Cvar_RegisterVariable(&psvita_back_sensitivity_x);
  Cvar_RegisterVariable(&psvita_back_sensitivity_y);

  sceMotionReset();
  sceMotionStartSampling();
}

void IN_PlatformShutdown(void)
{
}

void IN_PlatformCommands(void)
{
}

void IN_StartRumble (void)
{
	if (!pstv_rumble.value) return;
	SceCtrlActuator handle;
	handle.small = 100;
	handle.large = 100;
	sceCtrlSetActuator(1, &handle);
	rumble_tick = sceKernelGetProcessTimeWide();
}

void IN_StopRumble (void)
{
	SceCtrlActuator handle;
	handle.small = 0;
	handle.large = 0;
	sceCtrlSetActuator(1, &handle);
	rumble_tick = 0;
}

// void IN_RescaleAnalog(int *x, int *y, int dead) {
// 	//radial and scaled deadzone
// 	//http://www.third-helix.com/2013/04/12/doing-thumbstick-dead-zones-right.html

// 	float analogX = (float) *x;
// 	float analogY = (float) *y;
// 	float deadZone = (float) dead;
// 	float maximum = 128.0f;
// 	float magnitude = sqrt(analogX * analogX + analogY * analogY);
// 	if (magnitude >= deadZone)
// 	{
// 		float scalingFactor = maximum / magnitude * (magnitude - deadZone) / (maximum - deadZone);
// 		*x = (int) (analogX * scalingFactor);
// 		*y = (int) (analogY * scalingFactor);
// 	} else {
// 		*x = 0;
// 		*y = 0;
// 	}
// }


void IN_GetAnalogStick(in_analog_stick_id_t stick, in_analog_stick_t *value)
{
	SceCtrlData state;
	sceCtrlPeekBufferPositive(0, &state, 1);
	if (stick == IN_STICK_LEFT) {
		value->x = ((float)state.lx - 127.5f) / 127.5f;
		value->y = (127.5f - (float)state.ly) / 127.5f;
	} else {
		value->x = ((float)state.rx - 127.5f) / 127.5f;
		value->y = ((float)state.ry - 127.5f) / 127.5f;
	}
}

void IN_PlatformMove(usercmd_t *cmd)
{
	(void)cmd;
}
