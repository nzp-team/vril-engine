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
cvar_t retrotouch = {"retrotouch", "0", true};
cvar_t psvita_touchmode = {"psvita_touchmode", "0", true};
cvar_t psvita_front_sensitivity_x = {"psvita_front_sensitivity_x", "1", true};
cvar_t psvita_front_sensitivity_y = {"psvita_front_sensitivity_y", "0.5", true};
cvar_t psvita_back_sensitivity_x = {"psvita_back_sensitivity_x", "1", true};
cvar_t psvita_back_sensitivity_y = {"psvita_back_sensitivity_y", "0.5", true};

extern void Log (const char *format, ...);

#define lerp(value, from_max, to_max) ((((value*10) * (to_max*10))/(from_max*10))/10)

extern bool croshhairmoving;
extern float crosshair_opacity;

SceCtrlData oldanalogs, analogs;
static double rumble_stop_time;

static int IN_PSP2ControllerType(void)
{
	SceCtrlPortInfo info;
	memset(&info, 0, sizeof(info));
	if (sceCtrlGetControllerPortInfo(&info) < 0)
		return SCE_CTRL_TYPE_UNPAIRED;
	return info.port[1];
}

static void IN_PSP2SetRumble(byte small, byte large)
{
	SceCtrlActuator actuator;
	if (IN_PSP2ControllerType() == SCE_CTRL_TYPE_UNPAIRED)
		return;
	memset(&actuator, 0, sizeof(actuator));
	actuator.small = small;
	actuator.large = large;
	sceCtrlSetActuator(1, &actuator);
}

qboolean IN_PlatformHasMouse(void) { return false; }
qboolean IN_PlatformHasGamepad(void) { return true; }
void IN_SetMouseToRelative(bool relative) { (void)relative; }
void IN_PlatformClearPendingInput(void) {}
void IN_PlatformMouseMove(usercmd_t *cmd) { (void)cmd; }

void IN_PlatformInit(void)
{
  Cvar_SetValue("in_anub_mode", 1);
	Cvar_RegisterVariable (&m_filter);
	Cvar_RegisterVariable (&retrotouch);
	Cvar_RegisterVariable(&psvita_touchmode);

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
	IN_PSP2SetRumble(0, 0);
	sceMotionStopSampling();
}

void IN_PlatformCommands(void)
{
	if (rumble_stop_time && Sys_FloatTime() >= rumble_stop_time) {
		IN_PSP2SetRumble(0, 0);
		rumble_stop_time = 0.0;
	}
}

qboolean IN_PlatformGetGyro(float *x, float *y)
{
	SceMotionState state;
	*x = *y = 0.0f;
	if (sceMotionGetState(&state) < 0)
		return false;
	*x = -state.angularVelocity.x;
	*y = state.angularVelocity.y;
	return true;
}

void IN_PlatformRumble(unsigned short low_frequency, unsigned short high_frequency, unsigned int duration)
{
	IN_PSP2SetRumble((byte)(high_frequency / 257), (byte)(low_frequency / 257));
	rumble_stop_time = Sys_FloatTime() + (double)duration / 1000.0;
}

void IN_PlatformSetLightbar(byte red, byte green, byte blue)
{
	if (IN_PSP2ControllerType() == SCE_CTRL_TYPE_DS4)
		sceCtrlSetLightBar(1, red, green, blue);
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
