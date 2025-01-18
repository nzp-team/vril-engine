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
	m_options2,
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

		void M_Menu_Load_f (void);
		void M_Menu_Save_f (void);
	void M_Menu_MultiPlayer_f (void);
		void M_Menu_Setup_f (void);
		void M_Menu_Net_f (void);
	void M_Menu_Options_f (void);
		void M_Menu_Options2_f (void);
	void M_Menu_Help_f (void);
void M_Menu_LanConfig_f (void);
void M_Menu_GameOptions_f (void);
void M_Menu_Search_f (void);
void M_Menu_ServerList_f (void);
void M_Paused_Menu_f (void);

		void M_Load_Draw (void);
		void M_Save_Draw (void);
	void M_MultiPlayer_Draw (void);
		void M_Setup_Draw (void);
		void M_Net_Draw (void);
	void M_Options_Draw (void);
		void M_Options2_Draw (void);
	void M_Help_Draw (void);
void M_LanConfig_Draw (void);
void M_GameOptions_Draw (void);
void M_Search_Draw (void);
void M_ServerList_Draw (void);

		void M_Load_Key (int key);
		void M_Save_Key (int key);
	void M_MultiPlayer_Key (int key);
		void M_Setup_Key (int key);
		void M_Net_Key (int key);
	void M_Options_Key (int key);
		void M_Options2_Key (int key);
	void M_Help_Key (int key);
void M_LanConfig_Key (int key);
void M_GameOptions_Key (int key);
void M_Search_Key (int key);
void M_ServerList_Key (int key);


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

void M_Menu_Restart_f (void);
void M_Menu_Exit_f (void);

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

extern cvar_t	waypoint_mode;

extern int loadingScreen;
extern char* loadname2;
extern char* loadnamespec;
extern qboolean loadscreeninit;

char* game_build_date;

#define StartingGame	(m_multiplayer_cursor == 1)
#define JoiningGame		(m_multiplayer_cursor == 0)
#define	TCPIPConfig		(m_net_cursor == 3)

void M_ConfigureNetSubsystem(void);

/*
================
M_DrawCharacter

Draws one solid graphics character
================
*/
void M_DrawCharacter (int cx, int line, int num)
{
	Draw_Character ( cx + ((vid.conwidth - 320)>>1), line, num);
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
	Draw_TransPic (x + ((vid.conwidth - 320)>>1), y, pic);
}

void M_DrawPic (int x, int y, qpic_t *pic)
{
	Draw_Pic (x + ((vid.conwidth - 320)>>1), y, pic);
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
	Draw_TransPicTranslate (x + ((vid.conwidth - 320)>>1), y, pic, translationTable);
}

void M_DrawTextBox (int x, int y, int width, int lines)
{
	
	qpic_t	*p;
	int		cx, cy;
	int		n;

	// draw left side
	cx = x;
	cy = y;
	p = Draw_LMP ("gfx/box_tl.lmp");
	M_DrawTransPic (cx, cy, p);
	p = Draw_LMP ("gfx/box_ml.lmp");
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		M_DrawTransPic (cx, cy, p);
	}
	p = Draw_LMP ("gfx/box_bl.lmp");
	M_DrawTransPic (cx, cy+8, p);

	// draw middle
	cx += 8;
	while (width > 0)
	{
		cy = y;
		p = Draw_LMP ("gfx/box_tm.lmp");
		M_DrawTransPic (cx, cy, p);
		p = Draw_LMP ("gfx/box_mm.lmp");
		for (n = 0; n < lines; n++)
		{
			cy += 8;
			if (n == 1)
				p = Draw_LMP ("gfx/box_mm2.lmp");
			M_DrawTransPic (cx, cy, p);
		}
		p = Draw_LMP ("gfx/box_bm.lmp");
		M_DrawTransPic (cx, cy+8, p);
		width -= 2;
		cx += 16;
	}

	// draw right side
	cy = y;
	p = Draw_LMP ("gfx/box_tr.lmp");
	M_DrawTransPic (cx, cy, p);
	p = Draw_LMP ("gfx/box_mr.lmp");
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		M_DrawTransPic (cx, cy, p);
	}
	p = Draw_LMP ("gfx/box_br.lmp");
	M_DrawTransPic (cx, cy+8, p);

	return;
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
	Draw_FillByColor(0, 0, 680, 340, 0, 0, 0, 80);

	// Header
	Draw_ColoredString(30, 20, "PAUSED", 255, 255, 255, 255, 3);

	if ((M_Paused_Cusor == 0))
		Draw_ColoredString(30, 200, "Resume", 255, 0, 0, 255, 1.5);
	else
		Draw_ColoredString(30, 200, "Resume", 255, 255, 255, 255, 1.5);

	if ((M_Paused_Cusor == 1))
		Draw_ColoredString(30, 220, "Restart", 255, 0, 0, 255, 1.5);
	else
		Draw_ColoredString(30, 220, "Restart", 255, 255, 255, 255, 1.5);

	if ((M_Paused_Cusor == 2))
		Draw_ColoredString(30, 240, "Settings", 255, 0, 0, 255, 1.5);
	else
		Draw_ColoredString(30, 240, "Settings", 255, 255, 255, 255, 1.5);

	if (waypoint_mode.value) {
		if ((M_Paused_Cusor == 3))
			Draw_ColoredString(30, 260, "Save Waypoints", 255, 0, 0, 255, 1.5);
		else
			Draw_ColoredString(30, 260, "Save Waypoints", 255, 255, 255, 255, 1.5);
	} else {
		if ((M_Paused_Cusor == 3))
			Draw_ColoredString(30, 260, "Achievements", 255, 0, 0, 255, 1.5);
		else
			Draw_ColoredString(30, 260, "Achievements", 255, 255, 255, 255, 1.5);
	}

	if ((M_Paused_Cusor == 4))
		Draw_ColoredString(30, 280, "Main Menu", 255, 0, 0, 255, 1.5);
	else
		Draw_ColoredString(30, 280, "Main Menu", 255, 255, 255, 255, 1.5);
}

static void M_Paused_Menu_Key (int key)
{
	switch (key)
	{
		case K_BACKSPACE:
		case K_ESCAPE:
		case K_JOY1:
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
		case K_JOY0:
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
	Draw_StretchPic(0, 0, menu_bk, vid.width, vid.height);

	// Fill black to make everything easier to see
	Draw_FillByColor(0, 0, 400, 240, 0, 0, 0, 102);

	// Header
	Draw_ColoredString(5, 5, "CREDITS", 255, 255, 255, 255, 3);

	Draw_ColoredString(5, 30, "Programming:", 255, 255, 255, 255, 1.5);
	Draw_ColoredString(5, 40, "Blubs, Jukki, DR_Mabuse1981, Naievil", 255, 255, 255, 255, 1.5);
	Draw_ColoredString(5, 50, "Cypress, ScatterBox", 255, 255, 255, 255, 1.5);

	Draw_ColoredString(5, 70, "Models:", 255, 255, 255, 255, 1.5);
	Draw_ColoredString(5, 80, "Blubs, Ju[s]tice, Derped_Crusader", 255, 255, 255, 255, 1.5);

	Draw_ColoredString(5, 100, "GFX:", 255, 255, 255, 255, 1.5);
	Draw_ColoredString(5, 110, "Blubs, Ju[s]tice, Cypress, Derped_Crusader", 255, 255, 255, 255, 1.5);

	Draw_ColoredString(5, 130, "Sounds/Music:", 255, 255, 255, 255, 1.5);
	Draw_ColoredString(5, 140, "Blubs, Biodude, Cypress, Marty P.", 255, 255, 255, 255, 1.5);

	Draw_ColoredString(5, 160, "Special Thanks:", 255, 255, 255, 255, 1.5);
	Draw_ColoredString(5, 170, "- Spike, Eukara:     FTEQW", 255, 255, 255, 255, 1.5);
	Draw_ColoredString(5, 180, "- Shpuld:            CleanQC4FTE", 255, 255, 255, 255, 1.5);
	Draw_ColoredString(5, 190, "- Crow_Bar, st1x51:  dQuake(plus)", 255, 255, 255, 255, 1.5);
	Draw_ColoredString(5, 200, "- fgsfdsfgs:         Quakespasm-NX", 255, 255, 255, 255, 1.5);
	Draw_ColoredString(5, 210, "- MasterFeizz:       ctrQuake", 255, 255, 255, 255, 1.5);
	Draw_ColoredString(5, 220, "- Rinnegatamante:    Initial VITA Port & Updater", 255, 255, 255, 255, 1.5);

	Draw_ColoredString(5, 230, "Back", 255, 0, 0, 255, 1.5);
}


void M_Credits_Key (int key)
{
	switch (key)
	{
		case K_ENTER:
		case K_JOY0:
		case K_JOY1:
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
	case K_JOY1:
	case 'n':
	case 'N':
		m_state = m_paused_menu;
		m_entersound = true;
		break;

	case 'Y':
	case 'y':
	case K_ENTER:
	case K_JOY0:
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
	case K_JOY1:
	case 'n':
	case 'N':
		m_state = m_paused_menu;
		m_entersound = true;
		break;

	case 'Y':
	case 'y':
	case K_ENTER:
	case K_JOY0:
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
/* MAIN MENU */

void M_Start_Menu_f ()
{
	key_dest = key_menu;
	m_state = m_start;
	m_entersound = true;
	//CDAudio_Play("tracks/tensioned_by_the_damned.mp3", false);
}

static void M_Start_Menu_Draw ()
{
	char *str = "Press 'A'";
	
	// Fill black to make everything easier to see
	Draw_FillByColor(0, 0, vid.width, vid.height, 0, 0, 0, 255);
	
	// Background
	menu_bk = Draw_CachePic("gfx/menu/menu_background");
	Draw_StretchPic(0, 0, menu_bk, vid.width, vid.height);

	Draw_ColoredString((vid.width/2) - 64, (vid.height - 64), str, 255, 0, 0, 185, 2);
}

void M_Start_Key (int key)
{
	switch (key)
	{
		case K_JOY0:
		case K_JOY5:
			S_LocalSound ("sounds/menu/enter.wav");
			Cbuf_AddText("cd playstring tensioned_by_the_damned 1\n");
			Cbuf_AddText("togglemenu\n");
			break;
	}
}

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
	int y = 60;
	
	// Background
	menu_bk = Draw_CachePic("gfx/menu/menu_background");
	Draw_StretchPic(0, 0, menu_bk, vid.width, vid.height);

	// Fill black to make everything easier to see
	Draw_FillByColor(0, 0, vid.width, vid.height, 0, 0, 0, 102);

	// Version String
	Draw_ColoredString((vid.width - (strlen(game_build_date) * 12)), 6, game_build_date, 255, 255, 255, 255, 1.5);

	// Header
	Draw_ColoredString(6, 6, "MAIN MENU", 255, 255, 255, 255, 3.5);

	// Solo
	if (m_main_cursor == 0)
		Draw_ColoredString(5, y, "Solo", 255, 0, 0, 255, 1.5);
	else
		Draw_ColoredString(5, y, "Solo", 255, 255, 255, 255, 1.5);

	y+=20;

	// Co-Op (Unfinished, so non-selectable)
	Draw_ColoredString(5, y, "Co-Op (Coming Soon!)", 128, 128, 128, 255, 1.5);
	
	y+=20;

	// Divider
	Draw_FillByColor(5, y, 160, 2, 130, 130, 130, 255);
	
	y+=10;

	if (m_main_cursor == 1)
		Draw_ColoredString(5, y, "Settings", 255, 0, 0, 255, 1.5);
	else
		Draw_ColoredString(5, y, "Settings", 255, 255, 255, 255, 1.5);
	
	y+=20;

	Draw_ColoredString(5, y, "Achievements", 128, 128, 128, 255, 1.5);
	
	y+=20;

	// Divider
	Draw_FillByColor(5, y, 160, 2, 130, 130, 130, 255);
	
	y+=10;

	if (m_main_cursor == 2)
		Draw_ColoredString(5, y, "Credits", 255, 0, 0, 255, 1.5);
	else
		Draw_ColoredString(5, y, "Credits", 255, 255, 255, 255, 1.5);
	
	y+=20;

	// Divider
	Draw_FillByColor(5, y, 160, 2, 130, 130, 130, 255);
	
	y+=10;

	if (m_main_cursor == 3)
		Draw_ColoredString(5, y, "Exit", 255, 0, 0, 255, 1.5);
	else
		Draw_ColoredString(5, y, "Exit", 255, 255, 255, 255, 1.5);

	// Descriptions
	switch(m_main_cursor) {
		case 0: // Solo
			Draw_ColoredString(12, 455, "Take on the Hordes by yourself.", 255, 255, 255, 255, 1.5);
			break;
		case 1: // Settings
			Draw_ColoredString(12, 455, "Adjust your Settings to Optimize your Experience.", 255, 255, 255, 255, 1.5);
			break;
		case 2: // Credits
			Draw_ColoredString(12, 455, "See who made NZ:P possible.", 255, 255, 255, 255, 1.5);
			break;
		case 3: // Exit
			Draw_ColoredString(12, 455, "Return to Home Menu.", 255, 255, 255, 255, 1.5);
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
	case K_JOY0:
	case K_JOY9:
	case K_JOY20:
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
/* SINGLE PLAYER MENU */

int	m_singleplayer_cursor;
#define	SINGLEPLAYER_ITEMS	6 //6


void M_Menu_SinglePlayer_f (void)
{
	key_dest = key_menu;
	m_state = m_singleplayer;
	m_entersound = true;
}


void M_SinglePlayer_Draw (void)
{
	int y = 60;
	
	// Background
	menu_bk = Draw_CachePic("gfx/menu/menu_background");
	Draw_StretchPic(0, 0, menu_bk, vid.width, vid.height);

	// Fill black to make everything easier to see
	Draw_FillByColor(0, 0, vid.width, vid.height, 0, 0, 0, 102);

	// Header
	Draw_ColoredString(6, 6, "SOLO", 255, 255, 255, 255, 3);
	
	// Version String
	Draw_ColoredString((vid.width - (strlen(game_build_date) * 12)), 6, game_build_date, 255, 255, 255, 255, 1.5);

	// Nacht der Untoten
	if (m_singleplayer_cursor == 0)
		Draw_ColoredString(5, y, "Nacht der Untoten", 255, 0, 0, 255, 1.5);
	else
		Draw_ColoredString(5, y, "Nacht der Untoten", 255, 255, 255, 255, 1.5);
	
	y+=20;

	// Divider
	Draw_FillByColor(5, y, 160, 2, 130, 130, 130, 255);
	
	y+=10;

	// Warehouse
	if (m_singleplayer_cursor == 1)
		Draw_ColoredString(5, y, "Warehouse", 255, 0, 0, 255, 1.5);
	else
		Draw_ColoredString(5, y, "Warehouse", 255, 255, 255, 255, 1.5);
	
	y+=20;

	// Warehouse (Classic)
	if (m_singleplayer_cursor == 2)
		Draw_ColoredString(5, y, "Warehouse (Classic)", 255, 0, 0, 255, 1.5);
	else
		Draw_ColoredString(5, y, "Warehouse (Classic)", 255, 255, 255, 255, 1.5);
	
	y+=20;

	// Christmas Special
	if (m_singleplayer_cursor == 3)
		Draw_ColoredString(5, y, "Christmas Special", 255, 0, 0, 255, 1.5);
	else
		Draw_ColoredString(5, y, "Christmas Special", 255, 255, 255, 255, 1.5);
	
	y+=20;

	// Divider
	Draw_FillByColor(5, y, 160, 2, 130, 130, 130, 255);
	
	y+=10;

	// Custom Maps
	if (m_singleplayer_cursor == 4)
		Draw_ColoredString(5, y, "Custom Maps", 255, 0, 0, 255, 1.5);
	else
		Draw_ColoredString(5, y, "Custom Maps", 255, 255, 255, 255, 1.5);
	
	y+=22;

	// Back
	if (m_singleplayer_cursor == 5)
		Draw_ColoredString(5, y, "Back", 255, 0, 0, 255, 1.4);
	else
		Draw_ColoredString(5, y, "Back", 255, 255, 255, 255, 1.4);

	// Map description & pic
	switch(m_singleplayer_cursor) {
		case 0:
			menu_ndu = Draw_CachePic("gfx/menu/nacht_der_untoten");
			Draw_StretchPic(265, 55, menu_ndu, 320, 200);
			Draw_ColoredString(235, 275, "Desolate bunker located on a Ge-", 255, 255, 255, 255, 1.5);
			Draw_ColoredString(235, 290, "rman airfield, stranded after a", 255, 255, 255, 255, 1.5);
			Draw_ColoredString(235, 305, "brutal plane crash surrounded by", 255, 255, 255, 255, 1.5);
			Draw_ColoredString(235, 320, "hordes of undead. Exploit myste-", 255, 255, 255, 255, 1.5);
			Draw_ColoredString(235, 335, "rious forces at play and hold o-", 255, 255, 255, 255, 1.5);
			Draw_ColoredString(235, 350, "ut against relentless waves. Der", 255, 255, 255, 255, 1.5);
			Draw_ColoredString(235, 365, "Anstieg ist jetzt. Will you fall", 255, 255, 255, 255, 1.5);
			Draw_ColoredString(235, 380, "to the overwhelming onslaught?", 255, 255, 255, 255, 1.5);
			break;
		case 1:
			menu_wh2 = Draw_CachePic("gfx/menu/nzp_warehouse2");
			Draw_StretchPic(265, 55, menu_wh2, 320, 200);
			Draw_ColoredString(235, 275, "Four nameless marines find them-", 255, 255, 255, 255, 1.5);
			Draw_ColoredString(235, 290, "selves at a forsaken warehouse,", 255, 255, 255, 255, 1.5);
			Draw_ColoredString(235, 305, "or is it something more? Fight", 255, 255, 255, 255, 1.5);
			Draw_ColoredString(235, 320, "your way to uncovering its sec-", 255, 255, 255, 255, 1.5);
			Draw_ColoredString(235, 335, "rets, though you may not like", 255, 255, 255, 255, 1.5);
			Draw_ColoredString(235, 350, "what you find..", 255, 255, 255, 255, 1.5);
			break;
		case 2:
			menu_wh = Draw_CachePic("gfx/menu/nzp_warehouse");
			Draw_StretchPic(265, 55, menu_wh, 320, 200);
			Draw_ColoredString(235, 275, "Old Warehouse full of Zombies!", 255, 255, 255, 255, 1.5);
			Draw_ColoredString(235, 290, "Fight your way to the Power", 255, 255, 255, 255, 1.5);
			Draw_ColoredString(235, 305, "Switch through the Hordes!", 255, 255, 255, 255, 1.5);
			break;
		case 3:
			menu_ch = Draw_CachePic("gfx/menu/christmas_special");
			Draw_StretchPic(265, 55, menu_ch, 320, 200);
			Draw_ColoredString(235, 275, "No Santa this year. Though we're", 255, 255, 255, 255, 1.5);
			Draw_ColoredString(235, 290, "sure you will get presents from", 255, 255, 255, 255, 1.5);
			Draw_ColoredString(235, 305, "the undead! Will you accept them?", 255, 255, 255, 255, 1.5);
			break;
		case 4:
			menu_custom = Draw_CachePic("gfx/menu/custom");
			Draw_StretchPic(265, 55, menu_custom, 320, 200);
			Draw_ColoredString(235, 275, "Custom Maps made by Community", 255, 255, 255, 255, 1.5);
			Draw_ColoredString(235, 290, "Members on GitHub and on the", 255, 255, 255, 255, 1.5);
			Draw_ColoredString(235, 305, "NZ:P Forum!", 255, 255, 255, 255, 1.5);
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
	case K_JOY0:
	case K_JOY9:
	case K_JOY20:
		m_entersound = true;

		switch (m_singleplayer_cursor)
		{
		case 0:
			key_dest = key_game;
			if (sv.active)
				Cbuf_AddText ("disconnect\n");
			Cbuf_AddText ("maxplayers 1\n");
			Cbuf_AddText ("cd stop\n");
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
			Cbuf_AddText ("cd stop\n");
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
			Cbuf_AddText ("cd stop\n");
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
			Cbuf_AddText ("cd stop\n");
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
	case K_BACKSPACE:
	case K_ESCAPE:
	case K_JOY1:
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
#include <fat.h>
#include <sys/dir.h>
char *COM_FileExtension (char *in);
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
	Draw_StretchPic(0, 0, menu_bk, vid.width, vid.height);

	// Fill black to make everything easier to see
	Draw_FillByColor(0, 0, vid.width, vid.height, 0, 0, 0, 102);

	// Header
	Draw_ColoredString(6, 6, "CUSTOM MAPS", 255, 255, 255, 255, 3);
	
	// Version String
	Draw_ColoredString((vid.width - (strlen(game_build_date) * 12)), 6, game_build_date, 255, 255, 255, 255, 1.5);

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
					Draw_StretchPic(265, 55, menu_cuthum, 320, 200);
				}
			}
			
			if (custom_maps[i + multiplier].map_name_pretty != 0)
				Draw_ColoredString(5, 60 + (15 * i), custom_maps[i + multiplier].map_name_pretty, 255, 0, 0, 255, 1.5);
			else
				Draw_ColoredString(5, 60 + (15 * i), custom_maps[i + multiplier].map_name, 255, 0, 0, 255, 1.5);

			if (custom_maps[i + multiplier].map_desc_1 != 0) {
				if (strcmp(custom_maps[i + multiplier].map_desc_1, " ") != 0) {
					Draw_ColoredString(235, 275, custom_maps[i + multiplier].map_desc_1, 255, 255, 255, 255, 1.5);
				}
			}
			if (custom_maps[i + multiplier].map_desc_2 != 0) {
				if (strcmp(custom_maps[i + multiplier].map_desc_2, " ") != 0) {
					line_increment++;
					Draw_ColoredString(235, 290, custom_maps[i + multiplier].map_desc_2, 255, 255, 255, 255, 1.5);
				}
			}
			if (custom_maps[i + multiplier].map_desc_3 != 0) {
				if (strcmp(custom_maps[i + multiplier].map_desc_3, " ") != 0) {
					line_increment++;
					Draw_ColoredString(235, 305, custom_maps[i + multiplier].map_desc_3, 255, 255, 255, 255, 1.5);
				}
			}
			if (custom_maps[i + multiplier].map_desc_4 != 0) {
				if (strcmp(custom_maps[i + multiplier].map_desc_4, " ") != 0) {
					line_increment++;
					Draw_ColoredString(235, 320, custom_maps[i + multiplier].map_desc_4, 255, 255, 255, 255, 1.5);
				}
			}
			if (custom_maps[i + multiplier].map_desc_5 != 0) {
				if (strcmp(custom_maps[i + multiplier].map_desc_5, " ") != 0) {
					line_increment++;
					Draw_ColoredString(235, 335, custom_maps[i + multiplier].map_desc_5, 255, 255, 255, 255, 1.5);
				}
			}
			if (custom_maps[i + multiplier].map_desc_6 != 0) {
				if (strcmp(custom_maps[i + multiplier].map_desc_6, " ") != 0) {
					line_increment++;
					Draw_ColoredString(235, 350, custom_maps[i + multiplier].map_desc_6, 255, 255, 255, 255, 1.5);
				}
			}
			if (custom_maps[i + multiplier].map_desc_7 != 0) {
				if (strcmp(custom_maps[i + multiplier].map_desc_7, " ") != 0) {
					line_increment++;
					Draw_ColoredString(235, 365, custom_maps[i + multiplier].map_desc_7, 255, 255, 255, 255, 1.5);
				}
			}
			if (custom_maps[i + multiplier].map_desc_8 != 0) {
				if (strcmp(custom_maps[i + multiplier].map_desc_8, " ") != 0) {
					line_increment++;
					Draw_ColoredString(235, 380, custom_maps[i + multiplier].map_desc_8, 255, 255, 255, 255, 1.5);
				}
			}
			if (custom_maps[i + multiplier].map_author != 0) {
				if (strcmp(custom_maps[i + multiplier].map_author, " ") != 0) {
					int y = 238;
					Draw_ColoredString(270, y, custom_maps[i + multiplier].map_author, 255, 255, 0, 255, 1.5);
				}
			}
		} else {
			if (custom_maps[i + multiplier].map_name_pretty != 0)
				Draw_ColoredString(6, 60 + (15 * i), custom_maps[i + multiplier].map_name_pretty, 255, 255, 255, 255, 1.5);
			else
				Draw_ColoredString(6, 60 + (15 * i), custom_maps[i + multiplier].map_name, 255, 255, 255, 255, 1.5);
		}
	}

	if (current_custom_map_page != custom_map_pages) {
		if (m_map_cursor == 15)
			Draw_ColoredString(6, 315, "Next Page", 255, 0, 0, 255, 1.5);
		else
			Draw_ColoredString(6, 315, "Next Page", 255, 255, 255, 255, 1.5);
	} else {
		Draw_ColoredString(6, 315, "Next Page", 128, 128, 128, 255, 1.5);
	}

	if (current_custom_map_page != 1) {
		if (m_map_cursor == 16)
			Draw_ColoredString(6, 330, "Previous Page", 255, 0, 0, 255, 1.5);
		else
			Draw_ColoredString(6, 330, "Previous Page", 255, 255, 255, 255, 1.5);
	} else {
		Draw_ColoredString(6, 330, "Previous Page", 128, 128, 128, 255, 1.5);
	}



	if (m_map_cursor == 17)
		Draw_ColoredString(6, 348, "Back", 255, 0, 0, 255, 1.4);
	else
		Draw_ColoredString(6, 348, "Back", 255, 255, 255, 255, 1.4);
}


void M_Menu_CustomMaps_Key (int key)
{
	switch (key)
	{
		case K_ESCAPE:
		case K_JOY1:
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
		case K_JOY0:
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
				Cbuf_AddText ("cd stop\n");
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
/* MULTIPLAYER MENU */

int	m_multiplayer_cursor;
#define	MULTIPLAYER_ITEMS	3


void M_Menu_MultiPlayer_f (void)
{
	key_dest = key_menu;
	//m_state = m_multiplayer;
	m_entersound = true;
}


void M_MultiPlayer_Draw (void)
{
	//int		f;
	//qpic_t	*p;
	/*
	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);
	M_DrawTransPic (72, 32, Draw_CachePic ("gfx/mp_menu.lmp") );

	f = (int)(host_time * 10)%6;

	M_DrawTransPic (54, 32 + m_multiplayer_cursor * 20,Draw_CachePic( va("gfx/menudot%i.lmp", f+1 ) ) );

	if (tcpipAvailable)
		return;
	M_PrintWhite ((320/2) - ((27*8)/2), 148, "No Communications Available");
	*/
}


void M_MultiPlayer_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
	case K_JOY1:
	case K_JOY10:
	case K_JOY21:
		M_Menu_Main_f ();
		break;

	case K_DOWNARROW:
		//S_LocalSound ("misc/menu1.wav");
		if (++m_multiplayer_cursor >= MULTIPLAYER_ITEMS)
			m_multiplayer_cursor = 0;
		break;

	case K_UPARROW:
		//S_LocalSound ("misc/menu1.wav");
		if (--m_multiplayer_cursor < 0)
			m_multiplayer_cursor = MULTIPLAYER_ITEMS - 1;
		break;

	case K_ENTER:
	case K_JOY0:
	case K_JOY9:
	case K_JOY20:
		m_entersound = true;
		switch (m_multiplayer_cursor)
		{
		case 0:
			if (tcpipAvailable)
				M_Menu_Net_f ();
			break;

		case 1:
			if (tcpipAvailable)
				M_Menu_Net_f ();
			break;

		case 2:
			M_Menu_Setup_f ();
			break;
		}
	}
}

//=============================================================================
/* SETUP MENU */

int		setup_cursor = 4;
int		setup_cursor_table[] = {40, 56, 80, 104, 140};

char	setup_hostname[16];
char	setup_myname[16];
int		setup_oldtop;
int		setup_oldbottom;
int		setup_top;
int		setup_bottom;

#define	NUM_SETUP_CMDS	5

void M_Menu_Setup_f (void)
{
	key_dest = key_menu;
	m_state = m_setup;
	m_entersound = true;
	strcpy(setup_myname, cl_name.string);
	strcpy(setup_hostname, hostname.string);
	setup_top = setup_oldtop = 4;
	setup_bottom = setup_oldbottom = 15;
}


void M_Setup_Draw (void)
{
	/*
	qpic_t	*p;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	M_Print (64, 40, "Hostname");
	M_Print (168, 40, setup_hostname);

	M_Print (64, 56, "Your name");
	M_Print (168, 56, setup_myname);

	M_Print (64, 80, "Shirt color");
	M_Print (64, 104, "Pants color");

	M_Print (72, 140, "Accept Changes");

	p = Draw_CachePic ("gfx/bigbox.lmp");
	M_DrawTransPic (160, 64, p);
	p = Draw_CachePic ("gfx/menuplyr.lmp");
	M_BuildTranslationTable(setup_top*16, setup_bottom*16);
	M_DrawTransPicTranslate (172, 72, p);

	M_DrawCharacter (56, setup_cursor_table [setup_cursor], 12+((int)(realtime*4)&1));

	if (setup_cursor == 0)
		M_DrawCharacter (168 + 8*strlen(setup_hostname), setup_cursor_table [setup_cursor], 10+((int)(realtime*4)&1));

	if (setup_cursor == 1)
		M_DrawCharacter (168 + 8*strlen(setup_myname), setup_cursor_table [setup_cursor], 10+((int)(realtime*4)&1));
	*/
}


void M_Setup_Key (int k)
{
	int			l;

	switch (k)
	{
	case K_ESCAPE:
	case K_JOY1:
	case K_JOY10:
	case K_JOY21:
		M_Menu_MultiPlayer_f ();
		break;

	case K_UPARROW:
		//S_LocalSound ("misc/menu1.wav");
		setup_cursor--;
		if (setup_cursor < 0)
			setup_cursor = NUM_SETUP_CMDS-1;
		break;

	case K_DOWNARROW:
		//S_LocalSound ("misc/menu1.wav");
		setup_cursor++;
		if (setup_cursor >= NUM_SETUP_CMDS)
			setup_cursor = 0;
		break;

	case K_LEFTARROW:
		if (setup_cursor < 2)
			return;
		//S_LocalSound ("misc/menu3.wav");
		if (setup_cursor == 2)
			setup_top = setup_top - 1;
		if (setup_cursor == 3)
			setup_bottom = setup_bottom - 1;
		break;
	case K_RIGHTARROW:
		if (setup_cursor < 2)
			return;
forward:
		//S_LocalSound ("misc/menu3.wav");
		if (setup_cursor == 2)
			setup_top = setup_top + 1;
		if (setup_cursor == 3)
			setup_bottom = setup_bottom + 1;
		break;

	case K_ENTER:
	case K_JOY0:
	case K_JOY9:
	case K_JOY20:
		if (setup_cursor == 0 || setup_cursor == 1)
			return;

		if (setup_cursor == 2 || setup_cursor == 3)
			goto forward;

		// setup_cursor == 4 (OK)
		if (strcmp(cl_name.string, setup_myname) != 0)
			Cbuf_AddText ( va ("name \"%s\"\n", setup_myname) );
		if (strcmp(hostname.string, setup_hostname) != 0)
			Cvar_Set("hostname", setup_hostname);
		if (setup_top != setup_oldtop || setup_bottom != setup_oldbottom)
			Cbuf_AddText( va ("color %i %i\n", setup_top, setup_bottom) );
		m_entersound = true;
		M_Menu_MultiPlayer_f ();
		break;

	case K_BACKSPACE:
		if (setup_cursor == 0)
		{
			if (strlen(setup_hostname))
				setup_hostname[strlen(setup_hostname)-1] = 0;
		}

		if (setup_cursor == 1)
		{
			if (strlen(setup_myname))
				setup_myname[strlen(setup_myname)-1] = 0;
		}
		break;

	default:
		if (k < 32 || k > 127)
			break;
		if (setup_cursor == 0)
		{
			l = strlen(setup_hostname);
			if (l < 15)
			{
				setup_hostname[l+1] = 0;
				setup_hostname[l] = k;
			}
		}
		if (setup_cursor == 1)
		{
			l = strlen(setup_myname);
			if (l < 15)
			{
				setup_myname[l+1] = 0;
				setup_myname[l] = k;
			}
		}
	}

	if (setup_top > 13)
		setup_top = 0;
	if (setup_top < 0)
		setup_top = 13;
	if (setup_bottom > 13)
		setup_bottom = 0;
	if (setup_bottom < 0)
		setup_bottom = 13;
}

//=============================================================================
/* NET MENU */

int	m_net_cursor;
int m_net_items;
int m_net_saveHeight;

char *net_helpMessage [] =
{
/* .........1.........2.... */
  "                        ",
  " Two computers connected",
  "   through two modems.  ",
  "                        ",

  "                        ",
  " Two computers connected",
  " by a null-modem cable. ",
  "                        ",

  " Novell network LANs    ",
  " or Windows 95 DOS-box. ",
  "                        ",
  "(LAN=Local Area Network)",

  " Commonly used to play  ",
  " over the Internet, but ",
  " also used on a Local   ",
  " Area Network.          "
};

void M_Menu_Net_f (void)
{
	key_dest = key_menu;
	m_state = m_net;
	m_entersound = true;
	m_net_items = 4;

	if (m_net_cursor >= m_net_items)
		m_net_cursor = 0;
	m_net_cursor--;
	M_Net_Key (K_DOWNARROW);
}


void M_Net_Draw (void)
{
	/*
	int		f;
	qpic_t	*p;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	f = 32;

	p = Draw_CachePic ("gfx/dim_modm.lmp");

	if (p)
		M_DrawTransPic (72, f, p);

	f += 19;

	p = Draw_CachePic ("gfx/dim_drct.lmp");

	if (p)
		M_DrawTransPic (72, f, p);

	f += 19;
	p = Draw_CachePic ("gfx/dim_ipx.lmp");
	M_DrawTransPic (72, f, p);

	f += 19;
	if (tcpipAvailable)
		p = Draw_CachePic ("gfx/netmen4.lmp");
	else
		p = Draw_CachePic ("gfx/dim_tcp.lmp");
	M_DrawTransPic (72, f, p);

	if (m_net_items == 5)	// JDC, could just be removed
	{
		f += 19;
		p = Draw_CachePic ("gfx/netmen5.lmp");
		M_DrawTransPic (72, f, p);
	}

	f = (320-26*8)/2;
	f += 8;
	M_Print (f, 142, net_helpMessage[m_net_cursor*4+0]);
	M_Print (f, 150, net_helpMessage[m_net_cursor*4+1]);
	M_Print (f, 158, net_helpMessage[m_net_cursor*4+2]);
	M_Print (f, 166, net_helpMessage[m_net_cursor*4+3]);

	f = (int)(host_time * 10)%6;
	M_DrawTransPic (54, 32 + m_net_cursor * 20,Draw_CachePic( va("gfx/menudot%i.lmp", f+1 ) ) );
	*/
}


void M_Net_Key (int k)
{
again:
	switch (k)
	{
	case K_ESCAPE:
	case K_JOY1:
	case K_JOY10:
	case K_JOY21:
		M_Menu_MultiPlayer_f ();
		break;

	case K_DOWNARROW:
		//S_LocalSound ("misc/menu1.wav");
		if (++m_net_cursor >= m_net_items)
			m_net_cursor = 0;
		break;

	case K_UPARROW:
		//S_LocalSound ("misc/menu1.wav");
		if (--m_net_cursor < 0)
			m_net_cursor = m_net_items - 1;
		break;

	case K_ENTER:
	case K_JOY0:
	case K_JOY9:
	case K_JOY20:
		m_entersound = true;

		switch (m_net_cursor)
		{
		case 0:
			break;

		case 1:
			break;

		case 2:
			break;

		case 3:
			M_Menu_LanConfig_f ();
			break;

		case 4:
// multiprotocol
			break;
		}
	}

	if (m_net_cursor == 0)
		goto again;
	if (m_net_cursor == 1)
		goto again;
	if (m_net_cursor == 2)
		goto again;
	if (m_net_cursor == 3 && !tcpipAvailable)
		goto again;
}

//=============================================================================
/* OPTIONS MENU */

#define	OPTIONS_ITEMS	15

#define	SLIDER_RANGE	10

extern cvar_t	nunchuk_stick_as_arrows;
extern cvar_t	cl_weapon_inrollangle;
int		options_cursor;

void M_Menu_Options_f (void)
{
	if (key_dest != key_menu_pause)
		key_dest = key_menu;
	
	m_state = m_options;
	
	m_entersound = true;
}

void M_AdjustSliders (int dir)
{
	//S_LocalSound ("misc/menu3.wav");

	switch (options_cursor)
	{
	case 3:	// screen size
		scr_viewsize.value += dir * 10;
		if (scr_viewsize.value < 100)
			scr_viewsize.value = 100;
		if (scr_viewsize.value > 120)
			scr_viewsize.value = 120;
		Cvar_SetValue ("viewsize", scr_viewsize.value);
		break;
	case 4:	// gamma
		v_gamma.value -= dir * 0.05;
		if (v_gamma.value < 0.5f)
			v_gamma.value = 0.5f;
		if (v_gamma.value > 1)
			v_gamma.value = 1;
		Cvar_SetValue ("gamma", v_gamma.value);
		break;
	case 5:	// mouse speed
		sensitivity.value += dir * 0.5f;
		if (sensitivity.value < 1)
			sensitivity.value = 1;
		if (sensitivity.value > 11)
			sensitivity.value = 11;
		Cvar_SetValue ("sensitivity", sensitivity.value);
		break;
	case 6:	// music volume
		bgmvolume.value += dir * 0.1f;
		if (bgmvolume.value < 0)
			bgmvolume.value = 0;
		if (bgmvolume.value > 1)
			bgmvolume.value = 1;
		Cvar_SetValue ("bgmvolume", bgmvolume.value);
		break;
	case 7:	// sfx volume
		volume.value += dir * 0.1f;
		if (volume.value < 0)
			volume.value = 0;
		if (volume.value > 1)
			volume.value = 1;
		Cvar_SetValue ("volume", volume.value);
		break;

	case 8:	// Nunchuk stick as arrows
		Cvar_SetValue ("nunchuk_stick_as_arrows", !nunchuk_stick_as_arrows.value);
		break;
		
	case 9:	// Aim assist
		Cvar_SetValue ("in_aimassist", !in_aimassist.value);
		break;	
	case 10: // ADS Always Centered
		Cvar_SetValue ("ads_center", !ads_center.value);
		break;
	case 11: // Sniper Scope Always Centered
		Cvar_SetValue ("sniper_center", !sniper_center.value);
		break;
	case 12:	// weapon roll by input
		Cvar_SetValue ("cl_weapon_inrollangle", !cl_weapon_inrollangle.value);
		break;
	case 13:	// tv border
		vid_tvborder.value += dir * 0.005f;
		if (vid_tvborder.value < 0)
			vid_tvborder.value = 0;
		if (vid_tvborder.value > 0.2)
			vid_tvborder.value = 0.2;
		Cvar_SetValue ("vid_tvborder", vid_tvborder.value);
		break;		
	case 14:	// retro mode
		Cvar_SetValue ("vid_retromode", !vid_retromode.value);
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
		M_Print (x, y, "ON");
	else
		M_Print (x, y, "OFF");
}

void M_Options_Draw (void)
{
	float		r;
	
	if (key_dest != key_menu_pause)
		Draw_StretchPic (0, 0, menu_bk, vid.width, vid.height);

	M_Print (16, 40, "    Customize controls");
	M_Print (16, 52, "         Go to console");
	M_Print (16, 64, "     Reset to defaults");

	M_Print (16, 76, "           Screen size");
	r = (scr_viewsize.value - 100) / (120 - 100);
	M_DrawSlider (220, 76, r);

	M_Print (16, 88, "            Brightness");
	r = (1.0f - v_gamma.value) / 0.5f;
	M_DrawSlider (220, 88, r);

	M_Print (16, 100, "      	 Sensitivity");
	r = (sensitivity.value - 1)/10;
	M_DrawSlider (220, 100, r);

	M_Print (16, 112, "      CD Music Volume");
	r = bgmvolume.value;
	M_DrawSlider (220, 112, r);

	M_Print (16, 124, "         Sound Volume");
	r = volume.value;
	M_DrawSlider (220, 124, r);

	M_Print (16, 136,"    NK stick as arrows");
	M_DrawCheckbox (215, 136, nunchuk_stick_as_arrows.value);
	
	M_Print (16, 148,"            Aim Assist");
	M_DrawCheckbox (215, 148, in_aimassist.value);
	
	M_Print (16, 160,"   ADS Always Centered");
	M_DrawCheckbox (215, 160, ads_center.value);
	
	M_Print (16, 172," Scope Always Centered");
	M_DrawCheckbox (215, 172, sniper_center.value);

	M_Print (16, 184, "          Weapon Roll");
	M_DrawCheckbox (215, 184, cl_weapon_inrollangle.value);

	M_Print (16, 196, "          TV Overscan");
	r = vid_tvborder.value / 0.2f;
	M_DrawSlider (220, 196, r);
	
	M_Print (16, 208, "           Retro Mode");
	M_DrawCheckbox (215, 208, vid_retromode.value);

// cursor
	M_DrawCharacter (200, 40 + options_cursor*12, 12+((int)(realtime*4)&1));
}

extern qboolean console_enabled;
void M_Options_Key (int k)
{
	switch (k)
	{	
	case K_ENTER:
	case K_JOY0:
	case K_JOY9:
	case K_JOY20:
		m_entersound = true;
		switch (options_cursor)
		{
		case 0:
			M_Menu_Keys_f ();
			break;
		case 1:
			console_enabled = true;
			m_state = m_none;
			Con_Printf("\nPress MINUS to open keyboard.\n\n");
			Con_ToggleConsole_f ();
			break;
		case 2:
			Cbuf_AddText ("exec default.cfg\n");
			break;
		default:
			M_AdjustSliders (1);
			break;
		}
		return;

	case K_UPARROW:
		S_LocalSound ("sounds/menu/navigate.wav");
		options_cursor--;
		if (options_cursor < 0)
			options_cursor = OPTIONS_ITEMS-1;
		break;

	case K_DOWNARROW:
		S_LocalSound ("sounds/menu/navigate.wav");
		options_cursor++;
		if (options_cursor >= OPTIONS_ITEMS)
			options_cursor = 0;
		break;
		
	case K_ESCAPE:
	case K_JOY1:
		if (key_dest == key_menu_pause)
			M_Paused_Menu_f();
		else
			M_Menu_Main_f ();
		break;

	case K_LEFTARROW:
		M_AdjustSliders (-1);
		break;

	case K_RIGHTARROW:
		M_AdjustSliders (1);
		break;
	}
}

//=============================================================================
/* KEYS MENU */

char *bindnames[][2] =
{
{"+attack", 		"Attack"},
{"+switch", 		"Change Weapon"},
{"+reload",			"Reload"},
{"impulse 23", 		"Sprint"},
{"+aim", 			"Aim Down Sight"},
{"+knife", 			"Knife"},
{"+grenade", 		"Grenade"},
{"+jump", 			"Jump"},
{"impulse 30",		"Change Stance"},
{"impulse 33", 		"Place Betty"},
{"showscores", 		"Show Scoreboard"},
{"+forward", 		"Walk Forward"},
{"+back", 			"Walk Backward"},
{"+moveleft", 		"Step Left"},
{"+moveright", 		"Step Right"},
};

#define	NUMCOMMANDS	(sizeof(bindnames)/sizeof(bindnames[0]))

int		keys_cursor;
int		bind_grab;

void M_Menu_Keys_f (void)
{
	if (key_dest != key_menu_pause)
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


void M_Keys_Draw (void)
{
	int		i/*, l*/;
	int		keys[2];
	char	*name;
	int		x, y;
	//qpic_t	*p;

	//p = Draw_CachePic ("gfx/ttl_cstm.lmp");
	//M_DrawPic ( (320-p->width)/2, 4, p);

	if (bind_grab)
		M_Print (12, 32, "Press a key or button for this action");
	else
		M_Print (18, 32, "Enter to change, backspace to clear");

// search for known bindings
	for (i=0 ; i<NUMCOMMANDS ; i++)
	{
		y = 48 + 8*i;

		M_Print (16, y, bindnames[i][1]);

		//l = strlen (bindnames[i][0]);

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
		S_LocalSound ("sounds/menu/navigate.wav");
		if ((k == K_ESCAPE) || (k == K_JOY1) || (k == K_JOY10)|| (k == K_JOY21))
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
	case K_JOY1:
	case K_JOY10:
	case K_JOY21:
		M_Menu_Options_f ();
		break;

	case K_LEFTARROW:
	case K_UPARROW:
		//S_LocalSound ("misc/menu1.wav");
		keys_cursor--;
		if (keys_cursor < 0)
			keys_cursor = NUMCOMMANDS-1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		//S_LocalSound ("misc/menu1.wav");
		keys_cursor++;
		if (keys_cursor >= NUMCOMMANDS)
			keys_cursor = 0;
		break;

	case K_ENTER:   // go into bind mode
	case K_JOY0:
	case K_JOY9:
	case K_JOY20:
		M_FindKeysForCommand (bindnames[keys_cursor][0], keys);
		//S_LocalSound ("misc/menu2.wav");
		if (keys[1] != -1)
			M_UnbindCommand (bindnames[keys_cursor][0]);
		bind_grab = true;
		break;

	case K_BACKSPACE:		// delete bindings
	case K_DEL:				// delete bindings
		//S_LocalSound ("misc/menu2.wav");
		M_UnbindCommand (bindnames[keys_cursor][0]);
		break;
	}
}

//=============================================================================
/* CONSOLE OSK */

#define CHAR_SIZE 8
#define MAX_Y 8
#define MAX_X 12

#define MAX_CHAR_LINE 36
#define MAX_CHAR      72

int  osk_pos_x = 0;
int  osk_pos_y = 0;
int  max_len   = 0;
int  m_old_state = 0;

char* osk_out_buff = NULL;
char  osk_buffer[128];

char *osk_help [] =
{
	"ADD:   ",
	"A      ",
	"       ",
	"DELETE:",
	"B      ",
	"       ",
	"CLOSE: ",
	"MINUS  ",
	"       "
};

char *osk_text [] =
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

void M_OSK_Draw (void) {

	int x,y;
	int i;

	char *selected_line = osk_text[osk_pos_y];
	char selected_char[2];

	//GL_SetCanvas(CANVAS_MENU);

	selected_char[0] = selected_line[1+(2*osk_pos_x)];
	selected_char[1] = '\0';
	if (selected_char[0] == ' ' || selected_char[0] == '\t')
		selected_char[0] = 'X';

	y = 150;
	x = 150;

	M_DrawTextBox (x-3, y-10, 		     26, 10);
	M_DrawTextBox ((x-3)+(26*CHAR_SIZE),    y-10,  10, 10);
	M_DrawTextBox (x-3, (y-10)+(10*CHAR_SIZE),36,  3);

	for(i=0;i<=MAX_Y;i++)
	{
		M_PrintWhite (x, y+(CHAR_SIZE*i), osk_text[i]);
		if (i == 1 || i == 4 || i == 7)
			M_Print      (x+(27*CHAR_SIZE), y+(CHAR_SIZE*i), osk_help[i]);
		else
			M_PrintWhite (x+(27*CHAR_SIZE), y+(CHAR_SIZE*i), osk_help[i]);
	}

	int text_len = strlen(osk_buffer);
	if (text_len > MAX_CHAR_LINE) {

		char oneline[MAX_CHAR_LINE+1];
		strncpy(oneline,osk_buffer,MAX_CHAR_LINE);
		oneline[MAX_CHAR_LINE] = '\0';

		M_Print (x+4, y+4+(CHAR_SIZE*(MAX_Y+2)), oneline );

		strncpy(oneline,osk_buffer+MAX_CHAR_LINE, text_len - MAX_CHAR_LINE);
		oneline[text_len - MAX_CHAR_LINE] = '\0';

		M_Print (x+4, y+4+(CHAR_SIZE*(MAX_Y+3)), oneline );
		M_PrintWhite (x+4+(CHAR_SIZE*(text_len - MAX_CHAR_LINE)), y+4+(CHAR_SIZE*(MAX_Y+3)),"_");
	}
	else {
		M_Print (x+4, y+4+(CHAR_SIZE*(MAX_Y+2)), osk_buffer );
		M_PrintWhite (x+4+(CHAR_SIZE*(text_len)), y+4+(CHAR_SIZE*(MAX_Y+2)),"_");
	}
	M_Print      (x+((((osk_pos_x)*2)+1)*CHAR_SIZE), y+(osk_pos_y*CHAR_SIZE), selected_char);
}


//=============================================================================
/* HELP MENU */

int		help_page;
#define	NUM_HELP_PAGES	6


void M_Menu_Help_f (void)
{
	key_dest = key_menu;
	m_state = m_help;
	m_entersound = true;
	help_page = 0;
}



void M_Help_Draw (void)
{
	//M_DrawPic (0, 0, Draw_CachePic ( va("gfx/help%i.lmp", help_page)) );
}


void M_Help_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
	case K_JOY1:
	case K_JOY10:
	case K_JOY21:
		M_Menu_Main_f ();
		break;

	case K_UPARROW:
	case K_RIGHTARROW:
		m_entersound = true;
		if (++help_page >= NUM_HELP_PAGES)
			help_page = 0;
		break;

	case K_DOWNARROW:
	case K_LEFTARROW:
		m_entersound = true;
		if (--help_page < 0)
			help_page = NUM_HELP_PAGES-1;
		break;
	}

}

//=============================================================================
/* QUIT MENU */

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
}


void M_Quit_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
	case K_JOY1:
	case K_JOY10:
	case K_JOY21:
	case 'n':
	case 'N':
		if (wasInMenus)
		{
			m_state = m_quit_prevstate;
			m_entersound = true;
		}
		else
		{
			key_dest = key_game;
			m_state = m_none;
		}
		break;

	case K_ENTER:
	case K_JOY0:
	case K_JOY9:
	case K_JOY20:
	case 'Y':
	case 'y':
		key_dest = key_console;
		Host_Quit_f ();
		break;

	default:
		break;
	}

}


void M_Quit_Draw (void)
{
	char	yes[32];
	char	no[32];

	if (wasInMenus)
	{
		m_state = m_quit_prevstate;
		m_recursiveDraw = true;
		M_Draw ();
		m_state = m_quit;
	}

	sprintf(yes, "Y or A button: Yes");
	sprintf(no, "N or B button: No");

	M_Print (64, 84,  "Really quit?");
	M_Print (64, 92,  "");
	M_Print (64, 100, yes);
	M_Print (64, 108, no);
}

//=============================================================================
/* LAN CONFIG MENU */

int		lanConfig_cursor = -1;
int		lanConfig_cursor_table [] = {72, 92, 124};
#define NUM_LANCONFIG_CMDS	3

int 	lanConfig_port;
char	lanConfig_portname[6];
char	lanConfig_joinname[22];

void M_Menu_LanConfig_f (void)
{
	key_dest = key_menu;
	m_state = m_lanconfig;
	m_entersound = true;
	if (lanConfig_cursor == -1)
	{
		if (JoiningGame && TCPIPConfig)
			lanConfig_cursor = 2;
		else
			lanConfig_cursor = 1;
	}
	if (StartingGame && lanConfig_cursor == 2)
		lanConfig_cursor = 1;
	lanConfig_port = DEFAULTnet_hostport;
	sprintf(lanConfig_portname, "%u", lanConfig_port);

	m_return_onerror = false;
	m_return_reason[0] = 0;
}


void M_LanConfig_Draw (void)
{
	/*
	qpic_t	*p;
	int		basex;
	char	*startJoin;
	char	*protocol;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_multi.lmp");
	basex = (320-p->width)/2;
	M_DrawPic (basex, 4, p);

	if (StartingGame)
		startJoin = "New Game";
	else
		startJoin = "Join Game";
	protocol = "TCP/IP";
	M_Print (basex, 32, va ("%s - %s", startJoin, protocol));
	basex += 8;

	M_Print (basex, 52, "Address:");
	M_Print (basex+9*8, 52, my_tcpip_address);

	M_Print (basex, lanConfig_cursor_table[0], "Port");
	M_Print (basex+9*8, lanConfig_cursor_table[0], lanConfig_portname);

	if (JoiningGame)
	{
		M_Print (basex, lanConfig_cursor_table[1], "Search for local games...");
		M_Print (basex, 108, "Join game at:");
		M_Print (basex+16, lanConfig_cursor_table[2], lanConfig_joinname);
	}
	else
	{
		M_Print (basex+8, lanConfig_cursor_table[1], "OK");
	}

	M_DrawCharacter (basex-8, lanConfig_cursor_table [lanConfig_cursor], 12+((int)(realtime*4)&1));

	if (lanConfig_cursor == 0)
		M_DrawCharacter (basex+9*8 + 8*strlen(lanConfig_portname), lanConfig_cursor_table [0], 10+((int)(realtime*4)&1));

	if (lanConfig_cursor == 2)
		M_DrawCharacter (basex+16 + 8*strlen(lanConfig_joinname), lanConfig_cursor_table [2], 10+((int)(realtime*4)&1));

	if (*m_return_reason)
		M_PrintWhite (basex, 148, m_return_reason);
	*/
}


void M_LanConfig_Key (int key)
{
	int		l;

	switch (key)
	{
	case K_ESCAPE:
	case K_JOY1:
	case K_JOY10:
	case K_JOY21:
		M_Menu_Net_f ();
		break;

	case K_UPARROW:
		//S_LocalSound ("misc/menu1.wav");
		lanConfig_cursor--;
		if (lanConfig_cursor < 0)
			lanConfig_cursor = NUM_LANCONFIG_CMDS-1;
		break;

	case K_DOWNARROW:
		//S_LocalSound ("misc/menu1.wav");
		lanConfig_cursor++;
		if (lanConfig_cursor >= NUM_LANCONFIG_CMDS)
			lanConfig_cursor = 0;
		break;

	case K_ENTER:
	case K_JOY0:
	case K_JOY9:
	case K_JOY20:
		if (lanConfig_cursor == 0)
			break;

		m_entersound = true;

		M_ConfigureNetSubsystem ();

		if (lanConfig_cursor == 1)
		{
			if (StartingGame)
			{
				M_Menu_GameOptions_f ();
				break;
			}
			M_Menu_Search_f();
			break;
		}

		if (lanConfig_cursor == 2)
		{
			m_return_state = m_state;
			m_return_onerror = true;
			key_dest = key_game;
			m_state = m_none;
			Cbuf_AddText ( va ("connect \"%s\"\n", lanConfig_joinname) );
			break;
		}

		break;

	case K_BACKSPACE:
		if (lanConfig_cursor == 0)
		{
			if (strlen(lanConfig_portname))
				lanConfig_portname[strlen(lanConfig_portname)-1] = 0;
		}

		if (lanConfig_cursor == 2)
		{
			if (strlen(lanConfig_joinname))
				lanConfig_joinname[strlen(lanConfig_joinname)-1] = 0;
		}
		break;

	default:
		if (key < 32 || key > 127)
			break;

		if (lanConfig_cursor == 2)
		{
			l = strlen(lanConfig_joinname);
			if (l < 21)
			{
				lanConfig_joinname[l+1] = 0;
				lanConfig_joinname[l] = key;
			}
		}

		if (key < '0' || key > '9')
			break;
		if (lanConfig_cursor == 0)
		{
			l = strlen(lanConfig_portname);
			if (l < 5)
			{
				lanConfig_portname[l+1] = 0;
				lanConfig_portname[l] = key;
			}
		}
	}

	if (StartingGame && lanConfig_cursor == 2)
	{
		if (key == K_UPARROW)
			lanConfig_cursor = 1;
		else
			lanConfig_cursor = 0;
	}

	l =  atoi(lanConfig_portname);
	if (l > 65535)
		l = lanConfig_port;
	else
		lanConfig_port = l;
	sprintf(lanConfig_portname, "%u", lanConfig_port);
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
	/*
	qpic_t	*p;
	int		x;

	M_DrawTransPic (16, 4, Draw_CachePic ("gfx/qplaque.lmp") );
	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);

	M_Print (160, 40, "begin game");

	M_Print (0, 56, "      Max players");
	M_Print (160, 56, va("%i", maxplayers) );

	M_Print (0, 64, "        Game Type");
	if (coop.value)
		M_Print (160, 64, "Cooperative");
	else
		M_Print (160, 64, "Deathmatch");

	M_Print (0, 72, "        Teamplay");
	if (rogue)
	{
		char *msg;

		switch((int)teamplay.value)
		{
			case 1: msg = "No Friendly Fire"; break;
			case 2: msg = "Friendly Fire"; break;
			case 3: msg = "Tag"; break;
			case 4: msg = "Capture the Flag"; break;
			case 5: msg = "One Flag CTF"; break;
			case 6: msg = "Three Team CTF"; break;
			default: msg = "Off"; break;
		}
		M_Print (160, 72, msg);
	}
	else
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
   //MED 01/06/97 added hipnotic episodes
   if (hipnotic)
      M_Print (160, 112, hipnoticepisodes[startepisode].description);
   //PGM 01/07/97 added rogue episodes
   else if (rogue)
      M_Print (160, 112, rogueepisodes[startepisode].description);
   else
      M_Print (160, 112, episodes[startepisode].description);

	M_Print (0, 120, "           Level");
   //MED 01/06/97 added hipnotic episodes
   if (hipnotic)
   {
      M_Print (160, 120, hipnoticlevels[hipnoticepisodes[startepisode].firstLevel + startlevel].description);
      M_Print (160, 128, hipnoticlevels[hipnoticepisodes[startepisode].firstLevel + startlevel].name);
   }
   //PGM 01/07/97 added rogue episodes
   else if (rogue)
   {
      M_Print (160, 120, roguelevels[rogueepisodes[startepisode].firstLevel + startlevel].description);
      M_Print (160, 128, roguelevels[rogueepisodes[startepisode].firstLevel + startlevel].name);
   }
   else
   {
      M_Print (160, 120, levels[episodes[startepisode].firstLevel + startlevel].description);
      M_Print (160, 128, levels[episodes[startepisode].firstLevel + startlevel].name);
   }

// line cursor
	M_DrawCharacter (144, gameoptions_cursor_table[gameoptions_cursor], 12+((int)(realtime*4)&1));

	if (m_serverInfoMessage)
	{
		if ((realtime - m_serverInfoMessageTime) < 5.0)
		{
			x = (320-26*8)/2;
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
	*/
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
		if (rogue)
			count = 6;
		else
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
	//MED 01/06/97 added hipnotic count
		if (hipnotic)
			count = 6;
	//PGM 01/07/97 added rogue count
	//PGM 03/02/97 added 1 for dmatch episode
		else if (rogue)
			count = 4;
		else if (registered.value)
			count = 7;
		else
			count = 2;

		if (startepisode < 0)
			startepisode = count - 1;

		if (startepisode >= count)
			startepisode = 0;

		startlevel = 0;
		break;

	case 8:
		startlevel += dir;
    //MED 01/06/97 added hipnotic episodes
		if (hipnotic)
			count = hipnoticepisodes[startepisode].levels;
	//PGM 01/06/97 added hipnotic episodes
		else if (rogue)
			count = rogueepisodes[startepisode].levels;
		else
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
	case K_ESCAPE:
	case K_JOY1:
	case K_JOY10:
	case K_JOY21:
		M_Menu_Net_f ();
		break;

	case K_UPARROW:
		//S_LocalSound ("misc/menu1.wav");
		gameoptions_cursor--;
		if (gameoptions_cursor < 0)
			gameoptions_cursor = NUM_GAMEOPTIONS-1;
		break;

	case K_DOWNARROW:
		//S_LocalSound ("misc/menu1.wav");
		gameoptions_cursor++;
		if (gameoptions_cursor >= NUM_GAMEOPTIONS)
			gameoptions_cursor = 0;
		break;

	case K_LEFTARROW:
		if (gameoptions_cursor == 0)
			break;
		//S_LocalSound ("misc/menu3.wav");
		M_NetStart_Change (-1);
		break;

	case K_RIGHTARROW:
		if (gameoptions_cursor == 0)
			break;
		//S_LocalSound ("misc/menu3.wav");
		M_NetStart_Change (1);
		break;

	case K_ENTER:
	case K_JOY0:
	case K_JOY9:
	case K_JOY20:
		//S_LocalSound ("misc/menu2.wav");
		if (gameoptions_cursor == 0)
		{
			if (sv.active)
				Cbuf_AddText ("disconnect\n");
			Cbuf_AddText ("listen 0\n");	// so host_netport will be re-examined
			Cbuf_AddText ( va ("maxplayers %u\n", maxplayers) );
			SCR_BeginLoadingPlaque ();

			if (hipnotic)
				Cbuf_AddText ( va ("map %s\n", hipnoticlevels[hipnoticepisodes[startepisode].firstLevel + startlevel].name) );
			else if (rogue)
				Cbuf_AddText ( va ("map %s\n", roguelevels[rogueepisodes[startepisode].firstLevel + startlevel].name) );
			else
				Cbuf_AddText ( va ("map %s\n", levels[episodes[startepisode].firstLevel + startlevel].name) );

			return;
		}

		M_NetStart_Change (1);
		break;
	}
}

//=============================================================================
/* SEARCH MENU */

qboolean	searchComplete = false;
double		searchCompleteTime;

void M_Menu_Search_f (void)
{
	key_dest = key_menu;
	m_state = m_search;
	m_entersound = false;
	slistSilent = true;
	slistLocal = false;
	searchComplete = false;
	NET_Slist_f();

}


void M_Search_Draw (void)
{
	/*
	qpic_t	*p;
	int x;

	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);
	x = (320/2) - ((12*8)/2) + 4;
	M_Print (x, 40, "Searching...");

	if(slistInProgress)
	{
		NET_Poll();
		return;
	}

	if (! searchComplete)
	{
		searchComplete = true;
		searchCompleteTime = realtime;
	}

	if (hostCacheCount)
	{
		M_Menu_ServerList_f ();
		return;
	}

	M_PrintWhite ((320/2) - ((22*8)/2), 64, "No Quake servers found");
	if ((realtime - searchCompleteTime) < 3.0)
		return;

	M_Menu_LanConfig_f ();
	*/
}


void M_Search_Key (int key)
{
}

//=============================================================================
/* SLIST MENU */

int		slist_cursor;
qboolean slist_sorted;

void M_Menu_ServerList_f (void)
{
	key_dest = key_menu;
	m_state = m_slist;
	m_entersound = true;
	slist_cursor = 0;
	m_return_onerror = false;
	m_return_reason[0] = 0;
	slist_sorted = false;
}


void M_ServerList_Draw (void)
{
	/*
	int		n;
	char	string [64];
	qpic_t	*p;

	if (!slist_sorted)
	{
		if (hostCacheCount > 1)
		{
			int	i,j;
			hostcache_t temp;
			for (i = 0; i < hostCacheCount; i++)
				for (j = i+1; j < hostCacheCount; j++)
					if (strcmp(hostcache[j].name, hostcache[i].name) < 0)
					{
						memcpy(&temp, &hostcache[j], sizeof(hostcache_t));
						memcpy(&hostcache[j], &hostcache[i], sizeof(hostcache_t));
						memcpy(&hostcache[i], &temp, sizeof(hostcache_t));
					}
		}
		slist_sorted = true;
	}

	p = Draw_CachePic ("gfx/p_multi.lmp");
	M_DrawPic ( (320-p->width)/2, 4, p);
	for (n = 0; n < hostCacheCount; n++)
	{
		if (hostcache[n].maxusers)
			sprintf(string, "%-15.15s %-15.15s %2u/%2u\n", hostcache[n].name, hostcache[n].map, hostcache[n].users, hostcache[n].maxusers);
		else
			sprintf(string, "%-15.15s %-15.15s\n", hostcache[n].name, hostcache[n].map);
		M_Print (16, 32 + 8*n, string);
	}
	M_DrawCharacter (0, 32 + slist_cursor*8, 12+((int)(realtime*4)&1));

	if (*m_return_reason)
		M_PrintWhite (16, 148, m_return_reason);
	*/
}


void M_ServerList_Key (int k)
{
	switch (k)
	{
	case K_ESCAPE:
	case K_JOY1:
	case K_JOY10:
	case K_JOY21:
		M_Menu_LanConfig_f ();
		break;

	case K_SPACE:
		M_Menu_Search_f ();
		break;

	case K_UPARROW:
	case K_LEFTARROW:
		//S_LocalSound ("misc/menu1.wav");
		slist_cursor--;
		if (slist_cursor < 0)
			slist_cursor = hostCacheCount - 1;
		break;

	case K_DOWNARROW:
	case K_RIGHTARROW:
		//S_LocalSound ("misc/menu1.wav");
		slist_cursor++;
		if (slist_cursor >= hostCacheCount)
			slist_cursor = 0;
		break;

	case K_ENTER:
	case K_JOY0:
	case K_JOY9:
	case K_JOY20:
		//S_LocalSound ("misc/menu2.wav");
		m_return_state = m_state;
		m_return_onerror = true;
		slist_sorted = false;
		key_dest = key_game;
		m_state = m_none;
		Cbuf_AddText ( va ("connect \"%s\"\n", hostcache[slist_cursor].cname) );
		break;

	default:
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
	//(*vid_menudrawfn) ();
}


void M_Video_Key (int key)
{
	//(*vid_menukeyfn) (key);
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

	//M_Load_Menu_Pics();
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
			Draw_ConsoleBackground (vid.conheight);
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
		//M_Video_Draw ();
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

	case m_paused_menu:
		M_Paused_Menu_Key (key);
		break;
		
	case m_start:
		M_Start_Key (key);
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
		//M_Video_Key (key);
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


void M_ConfigureNetSubsystem(void)
{
// enable/disable net systems to match desired config

	Cbuf_AddText ("stopdemo\n");

	if (TCPIPConfig)
		net_hostport = lanConfig_port;
}