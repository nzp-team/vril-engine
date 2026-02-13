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

// Generic keys
#define	K_ENTER			128
#define	K_ESCAPE		129

#define	K_UPARROW		130
#define	K_DOWNARROW		131
#define	K_LEFTARROW		132
#define	K_RIGHTARROW	133

// Platform specific keys

// Face buttons
#define K_BOTTOMFACE    134
#define K_LEFTFACE      135
#define K_TOPFACE       136
#define K_RIGHTFACE     137

// Triggers
#define K_LTRIGGER      138
#define K_RTRIGGER      139
#define K_ZLTRIGGER     140
#define K_ZRTRIGGER     141

// naievil -- this is for using the select button for a game key, which is important
#define K_START         142
#define K_SELECT 		143

// NSpire Extras
#define K_CTRL          153
#define K_SHIFT         155
#define K_VAR           156
#define K_TAB           157
#define K_DELETE        158

// PSP2
#define K_TOUCH         159

// Joystick buttons
#define	K_JOY1			184
#define	K_JOY2			185
#define	K_JOY3			186
#define	K_JOY4			187

#define K_PLUS          188
#define K_MINUS         189

#define K_SPACE         190

// aux keys are for multi-buttoned joysticks to generate so they can use
// the normal binding process
// leaving these here in-case future platforms
// may need to implement them
#define	K_AUX1			207
#define	K_AUX2			208
#define	K_AUX3			209
#define	K_AUX4			210
#define	K_AUX5			211
#define	K_AUX6			212
#define	K_AUX7			213
#define	K_AUX8			214
#define	K_AUX9			215
#define	K_AUX10			216
#define	K_AUX11			217
#define	K_AUX12			218
#define	K_AUX13			219
#define	K_AUX14			220
#define	K_AUX15			221
#define	K_AUX16			222
#define	K_AUX17			223
#define	K_AUX18			224
#define	K_AUX19			225
#define	K_AUX20			226
#define	K_AUX21			227
#define	K_AUX22			228
#define	K_AUX23			229
#define	K_AUX24			230
#define	K_AUX25			231
#define	K_AUX26			232
#define	K_AUX27			233
#define	K_AUX28			234
#define	K_AUX29			235
#define	K_AUX30			236
#define	K_AUX31			237
#define	K_AUX32			238

typedef enum {key_game, key_console, key_message, key_menu, key_menu_pause} keydest_t;

extern keydest_t	    key_dest;
extern char             *keybindings[256];
extern char             *dtbindings[256];
extern	int		        key_repeats[256];
extern	int		        key_count;			// incremented every key event
extern	int		        key_lastpress;
extern qboolean	        keydown[256];

void Key_Event (int key, qboolean down);
void Key_Init (void);
void Key_WriteBindings (FILE *f);
void Key_WriteDTBindings (FILE *f);
void Key_SetBinding (int keynum, char *binding);
void Key_ClearStates (void);
void Key_SendText(char *text);
