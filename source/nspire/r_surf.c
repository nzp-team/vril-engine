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
// r_surf.c: surface-related refresh code

#include "../nzportable_def.h"
#include "r_local.h"

#define NSPIRE_CHEAP_SHOT_SURF 1

drawsurf_t	r_drawsurf;

int				lightleft, sourcesstep, blocksize, sourcetstep;
int				lightdelta, lightdeltastep;
int				lightright, lightleftstep, lightrightstep, blockdivshift;
unsigned		blockdivmask;
void			*prowdestbase;
unsigned char	*pbasesource;
int				surfrowbytes;	// used by ASM files
unsigned		*r_lightptr;
int				r_stepback;
int				r_lightwidth;
int				r_numhblocks, r_numvblocks;
unsigned char	*r_source, *r_sourcemax;

void R_DrawSurfaceBlock8_mip0 (void);
void R_DrawSurfaceBlock8_mip1 (void);
void R_DrawSurfaceBlock8_mip2 (void);
void R_DrawSurfaceBlock8_mip3 (void);


static void	(*surfmiptable[4])(void) = {
	R_DrawSurfaceBlock8_mip0,
	R_DrawSurfaceBlock8_mip1,
	R_DrawSurfaceBlock8_mip2,
	R_DrawSurfaceBlock8_mip3
};


void R_DrawSurfaceBlock8_mip0_aligned_colormap (void);
void R_DrawSurfaceBlock8_mip1_aligned_colormap (void);
void R_DrawSurfaceBlock8_mip2_aligned_colormap (void);
void R_DrawSurfaceBlock8_mip3_aligned_colormap (void);

static void	(*surfmiptable_aligned_colormap[4])(void) = {
	R_DrawSurfaceBlock8_mip0_aligned_colormap,
	R_DrawSurfaceBlock8_mip1_aligned_colormap,
	R_DrawSurfaceBlock8_mip2_aligned_colormap,
	R_DrawSurfaceBlock8_mip3_aligned_colormap
};


unsigned		blocklights[18*18];

/*
===============
R_AddDynamicLights
===============
*/
#define NSPIRE_WHAT_IS_GOING_ON_IN_HERE 1

void R_AddDynamicLights (void)
{
	msurface_t *surf;
	int			lnum;
	int			sd, td;
	float		dist, rad, minlight;
#if NSPIRE_WHAT_IS_GOING_ON_IN_HERE
	fixed16_t   f16_dist, f16_rad, f16_minlight;
	fixed16_t   rgf16_local[ 3 ];
#endif
	vec3_t		impact, local;
	int			s, t;
	int			i;
	int			smax, tmax;
	mtexinfo_t	*tex;

	surf = r_drawsurf.surf;
	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	tex = surf->texinfo;

	for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
	{
		if ( !(surf->dlightbits & (1<<lnum) ) )
			continue;		// not lit by this light

		rad = cl_dlights[lnum].radius;
		dist = DotProduct (cl_dlights[lnum].origin, surf->plane->normal) -
				surf->plane->dist;
		rad -= fabsf(dist);
		minlight = cl_dlights[lnum].minlight;
		if (rad < minlight)
			continue;
		minlight = rad - minlight;

		for (i=0 ; i<3 ; i++)
		{
			impact[i] = cl_dlights[lnum].origin[i] -
					surf->plane->normal[i]*dist;
		}

		local[0] = DotProduct (impact, tex->vecs[0]) + tex->vecs[0][3];
		local[1] = DotProduct (impact, tex->vecs[1]) + tex->vecs[1][3];

		local[0] -= surf->texturemins[0];
		local[1] -= surf->texturemins[1];
		
#if NSPIRE_WHAT_IS_GOING_ON_IN_HERE
		FIXED_FLOATTOFIXED( local[ 0 ], rgf16_local[ 0 ], 8 );
		FIXED_FLOATTOFIXED( local[ 1 ], rgf16_local[ 1 ], 8 );
		FIXED_FLOATTOFIXED( rad, f16_rad, 8 );
		FIXED_FLOATTOFIXED( minlight, f16_minlight, 8 );

		for (t = 0 ; t<tmax ; t++)
		{
			td = rgf16_local[1] - (t<<12);
			if (td < 0)
				td = -td;
			for (s=0 ; s<smax ; s++)
			{
				sd = rgf16_local[0] - (s<<12);
				if (sd < 0)
					sd = -sd;
				if (sd > td)
					f16_dist = sd + (td>>1);
				else
					f16_dist = td + (sd>>1);
				if (f16_dist < f16_minlight)
					blocklights[t*smax + s] += (f16_rad - f16_dist);
			}
		}
#else
		for (t = 0 ; t<tmax ; t++)
		{
			td = local[1] - t*16;
			if (td < 0)
				td = -td;
			for (s=0 ; s<smax ; s++)
			{
				sd = local[0] - s*16;
				if (sd < 0)
					sd = -sd;
				if (sd > td)
					dist = sd + (td>>1);
				else
					dist = td + (sd>>1);
				if (dist < minlight)
#ifdef QUAKE2
				{
					unsigned temp;
					temp = (rad - dist)*256;
					i = t*smax + s;
					if (!cl_dlights[lnum].dark)
						blocklights[i] += temp;
					else
					{
						if (blocklights[i] > temp)
							blocklights[i] -= temp;
						else
							blocklights[i] = 0;
					}
				}
#else
					blocklights[t*smax + s] += (rad - dist)*256;
#endif
			}
		}
#endif
	}
}

/*
===============
R_BuildLightMap

Combine and scale multiple lightmaps into the 8.8 format in blocklights
===============
*/
void R_BuildLightMap (void)
{
	int			smax, tmax;
	int			t;
	int			i, size;
	byte		*lightmap;
	unsigned	scale;
	int			maps;
	msurface_t	*surf;

	surf = r_drawsurf.surf;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	size = smax*tmax;
	lightmap = surf->samples;

	if (r_fullbright.value || !cl.worldmodel->lightdata)
	{
		for (i=0 ; i<size ; i++)
			blocklights[i] = 0;
		return;
	}

// clear to ambient
	for (i=0 ; i<size ; i++)
		blocklights[i] = r_refdef.ambientlight<<8;


// add all the lightmaps
	if (lightmap)
		for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
			 maps++)
		{
			scale = r_drawsurf.lightadj[maps];	// 8.8 fraction		
			for (i=0 ; i<size ; i++)
				blocklights[i] += lightmap[i] * scale;
			lightmap += size;	// skip to next lightmap
		}

// add all the dynamic lights
	if (surf->dlightframe == r_framecount)
		R_AddDynamicLights ();

// bound, invert, and shift
	for (i=0 ; i<size ; i++)
	{
		t = (255*256 - (int)blocklights[i]) >> (8 - VID_CBITS);

		if (t < (1 << 6))
			t = (1 << 6);

		blocklights[i] = t;
	}
}


/*
===============
R_TextureAnimation

Returns the proper texture for a given time and base texture
===============
*/
texture_t *R_TextureAnimation (texture_t *base)
{
	int		reletive;
	int		count;

	if (currententity->frame)
	{
		if (base->alternate_anims)
			base = base->alternate_anims;
	}
	
	if (!base->anim_total)
		return base;

	reletive = (int)(cl.time*10) % base->anim_total;

	count = 0;	
	while (base->anim_min > reletive || base->anim_max <= reletive)
	{
		base = base->anim_next;
		if (!base)
			Sys_Error ("R_TextureAnimation: broken cycle");
		if (++count > 100)
			Sys_Error ("R_TextureAnimation: infinite cycle");
	}

	return base;
}


/*
===============
R_DrawSurface
===============
*/
void R_DrawSurface (void)
{
	unsigned char	*basetptr;
	int				smax, tmax, twidth;
	int				u;
	int				soffset, basetoffset, texwidth;
	int				horzblockstep;
	unsigned char	*pcolumndest;
	void			(*pblockdrawer)(void);
	texture_t		*mt;

// calculate the lightings
	R_BuildLightMap ();
	
	surfrowbytes = r_drawsurf.rowbytes;

	mt = r_drawsurf.texture;
	
	r_source = (byte *)mt + mt->offsets[r_drawsurf.surfmip];
	
// the fractional light values should range from 0 to (VID_GRADES - 1) << 16
// from a source range of 0 - 255
	
	texwidth = mt->width >> r_drawsurf.surfmip;

	blocksize = 16 >> r_drawsurf.surfmip;
	blockdivshift = 4 - r_drawsurf.surfmip;
	blockdivmask = (1 << blockdivshift) - 1;
	
	r_lightwidth = (r_drawsurf.surf->extents[0]>>4)+1;

	r_numhblocks = r_drawsurf.surfwidth >> blockdivshift;
	r_numvblocks = r_drawsurf.surfheight >> blockdivshift;

//==============================

	if (r_pixbytes == 1)
	{
		if( ((int)vid.colormap) & 0xff )
		{
			pblockdrawer = surfmiptable[r_drawsurf.surfmip];
		}
		else
		{
			pblockdrawer = surfmiptable_aligned_colormap[ r_drawsurf.surfmip ];
		}
	// TODO: only needs to be set when there is a display settings change
		horzblockstep = blocksize;
	}
	else
	{
		pblockdrawer = R_DrawSurfaceBlock16;
	// TODO: only needs to be set when there is a display settings change
		horzblockstep = blocksize << 1;
	}

	smax = mt->width >> r_drawsurf.surfmip;
	twidth = texwidth;
	tmax = mt->height >> r_drawsurf.surfmip;
	sourcetstep = texwidth;
	r_stepback = tmax * twidth;

	r_sourcemax = r_source + (tmax * smax);

	soffset = r_drawsurf.surf->texturemins[0];
	basetoffset = r_drawsurf.surf->texturemins[1];

// << 16 components are to guarantee positive values for %
	soffset = ((soffset >> r_drawsurf.surfmip) + (smax << 16)) % smax;
	basetptr = &r_source[((((basetoffset >> r_drawsurf.surfmip) 
		+ (tmax << 16)) % tmax) * twidth)];

	pcolumndest = r_drawsurf.surfdat;

	/*
	{
		extern double d_generictimer;
		double time1 = Sys_FloatTime();
	*/

	for (u=0 ; u<r_numhblocks; u++)
	{
		r_lightptr = blocklights + u;

		prowdestbase = pcolumndest;

		pbasesource = basetptr + soffset;

		(*pblockdrawer)();

		soffset = soffset + blocksize;
		if (soffset >= smax)
			soffset = 0;

		pcolumndest += horzblockstep;
	}
	/*
		d_generictimer += Sys_FloatTime() - time1;
	}
	*/

}


//=============================================================================

#if	!id386

/*
================
R_DrawSurfaceBlock8_mip0
================
*/
void R_DrawSurfaceBlock8_mip0 (void)
{
	int				v, i, b, lightstep, lighttemp, light;
	unsigned char	pix, *psource, *prowdest;
	pixel_t *colormap = vid.colormap;
#if NSPIRE_CHEAP_SHOT_SURF
	int local_lightleft, local_lightright, local_lightleftstep, local_lightrightstep, local_surfrowbytes, local_sourcetstep;
#endif

	psource = pbasesource;
	prowdest = prowdestbase;

	for (v=0 ; v<r_numvblocks ; v++)
	{
	// FIXME: make these locals?
	// FIXME: use delta rather than both right and left, like ASM?
#if !NSPIRE_CHEAP_SHOT_SURF
		lightleft = r_lightptr[0];
		lightright = r_lightptr[1];
		r_lightptr += r_lightwidth;
		lightleftstep = (r_lightptr[0] - lightleft) >> 4;
		lightrightstep = (r_lightptr[1] - lightright) >> 4;
#else
		local_lightleft = r_lightptr[0];
		local_lightright = r_lightptr[1];
		r_lightptr += r_lightwidth;
		local_lightleftstep = (r_lightptr[0] - local_lightleft) >> 4;
		local_lightrightstep = (r_lightptr[1] - local_lightright) >> 4;
		local_surfrowbytes = surfrowbytes;
		local_sourcetstep = sourcetstep;
#endif

		for (i=0 ; i<16 ; i++)
		{
#if !NSPIRE_CHEAP_SHOT_SURF
			lighttemp = ( lightleft - lightright );
			lightstep = lighttemp >> 4;
			lightright += lightrightstep;
			lightleft += lightleftstep;

			light = lightright;

			for (b=15; b>=0; b--)
			{
				pix = psource[b];
				prowdest[b] = ((unsigned char *)colormap)[(light & 0xFF00) + pix];
				light += lightstep;
			}
			psource += sourcetstep;
			prowdest += surfrowbytes;

#else
			lighttemp = ( local_lightright - local_lightleft );
			lightstep = lighttemp >> 4;
			light = local_lightleft;
			local_lightright += local_lightrightstep;
			local_lightleft += local_lightleftstep;

			for (b=0; b < 16; b++)
			{
				pix = psource[b];
				prowdest[b] = ((unsigned char *)colormap)[(light & 0xFF00) + pix];
				light += lightstep;
			}
			psource += local_sourcetstep;
			prowdest += local_surfrowbytes;
#endif
		}

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}


/*
================
R_DrawSurfaceBlock8_mip1
================
*/
void R_DrawSurfaceBlock8_mip1 (void)
{
	int				v, i, b, lightstep, lighttemp, light;
	unsigned char	pix, *psource, *prowdest;
	pixel_t *colormap = vid.colormap;
#if NSPIRE_CHEAP_SHOT_SURF
	int local_lightleft, local_lightright, local_lightleftstep, local_lightrightstep, local_surfrowbytes, local_sourcetstep;
#endif

	psource = pbasesource;
	prowdest = prowdestbase;

	for (v=0 ; v<r_numvblocks ; v++)
	{
#if !NSPIRE_CHEAP_SHOT_SURF
	// FIXME: make these locals?
	// FIXME: use delta rather than both right and left, like ASM?

		lightleft = r_lightptr[0];
		lightright = r_lightptr[1];
		r_lightptr += r_lightwidth;
		lightleftstep = (r_lightptr[0] - lightleft) >> 3;
		lightrightstep = (r_lightptr[1] - lightright) >> 3;
#else
		local_lightleft = r_lightptr[0];
		local_lightright = r_lightptr[1];
		r_lightptr += r_lightwidth;
		local_lightleftstep = (r_lightptr[0] - local_lightleft) >> 3;
		local_lightrightstep = (r_lightptr[1] - local_lightright) >> 3;
		local_surfrowbytes = surfrowbytes;
		local_sourcetstep = sourcetstep;
#endif

		for (i=0 ; i<8 ; i++)
		{
#if !NSPIRE_CHEAP_SHOT_SURF
			lighttemp = ( lightleft - lightright );
			lightstep = lighttemp >> 3;
			lightright += lightrightstep;
			lightleft += lightleftstep;

			light = lightright;

			for (b=7; b>=0; b--)
			{
				pix = psource[b];
				prowdest[b] = ((unsigned char *)colormap)[(light & 0xFF00) + pix];
				light += lightstep;
			}
			psource += sourcetstep;
			prowdest += surfrowbytes;

#else
			lighttemp = ( local_lightright - local_lightleft );
			lightstep = lighttemp >> 3;
			light = local_lightleft;
			local_lightright += local_lightrightstep;
			local_lightleft += local_lightleftstep;

			for (b=0; b < 8; b++)
			{
				pix = psource[b];
				prowdest[b] = ((unsigned char *)colormap)[(light & 0xFF00) + pix];
				light += lightstep;
			}
			psource += local_sourcetstep;
			prowdest += local_surfrowbytes;
#endif
		}

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}


/*
================
R_DrawSurfaceBlock8_mip2
================
*/
void R_DrawSurfaceBlock8_mip2 (void)
{
	int				v, i, b, lightstep, lighttemp, light;
	unsigned char	pix, *psource, *prowdest;
	pixel_t *colormap = vid.colormap;
#if NSPIRE_CHEAP_SHOT_SURF
	int local_lightleft, local_lightright, local_lightleftstep, local_lightrightstep, local_surfrowbytes, local_sourcetstep;
#endif

	psource = pbasesource;
	prowdest = prowdestbase;

	for (v=0 ; v<r_numvblocks ; v++)
	{
#if !NSPIRE_CHEAP_SHOT_SURF
	// FIXME: make these locals?
	// FIXME: use delta rather than both right and left, like ASM?

		lightleft = r_lightptr[0];
		lightright = r_lightptr[1];
		r_lightptr += r_lightwidth;
		lightleftstep = (r_lightptr[0] - lightleft) >> 2;
		lightrightstep = (r_lightptr[1] - lightright) >> 2;
#else
		local_lightleft = r_lightptr[0];
		local_lightright = r_lightptr[1];
		r_lightptr += r_lightwidth;
		local_lightleftstep = (r_lightptr[0] - local_lightleft) >> 2;
		local_lightrightstep = (r_lightptr[1] - local_lightright) >> 2;
		local_surfrowbytes = surfrowbytes;
		local_sourcetstep = sourcetstep;
#endif

		for (i=0 ; i<4 ; i++)
		{
#if !NSPIRE_CHEAP_SHOT_SURF
			lighttemp = ( lightleft - lightright );
			lightstep = lighttemp >> 2;
			lightright += lightrightstep;
			lightleft += lightleftstep;

			light = lightright;

			for (b=3; b>=0; b--)
			{
				pix = psource[b];
				prowdest[b] = ((unsigned char *)colormap)[(light & 0xFF00) + pix];
				light += lightstep;
			}
			psource += sourcetstep;
			prowdest += surfrowbytes;

#else
			lighttemp = ( local_lightright - local_lightleft );
			lightstep = lighttemp >> 2;
			light = local_lightleft;
			local_lightright += local_lightrightstep;
			local_lightleft += local_lightleftstep;

			for (b=0; b < 4; b++)
			{
				pix = psource[b];
				prowdest[b] = ((unsigned char *)colormap)[(light & 0xFF00) + pix];
				light += lightstep;
			}
			psource += local_sourcetstep;
			prowdest += local_surfrowbytes;
#endif
		}

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}


/*
================
R_DrawSurfaceBlock8_mip3
================
*/
void R_DrawSurfaceBlock8_mip3 (void)
{
	int				v, i, b, lightstep, lighttemp, light;
	unsigned char	pix, *psource, *prowdest;
	pixel_t *colormap = vid.colormap;
#if NSPIRE_CHEAP_SHOT_SURF
	int local_lightleft, local_lightright, local_lightleftstep, local_lightrightstep, local_surfrowbytes, local_sourcetstep;
#endif

	psource = pbasesource;
	prowdest = prowdestbase;

	for (v=0 ; v<r_numvblocks ; v++)
	{
#if !NSPIRE_CHEAP_SHOT_SURF
	// FIXME: make these locals?
	// FIXME: use delta rather than both right and left, like ASM?

		lightleft = r_lightptr[0];
		lightright = r_lightptr[1];
		r_lightptr += r_lightwidth;
		lightleftstep = (r_lightptr[0] - lightleft) >> 1;
		lightrightstep = (r_lightptr[1] - lightright) >> 1;
#else
		local_lightleft = r_lightptr[0];
		local_lightright = r_lightptr[1];
		r_lightptr += r_lightwidth;
		local_lightleftstep = (r_lightptr[0] - local_lightleft) >> 1;
		local_lightrightstep = (r_lightptr[1] - local_lightright) >> 1;
		local_surfrowbytes = surfrowbytes;
		local_sourcetstep = sourcetstep;
#endif


		for (i=0 ; i<2 ; i++)
		{
#if !NSPIRE_CHEAP_SHOT_SURF
			lighttemp = ( lightleft - lightright );
			lightstep = lighttemp >> 1;
			lightright += lightrightstep;
			lightleft += lightleftstep;

			light = lightright;

			for (b=1; b>=0; b--)
			{
				pix = psource[b];
				prowdest[b] = ((unsigned char *)colormap)[(light & 0xFF00) + pix];
				light += lightstep;
			}
			psource += sourcetstep;
			prowdest += surfrowbytes;

#else
			lighttemp = ( local_lightright - local_lightleft );
			lightstep = lighttemp >> 1;
			light = local_lightleft;
			local_lightright += local_lightrightstep;
			local_lightleft += local_lightleftstep;

			for (b=0; b < 2; b++)
			{
				pix = psource[b];
				prowdest[b] = ((unsigned char *)colormap)[(light & 0xFF00) + pix];
				light += lightstep;
			}
			psource += local_sourcetstep;
			prowdest += local_surfrowbytes;
#endif
		}

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}


/*
================
R_DrawSurfaceBlock16

FIXME: make this work
================
*/
void R_DrawSurfaceBlock16 (void)
{
	int				k;
	unsigned char	*psource;
	int				lighttemp, lightstep, light;
	unsigned short	*prowdest;

	prowdest = (unsigned short *)prowdestbase;

	for (k=0 ; k<blocksize ; k++)
	{
		unsigned short	*pdest;
		unsigned char	pix;
		int				b;

		psource = pbasesource;
		lighttemp = lightright - lightleft;
		lightstep = lighttemp >> blockdivshift;

		light = lightleft;
		pdest = prowdest;

		for (b=0; b<blocksize; b++)
		{
			pix = *psource;
			*pdest = vid.colormap16[(light & 0xFF00) + pix];
			psource += sourcesstep;
			pdest++;
			light += lightstep;
		}

		pbasesource += sourcetstep;
		lightright += lightrightstep;
		lightleft += lightleftstep;
		prowdest = (unsigned short *)((long)prowdest + surfrowbytes);
	}

	prowdestbase = prowdest;
}




void R_DrawSurfaceBlock8_mip0_aligned_colormap (void)
{
	int				v, i, b, lightstep, lighttemp, light;
	unsigned char	pix, *psource, *prowdest;
	pixel_t *colormap = vid.colormap;
	byte *light_ptr;
	int local_lightleft, local_lightright, local_lightleftstep, local_lightrightstep, local_surfrowbytes, local_sourcetstep;

	psource = pbasesource;
	prowdest = prowdestbase;

	for (v=0 ; v<r_numvblocks ; v++)
	{
		local_lightleft = r_lightptr[0] & 0xffff;
		local_lightright = r_lightptr[1] & 0xffff;
		r_lightptr += r_lightwidth;
		local_lightleftstep = ( ( (int)r_lightptr[0] & 0xffff ) - local_lightleft) >> 4;
		local_lightrightstep = ( ( (int)r_lightptr[1] & 0xffff ) - local_lightright) >> 4;
		local_surfrowbytes = surfrowbytes;
		local_sourcetstep = sourcetstep;

		for (i=0 ; i<16 ; i++)
		{
			lighttemp = ( local_lightright - local_lightleft );
			lightstep = lighttemp >> 4;
			light = local_lightleft;
			local_lightright += local_lightrightstep;
			local_lightleft += local_lightleftstep;

			light_ptr = ( colormap + light );

			for (b=0; b < 16; b++)
			{
				pix = psource[b];
				prowdest[b] = ((unsigned char *)((( size_t )light_ptr)&~0xff))[pix];
				light_ptr += lightstep;
			}
			psource += local_sourcetstep;
			prowdest += local_surfrowbytes;
		}

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}

void R_DrawSurfaceBlock8_mip1_aligned_colormap (void)
{
	int				v, i, b, lightstep, lighttemp, light;
	unsigned char	pix, *psource, *prowdest;
	pixel_t *colormap = vid.colormap;
	byte *light_ptr;
	int local_lightleft, local_lightright, local_lightleftstep, local_lightrightstep, local_surfrowbytes, local_sourcetstep;

	psource = pbasesource;
	prowdest = prowdestbase;

	for (v=0 ; v<r_numvblocks ; v++)
	{
		local_lightleft = r_lightptr[0] & 0xffff;
		local_lightright = r_lightptr[1] & 0xffff;
		r_lightptr += r_lightwidth;
		local_lightleftstep = ( ( (int)r_lightptr[0] & 0xffff ) - local_lightleft) >> 3;
		local_lightrightstep = ( ( (int)r_lightptr[1] & 0xffff ) - local_lightright) >> 3;
		local_surfrowbytes = surfrowbytes;
		local_sourcetstep = sourcetstep;

		for (i=0 ; i<8 ; i++)
		{
			lighttemp = ( local_lightright - local_lightleft );
			lightstep = lighttemp >> 3;
			light = local_lightleft;
			local_lightright += local_lightrightstep;
			local_lightleft += local_lightleftstep;

			light_ptr = ( colormap + light );

			for (b=0; b < 8; b++)
			{
				pix = psource[b];
				prowdest[b] = ((unsigned char *)((( size_t )light_ptr)&~0xff))[pix];
				light_ptr += lightstep;
			}
			psource += local_sourcetstep;
			prowdest += local_surfrowbytes;
		}

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}

void R_DrawSurfaceBlock8_mip2_aligned_colormap (void)
{
	int				v, i, b, lightstep, lighttemp, light;
	unsigned char	pix, *psource, *prowdest;
	pixel_t *colormap = vid.colormap;
	byte *light_ptr;
	int local_lightleft, local_lightright, local_lightleftstep, local_lightrightstep, local_surfrowbytes, local_sourcetstep;

	psource = pbasesource;
	prowdest = prowdestbase;

	for (v=0 ; v<r_numvblocks ; v++)
	{
		local_lightleft = r_lightptr[0] & 0xffff;
		local_lightright = r_lightptr[1] & 0xffff;
		r_lightptr += r_lightwidth;
		local_lightleftstep = ( ( (int)r_lightptr[0] & 0xffff ) - local_lightleft) >> 2;
		local_lightrightstep = ( ( (int)r_lightptr[1] & 0xffff ) - local_lightright) >> 2;
		local_surfrowbytes = surfrowbytes;
		local_sourcetstep = sourcetstep;

		for (i=0 ; i<4 ; i++)
		{
			lighttemp = ( local_lightright - local_lightleft );
			lightstep = lighttemp >> 2;
			light = local_lightleft;
			local_lightright += local_lightrightstep;
			local_lightleft += local_lightleftstep;

			light_ptr = ( colormap + light );

			for (b=0; b < 4; b++)
			{
				pix = psource[b];
				prowdest[b] = ((unsigned char *)((( size_t )light_ptr)&~0xff))[pix];
				light_ptr += lightstep;
			}
			psource += local_sourcetstep;
			prowdest += local_surfrowbytes;
		}

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}

void R_DrawSurfaceBlock8_mip3_aligned_colormap (void)
{
	int				v, i, b, lightstep, lighttemp, light;
	unsigned char	pix, *psource, *prowdest;
	pixel_t *colormap = vid.colormap;
	byte *light_ptr;
	int local_lightleft, local_lightright, local_lightleftstep, local_lightrightstep, local_surfrowbytes, local_sourcetstep;

	psource = pbasesource;
	prowdest = prowdestbase;

	for (v=0 ; v<r_numvblocks ; v++)
	{
		local_lightleft = r_lightptr[0] & 0xffff;
		local_lightright = r_lightptr[1] & 0xffff;
		r_lightptr += r_lightwidth;
		local_lightleftstep = ( ( (int)r_lightptr[0] & 0xffff ) - local_lightleft) >> 1;
		local_lightrightstep = ( ( (int)r_lightptr[1] & 0xffff ) - local_lightright) >> 1;
		local_surfrowbytes = surfrowbytes;
		local_sourcetstep = sourcetstep;

		for (i=0 ; i<2 ; i++)
		{
			lighttemp = ( local_lightright - local_lightleft );
			lightstep = lighttemp >> 1;
			light = local_lightleft;
			local_lightright += local_lightrightstep;
			local_lightleft += local_lightleftstep;

			light_ptr = ( colormap + light );

			for (b=0; b < 2; b++)
			{
				pix = psource[b];
				prowdest[b] = ((unsigned char *)((( size_t )light_ptr)&~0xff))[pix];
				light_ptr += lightstep;
			}
			psource += local_sourcetstep;
			prowdest += local_surfrowbytes;
		}

		if (psource >= r_sourcemax)
			psource -= r_stepback;
	}
}


#endif


//============================================================================

/*
================
R_GenTurbTile
================
*/
void R_GenTurbTile (pixel_t *pbasetex, void *pdest)
{
	int		*turb;
	int		i, j, s, t;
	byte	*pd;
	
	turb = sintable + ((int)(cl.time*SPEED)&(CYCLE-1));
	pd = (byte *)pdest;

	for (i=0 ; i<TILE_SIZE ; i++)
	{
		for (j=0 ; j<TILE_SIZE ; j++)
		{	
			s = (((j << 16) + turb[i & (CYCLE-1)]) >> 16) & 63;
			t = (((i << 16) + turb[j & (CYCLE-1)]) >> 16) & 63;
			*pd++ = *(pbasetex + (t<<6) + s);
		}
	}
}


/*
================
R_GenTurbTile16
================
*/
void R_GenTurbTile16 (pixel_t *pbasetex, void *pdest)
{
	int				*turb;
	int				i, j, s, t;
	unsigned short	*pd;

	turb = sintable + ((int)(cl.time*SPEED)&(CYCLE-1));
	pd = (unsigned short *)pdest;

	for (i=0 ; i<TILE_SIZE ; i++)
	{
		for (j=0 ; j<TILE_SIZE ; j++)
		{	
			s = (((j << 16) + turb[i & (CYCLE-1)]) >> 16) & 63;
			t = (((i << 16) + turb[j & (CYCLE-1)]) >> 16) & 63;
			*pd++ = d_8to16table[*(pbasetex + (t<<6) + s)];
		}
	}
}


/*
================
R_GenTile
================
*/
void R_GenTile (msurface_t *psurf, void *pdest)
{
	if (psurf->flags & SURF_DRAWTURB)
	{
		if (r_pixbytes == 1)
		{
			R_GenTurbTile ((pixel_t *)
				((byte *)psurf->texinfo->texture + psurf->texinfo->texture->offsets[0]), pdest);
		}
		else
		{
			R_GenTurbTile16 ((pixel_t *)
				((byte *)psurf->texinfo->texture + psurf->texinfo->texture->offsets[0]), pdest);
		}
	}
	else if (psurf->flags & SURF_DRAWSKY)
	{
		if (r_pixbytes == 1)
		{
			R_GenSkyTile (pdest);
		}
		else
		{
			R_GenSkyTile16 (pdest);
		}
	}
	else
	{
		Sys_Error ("Unknown tile type");
	}
}

