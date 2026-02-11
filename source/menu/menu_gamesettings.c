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

char*           gamemode_description;
char*           gamemode_string;

char*           difficulty_description;
char*           difficulty_string;

char*           startround_string;
char*           magic_string;
char*           headshot_string;
char*           fast_string;
char*           hordesize_string;

void Menu_GameSettings_AllocStrings (void)
{
    gamemode_description = malloc(128*sizeof(char));
    gamemode_string = malloc(32*sizeof(char));

    difficulty_description = malloc(128*sizeof(char));
    difficulty_string = malloc(32*sizeof(char));

    startround_string = malloc(16*sizeof(char));
    magic_string = malloc(16*sizeof(char));
    headshot_string = malloc(16*sizeof(char));
    fast_string = malloc(16*sizeof(char));
    hordesize_string = malloc(16*sizeof(char));

    startround_string = "Round to begin the Game on.";
}

void Menu_GameSettings_FreeStrings (void)
{
    free(gamemode_description);
    free(gamemode_string);

    free(difficulty_description);
    free(difficulty_string);

    free(startround_string);
    free(magic_string);
    free(headshot_string);
    free(fast_string);
    free(hordesize_string);

    free(startround_string); 
}

void Menu_GameSettings_SetStrings (void)
{
    switch ((int)sv_gamemode.value) {
        case 0:
            gamemode_description = "Classic Round-Based Zombies.";
            gamemode_string = "CLASSIC";
            break;
        
        case 1:
            Cvar_SetValue ("sv_gamemode", 2);
            break;
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

    sprintf(hordesize_string, "%d", (int)sv_maxai.value);
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

void Menu_GameSettings_ApplyMagic (void)
{
    float current_magic = sv_magic.value;
    current_magic += 1;

    if (current_magic > 1) {
        current_magic = 0;
    }

    Cvar_SetValue("sv_magic", current_magic);
}

void Menu_GameSettings_ApplyHeadShotsOnly (void)
{
    float current_headshot = sv_headshotonly.value;
    current_headshot += 1;

    if (current_headshot > 1) {
        current_headshot = 0;
    }

    Cvar_SetValue("sv_headshotonly", current_headshot);
}

void Menu_GameSettings_ApplyFastRounds (void)
{
    float current_fastrounds = sv_fastrounds.value;
    current_fastrounds += 1;

    if (current_fastrounds > 1) {
        current_fastrounds = 0;
    }

    Cvar_SetValue("sv_fastrounds", current_fastrounds);
}

void Menu_GameSettings_Set (void)
{
    Menu_ResetMenuButtons();
    Menu_GameSettings_AllocStrings();

    key_dest = key_menu;
	m_state = m_gamesettings;
}

void Menu_GameSettings_Draw (void)
{
    // Background
	Menu_DrawCustomBackground (true);
    // Title
    Menu_DrawTitle("GAME SETTINGS", MENU_COLOR_WHITE);
    // Map panel makes the background darker
    Menu_DrawMapPanel();
    // Set Game Settings string
    Menu_GameSettings_SetStrings();

    // Game Mode button
    Menu_DrawButton(1, 0, "GAME MODE", gamemode_description, Menu_GameSettings_ApplyGameMode);
    Menu_DrawOptionButton(1, gamemode_string);

    // Difficulty button
    Menu_DrawButton(2, 1, "DIFFICULTY", difficulty_description, Menu_GameSettings_ApplyDifficulty);
    Menu_DrawOptionButton(2, difficulty_string); 

    // Start Round slider
    Menu_DrawButton(3, 2, "START ROUND", startround_string, NULL);
    Menu_DrawOptionSlider(3, 2, 0, 50, sv_startround, "sv_startround", true, true, 5.0f);

    // Magic button
    Menu_DrawButton(4, 3, "MAGIC", "Whether to allow Perks, Power-Ups, and the Mystery Box.", Menu_GameSettings_ApplyMagic);
    Menu_DrawOptionButton(4, magic_string);

    // Headshots Only button
    Menu_DrawButton(5, 4, "HEADSHOTS ONLY", "Headshots are the only means of Death. Explosives don't work, either.", Menu_GameSettings_ApplyHeadShotsOnly);
    Menu_DrawOptionButton(5, headshot_string);

    // Horde Size slider
    Menu_DrawButton(6, 5, "HORDE SIZE", "Maximum Zombies that can Active at once.", NULL);
    Menu_DrawOptionSlider(6, 5, 2, 64, sv_maxai, "sv_maxai", false, true, 2.0f);

    // Fast Rounds button
    Menu_DrawButton(7, 6, "FAST ROUNDS", "Minimize Time between Rounds.", Menu_GameSettings_ApplyFastRounds);
    Menu_DrawOptionButton(7, fast_string);

    // Back button
    Menu_DrawButton(-1, 7, "BACK", "Return to Pre-Game Menu.", Menu_Lobby_Set);
}