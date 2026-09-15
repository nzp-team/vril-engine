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

typedef struct {
	HidNpadIdType id;
	HidNpadStyleTag style;
	HidSixAxisSensorHandle gyro[2];
	HidVibrationDeviceHandle rumble[2];
	int gyro_count;
	int rumble_count;
} nx_motion_device_t;

static nx_motion_device_t nx_motion;
static double nx_rumble_stop_time;

extern PadState pad;

static void IN_NXSendRumble(float low, float high);

static HidNpadStyleTag IN_NXActiveStyle(HidNpadIdType *id)
{
	u32 styles;

	if (padIsNpadActive(&pad, HidNpadIdType_No1)) {
		*id = HidNpadIdType_No1;
		styles = hidGetNpadStyleSet(*id);
		if (styles & HidNpadStyleTag_NpadFullKey)
			return HidNpadStyleTag_NpadFullKey;
		if (styles & HidNpadStyleTag_NpadJoyDual)
			return HidNpadStyleTag_NpadJoyDual;
		if (styles & HidNpadStyleTag_NpadJoyRight)
			return HidNpadStyleTag_NpadJoyRight;
		if (styles & HidNpadStyleTag_NpadJoyLeft)
			return HidNpadStyleTag_NpadJoyLeft;
	}

	if (padIsHandheld(&pad)) {
		*id = HidNpadIdType_Handheld;
		return HidNpadStyleTag_NpadHandheld;
	}

	*id = HidNpadIdType_No1;
	return 0;
}

static void IN_NXStopGyro(void)
{
	int i;
	for (i = 0; i < nx_motion.gyro_count; ++i)
		hidStopSixAxisSensor(nx_motion.gyro[i]);
	nx_motion.gyro_count = 0;
}

static qboolean IN_NXRefreshDevice(void)
{
	HidNpadIdType id;
	HidNpadStyleTag style = IN_NXActiveStyle(&id);

	if (style == nx_motion.style && (!style || nx_motion.id == id))
		return style != 0;

	IN_NXSendRumble(0.0f, 0.0f);
	IN_NXStopGyro();
	nx_motion.rumble_count = 0;
	nx_motion.style = style;
	nx_motion.id = id;
	return style != 0;
}

static qboolean IN_NXBindGyro(void)
{
	int count;
	int i;

	if (!IN_NXRefreshDevice())
		return false;

	if (nx_motion.gyro_count)
		return true;

	count = nx_motion.style == HidNpadStyleTag_NpadJoyDual ? 2 : 1;

	if (R_FAILED(hidGetSixAxisSensorHandles(nx_motion.gyro, count, nx_motion.id, nx_motion.style)))
		return false;

	for (i = 0; i < count; ++i) {
		if (R_FAILED(hidStartSixAxisSensor(nx_motion.gyro[i]))) {
			IN_NXStopGyro();
			return false;
		}
		nx_motion.gyro_count++;
	}

	return true;
}

static qboolean IN_NXBindRumble(void)
{
	int count;

	if (!IN_NXRefreshDevice())
		return false;
	if (nx_motion.rumble_count)
		return true;

	count = (nx_motion.style == HidNpadStyleTag_NpadJoyLeft || nx_motion.style == HidNpadStyleTag_NpadJoyRight) ? 1 : 2;
	if (R_FAILED(hidInitializeVibrationDevices(nx_motion.rumble, count,
		nx_motion.id, nx_motion.style)))
		return false;

	nx_motion.rumble_count = count;
	return true;
}

static void IN_NXSendRumble(float low, float high)
{
	HidVibrationValue values[2];
	int i;

	for (i = 0; i < nx_motion.rumble_count; ++i) {
		values[i].amp_low = low;
		values[i].freq_low = 160.0f;
		values[i].amp_high = high;
		values[i].freq_high = 320.0f;
	}
	if (nx_motion.rumble_count)
		hidSendVibrationValues(nx_motion.rumble, values, nx_motion.rumble_count);
}

qboolean IN_PlatformHasMouse(void) { return false; }
qboolean IN_PlatformHasGamepad(void) { return true; }
void IN_SetMouseToRelative(bool relative) { (void)relative; }
void IN_PlatformClearPendingInput(void) {}
void IN_PlatformMouseMove(usercmd_t *cmd) { (void)cmd; }

void IN_PlatformInit(void)
{
	memset(&nx_motion, 0, sizeof(nx_motion));
	Cvar_SetValue("in_anub_mode", 1);
}

void IN_PlatformShutdown(void)
{
	IN_NXSendRumble(0.0f, 0.0f);
	IN_NXStopGyro();
}

void IN_PlatformCommands(void)
{
	if (nx_rumble_stop_time && Sys_FloatTime() >= nx_rumble_stop_time) {
		IN_NXSendRumble(0.0f, 0.0f);
		nx_rumble_stop_time = 0.0;
	}
}

qboolean IN_PlatformGetGyro(float *x, float *y)
{
	HidSixAxisSensorState state;
	const float revolutions_to_radians = 2.0f * (float)M_PI;
	int sensor = 0;

	*x = *y = 0.0f;
	if (!IN_NXBindGyro())
		return false;

	if (nx_motion.style == HidNpadStyleTag_NpadJoyDual && (padGetAttributes(&pad) & HidNpadAttribute_IsRightConnected))
		sensor = 1;
	if (!hidGetSixAxisSensorStates(nx_motion.gyro[sensor], &state, 1))
		return false;

	*x = -state.angular_velocity.x * revolutions_to_radians;
	*y = (nx_motion.style == HidNpadStyleTag_NpadJoyDual
		? state.angular_velocity.z : state.angular_velocity.y)
		* revolutions_to_radians;
	return true;
}

void IN_PlatformRumble(unsigned short low_frequency, unsigned short high_frequency, unsigned int duration)
{
	if (!IN_NXBindRumble())
		return;
	IN_NXSendRumble((float)low_frequency / 65535.0f, (float)high_frequency / 65535.0f);
	nx_rumble_stop_time = Sys_FloatTime() + (double)duration / 1000.0;
}

void IN_GetAnalogStick(in_analog_stick_id_t stick, in_analog_stick_t *value)
{
	HidAnalogStickState state = padGetStickPos(&pad, stick == IN_STICK_LEFT ? 0 : 1);
	value->x = (float)state.x / (float)SHRT_MAX;
	value->y = (float)state.y / (float)SHRT_MAX;
	if (stick == IN_STICK_RIGHT)
		value->y = -value->y;
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
