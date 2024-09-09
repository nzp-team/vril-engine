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
#include <sys/dirent.h>

extern cvar_t	r_wateralpha;
extern cvar_t	r_vsync;
extern cvar_t	in_disable_analog;
extern cvar_t	in_analog_strafe;
extern cvar_t	in_x_axis_adjust;
extern cvar_t	in_y_axis_adjust;
extern cvar_t	crosshair;
extern cvar_t	r_dithering;
//extern cvar_t	r_retro;
extern cvar_t	waypoint_mode;
extern cvar_t   in_anub_mode;

extern int loadingScreen;
extern char* loadname2;
extern char* loadnamespec;
extern qboolean loadscreeninit;

char* game_build_date;

// Backgrounds
qpic_t *menu_bk;

// Map screens
qpic_t *menu_ndu;
qpic_t *menu_wh;
qpic_t *menu_wh2;
//qpic_t *menu_kn;
qpic_t *menu_ch;
//qpic_t *menu_wn;
qpic_t *menu_custom;
qpic_t *menu_cuthum;

achievement_list_t achievement_list[MAX_ACHIEVEMENTS];

void (*vid_menudrawfn)(void);
void (*vid_menukeyfn)(int key);

enum 
{
	m_none, 
	m_start,
	m_main, 
	m_paused_menu, 
	m_singleplayer, 
	m_load, 
	m_save, 
	m_custommaps, 
	m_setup, 
	m_net, 
	m_options, 
	m_video, 
	m_keys, 
	m_help, 
	m_quit, 
	m_restart,
	m_credits,
	m_exit,
	m_serialconfig, 
	m_modemconfig, 
	m_lanconfig, 
	m_gameoptions, 
	m_search, 
	m_slist,
} m_state;

void M_Start_Menu_f (void);
void M_Menu_Main_f (void);
	void M_Menu_SinglePlayer_f (void);
		void M_Menu_CustomMaps_f (void);
	void M_Menu_Options_f (void);
		void M_Menu_Keys_f (void);
		void M_Menu_Video_f (void);
	void M_Menu_Credits_f (void);
	void M_Menu_Quit_f (void);
void M_Menu_GameOptions_f (void);

void M_Main_Draw (void);
	void M_SinglePlayer_Draw (void);
		void M_Menu_CustomMaps_Draw (void);
	void M_Options_Draw (void);
		void M_Keys_Draw (void);
		void M_Video_Draw (void);
	void M_Menu_Credits_Draw (void);
	void M_Quit_Draw (void);

void M_Main_Key (int key);
	void M_SinglePlayer_Key (int key);
		void M_Menu_CustomMaps_Key (int key);
	void M_Options_Key (int key);
		void M_Keys_Key (int key);
		void M_Video_Key (int key);
	void M_Menu_Credits_Key (int key);
	void M_Quit_Key (int key);
void M_GameOptions_Key (int key);

qboolean	m_entersound;		// play after drawing a frame, so caching
								// won't disrupt the sound
qboolean	m_recursiveDraw;

int			m_return_state;
qboolean	m_return_onerror;
char		m_return_reason [32];

typedef struct
{
	int 		occupied;
	int 	 	map_allow_game_settings;
	int 	 	map_use_thumbnail;
	char* 		map_name;
	char* 		map_name_pretty;
	char* 		map_desc_1;
	char* 		map_desc_2;
	char* 		map_desc_3;
	char* 		map_desc_4;
	char* 		map_desc_5;
	char* 		map_desc_6;
	char* 		map_desc_7;
	char* 		map_desc_8;
	char* 		map_author;
	char* 		map_thumbnail_path;
} usermap_t;

usermap_t custom_maps[50];

/*
================
M_DrawCharacter

Draws one solid graphics character
================
*/
void M_DrawCharacter (int cx, int line, int num)
{
	Draw_Character ( cx + ((vid.width - 320)>>1), line, num);
}

void M_Print (int cx, int cy, char *str)
{
	while (*str)
	{
		M_DrawCharacter (cx, cy, (*str)+128);
		str++;
		cx += 8;
	}
}

void M_PrintWhite (int cx, int cy, char *str)
{
	while (*str)
	{
		M_DrawCharacter (cx, cy, *str);
		str++;
		cx += 8;
	}
}

void M_DrawTransPic (int x, int y, qpic_t *pic)
{
	Draw_TransPic (x + ((vid.width - 320)>>1), y, pic);
}

void M_DrawPic (int x, int y, qpic_t *pic)
{
	Draw_Pic (x + ((vid.width - 320)>>1), y, pic);
}

byte identityTable[256];
byte translationTable[256];

void M_BuildTranslationTable(int top, int bottom)
{
	int		j;
	byte	*dest, *source;

	for (j = 0; j < 256; j++)
		identityTable[j] = j;
	dest = translationTable;
	source = identityTable;
	memcpy (dest, source, 256);

	if (top < 128)	// the artists made some backwards ranges.  sigh.
		memcpy (dest + TOP_RANGE, source + top, 16);
	else
		for (j=0 ; j<16 ; j++)
			dest[TOP_RANGE+j] = source[top+15-j];

	if (bottom < 128)
		memcpy (dest + BOTTOM_RANGE, source + bottom, 16);
	else
		for (j=0 ; j<16 ; j++)
			dest[BOTTOM_RANGE+j] = source[bottom+15-j];
}


void M_DrawTransPicTranslate (int x, int y, qpic_t *pic)
{
	Draw_TransPicTranslate (x + ((vid.width - 320)>>1), y, pic, translationTable);
}


void M_DrawTextBox (int x, int y, int width, int lines)
{

}

//=============================================================================

void M_Load_Menu_Pics ()
{
	menu_bk 	= Draw_CachePic("gfx/menu/menu_background");
	menu_ndu 	= Draw_CachePic("gfx/menu/nacht_der_untoten");
	//menu_kn 	= Draw_CachePic("gfx/menu/kino_der_toten");
	menu_wh 	= Draw_CachePic("gfx/menu/nzp_warehouse");
	menu_wh2 	= Draw_CachePic("gfx/menu/nzp_warehouse2");
	//menu_wn 	= Draw_CachePic("gfx/menu/wahnsinn");
	menu_ch 	= Draw_CachePic("gfx/menu/christmas_special");
	menu_custom = Draw_CachePic("gfx/menu/custom");
}

void M_Start_Menu_f ()
{
	//Load_Achivements();
	M_Load_Menu_Pics();
	key_dest = key_menu;
	m_state = m_start;
	m_entersound = true;
	//loadingScreen = 0;
}

static void M_Start_Menu_Draw ()
{
	// Background
	menu_bk = Draw_CachePic("gfx/menu/menu_background");
	Draw_StretchPic(0, 0, menu_bk, 400, 240);

	// Fill black to make everything easier to see
	Draw_FillByColor(0, 0, vid.width, vid.height, 0, 0, 0, 102);

	Draw_ColoredStringCentered(vid.height - 64, "Press A to Start", 255, 0, 0, 255, 1);
}

void M_Start_Key (int key)
{
	switch (key)
	{
		case K_AUX1:
			S_LocalSound ("sounds/menu/enter.wav");
			//Cbuf_AddText("cd playstring tensioned_by_the_damned 1\n");
			Cbuf_AddText("togglemenu\n");
			break;
	}
}


int m_save_demonum;

/*
================
M_ToggleMenu_f
================
*/
void M_ToggleMenu_f (void)
{
	m_entersound = true;

	if (key_dest == key_menu || key_dest == key_menu_pause)
	{
		if (m_state != m_main && m_state != m_paused_menu)
		{
			M_Menu_Main_f ();
			return;
		}
		key_dest = key_game;
		m_state = m_none;
		return;
	}
	if (key_dest == key_console)
	{
		Con_ToggleConsole_f ();
	}
	else if (sv.active && (svs.maxclients > 1 || key_dest == key_game))
	{
		M_Paused_Menu_f();
	}
	else
	{
		M_Menu_Main_f ();
	}
}


int M_Paused_Cusor;
#define Max_Paused_Iteams 5

void M_Paused_Menu_f ()
{
	key_dest = key_menu_pause;
	m_state = m_paused_menu;
	m_entersound = true;
	loadingScreen = 0;
	loadscreeninit = false;
	M_Paused_Cusor = 0;
}

static void M_Paused_Menu_Draw ()
{
	// Fill black to make everything easier to see
	Draw_FillByColor(0, 0, vid.width, vid.height, 0, 0, 0, 102);

	// Header
	Draw_ColoredString(10, 10, "PAUSED", 255, 255, 255, 255, 2);

	if ((M_Paused_Cusor == 0))
		Draw_ColoredString(10, 135, "Resume", 255, 0, 0, 255, 1);
	else
		Draw_ColoredString(10, 135, "Resume", 255, 255, 255, 255, 1);

	if ((M_Paused_Cusor == 1))
		Draw_ColoredString(10, 145, "Restart", 255, 0, 0, 255, 1);
	else
		Draw_ColoredString(10, 145, "Restart", 255, 255, 255, 255, 1);

	if ((M_Paused_Cusor == 2))
		Draw_ColoredString(10, 155, "Settings", 255, 0, 0, 255, 1);
	else
		Draw_ColoredString(10, 155, "Settings", 255, 255, 255, 255, 1);

	if (waypoint_mode.value) {
		if ((M_Paused_Cusor == 3))
			Draw_ColoredString(10, 165, "Save Waypoints", 255, 0, 0, 255, 1);
		else
			Draw_ColoredString(10, 165, "Save Waypoints", 255, 255, 255, 255, 1);
	} else {
		if ((M_Paused_Cusor == 3))
			Draw_ColoredString(10, 165, "Achievements", 255, 0, 0, 255, 1);
		else
			Draw_ColoredString(10, 165, "Achievements", 255, 255, 255, 255, 1);
	}

	if ((M_Paused_Cusor == 4))
		Draw_ColoredString(10, 175, "Main Menu", 255, 0, 0, 255, 1);
	else
		Draw_ColoredString(10, 175, "Main Menu", 255, 255, 255, 255, 1);
}

static void M_Paused_Menu_Key (int key)
{
	switch (key)
	{
		case K_ESCAPE:
		case K_AUX2:
			S_LocalSound ("sounds/menu/enter.wav");
			Cbuf_AddText("togglemenu\n");
			break;

		case K_DOWNARROW:
			S_LocalSound ("sounds/menu/navigate.wav");
			if (++M_Paused_Cusor >= Max_Paused_Iteams)
				M_Paused_Cusor = 0;
			break;

		case K_UPARROW:
			S_LocalSound ("sounds/menu/navigate.wav");
			if (--M_Paused_Cusor < 0)
				M_Paused_Cusor = Max_Paused_Iteams - 1;
			break;

		case K_ENTER:
		case K_AUX1:
			m_entersound = true;

			switch (M_Paused_Cusor)
			{
			case 0:
				key_dest = key_game;
				m_state = m_none;
				break;
			case 1:
				M_Menu_Restart_f();
				break;
			case 2:
				M_Menu_Options_f();
				key_dest = key_menu_pause;
				break;
			case 3:
				if (waypoint_mode.value) {
					Cbuf_AddText("impulse 101\n");
				}
				/*else
					M_Menu_Achievement_f();
				*/ // naievil -- fixme: do not have achievements
				key_dest = key_menu_pause;
				break;
			case 4:
				M_Menu_Exit_f();
				break;
			}
		}
}


//=============================================================================
/* MAIN MENU */

int	m_main_cursor;
#define	MAIN_ITEMS	4


void M_Menu_Main_f (void)
{
	if (key_dest != key_menu)
	{
		m_save_demonum = cls.demonum;
		cls.demonum = -1;
	}
	key_dest = key_menu;
	m_state = m_main;
	m_entersound = true;
}

void M_Main_Draw (void)
{
	// Background
	menu_bk = Draw_CachePic("gfx/menu/menu_background");
	Draw_StretchPic(0, 0, menu_bk, 400, 240);

	// Fill black to make everything easier to see
	Draw_FillByColor(0, 0, 400, 240, 0, 0, 0, 102);

	// Version String
	Draw_ColoredString((vid.width - getTextWidth(game_build_date, 1)) + 4, 5, game_build_date, 255, 255, 255, 255, 1);

	// Header
	Draw_ColoredString(5, 5, "MAIN MENU", 255, 255, 255, 255, 2);

	// Solo
	if (m_main_cursor == 0)
		Draw_ColoredString(5, 40, "Solo", 255, 0, 0, 255, 1);
	else
		Draw_ColoredString(5, 40, "Solo", 255, 255, 255, 255, 1);


	// Co-Op (Unfinished, so non-selectable)
	Draw_ColoredString(5, 50, "Co-Op (Coming Soon!)", 128, 128, 128, 255, 1);

	// Divider
	Draw_FillByColor(5, 63, 160, 2, 130, 130, 130, 255);

	if (m_main_cursor == 1)
		Draw_ColoredString(5, 70, "Settings", 255, 0, 0, 255, 1);
	else
		Draw_ColoredString(5, 70, "Settings", 255, 255, 255, 255, 1);

	Draw_ColoredString(5, 80, "Achievements", 128, 128, 128, 255, 1);

	// Divider
	Draw_FillByColor(5, 93, 160, 2, 130, 130, 130, 255);

	if (m_main_cursor == 2)
		Draw_ColoredString(5, 100, "Credits", 255, 0, 0, 255, 1);
	else
		Draw_ColoredString(5, 100, "Credits", 255, 255, 255, 255, 1);

	// Divider
	Draw_FillByColor(5, 113, 160, 2, 130, 130, 130, 255);

	if (m_main_cursor == 3)
		Draw_ColoredString(5, 120, "Exit", 255, 0, 0, 255, 1);
	else
		Draw_ColoredString(5, 120, "Exit", 255, 255, 255, 255, 1);

	// Descriptions
	switch(m_main_cursor) {
		case 0: // Solo
			Draw_ColoredString(5, 220, "Take on the Hordes by yourself.", 255, 255, 255, 255, 1);
			break;
		case 1: // Settings
			Draw_ColoredString(5, 220, "Adjust your Settings to Optimize your Experience.", 255, 255, 255, 255, 1);
			break;
		case 2: // Credits
			Draw_ColoredString(5, 220, "See who made NZ:P possible.", 255, 255, 255, 255, 1);
			break;
		case 3: // Exit
			Draw_ColoredString(5, 220, "Return to Home Menu.", 255, 255, 255, 255, 1);
			break;
	}
}


void M_Main_Key (int key)
{
	switch (key)
	{

	case K_DOWNARROW:
		S_LocalSound ("sounds/menu/navigate.wav");
		if (++m_main_cursor >= MAIN_ITEMS)
			m_main_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("sounds/menu/navigate.wav");
		if (--m_main_cursor < 0)
			m_main_cursor = MAIN_ITEMS - 1;
		break;

	case K_ENTER:
	case K_AUX1:
		m_entersound = true;

		switch (m_main_cursor)
		{
		case 0:
			M_Menu_SinglePlayer_f ();
			break;

		case 1:
			M_Menu_Options_f ();
			break;

		case 2:
			M_Menu_Credits_f ();
			break;

		case 3:
			M_Menu_Quit_f ();
			break;
		}
	}
}

//=============================================================================
/* CREDITS MENU */


void M_Menu_Credits_f (void)
{
	key_dest = key_menu;
	m_state = m_credits;
	m_entersound = true;
}

void M_Credits_Draw (void)
{
   	// Background
	menu_bk = Draw_CachePic("gfx/menu/menu_background");
	Draw_StretchPic(0, 0, menu_bk, 400, 240);

	// Fill black to make everything easier to see
	Draw_FillByColor(0, 0, 400, 240, 0, 0, 0, 102);

	// Header
	Draw_ColoredString(5, 5, "CREDITS", 255, 255, 255, 255, 2);

	Draw_ColoredString(5, 30, "Programming:", 255, 255, 255, 255, 1);
	Draw_ColoredString(5, 40, "Blubs, Jukki, DR_Mabuse1981, Naievil", 255, 255, 255, 255, 1);
	Draw_ColoredString(5, 50, "Cypress, ScatterBox", 255, 255, 255, 255, 1);

	Draw_ColoredString(5, 70, "Models:", 255, 255, 255, 255, 1);
	Draw_ColoredString(5, 80, "Blubs, Ju[s]tice, Derped_Crusader", 255, 255, 255, 255, 1);

	Draw_ColoredString(5, 100, "GFX:", 255, 255, 255, 255, 1);
	Draw_ColoredString(5, 110, "Blubs, Ju[s]tice, Cypress, Derped_Crusader", 255, 255, 255, 255, 1);

	Draw_ColoredString(5, 130, "Sounds/Music:", 255, 255, 255, 255, 1);
	Draw_ColoredString(5, 140, "Blubs, Biodude, Cypress, Marty P.", 255, 255, 255, 255, 1);

	Draw_ColoredString(5, 160, "Special Thanks:", 255, 255, 255, 255, 1);
	Draw_ColoredString(5, 170, "- Spike, Eukara:     FTEQW", 255, 255, 255, 255, 1);
	Draw_ColoredString(5, 180, "- Shpuld:            CleanQC4FTE", 255, 255, 255, 255, 1);
	Draw_ColoredString(5, 190, "- Crow_Bar, st1x51:  dQuake(plus)", 255, 255, 255, 255, 1);
	Draw_ColoredString(5, 200, "- fgsfdsfgs:         Quakespasm-NX", 255, 255, 255, 255, 1);
	Draw_ColoredString(5, 210, "- MasterFeizz:       ctrQuake", 255, 255, 255, 255, 1);
	Draw_ColoredString(5, 220, "- Rinnegatamante:    Initial VITA Port & Updater", 255, 255, 255, 255, 1);

	Draw_ColoredString(5, 230, "Back", 255, 0, 0, 255, 1);
}


void M_Credits_Key (int key)
{
	switch (key)
	{
		case K_ENTER:
		case K_AUX1:
        case K_AUX2:
            M_Menu_Main_f ();
            break;
	}
}

//=============================================================================
/* RESTART MENU */

qboolean	wasInMenus;


char *restartMessage [] =
{

  " Are you sure you want",
  "  to restart this game? ",  //msg:0
  "                               ",
  "   A :Yes    B : No       "
};


void M_Menu_Restart_f (void)
{
	wasInMenus = (key_dest == key_menu_pause);
	key_dest = key_menu_pause;
	m_state = m_restart;
	m_entersound = true;
}


void M_Restart_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
	case K_AUX2:
	case 'n':
	case 'N':
		m_state = m_paused_menu;
		m_entersound = true;
		break;

	case 'Y':
	case 'y':
	case K_ENTER:
	case K_AUX1:
		key_dest = key_game;
		m_state = m_none;
		// Cbuf_AddText ("restart\n"); // nai -- old, now do soft reset
		PR_ExecuteProgram (pr_global_struct->Soft_Restart);
		break;

	default:
		break;
	}

}


void M_Restart_Draw (void)
{
	m_state = m_paused_menu;
	m_recursiveDraw = true;
	M_Draw ();
	m_state = m_restart;

	M_DrawTextBox (56, 76, 24, 4);
	M_Print (64, 84,  restartMessage[0]);
	M_Print (64, 92,  restartMessage[1]);
	M_Print (64, 100, restartMessage[2]);
	M_Print (64, 108, restartMessage[3]);
}




//=============================================================================
/* EXIT MENU */


char *exitMessage [] =
{

  " Are you sure you want  ",
  "to quit to the Main Menu?",  //msg:0
  "                                  ",
  "   A :Yes    B : No       "
};


void M_Menu_Exit_f (void)
{
	wasInMenus = (key_dest == key_menu_pause);
	key_dest = key_menu_pause;
	m_state = m_exit;
	m_entersound = true;
}


void M_Exit_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
	case K_AUX2:
	case 'n':
	case 'N':
		m_state = m_paused_menu;
		m_entersound = true;
		break;

	case 'Y':
	case 'y':
	case K_ENTER:
	case K_AUX1:
		Cbuf_AddText("disconnect\n");
		CL_ClearState ();
		M_Menu_Main_f();
		break;

	default:
		break;
	}

}


void M_Exit_Draw (void)
{
	m_state = m_paused_menu;
	m_recursiveDraw = true;
	M_Draw ();
	m_state = m_exit;

	M_DrawTextBox (56, 76, 24, 4);
	M_Print (64, 84,  exitMessage[0]);
	M_Print (64, 92,  exitMessage[1]);
	M_Print (64, 100, exitMessage[2]);
	M_Print (64, 108, exitMessage[3]);
}


//=============================================================================
/* SINGLE PLAYER MENU */

int	m_singleplayer_cursor;
#define	SINGLEPLAYER_ITEMS	6


void M_Menu_SinglePlayer_f (void)
{
	key_dest = key_menu;
	m_state = m_singleplayer;
	m_entersound = true;
}



void M_SinglePlayer_Draw (void)
{
	// Background
	menu_bk = Draw_CachePic("gfx/menu/menu_background");
	Draw_StretchPic(0, 0, menu_bk, 400, 240);

	// Fill black to make everything easier to see
	Draw_FillByColor(0, 0, 400, 240, 0, 0, 0, 102);

	// Header
	Draw_ColoredString(5, 5, "SOLO", 255, 255, 255, 255, 2);

	// Nacht der Untoten
	if (m_singleplayer_cursor == 0)
		Draw_ColoredString(5, 40, "Nacht der Untoten", 255, 0, 0, 255, 1);
	else
		Draw_ColoredString(5, 40, "Nacht der Untoten", 255, 255, 255, 255, 1);

	// Divider
	Draw_FillByColor(5, 53, 160, 2, 130, 130, 130, 255);

	// Warehouse
	if (m_singleplayer_cursor == 1)
		Draw_ColoredString(5, 60, "Warehouse", 255, 0, 0, 255, 1);
	else
		Draw_ColoredString(5, 60, "Warehouse", 255, 255, 255, 255, 1);

	// Warehouse (Classic)
	if (m_singleplayer_cursor == 2)
		Draw_ColoredString(5, 70, "Warehouse (Classic)", 255, 0, 0, 255, 1);
	else
		Draw_ColoredString(5, 70, "Warehouse (Classic)", 255, 255, 255, 255, 1);

	// Christmas Special
	if (m_singleplayer_cursor == 3)
		Draw_ColoredString(5, 80, "Christmas Special", 255, 0, 0, 255, 1);
	else
		Draw_ColoredString(5, 80, "Christmas Special", 255, 255, 255, 255, 1);

	// Divider
	Draw_FillByColor(5, 93, 160, 2, 130, 130, 130, 255);

	// Custom Maps
	if (m_singleplayer_cursor == 4)
		Draw_ColoredString(5, 100, "Custom Maps", 255, 0, 0, 255, 1);
	else
		Draw_ColoredString(5, 100, "Custom Maps", 255, 255, 255, 255, 1);

	// Back
	if (m_singleplayer_cursor == 5)
		Draw_ColoredString(5, 230, "Back", 255, 0, 0, 255, 1);
	else
		Draw_ColoredString(5, 230, "Back", 255, 255, 255, 255, 1);

	// Map description & pic
	switch(m_singleplayer_cursor) {
		case 0:
			menu_ndu = Draw_CachePic("gfx/menu/nacht_der_untoten");
			Draw_StretchPic(185, 40, menu_ndu, 175, 100);
			Draw_ColoredString(180, 148, "Desolate bunker located on a Ge-", 255, 255, 255, 255, 1);
			Draw_ColoredString(180, 158, "rman airfield, stranded after a", 255, 255, 255, 255, 1);
			Draw_ColoredString(180, 168, "brutal plane crash surrounded by", 255, 255, 255, 255, 1);
			Draw_ColoredString(180, 178, "hordes of undead. Exploit myste-", 255, 255, 255, 255, 1);
			Draw_ColoredString(180, 188, "rious forces at play and hold o-", 255, 255, 255, 255, 1);
			Draw_ColoredString(180, 198, "ut against relentless waves. Der", 255, 255, 255, 255, 1);
			Draw_ColoredString(180, 208, "Anstieg ist jetzt. Will you fall", 255, 255, 255, 255, 1);
			Draw_ColoredString(180, 218, "to the overwhelming onslaught?", 255, 255, 255, 255, 1);
			break;
		case 1:
			menu_wh2 = Draw_CachePic("gfx/menu/nzp_warehouse2");
			Draw_StretchPic(185, 40, menu_wh2, 175, 100);
			Draw_ColoredString(180, 148, "Four nameless marines find them-", 255, 255, 255, 255, 1);
			Draw_ColoredString(180, 158, "selves at a forsaken warehouse,", 255, 255, 255, 255, 1);
			Draw_ColoredString(180, 168, "or is it something more? Fight", 255, 255, 255, 255, 1);
			Draw_ColoredString(180, 178, "your way to uncovering its sec-", 255, 255, 255, 255, 1);
			Draw_ColoredString(180, 188, "rets, though you may not like", 255, 255, 255, 255, 1);
			Draw_ColoredString(180, 198, "what you find..", 255, 255, 255, 255, 1);
			break;
		case 2:
			menu_wh = Draw_CachePic("gfx/menu/nzp_warehouse");
			Draw_StretchPic(185, 40, menu_wh, 175, 100);
			Draw_ColoredString(180, 148, "Old Warehouse full of Zombies!", 255, 255, 255, 255, 1);
			Draw_ColoredString(180, 158, "Fight your way to the Power", 255, 255, 255, 255, 1);
			Draw_ColoredString(180, 168, "Switch through the Hordes!", 255, 255, 255, 255, 1);
			break;
		case 3:
			menu_ch = Draw_CachePic("gfx/menu/christmas_special");
			Draw_StretchPic(185, 40, menu_ch, 175, 100);
			Draw_ColoredString(180, 148, "No Santa this year. Though we're", 255, 255, 255, 255, 1);
			Draw_ColoredString(180, 158, "sure you will get presents from", 255, 255, 255, 255, 1);
			Draw_ColoredString(180, 168, "the undead! Will you accept them?", 255, 255, 255, 255, 1);
			break;
		case 4:
			menu_custom = Draw_CachePic("gfx/menu/custom");
			Draw_StretchPic(185, 40, menu_custom, 175, 100);
			Draw_ColoredString(180, 148, "Custom Maps made by Community", 255, 255, 255, 255, 1);
			Draw_ColoredString(180, 158, "Members on GitHub and on the", 255, 255, 255, 255, 1);
			Draw_ColoredString(180, 168, "NZ:P Forum!", 255, 255, 255, 255, 1);
			break;
	}
}


void M_SinglePlayer_Key (int key)
{
	switch (key)
	{

	case K_DOWNARROW:
		S_LocalSound ("sounds/menu/navigate.wav");
		if (++m_singleplayer_cursor >= SINGLEPLAYER_ITEMS)
			m_singleplayer_cursor = 0;
		break;

	case K_UPARROW:
		S_LocalSound ("sounds/menu/navigate.wav");
		if (--m_singleplayer_cursor < 0)
			m_singleplayer_cursor = SINGLEPLAYER_ITEMS - 1;
		break;

	case K_ENTER:
	case K_AUX1:
		m_entersound = true;

		switch (m_singleplayer_cursor)
		{
		case 0:
			key_dest = key_game;
			if (sv.active)
				Cbuf_AddText ("disconnect\n");
			Cbuf_AddText ("maxplayers 1\n");
			Cbuf_AddText ("map ndu\n");
			loadingScreen = 1;
			loadname2 = "ndu";
			loadnamespec = "Nacht der Untoten";
			break;

		case 1:
			key_dest = key_game;
			if (sv.active)
				Cbuf_AddText ("disconnect\n");
			Cbuf_AddText ("maxplayers 1\n");
			Cbuf_AddText ("map nzp_warehouse2\n");
			loadingScreen = 1;
			loadname2 = "nzp_warehouse2";
			loadnamespec = "Warehouse";
			break;

		case 2:
			key_dest = key_game;
			if (sv.active)
				Cbuf_AddText ("disconnect\n");
			Cbuf_AddText ("maxplayers 1\n");
			Cbuf_AddText ("map nzp_warehouse\n");
			loadingScreen = 1;
			loadname2 = "nzp_warehouse";
			loadnamespec = "Warehouse (Classic)";
			break;

		case 3:
			key_dest = key_game;
			if (sv.active)
				Cbuf_AddText ("disconnect\n");
			Cbuf_AddText ("maxplayers 1\n");
			Cbuf_AddText ("map christmas_special\n");
			loadingScreen = 1;
			loadname2 = "christmas_special";
			loadnamespec = "Christmas Special";
			break;

		case 4:
			M_Menu_CustomMaps_f ();
			break;
		case 5:
			M_Menu_Main_f ();
			break;
		}
		break;

	// b button
	case K_AUX2:
		M_Menu_Main_f();
		break;
	}
}


//=============================================================================
/* SINGLE PLAYER MENU */

int	m_map_cursor;
int	MAP_ITEMS;
int user_maps_num = 0;
int current_custom_map_page;
int custom_map_pages;
int multiplier;
char  user_levels[256][MAX_QPATH];

// UGHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHHH
// fuck windows
char* remove_windows_newlines(const char* line)
{
    const char* p = line;
    size_t len = strlen(line);
    char* result = (char*)malloc(len + 1);
    
    if (result == NULL) {
        return NULL;
    }
    
    char* q = result;
    
    for (size_t i = 0; i < len; i++) {
        if (p[i] == '\r') {
            continue;
        }
        *q++ = p[i];
    }
    
    *q = '\0';
    
    return result;
}

void Map_Finder(void)
{
	struct dirent *dp;
    DIR *dir = opendir(va("%s/maps", com_gamedir)); // Open the directory - dir contains a pointer to manage the dir

	if(dir < 0)
	{
		Sys_Error ("Map_Finder");
		return;
	}

	for (int i = 0; i < 50; i++) {
		custom_maps[i].occupied = false;
	}
	
	while(dp=readdir(dir))
	{
		
		if(dp->d_name[0] == '.')
		{
			continue;
		}

		if(!strcmp(COM_FileExtension(dp->d_name),"bsp")|| !strcmp(COM_FileExtension(dp->d_name),"BSP"))
	    {
			char ntype[32];

			COM_StripExtension(dp->d_name, ntype);
			custom_maps[user_maps_num].occupied = true;
			custom_maps[user_maps_num].map_name = malloc(sizeof(char)*32);
			sprintf(custom_maps[user_maps_num].map_name, "%s", ntype);

			char* 		setting_path;

			setting_path 								  	= malloc(sizeof(char)*64);
			custom_maps[user_maps_num].map_thumbnail_path 	= malloc(sizeof(char)*64);
			strcpy(setting_path, 									va("%s/maps/", com_gamedir));
			strcpy(custom_maps[user_maps_num].map_thumbnail_path, 	"gfx/menu/custom/");
			strcat(setting_path, 									custom_maps[user_maps_num].map_name);
			strcat(custom_maps[user_maps_num].map_thumbnail_path, 	custom_maps[user_maps_num].map_name);
			strcat(setting_path, ".txt");

			FILE    *setting_file;			
			setting_file = fopen(setting_path, "rb");
			if (setting_file != NULL) {

				fseek(setting_file, 0L, SEEK_END);
				size_t sz = ftell(setting_file);
				fseek(setting_file, 0L, SEEK_SET);

				int state;
				state = 0;
				int value;

				custom_maps[user_maps_num].map_name_pretty = malloc(sizeof(char)*32);
				custom_maps[user_maps_num].map_desc_1 = malloc(sizeof(char)*40);
				custom_maps[user_maps_num].map_desc_2 = malloc(sizeof(char)*40);
				custom_maps[user_maps_num].map_desc_3 = malloc(sizeof(char)*40);
				custom_maps[user_maps_num].map_desc_4 = malloc(sizeof(char)*40);
				custom_maps[user_maps_num].map_desc_5 = malloc(sizeof(char)*40);
				custom_maps[user_maps_num].map_desc_6 = malloc(sizeof(char)*40);
				custom_maps[user_maps_num].map_desc_7 = malloc(sizeof(char)*40);
				custom_maps[user_maps_num].map_desc_8 = malloc(sizeof(char)*40);
				custom_maps[user_maps_num].map_author = malloc(sizeof(char)*40);

				char* buffer = (char*)calloc(sz+1, sizeof(char));
				fread(buffer, sz, 1, setting_file);

				strtok(buffer, "\n");
				while(buffer != NULL) {
					switch(state) {
						case 0: strcpy(custom_maps[user_maps_num].map_name_pretty, remove_windows_newlines(buffer)); break;
						case 1: strcpy(custom_maps[user_maps_num].map_desc_1, remove_windows_newlines(buffer)); break;
						case 2: strcpy(custom_maps[user_maps_num].map_desc_2, remove_windows_newlines(buffer)); break;
						case 3: strcpy(custom_maps[user_maps_num].map_desc_3, remove_windows_newlines(buffer)); break;
						case 4: strcpy(custom_maps[user_maps_num].map_desc_4, remove_windows_newlines(buffer)); break;
						case 5: strcpy(custom_maps[user_maps_num].map_desc_5, remove_windows_newlines(buffer)); break;
						case 6: strcpy(custom_maps[user_maps_num].map_desc_6, remove_windows_newlines(buffer)); break;
						case 7: strcpy(custom_maps[user_maps_num].map_desc_7, remove_windows_newlines(buffer)); break;
						case 8: strcpy(custom_maps[user_maps_num].map_desc_8, remove_windows_newlines(buffer)); break;
						case 9: strcpy(custom_maps[user_maps_num].map_author, remove_windows_newlines(buffer)); break;
						case 10: value = 0; sscanf(remove_windows_newlines(buffer), "%d", &value); custom_maps[user_maps_num].map_use_thumbnail = value; break;
						case 11: value = 0; sscanf(remove_windows_newlines(buffer), "%d", &value); custom_maps[user_maps_num].map_allow_game_settings = value; break;
						default: break;
					}
					state++;
					buffer = strtok(NULL, "\n");
				}
				free(buffer);
				buffer = 0;
				fclose(setting_file);
			}
			user_maps_num++;
		}
	}
    closedir(dir); // close the handle (pointer)
	custom_map_pages = (int)ceil((double)(user_maps_num)/15);
}

void M_Menu_CustomMaps_f (void)
{
	key_dest = key_menu;
	m_state = m_custommaps;
	m_entersound = true;
	MAP_ITEMS = 13;
	current_custom_map_page = 1;
}


void M_Menu_CustomMaps_Draw (void)
{
	// Background
	menu_bk = Draw_CachePic("gfx/menu/menu_background");
	Draw_StretchPic(0, 0, menu_bk, 400, 240);

	// Fill black to make everything easier to see
	Draw_FillByColor(0, 0, 400, 240, 0, 0, 0, 102);

	// Header
	Draw_ColoredString(5, 5, "CUSTOM MAPS", 255, 255, 255, 255, 2);

	int 	line_increment;

	line_increment = 0;

	if (current_custom_map_page > 1)
		multiplier = (current_custom_map_page - 1) * 15;
	else
		multiplier = 0;

	for (int i = 0; i < 15; i++) {
		if (custom_maps[i + multiplier].occupied == false)
			continue;

		if (m_map_cursor == i) {

			if (custom_maps[i + multiplier].map_use_thumbnail == 1) {
				menu_cuthum = Draw_CachePic(custom_maps[i + multiplier].map_thumbnail_path);
				if (menu_cuthum != NULL) {
					Draw_StretchPic(185, 40, menu_cuthum, 175, 100);
				}
			}
			
			if (custom_maps[i + multiplier].map_name_pretty != 0)
				Draw_ColoredString(5, 40 + (10 * i), custom_maps[i + multiplier].map_name_pretty, 255, 0, 0, 255, 1);
			else
				Draw_ColoredString(5, 40 + (10 * i), custom_maps[i + multiplier].map_name, 255, 0, 0, 255, 1);

			if (custom_maps[i + multiplier].map_desc_1 != 0) {
				if (strcmp(custom_maps[i + multiplier].map_desc_1, " ") != 0) {
					Draw_ColoredString(180, 148, custom_maps[i + multiplier].map_desc_1, 255, 255, 255, 255, 1);
				}
			}
			if (custom_maps[i + multiplier].map_desc_2 != 0) {
				if (strcmp(custom_maps[i + multiplier].map_desc_2, " ") != 0) {
					line_increment++;
					Draw_ColoredString(180, 158, custom_maps[i + multiplier].map_desc_2, 255, 255, 255, 255, 1);
				}
			}
			if (custom_maps[i + multiplier].map_desc_3 != 0) {
				if (strcmp(custom_maps[i + multiplier].map_desc_3, " ") != 0) {
					line_increment++;
					Draw_ColoredString(180, 168, custom_maps[i + multiplier].map_desc_3, 255, 255, 255, 255, 1);
				}
			}
			if (custom_maps[i + multiplier].map_desc_4 != 0) {
				if (strcmp(custom_maps[i + multiplier].map_desc_4, " ") != 0) {
					line_increment++;
					Draw_ColoredString(180, 178, custom_maps[i + multiplier].map_desc_4, 255, 255, 255, 255, 1);
				}
			}
			if (custom_maps[i + multiplier].map_desc_5 != 0) {
				if (strcmp(custom_maps[i + multiplier].map_desc_5, " ") != 0) {
					line_increment++;
					Draw_ColoredString(180, 188, custom_maps[i + multiplier].map_desc_5, 255, 255, 255, 255, 1);
				}
			}
			if (custom_maps[i + multiplier].map_desc_6 != 0) {
				if (strcmp(custom_maps[i + multiplier].map_desc_6, " ") != 0) {
					line_increment++;
					Draw_ColoredString(180, 198, custom_maps[i + multiplier].map_desc_6, 255, 255, 255, 255, 1);
				}
			}
			if (custom_maps[i + multiplier].map_desc_7 != 0) {
				if (strcmp(custom_maps[i + multiplier].map_desc_7, " ") != 0) {
					line_increment++;
					Draw_ColoredString(180, 208, custom_maps[i + multiplier].map_desc_7, 255, 255, 255, 255, 1);
				}
			}
			if (custom_maps[i + multiplier].map_desc_8 != 0) {
				if (strcmp(custom_maps[i + multiplier].map_desc_8, " ") != 0) {
					line_increment++;
					Draw_ColoredString(180, 218, custom_maps[i + multiplier].map_desc_8, 255, 255, 255, 255, 1);
				}
			}
			if (custom_maps[i + multiplier].map_author != 0) {
				if (strcmp(custom_maps[i + multiplier].map_author, " ") != 0) {
					int y = 158 + (10 * line_increment);
					Draw_ColoredString(180, y, custom_maps[i + multiplier].map_author, 255, 255, 0, 255, 1);
				}
			}
		} else {
			if (custom_maps[i + multiplier].map_name_pretty != 0)
				Draw_ColoredString(5, 40 + (10 * i), custom_maps[i + multiplier].map_name_pretty, 255, 255, 255, 255, 1);
			else
				Draw_ColoredString(5, 40 + (10 * i), custom_maps[i + multiplier].map_name, 255, 255, 255, 255, 1);
		}
	}

	if (current_custom_map_page != custom_map_pages) {
		if (m_map_cursor == 15)
			Draw_ColoredString(5, 210, "Next Page", 255, 0, 0, 255, 1);
		else
			Draw_ColoredString(5, 210, "Next Page", 255, 255, 255, 255, 1);
	} else {
		Draw_ColoredString(5, 210, "Next Page", 128, 128, 128, 255, 1);
	}

	if (current_custom_map_page != 1) {
		if (m_map_cursor == 16)
			Draw_ColoredString(5, 220, "Previous Page", 255, 0, 0, 255, 1);
		else
			Draw_ColoredString(5, 220, "Previous Page", 255, 255, 255, 255, 1);
	} else {
		Draw_ColoredString(5, 220, "Previous Page", 128, 128, 128, 255, 1);
	}



	if (m_map_cursor == 17)
		Draw_ColoredString(5, 230, "Back", 255, 0, 0, 255, 1);
	else
		Draw_ColoredString(5, 230, "Back", 255, 255, 255, 255, 1);
}


void M_Menu_CustomMaps_Key (int key)
{
	switch (key)
	{
		case K_ESCAPE:
		case K_AUX2:
			M_Menu_SinglePlayer_f ();
			break;
		case K_DOWNARROW:
			S_LocalSound ("sounds/menu/navigate.wav");

			m_map_cursor++;

			if (m_map_cursor < 14 && custom_maps[m_map_cursor + multiplier].occupied == false) {
				m_map_cursor = 15;
			}

			if (m_map_cursor == 15 && current_custom_map_page == custom_map_pages)
				m_map_cursor = 16;
			
			if (m_map_cursor == 16 && current_custom_map_page == 1)
				m_map_cursor = 17;

			if (m_map_cursor >= 18)
				m_map_cursor = 0;
			break;
		case K_UPARROW:
			S_LocalSound ("sounds/menu/navigate.wav");

			m_map_cursor--;

			if (m_map_cursor < 0)
				m_map_cursor = 17;

			if (m_map_cursor == 16 && current_custom_map_page == 1)
				m_map_cursor = 15;

			if (m_map_cursor == 15 && current_custom_map_page == custom_map_pages)
				m_map_cursor = 14;

			if (m_map_cursor <= 14 && custom_maps[m_map_cursor + multiplier].occupied == false) {
				for (int i = 14; i > -1; i--) {
					if (custom_maps[i + multiplier].occupied == true) {
						m_map_cursor = i;
						break;
					}
				}
			}
			break;
		case K_ENTER:
		case K_AUX1:
			m_entersound = true;
			if (m_map_cursor == 17) {
				M_Menu_SinglePlayer_f ();
			} else if (m_map_cursor == 16) {
				current_custom_map_page--;
				m_map_cursor = 0;
			} else if (m_map_cursor == 15) {
				current_custom_map_page++;
				m_map_cursor = 0;
			} else
			{
				key_dest = key_game;
				if (sv.active)
					Cbuf_AddText ("disconnect\n");
				Cbuf_AddText ("maxplayers 1\n");
				Cbuf_AddText (va("map %s\n", custom_maps[m_map_cursor + multiplier].map_name));
				loadingScreen = 1;
				loadname2 = custom_maps[m_map_cursor + multiplier].map_name;
				if (custom_maps[m_map_cursor + multiplier].map_name_pretty != 0)
					loadnamespec = custom_maps[m_map_cursor + multiplier].map_name_pretty;
				else
					loadnamespec = custom_maps[m_map_cursor + multiplier].map_name;
			}
			break;
	}
}

//=============================================================================
/* OPTIONS MENU */

#define	OPTIONS_ITEMS	13
#define	SLIDER_RANGE	10

int		options_cursor;

void M_Menu_Options_f (void)
{
	m_state = m_options;
	m_entersound = true;
}


void M_AdjustSliders (int dir)
{
	S_LocalSound ("misc/menu3.wav");

	switch (options_cursor)
	{
	case 3:	// screen size
		scr_viewsize.value += dir * 10;
		if (scr_viewsize.value < 30)
			scr_viewsize.value = 30;
		if (scr_viewsize.value > 120)
			scr_viewsize.value = 120;
		Cvar_SetValue ("viewsize", scr_viewsize.value);
		break;
	case 4:	// gamma
		v_gamma.value -= dir * 0.05;
		if (v_gamma.value < 0.5)
			v_gamma.value = 0.5;
		if (v_gamma.value > 1)
			v_gamma.value = 1;
		Cvar_SetValue ("gamma", v_gamma.value);
		break;
	case 5:	// mouse speed
		sensitivity.value += dir * 0.5;
		if (sensitivity.value < 1)
			sensitivity.value = 1;
		if (sensitivity.value > 11)
			sensitivity.value = 11;
		Cvar_SetValue ("sensitivity", sensitivity.value);
		break;
	case 6:	// music volume
		bgmvolume.value += dir * 0.1;
		if (bgmvolume.value < 0)
			bgmvolume.value = 0;
		if (bgmvolume.value > 1)
			bgmvolume.value = 1;
		Cvar_SetValue ("bgmvolume", bgmvolume.value);
		break;
	case 7:	// sfx volume
		volume.value += dir * 0.1;
		if (volume.value < 0)
			volume.value = 0;
		if (volume.value > 1)
			volume.value = 1;
		Cvar_SetValue ("volume", volume.value);
		break;

	case 8:	// allways run
		if (cl_forwardspeed > 200)
		{
			cl_forwardspeed = 200;
			Cvar_SetValue ("cl_backspeed", 200);
		}
		else
		{
			cl_forwardspeed = 400;
			Cvar_SetValue ("cl_backspeed", 400);
		}
		break;

	case 9:	// invert mouse
		Cvar_SetValue ("m_pitch", -m_pitch.value);
		break;

	case 10:	// lookspring
		Cvar_SetValue ("lookspring", !lookspring.value);
		break;

	case 11:	// lookstrafe
		Cvar_SetValue ("lookstrafe", !lookstrafe.value);
		break;

	case 12: // anub swap
		Cvar_SetValue ("in_anub_mode", !in_anub_mode.value);
		break;
	}
}


void M_DrawSlider (int x, int y, float range)
{
	int	i;

	if (range < 0)
		range = 0;
	if (range > 1)
		range = 1;
	M_DrawCharacter (x-8, y, 128);
	for (i=0 ; i<SLIDER_RANGE ; i++)
		M_DrawCharacter (x + i*8, y, 129);
	M_DrawCharacter (x+i*8, y, 130);
	M_DrawCharacter (x + (SLIDER_RANGE-1)*8 * range, y, 131);
}

void M_DrawCheckbox (int x, int y, int on)
{
#if 0
	if (on)
		M_DrawCharacter (x, y, 131);
	else
		M_DrawCharacter (x, y, 129);
#endif
	if (on)
		M_Print (x, y, "on");
	else
		M_Print (x, y, "off");
}

void M_Options_Draw (void)
{
	if (key_dest != key_menu_pause)
		Draw_Pic (0, 0, menu_bk);

	float		r;

	M_Print (16, 32, "    Customize controls");
	M_Print (16, 40, "         Go to console");
	M_Print (16, 48, "     Reset to defaults");

	M_Print (16, 56, "           Screen size");
	r = (scr_viewsize.value - 30) / (120 - 30);
	M_DrawSlider (220, 56, r);

	M_Print (16, 64, "            Brightness");
	r = (1.0 - v_gamma.value) / 0.5;
	M_DrawSlider (220, 64, r);

	M_Print (16, 72, "           Mouse Speed");
	r = (sensitivity.value - 1)/10;
	M_DrawSlider (220, 72, r);

	M_Print (16, 80, "       CD Music Volume");
	r = bgmvolume.value;
	M_DrawSlider (220, 80, r);

	M_Print (16, 88, "          Sound Volume");
	r = volume.value;
	M_DrawSlider (220, 88, r);

	M_Print (16, 96,  "            Always Run");
	M_DrawCheckbox (220, 96, cl_forwardspeed > 200);

	M_Print (16, 104, "          Invert Mouse");
	M_DrawCheckbox (220, 104, m_pitch.value < 0);

	M_Print (16, 112, "            Lookspring");
	M_DrawCheckbox (220, 112, lookspring.value);

	M_Print (16, 120, "            Lookstrafe");
	M_DrawCheckbox (220, 120, lookstrafe.value);

	M_Print (16, 128, "Swap C-Nub and C-Stick");
	M_DrawCheckbox (220, 128, in_anub_mode.value);

	if (vid_menudrawfn)
		M_Print (16, 136, "         Video Options");

// cursor
	M_DrawCharacter (200, 32 + options_cursor*8, 12+((int)(realtime*4)&1));
}

extern qboolean console_enabled;
void M_Options_Key (int k)
{
	switch (k)
	{
	case K_ENTER:
	case K_AUX1:
		m_entersound = true;
		switch (options_cursor)
		{
		case 0:
			M_Menu_Keys_f ();
			break;
		case 1:
			console_enabled = true;
			m_state = m_none;
			Con_Printf("\nPress SELECT to open keyboard.\n\n");
			Con_ToggleConsole_f ();
			break;
		case 2:
			Cbuf_AddText ("exec default.cfg\n");
			break;
		case 13:
			M_Menu_Video_f ();
			break;
		default:
			M_AdjustSliders (1);
			break;
		}
		return;

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		options_cursor--;
		if (options_cursor < 0)
			options_cursor = OPTIONS_ITEMS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		options_cursor++;
		if (options_cursor >= OPTIONS_ITEMS)
			options_cursor = 0;
		break;

	case K_LEFTARROW:
		M_AdjustSliders (-1);
		break;

	case K_RIGHTARROW:
		M_AdjustSliders (1);
		break;

	case K_ESCAPE:
	case K_AUX2:
		if (key_dest == key_menu_pause)
			M_Paused_Menu_f();
		else
			M_Menu_Main_f ();
		break;
	}

	if (options_cursor == 13 && vid_menudrawfn == NULL)
	{
		if (k == K_UPARROW)
			options_cursor = 12;
		else
			options_cursor = 0;
	}
}

//=============================================================================
/* KEYS MENU */

char *bindnames[][2] =
{
{"+attack", 		"attack"},
{"impulse 10", 		"change weapon"},
{"+jump", 			"jump / swim up"},
{"+forward", 		"walk forward"},
{"+back", 			"backpedal"},
{"+left", 			"turn left"},
{"+right", 			"turn right"},
{"+speed", 			"run"},
{"+moveleft", 		"step left"},
{"+moveright", 		"step right"},
{"+strafe", 		"sidestep"},
{"+lookup", 		"look up"},
{"+lookdown", 		"look down"},
{"centerview", 		"center view"},
{"+mlook", 			"mouse look"},
{"+klook", 			"keyboard look"},
{"+moveup",			"swim up"},
{"+movedown",		"swim down"}
};

#define	NUMCOMMANDS	(sizeof(bindnames)/sizeof(bindnames[0]))

int		keys_cursor;
int		bind_grab;

void M_Menu_Keys_f (void)
{
	m_state = m_keys;
	m_entersound = true;
}


void M_FindKeysForCommand (char *command, int *twokeys)
{
	int		count;
	int		j;
	int		l;
	char	*b;

	twokeys[0] = twokeys[1] = -1;
	l = strlen(command);
	count = 0;

	for (j=0 ; j<256 ; j++)
	{
		b = keybindings[j];
		if (!b)
			continue;
		if (!strncmp (b, command, l) )
		{
			twokeys[count] = j;
			count++;
			if (count == 2)
				break;
		}
	}
}

void M_UnbindCommand (char *command)
{
	int		j;
	int		l;
	char	*b;

	l = strlen(command);

	for (j=0 ; j<256 ; j++)
	{
		b = keybindings[j];
		if (!b)
			continue;
		if (!strncmp (b, command, l) )
			Key_SetBinding (j, "");
	}
}


void M_Keys_Draw (void)
{
	int		i;
	int		keys[2];
	char	*name;
	int		x, y;

	if (key_dest != key_menu_pause)
		Draw_Pic (0, 0, menu_bk);

	if (bind_grab)
		M_Print (12, 32, "Press a key or button for this action");
	else
		M_Print (18, 32, "Enter to change, backspace to clear");

// search for known bindings
	for (i=0 ; i<NUMCOMMANDS ; i++)
	{
		y = 48 + 8*i;

		M_Print (16, y, bindnames[i][1]);

		M_FindKeysForCommand (bindnames[i][0], keys);

		if (keys[0] == -1)
		{
			M_Print (140, y, "???");
		}
		else
		{
			name = Key_KeynumToString (keys[0]);
			M_Print (140, y, name);
			x = strlen(name) * 8;
			if (keys[1] != -1)
			{
				M_Print (140 + x + 8, y, "or");
				M_Print (140 + x + 32, y, Key_KeynumToString (keys[1]));
			}
		}
	}

	if (bind_grab)
		M_DrawCharacter (130, 48 + keys_cursor*8, '=');
	else
		M_DrawCharacter (130, 48 + keys_cursor*8, 12+((int)(realtime*4)&1));
}


void M_Keys_Key (int k)
{
	char	cmd[80];
	int		keys[2];

	if (bind_grab)
	{	// defining a key
		S_LocalSound ("misc/menu1.wav");
		if (k == K_ESCAPE)
		{
			bind_grab = false;
		}
		else if (k != '`')
		{
			sprintf (cmd, "bind \"%s\" \"%s\"\n", Key_KeynumToString (k), bindnames[keys_cursor][0]);
			Cbuf_InsertText (cmd);
		}

		bind_grab = false;
		return;
	}

	switch (k)
	{
	case K_ESCAPE:
	case K_AUX2:
		M_Menu_Options_f();
		break;

	case K_LEFTARROW:
	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		keys_cursor--;
		if (keys_cursor < 0)
			keys_cursor = NUMCOMMANDS-1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		S_LocalSound ("misc/menu1.wav");
		keys_cursor++;
		if (keys_cursor >= NUMCOMMANDS)
			keys_cursor = 0;
		break;

	case K_ENTER:		// go into bind mode
	case K_AUX1:
		M_FindKeysForCommand (bindnames[keys_cursor][0], keys);
		S_LocalSound ("misc/menu2.wav");
		if (keys[1] != -1)
			M_UnbindCommand (bindnames[keys_cursor][0]);
		bind_grab = true;
		break;

	case K_BACKSPACE:		// delete bindings
	case K_DEL:				// delete bindings
		S_LocalSound ("misc/menu2.wav");
		M_UnbindCommand (bindnames[keys_cursor][0]);
		break;
	}
}

//=============================================================================
/* VIDEO MENU */

void M_Menu_Video_f (void)
{
	key_dest = key_menu;
	m_state = m_video;
	m_entersound = true;
}


void M_Video_Draw (void)
{
	(*vid_menudrawfn) ();
}


void M_Video_Key (int key)
{
	(*vid_menukeyfn) (key);
}

//=============================================================================
/* QUIT MENU */

int		msgNumber;
int		m_quit_prevstate;
qboolean	wasInMenus;

#ifndef	_WIN32
char *quitMessage [] = 
{
/* .........1.........2.... */
  "   Hope you enjoyed     ",
  "  this tech demo you    ",
  "   pirate (press A)     ",
  "                        ",
};
#endif

void M_Menu_Quit_f (void)
{
	if (m_state == m_quit)
		return;
	wasInMenus = (key_dest == key_menu);
	key_dest = key_menu;
	m_quit_prevstate = m_state;
	m_state = m_quit;
	m_entersound = true;
	msgNumber = 0;
}

extern bool game_running;
void M_Quit_Key (int key)
{
	switch (key)
	{

	case K_AUX2:
		M_Menu_Main_f();
		break;

	case K_AUX1:
		game_running = false;
		break;
	}
}


void M_Quit_Draw (void)
{
	if (wasInMenus)
	{
		m_state = m_quit_prevstate;
		m_recursiveDraw = true;
		M_Draw ();
		m_state = m_quit;
	}

	M_DrawTextBox (56, 76, 24, 4);
	M_Print (64, 84,  quitMessage[0]);
	M_Print (64, 92,  quitMessage[1]);
	M_Print (64, 100, quitMessage[2]);
	M_Print (64, 108, quitMessage[3]);
}


//=============================================================================
/* GAME OPTIONS MENU */

typedef struct
{
	char	*name;
	char	*description;
} level_t;

level_t		levels[] =
{
	{"start", "Entrance"},	// 0

	{"e1m1", "Slipgate Complex"},				// 1
	{"e1m2", "Castle of the Damned"},
	{"e1m3", "The Necropolis"},
	{"e1m4", "The Grisly Grotto"},
	{"e1m5", "Gloom Keep"},
	{"e1m6", "The Door To Chthon"},
	{"e1m7", "The House of Chthon"},
	{"e1m8", "Ziggurat Vertigo"},

	{"e2m1", "The Installation"},				// 9
	{"e2m2", "Ogre Citadel"},
	{"e2m3", "Crypt of Decay"},
	{"e2m4", "The Ebon Fortress"},
	{"e2m5", "The Wizard's Manse"},
	{"e2m6", "The Dismal Oubliette"},
	{"e2m7", "Underearth"},

	{"e3m1", "Termination Central"},			// 16
	{"e3m2", "The Vaults of Zin"},
	{"e3m3", "The Tomb of Terror"},
	{"e3m4", "Satan's Dark Delight"},
	{"e3m5", "Wind Tunnels"},
	{"e3m6", "Chambers of Torment"},
	{"e3m7", "The Haunted Halls"},

	{"e4m1", "The Sewage System"},				// 23
	{"e4m2", "The Tower of Despair"},
	{"e4m3", "The Elder God Shrine"},
	{"e4m4", "The Palace of Hate"},
	{"e4m5", "Hell's Atrium"},
	{"e4m6", "The Pain Maze"},
	{"e4m7", "Azure Agony"},
	{"e4m8", "The Nameless City"},

	{"end", "Shub-Niggurath's Pit"},			// 31

	{"dm1", "Place of Two Deaths"},				// 32
	{"dm2", "Claustrophobopolis"},
	{"dm3", "The Abandoned Base"},
	{"dm4", "The Bad Place"},
	{"dm5", "The Cistern"},
	{"dm6", "The Dark Zone"}
};

//MED 01/06/97 added hipnotic levels
level_t     hipnoticlevels[] =
{
   {"start", "Command HQ"},  // 0

   {"hip1m1", "The Pumping Station"},          // 1
   {"hip1m2", "Storage Facility"},
   {"hip1m3", "The Lost Mine"},
   {"hip1m4", "Research Facility"},
   {"hip1m5", "Military Complex"},

   {"hip2m1", "Ancient Realms"},          // 6
   {"hip2m2", "The Black Cathedral"},
   {"hip2m3", "The Catacombs"},
   {"hip2m4", "The Crypt"},
   {"hip2m5", "Mortum's Keep"},
   {"hip2m6", "The Gremlin's Domain"},

   {"hip3m1", "Tur Torment"},       // 12
   {"hip3m2", "Pandemonium"},
   {"hip3m3", "Limbo"},
   {"hip3m4", "The Gauntlet"},

   {"hipend", "Armagon's Lair"},       // 16

   {"hipdm1", "The Edge of Oblivion"}           // 17
};

//PGM 01/07/97 added rogue levels
//PGM 03/02/97 added dmatch level
level_t		roguelevels[] =
{
	{"start",	"Split Decision"},
	{"r1m1",	"Deviant's Domain"},
	{"r1m2",	"Dread Portal"},
	{"r1m3",	"Judgement Call"},
	{"r1m4",	"Cave of Death"},
	{"r1m5",	"Towers of Wrath"},
	{"r1m6",	"Temple of Pain"},
	{"r1m7",	"Tomb of the Overlord"},
	{"r2m1",	"Tempus Fugit"},
	{"r2m2",	"Elemental Fury I"},
	{"r2m3",	"Elemental Fury II"},
	{"r2m4",	"Curse of Osiris"},
	{"r2m5",	"Wizard's Keep"},
	{"r2m6",	"Blood Sacrifice"},
	{"r2m7",	"Last Bastion"},
	{"r2m8",	"Source of Evil"},
	{"ctf1",    "Division of Change"}
};

typedef struct
{
	char	*description;
	int		firstLevel;
	int		levels;
} episode_t;

episode_t	episodes[] =
{
	{"Welcome to Quake", 0, 1},
	{"Doomed Dimension", 1, 8},
	{"Realm of Black Magic", 9, 7},
	{"Netherworld", 16, 7},
	{"The Elder World", 23, 8},
	{"Final Level", 31, 1},
	{"Deathmatch Arena", 32, 6}
};

//MED 01/06/97  added hipnotic episodes
episode_t   hipnoticepisodes[] =
{
   {"Scourge of Armagon", 0, 1},
   {"Fortress of the Dead", 1, 5},
   {"Dominion of Darkness", 6, 6},
   {"The Rift", 12, 4},
   {"Final Level", 16, 1},
   {"Deathmatch Arena", 17, 1}
};

//PGM 01/07/97 added rogue episodes
//PGM 03/02/97 added dmatch episode
episode_t	rogueepisodes[] =
{
	{"Introduction", 0, 1},
	{"Hell's Fortress", 1, 7},
	{"Corridors of Time", 8, 8},
	{"Deathmatch Arena", 16, 1}
};

int	startepisode;
int	startlevel;
int maxplayers;
qboolean m_serverInfoMessage = false;
double m_serverInfoMessageTime;

void M_Menu_GameOptions_f (void)
{
	key_dest = key_menu;
	m_state = m_gameoptions;
	m_entersound = true;
	if (maxplayers == 0)
		maxplayers = svs.maxclients;
	if (maxplayers < 2)
		maxplayers = svs.maxclientslimit;
}


int gameoptions_cursor_table[] = {40, 56, 64, 72, 80, 88, 96, 112, 120};
#define	NUM_GAMEOPTIONS	9
int		gameoptions_cursor;

void M_GameOptions_Draw (void)
{
	int		x;

	M_DrawTextBox (152, 32, 10, 1);
	M_Print (160, 40, "begin game");

	M_Print (0, 56, "      Max players");
	M_Print (160, 56, va("%i", maxplayers) );

	M_Print (0, 64, "        Game Type");
	if (coop.value)
		M_Print (160, 64, "Cooperative");
	else
		M_Print (160, 64, "Deathmatch");

	M_Print (0, 72, "        Teamplay");
	{
		char *msg;

		switch((int)teamplay.value)
		{
			case 1: msg = "No Friendly Fire"; break;
			case 2: msg = "Friendly Fire"; break;
			default: msg = "Off"; break;
		}
		M_Print (160, 72, msg);
	}

	M_Print (0, 80, "            Skill");
	if (skill.value == 0)
		M_Print (160, 80, "Easy difficulty");
	else if (skill.value == 1)
		M_Print (160, 80, "Normal difficulty");
	else if (skill.value == 2)
		M_Print (160, 80, "Hard difficulty");
	else
		M_Print (160, 80, "Nightmare difficulty");

	M_Print (0, 88, "       Frag Limit");
	if (fraglimit.value == 0)
		M_Print (160, 88, "none");
	else
		M_Print (160, 88, va("%i frags", (int)fraglimit.value));

	M_Print (0, 96, "       Time Limit");
	if (timelimit.value == 0)
		M_Print (160, 96, "none");
	else
		M_Print (160, 96, va("%i minutes", (int)timelimit.value));

	M_Print (0, 112, "         Episode");
	M_Print (160, 112, episodes[startepisode].description);

	M_Print (0, 120, "           Level");
    M_Print (160, 120, levels[episodes[startepisode].firstLevel + startlevel].description);
    M_Print (160, 128, levels[episodes[startepisode].firstLevel + startlevel].name);

// line cursor
	M_DrawCharacter (144, gameoptions_cursor_table[gameoptions_cursor], 12+((int)(realtime*4)&1));

	if (m_serverInfoMessage)
	{
		if ((realtime - m_serverInfoMessageTime) < 5.0)
		{
			x = (320-26*8)/2;
			M_DrawTextBox (x, 138, 24, 4);
			x += 8;
			M_Print (x, 146, "  More than 4 players   ");
			M_Print (x, 154, " requires using command ");
			M_Print (x, 162, "line parameters; please ");
			M_Print (x, 170, "   see techinfo.txt.    ");
		}
		else
		{
			m_serverInfoMessage = false;
		}
	}
}


void M_NetStart_Change (int dir)
{
	int count;

	switch (gameoptions_cursor)
	{
	case 1:
		maxplayers += dir;
		if (maxplayers > svs.maxclientslimit)
		{
			maxplayers = svs.maxclientslimit;
			m_serverInfoMessage = true;
			m_serverInfoMessageTime = realtime;
		}
		if (maxplayers < 2)
			maxplayers = 2;
		break;

	case 2:
		Cvar_SetValue ("coop", coop.value ? 0 : 1);
		break;

	case 3:
		count = 2;

		Cvar_SetValue ("teamplay", teamplay.value + dir);
		if (teamplay.value > count)
			Cvar_SetValue ("teamplay", 0);
		else if (teamplay.value < 0)
			Cvar_SetValue ("teamplay", count);
		break;

	case 4:
		Cvar_SetValue ("skill", skill.value + dir);
		if (skill.value > 3)
			Cvar_SetValue ("skill", 0);
		if (skill.value < 0)
			Cvar_SetValue ("skill", 3);
		break;

	case 5:
		Cvar_SetValue ("fraglimit", fraglimit.value + dir*10);
		if (fraglimit.value > 100)
			Cvar_SetValue ("fraglimit", 0);
		if (fraglimit.value < 0)
			Cvar_SetValue ("fraglimit", 100);
		break;

	case 6:
		Cvar_SetValue ("timelimit", timelimit.value + dir*5);
		if (timelimit.value > 60)
			Cvar_SetValue ("timelimit", 0);
		if (timelimit.value < 0)
			Cvar_SetValue ("timelimit", 60);
		break;

	case 7:
		startepisode += dir;
		count = 2;

		if (startepisode < 0)
			startepisode = count - 1;

		if (startepisode >= count)
			startepisode = 0;

		startlevel = 0;
		break;

	case 8:
		startlevel += dir;
		count = episodes[startepisode].levels;

		if (startlevel < 0)
			startlevel = count - 1;

		if (startlevel >= count)
			startlevel = 0;
		break;
	}
}

void M_GameOptions_Key (int key)
{
	switch (key)
	{

	case K_UPARROW:
		S_LocalSound ("misc/menu1.wav");
		gameoptions_cursor--;
		if (gameoptions_cursor < 0)
			gameoptions_cursor = NUM_GAMEOPTIONS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("misc/menu1.wav");
		gameoptions_cursor++;
		if (gameoptions_cursor >= NUM_GAMEOPTIONS)
			gameoptions_cursor = 0;
		break;

	case K_LEFTARROW:
		if (gameoptions_cursor == 0)
			break;
		S_LocalSound ("misc/menu3.wav");
		M_NetStart_Change (-1);
		break;

	case K_RIGHTARROW:
		if (gameoptions_cursor == 0)
			break;
		S_LocalSound ("misc/menu3.wav");
		M_NetStart_Change (1);
		break;

	case K_ENTER:
	case K_AUX1:
		S_LocalSound ("misc/menu2.wav");
		if (gameoptions_cursor == 0)
		{
			if (sv.active)
				Cbuf_AddText ("disconnect\n");
			Cbuf_AddText ("listen 0\n");	// so host_netport will be re-examined
			Cbuf_AddText ( va ("maxplayers %u\n", maxplayers) );
			SCR_BeginLoadingPlaque ();
			Cbuf_AddText ( va ("map %s\n", levels[episodes[startepisode].firstLevel + startlevel].name) );

			return;
		}

		M_NetStart_Change (1);
		break;
	}
}

//=============================================================================
/* Menu Subsystem */


void M_Init (void)
{
	Cmd_AddCommand ("togglemenu", M_ToggleMenu_f);

	Cmd_AddCommand ("menu_main", M_Menu_Main_f);
	Cmd_AddCommand ("menu_singleplayer", M_Menu_SinglePlayer_f);
	Cmd_AddCommand ("menu_options", M_Menu_Options_f);
	Cmd_AddCommand ("menu_keys", M_Menu_Keys_f);
	Cmd_AddCommand ("menu_video", M_Menu_Video_f);
	Cmd_AddCommand ("menu_quit", M_Menu_Quit_f);

	// Snag the game version
	long length;
	FILE* f = fopen(va("%s/version.txt", com_gamedir), "rb");

	if (f)
	{
		fseek (f, 0, SEEK_END);
		length = ftell (f);
		fseek (f, 0, SEEK_SET);
		game_build_date = malloc(length);

		if (game_build_date)
			fread (game_build_date, 1, length, f);

		fclose (f);
	} else {
		game_build_date = "version.txt not found.";
	}

	Map_Finder();
}


void M_Draw (void)
{
	if (m_state == m_none || key_dest != key_menu && key_dest != key_menu_pause)
		return;

	if (!m_recursiveDraw)
	{
		scr_copyeverything = 1;

		if (scr_con_current)
		{
			Draw_ConsoleBackground (vid.height);
			VID_UnlockBuffer ();
			S_ExtraUpdate ();
			VID_LockBuffer ();
		}
		else
			Draw_FadeScreen ();

		scr_fullupdate = 0;
	}
	else
	{
		m_recursiveDraw = false;
	}

	switch (m_state)
	{
	case m_none:
		break;

	case m_start:
		M_Start_Menu_Draw();
		break;

	case m_paused_menu:
		M_Paused_Menu_Draw();
		break;

	case m_main:
		M_Main_Draw ();
		break;

	case m_singleplayer:
		M_SinglePlayer_Draw ();
		break;

	case m_options:
		M_Options_Draw ();
		break;

	case m_keys:
		M_Keys_Draw ();
		break;

	case m_video:
		M_Video_Draw ();
		break;

	case m_quit:
		M_Quit_Draw ();
		break;

	case m_restart:
		M_Restart_Draw ();
		break;

	case m_credits:
		M_Credits_Draw ();
		break;

	case m_exit:
		M_Exit_Draw ();
		break;

	case m_gameoptions:
		M_GameOptions_Draw ();
		break;

	case m_custommaps:
		M_Menu_CustomMaps_Draw ();
		return;

	default:
		Con_Printf("Cannot identify menu for case %d\n", m_state);
	}

	if (m_entersound)
	{
		S_LocalSound ("sounds/menu/enter.wav");
		m_entersound = false;
	}

	VID_UnlockBuffer ();
	S_ExtraUpdate ();
	VID_LockBuffer ();
}


void M_Keydown (int key)
{
	switch (m_state)
	{
	case m_none:
		return;

	case m_start:
		M_Start_Key (key);
		break;

	case m_paused_menu:
		M_Paused_Menu_Key (key);
		break;

	case m_main:
		M_Main_Key (key);
		return;

	case m_singleplayer:
		M_SinglePlayer_Key (key);
		return;

	case m_options:
		M_Options_Key (key);
		return;

	case m_keys:
		M_Keys_Key (key);
		return;

	case m_restart:
		M_Restart_Key (key);
		return;

	case m_credits:
		M_Credits_Key (key);
		return;

	case m_video:
		M_Video_Key (key);
		return;

	case m_quit:
		M_Quit_Key (key);
		return;

	case m_exit:
		M_Exit_Key (key);
		return;

	case m_gameoptions:
		M_GameOptions_Key (key);
		return;

	case m_custommaps:
		M_Menu_CustomMaps_Key (key);
		return;

	default:
		Con_Printf("Cannot identify menu for case %d\n", m_state);
	}
}
