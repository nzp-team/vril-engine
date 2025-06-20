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

// draw.h -- these are the only functions outside the refresh allowed
// to touch the vid buffer

void Draw_Init (void);
void Draw_Character (int x, int y, int num);
void Draw_CharacterRGBA (int x, int y, int num, float r, float g, float b, float a, float scale);
void Draw_DebugChar (char num);
void Draw_Pic (int x, int y, qpic_t *pic);
#ifdef __PSP__
void Draw_PicIndex (int x, int y, int width, int height, int texture_index);
#endif // __PSP__
void Draw_StretchPic (int x, int y, qpic_t *pic, int x_value, int y_value);
void Draw_ColorPic (int x, int y, qpic_t *pic, float r, float g , float b, float a);
void Draw_ColoredStretchPic (int x, int y, qpic_t *pic, int x_value, int y_value, int r, int g, int b, int a);
void Draw_ColoredString (int x, int y, char *text, float r, float g, float b, float a, float scale);
void Draw_ColoredStringCentered(int y, char *text, float r, float g, float b, float a, float scale);
void Draw_TransPic (int x, int y, qpic_t *pic);
void Draw_TransPicTranslate (int x, int y, qpic_t *pic, byte *translation);
void Draw_ConsoleBackground (int lines);
void Draw_AlphaPic (int x, int y, qpic_t *pic, float alpha);
#ifndef __3DS__
void Draw_Fill (int x, int y, int w, int h, int c);
#endif
void Draw_LoadingFill(void);
void Draw_FillByColor (int x, int y, int w, int h, int r, int g, int b, int a);
void Draw_FadeScreen (void);
void Draw_String (int x, int y, char *str);
void Draw_TileClear (int x, int y, int w, int h);
int getTextWidth(char *str, float scale);

byte findclosestpalmatch(byte r, byte g, byte b, byte a);

//other
void Clear_LoadingFill (void);
#ifdef __PSP__
byte *StringToRGB (char *s);
#endif // __PSP__

extern float loading_cur_step;
extern int loading_step;
extern char loading_name[32];
extern float loading_num_step;
extern int font_kerningamount[96];

qpic_t *Draw_CachePic (char *path);
#ifdef __PSP__
qpic_t *Draw_CacheImg (char *path);
#elif __WII__
qpic_t *Draw_LMP (char *path);
#endif

#ifdef __NSPIRE__
void Draw_BlackBackground (void);
// naievil -- texture conversion start 
#define MAX_SINGLE_PLANE_PIXEL_SIZE 		1048576		// naievil -- 1024 x 1024 single plane (paletted) texture
extern byte converted_pixels[MAX_SINGLE_PLANE_PIXEL_SIZE]; 
extern byte temp_pixel_storage_pixels[MAX_SINGLE_PLANE_PIXEL_SIZE*4]; // naievil -- rgba storage for max pic size 
// naievil -- texture conversion end
#endif
