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
/* QUIT/RESTART MENU */

int	menu_quit_cursor;
#define QUIT_ITEMS 2

qboolean is_restart;
char* quit_string;
int previous_menu;

void Menu_Quit_Set (qboolean restart)
{
    quit_string = malloc(32*sizeof(char));

    previous_menu = m_state;
    m_state = m_quit;

    is_restart = restart;

    if (restart) {
        quit_string = "Are you sure you want to restart?";
    } else {
        quit_string = "Are you sure you want to quit?";
    }

    menu_quit_cursor = 0;
}

void Menu_Quit_Draw (void)
{
    int y_pos = (vid.height/3);

    // Background
	Menu_DrawCustomBackground ();

    if (previous_menu == m_paused) {
        // Header
	    Menu_DrawTitle ("PAUSED", MENU_COLOR_WHITE);

        // Resume
        Menu_DrawButton (1, -1, "RESUME CARNAGE", MENU_BUTTON_INACTIVE, "");
        // Restart
        Menu_DrawButton (2, -1, "RESTART LEVEL", MENU_BUTTON_INACTIVE, "");
        // Options
        Menu_DrawButton (3, -1, "OPTIONS", MENU_BUTTON_INACTIVE, "");
        // End game
        Menu_DrawButton (4, -1, "END GAME", MENU_BUTTON_INACTIVE, "");
    } else if (previous_menu == m_main) {
        // Draw Fake Main Menu Buttons
    }

    // Dark red background
    Draw_FillByColor (0, 0, vid.width, vid.height, 255, 0, 0, 20);

    // Black background box
    Draw_FillByColor (0, vid.height/3, vid.width, vid.height/3, 0, 0, 0, 255);
    // Top yellow line
    Draw_FillByColor (0, (vid.height/3)-small_bar_height, vid.width, small_bar_height, 255, 255, 0, 255);
    // Bottom yellow line
    Draw_FillByColor (0, vid.height-(vid.height/3), vid.width, small_bar_height, 255, 255, 0, 255);

    // Quit/Restart Text
    Menu_DrawTextCentered (vid.width/2, y_pos+CHAR_HEIGHT, quit_string, 255, 255, 255, 255);
    // Lose progress text
    Menu_DrawTextCentered (vid.width/2, y_pos+(CHAR_HEIGHT*2), "You will lose any progress that you have made.", 255, 255, 255, 255);

    Menu_DrawButton (6.5, 1, "GET ME OUTTA HERE!", MENU_BUTTON_ACTIVE, "");
    Menu_DrawButton (7.5, 2, "I WILL PERSEVERE", MENU_BUTTON_ACTIVE, "");
}

void Menu_Quit_Key (int key)
{
    switch (key)
	{

    case K_DOWNARROW:
        Menu_StartSound(MENU_SND_NAVIGATE);
        if (++menu_quit_cursor >= QUIT_ITEMS)
            menu_quit_cursor = 0;
        break;

	case K_UPARROW:
		Menu_StartSound(MENU_SND_NAVIGATE);
		if (--menu_quit_cursor < 0)
			menu_quit_cursor = QUIT_ITEMS - 1;
		break;

	case K_AUX1:
        Menu_StartSound(MENU_SND_ENTER);
        if (previous_menu == m_paused) {
            switch (menu_quit_cursor) {
                case 0:
                    if (is_restart) {
                        //Menu_LoadMap(current_selected_bsp);
                        key_dest = key_game;
                        m_state = m_none;
                        // Perform Soft Restart
                        PR_ExecuteProgram (pr_global_struct->Soft_Restart);
                    } else {
                        Menu_Main_Set();
                    }
                    break;
                case 1:
                    Menu_Paused_Set();
                    break;
            }
        }
	}
}