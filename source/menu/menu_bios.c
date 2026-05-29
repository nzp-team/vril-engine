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
#include "menu_bios.h"

//=============================================================================
/* CHARACTER BIOS MENU */

int menu_bios_page;
int menu_bios_subpage;

void Menu_Bios_NextPage (void)
{
    Menu_ResetMenuButtons();
    menu_bios_page = 1;
}

void Menu_Bios_PrevPage (void)
{
    Menu_ResetMenuButtons();
    menu_bios_page = 0;
}

void Menu_Bios_SetPanel (void)
{
#ifndef MENU_HIDE_MAP_DESCRIPTIONS
    if (menu_bios_page == 0) {
        menu_bios_subpage = current_menu.cursor + 1;
    } else {
        menu_bios_subpage = (current_menu.cursor + 8);
    }

    if (menu_bios_subpage > 1)
        menu_bios_subpage = 0;
    else
        Menu_ResetMenuButtons();
#endif
}

void Menu_Bios_Set (void)
{
    Menu_ResetMenuButtons();

    key_dest = key_menu;
    m_previous_state = m_main;
	m_state = m_bios;

    menu_bios_page = 0;
    menu_bios_subpage = 0;
}

void Menu_Bios_Draw (void)
{
	Menu_DrawCustomBackground(true);
    Menu_DrawTitle("CHARACTER BIOS", MENU_COLOR_WHITE);
    Menu_DrawMapPanel();

    int i;

    if (menu_bios_subpage > 0)
    {
        Menu_DrawCharacterPanel(menu_bios[menu_bios_subpage - 1].description);
        Menu_DrawButton(-1, 0, "BACK", "", Menu_Bios_Set);
        return;
    }

    if (menu_bios_page == 0)
    {
        for (i = 0; i < 8; i++) {
            Menu_DrawBioButton(
                i + 1, 
                i, 
                menu_bios[i].name, 
                menu_bios[i].map, 
                menu_bios[i].portrait, 
                menu_bios[i].portrait_coords,
                Menu_Bios_SetPanel
            );
        }

        Menu_DrawDivider(-3.5);

        Menu_DrawGreyButton(-3, "PREVIOUS PAGE");
        Menu_DrawButton(-2, 8, "NEXT PAGE", "", Menu_Bios_NextPage);
        Menu_DrawButton(-1, 9, "BACK", "Return to Main Menu.", Menu_Main_Set);
    }
    else
    {
        for (i = 8; i < 12; i++) {
            Menu_DrawBioButton(
                i - 7, 
                i - 8, 
                menu_bios[i].name, 
                menu_bios[i].map, 
                menu_bios[i].portrait, 
                menu_bios[i].portrait_coords,
                Menu_Bios_SetPanel
            );
        }

        Menu_DrawDivider(-3.5);
        Menu_DrawButton(-3, 4, "PREVIOUS PAGE", "", Menu_Bios_PrevPage);
        Menu_DrawGreyButton(-2, "NEXT PAGE");
        Menu_DrawButton(-1, 5, "BACK", "Return to Main Menu.", Menu_Main_Set);
    }
}