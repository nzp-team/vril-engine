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
#include <libpad.h>
#include <stddef.h>

static char pad_buffer[256] __attribute__((aligned(64)));
static struct padButtonStatus pad;
static unsigned short old_ui_dpad;
static qboolean pad_open, pad_analog;
enum {
	PAD_SETUP_START,
	PAD_SETUP_ANALOG,
	PAD_SETUP_ACTUATORS,
	PAD_SETUP_ALIGNED,
	PAD_SETUP_PRESSURE,
	PAD_SETUP_PRESSURE_WAIT,
	PAD_SETUP_READY,
	PAD_SETUP_DIGITAL
};
static int pad_setup;
static double pad_retry_time;
static qboolean rumble_ready, rumble_pending;
static double rumble_stop_time;
static unsigned int rumble_duration;
static char actuators[6], sent_actuators[6];
static qboolean rumble_sent_valid;
extern cvar_t in_pressure;
extern cvar_t in_rumble;
extern void Sys_PS2ProfileInit(void);

static struct
{
	int button;
	int key;
	int pressure;
} KeyTable[] = {
	{ PAD_SELECT, K_SELECT, -1 },
	{ PAD_START, K_START, -1 },
	{ PAD_L1, K_LTRIGGER, offsetof(struct padButtonStatus, l1_p) },
	{ PAD_R1, K_RTRIGGER, offsetof(struct padButtonStatus, r1_p) },
	{ PAD_L2, K_ZLTRIGGER, offsetof(struct padButtonStatus, l2_p) },
	{ PAD_R2, K_ZRTRIGGER, offsetof(struct padButtonStatus, r2_p) },
	{ PAD_L3, K_LTHUMB, -1 },
	{ PAD_R3, K_RTHUMB, -1 },
	{ PAD_SQUARE, K_LEFTFACE, offsetof(struct padButtonStatus, square_p) },
	{ PAD_TRIANGLE, K_TOPFACE, offsetof(struct padButtonStatus, triangle_p) },
	{ PAD_CROSS, K_BOTTOMFACE, offsetof(struct padButtonStatus, cross_p) },
	{ PAD_CIRCLE, K_RIGHTFACE, offsetof(struct padButtonStatus, circle_p) }
};

qboolean IN_PlatformHasMouse(void) { return false; }
qboolean IN_PlatformHasGamepad(void) { return true; }
void IN_SetMouseToRelative(bool relative) { (void)relative; }
void IN_PlatformClearPendingInput(void)
{
	old_ui_dpad = 0;
}
void IN_PlatformMouseMove(usercmd_t *cmd) { (void)cmd; }

static void IN_PS2RumbleTest(void)
{
	int duration = Cmd_Argc() > 1 ? atoi(Cmd_Argv(1)) : 500;
	Con_Printf("PS2 pad: state %d, request %d, setup %d, rumble %d\n",
		padGetState(0, 0), padGetReqState(0, 0), pad_setup, rumble_ready);
	IN_StartRumble(Cmd_Argc() > 2 ? atoi(Cmd_Argv(2)) : 65535,
		Cmd_Argc() > 3 ? atoi(Cmd_Argv(3)) : 65535, duration);
}

void IN_PlatformInit(void)
{
	extern cvar_t cl_controllerglyphs;

	Cvar_Set("cl_controllerglyphs", "sony");
	cl_controllerglyphs.flags |= CVAR_ROM;
	Cmd_AddCommand("ps2_rumble_test", IN_PS2RumbleTest);
	Sys_PS2ProfileInit();
	padInit(0);
	pad_open = padPortOpen(0, 0, pad_buffer) != 0;
	memset(&pad, 0, sizeof(pad));
	pad.ljoy_h = pad.ljoy_v = pad.rjoy_h = pad.rjoy_v = 128;
}

void IN_PlatformShutdown(void)
{
	if (pad_open) {
		memset(actuators, 0, sizeof(actuators));
		padSetActDirect(0, 0, actuators);
		padPortClose(0, 0);
		pad_open = false;
	}
}

static void IN_PS2Configure(void)
{
	static const char alignment[6] = {0, 1, -1, -1, -1, -1};
	int request = padGetReqState(0, 0), modes, i;
	double now = Sys_FloatTime();
	if (request == PAD_RSTAT_BUSY || now < pad_retry_time)
		return;
	if (request == PAD_RSTAT_FAILED && (pad_setup == PAD_SETUP_ALIGNED ||
		pad_setup == PAD_SETUP_PRESSURE_WAIT || pad_setup == PAD_SETUP_ANALOG)) {
		if (pad_setup == PAD_SETUP_ALIGNED) {
			pad_setup = PAD_SETUP_ACTUATORS;
			rumble_ready = false;
		} else if (pad_setup == PAD_SETUP_PRESSURE_WAIT)
			pad_setup = PAD_SETUP_PRESSURE;
		else if (pad_setup == PAD_SETUP_ANALOG) {
			pad_setup = PAD_SETUP_START;
			rumble_ready = false;
		}
		pad_retry_time = now + 0.25;
		return;
	}

	switch (pad_setup) {
	case PAD_SETUP_DIGITAL:
		pad_setup = PAD_SETUP_START;
		break;
	case PAD_SETUP_START:
		modes = padInfoMode(0, 0, PAD_MODETABLE, -1);
		if (modes <= 0) {
			pad_retry_time = now + 0.25;
			return;
		}
		for (i = 0; i < modes; ++i)
			if (padInfoMode(0, 0, PAD_MODETABLE, i) == PAD_TYPE_DUALSHOCK)
				break;
		if (i == modes) {
			pad_setup = PAD_SETUP_DIGITAL;
			pad_retry_time = now + 1;
			return;
		}
		if (padSetMainMode(0, 0, PAD_MMODE_DUALSHOCK, PAD_MMODE_LOCK) > 0)
			pad_setup = PAD_SETUP_ANALOG;
		break;
	case PAD_SETUP_ANALOG:
		if (pad_analog)
			pad_setup = PAD_SETUP_ACTUATORS;
		break;
	case PAD_SETUP_ACTUATORS:
		if (padInfoAct(0, 0, -1, 0) > 0) {
			if (padSetActAlign(0, 0, alignment) > 0)
				pad_setup = PAD_SETUP_ALIGNED;
		} else {
			pad_setup = PAD_SETUP_PRESSURE;
			pad_retry_time = now + 0.25;
		}
		break;
	case PAD_SETUP_ALIGNED:
		if (request != PAD_RSTAT_COMPLETE)
			return;
		rumble_ready = true;
		rumble_sent_valid = false;
		rumble_pending = true;
		pad_setup = PAD_SETUP_PRESSURE;
		break;
	case PAD_SETUP_PRESSURE:
		if (pad.mode == 0x79)
			pad_setup = PAD_SETUP_READY;
		else if (padInfoPressMode(0, 0) > 0) {
			if (padEnterPressMode(0, 0) > 0)
				pad_setup = PAD_SETUP_PRESSURE_WAIT;
		} else {
			pad_setup = PAD_SETUP_READY;
			pad_retry_time = now + 1;
		}
		break;
	case PAD_SETUP_PRESSURE_WAIT:
		if (pad.mode == 0x79)
			pad_setup = PAD_SETUP_READY;
		else if (request == PAD_RSTAT_COMPLETE) {
			pad_setup = PAD_SETUP_PRESSURE;
			pad_retry_time = now + 0.25;
		}
		break;
	case PAD_SETUP_READY:
		if (!rumble_ready)
			pad_setup = PAD_SETUP_ACTUATORS;
		else if (in_pressure.value && pad.mode != 0x79)
			pad_setup = PAD_SETUP_PRESSURE;
		break;
	}
}

void IN_PS2Poll(void)
{
	static const struct { unsigned short mask; int game_key; int ui_key; int pressure; } dpad[] = {
		{ PAD_UP, K_UPARROW, K_DPAD_UP, offsetof(struct padButtonStatus, up_p) },
		{ PAD_DOWN, K_DOWNARROW, K_DPAD_DOWN, offsetof(struct padButtonStatus, down_p) },
		{ PAD_LEFT, K_LEFTARROW, K_DPAD_LEFT, offsetof(struct padButtonStatus, left_p) },
		{ PAD_RIGHT, K_RIGHTARROW, K_DPAD_RIGHT, offsetof(struct padButtonStatus, right_p) }
	};
	unsigned short buttons = 0;
	unsigned short game_dpad, ui_dpad;
	int state, size = 0, i;
	qboolean pressure;

	if (!pad_open)
		return;
	state = padGetState(0, 0);
	if (state == PAD_STATE_STABLE || state == PAD_STATE_FINDCTP1) {
		size = padRead(0, 0, &pad);
		if (size < 4 || pad.ok)
			return;
		buttons = pad.btns ^ 0xffff;
		pad_analog = (pad.mode >> 4) == PAD_TYPE_DUALSHOCK;
		if (state == PAD_STATE_STABLE)
			IN_PS2Configure();
	} else if (state == PAD_STATE_DISCONN) {
		pad_setup = PAD_SETUP_START;
		pad_retry_time = 0;
		pad_analog = rumble_ready = false;
		rumble_stop_time = 0;
		rumble_duration = 0;
		rumble_pending = rumble_sent_valid = false;
		memset(actuators, 0, sizeof(actuators));
		memset(sent_actuators, 0, sizeof(sent_actuators));
	} else
		return;
	pressure = pad_analog && pad.mode == 0x79 && size >= offsetof(struct padButtonStatus, r2_p) + 1;
	for (i = 0; i < sizeof(KeyTable) / sizeof(KeyTable[0]); i++) {
		int value = pressure && KeyTable[i].pressure >= 0 ? ((const byte *)&pad)[KeyTable[i].pressure] : -1;
		Key_PressureEvent(KeyTable[i].key, (buttons & KeyTable[i].button) != 0, value);
	}
	game_dpad = key_dest == key_game ? buttons : 0;
	ui_dpad = key_dest == key_game ? 0 : buttons;
	for (i = 0; i < sizeof(dpad) / sizeof(dpad[0]); i++) {
		Key_PressureEvent(dpad[i].game_key, (game_dpad & dpad[i].mask) != 0,
			pressure ? ((const byte *)&pad)[dpad[i].pressure] : -1);
		if ((ui_dpad ^ old_ui_dpad) & dpad[i].mask)
			Key_Event(dpad[i].ui_key, (ui_dpad & dpad[i].mask) != 0);
	}
	old_ui_dpad = ui_dpad;
}

static void IN_PS2SendRumble(void)
{
	static const char stopped[6] = {0};
	if (!pad_open || !rumble_ready || !rumble_pending ||
		padGetState(0, 0) != PAD_STATE_STABLE ||
		padGetReqState(0, 0) == PAD_RSTAT_BUSY)
		return;
	if (rumble_sent_valid && memcmp(actuators, sent_actuators, sizeof(actuators)) &&
		(sent_actuators[0] || sent_actuators[1])) {
		if (padSetActDirect(0, 0, stopped) <= 0)
			return;
		memset(sent_actuators, 0, sizeof(sent_actuators));
	}
	if (!rumble_sent_valid || memcmp(actuators, sent_actuators, sizeof(actuators))) {
		if (padSetActDirect(0, 0, actuators) <= 0)
			return;
		memcpy(sent_actuators, actuators, sizeof(sent_actuators));
		rumble_sent_valid = true;
	}
	rumble_pending = false;
	rumble_stop_time = rumble_duration ? Sys_FloatTime() + rumble_duration / 1000.0 : 0;
	rumble_duration = 0;
}

void IN_PlatformCommands(void)
{
	if (!in_rumble.value || (rumble_stop_time && Sys_FloatTime() >= rumble_stop_time)) {
		if (actuators[0] || actuators[1]) {
			memset(actuators, 0, sizeof(actuators));
			rumble_pending = true;
		}
		rumble_stop_time = 0;
		rumble_duration = 0;
	}
	IN_PS2SendRumble();
}

void IN_PlatformRumble(unsigned short low_frequency, unsigned short high_frequency, unsigned int duration)
{
	if (!pad_open)
		return;
	actuators[0] = high_frequency != 0;
	actuators[1] = low_frequency / 257;
	rumble_pending = true;
	rumble_duration = duration;
	rumble_stop_time = 0;
	IN_PS2SendRumble();
}

static float IN_PS2Axis(unsigned char value)
{
	if (value < 127)
		return ((float)value - 127.0f) / 127.0f;
	if (value > 128)
		return ((float)value - 128.0f) / 127.0f;
	return 0.0f;
}

void IN_GetAnalogStick(in_analog_stick_id_t stick, in_analog_stick_t *value)
{
	value->x = value->y = 0;
	if (!pad_analog)
		return;
	if (stick == IN_STICK_LEFT) {
		value->x = IN_PS2Axis(pad.ljoy_h);
		value->y = -IN_PS2Axis(pad.ljoy_v);
	} else {
		value->x = IN_PS2Axis(pad.rjoy_h);
		value->y = -IN_PS2Axis(pad.rjoy_v);
	}
}

void IN_PlatformMove(usercmd_t *cmd)
{
	(void)cmd;
}
