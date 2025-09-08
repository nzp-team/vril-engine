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

void (*vid_menudrawfn)(void);
void (*vid_menukeyfn)(int keynum);

static void M_Menu_Main_f(void);
static void M_Main_Draw(void);
static void M_Main_Key(int keynum);

void M_Menu_Options_f(void);
static void M_Options_Draw(void);
static void M_Options_Key(int keynum);

static void M_Menu_Keys_f(void);
static void M_Keys_Draw(void);
static void M_Keys_Key(int keynum);

static void M_Menu_Video_f(void);
static void M_Video_Draw(void);
static void M_Video_Key(int keynum);

void M_Menu_Quit_f(void);
static void M_Quit_Draw(void);
static void M_Quit_Key(int keynum);

#ifdef NQ_HACK
static void M_Menu_SinglePlayer_f(void);
static void M_SinglePlayer_Draw(void);
static void M_SinglePlayer_Key(int keynum);

static void M_Menu_Setup_f(void);
static void M_Setup_Draw(void);
static void M_Setup_Key(int keynum);

static void M_Menu_Load_f(void);
static void M_Load_Draw(void);
static void M_Load_Key(int keynum);

static void M_Menu_Save_f(void);
static void M_Save_Draw(void);
static void M_Save_Key(int keynum);

static void M_Menu_MultiPlayer_f(void);
static void M_MultiPlayer_Draw(void);
static void M_MultiPlayer_Key(int keynum);

static void M_Menu_LanConfig_f(void);
static void M_LanConfig_Draw(void);
static void M_LanConfig_Key(int keynum);

static void M_Menu_GameOptions_f(void);
static void M_GameOptions_Draw(void);
static void M_GameOptions_Key(int keynum);

static void M_Menu_Search_f(void);
static void M_Search_Draw(void);
static void M_Search_Key(int keynum);

static void M_Menu_ServerList_f(void);
static void M_ServerList_Draw(void);
static void M_ServerList_Key(int keynum);

static void M_Menu_Help_f(void);
static void M_Help_Draw(void);
static void M_Help_Key(int keynum);
#endif /* NQ_HACK */

/* play enter sound after drawing a frame, so caching won't disrupt */
static qboolean m_entersound;
static qboolean m_recursiveDraw;

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

static int m_save_demonum;

enum {
    m_none,
    m_main,
    m_singleplayer,
    m_load,
    m_save,
    m_multiplayer,
    m_setup,
    m_options,
    m_video,
    m_keys,
    m_help,
    m_quit,
    m_lanconfig,
    m_gameoptions,
    m_search,
    m_slist
} m_state;

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

//=============================================================================
/* MAIN MENU */

typedef enum {
#ifdef NQ_HACK
    M_MAIN_CURSOR_SINGLEPLAYER,
    M_MAIN_CURSOR_MULTIPLAYER,
    M_MAIN_CURSOR_OPTIONS,
    M_MAIN_CURSOR_HELP,
    M_MAIN_CURSOR_QUIT,
    M_MAIN_CURSOR_LINES,
#endif
#ifdef QW_HACK
    M_MAIN_CURSOR_OPTIONS,
    M_MAIN_CURSOR_QUIT,
    M_MAIN_CURSOR_LINES,
#endif
} m_main_cursor_t;

static m_main_cursor_t m_main_cursor;

static void M_Menu_Main_f(void) {
    if (key_dest != key_menu) {
        m_save_demonum = cls.demonum;
        cls.demonum = -1;
    }
    key_dest = key_menu;
    m_state = m_main;
    m_entersound = true;
}

static void M_Main_Draw(void) {
#ifdef NQ_HACK
    static const int cursor_heights[] = {32, 52, 72, 92, 112};
#endif
#ifdef QW_HACK
    static const int cursor_heights[] = {72, 112};
#endif
    int flash;
    const qpic_t *pic;
}

static void M_Main_Key(int keynum) {
    switch (keynum) {
        case K_ESCAPE:
            key_dest = key_game;
            m_state = m_none;
            cls.demonum = m_save_demonum;
#ifdef NQ_HACK
            if (cls.demonum != -1 && !cls.demoplayback && cls.state <= ca_connected) CL_NextDemo();
#endif
#ifdef QW_HACK
            if (cls.demonum != -1 && !cls.demoplayback && cls.state == ca_disconnected)
                CL_NextDemo();
#endif
            break;

        case K_DOWNARROW:
            S_LocalSound("misc/menu1.wav");
            if (++m_main_cursor >= M_MAIN_CURSOR_LINES) m_main_cursor = 0;
            break;

        case K_UPARROW:
            S_LocalSound("misc/menu1.wav");
            if (m_main_cursor-- == 0) m_main_cursor = M_MAIN_CURSOR_LINES - 1;
            break;

        case K_ENTER:
            m_entersound = true;

            switch (m_main_cursor) {
                case M_MAIN_CURSOR_OPTIONS:
                    M_Menu_Options_f();
                    break;

                case M_MAIN_CURSOR_QUIT:
                    M_Menu_Quit_f();
                    break;

#ifdef NQ_HACK
                case M_MAIN_CURSOR_SINGLEPLAYER:
                    M_Menu_SinglePlayer_f();
                    break;

                case M_MAIN_CURSOR_MULTIPLAYER:
                    M_Menu_MultiPlayer_f();
                    break;

                case M_MAIN_CURSOR_HELP:
                    M_Menu_Help_f();
                    break;
#endif
                default:
                    break;
            }
            break;

        default:
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
#ifdef QW_HACK
    M_OPTIONS_CURSOR_SBAR,
    M_OPTIONS_CURSOR_HUD,
#endif
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

static void M_AdjustSliders(int dir) {
    S_LocalSound("misc/menu3.wav");

    switch (m_options_cursor) {
        case M_OPTIONS_CURSOR_SCREENSIZE:
            scr_viewsize.value += dir * 10;
            scr_viewsize.value = qclamp(scr_viewsize.value, 30.0f, 120.0f);
            Cvar_SetValue("viewsize", scr_viewsize.value);
            break;
        case M_OPTIONS_CURSOR_BRIGHTNESS:
            v_gamma.value -= dir * 0.05;
            v_gamma.value = qclamp(v_gamma.value, 0.5f, 1.0f);
            Cvar_SetValue("gamma", v_gamma.value);
            break;
        case M_OPTIONS_CURSOR_MOUSESPEED:
            sensitivity.value += dir * 0.5;
            sensitivity.value = qclamp(sensitivity.value, 1.0f, 11.0f);
            Cvar_SetValue("sensitivity", sensitivity.value);
            break;
        case M_OPTIONS_CURSOR_MUSICVOLUME:
#ifdef _WIN32
            bgmvolume.value += dir * 1.0;
#else
            bgmvolume.value += dir * 0.1;
#endif
            bgmvolume.value = qclamp(bgmvolume.value, 0.0f, 1.0f);
            Cvar_SetValue("bgmvolume", bgmvolume.value);
            break;
        case M_OPTIONS_CURSOR_SOUNDVOLUME:
            volume.value += dir * 0.1;
            volume.value = qclamp(volume.value, 0.0f, 1.0f);
            Cvar_SetValue("volume", volume.value);
            break;
        case M_OPTIONS_CURSOR_ALWAYSRUN:
            Cvar_SetValue("cl_run", !cl_run.value);
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
#ifdef QW_HACK
        case M_OPTIONS_CURSOR_SBAR:
            Cvar_SetValue("cl_sbar", !cl_sbar.value);
            break;
        case M_OPTIONS_CURSOR_HUD:
            Cvar_SetValue("cl_hudswap", !cl_hudswap.value);
            break;
#endif
        // case M_OPTIONS_CURSOR_MOUSEGRAB:
        //     Cvar_SetValue("_windowed_mouse", !_windowed_mouse.value);
        //     break;
        default:
            break;
    }
}

static void M_DrawSlider(int x, int y, float range) {
    int i;

    if (range < 0) range = 0;
    if (range > 1) range = 1;
    M_DrawCharacter(x - 8, y, 128);
    for (i = 0; i < SLIDER_RANGE; i++) M_DrawCharacter(x + i * 8, y, 129);
    M_DrawCharacter(x + i * 8, y, 130);
    M_DrawCharacter(x + (SLIDER_RANGE - 1) * 8 * range, y, 131);
}

void M_DrawCheckbox(int x, int y, qboolean checked) { M_Print(x, y, checked ? "on" : "off"); }

static void M_Options_Draw(void) {
    const qpic_t *pic;
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

    M_Print(16, height += 8, "            Always Run");
    M_DrawCheckbox(220, height, cl_run.value);

    M_Print(16, height += 8, "         Invert Look Y");
    M_DrawCheckbox(220, height, m_pitch.value < 0);

    // M_Print(16, height += 8, "            Mouse Look");
    // M_DrawCheckbox(220, height, m_freelook.value);

    M_Print(16, height += 8, "            Lookspring");
    M_DrawCheckbox(220, height, lookspring.value);

    M_Print(16, height += 8, "            Lookstrafe");
    M_DrawCheckbox(220, height, lookstrafe.value);

#ifdef QW_HACK
    M_Print(16, height += 8, "    Use old status bar");
    M_DrawCheckbox(220, height, cl_sbar.value);

    M_Print(16, height += 8, "      HUD on left side");
    M_DrawCheckbox(220, height, cl_hudswap.value);
#endif

    M_Print(16, height += 8, "         Video Options");

    // M_Print(16, height += 8, "             Use Mouse");
    // M_DrawCheckbox(220, height, _windowed_mouse.value);

    /* cursor */
    M_DrawCharacter(200, 32 + m_options_cursor * 8, 12 + ((int)(realtime * 4) & 1));
}

static void M_Options_SaveConfig(void) {
    FILE *f = fopen(va("%s/config.cfg", com_gamedir), "w");
    if (!f) {
        Con_Printf("Couldn't write config.cfg.\n");
        return;
    }

    Key_WriteBindings(f);
    Cvar_WriteVariables(f);

    fclose(f);
}

static void M_Options_Key(int keynum) {
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

static void M_Menu_Keys_f(void) {
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

static void M_Keys_Draw(void) {
    const qpic_t *pic;
    m_keys_cursor_t line;

    /* Draw the key bindings list */
    for (line = 0; line < M_KEYS_CURSOR_LINES; line++) {
        const int height = 48 + 8 * line;
        int keys[2];

        M_Print(16, height, m_keys_bindnames[line][1]);

        const char *keyname = Key_KeynumToString(keys[0]);
        M_Print(140, height, keyname);

        const int namewidth = strlen(keyname) * 8;
        keyname = Key_KeynumToString(keys[1]);
        M_Print(140 + namewidth + 8, height, "or");
        M_Print(140 + namewidth + 32, height, keyname);

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

static void M_Keys_Key(int keynum) {
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

static void M_Menu_Video_f(void) {
    key_dest = key_menu;
    m_state = m_video;
    m_entersound = true;
    VID_MenuInitState(&modelist[vid_modenum]);
}

static void M_Video_Draw(void) { (*vid_menudrawfn)(); }

static void M_Video_Key(int keynum) { (*vid_menukeyfn)(keynum); }

//=============================================================================
/* QUIT MENU */

static const char *const *m_quit_message;
static int m_quit_prevstate;

static const char *const m_quit_messages[][4] = {
    {
        "  Are you gonna quit    ",
        "  this game just like   ",
        "   everything else?     ",
        "                        ",
    },
    {
        " Milord, methinks that  ",
        "   thou art a lowly     ",
        " quitter. Is this true? ",
        "                        ",
    },
    {
        " Do I need to bust your ",
        "  face open for trying  ",
        "        to quit?        ",
        "                        ",
    },
    {
        " Man, I oughta smack you",
        "   for trying to quit!  ",
        "     Press Y to get     ",
        "      smacked out.      ",
    },
    {
        " Press Y to quit like a ",
        "   big loser in life.   ",
        "  Press N to stay proud ",
        "    and successful!     ",
    },
    {
        "   If you press Y to    ",
        "  quit, I will summon   ",
        "  Satan all over your   ",
        "      hard drive!       ",
    },
    {
        "  Um, Asmodeus dislikes ",
        " his children trying to ",
        " quit. Press Y to return",
        "   to your Tinkertoys.  ",
    },
    {
        "  If you quit now, I'll ",
        "  throw a blanket-party ",
        "   for you next time!   ",
        "                        ",
    },
};

void M_Menu_Quit_f(void) {
    if (m_state == m_quit) return;
    key_dest = key_menu;
    m_quit_prevstate = m_state;
    m_state = m_quit;
    m_entersound = true;
    m_quit_message = m_quit_messages[rand() % ARRAY_SIZE(m_quit_messages)];
}

static void M_Quit_Key(int keynum) {
    switch (keynum) {
        case K_ESCAPE:
        case K_ENTER:
            /* If we were previously in-game, return the key focus */
            if (m_quit_prevstate == m_none)
                key_dest = key_game;
            else
                m_entersound = true;
            m_state = m_quit_prevstate;
            break;

#ifdef NQ_HACK
            Host_Quit_f();
#endif
#ifdef QW_HACK
            CL_Disconnect();
            Sys_Quit();
#endif
            break;

        default:
            break;
    }
}

static void M_Quit_Draw(void) {
    if (m_quit_prevstate != m_none) {
        m_state = m_quit_prevstate;
        m_recursiveDraw = true;
        M_Draw();
        m_state = m_quit;
    }

    M_DrawTextBox(56, 76, 24, 4);
    M_Print(64, 84, m_quit_message[0]);
    M_Print(64, 92, m_quit_message[1]);
    M_Print(64, 100, m_quit_message[2]);
    M_Print(64, 108, m_quit_message[3]);
}

//=============================================================================
/* Menu Subsystem */

void M_Init(void) {
    Cmd_AddCommand("togglemenu", M_ToggleMenu_f);
    Cmd_AddCommand("menu_main", M_Menu_Main_f);
    Cmd_AddCommand("menu_options", M_Menu_Options_f);
    Cmd_AddCommand("menu_keys", M_Menu_Keys_f);
    Cmd_AddCommand("menu_video", M_Menu_Video_f);
    Cmd_AddCommand("menu_quit", M_Menu_Quit_f);
#ifdef NQ_HACK
    Cmd_AddCommand("menu_singleplayer", M_Menu_SinglePlayer_f);
    Cmd_AddCommand("menu_load", M_Menu_Load_f);
    Cmd_AddCommand("menu_save", M_Menu_Save_f);
    Cmd_AddCommand("menu_multiplayer", M_Menu_MultiPlayer_f);
    Cmd_AddCommand("menu_setup", M_Menu_Setup_f);
    Cmd_AddCommand("help", M_Menu_Help_f);
#endif
}

void M_Draw(void) {
    if (m_state == m_none || key_dest != key_menu) return;

    if (!m_recursiveDraw) {
        scr_copyeverything = 1;

        if (scr_con_current) {
            Draw_ConsoleBackground(vid.height);
            VID_UnlockBuffer();
            S_ExtraUpdate();
            VID_LockBuffer();
        } else
            Draw_FadeScreen();

        scr_fullupdate = 0;
    } else {
        m_recursiveDraw = false;
    }

    switch (m_state) {
        case m_main:
            M_Main_Draw();
            break;

        case m_options:
            M_Options_Draw();
            break;

        case m_keys:
            M_Keys_Draw();
            break;

        case m_video:
            M_Video_Draw();
            break;

        case m_quit:
            M_Quit_Draw();
            break;

#ifdef NQ_HACK
        case m_singleplayer:
            M_SinglePlayer_Draw();
            break;

        case m_load:
            M_Load_Draw();
            break;

        case m_save:
            M_Save_Draw();
            break;

        case m_multiplayer:
            M_MultiPlayer_Draw();
            break;

        case m_setup:
            M_Setup_Draw();
            break;

        case m_lanconfig:
            M_LanConfig_Draw();
            break;

        case m_gameoptions:
            M_GameOptions_Draw();
            break;

        case m_search:
            M_Search_Draw();
            break;

        case m_slist:
            M_ServerList_Draw();
            break;

        case m_help:
            M_Help_Draw();
            break;
#endif

        case m_none:
            break;
    }

    if (m_entersound) {
        S_LocalSound("misc/menu2.wav");
        m_entersound = false;
    }

    VID_UnlockBuffer();
    S_ExtraUpdate();
    VID_LockBuffer();
}

void M_Keydown(int keynum) {
    switch (m_state) {
        case m_none:
            return;

        case m_main:
            M_Main_Key(keynum);
            return;

        case m_options:
            M_Options_Key(keynum);
            return;

        case m_keys:
            M_Keys_Key(keynum);
            return;

        case m_video:
            M_Video_Key(keynum);
            return;

        case m_quit:
            M_Quit_Key(keynum);
            return;

#ifdef NQ_HACK
        case m_singleplayer:
            M_SinglePlayer_Key(keynum);
            return;

        case m_load:
            M_Load_Key(keynum);
            return;

        case m_save:
            M_Save_Key(keynum);
            return;

        case m_multiplayer:
            M_MultiPlayer_Key(keynum);
            return;

        case m_setup:
            M_Setup_Key(keynum);
            return;

        case m_lanconfig:
            M_LanConfig_Key(keynum);
            return;

        case m_gameoptions:
            M_GameOptions_Key(keynum);
            return;

        case m_search:
            M_Search_Key(keynum);
            break;

        case m_slist:
            M_ServerList_Key(keynum);
            return;

        case m_help:
            M_Help_Key(keynum);
            return;
#endif
    }
}

/* All NQ menus below */
#ifdef NQ_HACK

//=============================================================================
/* SINGLE PLAYER MENU */

int m_singleplayer_cursor;

#define SINGLEPLAYER_ITEMS 3

static void M_Menu_SinglePlayer_f(void) {
    key_dest = key_menu;
    m_state = m_singleplayer;
    m_entersound = true;
}

static void M_SinglePlayer_Draw(void) {

}

static void M_SinglePlayer_Key(int keynum) {
    switch (keynum) {
        case K_ESCAPE:
            M_Menu_Main_f();
            break;

        case K_DOWNARROW:
            S_LocalSound("misc/menu1.wav");
            if (++m_singleplayer_cursor >= SINGLEPLAYER_ITEMS) m_singleplayer_cursor = 0;
            break;

        case K_UPARROW:
            S_LocalSound("misc/menu1.wav");
            if (--m_singleplayer_cursor < 0) m_singleplayer_cursor = SINGLEPLAYER_ITEMS - 1;
            break;

        case K_ENTER:
            m_entersound = true;

            switch (m_singleplayer_cursor) {
                case 0:
                    if (sv.active)
                        if (!SCR_ModalMessage("Are you sure you want to\n"
                                              "start a new game?\n"))
                            break;
                    key_dest = key_game;
                    if (sv.active) Cbuf_AddText("disconnect\n");
                    Cbuf_AddText("maxplayers 1\n");
                    Cbuf_AddText("map ndu\n");
                    break;

                case 1:
                    M_Menu_Load_f();
                    break;

                case 2:
                    M_Menu_Save_f();
                    break;
            }

        default:
            break;
    }
}

//=============================================================================
/* LOAD/SAVE MENU */

#define MAX_SAVEGAMES 12

static char m_filenames[MAX_SAVEGAMES][SAVEGAME_COMMENT_LENGTH + 1];
static int loadable[MAX_SAVEGAMES];
static int load_cursor;  // 0 < load_cursor < MAX_SAVEGAMES

static void M_ScanSaves(void) {
    int i, j;
    char name[MAX_OSPATH];
    FILE *f;
    int version;

    for (i = 0; i < MAX_SAVEGAMES; i++) {
        snprintf(m_filenames[i], sizeof(m_filenames[i]), "--- UNUSED SLOT ---");
        loadable[i] = false;
        snprintf(name, sizeof(name), "%s/s%i.sav", com_gamedir, i);
        f = fopen(name, "r");
        if (!f) continue;

        fscanf(f, "%i\n", &version);
        fscanf(f, "%79s\n", name);
        snprintf(m_filenames[i], sizeof(m_filenames[i]), "%s", name);

        // change _ back to space
        for (j = 0; j < SAVEGAME_COMMENT_LENGTH; j++)
            if (m_filenames[i][j] == '_') m_filenames[i][j] = ' ';
        loadable[i] = true;
        fclose(f);
    }
}

static void M_Menu_Load_f(void) {
    m_entersound = true;
    m_state = m_load;
    key_dest = key_menu;
    M_ScanSaves();
}

static void M_Menu_Save_f(void) {
    if (!sv.active) return;
    if (cl.intermission) return;
    if (svs.maxclients != 1) return;
    m_entersound = true;
    m_state = m_save;
    key_dest = key_menu;
    M_ScanSaves();
}

static void M_Load_Draw(void) {
    int i;
    const qpic_t *p;

    for (i = 0; i < MAX_SAVEGAMES; i++) M_Print(16, 32 + 8 * i, m_filenames[i]);

    // line cursor
    M_DrawCharacter(8, 32 + load_cursor * 8, 12 + ((int)(realtime * 4) & 1));
}

static void M_Save_Draw(void) {
    int i;
    const qpic_t *p;

    for (i = 0; i < MAX_SAVEGAMES; i++) M_Print(16, 32 + 8 * i, m_filenames[i]);

    // line cursor
    M_DrawCharacter(8, 32 + load_cursor * 8, 12 + ((int)(realtime * 4) & 1));
}

static void M_Load_Key(int keynum) {
    switch (keynum) {
        case K_ESCAPE:
            M_Menu_SinglePlayer_f();
            break;

        case K_ENTER:
            S_LocalSound("misc/menu2.wav");
            if (!loadable[load_cursor]) return;
            m_state = m_none;
            key_dest = key_game;

            // Host_Loadgame_f can't bring up the loading plaque because too much
            // stack space has been used, so do it now
            SCR_BeginLoadingPlaque();

            // issue the load command
            Cbuf_AddText("load s\n");
            return;

        case K_UPARROW:
        case K_LEFTARROW:
            S_LocalSound("misc/menu1.wav");
            load_cursor--;
            if (load_cursor < 0) load_cursor = MAX_SAVEGAMES - 1;
            break;

        case K_DOWNARROW:
        case K_RIGHTARROW:
            S_LocalSound("misc/menu1.wav");
            load_cursor++;
            if (load_cursor >= MAX_SAVEGAMES) load_cursor = 0;
            break;

        default:
            break;
    }
}

static void M_Save_Key(int keynum) {
    switch (keynum) {
        case K_ESCAPE:
            M_Menu_SinglePlayer_f();
            break;

        case K_ENTER:
            m_state = m_none;
            key_dest = key_game;
            Cbuf_AddText("save s\n");
            return;

        case K_UPARROW:
        case K_LEFTARROW:
            S_LocalSound("misc/menu1.wav");
            load_cursor--;
            if (load_cursor < 0) load_cursor = MAX_SAVEGAMES - 1;
            break;

        case K_DOWNARROW:
        case K_RIGHTARROW:
            S_LocalSound("misc/menu1.wav");
            load_cursor++;
            if (load_cursor >= MAX_SAVEGAMES) load_cursor = 0;
            break;

        default:
            break;
    }
}

//=============================================================================
/* MULTIPLAYER MENU */

#define MULTIPLAYER_ITEMS 3

static int m_multiplayer_cursor;
#define StartingGame (m_multiplayer_cursor == 1)
#define JoiningGame (m_multiplayer_cursor == 0)

static void M_Menu_MultiPlayer_f(void) {
    key_dest = key_menu;
    m_state = m_multiplayer;
    m_entersound = true;
}

static void M_MultiPlayer_Draw(void) {
    int f;
    const qpic_t *p;

    if (tcpipAvailable) return;
    M_PrintWhite((320 / 2) - ((27 * 8) / 2), 148, "No Communications Available");
}

static void M_MultiPlayer_Key(int keynum) {
    switch (keynum) {
        case K_ESCAPE:
            M_Menu_Main_f();
            break;

        case K_DOWNARROW:
            S_LocalSound("misc/menu1.wav");
            if (++m_multiplayer_cursor >= MULTIPLAYER_ITEMS) m_multiplayer_cursor = 0;
            break;

        case K_UPARROW:
            S_LocalSound("misc/menu1.wav");
            if (--m_multiplayer_cursor < 0) m_multiplayer_cursor = MULTIPLAYER_ITEMS - 1;
            break;

        case K_ENTER:
            m_entersound = true;
            switch (m_multiplayer_cursor) {
                case 0:
                    if (tcpipAvailable) M_Menu_LanConfig_f();
                    break;

                case 1:
                    if (tcpipAvailable) M_Menu_LanConfig_f();
                    break;

                case 2:
                    M_Menu_Setup_f();
                    break;
            }
        default:
            break;
    }
}

//=============================================================================
/* SETUP MENU */

static int setup_cursor = 4;
static int setup_cursor_table[] = {40, 56, 80, 104, 140};

static char setup_hostname[16];
static char setup_myname[16];
static int setup_oldtop;
static int setup_oldbottom;
static int setup_top;
static int setup_bottom;

#define NUM_SETUP_CMDS 5

static void M_Menu_Setup_f(void) {
    key_dest = key_menu;
    m_state = m_setup;
    m_entersound = true;
    strcpy(setup_myname, cl_name.string);
    strcpy(setup_hostname, hostname.string);
    setup_top = setup_oldtop = ((int)cl_color.value) >> 4;
    setup_bottom = setup_oldbottom = ((int)cl_color.value) & 15;
}

static void M_Setup_Draw(void) {
    const qpic_t *p;

    M_DrawCharacter(56, setup_cursor_table[setup_cursor], 12 + ((int)(realtime * 4) & 1));

    if (setup_cursor == 0)
        M_DrawCharacter(168 + 8 * strlen(setup_hostname), setup_cursor_table[setup_cursor],
                        10 + ((int)(realtime * 4) & 1));

    if (setup_cursor == 1)
        M_DrawCharacter(168 + 8 * strlen(setup_myname), setup_cursor_table[setup_cursor],
                        10 + ((int)(realtime * 4) & 1));
}

static void M_Setup_Key(int keynum) {
    switch (keynum) {
        case K_ESCAPE:
            M_Menu_MultiPlayer_f();
            break;

        case K_UPARROW:
            S_LocalSound("misc/menu1.wav");
            setup_cursor--;
            if (setup_cursor < 0) setup_cursor = NUM_SETUP_CMDS - 1;
            break;

        case K_DOWNARROW:
            S_LocalSound("misc/menu1.wav");
            setup_cursor++;
            if (setup_cursor >= NUM_SETUP_CMDS) setup_cursor = 0;
            break;

        case K_LEFTARROW:
            if (setup_cursor < 2) return;
            S_LocalSound("misc/menu3.wav");
            if (setup_cursor == 2) setup_top = setup_top - 1;
            if (setup_cursor == 3) setup_bottom = setup_bottom - 1;
            break;
        case K_RIGHTARROW:
            if (setup_cursor < 2) return;
        forward:
            S_LocalSound("misc/menu3.wav");
            if (setup_cursor == 2) setup_top = setup_top + 1;
            if (setup_cursor == 3) setup_bottom = setup_bottom + 1;
            break;

        case K_ENTER:
            if (setup_cursor == 0 || setup_cursor == 1) return;

            if (setup_cursor == 2 || setup_cursor == 3) goto forward;

            // setup_cursor == 4 (OK)
            if (strcmp(cl_name.string, setup_myname) != 0)
                Cbuf_AddText("name ");
            if (strcmp(hostname.string, setup_hostname) != 0) Cvar_Set("hostname", setup_hostname);
            if (setup_top != setup_oldtop || setup_bottom != setup_oldbottom)
                Cbuf_AddText("color ");
            m_entersound = true;
            M_Menu_MultiPlayer_f();
            break;

        case K_BACKSPACE:
            if (setup_cursor == 0) {
                if (strlen(setup_hostname)) setup_hostname[strlen(setup_hostname) - 1] = 0;
            }

            if (setup_cursor == 1) {
                if (strlen(setup_myname)) setup_myname[strlen(setup_myname) - 1] = 0;
            }
            break;

        default:
            if (keynum < K_SPACE || keynum > K_DEL) break;
            if (setup_cursor == 0) {
                int length = strlen(setup_hostname);
                if (length < 15) {
                    setup_hostname[length + 1] = 0;
                    setup_hostname[length] = keynum;
                }
            }
            if (setup_cursor == 1) {
                int length = strlen(setup_myname);
                if (length < 15) {
                    setup_myname[length + 1] = 0;
                    setup_myname[length] = keynum;
                }
            }
    }

    if (setup_top > 13) setup_top = 0;
    if (setup_top < 0) setup_top = 13;
    if (setup_bottom > 13) setup_bottom = 0;
    if (setup_bottom < 0) setup_bottom = 13;
}

//=============================================================================
/* HELP MENU */

#define NUM_HELP_PAGES 6

static int help_page;

static void M_Menu_Help_f(void) {
    key_dest = key_menu;
    m_state = m_help;
    m_entersound = true;
    help_page = 0;
}

static void M_Help_Draw(void) { return; }

static void M_Help_Key(int keynum) {
    switch (keynum) {
        case K_ESCAPE:
            M_Menu_Main_f();
            break;

        case K_UPARROW:
        case K_RIGHTARROW:
            m_entersound = true;
            if (++help_page >= NUM_HELP_PAGES) help_page = 0;
            break;

        case K_DOWNARROW:
        case K_LEFTARROW:
            m_entersound = true;
            if (--help_page < 0) help_page = NUM_HELP_PAGES - 1;
            break;

        default:
            break;
    }
}

//=============================================================================
/* LAN CONFIG MENU */

qboolean m_return_onerror;
char m_return_reason[32];
int m_return_state;

static int lanConfig_cursor = -1;
static int lanConfig_cursor_table[] = {72, 92, 124};

#define NUM_LANCONFIG_CMDS 3

static int lanConfig_port;
static char lanConfig_portname[6];
static char lanConfig_joinname[22];

static void M_ConfigureNetSubsystem(void) {
    /* enable/disable net systems to match desired config */
    Cbuf_AddText("stopdemo");
    net_hostport = lanConfig_port;
}

static void M_Menu_LanConfig_f(void) {

}

//=============================================================================
/* GAME OPTIONS MENU */

typedef struct {
    const char *const name;
    const char *const description;
} level_t;

// FIXME - make this dynamic? get strings from bsps, scanning map dirs?
static const level_t levels[] = {{"start", "Entrance"},  // 0

                                 {"e1m1", "Slipgate Complex"},  // 1
                                 {"e1m2", "Castle of the Damned"},
                                 {"e1m3", "The Necropolis"},
                                 {"e1m4", "The Grisly Grotto"},
                                 {"e1m5", "Gloom Keep"},
                                 {"e1m6", "The Door To Chthon"},
                                 {"e1m7", "The House of Chthon"},
                                 {"e1m8", "Ziggurat Vertigo"},

                                 {"e2m1", "The Installation"},  // 9
                                 {"e2m2", "Ogre Citadel"},
                                 {"e2m3", "Crypt of Decay"},
                                 {"e2m4", "The Ebon Fortress"},
                                 {"e2m5", "The Wizard's Manse"},
                                 {"e2m6", "The Dismal Oubliette"},
                                 {"e2m7", "Underearth"},

                                 {"e3m1", "Termination Central"},  // 16
                                 {"e3m2", "The Vaults of Zin"},
                                 {"e3m3", "The Tomb of Terror"},
                                 {"e3m4", "Satan's Dark Delight"},
                                 {"e3m5", "Wind Tunnels"},
                                 {"e3m6", "Chambers of Torment"},
                                 {"e3m7", "The Haunted Halls"},

                                 {"e4m1", "The Sewage System"},  // 23
                                 {"e4m2", "The Tower of Despair"},
                                 {"e4m3", "The Elder God Shrine"},
                                 {"e4m4", "The Palace of Hate"},
                                 {"e4m5", "Hell's Atrium"},
                                 {"e4m6", "The Pain Maze"},
                                 {"e4m7", "Azure Agony"},
                                 {"e4m8", "The Nameless City"},

                                 {"end", "Shub-Niggurath's Pit"},  // 31

                                 {"dm1", "Place of Two Deaths"},  // 32
                                 {"dm2", "Claustrophobopolis"},
                                 {"dm3", "The Abandoned Base"},
                                 {"dm4", "The Bad Place"},
                                 {"dm5", "The Cistern"},
                                 {"dm6", "The Dark Zone"}};

// MED 01/06/97 added hipnotic levels
static const level_t hipnoticlevels[] = {
    {"start", "Command HQ"},  // 0

    {"hip1m1", "The Pumping Station"},  // 1
    {"hip1m2", "Storage Facility"},
    {"hip1m3", "The Lost Mine"},
    {"hip1m4", "Research Facility"},
    {"hip1m5", "Military Complex"},

    {"hip2m1", "Ancient Realms"},  // 6
    {"hip2m2", "The Black Cathedral"},
    {"hip2m3", "The Catacombs"},
    {"hip2m4", "The Crypt"},
    {"hip2m5", "Mortum's Keep"},
    {"hip2m6", "The Gremlin's Domain"},

    {"hip3m1", "Tur Torment"},  // 12
    {"hip3m2", "Pandemonium"},
    {"hip3m3", "Limbo"},
    {"hip3m4", "The Gauntlet"},

    {"hipend", "Armagon's Lair"},  // 16

    {"hipdm1", "The Edge of Oblivion"}  // 17
};

// PGM 01/07/97 added rogue levels
// PGM 03/02/97 added dmatch level
static const level_t roguelevels[] = {
    {"start", "Split Decision"},  {"r1m1", "Deviant's Domain"},     {"r1m2", "Dread Portal"},
    {"r1m3", "Judgement Call"},   {"r1m4", "Cave of Death"},        {"r1m5", "Towers of Wrath"},
    {"r1m6", "Temple of Pain"},   {"r1m7", "Tomb of the Overlord"}, {"r2m1", "Tempus Fugit"},
    {"r2m2", "Elemental Fury I"}, {"r2m3", "Elemental Fury II"},    {"r2m4", "Curse of Osiris"},
    {"r2m5", "Wizard's Keep"},    {"r2m6", "Blood Sacrifice"},      {"r2m7", "Last Bastion"},
    {"r2m8", "Source of Evil"},   {"ctf1", "Division of Change"}};

typedef struct {
    const char *const description;
    int firstLevel;
    int levels;
} episode_t;

static const episode_t episodes[] = {{"Welcome to Quake", 0, 1},     {"Doomed Dimension", 1, 8},
                                     {"Realm of Black Magic", 9, 7}, {"Netherworld", 16, 7},
                                     {"The Elder World", 23, 8},     {"Final Level", 31, 1},
                                     {"Deathmatch Arena", 32, 6}};

// MED 01/06/97  added hipnotic episodes
static const episode_t hipnoticepisodes[] = {
    {"Scourge of Armagon", 0, 1}, {"Fortress of the Dead", 1, 5}, {"Dominion of Darkness", 6, 6},
    {"The Rift", 12, 4},          {"Final Level", 16, 1},         {"Deathmatch Arena", 17, 1}};

// PGM 01/07/97 added rogue episodes
// PGM 03/02/97 added dmatch episode
static const episode_t rogueepisodes[] = {{"Introduction", 0, 1},
                                          {"Hell's Fortress", 1, 7},
                                          {"Corridors of Time", 8, 8},
                                          {"Deathmatch Arena", 16, 1}};

static int startepisode;
static int startlevel;
static int maxplayers;
static qboolean m_serverInfoMessage = false;
static double m_serverInfoMessageTime;

#define NUM_GAMEOPTIONS 9
static int gameoptions_cursor;
static int gameoptions_cursor_table[] = {40, 56, 64, 72, 80, 88, 96, 112, 120};

static void M_Menu_GameOptions_f(void) {
    key_dest = key_menu;
    m_state = m_gameoptions;
    m_entersound = true;
    if (maxplayers == 0) maxplayers = svs.maxclients;
    if (maxplayers < 2) maxplayers = svs.maxclientslimit;
}

static void M_GameOptions_Draw(void) {
    const qpic_t *p;
    int x;
}

static void M_NetStart_Change(int dir) {
    int count;

    switch (gameoptions_cursor) {
        case 1:
            maxplayers += dir;
            if (maxplayers > svs.maxclientslimit) {
                maxplayers = svs.maxclientslimit;
                m_serverInfoMessage = true;
                m_serverInfoMessageTime = realtime;
            }
            if (maxplayers < 2) maxplayers = 2;
            break;

        case 2:
            Cvar_SetValue("coop", coop.value ? 0 : 1);
            break;

        case 3:
            if (rogue)
                count = 6;
            else
                count = 2;

            Cvar_SetValue("teamplay", teamplay.value + dir);
            if (teamplay.value > count)
                Cvar_SetValue("teamplay", 0);
            else if (teamplay.value < 0)
                Cvar_SetValue("teamplay", count);
            break;

        case 4:
            Cvar_SetValue("skill", skill.value + dir);
            if (skill.value > 3) Cvar_SetValue("skill", 0);
            if (skill.value < 0) Cvar_SetValue("skill", 3);
            break;

        case 5:
            Cvar_SetValue("fraglimit", fraglimit.value + dir * 10);
            if (fraglimit.value > 100) Cvar_SetValue("fraglimit", 0);
            if (fraglimit.value < 0) Cvar_SetValue("fraglimit", 100);
            break;

        case 6:
            Cvar_SetValue("timelimit", timelimit.value + dir * 5);
            if (timelimit.value > 60) Cvar_SetValue("timelimit", 0);
            if (timelimit.value < 0) Cvar_SetValue("timelimit", 60);
            break;

        case 7:
            startepisode += dir;
            // MED 01/06/97 added hipnotic count
            if (hipnotic) count = 6;
            // PGM 01/07/97 added rogue count
            // PGM 03/02/97 added 1 for dmatch episode
            else if (rogue)
                count = 4;
            else if (registered.value)
                count = 7;
            else
                count = 2;

            if (startepisode < 0) startepisode = count - 1;

            if (startepisode >= count) startepisode = 0;

            startlevel = 0;
            break;

        case 8:
            startlevel += dir;
            // MED 01/06/97 added hipnotic episodes
            if (hipnotic) count = hipnoticepisodes[startepisode].levels;
            // PGM 01/06/97 added hipnotic episodes
            else if (rogue)
                count = rogueepisodes[startepisode].levels;
            else
                count = episodes[startepisode].levels;

            if (startlevel < 0) startlevel = count - 1;

            if (startlevel >= count) startlevel = 0;
            break;
    }
}

static void M_GameOptions_Key(int keynum) {
    switch (keynum) {
        case K_ESCAPE:
            M_Menu_MultiPlayer_f();
            break;

        case K_UPARROW:
            S_LocalSound("misc/menu1.wav");
            gameoptions_cursor--;
            if (gameoptions_cursor < 0) gameoptions_cursor = NUM_GAMEOPTIONS - 1;
            break;

        case K_DOWNARROW:
            S_LocalSound("misc/menu1.wav");
            gameoptions_cursor++;
            if (gameoptions_cursor >= NUM_GAMEOPTIONS) gameoptions_cursor = 0;
            break;

        case K_LEFTARROW:
            if (gameoptions_cursor == 0) break;
            S_LocalSound("misc/menu3.wav");
            M_NetStart_Change(-1);
            break;

        case K_RIGHTARROW:
            if (gameoptions_cursor == 0) break;
            S_LocalSound("misc/menu3.wav");
            M_NetStart_Change(1);
            break;

        case K_ENTER:
            S_LocalSound("misc/menu2.wav");
            if (gameoptions_cursor == 0) {
                if (sv.active) Cbuf_AddText("disconnect");
                Cbuf_AddText("listen 0\n");  // so host_netport will be re-examined
                Cbuf_AddText("maxplayers\n");
                SCR_BeginLoadingPlaque();

                if (hipnotic)
                    Cbuf_AddText(
                        "map %s\n");
                else if (rogue)
                    Cbuf_AddText(
                        "map %s\n");
                else
                    Cbuf_AddText("map %s\n");

                return;
            }

            M_NetStart_Change(1);
            break;

        default:
            break;
    }
}

//=============================================================================
/* SEARCH MENU */

static qboolean searchComplete = false;
static double searchCompleteTime;

static void M_Menu_Search_f(void) {
    key_dest = key_menu;
    m_state = m_search;
    m_entersound = false;
    slistSilent = true;
    slistLocal = false;
    searchComplete = false;
    NET_Slist_f();
}

static void M_Search_Draw(void) {
    const qpic_t *p;
    int x;

    if (slistInProgress) {
        NET_Poll();
        return;
    }

    if (!searchComplete) {
        searchComplete = true;
        searchCompleteTime = realtime;
    }

    if (hostCacheCount) {
        M_Menu_ServerList_f();
        return;
    }

    M_PrintWhite((320 / 2) - ((22 * 8) / 2), 64, "No Quake servers found");
    if ((realtime - searchCompleteTime) < 3.0) return;

    M_Menu_LanConfig_f();
}

static void M_Search_Key(int keynum) {}

//=============================================================================
/* SLIST MENU */

static int slist_cursor;
static qboolean slist_sorted;

static void M_Menu_ServerList_f(void) {
    key_dest = key_menu;
    m_state = m_slist;
    m_entersound = true;
    slist_cursor = 0;
    m_return_onerror = false;
    m_return_reason[0] = 0;
    slist_sorted = false;
}

static void M_ServerList_Draw(void) {
    int n;
    char string[64];
    const qpic_t *p;

    if (!slist_sorted) {
        if (hostCacheCount > 1) {
            int i, j;
            hostcache_t temp;

            for (i = 0; i < hostCacheCount; i++)
                for (j = i + 1; j < hostCacheCount; j++)
                    if (strcmp(hostcache[j].name, hostcache[i].name) < 0) {
                        memcpy(&temp, &hostcache[j], sizeof(hostcache_t));
                        memcpy(&hostcache[j], &hostcache[i], sizeof(hostcache_t));
                        memcpy(&hostcache[i], &temp, sizeof(hostcache_t));
                    }
        }
        slist_sorted = true;
    }

    M_DrawCharacter(0, 32 + slist_cursor * 8, 12 + ((int)(realtime * 4) & 1));

    if (*m_return_reason) M_PrintWhite(16, 148, m_return_reason);
}

static void M_ServerList_Key(int keynum) {
    switch (keynum) {
        case K_ESCAPE:
            M_Menu_LanConfig_f();
            break;

        case K_SPACE:
            M_Menu_Search_f();
            break;

        case K_UPARROW:
        case K_LEFTARROW:
            S_LocalSound("misc/menu1.wav");
            slist_cursor--;
            if (slist_cursor < 0) slist_cursor = hostCacheCount - 1;
            break;

        case K_DOWNARROW:
        case K_RIGHTARROW:
            S_LocalSound("misc/menu1.wav");
            slist_cursor++;
            if (slist_cursor >= hostCacheCount) slist_cursor = 0;
            break;

        case K_ENTER:
            S_LocalSound("misc/menu2.wav");
            m_return_state = m_state;
            m_return_onerror = true;
            slist_sorted = false;
            key_dest = key_game;
            m_state = m_none;
            Cbuf_AddText("connect \"%s\"\n");
            break;

        default:
            break;
    }
}

#endif /* NQ_HACK */
