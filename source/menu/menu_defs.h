///////////////////////////
///////////////////////////
///////////////////////////
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
#define m_lobby			6
#define m_gamesettings	7
#define	m_setup			8
#define	m_net			9
#define	m_options		10
#define	m_video			11
#define	m_keys			12
#define	m_quit			13
#define	m_restart		14
#define	m_credits		15
#define	m_exit			16
#define	m_serialconfig	17
#define	m_modemconfig	18
#define	m_lanconfig		19
#define	m_gameoptions	20
#define	m_search		21
#define	m_slist			22
///////////////////////////
///////////////////////////
///////////////////////////

// Menu page cursor management
extern int	            menu_main_cursor;
extern int              menu_stockmaps_cursor;
extern int				menu_lobby_cursor;
extern int				menu_gamesettings_cursor;

extern qboolean	        m_recursiveDraw;

// Menu scale factor set per platform
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

#define					MENU_COLOR_WHITE		0
#define					MENU_COLOR_YELLOW		1

#define					MENU_SLIDER_RIGHT		1
#define					MENU_SLIDER_LEFT		-1

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
extern image_t          menu_social;

extern char*            game_build_date;

// Loading screens
extern int          	loadingScreen;
extern qboolean         loadscreeninit;

// Custom maps
typedef struct
{
	int 		occupied;
	int 	 	map_allow_game_settings;
	int 	 	map_use_thumbnail;
	char* 		map_name;
	char* 		map_name_pretty;
	char* 		map_desc[8];
	char* 		map_author;
	char* 		map_thumbnail_path;
} usermap_t;

#define                 MAX_CUSTOMMAPS 64
extern usermap_t        custom_maps[MAX_CUSTOMMAPS];
extern image_t          menu_usermap_image[MAX_CUSTOMMAPS];
extern int              num_user_maps;
extern int              num_custom_images;
extern int     			custom_map_pages;

// Currently selected map
extern char*			current_selected_bsp;
extern char* 		    map_loadname;
extern char* 		    map_loadname_pretty;

// True if in single-player menu
extern qboolean		    menu_is_solo;

// bar heights are constant and used
// in multiple calculations
extern int 			    big_bar_height;
extern int 			    small_bar_height;

// Width and height of a character
// set by platform scale
extern float			CHAR_WIDTH;
extern float			CHAR_HEIGHT;

typedef struct
{
    char* bsp_name;
    int array_index;
} StockMaps;

extern StockMaps        stock_maps[4];
extern int 			    num_stock_maps;

// We need gamemode cvar values
extern cvar_t sv_gamemode;
extern cvar_t sv_difficulty;
extern cvar_t sv_startround;
extern cvar_t sv_magic;
extern cvar_t sv_headshotonly;
extern cvar_t sv_maxai;
extern cvar_t sv_fastrounds;


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
void Menu_InitStockMaps (void);
int UserMapSupportsCustomGameLookup (char *bsp_name);
void Map_SetDefaultValues (void);
void Menu_LoadMap (char *selected_map);
void Menu_DrawCustomBackground ();
void Menu_DrawTitle (char *title_name, int color);
void Menu_DrawButton (int order, int button_number, char* button_name, int button_active, char* button_summary);
void Menu_DrawOptionButton(int order, char* selection_name);
void Menu_DrawOptionSlider(int order, int max_option_value, cvar_t option);
void Menu_DrawMapButton (int order, int button_number, int usermap_index, char* bsp_name);
void Menu_DrawLobbyInfo (char* bsp_name, char* info_gamemode, char* info_difficulty, char* info_startround, char* info_magic, char* info_headshotonly, char* info_fastrounds, char* info_hordesize);
void Menu_DrawBuildDate ();
void Menu_DrawDivider (int order);
void Menu_DrawSocialBadge (int order, int which);
void Menu_DrawMapPanel (void);
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
void Menu_GameSettings_Set (void);

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
void Menu_GameSettings_Draw (void);

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
void Menu_GameSettings_Key (int key);

// Platform specifics
void Platform_Menu_MapFinder(void);
char *Platform_ReturnLoadingText (void);

#ifdef __PSP__
void Achievement_Init (void);
void Load_Achivements (void);
void Save_Achivements (void);
#endif //__PSP__