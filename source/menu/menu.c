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

void Menu_Main_Draw(void);
void Menu_Pause_Draw(void);
void Menu_Main_Draw(void);
void Menu_StockMaps_Draw(void);
void Menu_Video_Draw(void);
void Menu_Credits_Draw(void);
void Menu_GameOptions_Draw(void);
void Menu_CustomMaps_Draw(void);
void Menu_Lobby_Draw(void);
void Menu_GameSettings_Draw (void);
void Menu_Configuration_Draw (void);
void Menu_Video_Draw (void);
void Menu_Audio_Draw (void);
void Menu_Controls_Draw (void);
void Menu_Bindings_Draw (void);
void Menu_Accessibility_Draw (void);

menu_t			current_menu;
menu_button_t	current_menu_buttons[MAX_MENU_BUTTONS];

// Menu time keeping
float 			menu_time;
image_t			menu_background;
float			menu_changetime;
float			menu_starttime;

// Constant menu images
image_t			menu_bk;
image_t			menu_social;
image_t			menu_badges;

char*			game_build_date;

qboolean		m_recursiveDraw;

qboolean 		loading_init;
image_t 		lscreen_image;

// Current menu state
int 			m_state;

//=============================================================================
/* Menu Subsystem */

void Menu_ResetMenuButtons (void)
{
	for (int i = 0; i < MAX_MENU_BUTTONS; i++) {
		current_menu_buttons[i] = (menu_button_t){ 
			.enabled = false,
			.index = -1,
			.name = "",
			.on_activate = NULL 
		};
	}

	current_menu = (menu_t) {
		.button = current_menu_buttons,
		.cursor = 0,
		.slider_pressed = 0
	};
}

char *loading_string;
void Menu_DrawInitLoadScreen (void)
{
	if (loading_init == true) {
		loading_string = "please wait...";

		Menu_DrawFill (0, 0, vid.width, vid.width, 0, 0, 0, 255);

		Menu_DrawString (0, STD_UI_HEIGHT/2, loading_string, 255, 255, 255, 255, menu_text_scale_factor*2, 0);
	}
}

void Menu_SetVersionString (void)
{
#ifndef PLATFORM_GAMEFILES_IN_PAK
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
		game_build_date = malloc(24*sizeof(char));
		game_build_date = "version.txt not found.";
	}
#else
/*
	FILE *f = NULL;
	char version_text_path[32];
	int length = COM_FOpenFile("version.txt", &f);
	if (length != -1) {
		char line[32];
		game_build_date = malloc(sizeof(line));

		if (game_build_date) {
			while (fgets(line, sizeof(line), f) != NULL) {
				strip_newline (line);
				game_build_date = line;
			}
		}
		fclose(f);
	} else {
	*/
		game_build_date = malloc(24*sizeof(char));
		game_build_date = "version.txt not found.";
	//}

#endif
}

void Menu_Init (void)
{
	loading_string = malloc(16*(sizeof(char)));
	loading_init = true;

	Menu_InitUIScale();

	Menu_ResetMenuButtons();

	Menu_SetVersionString();

	Cmd_AddCommand ("togglemenu", Menu_ToggleMenu_f);

	// TODO - Achievements WIP
	//Menu_Achievements_Set();
	Menu_CustomMaps_MapFinder();

	menu_changetime = 0;

	loading_init = false;
}


void Menu_Draw (void)
{
	if (loading_init == true) {
		Menu_DrawInitLoadScreen();
	}

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

	if (key_dest == key_menu) {
		menu_time = Sys_FloatTime ();

		// Menu Background Changing
		// no reason to run this in pause menu or in-game
		if (menu_time > menu_changetime) {
			menu_background = Menu_PickBackground();
			menu_changetime = menu_time + 7;
			menu_starttime = menu_time;
		}
	}

	switch (m_state)
	{
	case m_none:
		break;

	case m_pause:
		Menu_Pause_Draw();
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

	case m_bindings:
		Menu_Bindings_Draw ();
		break;

	case m_accessibility:
		Menu_Accessibility_Draw ();
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

/*
================
Menu_ToggleMenu_f
================
*/
void Menu_ToggleMenu_f (void)
{
	if (key_dest == key_menu || key_dest == key_menu_pause) {
		if (m_state != m_main && m_state != m_pause) {
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
		Menu_Pause_Set();
	} else {
		Menu_Main_Set (false);
	}
}
