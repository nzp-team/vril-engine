/*
 * Copyright (C) 1996-1997 Id Software, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#pragma once

void
HUD_Init(void);
void
HUD_Draw(void);
void
HUD_CenterPrint(char * str);
void
HUD_UsePrint(int index, int cost);
void HUD_RegisterUsePrint(int index, const char *text, int red, int green, int blue);
void
HUD_PowerupToast(int powerup);
void
HUD_Hitmark(int type);
void
HUD_NewMap(void);

extern double HUD_Change_time;


#define MAX_ACHIEVEMENTS 5// 23
typedef struct achievement_list_s {
    int  img;
    int  unlocked;
    char name[64];
    char description[256];
    int  progress;
} achievement_list_t;

extern achievement_list_t achievement_list[MAX_ACHIEVEMENTS];
extern int achievement_locked;
extern char player_name[16];
extern double nameprint_time;

extern int perk_order[8];
extern int current_perk_order;
extern int perk_orientation;
extern double crosshair_spread_time;
extern float cur_spread;
extern float crosshair_offset_step;

extern int screenflash_color;
extern double screenflash_duration;
extern int screenflash_type;
extern double screenflash_worktime;
extern double screenflash_starttime;

extern vec3_t round_color_target;

#define HUD_PERK_ORI_DEFAULT 0
#define HUD_PERK_ORI_CW      1

//
// Types of screen-flashes.
//

// Colors
#define SCREENFLASH_COLOR_WHITE 0
#define SCREENFLASH_COLOR_BLACK 1

// Types
#define SCREENFLASH_FADE_INANDOUT 0
#define SCREENFLASH_FADE_IN       1
#define SCREENFLASH_FADE_OUT      2
