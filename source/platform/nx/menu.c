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
#include <sys/dirent.h>

extern cvar_t	waypoint_mode;

extern int loadingScreen;
extern char* loadname2;
extern char* loadnamespec;
extern qboolean loadscreeninit;

char* game_build_date;

// Backgrounds
int menu_bk;

// Map screens
int menu_ndu;
int menu_wh;
int menu_wh2;
//qpic_t *menu_kn;
int menu_ch;
//qpic_t *menu_wn;
int menu_custom;
#define MAX_CUSTOMMAPS 64
int menu_cuthum[MAX_CUSTOMMAPS];

achievement_list_t achievement_list[MAX_ACHIEVEMENTS];

/* play enter sound after drawing a frame, so caching won't disrupt */
static qboolean m_entersound;
static qboolean m_recursiveDraw;

int			m_return_state;
qboolean	m_return_onerror;
char		m_return_reason [32];

void M_DrawCharacter(int cx, int line, int num) {
    Draw_Character(cx + ((vid.width - 320) >> 1), line, num);
}

void M_Print(int cx, int cy, const char *str) {
    while (*str) {
        M_DrawCharacter(cx, cy, (*str) + 128);
        str++;
        cx += 8;
    }
}

void M_PrintWhite(int cx, int cy, const char *str) {
    while (*str) {
        M_DrawCharacter(cx, cy, *str);
        str++;
        cx += 8;
    }
}

/* --------------------------------------------------------------------------*/

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
void M_Paused_Menu_f (void);
void M_Menu_Restart_f(void);
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
	void M_Menu_Credits_Draw (void);
	void M_Quit_Draw (void);
    void M_GameOptions_Draw (void);

void M_Main_Key (int key);
	void M_SinglePlayer_Key (int key);
		void M_Menu_CustomMaps_Key (int key);
	void M_Options_Key (int key);
		void M_Keys_Key (int key);
	void M_Menu_Credits_Key (int key);
	void M_Quit_Key (int key);
void M_GameOptions_Key (int key);
void M_Menu_Exit_f(void);

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

int	m_map_cursor;
int	MAP_ITEMS;
int user_maps_num = 0;
int current_custom_map_page;
int custom_map_pages;
int multiplier;
char  user_levels[256][MAX_QPATH];

//=============================================================================

void M_Load_Menu_Pics ()
{
	menu_bk 	= Image_LoadImage("gfx/menu/menu_background", IMAGE_TGA, 0, true, false);
	menu_ndu 	= Image_LoadImage("gfx/menu/nacht_der_untoten", IMAGE_TGA, 0, false, false);
	//menu_kn 	= Draw_CachePic("gfx/menu/kino_der_toten");
	menu_wh 	= Image_LoadImage("gfx/menu/nzp_warehouse", IMAGE_TGA, 0, false, false);
	menu_wh2 	= Image_LoadImage("gfx/menu/nzp_warehouse2", IMAGE_TGA, 0, false, false);
	//menu_wn 	= Draw_CachePic("gfx/menu/wahnsinn");
	menu_ch 	= Image_LoadImage("gfx/menu/christmas_special", IMAGE_TGA, 0, false, false);
	menu_custom = Image_LoadImage("gfx/menu/custom", IMAGE_TGA, 0, false, false);

	// Precache all menu pics 
	for (int i = 0; i < 15; i++) {
		if (custom_maps[i].occupied == false)
			continue;
		
		if (custom_maps[i].map_use_thumbnail == 1) {
				menu_cuthum[i] = Image_LoadImage(custom_maps[i + multiplier].map_thumbnail_path, IMAGE_TGA | IMAGE_PNG | IMAGE_JPG, 0, false, false);
			}
	}
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
	//menu_bk = Image_LoadImage("gfx/menu/menu_background", IMAGE_TGA, 0, true, false);
	Draw_StretchPic(0, 0, menu_bk, vid.width, vid.height);

	// Fill black to make everything easier to see
	Draw_FillByColor(0, 0, vid.width, vid.height, 0, 0, 0, 102);

	Draw_ColoredStringCentered(vid.height - 64, "Press A to Start", 255, 0, 0, 255, 1);
}

void M_Start_Key (int key)
{
	switch (key)
	{
		case K_ENTER:
			S_LocalSound ("sounds/menu/enter.wav");
			//Cbuf_AddText("cd playstring tensioned_by_the_damned 1\n");
			Cbuf_AddText("togglemenu\n");
			break;
	}
}

/*
================
M_ToggleMenu_f
================
*/
void M_ToggleMenu_f(void) {
    m_entersound = true;

    if (key_dest == key_menu) {
        if (m_state != m_main) {
            M_Menu_Main_f();
            return;
        }
        key_dest = key_game;
        m_state = m_none;
        return;
    }
    if (key_dest == key_console) {
        Con_ToggleConsole_f();
    } else {
        M_Menu_Main_f();
    }
}

int M_Paused_Cusor;
#define Max_Paused_Items 5

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

	if (M_Paused_Cusor == 0)
		Draw_ColoredString(10, 135, "Resume", 255, 0, 0, 255, 1);
	else
		Draw_ColoredString(10, 135, "Resume", 255, 255, 255, 255, 1);

	if (M_Paused_Cusor == 1)
		Draw_ColoredString(10, 145, "Restart", 255, 0, 0, 255, 1);
	else
		Draw_ColoredString(10, 145, "Restart", 255, 255, 255, 255, 1);

	if (M_Paused_Cusor == 2)
		Draw_ColoredString(10, 155, "Settings", 255, 0, 0, 255, 1);
	else
		Draw_ColoredString(10, 155, "Settings", 255, 255, 255, 255, 1);

	if (waypoint_mode.value) {
		if (M_Paused_Cusor == 3)
			Draw_ColoredString(10, 165, "Save Waypoints", 255, 0, 0, 255, 1);
		else
			Draw_ColoredString(10, 165, "Save Waypoints", 255, 255, 255, 255, 1);
	} else {
		if (M_Paused_Cusor == 3)
			Draw_ColoredString(10, 165, "Achievements", 255, 0, 0, 255, 1);
		else
			Draw_ColoredString(10, 165, "Achievements", 255, 255, 255, 255, 1);
	}

	if (M_Paused_Cusor == 4)
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
			if (++M_Paused_Cusor >= Max_Paused_Items)
				M_Paused_Cusor = 0;
			break;

		case K_UPARROW:
			S_LocalSound ("sounds/menu/navigate.wav");
			if (--M_Paused_Cusor < 0)
				M_Paused_Cusor = Max_Paused_Items - 1;
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

#define	MAIN_ITEMS	4
int	m_main_cursor;

void M_Menu_Main_f(void) {

    key_dest = key_menu;
    m_state = m_main;
    m_entersound = true;
}

void M_Main_Draw(void) {
    // Background
	//menu_bk = Image_LoadImage("gfx/menu/menu_background", IMAGE_TGA, 0, true, false);
	Draw_StretchPic(0, 0, menu_bk, vid.width, vid.height);

	// Fill black to make everything easier to see
	Draw_FillByColor(0, 0, vid.width, vid.height, 0, 0, 0, 102);

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

void M_Main_Key(int keynum) {
switch (keynum)
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
	//menu_bk = Image_LoadImage("gfx/menu/menu_background", IMAGE_TGA, 0, true, false);
	Draw_StretchPic(0, 0, menu_bk, vid.width, vid.height);

	// Fill black to make everything easier to see
	Draw_FillByColor(0, 0, vid.width, vid.height, 0, 0, 0, 102);

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
	//menu_bk = Image_LoadImage("gfx/menu/menu_background", IMAGE_TGA, 0, true, false);
	Draw_StretchPic(0, 0, menu_bk, vid.width, vid.height);

	// Fill black to make everything easier to see
	Draw_FillByColor(0, 0, vid.width, vid.height, 0, 0, 0, 102);

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
			menu_ndu = Image_LoadImage("gfx/menu/nacht_der_untoten", IMAGE_TGA, 0, false, false);
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
			menu_wh2 = Image_LoadImage("gfx/menu/nzp_warehouse2", IMAGE_TGA, 0, false, false);
			Draw_StretchPic(185, 40, menu_wh2, 175, 100);
			Draw_ColoredString(180, 148, "Four nameless marines find them-", 255, 255, 255, 255, 1);
			Draw_ColoredString(180, 158, "selves at a forsaken warehouse,", 255, 255, 255, 255, 1);
			Draw_ColoredString(180, 168, "or is it something more? Fight", 255, 255, 255, 255, 1);
			Draw_ColoredString(180, 178, "your way to uncovering its sec-", 255, 255, 255, 255, 1);
			Draw_ColoredString(180, 188, "rets, though you may not like", 255, 255, 255, 255, 1);
			Draw_ColoredString(180, 198, "what you find..", 255, 255, 255, 255, 1);
			break;
		case 2:
			menu_wh = Image_LoadImage("gfx/menu/nzp_warehouse", IMAGE_TGA, 0, false, false);
			Draw_StretchPic(185, 40, menu_wh, 175, 100);
			Draw_ColoredString(180, 148, "Old Warehouse full of Zombies!", 255, 255, 255, 255, 1);
			Draw_ColoredString(180, 158, "Fight your way to the Power", 255, 255, 255, 255, 1);
			Draw_ColoredString(180, 168, "Switch through the Hordes!", 255, 255, 255, 255, 1);
			break;
		case 3:
			menu_ch = Image_LoadImage("gfx/menu/christmas_special", IMAGE_TGA, 0, false, false);
			Draw_StretchPic(185, 40, menu_ch, 175, 100);
			Draw_ColoredString(180, 148, "No Santa this year. Though we're", 255, 255, 255, 255, 1);
			Draw_ColoredString(180, 158, "sure you will get presents from", 255, 255, 255, 255, 1);
			Draw_ColoredString(180, 168, "the undead! Will you accept them?", 255, 255, 255, 255, 1);
			break;
		case 4:
			menu_custom = Image_LoadImage("gfx/menu/custom", IMAGE_TGA, 0, false, false);
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
	
	while((dp = readdir(dir)))
	{
		
		if(dp->d_name[0] == '.')
		{
			continue;
		}

		if(!strcmp(COM_FileExtension(dp->d_name),"bsp")|| !strcmp(COM_FileExtension(dp->d_name),"BSP"))
	    {
			char ntype[32];

			COM_StripExtension(dp->d_name, ntype, sizeof(ntype));
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
	//menu_bk = Image_LoadImage("gfx/menu/menu_background", IMAGE_TGA, 0, true, false);
	Draw_StretchPic(0, 0, menu_bk, vid.width, vid.height);

	// Fill black to make everything easier to see
	Draw_FillByColor(0, 0, vid.width, vid.height, 0, 0, 0, 102);

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
				menu_cuthum[i] = Image_LoadImage(custom_maps[i + multiplier].map_thumbnail_path, IMAGE_TGA | IMAGE_PNG | IMAGE_JPG, 0, false, false);
				if (menu_cuthum[i] > 0) {
					Draw_StretchPic(185, 40, menu_cuthum[i], 175, 100);
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

typedef enum {
    M_OPTIONS_CURSOR_CONTROLS,
    M_OPTIONS_CURSOR_CONSOLE,
    M_OPTIONS_CURSOR_RESETDEFAULTS,
    M_OPTIONS_CURSOR_SCREENSIZE,
    M_OPTIONS_CURSOR_BRIGHTNESS,
    M_OPTIONS_CURSOR_MOUSESPEED,
    M_OPTIONS_CURSOR_MUSICVOLUME,
    M_OPTIONS_CURSOR_SOUNDVOLUME,
    M_OPTIONS_CURSOR_ALWAYSRUN,
    M_OPTIONS_CURSOR_MOUSEINVERT,
    // M_OPTIONS_CURSOR_MOUSELOOK,
    M_OPTIONS_CURSOR_LOOKSPRING,
    M_OPTIONS_CURSOR_LOOKSTRAFE,
    M_OPTIONS_CURSOR_VIDEO,
    // M_OPTIONS_CURSOR_MOUSEGRAB,
    M_OPTIONS_CURSOR_LINES,
} m_options_cursor_t;

#define SLIDER_RANGE 10

static m_options_cursor_t m_options_cursor;

void M_Menu_Options_f(void) {
    key_dest = key_menu;
    m_state = m_options;
    m_entersound = true;
}

void M_AdjustSliders(int dir) {
    S_LocalSound("misc/menu3.wav");

    switch (m_options_cursor) {
        case M_OPTIONS_CURSOR_SCREENSIZE:
           // scr_viewsize.value += dir * 10;
           // scr_viewsize.value = CLAMP(scr_viewsize.value, 30.0f, 120.0f);
           // Cvar_SetValue("viewsize", scr_viewsize.value);
            break;
        case M_OPTIONS_CURSOR_BRIGHTNESS:
            v_gamma.value -= dir * 0.05;
            v_gamma.value = CLAMP(v_gamma.value, 0.5f, 1.0f);
            Cvar_SetValue("gamma", v_gamma.value);
            break;
        case M_OPTIONS_CURSOR_MOUSESPEED:
            sensitivity.value += dir * 0.5;
            sensitivity.value = CLAMP(sensitivity.value, 1.0f, 11.0f);
            Cvar_SetValue("sensitivity", sensitivity.value);
            break;
        case M_OPTIONS_CURSOR_MUSICVOLUME:
#ifdef _WIN32
            bgmvolume.value += dir * 1.0;
#else
            bgmvolume.value += dir * 0.1;
#endif
            bgmvolume.value = CLAMP(bgmvolume.value, 0.0f, 1.0f);
            Cvar_SetValue("bgmvolume", bgmvolume.value);
            break;
        case M_OPTIONS_CURSOR_SOUNDVOLUME:
            volume.value += dir * 0.1;
            volume.value = CLAMP(volume.value, 0.0f, 1.0f);
            Cvar_SetValue("volume", volume.value);
            break;
        case M_OPTIONS_CURSOR_MOUSEINVERT:
            Cvar_SetValue("m_pitch", -m_pitch.value);
            break;
        // case M_OPTIONS_CURSOR_MOUSELOOK:
        //     Cvar_SetValue("m_freelook", !m_freelook.value);
        //     break;
        case M_OPTIONS_CURSOR_LOOKSPRING:
            Cvar_SetValue("lookspring", !lookspring.value);
            break;
        case M_OPTIONS_CURSOR_LOOKSTRAFE:
            Cvar_SetValue("lookstrafe", !lookstrafe.value);
            break;
        // case M_OPTIONS_CURSOR_MOUSEGRAB:
        //     Cvar_SetValue("_windowed_mouse", !_windowed_mouse.value);
        //     break;
        default:
            break;
    }
}

void M_DrawSlider(int x, int y, float range) {
    int i;

    if (range < 0) range = 0;
    if (range > 1) range = 1;
    M_DrawCharacter(x - 8, y, 128);
    for (i = 0; i < SLIDER_RANGE; i++) M_DrawCharacter(x + i * 8, y, 129);
    M_DrawCharacter(x + i * 8, y, 130);
    M_DrawCharacter(x + (SLIDER_RANGE - 1) * 8 * range, y, 131);
}

void M_DrawCheckbox(int x, int y, qboolean checked) { M_Print(x, y, checked ? "on" : "off"); }

void M_Options_Draw(void) {
    int height;
    float slider;

    M_Print(16, height = 32, "    Customize controls");
    M_Print(16, height += 8, "         Go to console");
    M_Print(16, height += 8, "     Reset to defaults");

    slider = (scr_viewsize.value - 30) / (120 - 30);
    M_Print(16, height += 8, "           Screen size");
    M_DrawSlider(220, height, slider);

    slider = (1.0 - v_gamma.value) / 0.5;
    M_Print(16, height += 8, "            Brightness");
    M_DrawSlider(220, height, slider);

    slider = (sensitivity.value - 1) / 10;
    M_Print(16, height += 8, "            Look Speed");
    M_DrawSlider(220, height, slider);

    slider = bgmvolume.value;
    M_Print(16, height += 8, "       CD Music Volume");
    M_DrawSlider(220, height, slider);

    slider = volume.value;
    M_Print(16, height += 8, "          Sound Volume");
    M_DrawSlider(220, height, slider);

    M_Print(16, height += 8, "         Invert Look Y");
    M_DrawCheckbox(220, height, m_pitch.value < 0);

    // M_Print(16, height += 8, "            Mouse Look");
    // M_DrawCheckbox(220, height, m_freelook.value);

    M_Print(16, height += 8, "            Lookspring");
    M_DrawCheckbox(220, height, lookspring.value);

    M_Print(16, height += 8, "            Lookstrafe");
    M_DrawCheckbox(220, height, lookstrafe.value);

    M_Print(16, height += 8, "         Video Options");

    // M_Print(16, height += 8, "             Use Mouse");
    // M_DrawCheckbox(220, height, _windowed_mouse.value);

    /* cursor */
    M_DrawCharacter(200, 32 + m_options_cursor * 8, 12 + ((int)(realtime * 4) & 1));
}

void M_Options_SaveConfig(void) {
    FILE *f = fopen(va("%s/config.cfg", com_gamedir), "w");
    if (!f) {
        Con_Printf("Couldn't write config.cfg.\n");
        return;
    }

    Key_WriteBindings(f);
    Cvar_WriteVariables(f);

    fclose(f);
}

void M_Options_Key(int keynum) {
    switch (keynum) {
        case K_ESCAPE:
            M_Options_SaveConfig();
            M_Menu_Main_f();
            break;

        case K_ENTER:
            m_entersound = true;
            switch (m_options_cursor) {
                case M_OPTIONS_CURSOR_CONTROLS:
                    M_Menu_Keys_f();
                    break;
                case M_OPTIONS_CURSOR_CONSOLE:
                    m_state = m_none;
                    Con_ToggleConsole_f();
                    break;
                case M_OPTIONS_CURSOR_RESETDEFAULTS:
                    Cbuf_AddText("exec default.cfg\n");
                    break;
                case M_OPTIONS_CURSOR_VIDEO:
                    M_Menu_Video_f();
                    break;
                default:
                    M_AdjustSliders(1);
                    break;
            }
            return;

        case K_UPARROW:
            S_LocalSound("misc/menu1.wav");
            if (m_options_cursor-- == 0) m_options_cursor = M_OPTIONS_CURSOR_LINES - 1;
            break;

        case K_DOWNARROW:
            S_LocalSound("misc/menu1.wav");
            m_options_cursor++;
            m_options_cursor %= M_OPTIONS_CURSOR_LINES;
            break;

        case K_LEFTARROW:
            M_AdjustSliders(-1);
            break;

        case K_RIGHTARROW:
            M_AdjustSliders(1);
            break;

        default:
            break;
    }
}

int		msgNumber;
int		m_quit_prevstate;
qboolean	wasInMenus;

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

	M_Print (64, 84,  exitMessage[0]);
	M_Print (64, 92,  exitMessage[1]);
	M_Print (64, 100, exitMessage[2]);
	M_Print (64, 108, exitMessage[3]);
}

//=============================================================================
/* KEYS MENU */

typedef enum {
    M_KEYS_CURSOR_ATTACK,
    M_KEYS_CURSOR_NEXTWEAPON,
    M_KEYS_CURSOR_PREVWEAPON,
    M_KEYS_CURSOR_JUMP,
    M_KEYS_CURSOR_FORWARD,
    M_KEYS_CURSOR_BACK,
    M_KEYS_CURSOR_TURNLEFT,
    M_KEYS_CURSOR_TURNRIGHT,
    M_KEYS_CURSOR_RUN,
    M_KEYS_CURSOR_MOVELEFT,
    M_KEYS_CURSOR_MOVERIGHT,
    M_KEYS_CURSOR_STRAFE,
    M_KEYS_CURSOR_LOOKUP,
    M_KEYS_CURSOR_LOOKDOWN,
    M_KEYS_CURSOR_CENTERVIEW,
    M_KEYS_CURSOR_MLOOK,
    M_KEYS_CURSOR_KLOOK,
    M_KEYS_CURSOR_MOVEUP,
    M_KEYS_CURSOR_MOVEDOWN,
    M_KEYS_CURSOR_LINES,
} m_keys_cursor_t;

static const char *const m_keys_bindnames[M_KEYS_CURSOR_LINES][2] = {
    {"+attack", "attack"},       {"impulse 10", "next weapon"}, {"impulse 12", "prev weapon"},
    {"+jump", "jump / swim up"}, {"+forward", "walk forward"},  {"+back", "backpedal"},
    {"+left", "turn left"},      {"+right", "turn right"},      {"+speed", "run"},
    {"+moveleft", "step left"},  {"+moveright", "step right"},  {"+strafe", "sidestep"},
    {"+lookup", "look up"},      {"+lookdown", "look down"},    {"centerview", "center view"},
    {"+mlook", "mouse look"},    {"+klook", "keyboard look"},   {"+moveup", "swim up"},
    {"+movedown", "swim down"}};

static m_keys_cursor_t m_keys_cursor;
static int m_keys_bind_grab;

void M_Menu_Keys_f(void) {
    key_dest = key_menu;
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

void M_Keys_Draw(void) {
    m_keys_cursor_t line;

    /* Draw the key bindings list */
    for (line = 0; line < M_KEYS_CURSOR_LINES; line++) {
        const int height = 48 + 8 * line;
        //int keys[2];

        M_Print(16, height, m_keys_bindnames[line][1]);
/*
        const char *keyname = Key_KeynumToString(keys[0]);
        M_Print(140, height, keyname);

        const int namewidth = strlen(keyname) * 8;
        keyname = Key_KeynumToString(keys[1]);
        M_Print(140 + namewidth + 8, height, "or");
        M_Print(140 + namewidth + 32, height, keyname);
*/
    }

    /* Draw the header and cursor */
    if (m_keys_bind_grab) {
        M_Print(12, 32, "Press a key or button for this action");
        M_DrawCharacter(130, 48 + m_keys_cursor * 8, '=');
    } else {
        const int cursor_char = 12 + ((int)(realtime * 4) & 1);
        M_Print(18, 32, "Enter to change, backspace to clear");
        M_DrawCharacter(130, 48 + m_keys_cursor * 8, cursor_char);
    }
}

void M_Keys_Key(int keynum) {
    int keys[2];

    if (m_keys_bind_grab) {
        /* Define a key binding */
        S_LocalSound("misc/menu1.wav");
        if (keynum != K_ESCAPE) {
            const char *keyname = Key_KeynumToString(keynum);
            const char *command = m_keys_bindnames[m_keys_cursor][0];
            Cbuf_InsertText(va("bind \"%s\" \"%s\"\n", keyname, command));
        }
        m_keys_bind_grab = false;
        return;
    }

    switch (keynum) {
        case K_ESCAPE:
            M_Menu_Options_f();
            break;
        case K_LEFTARROW:
        case K_UPARROW:
            S_LocalSound("misc/menu1.wav");
            if (m_keys_cursor-- == 0) m_keys_cursor = M_KEYS_CURSOR_LINES - 1;
            break;
        case K_DOWNARROW:
        case K_RIGHTARROW:
            S_LocalSound("misc/menu1.wav");
            m_keys_cursor++;
            m_keys_cursor %= M_KEYS_CURSOR_LINES;
            break;
        case K_ENTER:
            /* go into bind mode */
            M_FindKeysForCommand("+use", keys);
            S_LocalSound("misc/menu2.wav");
            m_keys_bind_grab = true;
            break;
        case K_BACKSPACE:
        case K_DEL:
            /* delete bindings */
            break;
        default:
            break;
    }
}

//=============================================================================
/* VIDEO MENU */

void M_Menu_Video_f(void) {

}

//=============================================================================
/* Menu Subsystem */

void M_Init(void) {

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

//=============================================================================
/* MULTIPLAYER MENU */

#define MULTIPLAYER_ITEMS 3

void M_Menu_MultiPlayer_f(void) {

}

void M_MultiPlayer_Draw(void) {

}

void M_MultiPlayer_Key(int keynum) {

}

//=============================================================================
/* SETUP MENU */

void M_Menu_Setup_f(void) {

}

void M_Setup_Draw(void) {

}

void M_Setup_Key(int keynum) {

}

//=============================================================================
/* HELP MENU */


void M_Menu_Help_f(void) {

}

void M_Help_Draw(void) { return; }

void M_Help_Key(int keynum) {

}

//=============================================================================
/* LAN CONFIG MENU */


void M_ConfigureNetSubsystem(void) {

}

void M_Menu_LanConfig_f(void) {

}

//=============================================================================
/* GAME OPTIONS MENU */



void M_Menu_GameOptions_f(void) {

}

void M_GameOptions_Draw(void) {
}


void M_NetStart_Change(int dir) {

}

void M_GameOptions_Key(int keynum) {
   
}

//=============================================================================
/* SEARCH MENU */

void M_Menu_Search_f(void) {

}

void M_Search_Draw(void) {

}

void M_Search_Key(int keynum) {}

//=============================================================================
/* SLIST MENU */


void M_ServerList_Draw(void) {

}

void M_ServerList_Key(int keynum) {

}
