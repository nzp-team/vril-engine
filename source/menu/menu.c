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
#include "../nzportable_def.h"
#include "menu_defs.h"

// Menu time keeping
float 			menu_time;
image_t			menu_background;
float			menu_changetime;
float			menu_starttime;

// Constant menu images
image_t			menu_bk;
image_t			menu_social;

char*			game_build_date;

qboolean		m_recursiveDraw;

// Current menu state
int 			m_state;

qboolean		menu_is_solo;

int 			big_bar_height;
int 			small_bar_height;

int 			num_stock_maps;
char*			current_selected_bsp;

//=============================================================================
/* Menu Subsystem */

void Menu_Init (void)
{
	Cmd_AddCommand ("togglemenu", Menu_ToggleMenu_f);

	// TODO - Achievements WIP
	//Menu_Achievements_Set();
	Menu_CustomMaps_MapFinder();

	menu_changetime = 0;

	// Snag the game version
	long length;
	FILE* f = fopen(va("%s/version.txt", com_gamedir), "rb");

	if (f) {
		fseek (f, 0, SEEK_END);
		length = ftell (f);
		fseek (f, 0, SEEK_SET);
		game_build_date = malloc(length*sizeof(char));

		if (game_build_date) {
			fread (game_build_date, 1, length, f);
			strip_newline (game_build_date);
		}
		
		fclose (f);
	} else {
		game_build_date = "version.txt not found.";
	}
}


void Menu_Draw (void)
{
	if (m_state == m_none || (key_dest != key_menu && key_dest != key_menu_pause))
		return;

	menu_time = Sys_FloatTime ();

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

	//printf("menu time: %f\n", menu_time);
	//printf("menu_changetime: %f\n", menu_changetime);
	//printf("menu_starttime: %f\n", menu_starttime);

	// Menu Background Changing
	if (menu_time > menu_changetime) {
		menu_background = Menu_PickBackground();
		menu_changetime = menu_time + 7;
		menu_starttime = menu_time;
	}

	switch (m_state)
	{
	case m_none:
		break;

	case m_paused:
		Menu_Paused_Draw();
		break;

	case m_main:
		Menu_Main_Draw ();
		break;

	case m_stockmaps:
		Menu_StockMaps_Draw ();
		break;

	case m_custommaps:
		Menu_CustomMaps_Draw ();
		return;

	case m_lobby:
		Menu_Lobby_Draw ();
		break;

	case m_gamesettings:
		Menu_GameSettings_Draw ();
		break;

	case m_configuration:
		Menu_Configuration_Draw ();
		break;

	case m_video:
		Menu_Video_Draw ();
		break;

	case m_audio:
		Menu_Audio_Draw ();
		break;

	case m_controls:
		Menu_Controls_Draw ();
		break;

	case m_accessibility:
		Menu_Accessibility_Draw ();
		break;

	case m_keys:
		Menu_Keys_Draw ();
		break;

	case m_quit:
		Menu_Quit_Draw ();
		break;

	case m_credits:
		Menu_Credits_Draw ();
		break;

	default:
		Con_Printf("Cannot identify menu for case %d\n", m_state);
	}

	VID_UnlockBuffer ();
	S_ExtraUpdate ();
	VID_LockBuffer ();
}


void Menu_Keydown (int key)
{
	switch (m_state)
	{
	case m_none:
		return;

	case m_main:
		Menu_Main_Key (key);
		return;

	case m_paused:
		Menu_Paused_Key (key);
		break;

	case m_stockmaps:
		Menu_StockMaps_Key (key);
		return;

	case m_custommaps:
		Menu_CustomMaps_Key (key);
		return;

	case m_lobby:
		Menu_Lobby_Key (key);
		return;

	case m_gamesettings:
		Menu_GameSettings_Key (key);
		return;

	case m_configuration:
		Menu_Configuration_Key (key);
		return;

	case m_video:
		Menu_Video_Key (key);
		break;

	case m_audio:
		Menu_Audio_Key (key);
		break;

	case m_controls:
		Menu_Controls_Key (key);
		break;

	case m_accessibility:
		Menu_Accessibility_Key (key);
		break;

	case m_keys:
		Menu_Keys_Key (key);
		return;

	case m_credits:
		Menu_Credits_Key (key);
		return;

	case m_quit:
		Menu_Quit_Key (key);
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
	if (key_dest == key_menu || key_dest == key_menu_pause) {
		if (m_state != m_main && m_state != m_paused) {
			Menu_Main_Set (false);
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
		Menu_Main_Set (false);
	}
}
