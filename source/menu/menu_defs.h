// ===========
// Menu state
// ===========
extern int m_state;

#define	m_none			0
#define	m_start			1
#define	m_main			2
#define	m_paused_menu	3
#define	m_stockmaps 	4
#define	m_custommaps	5
#define	m_setup			6
#define	m_net			7
#define	m_options		8
#define	m_video			9
#define	m_keys			10
#define	m_help			11
#define	m_quit			12
#define	m_restart		13
#define	m_credits		14
#define	m_exit			15
#define	m_serialconfig	16
#define	m_modemconfig	17
#define	m_lanconfig		18
#define	m_gameoptions	19
#define	m_search		20
#define	m_slist			21

extern int	            m_main_cursor;
extern int              m_stockmaps_cursor;

extern qboolean	        m_recursiveDraw;

extern float            menu_scale_factor;

#define                 MENU_SND_NAVIGATE   	0
#define                 MENU_SND_ENTER      	1
#define                 MENU_SND_BEEP       	2

#define				    MENU_BUTTON_ACTIVE		0
#define				    MENU_BUTTON_INACTIVE 	1

#define                 MENU_SOC_DOCS           1
#define                 MENU_SOC_YOUTUBE        2
#define                 MENU_SOC_BLUESKY        3
#define                 MENU_SOC_PATREON        4

// Platform specific character representing
// which button is default bound to "enter"
// a menu
extern char*            enter_char;

// Menu background and timer
extern image_t          menu_background;
extern float            menu_time;
extern float            menu_changetime;
extern float            menu_starttime;

// Constant menu images
extern image_t          menu_bk;
extern image_t          menu_ndu;
extern image_t          menu_wh;
extern image_t          menu_wh2;
extern image_t          menu_ch;
extern image_t		    menu_custom;
extern image_t          menu_social;

extern char*            game_build_date;

// Loading screens
extern image_t          loadingScreen;
extern qboolean         loadscreeninit;

// Custom maps
#define                 MAX_CUSTOMMAPS 64
extern int              user_maps_num;
extern int              num_custom_images;
extern image_t          menu_usermap_image[MAX_CUSTOMMAPS];

extern char* 		    map_loadname;
extern char* 		    map_loadname_pretty;

extern qboolean		    menu_is_solo;

/*
===========================================================================
===========================================================================
FUNCTION DEFINES
===========================================================================
===========================================================================
*/

void Menu_DictateScaleFactor(void);
void Menu_LoadPics (void);
void Menu_StartSound (int type);
image_t Menu_PickBackground ();
void Menu_DrawCustomBackground ();
void Menu_DrawTitle (char *title_name);
void Menu_DrawButton (int order, int button_number, char* button_name, int button_active, char* button_summary);
void Menu_DrawBuildDate ();
void Menu_DrawDivider (int order);
void Menu_DrawSocialBadge (int order, int which);
void Menu_DrawMapPanel (void);

void Menu_Map_Finder (void);
void Menu_Preload_Custom_Images (void);

void Menu_Start_Set(void);
void Menu_StockMaps_Set (void);
void Menu_Paused_Set(void);
void Menu_Options_Set(void);
void Menu_Keys_Set(void);
void Menu_Video_Set(void);
void Menu_Quit_Set(void);
void Menu_Restart_Set(void);
void Menu_Credits_Set(void);
void Menu_Exit_Set(void);
void Menu_GameOptions_Set(void);
void Menu_CustomMaps_Set(void);
void Menu_Lobby_Set(void);

void Menu_Start_Draw(void);
void Menu_Paused_Draw(void);
void Menu_Main_Draw(void);
void Menu_StockMaps_Draw(void);
void Menu_Options_Draw(void);
void Menu_Keys_Draw(void);
void Menu_Video_Draw(void);
void Menu_Quit_Draw(void);
void Menu_Restart_Draw(void);
void Menu_Credits_Draw(void);
void Menu_Exit_Draw(void);
void Menu_GameOptions_Draw(void);
void Menu_CustomMaps_Draw(void);
void Menu_Lobby_Draw(void);

void Menu_Start_Key(int key);
void Menu_Paused_Key(int key);
void Menu_Main_Key(int key);
void Menu_StockMaps_Key(int key);
void Menu_Options_Key(int key);
void Menu_Keys_Key(int key);
void Menu_Restart_Key(int key);
void Menu_Credits_Key(int key);
void Menu_Video_Key(int key);
void Menu_Quit_Key(int key);
void Menu_Exit_Key(int key);
void Menu_GameOptions_Key(int key);
void Menu_CustomMaps_Key(int key);
void Menu_Lobby_Key(int key);