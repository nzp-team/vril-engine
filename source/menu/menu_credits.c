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
/* CREDITS MENU */

int CREDITS_LENGTH = 0;

typedef struct credits_s
{
	char *header;
	char *contributors[4];
} credits_t;

credits_t credits[6];

char *credits_models[4] =
{
    "blubs, Ju[s]tice, Derped_Crusader, cypress,",
    "BCDeshiG, Revnova, Naievil, Stoohp,",
    "Jacob Giguere, Lexi",
    ""
};

char *credits_code[4] =
{
    "blubs, Jukki, DR_Mabuse1981, Naievil, cypress,",
    "Scatterbox, Peter0x44",
    "",
    ""
};

char *credits_art[4] =
{
    "Ju[s]tice, blubs, cypress, Derped_Crusader",
    "",
    "",
    ""
};

char *credits_music[4] =
{
    "blubs, Marty P., cypress",
    "",
    "",
    ""
};

char *credits_sfx[4] =
{
    "blubs, Biodude, cypress",
    "",
    "",
    ""
};

char *credits_thx[4] =
{
    "Spike, eukara, Shpuld, Crow_Bar, st1x51,",
    "fgsfdsfgs, MasterFeizz, Rinnegatamante,",
    "Azenn",
    ""
};

/*
===============
Menu_Credits_Set
===============
*/
void Menu_Credits_Set (void)
{
	Menu_ResetMenuButtons();

    credits[0] = (credits_t) {
        .header = "MODELING, MAPS",
        .contributors[0] = credits_models[0],
        .contributors[1] = credits_models[1],
        .contributors[2] = credits_models[2],
        .contributors[3] = credits_models[3]
    };
    credits[1] = (credits_t) {
        .header = "PROGRAMMING",
        .contributors[0] = credits_code[0],
        .contributors[1] = credits_code[1],
        .contributors[2] = credits_code[2],
        .contributors[3] = credits_code[3]
    };
    credits[2] = (credits_t) {
        .header = "2D ART",
        .contributors[0] = credits_art[0],
        .contributors[1] = credits_art[1],
        .contributors[2] = credits_art[2],
        .contributors[3] = credits_art[3]
    };
    credits[3] = (credits_t) {
        .header = "MUSIC",
        .contributors[0] = credits_music[0],
        .contributors[1] = credits_music[1],
        .contributors[2] = credits_music[2],
        .contributors[3] = credits_music[3]
    };
    credits[4] = (credits_t) {
        .header = "SFX",
        .contributors[0] = credits_sfx[0],
        .contributors[1] = credits_sfx[1],
        .contributors[2] = credits_sfx[2],
        .contributors[3] = credits_sfx[3]
    };
    credits[5] = (credits_t) {
        .header = "SPECIAL THANKS",
        .contributors[0] = credits_thx[0],
        .contributors[1] = credits_thx[1],
        .contributors[2] = credits_thx[2],
        .contributors[3] = credits_thx[3]
    };

    CREDITS_LENGTH = sizeof(credits)/sizeof(credits[0]);

    m_previous_state = m_state;
	m_state = m_credits;
}

/*
===============
Menu_Credits_Draw
===============
*/
void Menu_Credits_Draw (void)
{
	Menu_DrawCustomBackground (true);
	Menu_DrawTitle ("CREDITS", MENU_COLOR_WHITE);
    Menu_DrawMapPanel();

    for(int i = 0; i < CREDITS_LENGTH; i++) {
        Menu_DrawCreditHeader(i, credits[i].header);

        for(int j = 0; j < 4; j++) {
            Menu_DrawCreditContributor(i, j, credits[i].contributors[j]);
        }
    }

	Menu_DrawButton(-1, 0, "BACK", "Return to Main Menu.", Menu_Main_Set);
}