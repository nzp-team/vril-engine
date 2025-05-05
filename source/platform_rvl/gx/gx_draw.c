/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2008 Eluan Miranda

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

// draw.c -- this is the only file outside the refresh that touches the
// vid buffer

// ELUTODO: MANY assumptions about the pictures sizes

#include <ogc/system.h>
#include <ogc/cache.h>

#include "../../nzportable_def.h"
#include <gccore.h>

byte		*draw_chars;				// 8*8 graphic characters

int			sniper_scope;
int			sniper_scope_nb;

int			translate_texture;
int			char_texture;

int			white_texturenum;

//Loading Fill by Crow_bar
float 	loading_cur_step;
char	loading_name[32];
float 	loading_num_step;
int 	loading_step;
float 	loading_cur_step_bk;

cachepic_t	cachepics[MAX_CACHED_PICS];
cachepic_t	menu_cachepics[MAX_CACHED_PICS];
int			menu_numcachepics;
int			numcachepics;

int		pic_texels;
int		pic_count;

// ! " # $ % & ' ( ) * _ , - . / 0
// 1 2 3 4 5 6 7 8 9 : ; < = > ? @
// A B C D E F G H I J K L M N O P
// Q R S T U V W X Y Z [ \ ] ^ _ `
// a b c d e f g h i j k l m n o p
// q r s t u v w x y z { | } ~
int font_kerningamount[96];

void InitKerningMap(void)
{
	// Initialize the kerning amount as 8px for each
	// char in the event we cant load the file.
	for(int i = 0; i < 96; i++) {
		font_kerningamount[i] = 8;
	}

    FILE *kerning_map = fopen(va("%s/gfx/kerning_map.txt", com_gamedir), "r");
    if (kerning_map == NULL) {
        return;
    }

    char buffer[1024];
    if (fgets(buffer, sizeof(buffer), kerning_map) != NULL) {
        char *token = strtok(buffer, ",");
        int i = 0;
        while (token != NULL && i < 96) {
            font_kerningamount[i++] = atoi(token);
            token = strtok(NULL, ",");
        }
    }

    fclose(kerning_map);
}

/*
===============
Draw_Init
===============
*/
void Draw_Init (void)
{
	byte	white_texture[64] = { // ELUTODO assumes 0xfe is white
									0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe,
									0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe,
									0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe,
									0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe,
									0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe,
									0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe,
									0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe,
									0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe
								}; // ELUTODO no necessity to be this big?

	// load the console background and the charset
	// by hand, because we need to write the version
	// string into the background before turning
	// it into a texture
	draw_chars = Image_LoadPixels ("gfx/charset", IMAGE_TGA);
	// now turn them into textures
	char_texture = GL_LoadTexture ("charset", 128, 128, draw_chars, false, true, true, 4);

	white_texturenum = GL_LoadTexture("white_texturenum", 8, 8, white_texture, false, false, true, 1);

	sniper_scope = Image_LoadImage ("gfx/hud/scope", IMAGE_TGA, 0, true, false);
	sniper_scope_nb = Image_LoadImage ("gfx/hud/scope_256", IMAGE_TGA, 0, true, false);

	Clear_LoadingFill ();
	
	InitKerningMap();
}

/*
================
Draw_CharacterRGBA

This is the same as Draw_Character, but with RGBA color codes.
- Cypress
================
*/
extern cvar_t scr_coloredtext;
void Draw_CharacterRGBA(int x, int y, int num, float r, float g, float b, float a, float scale)
{
	int				row, col;
	float			frow, fcol, size;

	if (num == 32)
		return;		// space

	num &= 255;
	
	if (y <= -8)
		return;			// totally off screen

	row = num>>4;
	col = num&15;

	frow = row*0.0625;
	fcol = col*0.0625;
	size = 0.0625*(float)scale;

	GL_Bind (char_texture);
	
	GX_SetMinMag (GX_NEAR, GX_NEAR);

	QGX_Blend(true);
	QGX_Alpha(false);
	
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
	
	GX_Position3f32(x, y, 0.0f);
	GX_Color4u8(r, g, b, a);
	GX_TexCoord2f32(fcol, frow);

	GX_Position3f32(x+(8*(scale)), y, 0.0f);
	GX_Color4u8(r, g, b, a);
	GX_TexCoord2f32(fcol + (size/scale), frow);

	GX_Position3f32(x+(8*(scale)), y+(8*(scale)), 0.0f);
	GX_Color4u8(r, g, b, a);
	GX_TexCoord2f32(fcol + (size/scale), frow + (size/scale));

	GX_Position3f32(x, y+(8*(scale)), 0.0f);
	GX_Color4u8(r, g, b, a);
	GX_TexCoord2f32(fcol, frow + (size/scale));

	GX_End ();

	QGX_Alpha(true);
	QGX_Blend(false);
}


/*
================
Draw_Character

Draws one 8*8 graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
*/
void Draw_Character (int x, int y, int num)
{
	Draw_CharacterRGBA(x, y, num, 255, 255, 255, 255, 1);
}

void Draw_ColoredString(int x, int y, char *str, float r, float g, float b, float a, float scale) 
{
	while (*str)
	{
		Draw_CharacterRGBA (x, y, *str, r, g, b, a, scale);
		
		// Hooray for variable-spacing!
		if (*str == ' ')
			if (scale == 1.5) {
				x += 6 * scale;
			} else {
				x += 4 * scale;
			}
        else if ((int)*str < 33 || (int)*str > 126)
            if (scale == 1.5) {
				x += 12 * scale;
			} else {
				x += 8 * scale;
			}
        else
            x += (font_kerningamount[(int)(*str - 33)] + 1) * scale;
		
		str++;
		//x += 8*scale;
	}
}

/*
================
Draw_String
================
*/
void Draw_String (int x, int y, char *str)
{
	Draw_ColoredString(x, y, str, 255, 255, 255, 255, 1); 
}

int getTextWidth(char *str, float scale)
{
	float width = 0;

    for (int i = 0; i < strlen(str); i++) {
        // Hooray for variable-spacing!
		if (str[i] == ' ')
			if (scale == 1.5) {
				width += 6 * scale;
			} else {
				width += 4 * scale;
			}
        else if ((int)str[i] < 33 || (int)str[i] > 126)
			if (scale == 1.5) {
				width += 12 * scale;
			} else {
				width += 8 * scale;
			}
        else
            width += (font_kerningamount[(int)(str[i] - 33)] + 1) * scale;
    }

	return (int)width;
}


void Draw_ColoredStringCentered(int y, char *str, float r, float g, float b, float a, float scale)
{
	Draw_ColoredString((vid.width - getTextWidth(str, scale))/2, y, str, r, g, b, a, scale);
}

/*
================
Draw_DebugChar

Draws a single character directly to the upper right corner of the screen.
This is for debugging lockups by drawing different chars in different parts
of the code.
================
*/
void Draw_DebugChar (char num)
{
}

/*
=============
Draw_ColoredStretchPic
=============
*/
void Draw_ColoredStretchPic (int x, int y, int texnum, int x_value, int y_value, int r, int g , int b, int a)
{
	QGX_Alpha(false);
	QGX_Blend(true);
	
	GL_Bind (texnum);
	if (vid_retromode.value == 1)
		GX_SetMinMag (GX_NEAR, GX_NEAR);
	else
		GX_SetMinMag (GX_LINEAR, GX_LINEAR);

	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
	
	GX_Position3f32(x, y, 0.0f);
	GX_Color4u8(r, g, b, a);
	GX_TexCoord2f32(0, 0);

	GX_Position3f32(x+x_value, y, 0.0f);
	GX_Color4u8(r, g, b, a);
	GX_TexCoord2f32(1, 0);

	GX_Position3f32(x+x_value, y+y_value, 0.0f);
	GX_Color4u8(r, g, b, a);
	GX_TexCoord2f32(1, 1);

	GX_Position3f32(x, y+y_value, 0.0f);
	GX_Color4u8(r, g, b, a);
	GX_TexCoord2f32(0, 1);
	
	GX_End();

	QGX_Blend(false);
	QGX_Alpha(true);
}

/*
=============
Draw_StretchPic
=============
*/
void Draw_StretchPic (int x, int y, int texnum, int x_value, int y_value)
{
	Draw_ColoredStretchPic (x, y, texnum, x_value, y_value, 255, 255, 255, 255);
}

/*
=============
Draw_ColorPic
=============
*/
void Draw_ColorPic (int x, int y, int texnum, float r, float g , float b, float a)
{
	QGX_Alpha(false);
	QGX_Blend(true);
	
	GL_Bind (texnum);
	if (vid_retromode.value == 1)
		GX_SetMinMag (GX_NEAR, GX_NEAR);
	else
		GX_SetMinMag (GX_LINEAR, GX_LINEAR);

	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
	
	GX_Position3f32(x, y, 0.0f);
	GX_Color4u8(r, g, b, a);
	GX_TexCoord2f32(0, 0);

	GX_Position3f32(x + gltextures[texnum].width, y, 0.0f);
	GX_Color4u8(r, g, b, a);
	GX_TexCoord2f32(1, 0);

	GX_Position3f32(x + gltextures[texnum].width, y + gltextures[texnum].height, 0.0f);
	GX_Color4u8(r, g, b, a);
	GX_TexCoord2f32(1, 1);

	GX_Position3f32(x, y + gltextures[texnum].height, 0.0f);
	GX_Color4u8(r, g, b, a);
	GX_TexCoord2f32(0, 1);
	
	GX_End();

	QGX_Blend(false);
	QGX_Alpha(true);
}

/*
=============
Draw_AlphaPic
=============
*/
void Draw_AlphaPic (int x, int y, int texnum, float alpha)
{
	Draw_ColorPic(x, y, texnum, 255, 255, 255, alpha);
}


/*
=============
Draw_Pic
=============
*/
void Draw_Pic (int x, int y, int texnum)
{
	Draw_ColorPic(x, y, texnum, 255, 255, 255, 255);
}


/*
=============
Draw_TransPic
=============
*/
void Draw_TransPic (int x, int y, int texnum)
{
	if (x < 0 || (unsigned)(x + gltextures[texnum].width) > vid.conwidth || y < 0 ||
		 (unsigned)(y + gltextures[texnum].height) > vid.conheight)
	{
		Sys_Error ("Draw_TransPic: bad coordinates");
	}
		
	Draw_Pic (x, y, texnum);
}

/*
==================
Draw_TransAlphaPic
==================
*/
void Draw_TransAlphaPic (int x, int y, int texnum, float alpha)
{
	if (x < 0 || (unsigned)(x + gltextures[texnum].width) > vid.conwidth || y < 0 ||
		 (unsigned)(y + gltextures[texnum].height) > vid.conheight)
	{
		Sys_Error ("Draw_TransPic: bad coordinates");
	}
		
	Draw_AlphaPic (x, y, texnum, alpha);
}

/*
================
Draw_LoadingFill
By Crow_bar
================
*/
void Draw_LoadingFill(void)
{
    if(!loading_num_step)
		return;

	int size       	= 16;
	int max_step   	= 350;
    int x          	= (vid.width  / 2) - (max_step / 2);
    int y          	= vid.height - (size/ 2) - 35;
	//int l;
	//char str[64];
	char* text;


	if(loading_cur_step > loading_num_step)
	      loading_cur_step = loading_num_step;

	if (loading_cur_step < loading_cur_step_bk)
		loading_cur_step = loading_cur_step_bk;

	if (loading_cur_step == loading_num_step && loading_cur_step_bk != loading_num_step)
		loading_cur_step = loading_cur_step_bk;

    float loadsize = loading_cur_step * (max_step / loading_num_step);
	Draw_FillByColor (x - 2, y - 2, max_step + 4, size + 4, 69, 69, 69, 255);
	Draw_FillByColor (x, y, (int)loadsize, size, 0, 0, 0, 200);

	switch(loading_step) {
		case 1: text = "Loading Models.."; break;
		case 2: text = "Loading World.."; break;
		case 3: text = "Running Test Frame.."; break;
		case 4: text = "Loading Sounds.."; break;
		default: text = "Initializing.."; break;
	}

	//l = strlen (text);
	//Draw_String((vid.width - l*12)/2, y+2, text);
	Draw_ColoredStringCentered(y, text, 255, 255, 255, 255, 2);

	loading_cur_step_bk = loading_cur_step;
}

void Clear_LoadingFill (void)
{
    //it is end loading
	loading_cur_step = 0;
	loading_cur_step_bk = 0;
	loading_num_step = 0;
	loading_step = -1;
	memset(loading_name, 0, sizeof(loading_name));
}


/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void Draw_Fill (int x, int y, int w, int h, float r, float g, float b, float a)
{	
	QGX_Alpha(false);
	QGX_Blend(true);

	GL_Bind (white_texturenum);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
	
	GX_Position3f32(x, y, 0.0f);
	GX_Color4u8(r, g, b, a);
	GX_TexCoord2f32(0, 0);

	GX_Position3f32(x + w, y, 0.0f);
	GX_Color4u8(r, g, b, a);
	GX_TexCoord2f32(1, 0);

	GX_Position3f32(x + w, y + h, 0.0f);
	GX_Color4u8(r, g, b, a);
	GX_TexCoord2f32(1, 1);

	GX_Position3f32(x, y + h, 0.0f);
	GX_Color4u8(r, g, b, a);
	GX_TexCoord2f32(0, 1);

	GX_End ();
	
	QGX_Blend(false);
	QGX_Alpha(true);
}

/*
================
Draw_ConsoleBackground

================
*/
void Draw_ConsoleBackground (int lines)
{
	Draw_Fill(0, 0, vid.width, lines, 0, 0, 0, 85);
}

/*
=============
Draw_FillByColor

Fills a box of pixels with a single color
=============
*/
void Draw_FillByColor (int x, int y, int w, int h, int r, int g, int b, int a)
{
	Draw_Fill(x, y, w, h, r, g, b, a);
}
//=============================================================================

extern cvar_t crosshair;
extern qboolean croshhairmoving;
//extern cvar_t cl_zoom;
extern int hitmark;
double Hitmark_Time, crosshair_spread_time;
float cur_spread;
float crosshair_offset_step;

int CrossHairWeapon (void)
{
    int i;
	switch(cl.stats[STAT_ACTIVEWEAPON])
	{
		case W_COLT:
		case W_BIATCH:
		case W_357:
		case W_KILLU:
			i = 22;
			break;
		case W_PTRS:
		case W_PENETRATOR:
		case W_KAR_SCOPE:
		case W_HEADCRACKER:
		case W_KAR:
		case W_ARMAGEDDON:
		case W_SPRING:
		case W_PULVERIZER:
			i = 65;
			break;
		case W_MP40:
		case W_AFTERBURNER:
		case W_STG:
		case W_SPATZ:
		case W_THOMPSON:
		case W_GIBS:
		case W_BAR:
		case W_WIDOW:
		case W_PPSH:
		case W_REAPER:
		case W_RAY:
		case W_PORTER:
		case W_TYPE:
		case W_SAMURAI:
		case W_FG:
		case W_IMPELLER:
		case W_MP5:
		case W_KOLLIDER:
			i = 10;
			break;
		case W_BROWNING:
		case W_ACCELERATOR:
		case W_MG:
		case W_BARRACUDA:
			i = 30;
			break;
		case W_SAWNOFF:
		case W_SNUFF:
			i = 50;
			break;
		case W_TRENCH:
		case W_GUT:
		case W_DB:
		case W_BORE:
			i = 35;
			break;
		case W_GEWEHR:
		case W_COMPRESSOR:
		case W_M1:
		case W_M1000:
		case W_M1A1:
		case W_WIDDER:
			i = 5;
			break;
		case W_PANZER:
		case W_LONGINUS:
		case W_TESLA:
			i = 0;
			break;
		default:
			i = 0;
			break;
	}

	i *= 0.68;
	i += 6;

    if (cl.perks & 64)
        i *= 0.75;

    return i;
}
int CrossHairMaxSpread (void)
{
	int i;
	switch(cl.stats[STAT_ACTIVEWEAPON])
	{
		case W_COLT:
		case W_BIATCH:
		case W_STG:
		case W_SPATZ:
		case W_MP40:
		case W_AFTERBURNER:
		case W_THOMPSON:
		case W_GIBS:
		case W_BAR:
		case W_WIDOW:
		case W_357:
		case W_KILLU:
		case W_BROWNING:
		case W_ACCELERATOR:
		case W_FG:
		case W_IMPELLER:
		case W_MP5:
		case W_KOLLIDER:
		case W_MG:
		case W_BARRACUDA:
		case W_PPSH:
		case W_REAPER:
		case W_RAY:
		case W_PORTER:
		case W_TYPE:
		case W_SAMURAI:
			i = 48;
			break;
		case W_PTRS:
		case W_PENETRATOR:
		case W_KAR_SCOPE:
		case W_HEADCRACKER:
		case W_KAR:
		case W_ARMAGEDDON:
		case W_SPRING:
		case W_PULVERIZER:
			i = 75;
			break;
		case W_SAWNOFF:
		case W_SNUFF:
			i = 50;
			break;
		case W_DB:
		case W_BORE:
		case W_TRENCH:
		case W_GUT:
		case W_GEWEHR:
		case W_COMPRESSOR:
		case W_M1:
		case W_M1000:
		case W_M1A1:
		case W_WIDDER:
			i = 35;
			break;
		case W_PANZER:
		case W_LONGINUS:
		case W_TESLA:
			i = 0;
			break;
		default:
			i = 0;
			break;
	}

	i *= 0.68;
	i += 6;

    if (cl.perks & 64)
        i *= 0.75;

    return i;
}

/*
================
Draw_Crosshair
================
*/

extern float crosshair_opacity;
extern cvar_t cl_crosshair_debug;
extern qboolean crosshair_pulse_grenade;
extern cvar_t cl_crossx, cl_crossy;
extern qboolean aimsnap;
extern cvar_t sniper_center;
void Draw_Crosshair (void)
{	
	int scopex, scopey;

	if (cl_crosshair_debug.value) {
		Draw_FillByColor(vid.width/2, 0, 1, 240, 255, 0, 0, 255);
		Draw_FillByColor(0, vid.height/2, 400, 1, 0, 255, 0, 255);
	}
	
	if (key_dest != key_game) {
		return;
	}
	
	// We only want the crosshair to show in-game and while player is alive.
	if (cl.stats[STAT_HEALTH] < 1) {
		return;
	}

	if (cl.stats[STAT_ZOOM] == 2) {
		scopex = ((scr_vrect.x + scr_vrect.width/2 + cl_crossx.value)/* * vid.conwidth/vid.width*/) - 156;
		scopey = ((scr_vrect.y + scr_vrect.height/2 + cl_crossy.value)/* * vid.conheight/vid.height*/) - 156;
		
		if (sniper_center.value) {
			Draw_StretchPic (0, 0, sniper_scope, vid.width, vid.height);
		} else {
			Draw_StretchPic (scopex, scopey, sniper_scope_nb, 312, 312);
			//left screen
			Draw_FillByColor (0, 0, scopex, scr_vrect.height, 0, 0, 0, 255);
			//right screen
			Draw_FillByColor (scopex + 312, 0, scr_vrect.width - scopex, scr_vrect.height, 0, 0, 0, 255);
			//top screen
			Draw_FillByColor (0, 0, scr_vrect.width, scopey, 0, 0, 0, 255);
			//bottom screen
			Draw_FillByColor (0, scopey + 312, scr_vrect.width, scr_vrect.height - scopey, 0, 0, 0, 255);
		}
	}
	
   	if (Hitmark_Time > sv.time) {
		
		if ((cl.stats[STAT_ZOOM] == 1 && ads_center.value) || (cl.stats[STAT_ZOOM] == 2 && sniper_center.value)) {
			Draw_ColoredStretchPic ((vid.width - 12)/2, (vid.height - 12)/2, hitmark, 24, 24, 255, 255, 255, 225);
		}
		else {
			Draw_ColoredStretchPic (((scr_vrect.x + scr_vrect.width/2 + cl_crossx.value) * vid.conwidth/vid.width) - 12/* - hitmark->width*/,
				((scr_vrect.y + scr_vrect.height/2 + cl_crossy.value) * vid.conheight/vid.height) - 12/* - hitmark->height*/, hitmark, 24, 24, 255, 255, 255, 225);
		}
	}
				 
	/*
	Draw_Character ((scr_vrect.x + scr_vrect.width/2 + cl_crossx.value) * vid.conwidth/vid.width,
					(scr_vrect.y + scr_vrect.height/2 + cl_crossy.value - hitmark->height) * vid.conheight/vid.height, '+');
	*/
	
	float col;

	if (sv_player->v.facingenemy == 1) {
		col = 0;
	} else {
		col = 255;
	}

	// crosshair moving
	if ((crosshair_spread_time > sv.time && crosshair_spread_time) || croshhairmoving == true)
    {
        cur_spread = cur_spread + 4;
		if (cur_spread >= CrossHairMaxSpread())
			cur_spread = CrossHairMaxSpread()*1.5f;
		
		crosshair_opacity -= 8;
		if (crosshair_opacity <= 155)
			crosshair_opacity = 155;
    }
	// crosshair not moving
    else if ((crosshair_spread_time < sv.time && crosshair_spread_time) || croshhairmoving == false)
    {
        cur_spread = cur_spread - 2;
		if (cur_spread <= 0) {
			cur_spread = 0;
			crosshair_spread_time = 0;
		}
		
		crosshair_opacity += 20;
		if (crosshair_opacity >= 255)
			crosshair_opacity = 255;
    }

	int x_value, y_value;
    int crosshair_offset;
	
	//Draw_CharacterRGBA((scr_vrect.x + scr_vrect.width/2 + cl_crossx.value) * vid.conwidth/vid.width - 4, (scr_vrect.y + scr_vrect.height/2 + cl_crossy.value) * vid.conheight/vid.height - 8, '.', 255, (int)col, (int)col, 255, 1.4);
	if (aimsnap == true)
			Draw_FillByColor(vid.width/2 - 4, vid.height/2 - 4, 4, 4, 255, (int)col, (int)col, (int)crosshair_opacity);
		else
			Draw_FillByColor(((scr_vrect.x + scr_vrect.width - 4)/2 + cl_crossx.value) * vid.conwidth/vid.width, ((scr_vrect.y + scr_vrect.height - 4)/2 + cl_crossy.value) * vid.conheight/vid.height, 4, 4, 255, (int)col, (int)col, (int)crosshair_opacity);

	// Make sure to do this after hitmark drawing.
	if (cl.stats[STAT_ZOOM] == 1 || cl.stats[STAT_ZOOM] == 2 || cl.stats[STAT_ZOOM] == 3)
		return;

	// Standard crosshair (+)
	if (crosshair.value == 1) {
		crosshair_offset = CrossHairWeapon() + cur_spread;
		if (CrossHairMaxSpread() < crosshair_offset)
			crosshair_offset = CrossHairMaxSpread()*1.5f;

		if (sv_player->v.view_ofs[2] == 8) {
			crosshair_offset *= 0.80;
		} else if (sv_player->v.view_ofs[2] == -10) {
			crosshair_offset *= 0.65;
		}
		
		/*
		Draw_Character ((scr_vrect.x + scr_vrect.width/2 + cl_crossx.value) * vid.conwidth/vid.width,
						(scr_vrect.y + scr_vrect.height/2 + cl_crossy.value - hitmark->height) * vid.conheight/vid.height, '+');
		*/
		
		crosshair_offset_step += (crosshair_offset - crosshair_offset_step)/* * 0.5*/; //0.5 place

		//x_value = (vid.width - 3)/2 - crosshair_offset_step;
		//y_value = (vid.height - 1)/2;
		x_value = ((scr_vrect.x + scr_vrect.width - 12)/2 + cl_crossx.value) * vid.conwidth/vid.width - crosshair_offset_step;
		y_value = ((scr_vrect.y + scr_vrect.height - 3)/2 + cl_crossy.value) * vid.conheight/vid.height;
		Draw_FillByColor(x_value - 20, y_value, 13, 2, 255, (int)col, (int)col, (int)crosshair_opacity);

		//x_value = (vid.width - 3)/2 + crosshair_offset_step;
		//y_value = (vid.height - 1)/2;
		x_value = ((scr_vrect.x + scr_vrect.width - 12)/2 + cl_crossx.value) * vid.conwidth/vid.width + crosshair_offset_step;
		y_value = ((scr_vrect.y + scr_vrect.height - 3)/2 + cl_crossy.value) * vid.conheight/vid.height;
		Draw_FillByColor(x_value + 20, y_value, 13, 2, 255, (int)col, (int)col, (int)crosshair_opacity);

		//x_value = (vid.width - 1)/2;
		//y_value = (vid.height - 3)/2 - crosshair_offset_step;
		x_value = ((scr_vrect.x + scr_vrect.width - 3)/2 + cl_crossx.value) * vid.conwidth/vid.width;
		y_value = ((scr_vrect.y + scr_vrect.height - 12)/2 + cl_crossy.value) * vid.conheight/vid.height - crosshair_offset_step;
		Draw_FillByColor(x_value, y_value - 20, 2, 13, 255, (int)col, (int)col, (int)crosshair_opacity);

		//x_value = (vid.width - 1)/2;
		//y_value = (vid.height - 3)/2 + crosshair_offset_step;
		x_value = ((scr_vrect.x + scr_vrect.width - 3)/2 + cl_crossx.value) * vid.conwidth/vid.width;
		y_value = ((scr_vrect.y + scr_vrect.height - 12)/2 + cl_crossy.value) * vid.conheight/vid.height + crosshair_offset_step;
		Draw_FillByColor(x_value, y_value + 20, 2, 13, 255, (int)col, (int)col, (int)crosshair_opacity);
	}
	// Area of Effect (o)
	else if (crosshair.value == 2) {
		Draw_CharacterRGBA((scr_vrect.x + scr_vrect.width/2 + cl_crossx.value) * vid.conwidth/vid.width - 5, (scr_vrect.y + scr_vrect.height/2 + cl_crossy.value) * vid.conheight/vid.height - 5, 'O', 255, (int)col, (int)col, (int)crosshair_opacity, 2.0);
	}
	// Dot crosshair (.)
	else if (crosshair.value == 3) {
		Draw_CharacterRGBA((scr_vrect.x + scr_vrect.width/2 + cl_crossx.value) * vid.conwidth/vid.width - 5, (scr_vrect.y + scr_vrect.height/2 + cl_crossy.value) * vid.conheight/vid.height - 5, '.', 255, (int)col, (int)col, (int)crosshair_opacity, 2.0);
	}
	// Grenade crosshair
	else if (crosshair.value == 4) {
		if (crosshair_pulse_grenade) {
			crosshair_offset_step = 0;
			cur_spread = 2;
		}

		crosshair_pulse_grenade = false;

		crosshair_offset = 12 + cur_spread;
		crosshair_offset_step += (crosshair_offset - crosshair_offset_step) * 0.5;

		//x_value = (vid.width - 3)/2 - crosshair_offset_step;
		//y_value = (vid.height - 1)/2;
		x_value = ((scr_vrect.x + scr_vrect.width - 10)/2 + cl_crossx.value) * vid.conwidth/vid.width - crosshair_offset_step;
		y_value = ((scr_vrect.y + scr_vrect.height - 2)/2 + cl_crossy.value) * vid.conheight/vid.height;
		Draw_FillByColor(x_value - 20, y_value, 10, 2, 255, 255, 255, 255);

		//x_value = (vid.width - 3)/2 + crosshair_offset_step;
		//y_value = (vid.height - 1)/2;
		x_value = ((scr_vrect.x + scr_vrect.width - 10)/2 + cl_crossx.value) * vid.conwidth/vid.width + crosshair_offset_step;
		y_value = ((scr_vrect.y + scr_vrect.height - 2)/2 + cl_crossy.value) * vid.conheight/vid.height;
		Draw_FillByColor(x_value + 20, y_value, 10, 2, 255, 255, 255, 255);

		//x_value = (vid.width - 1)/2;
		//y_value = (vid.height - 3)/2 - crosshair_offset_step;
		x_value = ((scr_vrect.x + scr_vrect.width - 2)/2 + cl_crossx.value) * vid.conwidth/vid.width;
		y_value = ((scr_vrect.y + scr_vrect.height - 10)/2 + cl_crossy.value) * vid.conheight/vid.height - crosshair_offset_step;
		Draw_FillByColor(x_value, y_value - 20, 2, 10, 255, 255, 255, 255);

		//x_value = (vid.width - 1)/2;
		//y_value = (vid.height - 3)/2 + crosshair_offset_step;
		x_value = ((scr_vrect.x + scr_vrect.width - 2)/2 + cl_crossx.value) * vid.conwidth/vid.width;
		y_value = ((scr_vrect.y + scr_vrect.height - 10)/2 + cl_crossy.value) * vid.conheight/vid.height + crosshair_offset_step;
		Draw_FillByColor(x_value, y_value + 20, 2, 10, 255, 255, 255, 255);
	}
	
}


/*
================
Draw_FadeScreen

================
*/
void Draw_FadeScreen (void)
{
	// ELUTODO: do not use a texture
	QGX_Alpha(false);
	QGX_Blend(true);

	GL_Bind (white_texturenum);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);

	GX_Position3f32(0, 0, 0.0f);
	GX_Color4u8(0x00, 0x00, 0x00, 0xcc);
	GX_TexCoord2f32(0, 0);

	GX_Position3f32(vid.conwidth, 0, 0.0f);
	GX_Color4u8(0x00, 0x00, 0x00, 0xcc);
	GX_TexCoord2f32(1, 0);

	GX_Position3f32(vid.conwidth, vid.conheight, 0.0f);
	GX_Color4u8(0x00, 0x00, 0x00, 0xcc);
	GX_TexCoord2f32(1, 1);

	GX_Position3f32(0, vid.conheight, 0.0f);
	GX_Color4u8(0x00, 0x00, 0x00, 0xcc);
	GX_TexCoord2f32(0, 1);
	GX_End();

	QGX_Blend(false);
	QGX_Alpha(true);
}

//=============================================================================

/*
================
GL_Set2D

Setup as if the screen was 320*200
================
*/
void GL_Set2D (void)
{
	GX_SetViewport(glx, gly, glwidth > 640 ? 640 : glwidth, glheight, 0.0f, 1.0f);

	guOrtho(perspective,0, vid.height, 0, vid.width, ZMIN2D, ZMAX2D);
	GX_LoadProjectionMtx(perspective, GX_ORTHOGRAPHIC);

	guMtxIdentity(modelview);
	GX_LoadPosMtxImm(modelview, GX_PNMTX0);

	// ELUODO: filtering is making some borders
	QGX_ZMode(false);
	QGX_Blend(true);
	GX_SetCullMode(GX_CULL_NONE);
	QGX_Alpha(true);

	GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);

	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

	GX_SetNumTexGens(1);
	GX_SetNumTevStages(1);
}