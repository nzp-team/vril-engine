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
#include <ogc/machine/processor.h>

#include "../../nzportable_def.h"

#include <gccore.h>
#include <malloc.h>

// ELUTODO: mipmap

cvar_t		gl_max_size = {"gl_max_size", "512"};
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
	texture_heap_size = 13 * 1024 * 1024;
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

void GL_Bind (int texnum)
{
	if (!gltextures[texnum].used)
		Sys_Error("Tried to bind a inactive texture0.");

	currenttexture0 = texnum;
	GX_LoadTexObj(&gltextures[texnum].gx_tex, GX_TEXMAP0);
}

void GX_SetMinMag (int minfilt, int magfilt)
{
	if(gltextures[currenttexture0].data != NULL) {
		GX_InitTexObjFilterMode(&gltextures[currenttexture0].gx_tex, minfilt, magfilt);
	};
}

void GX_SetMaxAniso (int aniso)
{
	if(gltextures[currenttexture0].data != NULL) {
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

	for (i=0, glt=gltextures ; i<numgltextures ; i++, glt++) {
		if (glt->used) {
			if (!strcmp (identifier, glt->identifier)) {
				return glt->texnum;
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

int GX_RGBA_To_RGB5A3(u32 srccolor, qboolean flip)
{
	u16 color;

	u32 r, g, b, a;
	if (flip) {
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
	
	if (a > 0xe0) {
		r = r >> 3;
		g = g >> 3;
		b = b >> 3;

		color = (r << 10) | (g << 5) | b;
		color |= 0x8000;
	} else {
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

int _calc_mipmap_level(int width, int height) 
{
	int max_mip;

	while (width > 4 && height > 4)
	{
		width >>= 1;
		height >>= 1;
		max_mip++;
	};
	
	if (max_mip != 0) {
		// account for memory offset
		max_mip += 1; 
	}

	return max_mip;
}
*/
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
	//int sw = 0;
	//int sh = 0;
	// start at mip level 0
	//int max_mip_level = 0;
	u32 tex_buffersize;

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
	
	/*
	if (mipmap) {
		sw = scaled_width;
		sh = scaled_height;
		
		max_mip_level = _calc_mipmap_level(sw, sh);
	}
	*/

	//get exact buffer size of memory aligned on a 32byte boundery
	//tex_buffersize = GX_GetTexBufferSize (scaled_width, scaled_height, GX_TF_RGB5A3, mipmap ? GX_TRUE : GX_FALSE, max_mip_level);
	tex_buffersize = GX_GetTexBufferSize (scaled_width, scaled_height, GX_TF_RGB5A3, 0, 0);
	//destination->data = memalign(32, tex_buffersize);
	destination->data = __lwp_heap_allocate(&texture_heap, tex_buffersize);	

	if (developer.value == 4) {
		heap_iblock info;
		__lwp_heap_getinfo(&texture_heap, &info);

		printf("identifier: %s\n", destination->identifier);
		printf("tex buff size %d\n", tex_buffersize);
		printf("used heap: %dM\n", info.used_size / (1024*1024));
		printf("\n");
	}

	if (!destination->data)
		Sys_Error("GL_Upload32: Out of memory.");
	
	s = scaled_width * scaled_height;
	if (s & 31)
		Sys_Error ("GL_Upload32: s&31");

	if ((int)destination->data & 31)
		Sys_Error ("GL_Upload32: destination->data&31");
	
	destination->scaled_width = scaled_width;
	destination->scaled_height = scaled_height;

	GX_CopyRGBA8_To_RGB5A3((u16 *)destination->data, scaled, 0, 0, scaled_width, scaled_height, scaled_width, flipRGBA);
	
	//
	// sBTODO finish mipmap implementation 
	//
	/*
	if (mipmap == true) {
		
		int	mip_level;
		unsigned mipmaptex[640*480];
		
		// copy texture to new buffer
		memcpy((void *)mipmaptex, scaled, scaled_width * scaled_height * 4);
		
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
			DCFlushRange(dst_addr, sw * sh * 4);
			GX_InitTexObj(&destination->gx_tex, dst_addr, sw, sh, GX_TF_RGB5A3, GX_REPEAT, GX_REPEAT, GX_TRUE);
			GX_InitTexObjLOD(&destination->gx_tex, GX_LIN_MIP_LIN, GX_LIN_MIP_LIN, mip_level, max_mip_level, 0, GX_ENABLE, GX_ENABLE, GX_ANISO_2);	
		}
		
		GX_InitTexObj(&destination->gx_tex, destination->data, scaled_width, scaled_height, GX_TF_RGB5A3, GX_REPEAT, GX_REPEAT, GX_TRUE);
		GX_InitTexObjLOD(&destination->gx_tex, GX_LIN_MIP_LIN, GX_LIN_MIP_LIN, 0, max_mip_level, 0, GX_ENABLE, GX_ENABLE, GX_ANISO_2);
		DCFlushRange(destination->data, texbuffs);
		GX_InvalidateTexAll();			

		if (vid_retromode.value == 1) {
			GX_InitTexObjFilterMode(&destination->gx_tex, GX_NEAR_MIP_NEAR, GX_NEAR_MIP_NEAR);
		} else {
			GX_InitTexObjFilterMode(&destination->gx_tex, GX_LIN_MIP_LIN, GX_LIN_MIP_LIN);
		}
		
	} else {*/
		DCFlushRange(destination->data, tex_buffersize);
		GX_InvalidateTexAll();
		GX_InitTexObj(&destination->gx_tex, destination->data, scaled_width, scaled_height, GX_TF_RGB5A3, GX_REPEAT, GX_REPEAT, GX_FALSE);
	//}
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
int GL_LoadTexture (char *identifier, int width, int height, byte *data, qboolean mipmap, qboolean alpha, qboolean keep, int bytesperpixel)
{
	int i;
	gltexture_t	*glt;

	// Out of textures?
	if (numgltextures == MAX_GLTEXTURES)
		Sys_Error ("GL_GetTextureIndex: Out of GL textures\n");

	int texture_index = GL_FindTexture(identifier);
	if (texture_index > 0) {
		return texture_index;
	}

	numgltextures++;

	for (i = 0, glt=gltextures; i < numgltextures; i++, glt++) {
		if (!glt->used) {
			texture_index = i;
			break;
		}
	}

	if (texture_index < 0)
		Sys_Error("Could not find a free GL texture! %i %s\n", numgltextures, identifier);

	strcpy (glt->identifier, identifier);
	glt->texnum = texture_index;
	glt->width = width;
	glt->height = height;
	glt->mipmap = mipmap;
	glt->type = 0;
	glt->keep = keep;
	glt->used = true;
	/*
	printf("\n");
	printf("tex index %i numgl %i name %s\n", glt->texnum, numgltextures, identifier);
	printf("gltexture[i] = %i\n", gltextures[texture_index].texnum);
	printf("gltextures[i].identifier  = %s\n", gltextures[texture_index].identifier);
	printf("\n");
	*/
	GL_Bind (glt->texnum);
	if (bytesperpixel == 1) {
		GL_Upload8 (glt, data, width, height, mipmap, alpha);
	}
	else if (bytesperpixel == 4) {
		GL_Upload32 (glt, (unsigned*)data, width, height, mipmap, alpha, false);
	}
	else {
		Sys_Error("GL_LoadTexture: unknown bytesperpixel\n");
	}

	return glt->texnum;
}

/*
======================
GL_LoadLightmapTexture
======================
*/
int GL_LoadLightmapTexture (char *identifier, int width, int height, byte *data, int lightmap_textures)
{
	gltexture_t	*glt;

	// They need to be allocated sequentially
	if (lightmap_textures == MAX_GLTEXTURES)
		Sys_Error ("GL_LoadLightmapTexture: numgltextures == MAX_GLTEXTURES\n");

	glt = &gltextures[lightmap_textures];
	strcpy (glt->identifier, identifier);
	//Con_Printf("Identifier: %s", identifier);
	glt->texnum = lightmap_textures;
	glt->width = width;
	glt->height = height;
	glt->mipmap = false; // ELUTODO
	glt->type = 1;
	glt->keep = false;
	glt->used = true;

	GL_Upload32 (glt, (unsigned *)data, width, height, false, false, false);

	if (width != glt->scaled_width || height != glt->scaled_height)
		Sys_Error("GL_LoadLightmapTexture: Tried to scale lightmap\n");

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
	
	DCFlushRange(destination->data, destination->scaled_width * destination->scaled_height * 4/*sizeof(data)*/);
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

void GL_ClearTextureCache(void)
{
	int i;
	int oldnumgltextures = numgltextures;
	void *newdata;
	u32 tex_buffersize;
	//qboolean mipmap;
	//int mip_level;
	//int sw, sh;

	numgltextures = 0;

	for (i = 0; i < oldnumgltextures; i++) {
		if (gltextures[i].used) {
			if (gltextures[i].keep) {
				/*
				mipmap = gltextures[i].mipmap;
				
				mip_level = 0;
		
				if (mipmap) {
					sw = gltextures[i].scaled_width;
					sh = gltextures[i].scaled_height;
					
					mip_level = _calc_mipmap_level(sw, sh);
				}
				*/
				//tex_buffersize = GX_GetTexBufferSize	(gltextures[i].scaled_width, gltextures[i].scaled_height, GX_TF_RGB5A3, mipmap ? GX_TRUE : GX_FALSE, mip_level);
				
				tex_buffersize = GX_GetTexBufferSize	(gltextures[i].scaled_width, gltextures[i].scaled_height, GX_TF_RGB5A3, 0, 0);
				numgltextures = i + 1;
		
				//newdata = memalign(32, tex_buffersize);
				newdata = __lwp_heap_allocate(&texture_heap, tex_buffersize);
				if (!newdata)
					Sys_Error("GL_ClearTextureCache: Out of memory.");
		
				// ELUTODO Pseudo-defragmentation that helps a bit :)
				memcpy(newdata, gltextures[i].data, tex_buffersize);

				if (!__lwp_heap_free(&texture_heap, gltextures[i].data))
					Sys_Error("GL_ClearTextureCache: Error freeing data.");
				
				//gltextures[i].data = memalign(32, tex_buffersize);
				gltextures[i].data = newdata;
				DCFlushRange(gltextures[i].data, tex_buffersize);
				GX_InitTexObj(&gltextures[i].gx_tex, gltextures[i].data, gltextures[i].scaled_width, gltextures[i].scaled_height, GX_TF_RGB5A3, GX_REPEAT, GX_REPEAT, GX_FALSE);		
			} else {

				gltextures[i].used = false;

				if (!__lwp_heap_free(&texture_heap, gltextures[i].data))
					Sys_Error("GL_ClearTextureCache: Error freeing data.");
			}
		}
	}

	GX_InvalidateTexAll();
}