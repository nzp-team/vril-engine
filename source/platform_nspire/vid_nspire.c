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
// vid_null.c -- null video driver to aid porting efforts

#ifndef WIN32
#include "os.h"
#endif

#include "../nzportable_def.h"
#include "d_local.h"

viddef_t	vid;				// global video state

#define	BASEWIDTH	320
#define	BASEHEIGHT	240

byte	vid_buffer[BASEWIDTH*BASEHEIGHT];
short	zbuffer[BASEWIDTH*BASEHEIGHT];
byte	surfcache[SURFCACHE_SIZE_AT_320X200];

unsigned short	d_8to16table[256];
unsigned	d_8to24table[256];


unsigned char rgui8_palette[ 256 ][ 3 ];
unsigned short rgui16_palette[ 256 ];

byte align256_colormap[ 0x4000 + 255 ];


void VID_UpdatePalette( )
{
	int i;
	for( i = 0; i < 256; i++ )
	{
		rgui16_palette[ i ] = ( rgui8_palette[ i ][ 2 ] >> 3 ) | ( ( rgui8_palette[ i ][ 1 ] >> 2 ) << 5 ) | ( ( rgui8_palette[ i ][ 0 ] >> 3 ) << 11 );
	}
}

void	VID_SetPalette (unsigned char *palette)
{
	memcpy( rgui8_palette, palette, 256 * 3 * sizeof( unsigned char ) );
	VID_UpdatePalette();
}

void	VID_ShiftPalette (unsigned char *palette)
{
	memcpy( rgui8_palette, palette, 256 * 3 * sizeof( unsigned char ) );
	VID_UpdatePalette();
}

void	VID_Init (unsigned char *palette)
{
	byte *aligned_colormap;
	vid.maxwarpwidth = vid.width = vid.conwidth = BASEWIDTH;
	vid.maxwarpheight = vid.height = vid.conheight = BASEHEIGHT;
	vid.aspect = 1.0;
	vid.numpages = 1;

	/* man the colormap has just been loaded and then we copy it over and let it sit there unused while quake runs.. wasted 16k, sad.. */
	aligned_colormap = ( byte * )( ( ( ( size_t )&align256_colormap[ 0 ] ) + 255 ) & ~0xff );
	memcpy( aligned_colormap, host_colormap, 0x4000 );
	vid.colormap = aligned_colormap;
	vid.fullbright = 256 - LittleLong (*((int *)vid.colormap + 2048));
	vid.buffer = vid.conbuffer = vid_buffer;
	vid.rowbytes = vid.conrowbytes = BASEWIDTH;
	
	d_pzbuffer = zbuffer;
	D_InitCaches (surfcache, sizeof(surfcache));

	memcpy( rgui8_palette, palette, 256 * 3 * sizeof( unsigned char ) );
	VID_UpdatePalette();
}

void	VID_Shutdown (void)
{
}

void	VID_Update (vrect_t *rects)
{
	int i_x, i_y, i_width, i_line_left;
	unsigned short *ptr = (*(void**)0xC0000010);

	i_width = SCREEN_WIDTH;
	for( i_y = rects->y; i_y < rects->y+rects->height; i_y++ )
	{
		if( rects->x == 0 )
		{
			unsigned char *pui8_line;
			unsigned int *pui_dst, ui_pelpair;
			unsigned short *pui16_rem;
			int i_pel, i_pel2;

			i_line_left = rects->width;
			pui8_line = &vid_buffer[ i_y * BASEWIDTH ];
			pui_dst = (unsigned int *)&ptr[ i_y * i_width ];
			while( i_line_left >= 16 )
			{
				i_pel = pui8_line[ 0 ];
				i_pel2 = pui8_line[ 1 ];
				ui_pelpair = rgui16_palette[ i_pel ] | ( rgui16_palette[ i_pel2 ] << 16 );
				pui_dst[ 0 ] = ui_pelpair;
				i_pel = pui8_line[ 2 ];
				i_pel2 = pui8_line[ 3 ];
				ui_pelpair = rgui16_palette[ i_pel ] | ( rgui16_palette[ i_pel2 ] << 16 );
				pui_dst[ 1 ] = ui_pelpair;
				i_pel = pui8_line[ 4 ];
				i_pel2 = pui8_line[ 5 ];
				ui_pelpair = rgui16_palette[ i_pel ] | ( rgui16_palette[ i_pel2 ] << 16 );
				pui_dst[ 2 ] = ui_pelpair;
				i_pel = pui8_line[ 6 ];
				i_pel2 = pui8_line[ 7 ];
				ui_pelpair = rgui16_palette[ i_pel ] | ( rgui16_palette[ i_pel2 ] << 16 );
				pui_dst[ 3 ] = ui_pelpair;
				i_pel = pui8_line[ 8 ];
				i_pel2 = pui8_line[ 9 ];
				ui_pelpair = rgui16_palette[ i_pel ] | ( rgui16_palette[ i_pel2 ] << 16 );
				pui_dst[ 4 ] = ui_pelpair;
				i_pel = pui8_line[ 10 ];
				i_pel2 = pui8_line[ 11 ];
				ui_pelpair = rgui16_palette[ i_pel ] | ( rgui16_palette[ i_pel2 ] << 16 );
				pui_dst[ 5 ] = ui_pelpair;
				i_pel = pui8_line[ 12 ];
				i_pel2 = pui8_line[ 13 ];
				ui_pelpair = rgui16_palette[ i_pel ] | ( rgui16_palette[ i_pel2 ] << 16 );
				pui_dst[ 6 ] = ui_pelpair;
				i_pel = pui8_line[ 14 ];
				i_pel2 = pui8_line[ 15 ];
				ui_pelpair = rgui16_palette[ i_pel ] | ( rgui16_palette[ i_pel2 ] << 16 );
				pui_dst[ 7 ] = ui_pelpair;
				i_line_left -= 16;
				pui_dst += 8;
				pui8_line += 16;
			}
			pui16_rem = ( unsigned short *)pui_dst;
			while( i_line_left-- > 0 )
			{
				i_pel = *(pui8_line++);
				*(pui16_rem++) = rgui16_palette[ i_pel ];
			}
		}
		else
		{
			for( i_x = rects->x; i_x < rects->x+rects->width; i_x++ )
			{
				int i_pel = vid_buffer[ i_x + i_y * BASEWIDTH ];
				ptr[ i_x + i_y * i_width ] = rgui16_palette[ i_pel ];
			}
		}
	}
}

/*
================
D_BeginDirectRect
================
*/
void D_BeginDirectRect (int x, int y, byte *pbitmap, int width, int height)
{
}


/*
================
D_EndDirectRect
================
*/
void D_EndDirectRect (int x, int y, int width, int height)
{
}


