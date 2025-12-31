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
#include "menu_defs.h"

//=============================================================================
/* PAUSE MENU */

int M_Paused_Cusor;
#define MAX_PAUSED_ITEMS 5

/*
===============
Menu_Paused_Set
===============
*/
void Menu_Paused_Set ()
{
	key_dest = key_menu_pause;
	m_state = m_paused_menu;
	m_entersound = true;
	loadingScreen = 0;
	loadscreeninit = false;
	M_Paused_Cusor = 0;
}

/*
===============
Menu_Paused_Draw
===============
*/
void Menu_Paused_Draw ()
{
	// Fill black to make everything easier to see
	Draw_FillByColor(0, 0, vid.width, vid.height, 0, 0, 0, 102);

	// Header
	Draw_ColoredString(10, 10, "PAUSED", 255, 255, 255, 255, 2);

	if (M_Paused_Cusor == 0)
		Draw_ColoredString(10, 135, "Resume", 255, 0, 0, 255, 1);
	else
		Draw_ColoredString(10, 135, "Resume", 255, 255, 255, 255, 1);

	if (M_Paused_Cusor == 1)
		Draw_ColoredString(10, 145, "Restart", 255, 0, 0, 255, 1);
	else
		Draw_ColoredString(10, 145, "Restart", 255, 255, 255, 255, 1);

	if (M_Paused_Cusor == 2)
		Draw_ColoredString(10, 155, "Settings", 255, 0, 0, 255, 1);
	else
		Draw_ColoredString(10, 155, "Settings", 255, 255, 255, 255, 1);

	if (waypoint_mode.value) {
		if (M_Paused_Cusor == 3)
			Draw_ColoredString(10, 165, "Save Waypoints", 255, 0, 0, 255, 1);
		else
			Draw_ColoredString(10, 165, "Save Waypoints", 255, 255, 255, 255, 1);
	} else {
		if (M_Paused_Cusor == 3)
			Draw_ColoredString(10, 165, "Achievements", 255, 0, 0, 255, 1);
		else
			Draw_ColoredString(10, 165, "Achievements", 255, 255, 255, 255, 1);
	}

	if (M_Paused_Cusor == 4)
		Draw_ColoredString(10, 175, "Main Menu", 255, 0, 0, 255, 1);
	else
		Draw_ColoredString(10, 175, "Main Menu", 255, 255, 255, 255, 1);
}

/*
===============
Menu_Paused_Key
===============
*/
void Menu_Paused_Key (int key)
{
	switch (key)
	{
		case K_ESCAPE:
		case K_AUX2:
			S_LocalSound ("sounds/menu/enter.wav");
			Cbuf_AddText("togglemenu\n");
			break;

		case K_DOWNARROW:
			S_LocalSound ("sounds/menu/navigate.wav");
			if (++M_Paused_Cusor >= Max_Paused_Iteams)
				M_Paused_Cusor = 0;
			break;

		case K_UPARROW:
			S_LocalSound ("sounds/menu/navigate.wav");
			if (--M_Paused_Cusor < 0)
				M_Paused_Cusor = Max_Paused_Iteams - 1;
			break;

		case K_ENTER:
		case K_AUX1:
			m_entersound = true;

			switch (M_Paused_Cusor)
			{
			case 0:
				key_dest = key_game;
				m_state = m_none;
				break;
			case 1:
				M_Menu_Restart_f();
				break;
			case 2:
				M_Menu_Options_f();
				key_dest = key_menu_pause;
				break;
			case 3:
				if (waypoint_mode.value) {
					Cbuf_AddText("impulse 101\n");
				}
				/*else
					M_Menu_Achievement_f();
				*/ // naievil -- fixme: do not have achievements
				key_dest = key_menu_pause;
				break;
			case 4:
				M_Menu_Exit_f();
				break;
			}
		}
}