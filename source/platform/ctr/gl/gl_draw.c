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
#include <3ds.h>

#define GL_COLOR_INDEX8_EXT     0x80E5
#define MAX_VRAM_TEX	256*256*4

cvar_t		gl_max_size = {"gl_max_size", "1024"};
cvar_t		gl_picmip = {"gl_picmip", "0"};

static int	sniper_scope;

int	char_texture;

typedef struct
{
	int		texnum;
	float	sl, tl, sh, th;
} glpic_t;

int		gl_lightmap_format = 4;
int		gl_solid_format = 3;
int		gl_alpha_format = 4;

int		gl_filter_min = GL_LINEAR_MIPMAP_NEAREST;
int		gl_filter_max = GL_LINEAR;

//Loading Fill by Crow_bar
float 	loading_cur_step;
char	loading_name[32];
float 	loading_num_step;
int 	loading_step;
float 	loading_cur_step_bk;

#define	MAX_GLTEXTURES	1024
static gltexture_t	gltextures[MAX_GLTEXTURES];
static int	numgltextures = 0;
static GLuint current_gl_id = 0;

void GL_Bind (int texnum)
{
	if (texnum < 0) {
		return;
	}

	gltexture_t *glt = &gltextures[texnum];
	if (!glt->used || (glt->gl_id < 0)) {
		Sys_Error("GL_Bind: Tried to bind unused texture %d\n", texnum);
		return;
	}

	if (current_gl_id != glt->gl_id) {
		glBindTexture(GL_TEXTURE_2D, glt->gl_id);
		current_gl_id = glt->gl_id;

		if (r_retro.value) {
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		} else {
			if (texnum == char_texture) {
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
			} else {
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			}
		}
	}
}

void GL_FreeTextures (int texnum)
{
	if (texnum < 0) return;

	gltexture_t *glt = &gltextures[texnum];
	if (glt->gl_id == current_gl_id) return;
	if (glt->used == false) return;
	if (glt->keep) return;

	glDeleteTextures(1, &glt->gl_id);
	glt->gl_id = -1;
	glt->texnum = -1;
	glt->identifier[0] = '\0';
	glt->width = 0;
	glt->height = 0;
	glt->original_width = 0;
	glt->original_height = 0;
	glt->bpp = 0;
	glt->keep = false;
	glt->used = false;

	//numgltextures--;
	current_gl_id = -1;
}

void GL_UnloadTextures (void)
{
	for (int i = 0; i < (MAX_GLTEXTURES); i++) {
		GL_FreeTextures(i);
	}
}
//=============================================================================
/* Support Routines */

typedef struct
{
	char *name;
	int	minimize, maximize;
} glmode_t;

glmode_t modes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_NEAREST", GL_NEAREST_MIPMAP_NEAREST, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_NEAREST", GL_LINEAR_MIPMAP_NEAREST, GL_LINEAR},
	{"GL_NEAREST_MIPMAP_LINEAR", GL_NEAREST_MIPMAP_LINEAR, GL_NEAREST},
	{"GL_LINEAR_MIPMAP_LINEAR", GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR}
};

/*
===============
Draw_TextureMode_f
===============
*/
void Draw_TextureMode_f (void)
{
	int		i;
	gltexture_t	*glt;

	if (Cmd_Argc() == 1)
	{
		for (i=0 ; i< 6 ; i++)
			if (gl_filter_min == modes[i].minimize)
			{
				Con_Printf ("%s\n", modes[i].name);
				return;
			}
		Con_Printf ("current filter is unknown???\n");
		return;
	}

	for (i=0 ; i< 6 ; i++)
	{
		if (!Q_strcasecmp (modes[i].name, Cmd_Argv(1) ) )
			break;
	}
	if (i == 6)
	{
		Con_Printf ("bad filter name\n");
		return;
	}

	gl_filter_min = modes[i].minimize;
	gl_filter_max = modes[i].maximize;

	// change all the existing mipmap texture objects
	for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
	{
		if (glt->mipmap)
		{
			GL_Bind (glt->texnum);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		}
	}
}

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
	int		start;

	numgltextures = 1;

	Cvar_RegisterVariable (&gl_max_size);
	Cvar_RegisterVariable (&gl_picmip);

	//if (!new3ds_flag) {
		//Cvar_SetValue("gl_picmip", 1);
		Cvar_Set ("gl_max_size", "256");
	//}

	Cmd_AddCommand ("gl_texturemode", &Draw_TextureMode_f);

	// now turn them into textures
	char_texture = Image_LoadImage ("gfx/charset", IMAGE_TGA, 0, true, false);
	if (char_texture < 0)// did not find a matching TGA...
		Sys_Error ("Could not load charset, make sure you have every folder and file installed properly\nDouble check that all of your files are in their correct places\nAnd that you have installed the game properly.\n");

	start = Hunk_LowMark();

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	// free loaded console
	Hunk_FreeToLowMark(start);

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
	if (str == NULL) return;

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
	if (str == NULL) return 0;

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
	if (pic > 0) {
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
		glDisable(GL_ALPHA_TEST);
		glColor4f(1,1,1,1);
	}
}

/*
=============
Draw_StretchPic
=============
*/
void Draw_StretchPic (int x, int y, int pic, int x_value, int y_value)
{
	if (pic > 0) {
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
		glDisable(GL_ALPHA_TEST);
		glColor4f(1,1,1,1);
	}
}

/*
=============
Draw_MenuPanningPic
=============
*/
void Draw_MenuPanningPic (int x, int y, int pic, int x_value, int y_value, float time)
{
	if (pic > 0) {
		// how much to "zoom in" (in texcoord space)
		const float zoom = 0.05f;   // 5% crop on both sides

		glEnable(GL_ALPHA_TEST);
		glColor4f(1,1,1,1);

		GL_Bind (pic);
		glBegin (GL_QUADS);
		glTexCoord2f (0, 0);
		glVertex2f (x, y);
		glTexCoord2f (1, 0);
		glVertex2f (x+(x_value) - zoom, y);
		glTexCoord2f (1, 1);
		glVertex2f (x+(x_value) - zoom, y+y_value);
		glTexCoord2f (0, 1);
		glVertex2f (x, y+y_value);
		glEnd ();
		glDisable(GL_ALPHA_TEST);
		glColor4f(1,1,1,1);
	}
}

/*
=============
Draw_ColorPic
=============
*/
void Draw_ColorPic (int x, int y, int pic, float r, float g , float b, float a)
{
	if (pic > 0) {
		glDisable(GL_ALPHA_TEST);
		glEnable(GL_BLEND);
		glColor4f(r/255.0f,g/255.0f,b/255.0f,a/255.0f);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

		gltexture_t *glt = &gltextures[pic];
		GL_Bind (glt->texnum);

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
		glDisable(GL_ALPHA_TEST);
		glColor4f(1,1,1,1);
	}
}

/*
=============
Draw_SubPic
=============
*/
void Draw_SubPic (int x, int y, int pic, float s, float t, float s_coord_size, float t_coord_size, float scale, float r, float g , float b, float a)
{
	if (pic > 0) {
		glDisable(GL_ALPHA_TEST);
		glEnable(GL_BLEND);
		glColor4f(r/255.0f,g/255.0f,b/255.0f,a/255.0f);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

		gltexture_t *glt = &gltextures[pic];
		GL_Bind (glt->texnum);

		float width_scale = scale * (s_coord_size / t_coord_size);

		float u_scale = (float)glt->original_width / (float)glt->width;
		float v_scale = (float)glt->original_height / (float)glt->height;

		float u0 = s * u_scale;
		float v0 = t * v_scale;
		float u1 = (s + s_coord_size) * u_scale;
		float v1 = (t + t_coord_size) * v_scale;

		glBegin (GL_QUADS);
		glTexCoord2f (u0, v0);
		glVertex2f (x, y);
		glTexCoord2f (u1, v0);
		glVertex2f (x+(glt->original_width*width_scale), y);
		glTexCoord2f (u1, v1);
		glVertex2f (x+(glt->original_width*width_scale), y+(glt->original_height*scale));
		glTexCoord2f (u0, v1);
		glVertex2f (x, y+(glt->original_height*scale));
		glEnd ();

		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		glDisable(GL_BLEND);
		glEnable(GL_ALPHA_TEST);
		glColor4f(1.0f,1.0f,1.0f,1.0f);
	}
}

/*
=============
Draw_SubPic
=============
*/
void Draw_SubPic (int x, int y, int pic, float s, float t, float r, float g , float b, float a)
{
	if (pic < 0) return;

	float size = 4;

	glDisable(GL_ALPHA_TEST);
	glEnable(GL_BLEND);
	glColor4f(r/255.0f,g/255.0f,b/255.0f,a/255.0f);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	gltexture_t *glt = &gltextures[pic];
	GL_Bind (pic);

	glBegin (GL_QUADS);
	glTexCoord2f (s, t);
	glVertex2f (x, y);
	glTexCoord2f (s, t);
	glVertex2f (x+(glt->width/size), y);
	glTexCoord2f (s, t);
	glVertex2f (x+(glt->width/size), y+(glt->height/size));
	glTexCoord2f (s, t);
	glVertex2f (x, y+(glt->height/size));
	glEnd ();

	glDisable(GL_BLEND);
	glColor4f(1.0f,1.0f,1.0f,1.0f);
}

/*
=============
Draw_TransPic
=============
*/
void Draw_TransPic (int x, int y, int pic)
{
	if (pic > 0) {
		gltexture_t *glt = &gltextures[pic];

		if (x < 0 || (unsigned)(x + glt->width) > vid.width || y < 0 ||
			(unsigned)(y + glt->height) > vid.height)
		{
			Sys_Error ("bad coordinates");
		}
			
		Draw_Pic (x, y, pic);
	}
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

extern cvar_t crosshair;
extern qboolean croshhairmoving;
//extern cvar_t cl_zoom;
extern int hitmark;
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
================
Draw_FadeScreen

================
*/
void Draw_FadeScreen (void)
{
	glEnable (GL_BLEND);
	glDisable (GL_TEXTURE_2D);
	glColor4f (0, 0, 0, 0.8);
	glBegin (GL_QUADS);

	glVertex2f (0,0);
	glVertex2f (vid.width, 0);
	glVertex2f (vid.width, vid.height);
	glVertex2f (0, vid.height);

	glEnd ();
	glColor4f (1,1,1,1);
	glEnable (GL_TEXTURE_2D);
	glDisable (GL_BLEND);
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
	glViewport (glx, gly, glwidth, glheight);

	glMatrixMode(GL_PROJECTION);
    glLoadIdentity ();
	glOrtho  (0, vid.width, vid.height, 0, -99999, 99999);

	glMatrixMode(GL_MODELVIEW);
    glLoadIdentity ();

	glDisable (GL_DEPTH_TEST);
	glDisable (GL_CULL_FACE);
	glDisable (GL_BLEND);
	//glEnable (GL_ALPHA_TEST);
	//glDisable (GL_ALPHA_TEST);

	glColor4f (1,1,1,1);
}

//====================================================================

/*
================
Image_FindImage
================
*/
// See if the texture is already present.
int Image_FindImage (const char *identifier)
{
	// See if the texture is already present.
	if (identifier[0]) {
		for (int i = 0; i < MAX_GLTEXTURES; ++i) {
			if (gltextures[i].used == true) {
				gltexture_t *glt = &gltextures[i];
				if (!strcmp (identifier, glt->identifier)) {
					return i;
				}
			}
		}
	}

	return -1;
}

/*
================
GL_ResampleTexture
================
*/
void GL_ResampleTexture (unsigned *in, int inwidth, int inheight, unsigned *out,  int outwidth, int outheight)
{
	int		i, j;
	unsigned	*inrow;
	unsigned	frac, fracstep;

	fracstep = inwidth*0x10000/outwidth;
	for (i=0 ; i<outheight ; i++, out += outwidth)
	{
		inrow = in + inwidth*(i*inheight/outheight);
		frac = fracstep >> 1;
		for (j=0 ; j<outwidth ; j+=4)
		{
			out[j] = inrow[frac>>16];
			frac += fracstep;
			out[j+1] = inrow[frac>>16];
			frac += fracstep;
			out[j+2] = inrow[frac>>16];
			frac += fracstep;
			out[j+3] = inrow[frac>>16];
			frac += fracstep;
		}
	}
}

/*
================
GL_MipMap

Operates in place, quartering the size of the texture
================
*/
void GL_MipMap (byte *in, int width, int height)
{
	int		i, j;
	byte	*out;

	width <<=2;
	height >>= 1;
	out = in;
	for (i=0 ; i<height ; i++, in+=width)
	{
		for (j=0 ; j<width ; j+=8, out+=4, in+=8)
		{
			out[0] = (in[0] + in[4] + in[width+0] + in[width+4])>>2;
			out[1] = (in[1] + in[5] + in[width+1] + in[width+5])>>2;
			out[2] = (in[2] + in[6] + in[width+2] + in[width+6])>>2;
			out[3] = (in[3] + in[7] + in[width+3] + in[width+7])>>2;
		}
	}
}

/*
===============
GL_Upload32
===============
*/
void GL_Upload32(GLuint gl_id, unsigned *data, int width, int height, qboolean mipmap, qboolean alpha)
{
    int scaled_width = 1, scaled_height = 1;
    while (scaled_width < width) scaled_width <<= 1;
    while (scaled_height < height) scaled_height <<= 1;

    scaled_width >>= (int)gl_picmip.value;
    scaled_height >>= (int)gl_picmip.value;

    if (scaled_width > gl_max_size.value) scaled_width = gl_max_size.value;
    if (scaled_height > gl_max_size.value) scaled_height = gl_max_size.value;

    if (scaled_width < 4) scaled_width = 4;
    if (scaled_height < 4) scaled_height = 4;

    unsigned *scaled = malloc(scaled_width * scaled_height * sizeof(unsigned));
    if (!scaled) Sys_Error("GL_Upload32: out of memory");

    // bind the texture
    glBindTexture(GL_TEXTURE_2D, gl_id);
	current_gl_id = gl_id; //update texture cache

    if (scaled_width != width || scaled_height != height)
        GL_ResampleTexture(data, width, height, scaled, scaled_width, scaled_height);
    else
        memcpy(scaled, data, width * height * sizeof(unsigned));

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);

    // mipmap generation (if enabled)
	
    if (mipmap) {
        int level = 0;
        int w = scaled_width, h = scaled_height;
        while (w > 1 || h > 1) {
            GL_MipMap((byte*)scaled, w, h);
            w = w > 1 ? w >> 1 : 1;
            h = h > 1 ? h >> 1 : 1;
            level++;
            glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
        }
    }

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, mipmap ? gl_filter_min : gl_filter_max);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);

    free(scaled);
}

/*
===============
GL_Upload8
===============
*/
void GL_Upload8 (GLuint gl_id, byte *data, int width, int height,  qboolean mipmap, qboolean alpha)
{
	int			i, s;
	qboolean	noalpha;
	int			p;
	unsigned 	*trans = malloc(width*height*4);

	s = width*height;
	// if there are no transparent pixels, make it a 3 component
	// texture even if it was specified as otherwise
	if (alpha) {
		noalpha = true;
		for (i=0 ; i<s ; i++) {
			p = data[i];
			if (p == 255)
				noalpha = false;
			trans[i] = d_8to24table[p];
		}

		if (alpha && noalpha)
			alpha = false;
	} else {
		if (s&3)
			Sys_Error ("s&3");
		for (i=0 ; i<s ; i+=4) {
			trans[i] = d_8to24table[data[i]];
			trans[i+1] = d_8to24table[data[i+1]];
			trans[i+2] = d_8to24table[data[i+2]];
			trans[i+3] = d_8to24table[data[i+3]];
		}
	}

	GL_Upload32 (gl_id, trans, width, height, mipmap, alpha);

	free(trans);
}

int GL_LoadTexture (char *identifier, int width, int height, byte *data, qboolean mipmap, qboolean alpha, int bytesperpixel, qboolean keep)
{
	int texture_index = Image_FindImage(identifier);
	if (texture_index > 0) {
		return texture_index;
	}

	int i;
	gltexture_t	*glt;

	texture_index = -1;
	for (i=0, glt=gltextures; i<MAX_GLTEXTURES; i++, glt++) {
		if (glt->used == false) {
			texture_index = i;
			break;
		}
	}

	if (numgltextures == MAX_GLTEXTURES) {
		Sys_Error("GL_LoadTexture: Out of GL textures\n");
	}

	if (texture_index < 0) {
		Sys_Error("Could not find a free GL texture!\n");
	}

    glt = &gltextures[texture_index];

	GLuint texID;
	glGenTextures(1, &texID);     // Ask GL for a real texture object

	snprintf(glt->identifier, sizeof(glt->identifier), identifier);
	glt->gl_id = texID;
	glt->texnum = texture_index;
	glt->width = width;
	glt->height = height;
	glt->original_width = width;
	glt->original_height = height;
	glt->bpp = bytesperpixel;
	glt->mipmap = mipmap;
	glt->keep = keep;
	glt->used = true;

	//Con_DPrintf("tex num %i tex name %s size %im\n", texture_index, identifier, (width*height*bytesperpixel)/1024);
	if (bytesperpixel == 1) {
		GL_Upload8 (glt->gl_id, data, width, height, mipmap, alpha);
	}
	else if (bytesperpixel == 4) {
		GL_Upload32 (glt->gl_id, (unsigned*)data, width, height, mipmap, alpha);
	}
	else {
		Sys_Error("GL_LoadTexture: unknown bytesperpixel\n");
	}
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	numgltextures++;

	return texture_index;
}

int GL_LoadLMTexture (char *identifier, int width, int height, byte *data, qboolean update)
{
	int i;
	gltexture_t	*glt;

	int texture_index = -1;

	if (update == false) {

		for (i=0, glt=gltextures; i<MAX_GLTEXTURES; i++, glt++) {
			if (glt->used == false) {
				texture_index = i;
				break;
			}
		}

		if (numgltextures == MAX_GLTEXTURES) {
			Sys_Error("GL_LoadTexture: Out of GL textures\n");
		}

		if (texture_index < 0) {
			Sys_Error("Could not find a free GL texture!\n");
		}

		glt = &gltextures[texture_index];

		GLuint texID;
		glGenTextures(1, &texID);

		//Con_DPrintf("lm num %i lm name %s\n", texture_index, identifier);

		strcpy(glt->identifier, identifier);
		glt->gl_id = texID;
		glt->texnum = texture_index;
		glt->width = width;
		glt->height = height;
		glt->original_width = width;
		glt->original_height = height;
		glt->bpp = 4;
		glt->mipmap = false;
		glt->keep = false;
		glt->used = true;

		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		GL_Upload32 (glt->gl_id, (unsigned*)data, width, height, false, false);

		numgltextures++;
	} else if (update == true) {
		texture_index = Image_FindImage(identifier);

		if (texture_index < 0) Sys_Error("tried to upload inactive lightmap\n");

		glt = &gltextures[texture_index];

		if (width == glt->original_width && height == glt->original_height) {
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			GL_Upload32 (glt->gl_id, (unsigned*)data, width, height, false, false);
		}
	}

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	return texture_index;
}

/****************************************/
float gVertexBuffer[VERTEXARRAYSIZE];
float gColorBuffer[VERTEXARRAYSIZE];
float gTexCoordBuffer[VERTEXARRAYSIZE];
