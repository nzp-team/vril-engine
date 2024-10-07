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

//
// these are the key numbers that should be passed to Key_Event
//

enum key_id_e
{
	K_TAB = 9,
	K_ENTER = 13,
	K_ESCAPE = 27,
	K_SPACE = 32,

	// normal keys should be passed as lowercased ascii

	K_BACKSPACE = 127,
	K_UPARROW,
	K_DOWNARROW,
	K_LEFTARROW,
	K_RIGHTARROW,

	K_ALT,
	K_CTRL,
	K_SHIFT,
	K_F1,
	K_F2,
	K_F3,
	K_F4,
	K_F5,
	K_F6,
	K_F7,
	K_F8,
	K_F9,
	K_F10,
	K_F11,
	K_F12,

	K_INS,
	K_DEL,
	K_PGDN,
	K_PGUP,
	K_HOME,
	K_END,
	K_LSHIFT,
	K_RSHIFT,
	K_NUMLOCK,
	K_MENU,
	K_LMETA,
	K_RMETA,
	K_CAPSLOCK,

	K_PAUSE,

	//
	// mouse buttons generate virtual keys
	//
	K_MOUSE1,
	K_MOUSE2,
	K_MOUSE3,

	//
	// aux keys are for multi-buttoned joysticks to generate so they can use
	// the normal binding process
	//
	K_JOY0,
	K_JOY1,
	K_JOY2,
	K_JOY3,
	K_JOY4,
	K_JOY5,
	K_JOY6,
	K_JOY7,
	K_JOY8,
	K_JOY9,
	K_JOY10,
	K_JOY11,
	K_JOY12,
	K_JOY13,
	K_JOY14,
	K_JOY15,
	K_JOY16,
	K_JOY17,
	K_JOY18,
	K_JOY19,
	K_JOY20,
	K_JOY21,
	K_JOY22,
	K_JOY23,
	K_JOY24,
	K_JOY25,
	K_JOY26,
	K_JOY27,
	K_JOY28,
	K_JOY29,
	K_JOY30,
	K_JOY31,
	K_JOY32,

	// JACK: Intellimouse(c) Mouse Wheel Support

	K_MWHEELUP,
	K_MWHEELDOWN,
	
	K_SHAKE,

	KEY_COUNT = 256
};

typedef enum key_id_e key_id_t;



typedef enum {key_game, key_console, key_message, key_menu, key_menu_pause} keydest_t;

extern keydest_t	key_dest;
extern char *keybindings[KEY_COUNT];
extern char *dtbindings[256];
extern	int		key_count;			// incremented every key event
extern	int		key_lastpress;
extern qboolean keydown[KEY_COUNT];

void Key_Event (key_id_t key, qboolean down);
void Key_Init (void);

struct file_s;

void Key_WriteBindings (FILE *f);
void Key_WriteDTBindings (FILE *f);
void Key_SetBinding (key_id_t keynum, char *binding);
void Key_ClearStates (void);

