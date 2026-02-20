/*
Copyright (C) 2025-2026 NZ:P Team

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

int             support_gamesettings;

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

void Menu_Lobby_StopCountdown (void)
{
    Menu_ResetMenuButtons();
    menu_lobby_countdown = 0;
    menu_lobby_last = 0;
}

void Menu_Lobby_SetCountdown (void)
{
    Menu_ResetMenuButtons();
    Menu_SetSound(MENU_SND_ENTER);

    if (menu_lobby_countdown == 0) {    
        menu_lobby_countdown = Sys_FloatTime() + 6;
        menu_lobby_last = 0;
    } else {
        Menu_Lobby_StopCountdown();
    } 
}

void Menu_Lobby_SetBack (void)
{
    Menu_Lobby_StopCountdown();

    if (Menu_IsStockMap(current_selected_bsp)) {
        Menu_StockMaps_Set();
    } else {
        Menu_CustomMaps_Set();
    }
}

void Menu_Lobby_Set (void)
{
    Menu_ResetMenuButtons();
    Menu_Lobby_AllocStrings();
    Menu_Lobby_SetStrings();

    support_gamesettings = Menu_UserMapSupportsGameSettings(current_selected_bsp);

    key_dest = key_menu;
    if (m_state == m_stockmaps || m_state == m_custommaps) {
        m_previous_state = m_state;
    }
	m_state = m_lobby;
}

void Menu_Lobby_Draw (void)
{
    // Background
	Menu_DrawCustomBackground (true);
    // Title
    Menu_DrawTitle("PRE-GAME", MENU_COLOR_WHITE);
    // Map panel makes the background darker
    Menu_DrawMapPanel();

    if (menu_lobby_countdown == 0) {
        Menu_DrawButton (1, 0, "START GAME", "Face the Horde!", Menu_Lobby_SetCountdown);    
    } else {
        Menu_DrawButton (1, 0, "CANCEL", "..Take it back!", Menu_Lobby_StopCountdown);
    }

    if (support_gamesettings) {
        Menu_DrawButton(2, 1, "GAME SETTINGS", "Adjust Gameplay Options.", Menu_GameSettings_Set);
    } else {
        Menu_DrawGreyButton(2, "NOT SUPPORTED");
    }

    Menu_DrawButton (-1, 2, "BACK", "Return to Map Selection.", Menu_Lobby_SetBack);

    Menu_DrawLobbyInfo (current_selected_bsp, gamemode, difficulty, startround, magic, headshotonly, fastrounds, hordesize);

    float lobby_delta = menu_lobby_countdown - (float)Sys_FloatTime();
    sprintf(game_starting, "Game Starting In..  %i", (int)lobby_delta);
    if (lobby_delta > 0) {
        int image_width = vid.width/3;
        int image_height = vid.height/3;
        int map_region_center = 150 + (((vid.width-150)/2)+2);
        int x_pos = map_region_center;
        int y_pos = big_bar_height + small_bar_height + image_height;

        UI_SetAlignment (UI_ANCHOR_LEFT, UI_ANCHOR_BOTTOM);
        Menu_DrawStringCentered(x_pos, y_pos, game_starting, 255, 255, 255, 255);

        // Countdown bar
        Menu_DrawFill(x_pos, y_pos - (CHAR_HEIGHT+2), ((float)(image_width)*(lobby_delta/(vid.width/60))/2), vid.height/36, 136, 136*(lobby_delta/(vid.width/60)), 136*(lobby_delta/(vid.width/60)), 230);
        Menu_DrawFill(x_pos, y_pos - (CHAR_HEIGHT+2), -((float)(image_width)*(lobby_delta/(vid.width/60))/2), vid.height/36, 136, 136*(lobby_delta/(vid.width/60)), 136*(lobby_delta/(vid.width/60)), 230);

        if (menu_lobby_last != (float)floor(lobby_delta)) {
            Menu_SetSound(MENU_SND_BEEP);
            menu_lobby_last = (float)floor(lobby_delta);
        }
    } else if (lobby_delta < 0 && menu_lobby_countdown != 0) {
        // Start a match!
        Menu_LoadMap(current_selected_bsp);
        Menu_Lobby_StopCountdown();
        //Menu_Lobby_FreeStrings();
    }
}