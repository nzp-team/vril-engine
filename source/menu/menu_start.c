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
/* START SCREEN */

/*
===============
Menu_Start_Set
===============
*/
void Menu_Start_Set ()
{
	Menu_LoadPics ();
	key_dest = key_menu;
	m_state = m_start;
	loadingScreen = 0;
}

// TODO - make this a platform global
char *enter_char = "A";

/*
===============
Menu_Start_Draw
===============
*/
void Menu_Start_Draw ()
{
    char start_string[24];

    snprintf(start_string, sizeof(start_string), "Press %s to Start", enter_char);

    // Fill black to make everything easier to see
	Draw_FillByColor(0, 0, vid.width, vid.height, 0, 0, 0, 255);

	// Draw Background;
	Draw_StretchPic(0, 0, menu_bk, vid.width, vid.height);

	Draw_ColoredStringCentered(vid.height - (vid.height/14), start_string, 255, 0, 0, 255, menu_scale_factor);
}

/*
===============
Menu_Start_Key
===============
*/
void Menu_Start_Key (int key)
{
	switch (key)
	{
		case K_ENTER:
		case K_AUX1:
            Menu_StartSound(MENU_SND_ENTER);
			Menu_Main_Set();
			break;
	}
}