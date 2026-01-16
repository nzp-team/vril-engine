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
/* GAME SETTINGS MENU */

int	menu_gamesettings_cursor;
#define GAMESETTINGS_ITEMS 8

char* gamemode_description;
char* gamemode_string;

char* difficulty_description;
char* difficulty_string;

char* startround_string;
char* magic_string;
char* headshot_string;
char* fast_string;
char* hordesize_string;

void Menu_GameSettings_Set (void)
{
    key_dest = key_menu;
	m_state = m_gamesettings;
    startround_string = "Round to begin the Game on.";
}

void Menu_GameSettings_SetStrings (void)
{
    switch ((int)sv_gamemode.value) {
        case 0:
            gamemode_description = "Classic Round-Based Zombies.";
            gamemode_string = "CLASSIC";
            break;
        /*
        case 1:
            gamemode_description = "Compete in Teams to outlive your opponents.";
            gamemode_string = "GRIEF";
        */
        case 2:
            gamemode_description = "Race to earn Score to cycle through Weaponry."; 
            gamemode_string = "GUN GAME";
            break;
        case 3: 
            gamemode_description = "No HUD? Two Perks? 150% Item Cost? No Problem."; 
            gamemode_string = "HARDCORE";
            break;
        case 4: 
            gamemode_description = "Ole Fashioned stand-off between you and the Dead."; 
            gamemode_string = "WILD WEST"; 
            break;
        case 5: 
            gamemode_description = "Knives-and-Nades only. Grenades Explode on Contact."; 
            gamemode_string = "STICKS & STONES"; 
            break;
        case 6: 
            gamemode_description = "The cycle continues. Old meets new in this Cold War inspired mode!"; 
            gamemode_string = "FEVER"; 
            break;
        default: 
            gamemode_description = "???"; 
            break;
    }

    switch ((int)sv_difficulty.value) {
        case 0: 
            difficulty_description = "Normal Zombies gameplay."; 
            difficulty_string = "NORMAL"; 
            break;
        case 1: 
            difficulty_description = "Less & Slower Zombies. More Power-Ups. More Health."; 
            difficulty_string = "EASY"; 
            break;
        case 2: 
            difficulty_description = "More & Faster Zombies. Less Power-Ups. Less Health."; 
            difficulty_string = "HARD"; 
            break;
        case 3: 
            difficulty_description = "Hellish Zombies. Earn Less Score. Rare Power-Ups. 1 hit KO."; 
            difficulty_string = "NIGHTMARE"; 
            break;
        default: 
            difficulty_description = "???"; 
            break; 
    }

    if ((int)sv_magic.value == 1) { 
        magic_string = "ENABLED";
    } else {
        magic_string = "DISABLED";
    }
    
    if ((int)sv_headshotonly.value == 1) {
        headshot_string = "ENABLED";
    } else {
        headshot_string = "DISABLED";
    }

    if ((int)sv_fastrounds.value == 1) {
        fast_string = "ENABLED";
    } else {
        fast_string = "DISABLED";
    }

    sprintf(hordesize_string, "%i", (int)sv_maxai.value);
}

void Menu_GameSettings_ApplyGameMode (void)
{
    float current_gamemode = sv_gamemode.value;
    current_gamemode += 1;

    if (current_gamemode > 6) {
        current_gamemode = 0;
    }

    Cvar_SetValue ("sv_gamemode", current_gamemode);
}

void Menu_GameSettings_ApplyDifficulty (void)
{
    float current_difficulty = sv_difficulty.value;
    current_difficulty += 1;

    if (current_difficulty > 3)
        current_difficulty = 0;

    Cvar_SetValue("sv_difficulty", current_difficulty);
}

void Menu_GameSettings_Draw (void)
{
    // Background
	Menu_DrawCustomBackground ();
    // Title
    Menu_DrawTitle("GAME SETTINGS", MENU_COLOR_WHITE);
    // Map panel makes the background darker
    Menu_DrawMapPanel();
    // Set Game Settings string
    Menu_GameSettings_SetStrings();

    // Game Mode button
    Menu_DrawButton(1, 1, "GAME MODE", MENU_BUTTON_ACTIVE, gamemode_description);
    Menu_DrawOptionButton(1, gamemode_string);

    // Difficulty button
    Menu_DrawButton(2, 2, "DIFFICULTY", MENU_BUTTON_ACTIVE, difficulty_description);
    Menu_DrawOptionButton(2, difficulty_string);

    //Start Round slider
    Menu_DrawButton(3, 3, "START ROUND", MENU_BUTTON_ACTIVE, startround_string);

    // Back button
    Menu_DrawButton(11.5, 8, "BACK", MENU_BUTTON_ACTIVE, "Return to Pre-Game Menu.");

}

void Menu_GameSettings_Key (int key)
{
    switch (key)
	{

	case K_DOWNARROW:
		Menu_StartSound(MENU_SND_NAVIGATE);
		if (++menu_gamesettings_cursor >= GAMESETTINGS_ITEMS)
			menu_gamesettings_cursor = 0;
		break;

	case K_UPARROW:
		Menu_StartSound(MENU_SND_NAVIGATE);
		if (--menu_gamesettings_cursor < 0)
			menu_gamesettings_cursor = GAMESETTINGS_ITEMS - 1;
		break;

    case K_ENTER:
	case K_AUX1:
		Menu_StartSound(MENU_SND_ENTER);
        switch (menu_gamesettings_cursor)
        {
            case 0:
                Menu_GameSettings_ApplyGameMode();
                break;
            case 1:
                Menu_GameSettings_ApplyDifficulty();
                break;
            case 7:
                Menu_Lobby_Set();
                break;
        }
    }
}