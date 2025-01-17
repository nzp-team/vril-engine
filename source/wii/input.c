/*
Quake GameCube port.
Copyright (C) 2007 Peter Mackay
Copyright (C) 2008 Eluan Miranda
Copyright (C) 2015 Fabio Olimpieri

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

// ELUTODO: do something about lookspring and lookstrafe
// ELUTODO: keys to: nunchuk turn and nunchuk look up/down?
// ELUTODO: osk doesn't work if client disconnected

#include "../quakedef.h"

cvar_t	nunchuk_stick_as_arrows = {"nunchuk_stick_as_arrows","0"};
cvar_t  rumble = {"rumble","1"};

// pass these values to whatever subsystem wants it
float in_pitchangle;
float in_yawangle;
float in_rollangle;

#include <ogc/pad.h>
#include <wiiuse/wpad.h>
#include <wiikeyboard/keyboard.h>
#include "input_wiimote.h"

#define FORCE_KEY_BINDINGS 0

u32 wiimote_ir_res_x;
u32 wiimote_ir_res_y;

// wiimote info
u32 wpad_previous_keys = 0xf;
u32 wpad_keys = 0xf;

ir_t pointer;
orient_t orientation;
expansion_t expansion;
nunchuk_t nunchuk; 
gforce_t gforce;//Nunchuk shake

bool wiimote_connected = true;
bool nunchuk_connected = false;
bool classic_connected = false;
bool keyboard_connected = false;

typedef enum  {LEFT, CENTER_X, RIGHT} stick_x_st_t;
typedef enum   {UP, CENTER_Y, DOWN} stick_y_st_t;

stick_x_st_t stick_x_st = CENTER_X;
stick_y_st_t stick_y_st = CENTER_Y;

u16 pad_previous_keys = 0xf;
u16 pad_keys = 0xf;

static float clamp(float value, float minimum, float maximum)
{
	if (value > maximum)
	{
		return maximum;
	}
	else if (value < minimum)
	{
		return minimum;
	}
	else
	{
		return value;
	}
}
qboolean aimsnap = false;
static void apply_dead_zone(float* x, float* y, float dead_zone)
{
	if (aimsnap == true)
		dead_zone = 0.15f;
	
	if ((fabsf(*x) >= dead_zone) || (fabsf(*y) >= dead_zone))
	{
		// Nothing to do.
	}
	else
	{
		// Clamp to the dead zone.
		*x = 0.0f;
		*y = 0.0f;
	}
}

static s8 WPAD_StickX(u8 which)
{
	float mag = 0.0;
	float ang = 0.0;

	switch (expansion.type)
	{
		case WPAD_EXP_NUNCHUK:
		case WPAD_EXP_GUITARHERO3:
			if (which == 0)
			{
				mag = expansion.nunchuk.js.mag;
				ang = expansion.nunchuk.js.ang;
			}
			break;

		case WPAD_EXP_CLASSIC:
			if (which == 0)
			{
				mag = expansion.classic.ljs.mag;
				ang = expansion.classic.ljs.ang;
			}
			else
			{
				mag = expansion.classic.rjs.mag;
				ang = expansion.classic.rjs.ang;
			}
			break;

		default:
			break;
	}

	/* calculate X value (angle needs to be converted into radians) */
	if (mag > 1.0) mag = 1.0;
	else if (mag < -1.0) mag = -1.0;
	double val = mag * sin(M_PI * ang/180.0f);

	return (s8)(val * 128.0f);
}

static s8 WPAD_StickY(u8 which)
{
	float mag = 0.0;
	float ang = 0.0;

	switch (expansion.type)
	{
		case WPAD_EXP_NUNCHUK:
		case WPAD_EXP_GUITARHERO3:
			if (which == 0)
			{
				mag = expansion.nunchuk.js.mag;
				ang = expansion.nunchuk.js.ang;
			}
			break;

		case WPAD_EXP_CLASSIC:
			if (which == 0)
			{
				mag = expansion.classic.ljs.mag;
				ang = expansion.classic.ljs.ang;
			}
			else
			{
				mag = expansion.classic.rjs.mag;
				ang = expansion.classic.rjs.ang;
			}
			break;

		default:
			break;
	}

	/* calculate X value (angle need to be converted into radian) */
	if (mag > 1.0) mag = 1.0;
	else if (mag < -1.0) mag = -1.0;
	double val = mag * cos(M_PI * ang/180.0f);

	return (s8)(val * 128.0f);
}

void IN_Init (void)
{
	in_pitchangle = .0f;
	in_yawangle = .0f;
	in_rollangle = .0f;

	WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS_ACC_IR);
	WPAD_SetVRes(WPAD_CHAN_ALL, wiimote_ir_res_x, wiimote_ir_res_y);
	
	Cvar_RegisterVariable (&nunchuk_stick_as_arrows);
	Cvar_RegisterVariable (&rumble);
}

void IN_Shutdown (void)
{
}

void IN_Commands (void)
{
	// Fetch the pad state.
	PAD_ScanPads();
	WPAD_ScanPads();

	// Manages the nunchunk or classic controller connection
	// and assigns the pressed buttons to wpad_keys (wiimote/classic) and pad_keys (gamecube)
	
	u32 exp_type;
	if ( WPAD_Probe(WPAD_CHAN_0, &exp_type) != 0 ) {
		Con_Printf ("No controller detected!\n");
		exp_type = WPAD_EXP_NONE;
	}

	if(exp_type == WPAD_EXP_NUNCHUK) {
		//Con_Printf ("Nunchuk detected..\n");
		if(!nunchuk_connected) {
			wpad_previous_keys = 0xf;
		}

		nunchuk_connected = true;
		classic_connected = false;
		wpad_keys = WPAD_ButtonsHeld(WPAD_CHAN_0);
		pad_keys = 0xf;
		pad_previous_keys = 0xf;
	} else if(exp_type == WPAD_EXP_CLASSIC) {
		//Con_Printf ("Classic controller detected..\n");
		if(!classic_connected) {
			wpad_previous_keys = 0xf;
		}

		nunchuk_connected = false;
		classic_connected = true;
		wpad_keys = WPAD_ButtonsHeld(WPAD_CHAN_0);
		pad_keys = 0xf;
		pad_previous_keys = 0xf;
	} else {
		//Here neither the classic controller nor the nuncunk are connected
		if(classic_connected || nunchuk_connected) {
			wpad_previous_keys = 0xf;
		}

		nunchuk_connected = false;
		classic_connected = false;
		pad_keys = PAD_ButtonsHeld(PAD_CHAN0);
		wpad_keys = WPAD_ButtonsHeld(WPAD_CHAN_0);
	}
	
	WPAD_IR(WPAD_CHAN_0, &pointer);
	WPAD_Orientation(WPAD_CHAN_0, &orientation);
	WPAD_Expansion(WPAD_CHAN_0, &expansion);
	WPAD_GForce(WPAD_CHAN_0, &gforce); //Shake to reload

	if(classic_connected) {
		//Send the wireless classic controller buttons events
		if ((wpad_previous_keys & WPAD_CLASSIC_BUTTON_LEFT) != (wpad_keys & WPAD_CLASSIC_BUTTON_LEFT)) {
			// Send a press event.
			Key_Event(K_LEFTARROW, ((wpad_keys & WPAD_CLASSIC_BUTTON_LEFT) == WPAD_CLASSIC_BUTTON_LEFT));
		}

		if ((wpad_previous_keys & WPAD_CLASSIC_BUTTON_RIGHT) != (wpad_keys & WPAD_CLASSIC_BUTTON_RIGHT)) {
			// Send a press event.
			Key_Event(K_RIGHTARROW, ((wpad_keys & WPAD_CLASSIC_BUTTON_RIGHT) == WPAD_CLASSIC_BUTTON_RIGHT));
		}

		if ((wpad_previous_keys & WPAD_CLASSIC_BUTTON_DOWN) != (wpad_keys & WPAD_CLASSIC_BUTTON_DOWN)) {
			// Send a press event.
			Key_Event(K_DOWNARROW, ((wpad_keys & WPAD_CLASSIC_BUTTON_DOWN) == WPAD_CLASSIC_BUTTON_DOWN));
		}

		if ((wpad_previous_keys & WPAD_CLASSIC_BUTTON_UP) != (wpad_keys & WPAD_CLASSIC_BUTTON_UP)) {
			// Send a press event.
			Key_Event(K_UPARROW, ((wpad_keys & WPAD_CLASSIC_BUTTON_UP) == WPAD_CLASSIC_BUTTON_UP));
		}

		if ((wpad_previous_keys & WPAD_CLASSIC_BUTTON_A) != (wpad_keys & WPAD_CLASSIC_BUTTON_A)) {
			// Send a press event.
			Key_Event(K_JOY9, ((wpad_keys & WPAD_CLASSIC_BUTTON_A) == WPAD_CLASSIC_BUTTON_A));
		}

		if ((wpad_previous_keys & WPAD_CLASSIC_BUTTON_B) != (wpad_keys & WPAD_CLASSIC_BUTTON_B)) {
			// Send a press event.
			Key_Event(K_JOY10, ((wpad_keys & WPAD_CLASSIC_BUTTON_B) == WPAD_CLASSIC_BUTTON_B));
		}

		if ((wpad_previous_keys & WPAD_CLASSIC_BUTTON_X) != (wpad_keys & WPAD_CLASSIC_BUTTON_X)) {
			// Send a press event.
			Key_Event(K_JOY11, ((wpad_keys & WPAD_CLASSIC_BUTTON_X) == WPAD_CLASSIC_BUTTON_X));
		}

		if ((wpad_previous_keys & WPAD_CLASSIC_BUTTON_Y) != (wpad_keys & WPAD_CLASSIC_BUTTON_Y)) {
			// Send a press event.
			Key_Event(K_JOY12, ((wpad_keys & WPAD_CLASSIC_BUTTON_Y) == WPAD_CLASSIC_BUTTON_Y));
		}
		
		if ((wpad_previous_keys & WPAD_CLASSIC_BUTTON_FULL_L) != (wpad_keys & WPAD_CLASSIC_BUTTON_FULL_L)) {
			// Send a press event.
			Key_Event(K_JOY13, ((wpad_keys & WPAD_CLASSIC_BUTTON_FULL_L) == WPAD_CLASSIC_BUTTON_FULL_L));
		}

		if ((wpad_previous_keys & WPAD_CLASSIC_BUTTON_FULL_R) != (wpad_keys & WPAD_CLASSIC_BUTTON_FULL_R)) {
			// Send a press event.
			Key_Event(K_JOY14, ((wpad_keys & WPAD_CLASSIC_BUTTON_FULL_R) == WPAD_CLASSIC_BUTTON_FULL_R));
		}
		
		if ((wpad_previous_keys & WPAD_CLASSIC_BUTTON_ZL) != (wpad_keys & WPAD_CLASSIC_BUTTON_ZL)) {
			// Send a press event.
			Key_Event(K_JOY15, ((wpad_keys & WPAD_CLASSIC_BUTTON_ZL) == WPAD_CLASSIC_BUTTON_ZL));
		}

		if ((wpad_previous_keys & WPAD_CLASSIC_BUTTON_ZR) != (wpad_keys & WPAD_CLASSIC_BUTTON_ZR)) {
			// Send a press event.
			Key_Event(K_JOY16, ((wpad_keys & WPAD_CLASSIC_BUTTON_ZR) == WPAD_CLASSIC_BUTTON_ZR));
		}
		
		if ((wpad_previous_keys & WPAD_CLASSIC_BUTTON_MINUS) != (wpad_keys & WPAD_CLASSIC_BUTTON_MINUS)) {
			// Send a press event.
			Key_Event(K_JOY17, ((wpad_keys & WPAD_CLASSIC_BUTTON_MINUS) == WPAD_CLASSIC_BUTTON_MINUS));
		}
		
		if ((wpad_previous_keys & WPAD_CLASSIC_BUTTON_PLUS) != (wpad_keys & WPAD_CLASSIC_BUTTON_PLUS)) {
			// Send a press event.
			Key_Event(K_JOY18, ((wpad_keys & WPAD_CLASSIC_BUTTON_PLUS) == WPAD_CLASSIC_BUTTON_PLUS));
		}
		
	} else {
		//Send the wiimote button events if the classic controller is not connected	
		if ((wpad_previous_keys & WPAD_BUTTON_LEFT) != (wpad_keys & WPAD_BUTTON_LEFT)) {
			// Send a press event.
			Key_Event(K_LEFTARROW, ((wpad_keys & WPAD_BUTTON_LEFT) == WPAD_BUTTON_LEFT));
		}

		if ((wpad_previous_keys & WPAD_BUTTON_RIGHT) != (wpad_keys & WPAD_BUTTON_RIGHT)) {
			// Send a press event.
			Key_Event(K_RIGHTARROW, ((wpad_keys & WPAD_BUTTON_RIGHT) == WPAD_BUTTON_RIGHT));
		}

		if ((wpad_previous_keys & WPAD_BUTTON_DOWN) != (wpad_keys & WPAD_BUTTON_DOWN)) {
			// Send a press event.
			Key_Event(K_DOWNARROW, ((wpad_keys & WPAD_BUTTON_DOWN) == WPAD_BUTTON_DOWN));
		}

		if ((wpad_previous_keys & WPAD_BUTTON_UP) != (wpad_keys & WPAD_BUTTON_UP)) {
			// Send a press event.
			Key_Event(K_UPARROW, ((wpad_keys & WPAD_BUTTON_UP) == WPAD_BUTTON_UP));
		}
		
		if ((wpad_previous_keys & WPAD_BUTTON_A) != (wpad_keys & WPAD_BUTTON_A)) {
			// Send a press event.
			Key_Event(K_JOY0, ((wpad_keys & WPAD_BUTTON_A) == WPAD_BUTTON_A));
		}

		if ((wpad_previous_keys & WPAD_BUTTON_B) != (wpad_keys & WPAD_BUTTON_B)) {
			// Send a press event.
			Key_Event(K_JOY1, ((wpad_keys & WPAD_BUTTON_B) == WPAD_BUTTON_B));
		}

		if ((wpad_previous_keys & WPAD_BUTTON_1) != (wpad_keys & WPAD_BUTTON_1)) {
			// Send a press event.
			Key_Event(K_JOY2, ((wpad_keys & WPAD_BUTTON_1) == WPAD_BUTTON_1));
		}

		if ((wpad_previous_keys & WPAD_BUTTON_2) != (wpad_keys & WPAD_BUTTON_2)) {
			// Send a press event.
			Key_Event(K_JOY3, ((wpad_keys & WPAD_BUTTON_2) == WPAD_BUTTON_2));
		}
		
		if ((wpad_previous_keys & WPAD_BUTTON_PLUS) != (wpad_keys & WPAD_BUTTON_PLUS)) {
			// Send a press event.
			Key_Event(K_JOY5, ((wpad_keys & WPAD_BUTTON_PLUS) == WPAD_BUTTON_PLUS));
		}
		// select:
		if ((wpad_previous_keys & WPAD_BUTTON_MINUS) != (wpad_keys & WPAD_BUTTON_MINUS)) {
			// Send a press event.
			Key_Event(K_JOY17, ((wpad_keys & WPAD_BUTTON_MINUS) == WPAD_BUTTON_MINUS));
		}
		
		if(nunchuk_connected) {
			//Send nunchunk button events	
			if ((wpad_previous_keys & WPAD_NUNCHUK_BUTTON_Z) != (wpad_keys & WPAD_NUNCHUK_BUTTON_Z)) {
				// Send a press event.
				Key_Event(K_JOY7, ((wpad_keys & WPAD_NUNCHUK_BUTTON_Z) == WPAD_NUNCHUK_BUTTON_Z));
			}
			
			if ((wpad_previous_keys & WPAD_NUNCHUK_BUTTON_C) != (wpad_keys & WPAD_NUNCHUK_BUTTON_C)) {
				// Send a press event.
				Key_Event(K_JOY8, ((wpad_keys & WPAD_NUNCHUK_BUTTON_C) == WPAD_NUNCHUK_BUTTON_C));
			}
			
			//Con_Printf("oriy:%f, acy:%f\n", expansion.nunchuk.orient.pitch, expansion.nunchuk.gforce.y);
			
			if(expansion.nunchuk.orient.pitch <= 15 && expansion.nunchuk.gforce.y > 0.25) {
				Key_Event(K_SHAKE, true);
			} else {
				Key_Event(K_SHAKE, false);
			}
			
			if(nunchuk_stick_as_arrows.value) {
				//Emulation of the wimote arrows with the nunchuk stick
				
				const s8 nunchuk_stick_x = WPAD_StickX(0);
				const s8 nunchuk_stick_y = WPAD_StickY(0);
				if (nunchuk_stick_x > 10) {
					switch (stick_x_st) {
						
						case CENTER_X : Key_Event(K_RIGHTARROW, true);break;
						default : break;
					}
					stick_x_st = RIGHT;
				} else if (nunchuk_stick_x < -10)  {
					switch (stick_x_st) {
						case CENTER_X : Key_Event(K_LEFTARROW, true);break;
						default: break;
					}
					stick_x_st = LEFT;
				} else {
					switch (stick_x_st) {
						case LEFT :	Key_Event(K_LEFTARROW, false);break;							
						case RIGHT: Key_Event(K_RIGHTARROW, false);break;
						default: break;
					}
					stick_x_st = CENTER_X;
				}
				
				if (nunchuk_stick_y > 10) {
					switch (stick_y_st) {
						case CENTER_Y : Key_Event(K_UPARROW, true); break;
						default: break;	
					}
					stick_y_st = UP;
				} else if (nunchuk_stick_y < -10) {
					switch (stick_y_st) {	
						case CENTER_Y : Key_Event(K_DOWNARROW, true);break;
						default: break;
					}
					stick_y_st = DOWN;
				} else {
					switch (stick_y_st) {
						case DOWN :	Key_Event(K_DOWNARROW, false);break;
						case UP: Key_Event(K_UPARROW, false);break;
						default: break;
					}
					stick_y_st = CENTER_Y;
				}	
			}	
		}
	}

	if(!nunchuk_connected && !classic_connected) {
		//Send the gamecube controller button events in the case neither the nunchuk nor the classic controller is connected
		//Con_Printf ("gamecube dedected\n");	
		if ((pad_previous_keys & PAD_BUTTON_LEFT) != (pad_keys & PAD_BUTTON_LEFT)) {
			// Send a press event.
			Key_Event(K_LEFTARROW, ((pad_keys & PAD_BUTTON_LEFT) == PAD_BUTTON_LEFT));
		}

		if ((pad_previous_keys & PAD_BUTTON_RIGHT) != (pad_keys & PAD_BUTTON_RIGHT)) {
			// Send a press event.
			Key_Event(K_RIGHTARROW, ((pad_keys & PAD_BUTTON_RIGHT) == PAD_BUTTON_RIGHT));
		}

		if ((pad_previous_keys & PAD_BUTTON_DOWN) != (pad_keys & PAD_BUTTON_DOWN)) {
			// Send a press event.
			Key_Event(K_DOWNARROW, ((pad_keys & PAD_BUTTON_DOWN) == PAD_BUTTON_DOWN));
		}
		if ((pad_previous_keys & PAD_BUTTON_UP) != (pad_keys & PAD_BUTTON_UP)) {
			// Send a press event.
			Key_Event(K_UPARROW, ((pad_keys & PAD_BUTTON_UP) == PAD_BUTTON_UP));
		}

		if ((pad_previous_keys & PAD_BUTTON_A) != (pad_keys & PAD_BUTTON_A)) {
			// Send a press event.
			Key_Event(K_JOY20, ((pad_keys & PAD_BUTTON_A) == PAD_BUTTON_A));
		}

		if ((pad_previous_keys & PAD_BUTTON_B) != (pad_keys & PAD_BUTTON_B)) {
			// Send a press event.
			Key_Event(K_JOY21, ((pad_keys & PAD_BUTTON_B) == PAD_BUTTON_B));
		}

		if ((pad_previous_keys & PAD_BUTTON_X) != (pad_keys & PAD_BUTTON_X)) {
			// Send a press event.
			Key_Event(K_JOY22, ((pad_keys & PAD_BUTTON_X) == PAD_BUTTON_X));
		}

		if ((pad_previous_keys & PAD_BUTTON_Y) != (pad_keys & PAD_BUTTON_Y)) {
			// Send a press event.
			Key_Event(K_JOY23, ((pad_keys & PAD_BUTTON_Y) == PAD_BUTTON_Y));
		}
		
		if ((pad_previous_keys & PAD_TRIGGER_Z) != (pad_keys & PAD_TRIGGER_Z)) {
			// Send a press event.
			Key_Event(K_JOY24, ((pad_keys & PAD_TRIGGER_Z) == PAD_TRIGGER_Z));
		}
		
		if ((pad_previous_keys & PAD_TRIGGER_R) != (pad_keys & PAD_TRIGGER_R)) {
			// Send a press event.
			Key_Event(K_JOY25, ((pad_keys & PAD_TRIGGER_R) == PAD_TRIGGER_R));
		}

		if ((pad_previous_keys & PAD_TRIGGER_L) != (pad_keys & PAD_TRIGGER_L)) {
			// Send a press event.
			Key_Event(K_JOY26, ((pad_keys & PAD_TRIGGER_L) == PAD_TRIGGER_L));
		}

		if ((pad_previous_keys & PAD_BUTTON_START) != (pad_keys & PAD_BUTTON_START)) {
			// Send a press event.
			Key_Event(K_ESCAPE, ((pad_keys & PAD_BUTTON_START) == PAD_BUTTON_START));
		}
		pad_previous_keys = pad_keys;
	}
	wpad_previous_keys = wpad_keys;
}

extern float crosshair_opacity;
float centerdrift_offset_yaw, centerdrift_offset_pitch;
extern int zoom_snap;
extern kbutton_t in_forward, in_left, in_right;
extern qboolean croshhairmoving;
extern cvar_t ads_center;
extern cvar_t sniper_center;
// Some things here rely upon IN_Move always being called after IN_Commands on the same frame
void IN_Move (usercmd_t *cmd)
{
	const float dead_zone = 0.3f;

	float x1;
	float y1;
	float x2;
	float y2;

	// TODO: sensor bar position correct? aspect ratio correctly set? etc...
	// In "pointer" variable there are the IR values
	int last_wiimote_ir_x = pointer.x;
	int last_wiimote_ir_y = pointer.y;
	int wiimote_ir_x = 0, wiimote_ir_y = 0;


	if (pointer.x < 1 || (unsigned int)pointer.x > pointer.vres[0] - 1)
		wiimote_ir_x = last_wiimote_ir_x;
	else
		wiimote_ir_x = pointer.x;
	if (pointer.y < 1 || (unsigned int)pointer.y > pointer.vres[1] - 1)
		wiimote_ir_y = last_wiimote_ir_y;
	else
		wiimote_ir_y = pointer.y;

	last_wiimote_ir_x = wiimote_ir_x;
	last_wiimote_ir_y = wiimote_ir_y;

// Movement management of nunchuk stick (x1/y1) and of IR (x2/y2) if the nunchuk is connected
	if(nunchuk_connected && !nunchuk_stick_as_arrows.value)
	{
		const s8 nunchuk_stick_x = WPAD_StickX(0);
		const s8 nunchuk_stick_y = WPAD_StickY(0);

		x1 = clamp(((float)nunchuk_stick_x / 128.0f) * 1.5, -1.0f, 1.0f);
		y1 = clamp(((float)nunchuk_stick_y / (128.0f)) * 1.5, -1.0f, 1.0f);
			
		x2 = clamp((float)wiimote_ir_x / (pointer.vres[0] / 2.0f) - 1.0f, -1.0f, 1.0f);
		y2 = clamp((float)wiimote_ir_y / (pointer.vres[1] / 2.0f) - 1.0f, -1.0f, 1.0f);
		// Move the cross position
		
		// sB if sniper scope we want to aim in da middle
		// Unless I can find a way to make the scope image 
		// move with pointer>>??
		// seems impossible in my mind
		
		if (aimsnap == true || (cl.stats[STAT_ZOOM] == 1 && ads_center.value) || (cl.stats[STAT_ZOOM] == 2 && sniper_center.value)) {
			Cvar_SetValue("cl_crossx", vid.width / 2);
			Cvar_SetValue("cl_crossy", vid.height / 2);
		} else {
			Cvar_SetValue("cl_crossx", vid.width / 2 * x2);
			Cvar_SetValue("cl_crossy", vid.height / 2 * y2);
			
			//Con_Printf ("crossx: %f crossy %f\n", scr_vrect.width / 2 * x2, scr_vrect.height / 2 * y2);
		}
	}
		
// Movement management of 2 classic controller sticks (x1/y1) and (y1/y2) if the cc is connected

	else if(classic_connected)
	{
		const s8 left_stick_x = WPAD_StickX(0);
		const s8 left_stick_y = WPAD_StickY(0);

		const s8 right_stick_x = WPAD_StickX(1);
		const s8 right_stick_y = WPAD_StickY(1);

		x1 = clamp(((float)left_stick_x / 128.0f) * 1.5, -1.0f, 1.0f);
		y1 = clamp(((float)left_stick_y / (128.0f)) * 1.5, -1.0f, 1.0f);

		x2 = clamp(((float)right_stick_x / 128.0f) * 1.5, -1.0f, 1.0f);
		Cvar_SetValue("cl_crossx", /*(in_vlock.state & 1) ? */scr_vrect.width / 2 * x2/* : 0*/);
		y2 = clamp(((float)right_stick_y / (-128.0f)) * 1.5, -1.0f, 1.0f);
		Cvar_SetValue("cl_crossy", /*(in_vlock.state & 1) ? */scr_vrect.height / 2 * y2/* : 0*/);
	}
// Movement management of 2 gamecube controller sticks (x1/y1) e  (y1/y2) if neither the cc nor nn is connected
	else
	{
		const s8 stick_x = PAD_StickX(0);
		const s8 stick_y = PAD_StickY(0);

		const s8 sub_stick_x = PAD_SubStickX(0);
		const s8 sub_stick_y = PAD_SubStickY(0);

		x1 = clamp(stick_x / 90.0f, -1.0f, 1.0f);
		y1 = clamp(stick_y / 90.0f, -1.0f, 1.0f);

		x2 = clamp(sub_stick_x / 80.0f, -1.0f, 1.0f);
		Cvar_SetValue("cl_crossx", /*(in_vlock.state & 1) ? */scr_vrect.width / 2 * x2/* : 0*/);

		y2 = clamp(sub_stick_y / -80.0f, -1.0f, 1.0f);
		Cvar_SetValue("cl_crossy", /*(in_vlock.state & 1) ? */scr_vrect.height / 2 * y2/* : 0*/);
	}
	
	//non-linear sensitivity based on how
	//far the IR pointer is from the 
	//center of the screen.
	
	/*
	if (cl.stats[STAT_ZOOM] == 1 || cl.stats[STAT_ZOOM] == 2) {
		centerdrift_offset_yaw = 1; //yaw
		centerdrift_offset_pitch = 1; //pitch
	} else {*/
		centerdrift_offset_yaw = fabsf(x2); //yaw
		centerdrift_offset_pitch = fabsf(y2); //pitch
	//}
	
	// Apply the dead zone.
	apply_dead_zone(&x1, &y1, dead_zone);
	apply_dead_zone(&x2, &y2, dead_zone);

	// Don't let the pitch drift back to centre if mouse look is on or the right stick is being used.
	//if ((in_mlook.state & 1) || (fabsf(y2) >= dead_zone)) Disabled, always very convenient with a gamepad or wiimote
	{
		V_StopPitchDrift();
	}

	// Lock view?
	if (in_vlock.state & 1)
	{
		x2 = 0;
		y2 = 0;
	}

	float yaw_rate;
	float pitch_rate;

	yaw_rate = x2;
	pitch_rate = y2;
	
	cl_backspeed = cl_forwardspeed = cl_sidespeed = sv_player->v.maxspeed;
	cl_sidespeed *= 0.8;
	cl_backspeed *= 0.7;

	// Move using the main stick.
	//Send the stick movement commands
	if (!(nunchuk_stick_as_arrows.value&&nunchuk_connected))
	{
		cmd->sidemove += cl_sidespeed * x1;
		if (y1>0) cmd->forwardmove += cl_forwardspeed * y1; /* TODO: use cl_backspeed when going backwards? */
			else cmd->forwardmove += cl_backspeed * y1; 
			
		if (cmd->forwardmove == 0.0f && cmd->sidemove == 0.0f)
			croshhairmoving = false;
		else 
			croshhairmoving = true;
		/*
		//if the nunchuk c button is pressed it speeds up
		if (in_speed.state & 1)
		{
			if (cl_forwardspeed > 200)
			{
				cmd->forwardmove /= cl_movespeedkey.value;
				cmd->sidemove /= cl_movespeedkey.value;
			}
			else
			{
				cmd->forwardmove *= cl_movespeedkey.value;
				cmd->sidemove *= cl_movespeedkey.value; // TODO: always seem to be at the max and I'm too sleepy now to figure out why
			}
		}
		*/
	}

	// TODO: Use yawspeed and pitchspeed

	// Adjust the yaw.
	const float turn_rate = sensitivity.value * 40.0;
	
	float speed = 1;
	
	// cut look speed in half when facing enemy, unless mag is empty
	if ((in_aimassist.value) && (sv_player->v.facingenemy == 1) && cl.stats[STAT_CURRENTMAG] > 0)
		speed = 0.6;
	else
		speed = 1;
		
	// additionally, slice look speed when ADS/scopes
	if (cl.stats[STAT_ZOOM] == 1)
		speed = 0.5;
	else if (cl.stats[STAT_ZOOM] == 2)
		speed = 0.25;
	else
		speed = 1;
	
	// How fast to yaw?
	float yaw_offset;
	yaw_offset = ((turn_rate * yaw_rate * host_frametime) * speed) * centerdrift_offset_yaw;
	cl.viewangles[YAW] -= yaw_offset;
	
	// How fast to pitch?
	float pitch_offset;
	pitch_offset = ((turn_rate * pitch_rate * host_frametime) * speed) * centerdrift_offset_pitch;

	// Do the pitch.
	const bool	invert_pitch = m_pitch.value < 0;
	if (invert_pitch)
	{
		cl.viewangles[PITCH] -= pitch_offset;
	}
	else
	{
		cl.viewangles[PITCH] += pitch_offset;
	}

	// Don't look too far up or down.
	if (cl.viewangles[PITCH] > 60.0f)
	{
		cl.viewangles[PITCH] = 60.0f;
	}
	else if (cl.viewangles[PITCH] < -50.0f)
	{
		cl.viewangles[PITCH] = -50.0f;
	}
	
	if (wiimote_connected && nunchuk_connected && !nunchuk_stick_as_arrows.value)
	{
		in_pitchangle = orientation.pitch;
		in_yawangle = orientation.yaw;
		in_rollangle = orientation.roll;
	}
	else
	{
		in_pitchangle = .0f;
		in_yawangle = .0f;
		in_rollangle = .0f;
	}
	
}

extern double time_wpad_off;
extern int rumble_on;
void Wiimote_Rumble (int low_frequency, int high_frequency, int duration) {
	if (!rumble.value) return;
	double rumble_time;
	rumble_time = duration / 1000.0;
	//low frequency and high frequency not used for wiimote. read them anyways
	
	//it switches rumble on for rumble_time milliseconds  
	WPAD_Rumble(0, true);
	rumble_on=1;
	time_wpad_off = Sys_FloatTime() + rumble_time;
}
