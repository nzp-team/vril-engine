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
#include "../image.h"
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
qpic_t		*sniper_scope;
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


int		texels;

//Loading Fill by Crow_bar
float 	loading_cur_step;
char	loading_name[32];
float 	loading_num_step;
int 	loading_step;
float 	loading_cur_step_bk;

typedef struct
{
	int		texnum;
	char identifier[64];
	int		original_width;
	int		original_height;
	int		width, height;
	bool	mipmap;
} gltexture_t;

#define	MAX_GLTEXTURES	1024
gltexture_t	gltextures[MAX_GLTEXTURES];
int numgltextures = 0;

static int LoadExternalPic(char *identifier)
{
	char fname[512];
	COM_StripExtension (identifier, fname);
	int w, h;
	byte *data = Image_LoadImage (fname, &w, &h);
	if (data) {
		int r = GL_LoadTexture32 ("", w, h, data, false, true, false);
		free(data);
		return r;
	}
	return -1;
}

// FIXME: It seems the texture manager fails with Half Life BSPs

/*
 * Texture Manager - derived from glesquake
 */
class textureStore {

private:
    static const GLuint UNUSED = (GLuint) -2;
    static const GLuint PAGED_OUT = (GLuint) -1;

    struct entry
    {
        entry* next;
        entry* prev;
        GLuint real_texnum;    // UNUSED, PAGED_OUT
		bool is32;
        byte* pData; // 0 ==> not created by us.
        size_t size;
        bool alpha;
        int width;
        int height;
        bool mipmap;

        entry() {
            next = 0;
            prev = 0;
            real_texnum = UNUSED;
            pData = 0;
        }


        void unlink() {
            if (next) {
                next->prev = prev;
            }
            if (prev) {
                prev->next = next;
            }
            next = 0;
            prev = 0;
        }

        void insertBefore(entry* e){
            if (e) {
                prev = e->prev;
                if ( prev ) {
                    prev->next = this;
                }
                next = e;
                e->prev = this;
            }
            else {
                prev = 0;
                next = 0;
            }
        }
    };

public:

    static textureStore* get() {
        if (g_pTextureCache == 0) {
            g_pTextureCache = new textureStore();
        }
        return g_pTextureCache;
    }

    // Equivalent of glBindTexture, but uses the virtual texture table

    void bind(int virtTexNum) {
        if ( (unsigned int) virtTexNum >= TEXTURE_STORE_NUM_TEXTURES) {
            Sys_Error("not in the range we're managing");
        }
        mBoundTextureID = virtTexNum;
        entry* e = &mTextures[virtTexNum];

        if ( e->real_texnum == UNUSED) {
            glGenTextures( 1, &e->real_texnum);
        }

        if ( e->pData == 0) {
            glBindTexture(GL_TEXTURE_2D, e->real_texnum);
            return;
        }

        update(e);
    }

    void update(entry* e)
    {
        // Update the "LRU" part of the cache
        unlink(e);
        e->insertBefore(mFirst);
        mFirst = e;
        if (! mLast) {
            mLast = e;
        }

        if (e->real_texnum == PAGED_OUT ) {
            // Create a real texture
            // Make sure there is enough room for this texture
            ensure(e->size);

            glGenTextures( 1, &e->real_texnum);
			
            glBindTexture(GL_TEXTURE_2D, e->real_texnum);
			if (e->is32)
				GL_Upload32 ((unsigned*)e->pData, e->width, e->height, e->mipmap,
                    e->alpha);
			else
				GL_Upload8 (e->pData, e->width, e->height, e->mipmap,
                    e->alpha);
        }
        else {
            glBindTexture(GL_TEXTURE_2D, e->real_texnum);
        }
    }

    // Create a texture, and remember the data so we can create
    // it again later.

    void create(int width, int height, byte* data, bool mipmap,
            bool alpha, bool is32) {
        int size = width * height;
		if (is32) size *= 4;
        if (size + mLength > mCapacity) {
            Sys_Error("Ran out of virtual texture space. %d", size);
        };
        entry* e = &mTextures[mBoundTextureID];

        // Call evict in case the currently bound texture id is already
        // in use. (Shouldn't happen in Quake.)
        // To Do: reclaim the old texture memory from the virtual memory.

        evict(e);

        e->alpha = alpha;
        e->pData = mBase + mLength;
        memcpy(e->pData, data, size);
        e->size = size;
        e->width = width;
        e->height = height;
        e->mipmap = mipmap;
		e->is32 = is32;
        e->real_texnum = PAGED_OUT;
        mLength += size;

        update(e);
    }

    // Re-upload the current textures because we've been reset.
    void rebindAll() {
        grabMagicTextureIds();
        for (entry* e = mFirst; e; e = e->next ) {
            if (! (e->real_texnum == UNUSED || e->real_texnum == PAGED_OUT)) {
                glBindTexture(GL_TEXTURE_2D, e->real_texnum);
                if (e->pData) {
					if (e->is32)
						GL_Upload32 ((unsigned*)e->pData, e->width, e->height, e->mipmap,
							e->alpha);
					else
						GL_Upload8 (e->pData, e->width, e->height, e->mipmap,
							e->alpha);
                }
            }
        }
    }

private:

    textureStore() {
        grabMagicTextureIds();
        mFirst = 0;
        mLast = 0;
        mTextureCount = 0;

        mBase = (byte*)malloc(TEXTURE_STORE_SIZE);
		mBase[TEXTURE_STORE_SIZE-1] = 0;
		
        mLength = 0;
        mCapacity = TEXTURE_STORE_SIZE;
        mRamUsed = 0;
        mRamSize = LIVE_TEXTURE_LIMIT;
    }

    ~textureStore() {
        free(mBase);
    }

    void grabMagicTextureIds() {
        // reserve these two texture ids.
        glBindTexture(GL_TEXTURE_2D, UNUSED);
        glBindTexture(GL_TEXTURE_2D, PAGED_OUT);
    }

    void unlink(entry* e) {
        if (e == mFirst) {
            mFirst = e->next;
        }
        if (e == mLast) {
            mLast = e->prev;
        }
        e->unlink();
    }

    void ensure(int size) {
        while ( mRamSize - mRamUsed < (unsigned int) size) {
            entry* e = mLast;
            if(! e) {
                Sys_Error("Ran out of entries");
                return;
            }
            evict(e);
        }
        mRamUsed += size;
    }

    void evict(entry* e) {
        unlink(e);
        if ( e->pData ) {
            glDeleteTextures(1, &e->real_texnum);
            e->real_texnum = PAGED_OUT;
            mRamUsed -= e->size;
        }
    }

    static const size_t TEXTURE_STORE_SIZE = 16 * 1024 * 1024 * 2;
    static const size_t LIVE_TEXTURE_LIMIT = 1 * 1024 * 1024 * 2;
    static const size_t TEXTURE_STORE_NUM_TEXTURES = 1024;

    byte* mBase;
    size_t mLength;
    size_t mCapacity;

    // Keep track of texture RAM.
    size_t mRamUsed;
    size_t mRamSize;

    // The virtual textures
    entry mTextures[MAX_GLTEXTURES];
    entry* mFirst; // LRU queue
    entry* mLast;
    size_t mTextureCount; // How many virtual textures have been allocated

    static textureStore* g_pTextureCache;

    int mBoundTextureID;
};

textureStore* textureStore::g_pTextureCache;

void GL_Bind (int texnum)
{
	if (currenttexture == texnum)
		return;
	currenttexture = texnum;
	
	if (tex_cache)
		textureStore::get()->bind(texnum);
	else
		glBindTexture(GL_TEXTURE_2D, texnum);
}

/*
=============================================================================

  scrap allocation

  Allocate all the little status bar obejcts into a single texture
  to crutch up stupid hardware / drivers

=============================================================================
*/

#define	MAX_SCRAPS		2
#define	BLOCK_WIDTH		256
#define	BLOCK_HEIGHT	256

int			scrap_allocated[MAX_SCRAPS][BLOCK_WIDTH];
byte		scrap_texels[MAX_SCRAPS][BLOCK_WIDTH*BLOCK_HEIGHT*4];
bool	scrap_dirty;
int			scrap_texnum;

// returns a texture number and the position inside it
int Scrap_AllocBlock (int w, int h, int *x, int *y)
{
	int		i, j;
	int		best, best2;
	int		bestx;
	int		texnum;

	for (texnum=0 ; texnum<MAX_SCRAPS ; texnum++)
	{
		best = BLOCK_HEIGHT;

		for (i=0 ; i<BLOCK_WIDTH-w ; i++)
		{
			best2 = 0;

			for (j=0 ; j<w ; j++)
			{
				if (scrap_allocated[texnum][i+j] >= best)
					break;
				if (scrap_allocated[texnum][i+j] > best2)
					best2 = scrap_allocated[texnum][i+j];
			}
			if (j == w)
			{	// this is a valid spot
				*x = i;
				*y = best = best2;
			}
		}

		if (best + h > BLOCK_HEIGHT)
			continue;

		for (i=0 ; i<w ; i++)
			scrap_allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	Sys_Error ("Scrap_AllocBlock: full");
	return 0;
}

int	scrap_uploads;

void Scrap_Upload (void)
{
	int		texnum;

	scrap_uploads++;

	for (texnum=0 ; texnum<MAX_SCRAPS ; texnum++) {
		GL_Bind(scrap_texnum + texnum);
		GL_Upload8 (scrap_texels[texnum], BLOCK_WIDTH, BLOCK_HEIGHT, false, true);
	}
	scrap_dirty = false;
}

//=============================================================================
/* Support Routines */

typedef struct cachepic_s
{
	char		name[MAX_QPATH];
	qpic_t		pic;
	byte		padding[32];	// for appended glpic
} cachepic_t;

#define	MAX_CACHED_PICS		128
cachepic_t	menu_cachepics[MAX_CACHED_PICS];
int			menu_numcachepics;

byte		menuplyr_pixels[4096];

int		pic_texels;
int		pic_count;

/*
================
GL_LoadPicTexture
================
*/
int GL_LoadPicTexture (qpic_t *pic)
{
  return GL_LoadTexture ("", pic->width, pic->height, pic->data, false, true);
}


//Diabolickal TGA Begin
int		image_width;
int		image_height;
#define	IMAGE_MAX_DIMENSIONS	512

/*
=================================================================
  PCX Loading
=================================================================
*/

typedef struct
{
    char	manufacturer;
    char	version;
    char	encoding;
    char	bits_per_pixel;
    unsigned short	xmin,ymin,xmax,ymax;
    unsigned short	hres,vres;
    unsigned char	palette[48];
    char	reserved;
    char	color_planes;
    unsigned short	bytes_per_line;
    unsigned short	palette_type;
    char	filler[58];
    unsigned 	data;			// unbounded
} pcx_t;

/*
============
LoadPCX
============
*/
byte* LoadPCX (FILE *f, int matchwidth, int matchheight)
{
	pcx_t	*pcx, pcxbuf;
	byte	palette[768];
	byte	*pix, *image_rgba;
	int		x, y;
	int		dataByte, runLength;
	int		count;

//
// parse the PCX file
//
	fread (&pcxbuf, 1, sizeof(pcxbuf), f);
	pcx = &pcxbuf;

	if (pcx->manufacturer != 0x0a
		|| pcx->version != 5
		|| pcx->encoding != 1
		|| pcx->bits_per_pixel != 8
		|| pcx->xmax >= 514
		|| pcx->ymax >= 514)
	{
		Con_Printf ("Bad pcx file\n");
		return NULL;
	}
	if (matchwidth && (pcx->xmax+1) != matchwidth)
		return NULL;
	if (matchheight && (pcx->ymax+1) != matchheight)
		return NULL;
	// seek to palette
	fseek (f, -768, SEEK_END);
	fread (palette, 1, 768, f);
	fseek (f, sizeof(pcxbuf) - 4, SEEK_SET);
	count = (pcx->xmax+1) * (pcx->ymax+1);
	image_rgba = (byte*)malloc( count * 4);

	for (y=0 ; y<=pcx->ymax ; y++)
	{
		pix = image_rgba + 4*y*(pcx->xmax+1);
		for (x=0 ; x<=pcx->xmax ; ) // muff - fixed - was referencing ymax
		{
			dataByte = fgetc(f);
			if((dataByte & 0xC0) == 0xC0)
			{
				runLength = dataByte & 0x3F;
				dataByte = fgetc(f);
			}
			else
				runLength = 1;

			while(runLength-- > 0)
			{
				pix[0] = palette[dataByte*3];
				pix[1] = palette[dataByte*3+1];
				pix[2] = palette[dataByte*3+2];
				pix[3] = 255;
				pix += 4;
				x++;
			}
		}
	}
	image_width = pcx->xmax+1;
	image_height = pcx->ymax+1;

	fclose(f);
	return image_rgba;
}


/*
=========================================================

			Targa

=========================================================
*/

#define TGA_MAXCOLORS 16384

/* Definitions for image types. */
#define TGA_Null	0	/* no image data */
#define TGA_Map		1	/* Uncompressed, color-mapped images. */
#define TGA_RGB		2	/* Uncompressed, RGB images. */
#define TGA_Mono	3	/* Uncompressed, black and white images. */
#define TGA_RLEMap	9	/* Runlength encoded color-mapped images. */
#define TGA_RLERGB	10	/* Runlength encoded RGB images. */
#define TGA_RLEMono	11	/* Compressed, black and white images. */
#define TGA_CompMap	32	/* Compressed color-mapped data, using Huffman, Delta, and runlength encoding. */
#define TGA_CompMap4	33	/* Compressed color-mapped data, using Huffman, Delta, and runlength encoding. 4-pass quadtree-type process. */

/* Definitions for interleave flag. */
#define TGA_IL_None	0	/* non-interleaved. */
#define TGA_IL_Two	1	/* two-way (even/odd) interleaving */
#define TGA_IL_Four	2	/* four way interleaving */
#define TGA_IL_Reserved	3	/* reserved */

/* Definitions for origin flag */
#define TGA_O_UPPER	0	/* Origin in lower left-hand corner. */
#define TGA_O_LOWER	1	/* Origin in upper left-hand corner. */

typedef struct _TargaHeader
{
	unsigned char 	id_length, colormap_type, image_type;
	unsigned short	colormap_index, colormap_length;
	unsigned char	colormap_size;
	unsigned short	x_origin, y_origin, width, height;
	unsigned char	pixel_size, attributes;
} TargaHeader;

int fgetLittleShort (FILE *f)
{
	byte	b1, b2;

	b1 = fgetc(f);
	b2 = fgetc(f);

	return (short)(b1 + b2*256);
}

int fgetLittleLong (FILE *f)
{
	byte	b1, b2, b3, b4;

	b1 = fgetc(f);
	b2 = fgetc(f);
	b3 = fgetc(f);
	b4 = fgetc(f);

	return b1 + (b2<<8) + (b3<<16) + (b4<<24);
}

/*
=============
LoadTGA
=============
*/
byte *LoadTGA (FILE *fin, int matchwidth, int matchheight)
{
	int		w, h, x, y, realrow, truerow, baserow, i, temp1, temp2, pixel_size, map_idx;
	int		RLE_count, RLE_flag, size, interleave, origin;
	bool	mapped, rlencoded;
	byte		*data, *dst, r, g, b, a, j, k, l, *ColorMap;
	TargaHeader	header;

	header.id_length = fgetc (fin);
	header.colormap_type = fgetc (fin);
	header.image_type = fgetc (fin);
	header.colormap_index = fgetLittleShort (fin);
	header.colormap_length = fgetLittleShort (fin);
	header.colormap_size = fgetc (fin);
	header.x_origin = fgetLittleShort (fin);
	header.y_origin = fgetLittleShort (fin);
	header.width = fgetLittleShort (fin);
	header.height = fgetLittleShort (fin);
	header.pixel_size = fgetc (fin);
	header.attributes = fgetc (fin);

	if (header.width > IMAGE_MAX_DIMENSIONS || header.height > IMAGE_MAX_DIMENSIONS)
	{
		Con_DPrintf ("TGA image %s exceeds maximum supported dimensions\n", fin);
		fclose (fin);
		return NULL;
	}

	if ((matchwidth && header.width != matchwidth) || (matchheight && header.height != matchheight))
	{
		fclose (fin);
		return NULL;
	}

	if (header.id_length != 0)
		fseek (fin, header.id_length, SEEK_CUR);

	/* validate TGA type */
	switch (header.image_type)
	{
	case TGA_Map:
	case TGA_RGB:
	case TGA_Mono:
	case TGA_RLEMap:
	case TGA_RLERGB:
	case TGA_RLEMono:
		break;

	default:
		Con_DPrintf ("Unsupported TGA image %s: Only type 1 (map), 2 (RGB), 3 (mono), 9 (RLEmap), 10 (RLERGB), 11 (RLEmono) TGA images supported\n");
		fclose (fin);
		return NULL;
	}

	/* validate color depth */
	switch (header.pixel_size)
	{
	case 8:
	case 15:
	case 16:
	case 24:
	case 32:
		break;

	default:
		Con_DPrintf ("Unsupported TGA image %s: Only 8, 15, 16, 24 or 32 bit images (with colormaps) supported\n");
		fclose (fin);
		return NULL;
	}

	r = g = b = a = l = 0;

	/* if required, read the color map information. */
	ColorMap = NULL;
	mapped = (header.image_type == TGA_Map || header.image_type == TGA_RLEMap) && header.colormap_type == 1;
	if (mapped)
	{
		/* validate colormap size */
		switch (header.colormap_size)
		{
		case 8:
		case 15:
		case 16:
		case 32:
		case 24:
			break;

		default:
			Con_DPrintf ("Unsupported TGA image %s: Only 8, 15, 16, 24 or 32 bit colormaps supported\n");
			fclose (fin);
			return NULL;
		}

		temp1 = header.colormap_index;
		temp2 = header.colormap_length;
		if ((temp1 + temp2 + 1) >= TGA_MAXCOLORS)
		{
			fclose (fin);
			return NULL;
		}
		ColorMap = (byte*)(malloc (TGA_MAXCOLORS * 4));
		map_idx = 0;
		for (i = temp1 ; i < temp1 + temp2 ; ++i, map_idx += 4)
		{
			/* read appropriate number of bytes, break into rgb & put in map. */
			switch (header.colormap_size)
			{
			case 8:	/* grey scale, read and triplicate. */
				r = g = b = getc (fin);
				a = 255;
				break;

			case 15:	/* 5 bits each of red green and blue. */
						/* watch byte order. */
				j = getc (fin);
				k = getc (fin);
				l = ((unsigned int)k << 8) + j;
				r = (byte)(((k & 0x7C) >> 2) << 3);
				g = (byte)((((k & 0x03) << 3) + ((j & 0xE0) >> 5)) << 3);
				b = (byte)((j & 0x1F) << 3);
				a = 255;
				break;

			case 16:	/* 5 bits each of red green and blue, 1 alpha bit. */
						/* watch byte order. */
				j = getc (fin);
				k = getc (fin);
				l = ((unsigned int)k << 8) + j;
				r = (byte)(((k & 0x7C) >> 2) << 3);
				g = (byte)((((k & 0x03) << 3) + ((j & 0xE0) >> 5)) << 3);
				b = (byte)((j & 0x1F) << 3);
				a = (k & 0x80) ? 255 : 0;
				break;

			case 24:	/* 8 bits each of blue, green and red. */
				b = getc (fin);
				g = getc (fin);
				r = getc (fin);
				a = 255;
				l = 0;
				break;

			case 32:	/* 8 bits each of blue, green, red and alpha. */
				b = getc (fin);
				g = getc (fin);
				r = getc (fin);
				a = getc (fin);
				l = 0;
				break;
			}
			ColorMap[map_idx+0] = r;
			ColorMap[map_idx+1] = g;
			ColorMap[map_idx+2] = b;
			ColorMap[map_idx+3] = a;
		}
	}

	/* check run-length encoding. */
	rlencoded = (header.image_type == TGA_RLEMap || header.image_type == TGA_RLERGB || header.image_type == TGA_RLEMono);
	RLE_count = RLE_flag = 0;

	image_width = w = header.width;
	image_height = h = header.height;

	size = w * h * 4;
	data = (byte*)(malloc (size));

	/* read the Targa file body and convert to portable format. */
	pixel_size = header.pixel_size;
	origin = (header.attributes & 0x20) >> 5;
	interleave = (header.attributes & 0xC0) >> 6;
	truerow = baserow = 0;
	for (y=0 ; y<h ; y++)
	{
		realrow = truerow;
		if (origin == TGA_O_UPPER)
			realrow = h - realrow - 1;

		dst = data + realrow * w * 4;

		for (x=0 ; x<w ; x++)
		{
			/* check if run length encoded. */
			if (rlencoded)
			{
				if (!RLE_count)
				{
					/* have to restart run. */
					i = getc (fin);
					RLE_flag = (i & 0x80);
					if (!RLE_flag)	// stream of unencoded pixels
						RLE_count = i + 1;
					else		// single pixel replicated
						RLE_count = i - 127;
					/* decrement count & get pixel. */
					--RLE_count;
				}
				else
				{
					/* have already read count & (at least) first pixel. */
					--RLE_count;
					if (RLE_flag)
						/* replicated pixels. */
						goto PixEncode;
				}
			}

			/* read appropriate number of bytes, break into RGB. */
			switch (pixel_size)
			{
			case 8:	/* grey scale, read and triplicate. */
				r = g = b = l = getc (fin);
				a = 255;
				break;

			case 15:	/* 5 bits each of red green and blue. */
						/* watch byte order. */
				j = getc (fin);
				k = getc (fin);
				l = ((unsigned int)k << 8) + j;
				r = (byte)(((k & 0x7C) >> 2) << 3);
				g = (byte)((((k & 0x03) << 3) + ((j & 0xE0) >> 5)) << 3);
				b = (byte)((j & 0x1F) << 3);
				a = 255;
				break;

			case 16:	/* 5 bits each of red green and blue, 1 alpha bit. */
						/* watch byte order. */
				j = getc (fin);
				k = getc (fin);
				l = ((unsigned int)k << 8) + j;
				r = (byte)(((k & 0x7C) >> 2) << 3);
				g = (byte)((((k & 0x03) << 3) + ((j & 0xE0) >> 5)) << 3);
				b = (byte)((j & 0x1F) << 3);
				a = (k & 0x80) ? 255 : 0;
				break;

			case 24:	/* 8 bits each of blue, green and red. */
				b = getc (fin);
				g = getc (fin);
				r = getc (fin);
				a = 255;
				l = 0;
				break;

			case 32:	/* 8 bits each of blue, green, red and alpha. */
				b = getc (fin);
				g = getc (fin);
				r = getc (fin);
				a = getc (fin);
				l = 0;
				break;

			default:
				Con_DPrintf ("Malformed TGA image: Illegal pixel_size '%d'\n", pixel_size);
				fclose (fin);
				free (data);
				if (mapped)
					free (ColorMap);
				return NULL;
			}

PixEncode:
			if (mapped)
			{
				map_idx = l * 4;
				*dst++ = ColorMap[map_idx+0];
				*dst++ = ColorMap[map_idx+1];
				*dst++ = ColorMap[map_idx+2];
				*dst++ = ColorMap[map_idx+3];
			}
			else
			{
				*dst++ = r;
				*dst++ = g;
				*dst++ = b;
				*dst++ = a;
			}
		}

		if (interleave == TGA_IL_Four)
			truerow += 4;
		else if (interleave == TGA_IL_Two)
			truerow += 2;
		else
			truerow++;
		if (truerow >= h)
			truerow = ++baserow;
	}

	if (mapped)
		free (ColorMap);

	fclose (fin);

	return data;
}

/*small function to read files with stb_image - single-file image loader library.
** downloaded from: https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
** only use jpeg+png formats, because tbh there's not much need for the others.
** */
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#include "../../../stb_image.h"
byte* LoadSTBI(FILE *f, int width, int height)
{
	int bpp;
	int inwidth, inheight;
	byte* image = stbi_load_from_file(f, &inwidth, &inheight, &bpp, 4);
	// wtf?
	image_width = inwidth;
	image_height = inheight;
	fclose(f);
	return image;
}

byte* loadimagepixels (const char* filename, bool complain, int matchwidth, int matchheight)

{
	FILE	*f;
	char	basename[128], name[132];
	byte	*c;

	if (complain == qfalse)
		COM_StripExtension((char*)filename, basename); // strip the extension to allow TGA
	else
		strcpy(basename, filename);

	c = (byte*)basename;
	while (*c)
	{
		if (*c == '*')
			*c = '+';
		c++;
	}

	//Try PCX
	sprintf (name, "%s.pcx", basename);
	COM_FOpenFile (name, &f);
	if (f)
		return LoadPCX (f, matchwidth, matchheight);
	//Try TGA
	sprintf (name, "%s.tga", basename);
	COM_FOpenFile (name, &f);
	if (f)
		return LoadTGA (f, matchwidth, matchheight);
	//Try PNG
	sprintf (name, "%s.png", basename);
	COM_FOpenFile (name, &f);
	if (f)
		return LoadSTBI (f, matchwidth, matchheight);
	//Try JPEG
	sprintf (name, "%s.jpeg", basename);
	COM_FOpenFile (name, &f);
	if (f)
		return LoadSTBI (f, matchwidth, matchheight);
	sprintf (name, "%s.jpg", basename);
	COM_FOpenFile (name, &f);
	if (f)
		return LoadSTBI (f, matchwidth, matchheight);
	
	//if (complain)
	//	Con_Printf ("Couldn't load %s.tga or %s.pcx \n", filename);
	
	return NULL;
}

int loadtextureimage (const char* filename, int matchwidth, int matchheight, bool complain, bool mipmap)
{
	int texnum;
	byte *data;
	if (!(data = loadimagepixels (filename, complain, matchwidth, matchheight))) { 
		Con_DPrintf("Cannot load image %s\n", filename);
		return 0;
	}
	texnum = GL_LoadTexture32 (filename, image_width, image_height, data, mipmap, true, false);
	free(data);
	return texnum;
}
// Tomaz || TGA End


/*
================
Draw_CachePic
================
*/
qpic_t	*Draw_CachePic (char *path)
{
	cachepic_t	*pic;
	int			i;
	qpic_t		*dat;
	glpic_t		*gl;
	char		str[128];

	strcpy (str, path);
	for (pic=menu_cachepics, i=0 ; i<menu_numcachepics ; pic++, i++)
		if (!strcmp (str, pic->name))
			return &pic->pic;

	if (menu_numcachepics == MAX_CACHED_PICS)
		Sys_Error ("menu_numcachepics == MAX_CACHED_PICS");
	menu_numcachepics++;
	strcpy (pic->name, str);

//
// load the pic from disk
//

	int index = loadtextureimage (str, 0, 0, false, false);
	if(index)
	{
		pic->pic.width  = gltextures[index].original_width;
		pic->pic.height = gltextures[index].original_height;

		gl = (glpic_t *)pic->pic.data;
		gl->texnum = index;

		return &pic->pic;
	}

	dat = (qpic_t *)COM_LoadTempFile (str);
	if (!dat)
	{
		strcat (str, ".lmp");
		dat = (qpic_t *)COM_LoadTempFile (str);
		if (!dat)
		{
			Con_Printf ("Draw_CachePic: failed to load file %s\n", str);
			return NULL;
		}
	}
	SwapPic (dat);


	pic->pic.width = dat->width;
	pic->pic.height = dat->height;

	gl = (glpic_t *)pic->pic.data;
	gl->texnum = GL_LoadPicTexture (dat);

	return &pic->pic;
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
	char_texture = loadtextureimage ("gfx/charset", 0, 0, false, false);
	if (char_texture == 0)// did not find a matching TGA...
		Sys_Error ("Could not load charset, make sure you have every folder and file installed properly\nDouble check that all of your files are in their correct places\nAnd that you have installed the game properly.\n");

	// save a texture slot for translated picture
	translate_texture = texture_extension_number++;

	// save slots for scraps
	scrap_texnum = texture_extension_number;
	texture_extension_number += MAX_SCRAPS;
	sniper_scope = Draw_CachePic ("gfx/hud/scope");
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
Draw_AlphaPic
=============
*/
void Draw_AlphaPic (int x, int y, qpic_t *pic, float alpha)
{
	byte			*dest, *source;
	unsigned short	*pusdest;
	int				v, u;
	glpic_t			*gl;

	if (alpha == 0.0f) return;
	if (scrap_dirty)
		Scrap_Upload ();
	gl = (glpic_t *)pic->data;
	GL_DisableState(GL_ALPHA_TEST);
	GL_EnableState(GL_MODULATE);
	glEnable (GL_BLEND);
	Platform_Graphics_Color(1,1,1,alpha);
	GL_Bind (gl->texnum);
	DrawQuad(x, y, pic->width, pic->height, gl->sl, gl->tl, gl->sh - gl->sl, gl->th - gl->tl);
	Platform_Graphics_Color(1,1,1,1);
	GL_EnableState(GL_ALPHA_TEST);
	glDisable (GL_BLEND);
	glDepthMask(GL_TRUE);
}

/*
=============
Draw_Pic
=============
*/
void Draw_Pic (int x, int y, qpic_t *pic)
{
	byte			*dest, *source;
	unsigned short	*pusdest;
	int				v, u;
	glpic_t			*gl;

	if (scrap_dirty)
		Scrap_Upload ();
	glpic_t temp;
	memcpy(&temp, pic->data, sizeof(temp));
	gl = &temp;
	Platform_Graphics_Color(1, 1, 1, 1);
	GL_Bind (gl->texnum);

	DrawQuad(x, y, pic->width, pic->height, gl->sl, gl->tl, gl->sh - gl->sl, gl->th - gl->tl);
}

void Draw_StretchPic (int x, int y, qpic_t *pic, int x_value, int y_value) { Draw_Pic(x, y, pic); };
void Draw_ColorPic (int x, int y, qpic_t *pic, float r, float g , float b, float a) { Draw_Pic(x, y, pic); };
void Draw_ColoredStretchPic (int x, int y, qpic_t *pic, int x_value, int y_value, int r, int g, int b, int a) { Draw_Pic(x, y, pic); };

/*
=============
Draw_AlphaPic
=============
*/
void Draw_AlphaPic (int x, int y, qpic_t *pic)
{
	byte			*dest, *source;
	unsigned short	*pusdest;
	int				v, u;
	glpic_t			*gl;

	if (scrap_dirty)
		Scrap_Upload ();
	glpic_t temp;
	memcpy(&temp, pic->data, sizeof(temp));
	gl = &temp;
	Platform_Graphics_Color(1, 1, 1, 1);
	GL_Bind (gl->texnum);

	DrawQuad(x, y, pic->width, pic->height, gl->sl, gl->tl, gl->sh - gl->sl, gl->th - gl->tl);
}

/*
=============
Draw_TransPic
=============
*/
void Draw_TransPic (int x, int y, qpic_t *pic)
{
	byte	*dest, *source, tbyte;
	unsigned short	*pusdest;
	int				v, u;

	if (x < 0 || (unsigned)(x + pic->width) > vid.width || y < 0 ||
		 (unsigned)(y + pic->height) > vid.height)
	{
		Sys_Error ("Draw_TransPic: bad coordinates");
	}
	
	Draw_Pic (x, y, pic);
}


/*
=============
Draw_TransPicTranslate

Only used for the player color selection menu
=============
*/
void Draw_TransPicTranslate (int x, int y, qpic_t *pic, byte *translation)
{
	int				v, u, c;
	unsigned		trans[64*64], *dest;
	byte			*src;
	int				p;

	GL_Bind (translate_texture);

	c = pic->width * pic->height;

	dest = trans;
	for (v=0 ; v<64 ; v++, dest += 64)
	{
		src = &menuplyr_pixels[ ((v*pic->height)>>6) *pic->width];
		for (u=0 ; u<64 ; u++)
		{
			p = src[(u*pic->width)>>6];
			if (p == 255)
				dest[u] = p;
			else
				dest[u] =  d_8to24table[translation[p]];
		}
	}
	
	glTexImage2D (GL_TEXTURE_2D, 0, gl_compress.value ? GL_COMPRESSED_RGBA_S3TC_DXT5_EXT : gl_alpha_format, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	Platform_Graphics_Color(1,1,1,1);
	DrawQuad(x, y, pic->width, pic->height, 0, 0, 1, 1);
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
GL_Resample8BitTexture -- JACK
================
*/
void GL_Resample8BitTexture (unsigned char *in, int inwidth, int inheight, unsigned char *out,  int outwidth, int outheight)
{
	int		i, j;
	unsigned	char *inrow;
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
================
GL_MipMap8Bit

Mipping for 8 bit textures
================
*/
void GL_MipMap8Bit (byte *in, int width, int height)
{
	int		i, j;
	unsigned short     r,g,b;
	byte	*out, *at1, *at2, *at3, *at4;

//	width <<=2;
	height >>= 1;
	out = in;
	for (i=0 ; i<height ; i++, in+=width)
	{
		for (j=0 ; j<width ; j+=2, out+=1, in+=2)
		{
			at1 = (byte *) (d_8to24table + in[0]);
			at2 = (byte *) (d_8to24table + in[1]);
			at3 = (byte *) (d_8to24table + in[width+0]);
			at4 = (byte *) (d_8to24table + in[width+1]);

 			r = (at1[0]+at2[0]+at3[0]+at4[0]); r>>=5;
 			g = (at1[1]+at2[1]+at3[1]+at4[1]); g>>=5;
 			b = (at1[2]+at2[2]+at3[2]+at4[2]); b>>=5;

			out[0] = d_15to8table[(r<<0) + (g<<5) + (b<<10)];
		}
	}
}

/*
===============
GL_Upload32
===============
*/
void GL_Upload32 (unsigned *data, int width, int height,  bool mipmap, bool alpha)
{
	if (!gl_mipmap.value) mipmap = false;
	int			samples;
	static unsigned int scaled[2048*2048];	// [512*256];
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

	if (scaled_width * scaled_height > sizeof(scaled)/4)
		Sys_Error ("GL_LoadTexture: too big");

	if (gl_compress.value) {
		samples = alpha ? GL_COMPRESSED_RGBA_S3TC_DXT5_EXT : GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
		mipmap = false; // Compressed textures do not support mipmaps yet in vitaGL
	} else samples = alpha ? gl_alpha_format : gl_solid_format;

	texels += scaled_width * scaled_height;

	if (scaled_width == width && scaled_height == height)
	{
		if (!mipmap)
		{
			glTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
			goto done;
		}
		memcpy (scaled, data, width*height*4);
	}
	else
		GL_ResampleTexture (data, width, height, scaled, scaled_width, scaled_height);
	
	glTexImage2D (GL_TEXTURE_2D, 0, samples, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, scaled);
	if (mipmap) glGenerateMipmap(GL_TEXTURE_2D);
	
done: ;
	
	if (mipmap)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_min);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, gl_filter_max);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, gl_filter_max);
	}
	
}

/*
===============
GL_Upload8
===============
*/
void GL_Upload8 (byte *data, int width, int height,  bool mipmap, bool alpha)
{
static	unsigned	trans[640*480];		// FIXME, temporary
	int			i, s;
	bool	noalpha;
	int			p;

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
			Sys_Error ("GL_Upload8: s&3");
		for (i=0 ; i<s ; i+=4)
		{
			trans[i] = d_8to24table[data[i]];
			trans[i+1] = d_8to24table[data[i+1]];
			trans[i+2] = d_8to24table[data[i+2]];
			trans[i+3] = d_8to24table[data[i+3]];
		}
	}

	GL_Upload32 (trans, width, height, mipmap, alpha);
}

/*
================
GL_LoadTexture
================
*/
int GL_LoadTexture (const char *identifier, int width, int height, byte *data, bool mipmap, bool alpha)
{
	bool	noalpha;
	int			i, p, s;
	gltexture_t	*glt;

	// see if the texture is already present
	if (identifier[0])
	{
		for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
		{
			if (!strcmp (identifier, glt->identifier))
			{
				// FIXME: Caching is broken with external textures
				//if (width != glt->width || height != glt->height)
				//	Sys_Error ("GL_LoadTexture32: cache mismatch for %s, expected: %ld, got %d", identifier, width, glt->width);
				return gltextures[i].texnum;
			}
		}
	}
	//else {                                    // 13/02/2000 removed: M.Tretene
		glt = &gltextures[numgltextures];
		numgltextures++;
	//}

	strcpy (glt->identifier, identifier);
	glt->texnum = texture_extension_number;
	glt->width = width;
	glt->height = height;
	glt->original_width	= width;
	glt->original_height = height;
	glt->mipmap = mipmap;

	GL_Bind(texture_extension_number );
	
	if (tex_cache)
		textureStore::get()->create(width, height, data, mipmap, alpha, false);
	else
		GL_Upload8 (data, width, height, mipmap, alpha);

	texture_extension_number++;

	return texture_extension_number-1;
}

/*
================
GL_LoadTexture32
================
*/
int GL_LoadTexture32 (const char *identifier, int width, int height, byte *data, bool mipmap, bool alpha, bool fullbright)
{
	bool	noalpha;
	int			i, p, s;
	gltexture_t	*glt;

	// see if the texture is already present
	if (identifier[0])
	{
		for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
		{
			if (!strcmp (identifier, glt->identifier))
			{
				// FIXME: Caching is broken with external textures
				//if (width != glt->width || height != glt->height)
				//	Sys_Error ("GL_LoadTexture32: cache mismatch for %s, expected: %ld, got %d", identifier, width, glt->width);
				return gltextures[i].texnum;
			}
		}
	}
	//else {                                    // 13/02/2000 removed: M.Tretene
		glt = &gltextures[numgltextures];
		numgltextures++;
	//}
	
	// Make black pixels transparent for *_luma textures
	if (fullbright) {
		int cnt = width * height * 4;
		for (i=0; i<cnt; i+=4) {
			if (data[i] < 1 && data[i+1] < 1 && data[i+2] < 1)
				data[i+3] = 0;
		}
	}
	
	strcpy (glt->identifier, identifier);
	glt->texnum = texture_extension_number;
	glt->width = width;
	glt->height = height;
	glt->original_width	= width;
	glt->original_height = height;
	glt->mipmap = mipmap;
	
	GL_Bind(texture_extension_number );
	
	if (tex_cache)
		textureStore::get()->create(width, height, data, mipmap, alpha, true);
	else
		GL_Upload32 ((unsigned*)data, width, height, mipmap, alpha);
	
	texture_extension_number++;

	return texture_extension_number-1;
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

void Clear_LoadingFill (void)
{
    //it is end loading
	loading_cur_step = 0;
	loading_cur_step_bk = 0;
	loading_num_step = 0;
	loading_step = -1;
	memset(loading_name, 0, sizeof(loading_name));
}

