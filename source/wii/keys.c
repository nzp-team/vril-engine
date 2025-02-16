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
#include "../quakedef.h"
#include <wiiuse/wpad.h>

/*

key up events are sent even if in console mode

*/


#define		MAXCMDLINE	256
char	key_lines[64][MAXCMDLINE];
int		key_linepos;
int		shift_down=false;
int		key_lastpress;

int		edit_line=0;
int		history_line=0;

keydest_t	key_dest;

int		key_count;			// incremented every key event

char	*keybindings[256];
char	*dtbindings[256];
qboolean	consolekeys[256];	// if true, can't be rebound while in console
qboolean	menubound[256];	// if true, can't be rebound while in menu
int		keyshift[256];		// key to map to if shift held down in console
int		key_repeats[256];	// if > 1, it is autorepeating
qboolean	keydown[256];

qboolean	keydown[KEY_COUNT];

extern bool nunchuk_connected;
extern u32 wpad_keys;

typedef struct
{
	char		*name;
	key_id_t	keynum;
} keyname_t;

// Are we inside the on-screen keyboard?
qboolean in_osk = false;

// \0 means not mapped...
// 5 * 15
/*
char osk_normal[75] =
{
	'\'', '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', ']', K_BACKSPACE,
	0, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', 0, '[', K_ENTER, K_ENTER,
	0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 0, '~', '/', K_ENTER, K_ENTER,
	0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', ';', K_ENTER, K_ENTER, K_ENTER,
	0 , 0, K_SPACE, K_SPACE, K_SPACE, K_SPACE, K_SPACE, K_SPACE, K_SPACE, K_SPACE, K_SPACE, K_SPACE, K_SPACE, 0, 0
};

char osk_shifted[75] =
{
	'\"', '!', '@', '#', '$', '%', 0, '&', '*', '(', ')', '_', '+', '}', K_BACKSPACE,
	0, 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', 0, '{', K_ENTER, K_ENTER,
	0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', 0, '^', '?', K_ENTER, K_ENTER,
	0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', ':', K_ENTER, K_ENTER, K_ENTER,
	0 , 0, K_SPACE, K_SPACE, K_SPACE, K_SPACE, K_SPACE, K_SPACE, K_SPACE, K_SPACE, K_SPACE, K_SPACE, K_SPACE, 0, 0
};

char *osk_set;
int osk_selected;
int osk_last_selected;
int osk_coords[2];

float osk_last_press_time = 0.0f;
extern cvar_t osk_repeat_delay;
*/
//int last_irx, last_iry;
/*
static float clampLine(float value, float minimum, float maximum)
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
*/
static keyname_t keynames[] =
{
	{"TAB", K_TAB},
	{"ENTER", K_ENTER},
	{"ESCAPE", K_ESCAPE},
	{"SPACE", K_SPACE},
	{"BACKSPACE", K_BACKSPACE},
	{"UPARROW", K_UPARROW},
	{"DOWNARROW", K_DOWNARROW},
	{"LEFTARROW", K_LEFTARROW},
	{"RIGHTARROW", K_RIGHTARROW},

	{"ALT", K_ALT},
	{"CTRL", K_CTRL},
	{"SHIFT", K_SHIFT},
	
	{"F1", K_F1},
	{"F2", K_F2},
	{"F3", K_F3},
	{"F4", K_F4},
	{"F5", K_F5},
	{"F6", K_F6},
	{"F7", K_F7},
	{"F8", K_F8},
	{"F9", K_F9},
	{"F10", K_F10},
	{"F11", K_F11},
	{"F12", K_F12},

	{"INS", K_INS},
	{"DEL", K_DEL},
	{"PGDN", K_PGDN},
	{"PGUP", K_PGUP},
	{"HOME", K_HOME},
	{"END", K_END},
	{"LSHIFT", K_LSHIFT},
	{"RSHIFT", K_RSHIFT},
	{"NUMLOCK", K_NUMLOCK},
	{"MENU", K_MENU},
	{"LMETA", K_LMETA},
	{"RMETA", K_RMETA},
	{"CAPSLOCK", K_CAPSLOCK},

	{"MOUSE1", K_MOUSE1},
	{"MOUSE2", K_MOUSE2},
	{"MOUSE3", K_MOUSE3},

	{"JOY0", K_JOY0},
	{"JOY1", K_JOY1},
	{"JOY2", K_JOY2},
	{"JOY3", K_JOY3},
	{"JOY4", K_JOY4},
	{"JOY5", K_JOY5},
	{"JOY6", K_JOY6},
	{"JOY7", K_JOY7},
	{"JOY8", K_JOY8},
	{"JOY9", K_JOY9},
	{"JOY10", K_JOY10},
	{"JOY11", K_JOY11},
	{"JOY12", K_JOY12},
	{"JOY13", K_JOY13},
	{"JOY14", K_JOY14},
	{"JOY15", K_JOY15},
	{"JOY16", K_JOY16},
	{"JOY17", K_JOY17},
	{"JOY18", K_JOY18},
	{"JOY19", K_JOY19},
	{"JOY20", K_JOY20},
	{"JOY21", K_JOY21},
	{"JOY22", K_JOY22},
	{"JOY23", K_JOY23},
	{"JOY24", K_JOY24},
	{"JOY25", K_JOY25},
	{"JOY26", K_JOY26},
	{"JOY27", K_JOY27},
	{"JOY28", K_JOY28},
	{"JOY29", K_JOY29},
	{"JOY30", K_JOY30},
	{"JOY31", K_JOY31},
	{"JOY32", K_JOY32},

	{"PAUSE", K_PAUSE},

	{"MWHEELUP", K_MWHEELUP},
	{"MWHEELDOWN", K_MWHEELDOWN},
	
	{"SHAKENUN", K_SHAKE},

	{"SEMICOLON", ';'},	// because a raw semicolon seperates commands

	{NULL,0}
};

/*
==============================================================================

			LINE TYPING INTO THE CONSOLE

==============================================================================
*/
//#define MAX_Y 8
//#define MAX_X 12
//#define MAX_CHAR_LINE 35
#define MAX_Y 12
#define MAX_X 18
#define MAX_CHAR_LINE 54

extern int  osk_pos_x;
extern int  osk_pos_y;
extern char osk_buffer[54];

// naievil -- quick hack to make everything work properly
char *osk_text2 [] =
{
	" 1 2 3 4 5 6 7 8 9 0 - = ` ",
	" q w e r t y u i o p [ ]   ",
	"   a s d f g h j k l ; ' \\ ",
	"     z x c v b n m   , . / ",
	"                           ",
	" ! @ # $ % ^ & * ( ) _ + ~ ",
	" Q W E R T Y U I O P { }   ",
	"   A S D F G H J K L : \" | ",
	"     Z X C V B N M   < > ? "
};
/*
====================
Key_Console

Interactive line editing and console scrollback
====================
*/
extern qboolean console_enabled;
qboolean Con_isSetOSKActive(void);
void Con_SetOSKActive(qboolean active);
void Key_Console (int key)
{
	char	*cmd;
	//static	char current[MAXCMDLINE] = "";
	//int	history_line_last;
	//size_t		len;
	char *workline = key_lines[edit_line];
	
	// minus is to open console
	if (Con_isSetOSKActive())
	{	
		switch(key) {
			case K_JOY0:
				if (MAX_CHAR_LINE > strlen(osk_buffer)) {
					char *selected_line = osk_text2[osk_pos_y];
					char selected_char[2];

					selected_char[0] = selected_line[1+(2*osk_pos_x)];

					if (selected_char[0] == '\t')
						selected_char[0] = ' ';

					selected_char[1] = '\0';
					strcat(osk_buffer,selected_char);
				}
				break;

			case K_JOY17:
				strncpy(workline,osk_buffer,MAX_CHAR_LINE);
				//strcpy(key_lines[edit_line],workline);
				Con_SetOSKActive(false);
				break;

			case K_JOY1:
				Con_SetOSKActive(false);
				if (strlen(osk_buffer) > 1) {
					osk_buffer[strlen(osk_buffer)-1] = '\0';
				}
				break;

			case K_RIGHTARROW:
				osk_pos_x++;
				if (osk_pos_x > MAX_X)
					osk_pos_x = 0;//MAX_X
				break;

			case K_LEFTARROW:
				osk_pos_x--;
				if (osk_pos_x < 0)
					osk_pos_x = MAX_X;//0
				break;

			case K_DOWNARROW:
				osk_pos_y++;
				if (osk_pos_y > MAX_Y)
					osk_pos_y = 0;//MAX_Y
				break;
			case K_UPARROW:
				osk_pos_y--;
				if (osk_pos_y < 0)
					osk_pos_y = MAX_Y;//0
				break;

			default:
				break;
		}
	} else {
		if (key == K_JOY0)
		{
			Cbuf_AddText (key_lines[edit_line]+1);	// skip the >
			Cbuf_AddText ("\n");
			Con_Printf ("%s\n",key_lines[edit_line]);
			edit_line = (edit_line + 1) & 31;
			history_line = edit_line;
			key_lines[edit_line][0] = ']';
			key_linepos = 1;
			if (cls.state == ca_disconnected)
				SCR_UpdateScreen ();	// force an update, because the command
										// may take some time
			return;
		}

		if (key == K_TAB)
		{	// command completion
			cmd = Cmd_CompleteCommand (key_lines[edit_line]+1);
			if (!cmd)
				cmd = Cvar_CompleteVariable (key_lines[edit_line]+1);
			if (cmd)
			{
				strcpy (key_lines[edit_line]+1, cmd);
				key_linepos = Q_strlen(cmd)+1;
				key_lines[edit_line][key_linepos] = ' ';
				key_linepos++;
				key_lines[edit_line][key_linepos] = 0;
				return;
			}
		}
		
		if (key == K_JOY17) {
			Con_SetOSKActive(true);
			//strcpy(osk_buffer, workline);
			return;
		}
		
		if (key == K_BACKSPACE)
		{
			if (key_linepos > 1)
				key_linepos--;
			return;
		}

		if (key == K_UPARROW)
		{
			do
			{
				history_line = (history_line - 1) & 31;
			} while (history_line != edit_line
					&& !key_lines[history_line][1]);
			if (history_line == edit_line)
				history_line = (edit_line+1)&31;
			Q_strcpy(key_lines[edit_line], key_lines[history_line]);
			key_linepos = Q_strlen(key_lines[edit_line]);
			return;
		}

		if (key == K_DOWNARROW)
		{
			if (history_line == edit_line) return;
			do
			{
				history_line = (history_line + 1) & 31;
			}
			while (history_line != edit_line
				&& !key_lines[history_line][1]);
			if (history_line == edit_line)
			{
				key_lines[edit_line][0] = ']';
				key_linepos = 1;
			}
			else
			{
				Q_strcpy(key_lines[edit_line], key_lines[history_line]);
				key_linepos = Q_strlen(key_lines[edit_line]);
			}
			return;
		}

		if (key == K_PGUP || key==K_MWHEELUP)
		{
			con_backscroll += 2;
			if (con_backscroll > con_totallines - (vid.conheight>>3) - 1)
				con_backscroll = con_totallines - (vid.conheight>>3) - 1;
			return;
		}

		if (key == K_PGDN || key==K_MWHEELDOWN)
		{
			con_backscroll -= 2;
			if (con_backscroll < 0)
				con_backscroll = 0;
			return;
		}

		if (key == K_HOME)
		{
			con_backscroll = con_totallines - (vid.conheight>>3) - 1;
			return;
		}

		if (key == K_END)
		{
			//console_enabled = false;
			//con_backscroll = 0;
			return;
		}
		
		if (key < 32 || key > 127)
			return;	// non printable
			
		if (key_linepos < MAXCMDLINE-1)
		{
			key_lines[edit_line][key_linepos] = key;
			key_linepos++;
			key_lines[edit_line][key_linepos] = 0;
		}
		if (key == K_JOY1) {
			//console_enabled = false;
			//con_backscroll = 0;
			return;
		}
	
	}

}

//============================================================================

char chat_buffer[32];
qboolean team_message = false;

void Key_Message (key_id_t key)
{
	static int chat_bufferlen = 0;

	if (key == K_ENTER)
	{
		if (team_message)
			Cbuf_AddText ("say_team \"");
		else
			Cbuf_AddText ("say \"");
		Cbuf_AddText(chat_buffer);
		Cbuf_AddText("\"\n");

		key_dest = key_game;
		chat_bufferlen = 0;
		chat_buffer[0] = 0;
		return;
	}

	if (key == K_ESCAPE)
	{
		console_enabled = false;
		key_dest = key_game;
		chat_bufferlen = 0;
		chat_buffer[0] = 0;
		return;
	}

	if (key < 32 || key > 127)
		return;	// non printable

	if (key == K_BACKSPACE)
	{
		if (chat_bufferlen)
		{
			chat_bufferlen--;
			chat_buffer[chat_bufferlen] = 0;
		}
		return;
	}

	if (chat_bufferlen == 31)
		return; // all full

	chat_buffer[chat_bufferlen++] = key;
	chat_buffer[chat_bufferlen] = 0;
}

//============================================================================


/*
===================
Key_StringToKeynum

Returns a key number to be used to index keybindings[] by looking at
the given string.  Single ascii characters return themselves, while
the K_* names are matched up.
===================
*/
int Key_StringToKeynum (char *str)
{
	keyname_t	*kn;
	
	if (!str || !str[0])
		return -1;
	if (!str[1])
		return str[0];

	for (kn=keynames ; kn->name ; kn++)
	{
		if (!Q_strcasecmp(str,kn->name))
			return kn->keynum;
	}
	return -1;
}

/*
===================
Key_KeynumToString

Returns a string (either a single ascii char, or a K_* name) for the
given keynum.
FIXME: handle quote special (general escape sequence?)
===================
*/
char *Key_KeynumToString (int keynum)
{
	keyname_t	*kn;	
	static	char	tinystr[2];
	
	if (keynum == -1)
		return "<KEY NOT FOUND>";
	if (keynum > 32 && keynum < 127)
	{	// printable ascii
		tinystr[0] = keynum;
		tinystr[1] = 0;
		return tinystr;
	}
	
	for (kn=keynames ; kn->name ; kn++)
		if (keynum == kn->keynum)
			return kn->name;

	return "<UNKNOWN KEYNUM>";
}


/*
===================
Key_SetBinding
===================
*/
void Key_SetBinding (key_id_t keynum, char *binding)
{
	char	*new;
	int		l;
			
	if (keynum == -1)
		return;

// free old bindings
	if (keybindings[keynum])
	{
		Z_Free (keybindings[keynum]);
		keybindings[keynum] = NULL;
	}
			
// allocate memory for new binding
	l = Q_strlen (binding);	
	new = Z_Malloc (l+1);
	strcpy (new, binding);
	new[l] = 0;
	keybindings[keynum] = new;	
}

/*
===================
Key_SetDTBinding
===================
*/
void Key_SetDTBinding (int keynum, char *binding)
{
	char	*new;
	int		l;

	if (keynum == -1)
		return;

// free old bindings
	if (dtbindings[keynum])
	{
		Z_Free (dtbindings[keynum]);
		dtbindings[keynum] = NULL;
	}

// allocate memory for new binding
	l = strlen (binding);
	new = Z_Malloc (l+1);
	strcpy (new, binding);
	new[l] = 0;
	dtbindings[keynum] = new;
}

/*
===================
Key_Unbind_f
===================
*/
void Key_Unbind_f (void)
{
	int		b;

	if (Cmd_Argc() != 2)
	{
		Con_Printf ("unbind <key> : remove commands from a key\n");
		return;
	}
	
	b = Key_StringToKeynum (Cmd_Argv(1));
	if (b==-1)
	{
		Con_Printf ("\"%s\" isn't a valid key\n", Cmd_Argv(1));
		return;
	}

	Key_SetBinding (b, "");
}

void Key_Unbindall_f (void)
{
	int		i;
	
	for (i=0 ; i<KEY_COUNT ; i++)
		if (keybindings[i])
			Key_SetBinding (i, "");
}


/*
===================
Key_Bind_f
===================
*/
void Key_Bind_f (void)
{
	int			i, c, b;
	char		cmd[1024];
	
	c = Cmd_Argc();

	if (c != 2 && c != 3)
	{
		Con_Printf ("bind <key> [command] : attach a command to a key\n");
		return;
	}
	b = Key_StringToKeynum (Cmd_Argv(1));
	if (b==-1)
	{
		Con_Printf ("\"%s\" isn't a valid key\n", Cmd_Argv(1));
		return;
	}

	if (c == 2)
	{
		if (keybindings[b])
			Con_Printf ("\"%s\" = \"%s\"\n", Cmd_Argv(1), keybindings[b] );
		else
			Con_Printf ("\"%s\" is not bound\n", Cmd_Argv(1) );
		return;
	}
	
// copy the rest of the command line
	cmd[0] = 0;		// start out with a null string
	for (i=2 ; i< c ; i++)
	{
		if (i > 2)
			strcat (cmd, " ");
		strcat (cmd, Cmd_Argv(i));
	}

	Key_SetBinding (b, cmd);
}

/*
===================
Key_Binddt_f
===================
*/
void Key_Binddt_f (void)
{
	int			i, c, b;
	char		cmd[1024];

	c = Cmd_Argc();

	if (c != 2 && c != 3)
	{
		Con_Printf ("binddt <key> [command] : attach a command to a double tap key\n");
		return;
	}
	b = Key_StringToKeynum (Cmd_Argv(1));
	if (b==-1)
	{
		Con_Printf ("\"%s\" isn't a valid key\n", Cmd_Argv(1));
		return;
	}

	if (c == 2)
	{
		if (dtbindings[b])
			Con_Printf ("\"%s\" = \"%s\"\n", Cmd_Argv(1), dtbindings[b] );
		else
			Con_Printf ("\"%s\" is not bound\n", Cmd_Argv(1) );
		return;
	}

// copy the rest of the command line
	cmd[0] = 0;		// start out with a null string
	for (i=2 ; i< c ; i++)
	{
		if (i > 2)
			strcat (cmd, " ");
		strcat (cmd, Cmd_Argv(i));
	}

	Key_SetDTBinding (b, cmd);
}

/*
============
Key_WriteBindings

Writes lines containing "bind key value"
============
*/
void Key_WriteBindings (FILE *f)
{
	int		i;

	for (i=0 ; i<KEY_COUNT ; i++)
		if (keybindings[i])
			if (*keybindings[i])
				fprintf (f, "bind \"%s\" \"%s\"\n", Key_KeynumToString(i), keybindings[i]);
}

/*
============
Key_WriteDTBindings

Writes lines containing "binddt key value"
============
*/
void Key_WriteDTBindings (FILE *f)
{
	int		i;

	for (i=0 ; i<256 ; i++)
		if (dtbindings[i])
			if (*dtbindings[i])
				fprintf (f, "binddt \"%s\" \"%s\"\n", Key_KeynumToString(i), dtbindings[i]);
}

/*
===================
Key_Init
===================
*/
void Key_Init (void)
{
	int		i;

	for (i=0 ; i<32 ; i++)
	{
		key_lines[i][0] = ']';
		key_lines[i][1] = 0;
	}
	key_linepos = 1;
	
//
// init ascii characters in console mode
//
	for (i=32 ; i<128 ; i++)
		consolekeys[i] = true;
	consolekeys[K_ENTER] = true;
	consolekeys[K_TAB] = true;
	consolekeys[K_LEFTARROW] = true;
	consolekeys[K_RIGHTARROW] = true;
	consolekeys[K_UPARROW] = true;
	consolekeys[K_DOWNARROW] = true;
	consolekeys[K_BACKSPACE] = true;
	consolekeys[K_PGUP] = true;
	consolekeys[K_PGDN] = true;
	consolekeys[K_SHIFT] = true;
	consolekeys[K_MWHEELUP] = true;
	consolekeys[K_MWHEELDOWN] = true;
	// sB -- Wii bindings start
	consolekeys[K_JOY0] = true;
	consolekeys[K_JOY1] = true;
	consolekeys[K_JOY17] = true;
	// end 
	consolekeys['`'] = false;
	consolekeys['~'] = false;

	for (i=0 ; i<KEY_COUNT ; i++)
		keyshift[i] = i;
	for (i='a' ; i<='z' ; i++)
		keyshift[i] = i - 'a' + 'A';
	keyshift['1'] = '!';
	keyshift['2'] = '@';
	keyshift['3'] = '#';
	keyshift['4'] = '$';
	keyshift['5'] = '%';
	keyshift['6'] = '^';
	keyshift['7'] = '&';
	keyshift['8'] = '*';
	keyshift['9'] = '(';
	keyshift['0'] = ')';
	keyshift['-'] = '_';
	keyshift['='] = '+';
	keyshift[','] = '<';
	keyshift['.'] = '>';
	keyshift['/'] = '?';
	keyshift[';'] = ':';
	keyshift['\''] = '"';
	keyshift['['] = '{';
	keyshift[']'] = '}';
	keyshift['`'] = '~';
	keyshift['\\'] = '|';

	menubound[K_ESCAPE] = true;
	for (i=0 ; i<12 ; i++)
		menubound[K_F1+i] = true;

//
// register our functions
//
	Cmd_AddCommand ("bind",Key_Bind_f);
	Cmd_AddCommand ("unbind",Key_Unbind_f);
	Cmd_AddCommand ("unbindall",Key_Unbindall_f);


}

/*
===================
Key_Event

Called by the system between frames for both key up and key down events
Should NOT be called during an interrupt!
===================
*/
void Key_Event (key_id_t key, qboolean down)
{
	char	*kb;
	char	cmd[1024];

	keydown[key] = down;

	if (!down)
		key_repeats[key] = 0;

	key_lastpress = key;
	key_count++;
	if (key_count <= 0)
	{
		return;		// just catching keys for Con_NotifyBox
	}

// update auto-repeat status
	if (down)
	{
		key_repeats[key]++;
		if (key != K_BACKSPACE && key != K_PAUSE && key_repeats[key] > 1)
		{
			return;	// ignore most autorepeats
		}
			
		if (key >= K_BACKSPACE && !keybindings[key])
			Con_Printf ("%s is unbound, use the options screen to set.\n", Key_KeynumToString (key) );
	}

	if (key == K_SHIFT)
		shift_down = down;

//
// handle escape specialy, so the user can never unbind it
//
	/*
	if (key == K_JOY1)
	{
		if (!down)
			return;
		switch (key_dest)
		{
		case key_message:
			Key_Message (key);
			break;
		case key_menu:
		case key_menu_pause:
			M_Keydown (key);
			break;
		case key_game:
			break;
		case key_console:
			console_enabled = false;
			M_ToggleMenu_f ();
			break;
		default:
			Sys_Error ("Bad key_dest");
		}
		return;
	}
	*/
//
// key up events only generate commands if the game key binding is
// a button command (leading + sign).  These will occur even in console mode,
// to keep the character from continuing an action started before a console
// switch.  Button commands include the kenum as a parameter, so multiple
// downs can be matched with ups
//
	if (!down)
	{
		kb = keybindings[key];
		if (kb && kb[0] == '+')
		{
			sprintf (cmd, "-%s %i\n", kb+1, key);
			Cbuf_AddText (cmd);
		}
		if (keyshift[key] != key)
		{
			kb = keybindings[keyshift[key]];
			if (kb && kb[0] == '+')
			{
				sprintf (cmd, "-%s %i\n", kb+1, key);
				Cbuf_AddText (cmd);
			}
		}
		return;
	}

//
// during demo playback, most keys bring up the main menu
//
	if (cls.demoplayback && down && consolekeys[key] && key_dest == key_game)
	{
		M_ToggleMenu_f ();
		return;
	}

//
// if not a consolekey, send to the interpreter no matter what mode is
//
	if ((key_dest == key_menu && menubound[key]) ||
	    (key_dest == key_console && !consolekeys[key]) ||
	    (key_dest == key_game && (!con_forcedup || !consolekeys[key])))
	{
		kb = keybindings[key];
		if (kb)
		{
			if (kb[0] == '+')
			{	// button commands add keynum as a parm
				sprintf (cmd, "%s %i\n", kb, key);
				Cbuf_AddText (cmd);
			}
			else
			{
				Cbuf_AddText (kb);
				Cbuf_AddText ("\n");
			}
		}
		return;
	}
	
	if (!down)
		return;		// other systems only care about key down events

	if (shift_down)
	{
		key = keyshift[key];
	}
	switch (key_dest)
	{
		case key_message:
			Key_Message (key);
			break;
		case key_menu:
		case key_menu_pause:
			M_Keydown (key);
			break;

		case key_game:
		case key_console: {
			Key_Console (key);
			break;
		}
		default:
			Sys_Error ("Bad key_dest");
	}
}


/*
===================
Key_ClearStates
===================
*/
void Key_ClearStates (void)
{
	int		i;

	for (i=0 ; i<KEY_COUNT ; i++)
	{
		keydown[i] = false;
		key_repeats[i] = 0;
	}
}

