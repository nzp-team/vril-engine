/*
Copyright (C) 1996-1997 Id Software, Inc.

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
// sbar.c -- status bar code

#include "../quakedef.h"


int			sb_updates;		// if >= vid.numpages, no update needed

#define STAT_MINUS		10	// num frame for '-' stats digit
qpic_t		*sb_nums[2][11];
qpic_t		*sb_colon, *sb_slash;
qpic_t		*sb_ibar;
qpic_t		*sb_sbar;
qpic_t		*sb_scorebar;

qpic_t      *sb_weapons[7][8];   // 0 is active, 1 is owned, 2-5 are flashes
qpic_t      *sb_ammo[4];
qpic_t		*sb_sigil[4];
qpic_t		*sb_armor[3];
qpic_t		*sb_items[32];

qpic_t	*sb_faces[7][2];		// 0 is gibbed, 1 is dead, 2-6 are alive
							// 0 is static, 1 is temporary animation
qpic_t	*sb_face_invis;
qpic_t	*sb_face_quad;
qpic_t	*sb_face_invuln;
qpic_t	*sb_face_invis_invuln;

extern qboolean	sb_showscores;

extern int			sb_lines;			// scan lines to draw

qpic_t      *rsb_invbar[2];
qpic_t      *rsb_weapons[5];
qpic_t      *rsb_items[2];
qpic_t      *rsb_ammo[3];
qpic_t      *rsb_teambord;		// PGM 01/19/97 - team color border

//MED 01/04/97 added two more weapons + 3 alternates for grenade launcher
qpic_t      *hsb_weapons[7][5];   // 0 is active, 1 is owned, 2-5 are flashes
//MED 01/04/97 added array to simplify weapon parsing
//int         hipweapons[4] = {HIT_LASER_CANNON_BIT,HIT_MJOLNIR_BIT,4,HIT_PROXIMITY_GUN_BIT};
//MED 01/04/97 added hipnotic items array
qpic_t      *hsb_items[2];

void Sbar_MiniDeathmatchOverlay (void);
void Sbar_DeathmatchOverlay (void);
void M_DrawPic (int x, int y, qpic_t *pic);

/*
===============
Sbar_ShowScores

Tab key down
===============
*/
void Sbar_ShowScores (void)
{

}

/*
===============
Sbar_DontShowScores

Tab key up
===============
*/
void Sbar_DontShowScores (void)
{

}

/*
===============
Sbar_Changed
===============
*/
void Sbar_Changed (void)
{

}

/*
===============
Sbar_Init
===============
*/
void Sbar_Init (void)
{

}


//=============================================================================

// drawing routines are relative to the status bar location

/*
=============
Sbar_DrawPic
=============
*/
void Sbar_DrawPic (int x, int y, qpic_t *pic)
{

}

/*
=============
Sbar_DrawTransPic
=============
*/
void Sbar_DrawTransPic (int x, int y, qpic_t *pic)
{

}

/*
================
Sbar_DrawCharacter

Draws one solid graphics character
================
*/
void Sbar_DrawCharacter (int x, int y, int num)
{

}

/*
================
Sbar_DrawString
================
*/
void Sbar_DrawString (int x, int y, char *str)
{

}

/*
=============
Sbar_itoa
=============
*/
int Sbar_itoa (int num, char *buf)
{

}


/*
=============
Sbar_DrawNum
=============
*/
void Sbar_DrawNum (int x, int y, int num, int digits, int color)
{

}

//=============================================================================

int		fragsort[MAX_SCOREBOARD];


/*
===============
Sbar_SortFrags
===============
*/
void Sbar_SortFrags (void)
{

}

int	Sbar_ColorForMap (int m)
{
	return m < 128 ? m + 8 : m + 8;
}

/*
===============
Sbar_UpdateScoreboard
===============
*/
void Sbar_UpdateScoreboard (void)
{

}



/*
===============
Sbar_SoloScoreboard
===============
*/
void Sbar_SoloScoreboard (void)
{
	
}

/*
===============
Sbar_DrawScoreboard
===============
*/
void Sbar_DrawScoreboard (void)
{

}

//=============================================================================

/*
===============
Sbar_DrawInventory
===============
*/
void Sbar_DrawInventory (void)
{

}

//=============================================================================

/*
===============
Sbar_DrawFrags
===============
*/
void Sbar_DrawFrags (void)
{

}

//=============================================================================


/*
===============
Sbar_DrawFace
===============
*/
void Sbar_DrawFace (void)
{

}

/*
===============
Sbar_Draw
===============
*/
void Sbar_Draw (void)
{

}

//=============================================================================

/*
==================
Sbar_IntermissionNumber

==================
*/
void Sbar_IntermissionNumber (int x, int y, int num, int digits, int color)
{

}

/*
==================
Sbar_DeathmatchOverlay

==================
*/
void Sbar_DeathmatchOverlay (void)
{

}

/*
==================
Sbar_DeathmatchOverlay

==================
*/
void Sbar_MiniDeathmatchOverlay (void)
{

}

/*
==================
Sbar_IntermissionOverlay

==================
*/
void Sbar_IntermissionOverlay (void)
{

}


/*
==================
Sbar_FinaleOverlay

==================
*/
void Sbar_FinaleOverlay (void)
{

}
