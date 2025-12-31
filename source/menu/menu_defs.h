#include "../../nzportable_def.h"

// ===========
// Menu state
// ===========
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

// play after drawing a frame, so caching 
// won't disrupt the sound
qboolean	        m_entersound;
qboolean	        m_recursiveDraw;

float               menu_scale_factor;

#define             MENU_SND_NAVIGATE   0
#define             MENU_SND_ENTER      1
#define             MENU_SND_BEEP       2

#define				BUTTON_DESELECTED	0
#define				BUTTON_SELECTED		1

char*               menu_background;
float               menu_changetime;
float               menu_starttime;

// Constant menu images
image_t             menu_bk;
image_t             menu_ndu;
image_t             menu_wh;
image_t             menu_wh2;
image_t             menu_ch;

char*               game_build_date;

/*
===========================================================================
===========================================================================
FUNCTION DEFINES
===========================================================================
===========================================================================
*/

void Menu_Start_Set(void);
void Menu_Main_Set(void);
void Menu_Paused_Set(void);

void Menu_Start_Draw(void);
void Menu_Paused_Draw(void);
void Menu_Main_Draw(void);
void Menu_SinglePlayer_Draw(void);
void Menu_Options_Draw(void);
void Menu_Keys_Draw(void);
void Menu_Video_Draw(void);
void Menu_Quit_Draw(void);
void Menu_Restart_Draw(void);
void Menu_Credits_Draw(void);
void Menu_Exit_Draw(void);
void Menu_GameOptions_Draw(void);
void Menu_CustomMaps_Draw(void);

void Menu_Start_Key(int key);
void Menu_Paused_Key(int key);
void Menu_Main_Key(int key);
void Menu_SinglePlayer_Key(int key);
void Menu_Options_Key(int key);
void Menu_Keys_Key(int key);
void Menu_Restart_Key(int key);
void Menu_Credits_Key(int key);
void Menu_Video_Key(int key);
void Menu_Quit_Key(int key);
void Menu_Exit_Key(int key);
void Menu_GameOptions_Key(int key);
void Menu_CustomMaps_Key(int key);

void Menu_ToggleMenu_f (void);