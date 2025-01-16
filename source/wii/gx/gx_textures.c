/*
Copyright (C) 2008 Eluan Costa Miranda

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

#include <ogc/cache.h>
#include <ogc/system.h>
#include <ogc/lwp_heap.h>
#include <ogc/lwp_mutex.h>

#include "../../quakedef.h"

#include <gccore.h>
#include <malloc.h>

// ELUTODO: mipmap

cvar_t		gl_max_size = {"gl_max_size", "1024"};
cvar_t 		vid_retromode = {"vid_retromode", "1", false};

gltexture_t	gltextures[MAX_GLTEXTURES];
int			numgltextures;

heap_cntrl texture_heap;
void *texture_heap_ptr;
u32 texture_heap_size;

void R_InitTextureHeap (void)
{
	u32 level, size;

	_CPU_ISR_Disable(level);
	texture_heap_ptr = SYS_GetArena2Lo();
	texture_heap_size = 12 * 1024 * 1024;
	if ((u32)texture_heap_ptr + texture_heap_size > (u32)SYS_GetArena2Hi())
	{
		_CPU_ISR_Restore(level);
		Sys_Error("texture_heap + texture_heap_size > (u32)SYS_GetArena2Hi()");
	}	
	else
	{
		SYS_SetArena2Lo(texture_heap_ptr + texture_heap_size);
		_CPU_ISR_Restore(level);
	}

	memset(texture_heap_ptr, 0, texture_heap_size);

	size = __lwp_heap_init(&texture_heap, texture_heap_ptr, texture_heap_size, PPC_CACHE_ALIGNMENT);

	Con_Printf("Allocated %dM texture heap.\n", size / (1024 * 1024));
	
}

/*
==================
R_InitTextures
==================
*/
void	R_InitTextures (void)
{
	int		x,y, m;
	byte	*dest;

	R_InitTextureHeap();

	Cvar_RegisterVariable (&gl_max_size);

	numgltextures = 0;
	
// create a simple checkerboard texture for the default
	r_notexture_mip = Hunk_AllocName (sizeof(texture_t) + 16*16+8*8+4*4+2*2, "notexture");
	
	r_notexture_mip->width = r_notexture_mip->height = 16;
	r_notexture_mip->offsets[0] = sizeof(texture_t);
	r_notexture_mip->offsets[1] = r_notexture_mip->offsets[0] + 16*16;
	r_notexture_mip->offsets[2] = r_notexture_mip->offsets[1] + 8*8;
	r_notexture_mip->offsets[3] = r_notexture_mip->offsets[2] + 4*4;
	
	for (m=0 ; m<4 ; m++)
	{
		dest = (byte *)r_notexture_mip + r_notexture_mip->offsets[m];
		for (y=0 ; y< (16>>m) ; y++)
			for (x=0 ; x< (16>>m) ; x++)
			{
				if (  (y< (8>>m) ) ^ (x< (8>>m) ) )
					*dest++ = 0;
				else
					*dest++ = 0xff;
			}
	}	
	
}

void GL_Bind0 (int texnum)
{
	if (currenttexture0 == texnum)
		return;

	if (!gltextures[texnum].used)
		Sys_Error("Tried to bind a inactive texture0.");

	currenttexture0 = texnum;
	GX_LoadTexObj(&(gltextures[texnum].gx_tex), GX_TEXMAP0);
}

void GX_SetMinMag (int minfilt, int magfilt)
{
	if(gltextures[currenttexture0].data != NULL)
	{
		GX_InitTexObjFilterMode(&gltextures[currenttexture0].gx_tex, minfilt, magfilt);
	};
}

void GX_SetMaxAniso (int aniso)
{
	if(gltextures[currenttexture0].data != NULL)
	{
		GX_InitTexObjMaxAniso(&gltextures[currenttexture0].gx_tex, aniso);
	};
}

void QGX_ZMode(qboolean state)
{
	if (state)
		GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
	else
		GX_SetZMode(GX_FALSE, GX_LEQUAL, GX_TRUE);
}

void QGX_Alpha(qboolean state)
{
	if (state)
		GX_SetAlphaCompare(GX_GREATER,0,GX_AOP_AND,GX_ALWAYS,0);
	else
		GX_SetAlphaCompare(GX_ALWAYS,0,GX_AOP_AND,GX_ALWAYS,0);
	
}

void QGX_Blend(qboolean state)
{
	if (state)
		GX_SetBlendMode(GX_BM_BLEND, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	else
		GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);
}

void QGX_BlendMap(qboolean state)
{
	if (state)
		GX_SetBlendMode(GX_BM_BLEND, GX_BL_DSTCLR, GX_BL_SRCCLR, GX_LO_CLEAR);
	else
		GX_SetBlendMode(GX_BM_NONE,GX_BL_ONE,GX_BL_ZERO,GX_LO_COPY);
}

//====================================================================

/*
================
GL_FindTexture
================
*/
int GL_FindTexture (char *identifier)
{
	int		i;
	gltexture_t	*glt;

	for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
	{
		if (gltextures[i].used)
			if (!strcmp (identifier, glt->identifier))
				return gltextures[i].texnum;
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

int GX_RGBA_To_RGB5A3(u32 srccolor, qboolean flip)
{
	u16 color;

	u32 r, g, b, a;
	if (flip){
		r = srccolor & 0xFF;
		srccolor >>= 8;
		g = srccolor & 0xFF;
		srccolor >>= 8;
		b = srccolor & 0xFF;
		srccolor >>= 8;
		a = srccolor & 0xFF;
	} else {
		a = srccolor & 0xFF;
		srccolor >>= 8;
		b = srccolor & 0xFF;
		srccolor >>= 8;
		g = srccolor & 0xFF;
		srccolor >>= 8;
		r = srccolor & 0xFF;
	}
	
	if (a > 0xe0)
	{
		r = r >> 3;
		g = g >> 3;
		b = b >> 3;

		color = (r << 10) | (g << 5) | b;
		color |= 0x8000;
	}
	else
	{
		r = r >> 4;
		g = g >> 4;
		b = b >> 4;
		a = a >> 5;

		color = (a << 12) | (r << 8) | (g << 4) | b;
	}

	return color;
}

int GX_LinearToTiled(int x, int y, int width)
{
	int x0, x1, y0, y1;
	int offset;

	x0 = x & 3;
	x1 = x >> 2;
	y0 = y & 3;
	y1 = y >> 2;
	offset = x0 + 4 * y0 + 16 * x1 + 4 * width * y1;

	return offset;
}


/*
===============
GL_CopyRGB5A3

Converts from linear to tiled during copy
===============
*/
void GX_CopyRGB5A3(u16 *dest, u32 *src, int x1, int y1, int x2, int y2, int src_width)
{
	int i, j;

	for (i = y1; i < y2; i++)
		for (j = x1; j < x2; j++)
			dest[GX_LinearToTiled(j, i, src_width)] = src[j + i * src_width];
}

/*
===============
GL_CopyRGB5A3

Converts from linear RGBA8 to tiled RGB5A3 during copy
===============
*/
void GX_CopyRGBA8_To_RGB5A3(u16 *dest, u32 *src, int x1, int y1, int x2, int y2, int src_width, qboolean flip)
{
	int i, j;

	for (i = y1; i < y2; i++)
		for (j = x1; j < x2; j++)
			dest[GX_LinearToTiled(j, i, src_width)] = GX_RGBA_To_RGB5A3(src[j + i * src_width], flip);
}

/*
================
GL_MipMap

Operates in place, quartering the size of the texture
================
*/
void GX_MipMap (byte *in, int width, int height)
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

// Given w,h,level,and bpp, returns the offset to the mipmap at level "level"
static int _calc_mipmap_offset(int level, int w, int h, int b) {
	int size = 0;
	while (level > 0) {
		int mipsize = ((w*h)*b);
		if ((mipsize % 32) != 0)
			mipsize += (32-(mipsize % 32)); // Alignment
		size += mipsize;
		if (w != 1) w = w/2;
		if (h != 1) h = h/2;
		level--;
	}
	return size;
}

// FIXME, temporary
static	unsigned	scaled[640*480];
static	unsigned	trans[640*480];

/*
===============
GL_Upload32
===============
*/
void GL_Upload32 (gltexture_t *destination, unsigned *data, int width, int height, qboolean mipmap, qboolean alpha, qboolean flipRGBA)
{
	int	s;
	int	scaled_width, scaled_height;
	int sw, sh;
	u32 texbuffs;
	u32 texbuffs_mip;
	int max_mip_level;
	//heap_iblock info;

	for (scaled_width = 1 << 5 ; scaled_width < width ; scaled_width<<=1)
		;
	for (scaled_height = 1 << 5 ; scaled_height < height ; scaled_height<<=1)
		;

	if (scaled_width > gl_max_size.value)
		scaled_width = gl_max_size.value;
	if (scaled_height > gl_max_size.value)
		scaled_height = gl_max_size.value;
	
	if (scaled_width * scaled_height > sizeof(scaled)/4)
		Sys_Error ("GL_Upload32: too big");
	
	if (scaled_width != width || scaled_height != height)
	{
		GL_ResampleTexture (data, width, height, scaled, scaled_width, scaled_height);
		
	} else {
		memcpy(scaled, data, scaled_width * scaled_height * 4);
	}
	
	// start at mip level 0
	max_mip_level = 0;	
	if (mipmap) {
		sw = scaled_width;
		sh = scaled_height;
		
		while (sw > 4 && sh > 4)
		{
			sw >>= 1;
			sh >>= 1;
			max_mip_level++;
		};
		
		if (max_mip_level != 0) {
			// account for memory offset
			max_mip_level += 1; 
		}
	}
	
	//get exact buffer size of memory aligned on a 32byte boundery
	texbuffs = GX_GetTexBufferSize (scaled_width, scaled_height, GX_TF_RGB5A3, mipmap ? GX_TRUE : GX_FALSE, max_mip_level);
	destination->data = __lwp_heap_allocate(&texture_heap, texbuffs/*scaled_width * scaled_height * 2*/);	
	//__lwp_heap_getinfo(&texture_heap, &info);
	//Con_Printf ("tex buff size %d\n", texbuffs);
	//Con_Printf("Used Heap: %dM\n", info.used_size / (1024*1024));
	
	if (!destination->data)
		Sys_Error("GL_Upload32: Out of memory.");
	
	s = scaled_width * scaled_height;
	if (s & 31)
		Sys_Error ("GL_Upload32: s&31");

	if ((int)destination->data & 31)
		Sys_Error ("GL_Upload32: destination->data&31");
	
	destination->scaled_width = scaled_width;
	destination->scaled_height = scaled_height;
	
	//
	// sBTODO finish mipmap implementation 
	//
	
	if (mipmap == true) {
		
		int	mip_level;
		int sw, sh;
		unsigned mipmaptex[640*480];
		
		texbuffs_mip = GX_GetTexBufferSize (scaled_width, scaled_height, GX_TF_RGB5A3, GX_TRUE, max_mip_level);	
		
		// this should never happen currently however, 
		// I plan on circumventing reloading textures
		// which are already loaded, and this check will be neccesary 
		// once that happens
		if (texbuffs < texbuffs_mip) {
			// copy the texture mem to a temporary buffer
			unsigned char * tempbuf = malloc(texbuffs);
			memcpy(tempbuf,destination->data,texbuffs);
			
			// free the used heap memory
			if (!__lwp_heap_free(&texture_heap, destination->data))
				Sys_Error ("Failed to free texture mem for mipmap");
			
			// reallocate in a section of memory big enough for mipmaps and copy in the OG texture buffer
			destination->data = __lwp_heap_allocate (&texture_heap, texbuffs_mip);
			memcpy(destination->data,tempbuf,texbuffs);
			free (tempbuf);
		}
		
		// copy texture to dst addr and convert to RGB5A3
		GX_CopyRGBA8_To_RGB5A3((u16 *)destination->data, scaled, 0, 0, scaled_width, scaled_height, scaled_width, flipRGBA);
		// copy texture to new buffer
		memcpy((void *)mipmaptex, scaled, scaled_width * scaled_height * 4);
		
		sw = scaled_width;
		sh = scaled_height;
		mip_level = 1;
		
		//Con_Printf ("mip max: %i\n", mip_level);
		//Con_Printf ("texbuffs: %d\n", texbuffs);
		//Con_Printf ("texbuffs_mip: %d\n", texbuffs_mip);
		
		while (sw > 4 && sh > 4 && mip_level < 10) {
			// Operates in place, quartering the size of the texture
			GX_MipMap ((byte *)mipmaptex, sw, sh);
			
			sw >>= 1;
			sh >>= 1;
			if (sw < 4)
				sw = 4;
			if (sh < 4)
				sh = 4;
		
			//Con_Printf ("gen mipmaps: %i\n", mip_level);
			
			// Calculate the offset and address of the mipmap
			int offset = _calc_mipmap_offset(mip_level, scaled_width, scaled_height, 2);
			unsigned char* dst_addr = (unsigned char*)destination->data;
			dst_addr += offset;
			
			//Con_Printf ("mipmap mem offset: %i\n", offset);
			
			mip_level++;
			
			GX_CopyRGBA8_To_RGB5A3((u16 *)dst_addr, (u32 *)mipmaptex, 0, 0, sw, sh, sw, flipRGBA);
			DCFlushRange(dst_addr, sw * sh * 2);
			GX_InitTexObj(&destination->gx_tex, dst_addr, sw, sh, GX_TF_RGB5A3, GX_REPEAT, GX_REPEAT, GX_TRUE);
			GX_InitTexObjLOD(&destination->gx_tex, GX_LIN_MIP_LIN, GX_LIN_MIP_LIN, mip_level, max_mip_level, 0, GX_ENABLE, GX_ENABLE, GX_ANISO_2);	
		}
		
		DCFlushRange(destination->data, texbuffs_mip/*scaled_width * scaled_height * 2*/);
		GX_InvalidateTexAll();
		GX_InitTexObj(&destination->gx_tex, destination->data, scaled_width, scaled_height, GX_TF_RGB5A3, GX_REPEAT, GX_REPEAT, GX_TRUE);
		GX_InitTexObjLOD(&destination->gx_tex, GX_LIN_MIP_LIN, GX_LIN_MIP_LIN, 0, max_mip_level, 0, GX_ENABLE, GX_ENABLE, GX_ANISO_2);			
		//GX_LoadTexObj((&destination->gx_tex), GX_TEXMAP0);
	
		if (vid_retromode.value == 1) {
			GX_InitTexObjFilterMode(&destination->gx_tex, GX_NEAR_MIP_NEAR, GX_NEAR_MIP_NEAR);
		} else {
			GX_InitTexObjFilterMode(&destination->gx_tex, GX_LIN_MIP_LIN, GX_LIN_MIP_LIN);
		}
		
	} else {
		GX_CopyRGBA8_To_RGB5A3((u16 *)destination->data, scaled, 0, 0, scaled_width, scaled_height, scaled_width, flipRGBA);	
		DCFlushRange(destination->data, texbuffs/*scaled_width * scaled_height * 2*/);
		GX_InvalidateTexAll();
		GX_InitTexObj(&destination->gx_tex, destination->data, scaled_width, scaled_height, GX_TF_RGB5A3, GX_REPEAT, GX_REPEAT, /*mipmap ? GX_TRUE :*/ GX_FALSE);
		// do not init mipmaps for lightmaps
		if (destination->type != 1) {
			GX_InitTexObjLOD(&destination->gx_tex, GX_LIN_MIP_LIN, GX_LIN_MIP_LIN, 0, max_mip_level, 0, GX_ENABLE, GX_ENABLE, GX_ANISO_2);
		}
	}
}

/*
===============
GL_Upload8
===============
*/
void GL_Upload8 (gltexture_t *destination, byte *data, int width, int height,  qboolean mipmap, qboolean alpha)
{
	int			i, s;
	qboolean	noalpha;
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

	GL_Upload32 (destination, trans, width, height, mipmap, alpha, true);
}

byte		vid_gamma_table[256];
void Build_Gamma_Table (void) {
	int		i;
	float		inf;
	float   in_gamma;

	if ((i = COM_CheckParm("-gamma")) != 0 && i+1 < com_argc) {
		in_gamma = atof(com_argv[i+1]);
		if (in_gamma < 0.3) in_gamma = 0.3;
		if (in_gamma > 1) in_gamma = 1.0;
	} else {
		in_gamma = 1;
	}

	if (in_gamma != 1) {
		for (i=0 ; i<256 ; i++) {
			inf = MIN(255 * pow((i + 0.5) / 255.5, in_gamma) + 0.5, 255);
			vid_gamma_table[i] = inf;
		}
	} else {
		for (i=0 ; i<256 ; i++)
			vid_gamma_table[i] = i;
	}

}

/*
================
GL_LoadTexture
================
*/

//Diabolickal TGA Begin

int lhcsumtable[256];
int GL_LoadTexture (char *identifier, int width, int height, byte *data, qboolean mipmap, qboolean alpha, qboolean keep, int bytesperpixel)
{
	int			i, s, lhcsum;
	gltexture_t	*glt;
	// occurances. well this isn't exactly a checksum, it's better than that but
	// not following any standards.
	lhcsum = 0;
	s = width*height*bytesperpixel;
	
	for (i = 0;i < 256;i++) lhcsumtable[i] = i + 1;
	for (i = 0;i < s;i++) lhcsum += (lhcsumtable[data[i] & 255]++);

	// see if the texture is allready present
	if (identifier[0])
	{
		for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++)
		{
			if (glt->used)
			{
				// ELUTODO: causes problems if we compare to a texture with NO name?
				// sBTODO we definitely have issues with identifier strings. will investigate later..
				if (!strcmp (identifier, glt->identifier))
				{
					if (width != glt->width || height != glt->height)
					{
						Con_Printf ("GL_LoadTexture: cache mismatch, reloading");
						if (!__lwp_heap_free(&texture_heap, glt->data))
							Sys_Error("GL_ClearTextureCache: Error freeing data.");
						goto reload; // best way to do it
					}
					return glt->texnum;
				}
			}
		}
	}

	for (i = 0, glt = gltextures; i < numgltextures; i++, glt++)
	{
		if (!glt->used)
			break;
	}

	if (i == MAX_GLTEXTURES)
		Sys_Error ("GL_LoadTexture: numgltextures == MAX_GLTEXTURES\n");

reload:
	strcpy (glt->identifier, identifier);
	
	gltextures[glt->texnum].checksum = lhcsum;
	gltextures[glt->texnum].lhcsum = lhcsum;
	
	glt->texnum = i;
	glt->width = width;
	glt->height = height;
	glt->mipmap = mipmap;
	glt->type = 0;
	glt->keep = keep;
	glt->used = true;
	
	GL_Bind0 (glt->texnum);
	
	//Con_Printf ("tex %s\n", identifier);
	if (bytesperpixel == 1) {
		GL_Upload8 (glt, data, width, height, mipmap, alpha);
	}
	else if (bytesperpixel == 4) {
		GL_Upload32 (glt, (unsigned*)data, width, height, mipmap, alpha, false);
	}
	else {
		Sys_Error("GL_LoadTexture: unknown bytesperpixel\n");
	}

	if (glt->texnum == numgltextures)
		numgltextures++;

	return glt->texnum;
}

/*
======================
GL_LoadLightmapTexture
======================
*/
int GL_LoadLightmapTexture (char *identifier, int width, int height, byte *data)
{
	gltexture_t	*glt;

	// They need to be allocated sequentially
	if (numgltextures == MAX_GLTEXTURES)
		Sys_Error ("GL_LoadLightmapTexture: numgltextures == MAX_GLTEXTURES\n");

	glt = &gltextures[numgltextures];
	//Con_Printf("gltexnum: %i", numgltextures);
	strcpy (glt->identifier, identifier);
	//Con_Printf("Identifier: %s", identifier);
	glt->texnum = numgltextures;
	glt->width = width;
	glt->height = height;
	glt->mipmap = false; // ELUTODO
	glt->type = 1;
	glt->keep = false;
	glt->used = true;

	GL_Upload32 (glt, (unsigned *)data, width, height, false /*mipmap?*/, false, false);

	if (width != glt->scaled_width || height != glt->scaled_height)
		Sys_Error("GL_LoadLightmapTexture: Tried to scale lightmap\n");

	numgltextures++;

	return glt->texnum;
}

/*
================================
GL_UpdateLightmapTextureRegion32
================================
*/
void GL_UpdateLightmapTextureRegion32 (gltexture_t *destination, unsigned *data, int width, int height, int xoffset, int yoffset, qboolean mipmap, qboolean alpha)
{
	int			x, y;
	int			realwidth = width + xoffset;
	int			realheight = height + yoffset;
	u16			*dest = (u16 *)destination->data;

	// ELUTODO: mipmaps
	// ELUTODO samples = alpha ? GX_TF_RGBA8 : GX_TF_RGBA8;

	if ((int)destination->data & 31)
		Sys_Error ("GL_Update32: destination->data&31");
	
	for (y = yoffset; y < realheight; y++)
		for (x = xoffset; x < realwidth; x++)
			dest[GX_LinearToTiled(x, y, width)] = GX_RGBA_To_RGB5A3(data[x + y * realwidth], false);
	
	DCFlushRange(destination->data, destination->scaled_width * destination->scaled_height * 2/*sizeof(data)*/);
	GX_InvalidateTexAll();
}

/*
==============================
GL_UpdateLightmapTextureRegion
==============================
*/
void GL_UpdateLightmapTextureRegion (int pic_id, int width, int height, int xoffset, int yoffset, byte *data)
{
	gltexture_t	*destination;

	// see if the texture is allready present
	destination = &gltextures[pic_id];
	
	GL_UpdateLightmapTextureRegion32 (destination, (unsigned *)data, width, height, xoffset, yoffset, false, true);
}

/*
================
GL_LoadPicTexture
================
*/
int GL_LoadPicTexture (qpic_t *pic, char *name)
{
	return GL_LoadTexture (name, pic->width, pic->height, pic->data, false, true, true, 1);
}

// ELUTODO: clean the disable/enable multitexture calls around the engine

void GL_DisableMultitexture(void)
{
	// ELUTODO: we shouldn't need the color atributes for the vertices...

	// setup the vertex descriptor
	// tells the flipper to expect direct data
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);

	GX_SetNumTexGens(1);
	GX_SetNumTevStages(1);
}

void GL_EnableMultitexture(void)
{
	// ELUTODO: we shouldn't need the color atributes for the vertices...

	// setup the vertex descriptor
	// tells the flipper to expect direct data
	GX_ClearVtxDesc();
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_CLR0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GX_SetVtxDesc(GX_VA_TEX1, GX_DIRECT);

	GX_SetNumTexGens(2);
	GX_SetNumTevStages(2);
}

void GL_ClearTextureCache(void)
{
	int i;
	int oldnumgltextures = numgltextures;
	void *newdata;
	u32 texbuffs;
	qboolean mipmap;
	int mip_level;
	int sw, sh;

	numgltextures = 0;

	for (i = 0; i < oldnumgltextures; i++)
	{
		if (gltextures[i].used)
		{
			if (gltextures[i].keep)
			{
				mipmap = gltextures[i].mipmap;
				
				mip_level = 1;
	
				if (mipmap) {
					sw = gltextures[i].scaled_width;
					sh = gltextures[i].scaled_height;
					
					while (sw > 4 && sh > 4)
					{
						sw >>= 1;
						sh >>= 1;
						mip_level++;
					};
				}
				
				texbuffs = GX_GetTexBufferSize	(gltextures[i].scaled_width, gltextures[i].scaled_height, GX_TF_RGB5A3, mipmap ? GX_TRUE : GX_FALSE, mip_level);
				
				numgltextures = i + 1;

				newdata = __lwp_heap_allocate(&texture_heap, texbuffs/*gltextures[i].scaled_width * gltextures[i].scaled_height * sizeof(data)*/);
				if (!newdata)
					Sys_Error("GL_ClearTextureCache: Out of memory.");

				// ELUTODO Pseudo-defragmentation that helps a bit :)
				memcpy(newdata, gltextures[i].data, texbuffs/*gltextures[i].scaled_width * gltextures[i].scaled_height * sizeof(data)*/);

				if (!__lwp_heap_free(&texture_heap, gltextures[i].data))
					Sys_Error("GL_ClearTextureCache: Error freeing data.");

				gltextures[i].data = newdata;
				GX_InitTexObj(&gltextures[i].gx_tex, gltextures[i].data, gltextures[i].scaled_width, gltextures[i].scaled_height, GX_TF_RGB5A3, GX_REPEAT, GX_REPEAT, mipmap ? GX_TRUE : GX_FALSE);

				DCFlushRange(gltextures[i].data, texbuffs/*gltextures[i].scaled_width * gltextures[i].scaled_height * sizeof(data)*/);
			}
			else
			{
				gltextures[i].used = false;
				if (!__lwp_heap_free(&texture_heap, gltextures[i].data))
					Sys_Error("GL_ClearTextureCache: Error freeing data.");
			}
		}
	}

	GX_InvalidateTexAll();
}
/*
//
//
//
//
//
//
*/
int		image_width;
int		image_height;
/*
=================================================================

PCX LOADING

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
    unsigned char	data;			// unbounded
} pcx_t;
/*
============
LoadPCX
============
*/
byte* LoadPCX (char* filename, int matchwidth, int matchheight)
{
	pcx_t	*pcx;
	byte	*palette;
	byte	*out;
	byte	*pix, *image_rgba;
	int		x, y;
	int		dataByte, runLength;
	byte	*pcx_data;
	byte	*pic;

//
// parse the PCX file
//

	// Load the raw data into memory, then store it
    pcx_data = COM_LoadFile(filename, 5);

    pcx = (pcx_t *)pcx_data;
	
	pcx->xmin = LittleShort(pcx->xmin);
    pcx->ymin = LittleShort(pcx->ymin);
    pcx->xmax = LittleShort(pcx->xmax);
    pcx->ymax = LittleShort(pcx->ymax);
    pcx->hres = LittleShort(pcx->hres);
    pcx->vres = LittleShort(pcx->vres);
    pcx->bytes_per_line = LittleShort(pcx->bytes_per_line);
    pcx->palette_type = LittleShort(pcx->palette_type);
	
	if (pcx->manufacturer != 0x0a
		|| pcx->version != 5
		|| pcx->encoding != 1
		|| pcx->bits_per_pixel != 8
		|| pcx->xmax >= 640
		|| pcx->ymax >= 480)
	{
		Con_Printf ("Bad pcx file %i\n");
		return 0;
	}
	
	image_rgba = &pcx->data;

	out = malloc ((pcx->ymax+1)*(pcx->xmax+1)*4);	
	pic = out;
	pix = out;

	if (matchwidth && (pcx->xmax+1) != matchwidth)
		return 0;
	if (matchheight && (pcx->ymax+1) != matchheight)
		return 0;
	
	palette = malloc(768);
	memcpy (palette, (byte *)pcx + com_filesize - 768, 768);
	
	for (y=0 ; y<=pcx->ymax ; y++)
	{
		pix = out + 4*y*(pcx->xmax+1);
		
		for (x=0 ; x<=pcx->xmax ; )
		{
			dataByte = *image_rgba++;

			if((dataByte & 0xC0) == 0xC0)
			{
				runLength = dataByte & 0x3F;
				dataByte = *image_rgba++;
			}
			else
				runLength = 1;

			while(runLength-- > 0) {
				pix[0] = palette[dataByte*3+0];
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
	
	free (palette);
	free (pcx_data);
	return pic;
}

/*small function to read files with stb_image - single-file image loader library.
** downloaded from: https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
** only use jpeg+png formats, because tbh there's not much need for the others.
** */
#define STB_IMAGE_IMPLEMENTATION
#define STBI_FAILURE_USERMSG
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_ONLY_TGA
#define STBI_ONLY_PIC
#include "../../stb_image.h"
byte* loadimagepixels (char* filename, qboolean complain, int matchwidth, int matchheight, int reverseRGBA)
{
	int bpp;
	int width, height;
	
	byte* rgba_data;
	
	// Load the raw data into memory, then store it
    rgba_data = COM_LoadFile(filename, 5);

	if (rgba_data == NULL) {
		Con_Printf("NULL: %s\n", filename);
		return 0;
	}

    byte *image = stbi_load_from_memory(rgba_data, com_filesize, &width, &height, &bpp, 4);
	
	if(image == NULL) {
		Con_Printf("%s\n", stbi_failure_reason());
		return 0;
	}
	
	//set image width/height for texture uploads
	image_width = width;
	image_height = height;

	free(rgba_data);

	return image;
}

int loadtextureimage (char* filename, int matchwidth, int matchheight, qboolean complain, qboolean mipmap, qboolean keep)
{
	int	f = 0;
	int texnum;
	char basename[128], name[128];
	char *texname = malloc(32);
	byte *data;
	byte *c;
	
	if (complain == false)
		COM_StripExtension(filename, basename); // strip the extension to allow TGA
	else
		strcpy(basename, filename);

	c = (byte*)basename;
	while (*c)
	{
		if (*c == '*')
			*c = '+';
		c++;
	}
	
	int len = strlen(basename);
	texname = basename + len - 20;
	
	//Try PCX	
	sprintf (name, "%s.pcx", basename);
	COM_FOpenFile (name, &f);
	if (f > 0) {
		COM_CloseFile (f);
		data = LoadPCX (name, matchwidth, matchheight);
		if (data == 0) {
			Con_Printf("PCX: can't load %s\n", name);	
			return 0;
		}
		texnum = GL_LoadTexture (texname, image_width, image_height, data, false, true, true, 4);		
		free(data);
		return texnum;
	}	
	//Try TGA
	sprintf (name, "%s.tga", basename);
	COM_FOpenFile (name, &f);
	if (f > 0){
		COM_CloseFile (f);
		data = loadimagepixels (name, complain, matchwidth, matchheight, 4);	
		if (data == 0) {
			Con_Printf("TGA: can't load %s\n", name);	
			return 0;
		}
		texnum = GL_LoadTexture (texname, image_width, image_height, data, false, true, keep, 4);
		free(data);
		return texnum;
	}
	//Try PNG
	sprintf (name, "%s.png", basename);
	COM_FOpenFile (name, &f);
	if (f > 0){
		COM_CloseFile (f);
		data = loadimagepixels (name, complain, matchwidth, matchheight, 1);
		if (data == 0) {
			Con_Printf("PNG: can't load %s\n", name);	
			return 0;
		}
		texnum = GL_LoadTexture (texname, image_width, image_height, data, false, true, keep, 4);	
		free(data);
		return texnum;
	}
	//Try JPEG
	sprintf (name, "%s.jpeg", basename);
	COM_FOpenFile (name, &f);
	if (f > 0){
		COM_CloseFile (f);
		data = loadimagepixels (name, complain, matchwidth, matchheight, 1);
		if (data == 0) {
			Con_Printf("JPEG: can't load %s\n", name);	
			return 0;
		}
		texnum = GL_LoadTexture (texname, image_width, image_height, data, false, true, keep, 4);	
		free(data);
		return texnum;
	}
	sprintf (name, "%s.jpg", basename);
	COM_FOpenFile (name, &f);
	if (f > 0){
		COM_CloseFile (f);
		data = loadimagepixels (name, complain, matchwidth, matchheight, 1);
		if (data == 0) {
			Con_Printf("JPG: can't load %s\n", name);	
			return 0;
		}
		texnum = GL_LoadTexture (texname, image_width, image_height, data, false, true, keep, 4);
		free(data);
		return texnum;
	}
	return 0;
}

extern char	skybox_name[32];
extern char skytexname[32];
int loadskyboximage (char* filename, int matchwidth, int matchheight, qboolean complain, qboolean mipmap)
{
	int	f = 0;
	int texnum;
	char basename[128], name[128];
	char *texname = malloc(32);
	
	int image_size = 128 * 128;
	
	byte* data = (byte*)malloc(image_size * 4);
	byte *c;
	
	if (complain == false)
		COM_StripExtension(filename, basename); // strip the extension to allow TGA
	else
		strcpy(basename, filename);

	c = (byte*)basename;
	while (*c)
	{
		if (*c == '*')
			*c = '+';
		c++;
	}
	
	if (strcmp(skybox_name, ""))
		return 0;
//Try PCX

	int len = strlen(basename);
	texname = basename + len - 20;
	
	sprintf (name, "%s.pcx", basename);
	COM_FOpenFile (name, &f);
	if (f > 0) {
		COM_CloseFile (f);
		data = LoadPCX (name, matchwidth, matchheight);	
		texnum = GL_LoadTexture (texname, image_width, image_height, data, false, true, true, 4);		
		free(data);
		return texnum;
	}
	//Try TGA
	sprintf (name, "%s.tga", basename);
	COM_FOpenFile (name, &f);
	if (f > 0){
		COM_CloseFile (f);
		data = loadimagepixels (name, complain, matchwidth, matchheight, 4);	
		texnum = GL_LoadTexture (texname, image_width, image_height, data, false, false, true, 4);		
		free(data);
		return texnum;
	}
	//Try PNG
	sprintf (name, "%s.png", basename);
	COM_FOpenFile (name, &f);
	if (f > 0){
		COM_CloseFile (f);
		data = loadimagepixels (name, complain, matchwidth, matchheight, 1);
		texnum = GL_LoadTexture (texname, image_width, image_height, data, false, false, true, 4);		
		free(data);
		return texnum;
	}
	//Try JPEG
	sprintf (name, "%s.jpeg", basename);
	COM_FOpenFile (name, &f);
	if (f > 0){
		COM_CloseFile (f);
		data = loadimagepixels (name, complain, matchwidth, matchheight, 1);
		texnum = GL_LoadTexture (texname, image_width, image_height, data, false, false, true, 4);
		free(data);
		return texnum;
	}
	sprintf (name, "%s.jpg", basename);
	COM_FOpenFile (name, &f);
	if (f > 0){
		COM_CloseFile (f);
		data = loadimagepixels (name, complain, matchwidth, matchheight, 1);
		texnum = GL_LoadTexture (texname, image_width, image_height, data, false, false, true, 4);		
		free(data);
		return texnum;
	}
	return 0;
}
