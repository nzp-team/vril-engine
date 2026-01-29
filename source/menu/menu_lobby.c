/*
Copyright (C) 2025 NZ:P Team

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

//=============================================================================
/* LOBBY MENU */

int	            menu_lobby_cursor;
int             support_gamesettings;
int             LOBBY_ITEMS;

float           menu_lobby_countdown = 0;
float           menu_lobby_last;

char*           game_starting;

char*			gamemode;
char*			difficulty;
char*			startround;
char*			magic;
char*			headshotonly;
char*			fastrounds;
char*			hordesize;

void Menu_Lobby_AllocStrings (void)
{
    game_starting = malloc(64*sizeof(char));

    gamemode = malloc(16*sizeof(char));
    difficulty = malloc(12*sizeof(char));
    startround = malloc(16*sizeof(char));
    magic = malloc(16*sizeof(char));
    headshotonly = malloc(16*sizeof(char));
    fastrounds = malloc(16*sizeof(char));
    hordesize = malloc(16*sizeof(char));
}

void Menu_Lobby_FreeStrings (void)
{
    free(game_starting);
    free(gamemode);
    free(difficulty);
    free(startround);
    free(magic);
    free(headshotonly);
    free(fastrounds);
    free(hordesize);
}

void Menu_Lobby_StartCountdown (void)
{
    Menu_StartSound(MENU_SND_ENTER);
    menu_lobby_countdown = Sys_FloatTime() + 6;
    menu_lobby_last = 0;
}

void Menu_Lobby_StopCountdown (void)
{
    Menu_StartSound(MENU_SND_ENTER);
    menu_lobby_countdown = 0;
    menu_lobby_last = 0;
}

void Menu_Lobby_SetStrings (void)
{
    switch ((int)sv_gamemode.value) {
        case 0:
            gamemode = "CLASSIC";
            break;
        case 1:
            gamemode = "GRIEF";
            break;
        case 2:
            gamemode = "GUN GAME";
            break;
        case 3:
            gamemode = "HARDCORE";
            break;
        case 4:
            gamemode = "WILD WEST";
            break;
        case 5:
            gamemode = "STICKS & STONES";
            break;
        case 6:
            gamemode = "FEVER";
            break;
        default:
            gamemode = "???";
            break;
    }

    switch ((int)sv_difficulty.value) {
        case 0:
            difficulty = "EASY";
            break;
        case 1:
            difficulty = "NORMAL";
            break;
        case 2:
            difficulty = "HARD";
            break;
        case 3:
            difficulty = "NIGHTMARE";
            break;
        default:
            difficulty = "???";
            break;
    }

    sprintf (startround, "%i", (int)sv_startround.value);
    if ((int)sv_startround.value == 0) startround = "1";

    if ((int)sv_magic.value == 1) { 
        magic = "ENABLED";
    } else {
        magic = "DISABLED";
    }
    
    if ((int)sv_headshotonly.value == 1) {
        headshotonly = "ENABLED";
    } else {
        headshotonly = "DISABLED";
    }

    if ((int)sv_fastrounds.value == 1) {
        fastrounds = "ENABLED";
    } else {
        fastrounds = "DISABLED";
    }

    sprintf(hordesize, "%i", (int)sv_maxai.value);
}

void Menu_Lobby_Set (void)
{
    Menu_Lobby_AllocStrings();
    Menu_Lobby_SetStrings();
    LOBBY_ITEMS = 2;

    key_dest = key_menu;
	m_state = m_lobby;
}

void Menu_Lobby_SetBack (void)
{
    if (Menu_IsStockMap(current_selected_bsp)) {
        Menu_StockMaps_Set();
    } else {
        Menu_CustomMaps_Set();
    }
}

void Menu_Lobby_Draw (void)
{
    // Background
	Menu_DrawCustomBackground ();
    // Title
    Menu_DrawTitle("PRE-GAME", MENU_COLOR_WHITE);
    // Map panel makes the background darker
    Menu_DrawMapPanel();

    support_gamesettings = UserMapSupportsCustomGameLookup (current_selected_bsp);

    LOBBY_ITEMS = 2;
    LOBBY_ITEMS = LOBBY_ITEMS + support_gamesettings;
    if (LOBBY_ITEMS > 3) LOBBY_ITEMS = 3;

    if (menu_lobby_countdown == 0) {
        Menu_DrawButton (1, 1, "START GAME", MENU_BUTTON_ACTIVE, "Face the Horde!");

        if (support_gamesettings) {
            Menu_DrawButton(2, 2, "GAME SETTINGS", MENU_BUTTON_ACTIVE, "Adjust Gameplay Options.");
        } else {
            Menu_DrawButton(2, -1, "NOT SUPPORTED", MENU_BUTTON_INACTIVE, "");
        }
        
        Menu_DrawButton (11.5, 2+support_gamesettings, "BACK", MENU_BUTTON_ACTIVE, "Return to Map Selection.");
    } else {
        Menu_DrawButton (1, 1, "CANCEL", MENU_BUTTON_ACTIVE, "..Take it back!");

        if (support_gamesettings) {
            Menu_DrawButton(2, 2, "GAME SETTINGS", MENU_BUTTON_ACTIVE, "");
        } else {
            Menu_DrawButton(2, -1, "NOT SUPPORTED", MENU_BUTTON_INACTIVE, "");
        }
            
        Menu_DrawButton(11.5, 2+support_gamesettings, "BACK", MENU_BUTTON_ACTIVE, "Return to Map Selection.");
    }

    Menu_DrawLobbyInfo (current_selected_bsp, gamemode, difficulty, startround, magic, headshotonly, fastrounds, hordesize);

    float lobby_delta = menu_lobby_countdown - (float)Sys_FloatTime();
    sprintf(game_starting, "Game Starting In..  %i", (int)lobby_delta);
    if (lobby_delta > 0) {
        int x_pos = (vid.width/2) + (vid.width/28);
        int y_pos = (vid.height - (vid.height/2)) + (vid.height/20);
        Draw_ColoredString(x_pos, y_pos, game_starting, 255, 255, 255, 255, menu_scale_factor);
        Draw_FillByColor(x_pos, y_pos + (CHAR_HEIGHT), (vid.width/4)*(lobby_delta/(vid.width/60)), vid.height/36, 180, 130*(lobby_delta/(vid.width/60)), 130, 180);

        if (menu_lobby_last != (float)floor(lobby_delta)) {
            Menu_StartSound(MENU_SND_BEEP);
            menu_lobby_last = (float)floor(lobby_delta);
        }
    } else if (lobby_delta < 0 && menu_lobby_countdown != 0) {
        // Start a match!
        Menu_LoadMap(current_selected_bsp);
        Menu_Lobby_StopCountdown();
        //Menu_Lobby_FreeStrings();
    }
}

void Menu_Lobby_Key (int key)
{
    switch (key)
	{

	case K_DOWNARROW:
		Menu_StartSound(MENU_SND_NAVIGATE);
		if (++menu_lobby_cursor >= LOBBY_ITEMS)
			menu_lobby_cursor = 0;
		break;

	case K_UPARROW:
		Menu_StartSound(MENU_SND_NAVIGATE);
		if (--menu_lobby_cursor < 0)
			menu_lobby_cursor = LOBBY_ITEMS - 1;
		break;

	case K_ENTER:
	case K_AUX1:
		Menu_StartSound(MENU_SND_ENTER);
        if (menu_lobby_countdown == 0) {
            switch (menu_lobby_cursor)
            {
                case 0:
                    Menu_Lobby_StartCountdown();
                    break;
                case 1:
                    if (support_gamesettings) {
                        Menu_GameSettings_Set();
                        break;
                    } else {
                        Menu_Lobby_StopCountdown();
                        Menu_Lobby_SetBack();
                        break;
                    }
                case 2:
                    if (support_gamesettings) {
                        Menu_Lobby_StopCountdown();
                        Menu_Lobby_SetBack();
                        break;
                    }
            }
        } else {
            switch (menu_lobby_cursor) {
                case 0:
                    Menu_Lobby_StopCountdown();
                    break;
                case 1:
                    if (support_gamesettings) {
                        Menu_GameSettings_Set();
                        break;
                    } else {
                        Menu_Lobby_StopCountdown();
                        Menu_Lobby_SetBack();
                        break;
                    }
                case 2:
                    if (support_gamesettings) {
                        Menu_Lobby_StopCountdown();
                        Menu_Lobby_SetBack();
                        break;
                    }
            }
        }
	}
}