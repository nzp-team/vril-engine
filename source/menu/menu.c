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
#include "menu_defs.h"

// Platform specific character representing
// which button is default bound to "enter"
// a menu
extern char         enter_char;

// Loading screens
extern image_t      loadingScreen;
extern char*        loadname2;
extern char*        loadnamespec;
extern qboolean     loadscreeninit;

// =============
// Custom maps
// =============
#define             MAX_CUSTOMMAPS 64

image_t             menu_custom;
image_t             menu_cuthum[MAX_CUSTOMMAPS];
achievement_list_t  achievement_list[MAX_ACHIEVEMENTS];

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

usermap_t   custom_maps[50];

int	    m_map_cursor;
int	    MAP_ITEMS;
int     user_maps_num = 0;
int     current_custom_map_page;
int     custom_map_pages;
int     multiplier;
char    user_levels[256][MAX_QPATH];

//=============================================================================
/* Menu Subsystem */

void Menu_Init (void)
{
	Cmd_AddCommand ("togglemenu", Menu_ToggleMenu_f);

	Cmd_AddCommand ("menu_main", Menu_Main_Set);
	//Cmd_AddCommand ("menu_singleplayer", Menu_SinglePlayer_Set);
	//Cmd_AddCommand ("menu_options", Menu_Options_Set);
	//Cmd_AddCommand ("menu_keys", Menu_Keys_Set);
	//Cmd_AddCommand ("menu_video", Menu_Video_Set);
	//Cmd_AddCommand ("menu_quit", Menu_Quit_Set);

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

	//Map_Finder();
}


void Menu_Draw (void)
{
	if (m_state == m_none || (key_dest != key_menu && key_dest != key_menu_pause))
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
		Menu_Start_Draw();
		break;

	case m_paused_menu:
		Menu_Paused_Draw();
		break;

	case m_main:
		Menu_Main_Draw ();
		break;

	case m_singleplayer:
		Menu_SinglePlayer_Draw ();
		break;

	case m_options:
		Menu_Options_Draw ();
		break;

	case m_keys:
		Menu_Keys_Draw ();
		break;

	case m_video:
		Menu_Video_Draw ();
		break;

	case m_quit:
		Menu_Quit_Draw ();
		break;

	case m_restart:
		Menu_Restart_Draw ();
		break;

	case m_credits:
		Menu_Credits_Draw ();
		break;

	case m_exit:
		Menu_Exit_Draw ();
		break;

	case m_gameoptions:
		Menu_GameOptions_Draw ();
		break;

	case m_custommaps:
		Menu_CustomMaps_Draw ();
		return;

	default:
		Con_Printf("Cannot identify menu for case %d\n", m_state);
	}

	if (m_entersound)
	{
		Menu_StartSound (MENU_SND_ENTER);
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
		Menu_Start_Key (key);
		break;

	case m_paused_menu:
		Menu_Paused_Key (key);
		break;

	case m_main:
		Menu_Main_Key (key);
		return;

	case m_singleplayer:
		Menu_SinglePlayer_Key (key);
		return;

	case m_options:
		Menu_Options_Key (key);
		return;

	case m_keys:
		Menu_Keys_Key (key);
		return;

	case m_restart:
		Menu_Restart_Key (key);
		return;

	case m_credits:
		Menu_Credits_Key (key);
		return;

	case m_video:
		Menu_Video_Key (key);
		return;

	case m_quit:
		Menu_Quit_Key (key);
		return;

	case m_exit:
		Menu_Exit_Key (key);
		return;

	case m_gameoptions:
		Menu_GameOptions_Key (key);
		return;

	case m_custommaps:
		Menu_CustomMaps_Key (key);
		return;

	default:
		Con_Printf("Cannot identify menu for case %d\n", m_state);
	}
}

/*
================
Menu_ToggleMenu_f
================
*/
void Menu_ToggleMenu_f (void)
{
	m_entersound = true;

	if (key_dest == key_menu || key_dest == key_menu_pause) {
		if (m_state != m_main && m_state != m_paused_menu) {
			Menu_Main_Set ();
			return;
		}
		key_dest = key_game;
		m_state = m_none;
		return;
	}
	if (key_dest == key_console) {
		Con_ToggleConsole_f ();
	} else if (sv.active && (svs.maxclients > 1 || key_dest == key_game)) {
		Menu_Paused_Set();
	} else {
		Menu_Main_Set ();
	}
}
