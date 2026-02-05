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

#include "../../nzportable_def.h"

// Temporary picture color mod, gets reset after Draw_TransPic is finished
#define PAL_STANDARD		0
#define PAL_WHITETORED		1
#define PAL_WHITETOYELLOW	2
void Draw_AdvancedPic (int x, int y, int pic, unsigned char alpha, unsigned char palette_hack);

extern int numcachepics;

typedef struct {
	vrect_t	rect;
	int		width;
	int		height;
	byte	*ptexbytes;
	int		rowbytes;
} rectdesc_t;

static rectdesc_t	r_rectdesc;

static int  charset;
byte		*draw_chars;				// 8*8 graphic characters
int			sniper_scope;

//Loading Fill by Crow_bar
float 	loading_cur_step;
char	loading_name[32];
float 	loading_num_step;
int 	loading_step;
float 	loading_cur_step_bk;

//=============================================================================
/* Support Routines */

int Image_FindImage(const char *identifier)
{
	cachepic_t	*pic;
	int			i;

	for (pic=cachepics, i=0 ; i<numcachepics ; pic++, i++) {
		if (!strcmp (identifier, pic->name)) {
			return i;
		}
	}

    return -1;
}

// you've never seen a hack this bad!
// this is basically a duplicate of Draw_CachePic
// but i was super lazy and didnt wanna go editing
// all references to it to add this extra parm.
// basically it switches over color_hack to decide
// whether or not to manipulate the color values
inline byte convert_white_to_red(byte color_index)
{
	// keep transparency
	if (color_index == 255)
		return color_index;

	// we have significantly less
	// reds than we do whites so
	// we have to lose some precision :(
	switch(color_index) {
		case 254: return 251;
		case 15: return 251;
		case 14: return 250;
		case 13: return 249;
		case 12: return 248;
		case 11: return 247;
		// precision loss begin
		case 10: return 228;
		case 9: return 228;
		case 8: return 227;
		case 7: return 227;
		case 6: return 226;
		case 5: return 226;
		case 4: return 225;
		case 3: return 225;
		// this is technically gray/black but oh well!
		case 2: return 224;
		case 1: return 224;
		case 0: return 224;
		// nzp's text shadow also likes to use 185 so..
		case 185: return 224;
	}

	return color_index;
}

inline byte convert_white_to_yellow(byte color_index)
{
	// keep transparency
	if (color_index == 255)
		return color_index;

	// quake has a nice full selection of yellows!
	// we could probably compute this, but a hashmao seems
	// faster.
	switch(color_index) {
		case 254: return 253;
		case 15: return 192;
		case 14: return 193;
		case 13: return 194;
		case 12: return 195;
		case 11: return 196;
		case 10: return 197;
		case 9: return 198;
		case 8: return 199;
		case 7: return 200;
		case 6: return 201;
		case 5: return 202;
		case 4: return 203;
		case 3: return 204;
		case 2: return 205;
		case 1: return 206;
		case 0: return 207;
		// nzp's text shadow also likes to use 185 so..
		case 185: return 204;
	}

	return color_index;
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

	// naievil -- Cannot use fopen because it is in a pak
    byte *data;
    data = COM_LoadTempFile("gfx/kerning_map.txt");
    if (!data) {
    	Con_Printf("WARNING: Failed to load font kerning file!");
        return;
    }

    for (int i = 0, j=0; i < 96; i++, j+=2)
    {
        font_kerningamount[i] = ((int)data[j]) - 48;
    }
}


/*
===============
Draw_Init
===============
*/
void Draw_Init (void)
{
	int		i;
	cachepic_t *holder;

	charset = Image_LoadImage ("gfx/charset", IMAGE_TGA, 1, qtrue, qfalse);
	holder = &cachepics[charset];
	draw_chars = holder->data;

	sniper_scope = Image_LoadImage ("gfx/hud/scope_256", IMAGE_TGA, 0, qfalse, qfalse);

	r_rectdesc.width = 64;
	r_rectdesc.height = 64;
	r_rectdesc.rowbytes = 64;
	r_rectdesc.ptexbytes = Q_malloc(r_rectdesc.width * r_rectdesc.height);
	memset(r_rectdesc.ptexbytes, 17, r_rectdesc.width * r_rectdesc.height);

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
	byte			*dest;
	byte			*source;
	unsigned short	*pusdest;
	int				drawline;	
	int				row, col;

	num &= 255;
	
	if (y <= -8)
		return;			// totally off screen

#ifdef PARANOID
	if (y > vid.height - 8 || x < 0 || x > vid.width - 8)
		Sys_Error ("Con_DrawCharacter: (%i, %i)", x, y);
	if (num < 0 || num > 255)
		Sys_Error ("Con_DrawCharacter: char %i", num);
#endif

	row = num>>4;
	col = num&15;
	source = draw_chars + (row<<10) + (col<<3);

	if (y < 0)
	{	// clipped
		drawline = 8 + y;
		source -= 128*y;
		y = 0;
	}
	else
		drawline = 8;


	if (r_pixbytes == 1)
	{
		dest = vid.conbuffer + y*vid.conrowbytes + x;
	
		while (drawline--)
		{
			if (source[0])
				dest[0] = source[0];
			if (source[1])
				dest[1] = source[1];
			if (source[2])
				dest[2] = source[2];
			if (source[3])
				dest[3] = source[3];
			if (source[4])
				dest[4] = source[4];
			if (source[5])
				dest[5] = source[5];
			if (source[6])
				dest[6] = source[6];
			if (source[7])
				dest[7] = source[7];
			source += 128;
			dest += vid.conrowbytes;
		}
	}
	else
	{
	// FIXME: pre-expand to native format?
		pusdest = (unsigned short *)
				((byte *)vid.conbuffer + y*vid.conrowbytes + (x<<1));

		while (drawline--)
		{
			if (source[0])
				pusdest[0] = d_8to16table[source[0]];
			if (source[1])
				pusdest[1] = d_8to16table[source[1]];
			if (source[2])
				pusdest[2] = d_8to16table[source[2]];
			if (source[3])
				pusdest[3] = d_8to16table[source[3]];
			if (source[4])
				pusdest[4] = d_8to16table[source[4]];
			if (source[5])
				pusdest[5] = d_8to16table[source[5]];
			if (source[6])
				pusdest[6] = d_8to16table[source[6]];
			if (source[7])
				pusdest[7] = d_8to16table[source[7]];

			source += 128;
			pusdest += (vid.conrowbytes >> 1);
		}
	}
}

/*
================
Draw_CharacterRGBA

This is the same as Draw_Character, but with RGBA color codes.
- Cypress
================
*/

/*
================
Draw_Character

Draws one 8*8 graphics character with 0 being transparent.
It can be clipped to the top of the screen to allow the console to be
smoothly scrolled off.
================
*/

void Draw_AdvancedCharacter(int x, int y, int num, int alpha, unsigned short color_hack) {
	byte			*dest;
	byte			*source;
	unsigned short	*pusdest;
	int				drawline;	
	int				row, col;
	int 			dither_factor;
	int 			pixel_tracker;

	if (alpha < 16)
		return;
	if (alpha < 32)
		dither_factor = 9;
	else if (alpha < 64)
		dither_factor = 6;
	else if (alpha < 128)
		dither_factor = 3;
	else
		dither_factor = 0;

	pixel_tracker = 0;

	num &= 255;
	
	if (y <= -8)
		return;			// totally off screen

#ifdef PARANOID
	if (y > vid.height - 8 || x < 0 || x > vid.width - 8)
		Sys_Error ("Con_DrawCharacter: (%i, %i)", x, y);
	if (num < 0 || num > 255)
		Sys_Error ("Con_DrawCharacter: char %i", num);
#endif

	row = num>>4;
	col = num&15;
	source = draw_chars + (row<<10) + (col<<3);

	if (y < 0)
	{	// clipped
		drawline = 8 + y;
		source -= 128*y;
		y = 0;
	}
	else
		drawline = 8;


	if (r_pixbytes == 1)
	{
		dest = vid.conbuffer + y*vid.conrowbytes + x;
	
		while (drawline--)
		{
			pixel_tracker++;

			// guard it to avoid spamming moduli
			if (dither_factor != 0) {
				// motolegacy -- this actually doesnt work as originally intended but it looks fucking awesome so im keeping it
				if (pixel_tracker % dither_factor != 0)
					continue;
			}

			// "Modern" (not 1996) GCC makes this almost as-fast
			for(int i = 0; i < 8; i++) {
				if (source[i]) {
					switch(color_hack) {
						case PAL_WHITETORED:
							dest[i] = convert_white_to_red(source[i]); 
							break;
						case PAL_WHITETOYELLOW:
							dest[i] = convert_white_to_yellow(source[i]); 
							break;
						default: 
							dest[i] = source[i]; 
							break;
					}
				}
					
			}

			source += 128;
			dest += vid.conrowbytes;
		}
	}
	else
	{
	// FIXME: pre-expand to native format?
		pusdest = (unsigned short *)
				((byte *)vid.conbuffer + y*vid.conrowbytes + (x<<1));

		while (drawline--)
		{
			pixel_tracker++;

			// guard it to avoid spamming moduli
			if (dither_factor != 0) {
				// motolegacy -- this actually doesnt work as originally intended but it looks fucking awesome so im keeping it
				if (pixel_tracker % dither_factor != 0)
					continue;
			}

			// "Modern" (not 1996) GCC makes this almost as-fast
			for(int i = 0; i < 8; i++) {
				if (source[i]) {
					switch(color_hack) {
						case PAL_WHITETORED:
							pusdest[i] = d_8to16table[convert_white_to_red(source[i])];
							break;
						case PAL_WHITETOYELLOW:
							pusdest[i] = d_8to16table[convert_white_to_yellow(source[i])];
							break;
						default: 
							pusdest[i] = d_8to16table[source[i]]; 
							break;
					}
				}		
			}

			source += 128;
			pusdest += (vid.conrowbytes >> 1);
		}
	}
}

unsigned char find_color_hack_from_rgb(byte r, byte g, byte b)
{
	// Check which color the caller wants the text to be
	// and if it is a certain range just set the color_hack to whatever
	// is close enough to that color
	unsigned char color_hack = PAL_STANDARD;
	int closest_color = findclosestpalmatch(r, g, b, 255);

	switch(closest_color) {
		case 64:
		case 65:
		case 66:
		case 67:
		case 68:
		case 69:
		case 70:
		case 71:
		case 72:
		case 73:
		case 74:
		case 75:
		case 76:
		case 77:
		case 78:
		case 79:
		case 224:
		case 225:
		case 226:
		case 227:
		case 228:
		case 229:
		case 230:
		case 231:
		case 232:
		case 247:
		case 248:
		case 249:
		case 250:
		case 251:
		    color_hack = PAL_WHITETORED;
			break;
		case 192:
		case 193:
		case 194:
		case 111:
		    color_hack = PAL_WHITETOYELLOW;
			break;
		case 254:
		case 15:
		default: 
			color_hack = PAL_STANDARD;
			break;
	}

	return color_hack;
}

extern cvar_t scr_coloredtext;
void Draw_CharacterRGBA(int x, int y, int num, float r, float g, float b, float a, float scale)
{
	unsigned char palette_hack = find_color_hack_from_rgb((byte) r, (byte) g, (byte) b);

	Draw_AdvancedCharacter(x, y, num, 255, palette_hack);
}

void Draw_ColoredString(int x, int y, char *str, float r, float g, float b, float a, float scale) 
{
	unsigned char palette_hack = find_color_hack_from_rgb((byte) r, (byte) g, (byte) b);

	while (*str)
	{
		Draw_AdvancedCharacter (x, y, *str, 255, palette_hack);

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

/*
================
Draw_String
================
*/
void Draw_String (int x, int y, char *str)
{
	while (*str)
	{
		Draw_Character (x, y, *str);

		// Hooray for variable-spacing!
		if (*str == ' ')
			x += 4;
        else if ((int)*str < 33 || (int)*str > 126)
            x += 8;
        else
            x += (font_kerningamount[(int)(*str - 33)] + 1);

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
Draw_DebugChar

Draws a single character directly to the upper right corner of the screen.
This is for debugging lockups by drawing different chars in different parts
of the code.
================
*/
void Draw_DebugChar (char num)
{
	byte			*dest;
	byte			*source;
	int				drawline;	
	extern byte		*draw_chars;
	int				row, col;

	if (!vid.direct)
		return;		// don't have direct FB access, so no debugchars...

	drawline = 8;

	row = num>>4;
	col = num&15;
	source = draw_chars + (row<<10) + (col<<3);

	dest = vid.direct + 312;

	while (drawline--)
	{
		dest[0] = source[0];
		dest[1] = source[1];
		dest[2] = source[2];
		dest[3] = source[3];
		dest[4] = source[4];
		dest[5] = source[5];
		dest[6] = source[6];
		dest[7] = source[7];
		source += 128;
		dest += 320;
	}
}


// naievil -- So we never seem to want to do this on NSPIRE because we 
// typically need transparency ALWAYS
void Draw_Pic (int x, int y, int pic)
{
	Draw_TransPic(x, y, pic);
}

/*
=============
Draw_StretchPic
=============
*/
void Draw_StretchPic (int x, int y, int pic, int x_value, int y_value)
{
	// naievil -- TODO: implement stretching?
	//Draw_Pic(x, y, pic);
}

/*
=============
Draw_ColoredStretchPic
=============
*/
void Draw_ColoredStretchPic (int x, int y, int pic, int x_value, int y_value, int r, int g, int b, int a)
{
	unsigned char palette_hack = find_color_hack_from_rgb((byte) r, (byte) g, (byte) b);

	// naievil -- TODO: implement stretching?
	Draw_AdvancedPic(x, y, pic, a, palette_hack);
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
Draw_ColorPic
=============
*/
void Draw_ColorPic (int x, int y, int pic, float r, float g , float b, float a)
{
	unsigned char palette_hack = find_color_hack_from_rgb((byte) r, (byte) g, (byte) b);

	Draw_AdvancedPic(x, y, pic, 255, palette_hack);
}

void Draw_MenuPanningPic (int x, int y, int pic, int x_value, int y_value, float time)
{
	//Draw_TransPic (x, y, pic);
}

void Draw_SubPic (int x, int y, int pic, float s, float t, float coord_size, float scale, float r, float g , float b, float a)
{
	//Draw_TransPic (x, y, pic);
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
	 
	cachepic_t *tex = &cachepics[pic];

	if (x < 0 || (unsigned)(x + tex->width) > vid.width || y < 0 ||
		 (unsigned)(y + tex->height) > vid.height)
	{
		Sys_Error ("Draw_TransPic: bad coordinates");
	}
		
	source = tex->data;

	if (r_pixbytes == 1)
	{
		dest = vid.buffer + y * vid.rowbytes + x;

		if (tex->width & 7)
		{	// general
			for (v=0 ; v<tex->height ; v++)
			{
				for (u=0 ; u<tex->width ; u++)
					if ( (tbyte=source[u]) != TRANSPARENT_COLOR) 
					{
						dest[u] = tbyte;
					}
	
				dest += vid.rowbytes;
				source += tex->width;
			}
		}
		else
		{	// unwound
			for (v=0 ; v<tex->height ; v++)
			{
				for (u=0 ; u<tex->width ; u+=8)
				{
					for (int i = 0; i < 8; i++)
					{
						if ( (tbyte=source[u+i]) != TRANSPARENT_COLOR)
						{
							dest[u+i] = tbyte;
						}
					}
				}
				dest += vid.rowbytes;
				source += tex->width;
			}
		}
	}
	else
	{
	// FIXME: pretranslate at load time?
		pusdest = (unsigned short *)vid.buffer + y * (vid.rowbytes >> 1) + x;

		for (v=0 ; v<tex->height ; v++)
		{
			for (u=0 ; u<tex->width ; u++)
			{
				tbyte = source[u];

				if (tbyte != TRANSPARENT_COLOR)
				{
					pusdest[u] = d_8to16table[tbyte];
				}
			}

			pusdest += vid.rowbytes >> 1;
			source += tex->width;
		}
	}
}

/*
=============
Draw_AdvancedPic
=============
*/
void Draw_AdvancedPic (int x, int y, int pic, unsigned char alpha, unsigned char palette_hack)
{
	byte	*dest, *source, tbyte;
	unsigned short	*pusdest;
	int				v, u;
	int 			dither_factor;
	int 			pixel_tracker;
	 
	cachepic_t *tex = &cachepics[pic];

	if (x < 0 || (unsigned)(x + tex->width) > vid.width || y < 0 ||
		 (unsigned)(y + tex->height) > vid.height)
	{
		Sys_Error ("Draw_AdvancedPic: bad coordinates");
	}

	if (alpha < 16)
		return;
	if (alpha < 32)
		dither_factor = 9;
	else if (alpha < 64)
		dither_factor = 6;
	else if (alpha < 128)
		dither_factor = 3;
	else
		dither_factor = 0;

	pixel_tracker = 0;
		
	source = tex->data;

	if (r_pixbytes == 1)
	{
		dest = vid.buffer + y * vid.rowbytes + x;

		if (tex->width & 7)
		{	// general
			for (v=0 ; v<tex->height ; v++)
			{
				for (u=0 ; u<tex->width ; u++)
				{
					pixel_tracker++;

					// guard it to avoid spamming moduli
					if (dither_factor != 0) {
						// motolegacy -- this actually doesnt work as originally intended but it looks fucking awesome so im keeping it
						if (pixel_tracker % dither_factor != 0)
							continue;
					}

					if ( (tbyte=source[u]) != TRANSPARENT_COLOR) {
						switch(palette_hack) {
							case PAL_WHITETORED:
								dest[u] = convert_white_to_red(tbyte); 
								break;
							case PAL_WHITETOYELLOW:
								dest[u] = convert_white_to_yellow(tbyte); 
								break;
							default: 
								dest[u] = tbyte;
								break;
						}
					}
				}
	
				dest += vid.rowbytes;
				source += tex->width;
			}
		}
		else
		{	// unwound
			for (v=0 ; v<tex->height ; v++)
			{
				for (u=0 ; u<tex->width ; u+=8)
				{

					pixel_tracker++;

					// guard it to avoid spamming moduli
					if (dither_factor != 0) {
						// motolegacy -- this actually doesnt work as originally intended but it looks fucking awesome so im keeping it
						if (pixel_tracker % dither_factor != 0)
							continue;
					}

					for (int i = 0; i < 8; i++)
					{
						if ( (tbyte=source[u+i]) != TRANSPARENT_COLOR)
						{
							switch(palette_hack) {
								case PAL_WHITETORED:
									dest[u+i] = convert_white_to_red(tbyte); 
									break;
								case PAL_WHITETOYELLOW:
									dest[u+i] = convert_white_to_yellow(tbyte); 
									break;
								default: 
									dest[u+i] = tbyte;
									break;
							}
						}
					}
				}
				dest += vid.rowbytes;
				source += tex->width;
			}
		}
	}
	else
	{
	// FIXME: pretranslate at load time?
		pusdest = (unsigned short *)vid.buffer + y * (vid.rowbytes >> 1) + x;

		for (v=0 ; v<tex->height ; v++)
		{
			for (u=0 ; u<tex->width ; u++)
			{

				pixel_tracker++;

				// guard it to avoid spamming moduli
				if (dither_factor != 0) {
					// motolegacy -- this actually doesnt work as originally intended but it looks fucking awesome so im keeping it
					if (pixel_tracker % dither_factor != 0)
						continue;
				}

				tbyte = source[u];

				if (tbyte != TRANSPARENT_COLOR)
				{
					switch(palette_hack) {
						case PAL_WHITETORED:
							pusdest[u] = d_8to16table[convert_white_to_red(tbyte)]; 
							break;
						case PAL_WHITETOYELLOW:
							pusdest[u] = d_8to16table[convert_white_to_yellow(tbyte)]; 
							break;
						default: 
							pusdest[u] = d_8to16table[tbyte];
							break;
					}
				}
			}

			pusdest += vid.rowbytes >> 1;
			source += tex->width;
		}
	}
}

/*
=============
Draw_TransPicTranslate
=============
*/
void Draw_TransPicTranslate (int x, int y, int pic, byte *translation)
{
	byte	*dest, *source, tbyte;
	unsigned short	*pusdest;
	int				v, u;
	 
	cachepic_t *tex = &cachepics[pic];

	if (x < 0 || (unsigned)(x + tex->width) > vid.width || y < 0 ||
		 (unsigned)(y + tex->height) > vid.height)
	{
		Sys_Error ("Draw_TransPic: bad coordinates");
	}
		
	source = tex->data;

	if (r_pixbytes == 1)
	{
		dest = vid.buffer + y * vid.rowbytes + x;

		if (tex->width & 7)
		{	// general
			for (v=0 ; v<tex->height ; v++)
			{
				for (u=0 ; u<tex->width ; u++)
					if ( (tbyte=source[u]) != TRANSPARENT_COLOR)
						dest[u] = translation[tbyte];

				dest += vid.rowbytes;
				source += tex->width;
			}
		}
		else
		{	// unwound
			for (v=0 ; v<tex->height ; v++)
			{
				for (u=0 ; u<tex->width ; u+=8)
				{
					if ( (tbyte=source[u]) != TRANSPARENT_COLOR)
						dest[u] = translation[tbyte];
					if ( (tbyte=source[u+1]) != TRANSPARENT_COLOR)
						dest[u+1] = translation[tbyte];
					if ( (tbyte=source[u+2]) != TRANSPARENT_COLOR)
						dest[u+2] = translation[tbyte];
					if ( (tbyte=source[u+3]) != TRANSPARENT_COLOR)
						dest[u+3] = translation[tbyte];
					if ( (tbyte=source[u+4]) != TRANSPARENT_COLOR)
						dest[u+4] = translation[tbyte];
					if ( (tbyte=source[u+5]) != TRANSPARENT_COLOR)
						dest[u+5] = translation[tbyte];
					if ( (tbyte=source[u+6]) != TRANSPARENT_COLOR)
						dest[u+6] = translation[tbyte];
					if ( (tbyte=source[u+7]) != TRANSPARENT_COLOR)
						dest[u+7] = translation[tbyte];
				}
				dest += vid.rowbytes;
				source += tex->width;
			}
		}
	}
	else
	{
	// FIXME: pretranslate at load time?
		pusdest = (unsigned short *)vid.buffer + y * (vid.rowbytes >> 1) + x;

		for (v=0 ; v<tex->height ; v++)
		{
			for (u=0 ; u<tex->width ; u++)
			{
				tbyte = source[u];

				if (tbyte != TRANSPARENT_COLOR)
				{
					pusdest[u] = d_8to16table[tbyte];
				}
			}

			pusdest += vid.rowbytes >> 1;
			source += tex->width;
		}
	}
}


void Draw_CharToConback (int num, byte *dest)
{
	int		row, col;
	byte	*source;
	int		drawline;
	int		x;

	row = num>>4;
	col = num&15;
	source = draw_chars + (row<<10) + (col<<3);

	drawline = 8;

	while (drawline--)
	{
		for (x=0 ; x<8 ; x++)
			if (source[x])
				dest[x] = 0x60 + source[x];
		source += 128;
		dest += 320;
	}

}

/*
================
Draw_BlackBackground

================
*/
void Draw_BlackBackground (void)
{
	Draw_Fill(0, 0, vid.width, vid.height, 0);
}


/*
================
Draw_ConsoleBackground

================
*/
void Draw_ConsoleBackground (int lines)
{
	Draw_BlackBackground();
}


/*
==============
R_DrawRect8
==============
*/
void R_DrawRect8 (vrect_t *prect, int rowbytes, byte *psrc,
	int transparent)
{
	byte	t;
	int		i, j, srcdelta, destdelta;
	byte	*pdest;

	pdest = vid.buffer + (prect->y * vid.rowbytes) + prect->x;

	srcdelta = rowbytes - prect->width;
	destdelta = vid.rowbytes - prect->width;

	if (transparent)
	{
		for (i=0 ; i<prect->height ; i++)
		{
			for (j=0 ; j<prect->width ; j++)
			{
				t = *psrc;
				if (t != TRANSPARENT_COLOR)
				{
					*pdest = t;
				}

				psrc++;
				pdest++;
			}

			psrc += srcdelta;
			pdest += destdelta;
		}
	}
	else
	{
		for (i=0 ; i<prect->height ; i++)
		{
			memcpy (pdest, psrc, prect->width);
			psrc += rowbytes;
			pdest += vid.rowbytes;
		}
	}
}


/*
==============
R_DrawRect16
==============
*/
void R_DrawRect16 (vrect_t *prect, int rowbytes, byte *psrc,
	int transparent)
{
	byte			t;
	int				i, j, srcdelta, destdelta;
	unsigned short	*pdest;

// FIXME: would it be better to pre-expand native-format versions?

	pdest = (unsigned short *)vid.buffer +
			(prect->y * (vid.rowbytes >> 1)) + prect->x;

	srcdelta = rowbytes - prect->width;
	destdelta = (vid.rowbytes >> 1) - prect->width;

	if (transparent)
	{
		for (i=0 ; i<prect->height ; i++)
		{
			for (j=0 ; j<prect->width ; j++)
			{
				t = *psrc;
				if (t != TRANSPARENT_COLOR)
				{
					*pdest = d_8to16table[t];
				}

				psrc++;
				pdest++;
			}

			psrc += srcdelta;
			pdest += destdelta;
		}
	}
	else
	{
		for (i=0 ; i<prect->height ; i++)
		{
			for (j=0 ; j<prect->width ; j++)
			{
				*pdest = d_8to16table[*psrc];
				psrc++;
				pdest++;
			}

			psrc += srcdelta;
			pdest += destdelta;
		}
	}
}


/*
=============
Draw_TileClear

This repeats a 64*64 tile graphic to fill the screen around a sized down
refresh window.
=============
*/
void Draw_TileClear (int x, int y, int w, int h)
{
	return; // naievil -- FIXME: remove this

	int				width, height, tileoffsetx, tileoffsety;
	byte			*psrc;
	vrect_t			vr;

	r_rectdesc.rect.x = x;
	r_rectdesc.rect.y = y;
	r_rectdesc.rect.width = w;
	r_rectdesc.rect.height = h;

	vr.y = r_rectdesc.rect.y;
	height = r_rectdesc.rect.height;

	tileoffsety = vr.y % r_rectdesc.height;

	while (height > 0)
	{
		vr.x = r_rectdesc.rect.x;
		width = r_rectdesc.rect.width;

		if (tileoffsety != 0)
			vr.height = r_rectdesc.height - tileoffsety;
		else
			vr.height = r_rectdesc.height;

		if (vr.height > height)
			vr.height = height;

		tileoffsetx = vr.x % r_rectdesc.width;

		while (width > 0)
		{
			if (tileoffsetx != 0)
				vr.width = r_rectdesc.width - tileoffsetx;
			else
				vr.width = r_rectdesc.width;

			if (vr.width > width)
				vr.width = width;

			psrc = r_rectdesc.ptexbytes +
					(tileoffsety * r_rectdesc.rowbytes) + tileoffsetx;

			if (r_pixbytes == 1)
			{
				R_DrawRect8 (&vr, r_rectdesc.rowbytes, psrc, 0);
			}
			else
			{
				R_DrawRect16 (&vr, r_rectdesc.rowbytes, psrc, 0);
			}

			vr.x += vr.width;
			width -= vr.width;
			tileoffsetx = 0;	// only the left tile can be left-clipped
		}

		vr.y += vr.height;
		height -= vr.height;
		tileoffsety = 0;		// only the top tile can be top-clipped
	}
}

/* sample a 24-bit RGB value to one of the colours on the existing 8-bit palette */
unsigned char convert_24_to_8(const unsigned char palette[768], const int rgb[3])
{
  int i, j;
  int best_index = -1;
  int best_dist = 0;

  for (i = 0; i < 256; i+=1)
  {
    int dist = 0;

    for (j = 0; j < 3; j++)
    {
    /* note that we could use RGB luminosity bias for greater accuracy, but quake's colormap apparently didn't do this */
      int d = abs(rgb[j] - palette[i*3+j]);
      dist += d * d;
    }

    if (best_index == -1 || dist < best_dist)
    {
      best_index = i;
      best_dist = dist;
    }
  }

  //Con_Printf("RGB: %d %d %d\tBest index: %d\n", rgb[0], rgb[1], rgb[2], best_index);

  return (unsigned char)best_index;
}

byte findclosestpalmatch(byte r, byte g, byte b, byte a)
{
	// naievil -- force alpha
	if (a == 0 || a < 128) {
		return 255;
	}

	int rgb[3];
	rgb[0] = r;
	rgb[1] = g; 
	rgb[2] = b;

	return (byte)convert_24_to_8(host_basepal, rgb);
}


/*
=============
Draw_Fill

Fills a box of pixels with a single color
=============
*/
void Draw_Fill (int x, int y, int w, int h, int c)
{
	byte			*dest;
	unsigned short	*pusdest;
	unsigned		uc;
	int				u, v;

	if (r_pixbytes == 1)
	{
		dest = vid.buffer + y*vid.rowbytes + x;
		for (v=0 ; v<h ; v++, dest += vid.rowbytes)
			for (u=0 ; u<w ; u++)
				dest[u] = c;
	}
	else
	{
		uc = d_8to16table[c];

		pusdest = (unsigned short *)vid.buffer + y * (vid.rowbytes >> 1) + x;
		for (v=0 ; v<h ; v++, pusdest += (vid.rowbytes >> 1))
			for (u=0 ; u<w ; u++)
				pusdest[u] = uc;
	}
}


void Draw_FillByColor (int x, int y, int w, int h, int r, int g, int b, int a)
{
	int c = (int)findclosestpalmatch(r, g, b, a);

	// naievil -- TODO: implement this
	Draw_Fill(x, y, w, h, c);
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
	cachepic_t *tex = &cachepics[hitmark];

	if (cl_crosshair_debug.value) {
		Draw_FillByColor(vid.width/2, 0, 1, 8, 255, 0, 0, 255);
		Draw_FillByColor(0, vid.height/2, 8, 1, 0, 255, 0, 255);
	}
	if (cl.stats[STAT_HEALTH] <= 20)
		return;

	if (cl.stats[STAT_ZOOM] == 2)
		Draw_Pic (0, 0, sniper_scope);

   	if (Hitmark_Time > sv.time)
        Draw_Pic ((vid.width - tex->width)/2,(vid.height - tex->height)/2, hitmark);

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
			crosshair_offset *= 0.80;
		} else if (sv_player->v.view_ofs[2] == -10) {
			crosshair_offset *= 0.65;
		}

		crosshair_offset_step += (crosshair_offset - crosshair_offset_step) * 0.5;

		x_value = (vid.width - 3)/2 - crosshair_offset_step;
		y_value = (vid.height - 1)/2;
		Draw_FillByColor(x_value, y_value, 3, 1, 255, (int)col, (int)col, (int)crosshair_opacity);

		x_value = (vid.width - 3)/2 + crosshair_offset_step;
		y_value = (vid.height - 1)/2;
		Draw_FillByColor(x_value, y_value, 3, 1, 255, (int)col, (int)col, (int)crosshair_opacity);

		x_value = (vid.width - 1)/2;
		y_value = (vid.height - 3)/2 - crosshair_offset_step;
		Draw_FillByColor(x_value, y_value, 1, 3, 255, (int)col, (int)col, (int)crosshair_opacity);

		x_value = (vid.width - 1)/2;
		y_value = (vid.height - 3)/2 + crosshair_offset_step;
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
		crosshair_offset_step += (crosshair_offset - crosshair_offset_step) * 0.5;

		x_value = (vid.width - 3)/2 - crosshair_offset_step;
		y_value = (vid.height - 1)/2;
		Draw_FillByColor(x_value, y_value, 3, 1, 255, 255, 255, 255);

		x_value = (vid.width - 3)/2 + crosshair_offset_step;
		y_value = (vid.height - 1)/2;
		Draw_FillByColor(x_value, y_value, 3, 1, 255, 255, 255, 255);

		x_value = (vid.width - 1)/2;
		y_value = (vid.height - 3)/2 - crosshair_offset_step;
		Draw_FillByColor(x_value, y_value, 1, 3, 255, 255, 255, 255);

		x_value = (vid.width - 1)/2;
		y_value = (vid.height - 3)/2 + crosshair_offset_step;
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
	unsigned			x,y,t;
	byte		*pbuf;

	VID_UnlockBuffer ();
	S_ExtraUpdate ();
	VID_LockBuffer ();

	for (y=0 ; y<vid.height ; y++)
	{

		pbuf = (byte *)(vid.buffer + vid.rowbytes*y);
		t = (y & 1) << 1;

		for (x=0 ; x<vid.width ; x++)
		{
			if ((x & 3) != t)
				pbuf[x] = 0;
		}
	}

	VID_UnlockBuffer ();
	S_ExtraUpdate ();
	VID_LockBuffer ();
}

//=============================================================================
