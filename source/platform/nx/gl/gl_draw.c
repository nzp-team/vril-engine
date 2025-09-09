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

// draw.c -- this is the only file outside the refresh that touches the
// vid buffer

#include "../../../nzportable_def.h"

static int	sniper_scope;
static int	char_texture;

//Loading Fill by Crow_bar
float 	loading_cur_step;
char	loading_name[32];
float 	loading_num_step;
int 	loading_step;
float 	loading_cur_step_bk;

typedef struct {
    int texnum;
    float sl, tl, sh, th;
    qpic_t pic;
} glpic_t;

//=============================================================================
/* Support Routines */

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
void Draw_Init(void) {
    
    GL_InitTextures();

    char_texture = Image_LoadImage ("gfx/charset", IMAGE_TGA, 0, true, false);
	if (char_texture < 0)// did not find a matching TGA...
		Sys_Error ("Could not load charset, make sure you have every folder and file installed properly\nDouble check that all of your files are in their correct places\nAnd that you have installed the game properly.\n");

    GL_Bind(char_texture);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    //
	// get the other pics we need
	//
	sniper_scope = Image_LoadImage ("gfx/hud/scope", IMAGE_TGA, 0, true, false);

	Clear_LoadingFill ();

	InitKerningMap();
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
	size = 0.0625;

	GL_Bind (char_texture);

	glEnable(GL_ALPHA_TEST);

	glBegin (GL_QUADS);
	glTexCoord2f (fcol, frow);
	glVertex2f (x, y);
	glTexCoord2f (fcol + size, frow);
	glVertex2f (x+8, y);
	glTexCoord2f (fcol + size, frow + size);
	glVertex2f (x+8, y+8);
	glTexCoord2f (fcol, frow + size);
	glVertex2f (x, y+8);
	glEnd ();

	glDisable(GL_ALPHA_TEST);
}

/*
================
Draw_CharacterRGBA

Draws one 8*8 graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
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

	frow = row*0.0625f;
	fcol = col*0.0625f;
	size = 0.0625f*scale;

	GL_Bind (char_texture);

	glEnable(GL_BLEND);
	glColor4f(r/255, g/255, b/255, a/255);
	glDisable (GL_ALPHA_TEST);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	glBegin (GL_QUADS);
	glTexCoord2f (fcol, frow);
	glVertex2f (x, y);
	glTexCoord2f (fcol + (size/scale), frow);
	glVertex2f (x+(8*(scale)), y);
	glTexCoord2f (fcol + (size/scale), frow + (size/scale));
	glVertex2f (x+(8*(scale)), y+(8*(scale)));
	glTexCoord2f (fcol, frow + (size/scale));
	glVertex2f (x, y+(8*(scale)));
	glEnd ();
	
	glColor4f(1,1,1,1);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glEnable(GL_ALPHA_TEST);
	glDisable (GL_BLEND);
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

void Draw_ColoredString(int x, int y, char *str, float r, float g, float b, float a, float scale) 
{
	while (*str)
	{
		Draw_CharacterRGBA (x, y, *str, r, g, b, a, (int)scale);

		// Hooray for variable-spacing!
		if (*str == ' ')
			x += 4 * (int)scale;
		else if ((int)*str < 33 || (int)*str > 126)
            x += 8 * (int)scale;
		else
            x += (font_kerningamount[(int)(*str - 33)] + 1) * (int)scale;
		
		str++;
	}
}

int getTextWidth(char *str, float scale)
{
	int width = 0;

    for (int i = 0; i < strlen(str); i++) {
        // Hooray for variable-spacing!
		if (str[i] == ' ')
			width += 4 * (int)scale;
		else if ((int)str[i] < 33 || (int)str[i] > 126)
            width += 8 * (int)scale;
		else
            width += (font_kerningamount[(int)(str[i] - 33)] + 1) * (int)scale;
    }

	return width;
}


void Draw_ColoredStringCentered(int y, char *str, float r, float g, float b, float a, float scale)
{
	Draw_ColoredString((vid.width - getTextWidth(str, scale))/2, y, str, r, g, b, a, scale);
}


/*
=============
Draw_AlphaPic
=============
*/
void Draw_AlphaPic (int x, int y, int pic, float alpha)
{
	Draw_ColorPic(x, y, pic, 255, 255, 255, alpha);
}

/*
=============
Draw_Pic
=============
*/
void Draw_Pic (int x, int y, int pic)
{
	Draw_ColorPic(x, y, pic, 255, 255, 255, 255);
}

/*
=============
Draw_ColoredStretchPic
=============
*/
void Draw_ColoredStretchPic (int x, int y, int pic, int x_value, int y_value, int r, int g, int b, int a)
{
	glEnable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glColor4f(r/255.0,g/255.0,b/255.0,a/255.0);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	GL_Bind (pic);
	glBegin (GL_QUADS);
	glTexCoord2f (0, 0);
	glVertex2f (x, y);
	glTexCoord2f (1, 0);
	glVertex2f (x+x_value, y);
	glTexCoord2f (1, 1);
	glVertex2f (x+x_value, y+y_value);
	glTexCoord2f (0, 1);
	glVertex2f (x, y+y_value);
	glEnd ();

	glColor4f(1,1,1,1);
}

/*
=============
Draw_StretchPic
=============
*/
void Draw_StretchPic (int x, int y, int pic, int x_value, int y_value)
{
	glEnable(GL_ALPHA_TEST);
	glColor4f(1,1,1,1);

	GL_Bind (pic);
	glBegin (GL_QUADS);
	glTexCoord2f (0, 0);
	glVertex2f (x, y);
	glTexCoord2f (1, 0);
	glVertex2f (x+x_value, y);
	glTexCoord2f (1, 1);
	glVertex2f (x+x_value, y+y_value);
	glTexCoord2f (0, 1);
	glVertex2f (x, y+y_value);
	glEnd ();

	glColor4f(1,1,1,1);
}

/*
=============
Draw_ColorPic
=============
*/
void Draw_ColorPic (int x, int y, int pic, float r, float g , float b, float a)
{
	glDisable(GL_ALPHA_TEST);
	glEnable(GL_BLEND);
	glColor4f(r/255.0f,g/255.0f,b/255.0f,a/255.0f);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	gltexture_t *glt = &gltextures[pic];
	GL_Bind (pic);

	glBegin (GL_QUADS);
	glTexCoord2f (0, 0);
	glVertex2f (x, y);
	glTexCoord2f (1, 0);
	glVertex2f (x+glt->width, y);
	glTexCoord2f (1, 1);
	glVertex2f (x+glt->width, y+glt->height);
	glTexCoord2f (0, 1);
	glVertex2f (x, y+glt->height);
	glEnd ();

	glDisable(GL_BLEND);
	//glDisable(GL_ALPHA_TEST);
	glColor4f(1,1,1,1);
}

/*
=============
Draw_TransPic
=============
*/
void Draw_TransPic (int x, int y, int pic)
{
	gltexture_t *glt = &gltextures[pic];

	if (x < 0 || (unsigned)(x + glt->width) > vid.width || y < 0 ||
		 (unsigned)(y + glt->height) > vid.height)
	{
		Sys_Error ("bad coordinates");
	}
		
	Draw_Pic (x, y, pic);
}

/*
================
Draw_ConsoleBackground

================
*/
void Draw_ConsoleBackground (int lines)
{
	Draw_FillByColor(0, 0, vid.width, lines, 0, 0, 0, 255);
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

	int size       	= 8;
	int max_step   	= 350;
    int x          	= (vid.width  / 2) - (max_step / 2);
    int y          	= vid.height - (size/ 2) - 25;
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
	
	Draw_ColoredStringCentered(y, text, 255, 255, 255, 255, 1);

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
Draw_FillByColor

Fills a box of pixels with a single color
=============
*/
void Draw_FillByColor (int x, int y, int w, int h, int r, int g, int b, int a)
{
	glDisable (GL_TEXTURE_2D);
	glEnable (GL_BLEND); //johnfitz -- for alpha
	glDisable (GL_ALPHA_TEST); //johnfitz -- for alpha
	glColor4f ((float)(r/255.0f), (float)(g/255.0f), (float)(b/255.0f), (float)(a/255.0f));

	glBegin (GL_QUADS);

	glVertex2f (x,y);
	glVertex2f (x+w, y);
	glVertex2f (x+w, y+h);
	glVertex2f (x, y+h);

	glEnd ();
	glColor4f (1,1,1,1);
	glDisable (GL_BLEND); //johnfitz -- for alpha
	glEnable(GL_ALPHA_TEST); //johnfitz -- for alpha
	glEnable (GL_TEXTURE_2D);
}
//=============================================================================

byte *StringToRGB (char *s)
{
	byte		*col;
	static	byte	rgb[4];

	Cmd_TokenizeString (s);
	if (Cmd_Argc() == 3)
	{
		rgb[0] = (byte)Q_atoi(Cmd_Argv(0));
		rgb[1] = (byte)Q_atoi(Cmd_Argv(1));
		rgb[2] = (byte)Q_atoi(Cmd_Argv(2));
	}
	else
	{
		col = (byte *)&d_8to24table[(byte)Q_atoi(s)];
		rgb[0] = col[0];
		rgb[1] = col[1];
		rgb[2] = col[2];
	}
	rgb[3] = 255;

	return rgb;
}

extern cvar_t crosshair;
extern qboolean croshhairmoving;
//extern cvar_t cl_zoom;
extern int hitmark;
extern double Hitmark_Time, crosshair_spread_time;
extern float cur_spread;
extern float crosshair_offset_step; 
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
void Draw_Crosshair (void)
{	
	if (cl_crosshair_debug.value) {
		Draw_FillByColor(vid.width/2, 0, 1, 240, 255, 0, 0, 255);
		Draw_FillByColor(0, vid.height/2, 400, 1, 0, 255, 0, 255);
	}

	if (cl.stats[STAT_HEALTH] <= 20)
		return;

	if (cl.stats[STAT_ZOOM] == 2) {
		Draw_Pic (-39, -15, sniper_scope);
	}

   	if (Hitmark_Time > sv.time) {
        Draw_Pic ((vid.width - gltextures[hitmark].width)/2,(vid.height - gltextures[hitmark].height)/2, hitmark);
	}

	// Make sure to do this after hitmark drawing.
	if (cl.stats[STAT_ZOOM] == 2 || cl.stats[STAT_ZOOM] == 1)
		return;

	if (!crosshair_opacity)
		crosshair_opacity = 255;

	float col;

	if (sv_player->v.facingenemy == 1) {
		col = 0;
	} else {
		col = 255;
	}

	// crosshair moving
	if (crosshair_spread_time > sv.time && crosshair_spread_time)
    {
        cur_spread = cur_spread + 10;
		crosshair_opacity = 128;

		if (cur_spread >= CrossHairMaxSpread())
			cur_spread = CrossHairMaxSpread();
    }
	// crosshair not moving
	else if (crosshair_spread_time < sv.time && crosshair_spread_time)
    {
        cur_spread = cur_spread - 4;
		crosshair_opacity = 255;

		if (cur_spread <= 0) {
			cur_spread = 0;
			crosshair_spread_time = 0;
		}
    }

	int x_value, y_value;
    int crosshair_offset;

	// Standard crosshair (+)
	if (crosshair.value == 1) {
		crosshair_offset = CrossHairWeapon() + cur_spread;
		if (CrossHairMaxSpread() < crosshair_offset || croshhairmoving)
			crosshair_offset = CrossHairMaxSpread();

		if (sv_player->v.view_ofs[2] == 8) {
			crosshair_offset *= 0.80f;
		} else if (sv_player->v.view_ofs[2] == -10) {
			crosshair_offset *= 0.65f;
		}

		crosshair_offset_step += (crosshair_offset - crosshair_offset_step) * 0.5f;

		x_value = (vid.width - 3)/2.0f - crosshair_offset_step;
		y_value = (vid.height - 1)/2.0f;
		Draw_FillByColor(x_value, y_value, 3, 1, 255, (int)col, (int)col, (int)crosshair_opacity);

		x_value = (vid.width - 3)/2.0f + crosshair_offset_step;
		y_value = (vid.height - 1)/2.0f;
		Draw_FillByColor(x_value, y_value, 3, 1, 255, (int)col, (int)col, (int)crosshair_opacity);

		x_value = (vid.width - 1)/2.0f;
		y_value = (vid.height - 3)/2.0f - crosshair_offset_step;
		Draw_FillByColor(x_value, y_value, 1, 3, 255, (int)col, (int)col, (int)crosshair_opacity);

		x_value = (vid.width - 1)/2.0f;
		y_value = (vid.height - 3)/2.0f + crosshair_offset_step;
		Draw_FillByColor(x_value, y_value, 1, 3, 255, (int)col, (int)col, (int)crosshair_opacity);
	}
	// Area of Effect (o)
	else if (crosshair.value == 2) {
		Draw_CharacterRGBA((vid.width)/2-4, (vid.height)/2, 'O', 255, (int)col, (int)col, (int)crosshair_opacity, 1);
	}
	// Dot crosshair (.)
	else if (crosshair.value == 3) {
		Draw_CharacterRGBA((vid.width - 8)/2, (vid.height - 8)/2, '.', 255, (int)col, (int)col, (int)crosshair_opacity, 1);
	}
	// Grenade crosshair
	else if (crosshair.value == 4) {
		if (crosshair_pulse_grenade) {
			crosshair_offset_step = 0;
			cur_spread = 2;
		}

		crosshair_pulse_grenade = false;

		crosshair_offset = 12 + cur_spread;
		crosshair_offset_step += (crosshair_offset - crosshair_offset_step) * 0.5f;

		x_value = (vid.width - 3)/2.0f - crosshair_offset_step;
		y_value = (vid.height - 1)/2.0f;
		Draw_FillByColor(x_value, y_value, 3, 1, 255, 255, 255, 255);

		x_value = (vid.width - 3)/2.0f + crosshair_offset_step;
		y_value = (vid.height - 1)/2.0f;
		Draw_FillByColor(x_value, y_value, 3, 1, 255, 255, 255, 255);

		x_value = (vid.width - 1)/2.0f;
		y_value = (vid.height - 3)/2.0f - crosshair_offset_step;
		Draw_FillByColor(x_value, y_value, 1, 3, 255, 255, 255, 255);

		x_value = (vid.width - 1)/2.0f;
		y_value = (vid.height - 3)/2.0f + crosshair_offset_step;
		Draw_FillByColor(x_value, y_value, 1, 3, 255, 255, 255, 255);
	}
}

/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void Draw_Fill(int x, int y, int w, int h, int c) {
    Draw_FillByColor (x, y, w, h, c, c, c, 255);
}

//=============================================================================

/*
================
Draw_FadeScreen

================
*/
void Draw_FadeScreen(void) {
    glEnable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glColor4f(0, 0, 0, 0.8);
    glBegin(GL_QUADS);

    glVertex2f(0, 0);
    glVertex2f(vid.width, 0);
    glVertex2f(vid.width, vid.height);
    glVertex2f(0, vid.height);

    glEnd();
    glColor4f(1, 1, 1, 1);
    glEnable(GL_TEXTURE_2D);
    glDisable(GL_BLEND);
}

//=============================================================================

/*
================
GL_Set2D

Setup as if the screen was 320*200
================
*/
void GL_Set2D(void) {
    glViewport(glx, gly, glwidth, glheight);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, vid.width, vid.height, 0, -99999, 99999);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glEnable(GL_ALPHA_TEST);
    //      glDisable(GL_ALPHA_TEST);

    glColor4f(1, 1, 1, 1);
}
