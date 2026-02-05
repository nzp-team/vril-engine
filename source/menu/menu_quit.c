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

#define         QUIT_ITEMS 6

menu_t          quit_menu;
menu_button_t   quit_menu_buttons[QUIT_ITEMS];

qboolean        is_restart;
char*           quit_string;
int             previous_menu;

void Menu_Quit_DecideResume (void)
{
    if (previous_menu == m_pause) {
        Menu_Pause_Set();
    } else {
        Menu_Main_Set(false);
    }
}

void Menu_Quit_DecideQuit (void)
{
    if (is_restart) {
        key_dest = key_game;
        m_state = m_none;
        // Perform Soft Restart
        PR_ExecuteProgram (pr_global_struct->Soft_Restart);
    } else {
        if (key_dest == key_menu_pause) {
            Menu_Main_Set(false);
        } else {
            key_dest = key_console;
	        Host_Quit_f ();
        }
    }
}

void Menu_Quit_BuildMenuItems (void)
{
	quit_menu_buttons[0] = (menu_button_t){ false, -1, NULL };
	quit_menu_buttons[1] = (menu_button_t){ false, -1, NULL };
	quit_menu_buttons[2] = (menu_button_t){ false, -1, NULL };
	quit_menu_buttons[3] = (menu_button_t){ false, -1, NULL };
    quit_menu_buttons[4] = (menu_button_t){ true, 0, Menu_Quit_DecideQuit };
    quit_menu_buttons[5] = (menu_button_t){ true, 1, Menu_Quit_DecideResume };

	quit_menu = (menu_t) {
		quit_menu_buttons,
		QUIT_ITEMS,
		0
	};
}

void Menu_Quit_Set (qboolean restart)
{
    previous_menu = m_state;
    is_restart = restart;
    quit_string = malloc(32*sizeof(char));

    Menu_Quit_BuildMenuItems();

    if (restart) {
        quit_string = "Are you sure you want to restart?";
    } else {
        quit_string = "Are you sure you want to quit?";
    }

    m_state = m_quit;
}

void Menu_Quit_Draw (void)
{
    int y_pos = (vid.height/3);

    // Background
	Menu_DrawCustomBackground (true);

    if (previous_menu == m_pause) {
        // Header
	    Menu_DrawTitle ("PAUSED", MENU_COLOR_WHITE);

        // Resume
        Menu_DrawButton (1, &quit_menu, &quit_menu_buttons[0], "RESUME CARNAGE", "");
        // Restart
        Menu_DrawButton (2, &quit_menu, &quit_menu_buttons[1], "RESTART LEVEL", "");
        // Options
        Menu_DrawButton (3, &quit_menu, &quit_menu_buttons[2], "OPTIONS", "");
        // End game
        Menu_DrawButton (4, &quit_menu, &quit_menu_buttons[3], "END GAME", "");
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

    Menu_DrawButton (6.5, &quit_menu, &quit_menu_buttons[4], "GET ME OUTTA HERE!", "");
    Menu_DrawButton (7.5, &quit_menu, &quit_menu_buttons[5], "I WILL PERSEVERE", "");
}

void Menu_Quit_Key (int key)
{
    Menu_KeyInput(key, &quit_menu);
}