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
extern "C"{}
extern "C"{
#include <vitasdk.h>
#include "../../../nzportable_def.h"
#include "../../../images.h"

extern unsigned short CRC_Block(const unsigned char *data, size_t size);
}

#define GL_COLOR_INDEX8_EXT     0x80E5

extern unsigned char d_15to8table[65536];

int			cs_texture;

static byte cs_data[64]  = {
	0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff,
	0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};

cvar_t		gl_nobind = {"gl_nobind", "0"};
cvar_t		gl_max_size = {"gl_max_size", "1024"};
cvar_t		gl_picmip = {"gl_picmip", "0"};
cvar_t 		gl_bilinear = {"gl_bilinear", "1"};
cvar_t 		gl_compress = {"gl_compress", "0"};

extern cvar_t cl_crossx;
extern cvar_t cl_crossy;
extern cvar_t crosshair;
extern cvar_t gl_mipmap;
extern cvar_t scr_crosshairscale;
extern cvar_t scr_conalpha;
extern cvar_t scr_menuscale;
extern cvar_t scr_sbarscale;

extern int tex_cache;

byte		*draw_chars;				// 8*8 graphic characters
qpic_t		*draw_disc;
qpic_t		*draw_backtile;

int			translate_texture;
int			char_texture;
int			sniper_scope;
int 		currentcanvas = -1;

typedef struct
{
	int		texnum;
	float	sl, tl, sh, th;
} glpic_t;

byte		conback_buffer[sizeof(qpic_t) + sizeof(glpic_t)];
qpic_t	*conback = (qpic_t *)&conback_buffer;

int		gl_lightmap_format = GL_RGBA;
int		gl_solid_format = 3;
int		gl_alpha_format = 4;

int		gl_filter_min = GL_NEAREST;
int		gl_filter_max = GL_NEAREST;

//Loading Fill by Crow_bar
float 	loading_cur_step;
char	loading_name[32];
float 	loading_num_step;
int 	loading_step;
float 	loading_cur_step_bk;

gltexture_t	gltextures[MAX_GLTEXTURES];
static int numgltextures = 0;
static GLuint current_gl_id = 0;

void GL_Bind (int texnum)
{
	if (texnum < 0) {
        Con_DPrintf("GL_Bind: invalid texnum %d\n", texnum);
        return;
    }
    gltexture_t *glt = &gltextures[texnum];
    if (!glt->used) {
        Con_DPrintf("GL_Bind: unused texnum %d\n", texnum);
        return;
    }

    if (current_gl_id != glt->gl_id) {
        glBindTexture(GL_TEXTURE_2D, glt->gl_id);
        current_gl_id = glt->gl_id;
    }
}

void GL_FreeTextures (int texnum)
{
	if (texnum <= 0) return;

	gltexture_t *glt = &gltextures[texnum];
	if (glt->used == false) return;
	if (glt->keep) return;

	glDeleteTextures(1, &glt->gl_id);
	glt->gl_id = 0;
	glt->texnum = texnum;
	strcpy(glt->identifier, "");
	glt->width = 0;
	glt->height = 0;
	glt->original_width = 0;
	glt->original_height = 0;
	glt->bpp = 0;
	glt->keep = false;
	glt->used = false;
	memset(glt, 0, sizeof(*glt));   // clear slot
	numgltextures--;
}

void GL_UnloadTextures (void)
{
	for (int i = 0; i < (MAX_GLTEXTURES); i++) {
		GL_FreeTextures(i);
	}
}

/*
================
GL_FindTexture
================
*/
int GL_FindTexture (const char *identifier)
{
	int		i;
	gltexture_t	*glt;

	for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++) {
		if (glt->used) {
			if (!strcmp (identifier, glt->identifier)) {
				return glt->texnum;
			}
		}
	}

	return -1;
}

typedef struct
{
	const char *name;
	int	minimize, maximize;
} glmode_t;

glmode_t modes[] = {
	{"GL_NEAREST", GL_NEAREST, GL_NEAREST},
	{"GL_LINEAR", GL_LINEAR, GL_LINEAR},
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
		if (!strcasecmp ((char*)modes[i].name, Cmd_Argv(1) ) )
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
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
		}
	}
}

/*
===============
Callback_Bilinear_f
This callback is used to set at launch the texture mode.
===============
*/
static void Callback_Bilinear_f(cvar_t *var)
{
	if (gl_bilinear.value)
		Cbuf_AddText("gl_texturemode GL_LINEAR\n");
	else
		Cbuf_AddText("gl_texturemode GL_NEAREST\n");
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
	int		i;
	qpic_t	*cb;
	byte	*dest, *src;
	int		x, y;
	char	ver[40];
	glpic_t	*gl;
	int		start;
	byte	*ncdata;
	int		f, fstep, maxsize;

	Cvar_RegisterVariable (&gl_nobind);
	Cvar_RegisterVariable (&gl_max_size);
	Cvar_RegisterVariable (&gl_picmip);
	Cvar_RegisterVariable (&gl_bilinear);
	Cvar_RegisterVariable (&st_separation );
	Cvar_RegisterVariable (&st_zeropdist );
	Cvar_RegisterVariable (&st_fustbal );

	Cvar_SetCallback (&gl_bilinear, &Callback_Bilinear_f);

	// texture_max_size
	if ((i = COM_CheckParm("-maxsize")) != 0) {
		maxsize = atoi(com_argv[i+1]);
		maxsize &= 0xff80;
		Cvar_SetValue("gl_max_size", maxsize);
	} 

	Cmd_AddCommand ("gl_texturemode", &Draw_TextureMode_f);

	// now turn them into textures
	char_texture = Image_LoadImage ("gfx/charset", IMAGE_TGA, 0, true, false);
	if (char_texture < 0)// did not find a matching TGA...
		Sys_Error ("Could not load charset, make sure you have every folder and file installed properly\nDouble check that all of your files are in their correct places\nAnd that you have installed the game properly.\n");

	sniper_scope = Image_LoadImage ("gfx/hud/scope", IMAGE_TGA, 0, true, false);

	Clear_LoadingFill ();

	InitKerningMap();
}

void DrawQuad_NoTex(float x, float y, float w, float h, float r, float g, float b, float a)
{
	gVertexBuffer[0] = gVertexBuffer[9] = x;
	gVertexBuffer[1] = gVertexBuffer[4] = y;
	gVertexBuffer[3] = gVertexBuffer[6] = x+w;
	gVertexBuffer[7] = gVertexBuffer[10] = y+h;
	gVertexBuffer[2] = gVertexBuffer[5] = gVertexBuffer[8] = gVertexBuffer[11] = 0.5f;
	
	float color[4] = {r,g,b,a};
	GL_DisableState(GL_TEXTURE_COORD_ARRAY);
	vglVertexAttribPointerMapped(0, gVertexBuffer);
	gVertexBuffer += 12;
	glUniform4fv(monocolor, 1, color);
	GL_DrawPolygon(GL_TRIANGLE_FAN, 4);
	GL_EnableState(GL_TEXTURE_COORD_ARRAY);
}

void DrawQuad(float x, float y, float w, float h, float u, float v, float uw, float vh)
{
	gTexCoordBuffer[0] = gTexCoordBuffer[6] = u;
	gTexCoordBuffer[1] = gTexCoordBuffer[3] = v;
	gTexCoordBuffer[2] = gTexCoordBuffer[4] = u+uw;
	gTexCoordBuffer[5] = gTexCoordBuffer[7] = v+vh;

	gVertexBuffer[0] = gVertexBuffer[9] = x;
	gVertexBuffer[1] = gVertexBuffer[4] = y;
	gVertexBuffer[3] = gVertexBuffer[6] = x+w;
	gVertexBuffer[7] = gVertexBuffer[10] = y+h;
	gVertexBuffer[2] = gVertexBuffer[5] = gVertexBuffer[8] = gVertexBuffer[11] = 0.5f;
		
	vglVertexAttribPointerMapped(0, gVertexBuffer);
	vglVertexAttribPointerMapped(1, gTexCoordBuffer);
	gVertexBuffer += 12;
	gTexCoordBuffer += 8;
	GL_DrawPolygon(GL_TRIANGLE_FAN, 4);
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

	if (num == 0x20)
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

	DrawQuad(x, y, 8, 8, fcol, frow, size, size);
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

	frow = row*0.0625f;
	fcol = col*0.0625f;
	size = 0.0625f;

	GL_Bind (char_texture);

	glEnable(GL_BLEND);
	Platform_Graphics_Color(r/255, g/255, b/255, a/255);
	glDisable (GL_ALPHA_TEST);
	GL_EnableState(GL_MODULATE);
	DrawQuad(x, y, 8*scale, 8*scale, fcol, frow, size, size);
	Platform_Graphics_Color(1,1,1,1);
	GL_EnableState(GL_REPLACE);
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
	Draw_ColoredString(x, x, str, 255, 255, 255, 255, 2); 
}

void Draw_ColoredString(int x, int y, char *str, float r, float g, float b, float a, float scale) 
{
	while (*str)
	{
		Draw_CharacterRGBA (x, y, *str, r, g, b, a, scale);

		// Hooray for variable-spacing!
		if (*str == ' ')
			x += 4 * (int)scale;
		else if ((int)*str < 33 || (int)*str > 126)
            x += 8 * (int)scale;
		else
            x += (font_kerningamount[(int)(*str - 33)] + 1) * scale;
		
		str++;
	}
}

int getTextWidth(char *str, float scale)
{
	int width = 0;

    for (size_t i = 0; i < strlen(str); i++) {
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
Batch_Character
================
*/
int batched_vertices = 0;
float *batched_vbuffer = NULL;
float *batched_tbuffer = NULL;
int is_batching = 0;
void Batch_Character (int x, int y, int num) {
	if (!is_batching) {
		is_batching = 1;
		batched_vbuffer = gVertexBuffer;
		batched_tbuffer = gTexCoordBuffer;
		batched_vertices = 0;
	}

	int				row, col;
	float			frow, fcol, size;

	if (num == 0x20)
		return;		// space

	num &= 255;
	
	if (y <= -8)
		return;			// totally off screen

	row = num>>4;
	col = num&15;

	frow = row*0.0625;
	fcol = col*0.0625;
	size = 0.0625;
	
	gVertexBuffer[0] = gVertexBuffer[3] = gVertexBuffer[9] = x;
	gVertexBuffer[1] = gVertexBuffer[10] = gVertexBuffer[13] = y;
	gVertexBuffer[4] = gVertexBuffer[7] = gVertexBuffer[16] = y + 8;
	gVertexBuffer[6] = gVertexBuffer[12] = gVertexBuffer[15] = x + 8;
	gVertexBuffer[2] = gVertexBuffer[5] = gVertexBuffer[8] = gVertexBuffer[11] = gVertexBuffer[14] = gVertexBuffer[17] = 0.5f;
				
	gTexCoordBuffer[0] = gTexCoordBuffer[2] = gTexCoordBuffer[6] = fcol;
	gTexCoordBuffer[1] = gTexCoordBuffer[7] = gTexCoordBuffer[9] = frow;
	gTexCoordBuffer[3] = gTexCoordBuffer[5] = gTexCoordBuffer[11] = frow+size;
	gTexCoordBuffer[4] = gTexCoordBuffer[8] = gTexCoordBuffer[10] = fcol+size;
		
	gVertexBuffer += 18;
	gTexCoordBuffer += 12;
	batched_vertices += 6;
}

/*
================
Batch_String
================
*/
void Batch_String (int x, int y, const char *str, int delta) {
	if (!is_batching) {
		is_batching = 1;
		batched_vbuffer = gVertexBuffer;
		batched_tbuffer = gTexCoordBuffer;
		batched_vertices = 0;
	}
	
	while (*str)
	{
		int num = (*str) + delta;
		int	row, col;
		float frow, fcol, size;

		if (num != 0x20) {
			num &= 255;
			if (y > -8) {
				row = num>>4;
				col = num&15;

				frow = row*0.0625;
				fcol = col*0.0625;
				size = 0.0625;
				
				gVertexBuffer[0] = gVertexBuffer[3] = gVertexBuffer[9] = x;
				gVertexBuffer[1] = gVertexBuffer[10] = gVertexBuffer[13] = y;
				gVertexBuffer[4] = gVertexBuffer[7] = gVertexBuffer[16] = y + 8;
				gVertexBuffer[6] = gVertexBuffer[12] = gVertexBuffer[15] = x + 8;
				gVertexBuffer[2] = gVertexBuffer[5] = gVertexBuffer[8] = gVertexBuffer[11] = gVertexBuffer[14] = gVertexBuffer[17] = 0.5f;
				
				gTexCoordBuffer[0] = gTexCoordBuffer[2] = gTexCoordBuffer[6] = fcol;
				gTexCoordBuffer[1] = gTexCoordBuffer[7] = gTexCoordBuffer[9] = frow;
				gTexCoordBuffer[3] = gTexCoordBuffer[5] = gTexCoordBuffer[11] = frow+size;
				gTexCoordBuffer[4] = gTexCoordBuffer[8] = gTexCoordBuffer[10] = fcol+size;
		
				gVertexBuffer += 18;
				gTexCoordBuffer += 12;
				batched_vertices += 6;
			}
		}

		str++;
		x += 8;
	}
}

/*
================
Draw_Batched
================
*/
void Draw_Batched() {
	if (batched_vertices > 0) {
		GL_Bind (char_texture);
		vglVertexAttribPointerMapped(0, batched_vbuffer);
		vglVertexAttribPointerMapped(1, batched_tbuffer);
		GL_DrawPolygon(GL_TRIANGLES, batched_vertices);
	}
	is_batching = 0;
}

/*
================
Draw_DebugChar

Draws a single character directly to the upper right corner of the screen.
This is for debugging lockups by drawing different chars in different parts
of the code.
================
*/
void Draw_DebugChar (signed char num)
{
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
	Platform_Graphics_Color(r/255.0,g/255.0,b/255.0,a/255.0);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_EnableState(GL_MODULATE);

	GL_Bind (pic);
	DrawQuad(x, y, x_value, y_value, 0, 0, 1, 1);

	Platform_Graphics_Color(1,1,1,1);
}

/*
=============
Draw_StretchPic
=============
*/
void Draw_StretchPic (int x, int y, int pic, int x_value, int y_value)
{
	glEnable(GL_ALPHA_TEST);
	Platform_Graphics_Color(1,1,1,1);

	GL_Bind (pic);
	DrawQuad(x, y, x_value, y_value, 0, 0, 1, 1);

	Platform_Graphics_Color(1,1,1,1);
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
	Platform_Graphics_Color(r/255.0f,g/255.0f,b/255.0f,a/255.0f);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_EnableState(GL_MODULATE);

	GL_Bind (pic);
	DrawQuad(x, y, gltextures[pic].width, gltextures[pic].height, 0, 0, 1, 1);

	glDisable(GL_BLEND);
	//glDisable(GL_ALPHA_TEST);
	Platform_Graphics_Color(r/255.0f,g/255.0f,b/255.0f,a/255.0f);
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
Draw_AlphaPic
=============
*/
void Draw_AlphaPic (int x, int y, int pic, float alpha)
{
	Draw_ColorPic(x, y, pic, 255, 255, 255, alpha);
}

/*
=============
Draw_TransPic
=============
*/
void Draw_TransPic (int x, int y, int pic)
{
	byte	*dest, *source, tbyte;
	unsigned short	*pusdest;
	int				v, u;

	if (x < 0 || (unsigned)(x + gltextures[pic].width) > vid.width || y < 0 ||
		 (unsigned)(y + gltextures[pic].height) > vid.height)
	{
		Sys_Error ("Draw_TransPic: bad coordinates");
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
=============
Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
typedef union ByteToInt_t {
    byte b[4];
    int i;
} ByteToInt;

void Draw_TileClear (int x, int y, int w, int h)
{
	Platform_Graphics_Color(1,1,1,1);
	ByteToInt b;
	memcpy(b.b, draw_backtile->data, sizeof(b.b));
	GL_Bind (b.i);
	DrawQuad(x, y, w, h, x/64.0, y/64.0, w/64.0, h/64.0);
}


/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void Draw_Fill (int x, int y, int w, int h, int c)
{
	DrawQuad_NoTex(x, y, w, h, host_basepal[c*3]/255.0, host_basepal[c*3+1]/255.0, host_basepal[c*3+2]/255.0, 1);
	Platform_Graphics_Color(1,1,1,1);
}

void Draw_FillByColor (int x, int y, int w, int h, int r, int g, int b, int a)
{
	DrawQuad_NoTex(x, y, w, h, (float)r/255, (float)g/255, (float)b/255, (float)a/255);
	Platform_Graphics_Color(1,1,1,1);
}
//=============================================================================

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
    int y          	= vid.height - (size/ 2) - 25;
	char* text;


	if(loading_cur_step > loading_num_step)
	      loading_cur_step = loading_num_step;

	if (loading_cur_step < loading_cur_step_bk)
		loading_cur_step = loading_cur_step_bk;

	if (loading_cur_step == loading_num_step && loading_cur_step_bk != loading_num_step)
		loading_cur_step = loading_cur_step_bk;

    float loadsize = loading_cur_step * (max_step / loading_num_step);
	Draw_FillByColor (x - 2, y - 2, max_step + 8, size + 8, 69, 69, 69, 255);
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
================
Draw_FadeScreen

================
*/
void Draw_FadeScreen (void)
{

	glEnable (GL_BLEND);
	DrawQuad_NoTex(0, 0, vid.width, vid.height, 0, 0, 0, 0.8f);
	Platform_Graphics_Color(1,1,1,1);
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
	currentcanvas = -1;
	glViewport (glx, gly, glwidth, glheight);

	glMatrixMode(GL_PROJECTION);
    glLoadIdentity ();
	glOrtho  (0, vid.width, vid.height, 0, -99999, 99999);

	glMatrixMode(GL_MODELVIEW);
    glLoadIdentity ();

	glDisable (GL_DEPTH_TEST);
	glDisable (GL_CULL_FACE);
	glDisable (GL_BLEND);
	//GL_EnableState(GL_ALPHA_TEST);


	Platform_Graphics_Color (1,1,1,1);
}

//====================================================================

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
void GL_Upload32 (GLuint gl_id, unsigned *data, int width, int height,  qboolean mipmap, qboolean alpha)
{
	int			scaled_width, scaled_height;

	for (scaled_width = 1 ; scaled_width < width ; scaled_width<<=1)
		;
	for (scaled_height = 1 ; scaled_height < height ; scaled_height<<=1)
		;

	scaled_width >>= (int)gl_picmip.value;
	scaled_height >>= (int)gl_picmip.value;

	if (scaled_width > gl_max_size.value)
		scaled_width = gl_max_size.value;
	if (scaled_height > gl_max_size.value)
		scaled_height = gl_max_size.value;

	unsigned *scaled = (unsigned*)malloc(scaled_width * scaled_height * sizeof(unsigned));
	if (!scaled) Sys_Error("GL_Upload32: out of memory");

	// bind the texture to make sure we're uploading to the right one
    glBindTexture(GL_TEXTURE_2D, gl_id);
	current_gl_id = gl_id;

	if (scaled_width == width && scaled_height == height)
	{
		if (!mipmap)
		{
			glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
			goto done;
		}
		memcpy (scaled, data, width*height*4);
	} else {
		GL_ResampleTexture (data, width, height, scaled, scaled_width, scaled_height);
	}

	glTexImage2D (GL_TEXTURE_2D, 0, GL_RGBA, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
	
	if (mipmap) {
		glGenerateMipmap(GL_TEXTURE_2D);
	}
		
done: ;

	if (mipmap) {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	} else {
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}

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
	unsigned *trans = (unsigned*)malloc(width*height*sizeof(unsigned));

	s = width*height;
	// if there are no transparent pixels, make it a 3 component
	// texture even if it was specified as otherwise
	if (alpha)
	{
		noalpha = true;
		for (i=0 ; i<s ; i++)
		{
			p = data[i];
			if (p == 255)
				noalpha = false;
			trans[i] = d_8to24table[p];
		}

		if (alpha && noalpha)
			alpha = false;
	}
	else
	{
		if (s&3)
			Sys_Error ("s&3");
		for (i=0 ; i<s ; i+=4)
		{
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
	int texture_index = GL_FindTexture(identifier);
	if (texture_index >= 0) {
		return texture_index;
	}

	int i;
	gltexture_t	*glt;

	// Out of textures?
	if (numgltextures == MAX_GLTEXTURES)
		Sys_Error ("GL_GetTextureIndex: Out of GL textures\n");

	texture_index = -1;
	for (i=0, glt=gltextures; i < MAX_GLTEXTURES; i++, glt++) {
		if (glt->used == false) {
			texture_index = i;
			break;
		}
	}

    glt = &gltextures[texture_index];

	if (texture_index < 0) {
		Sys_Error("Could not find a free GL texture!\n");
	}

	GLuint texID;
	glGenTextures(1, &texID);     // Ask GL for a real texture object

	strcpy(glt->identifier, identifier);
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

	GL_EnableState(GL_MODULATE);

	return texture_index;
}

int GL_LoadLMTexture (char *identifier, int width, int height, byte *data, qboolean update)
{
	int i;
	gltexture_t	*glt;

	int texture_index = -1;

	if (update == false) {
		// Out of textures?
		if (numgltextures == MAX_GLTEXTURES)
			Sys_Error ("GL_GetTextureIndex: Out of GL textures\n");

		for (i=0, glt=gltextures; i < MAX_GLTEXTURES; i++, glt++) {
			if (glt->used == false) {
				texture_index = i;
				break;
			}
		}

		glt = &gltextures[texture_index];

		if (texture_index < 0)
			Sys_Error("Could not find a free GL texture!\n");

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

		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

		GL_Upload32 (glt->gl_id, (unsigned*)data, width, height, false, false);
	} else if (update == true) {
		texture_index = GL_FindTexture(identifier);

		if (texture_index < 0) Sys_Error("tried to upload inactive lightmap\n");

		glt = &gltextures[texture_index];

		if (width == glt->original_width && height == glt->original_height) {
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

			GL_Upload32 (glt->gl_id, (unsigned*)data, width, height, false, false);
		}
	}

	GL_EnableState(GL_MODULATE);

	return texture_index;
}

/****************************************/

static GLenum oldtarget = 0; // KH

// Benchmark
int max_fps = 0;
int average_fps = 0; // TODO: Add this
int min_fps = 999;
bool bBlinkBenchmark;

void GL_DrawFPS(void){
	extern cvar_t show_fps;
	static double lastframetime;
	double t;
	extern int fps_count;
	static int lastfps;
	int x;
	char st[80];
	
	if (!show_fps.value)
		return;

	t = Sys_FloatTime ();

	if ((t - lastframetime) >= 1.0) {
		lastfps = fps_count;
		fps_count = 0;
		lastframetime = t;

	}
	sprintf(st, "%3d FPS", lastfps);

	x = 329 - strlen(st) * 8 - 16;

	Draw_String(x, 2, st);
}

