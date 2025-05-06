/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2007 Peter Mackay and Chris Swindle.

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

#include <pspgu.h>
#include <pspgum.h>

extern "C"
{
#include "../../nzportable_def.h"
}

#ifdef PSP_VFPU
#include <pspmath.h>
#endif

extern int LIGHTMAP_BYTES;

#include "../clipping.hpp"
#include "gu_fullbright.h"

using namespace quake;

int			skytexturenum;
int 		last_lightmap_allocated; // ericw -- optimization: remember the index of the last lightmap AllocBlock stored a surf in

#define	BLOCK_WIDTH  128
#define	BLOCK_HEIGHT 128

int		lightmap_textures;
unsigned    blocklights[BLOCK_WIDTH*BLOCK_HEIGHT*3]; // LordHavoc: .lit support (*3 for RGB)


int		active_lightmaps;


typedef struct glRect_s
{
	unsigned char l,t,w,h;
} glRect_t;

//////////////////////////////////////////////////////////////////////////////
//For none .lit maps./////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
qboolean	lightmap_modified[MAX_LIGHTMAPS];
glRect_t	lightmap_rectchange[MAX_LIGHTMAPS];

#define MAX_VISIBLE_LIGHTMAPPED_FACES 6000
// contains references to every msurface_t that's getting rendered.
// in testing 512 is not enough for bigger complicated areas (like parts of dm2).
// consider doubling to 2048 or somewhere inbetween.
// when the limit gets exceeded, there will be some surfaces without lightmaps.
lightmap_face_t lightmap_faces[MAX_VISIBLE_LIGHTMAPPED_FACES];
// chains of lightmap faces for each lightmap num
lightmap_face_t	*lightmap_chains[MAX_LIGHTMAPS];

int num_lightmapped_faces;

int			allocated[MAX_LIGHTMAPS][BLOCK_WIDTH];

// the lightmap texture data needs to be kept in
// main memory so texsubimage can update properly
byte		lightmaps[1*MAX_LIGHTMAPS*BLOCK_WIDTH*BLOCK_HEIGHT];
int 		lightmap_index[MAX_LIGHTMAPS];

// For gl_texsort 0
msurface_t  *skychain = NULL;
msurface_t  *waterchain = NULL;
glpoly_t	*caustics_polys = NULL;
glpoly_t	*detail_polys   = NULL;

void 	VID_SetPaletteLM();
// switch palette for lightmaps

void 	VID_SetPaletteTX();
// switch palette for textures

/*
===============
R_AddDynamicLights
===============
*/
void R_AddDynamicLights (msurface_t *surf)
{
	int			lnum;
	int			sd, td;
	float		dist, rad, minlight;
	vec3_t		impact, local;
	int			s, t;
	int			i;
	int			smax, tmax;
	mtexinfo_t	*tex;

	// LordHavoc: .lit support begin
	float		cred, cgreen, cblue, brightness;
	unsigned	*bl;
	// LordHavoc: .lit support end

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

		// LordHavoc: .lit support begin
		bl = blocklights;
		cred = cl_dlights[lnum].color[0] * 256.0f;
		cgreen = cl_dlights[lnum].color[1] * 256.0f;
		cblue = cl_dlights[lnum].color[2] * 256.0f;
		// LordHavoc: .lit support end
		for (t = 0 ; t<tmax ; t++)
		{
			td = int(local[1]) - t*16;
			if (td < 0)
				td = -td;
			for (s=0 ; s<smax ; s++)
			{
				sd = int(local[0]) - s*16;
				if (sd < 0)
					sd = -sd;
				if (sd > td)
					dist = sd + (td>>1);
				else
					dist = td + (sd>>1);
				if (dist < minlight)
				// LordHavoc: .lit support begin
				//	blocklights[t*smax + s] += (rad - dist)*256; // LordHavoc: original code
				{

					brightness = rad - dist;
					if (!cl_dlights[lnum].dark)
					{
						bl[0] += (int) (brightness * cred);
					    bl[1] += (int) (brightness * cgreen);
					    bl[2] += (int) (brightness * cblue);
					}
					else
				    {
					  if(bl[0] > (int) (brightness * cred))
                         bl[0] -= (int) (brightness * cred);
                      else
						 bl[0] = 0;

					  if(bl[1] > (int) (brightness * cgreen))
                         bl[1] -= (int) (brightness * cgreen);
                      else
						 bl[1] = 0;

					  if(bl[2] > (int) (brightness * cblue))
                         bl[2] -= (int) (brightness * cblue);
					  else
						 bl[2] = 0;
					}
				 /*
					brightness = rad - dist;
					bl[0] += (int) (brightness * cred);
					bl[1] += (int) (brightness * cgreen);
					bl[2] += (int) (brightness * cblue);
				  */
				}
				bl += 3;
				// LordHavoc: .lit support end
			}
		}
	}
}


/*
===============
R_BuildLightMap

Combine and scale multiple lightmaps into the 8.8 format in blocklights
===============
*/
void R_BuildLightMap (msurface_t *surf, byte *dest, int stride)
{
	int			smax, tmax;
	int			t;
	int			i, j, size;
	byte		*lightmap;
	unsigned	scale;
	int			maps;
	unsigned	*bl;
	int r, g, b, a;

    //unsigned *blcr, *blcg, *blcb;

	surf->cached_dlight = (surf->dlightframe == r_framecount) ? qtrue : qfalse;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	size = smax*tmax;
	lightmap = surf->samples;

// set to full bright if no light data
	if (r_fullbright.value || !cl.worldmodel->lightdata)
	{
		// LordHavoc: .lit support begin
		bl = blocklights;
		for (i=0 ; i<size ; i++) // LordHavoc: original code
		//	blocklights[i] = 255*256; // LordHavoc: original code
		{
			*bl++ = 255*256;
			*bl++ = 255*256;
			*bl++ = 255*256;
		}
		// LordHavoc: .lit support end
		goto store;
	}

// clear to no light
	// LordHavoc: .lit support begin
	bl = blocklights;
	for (i=0 ; i<size ; i++) // LordHavoc: original code
	//	blocklights[i] = 0; // LordHavoc: original code
	{
		*bl++ = 0;
		*bl++ = 0;
		*bl++ = 0;
	}
	// LordHavoc: .lit support end

// add all the lightmaps
	if (lightmap)
		for (maps = 0 ; maps < MAXLIGHTMAPS && surf->styles[maps] != 255 ;
			 maps++)
		{
			scale = d_lightstylevalue[surf->styles[maps]];
			surf->cached_light[maps] = scale;	// 8.8 fraction
			// LordHavoc: .lit support begin
			bl = blocklights;
			for (i=0 ; i<size ; i++) // LordHavoc: original code
			//	blocklights[i] += lightmap[i] * scale; // LordHavoc: original code
			//lightmap += size;	// skip to next lightmap // LordHavoc: original code
			{
				*bl++ += *lightmap++ * scale;
				*bl++ += *lightmap++ * scale;
				*bl++ += *lightmap++ * scale;
			}
			// LordHavoc: .lit support end
		}

// add all the dynamic lights
	if (surf->dlightframe == r_framecount)
		R_AddDynamicLights (surf);

// bound, invert, and shift
store:
	switch (LIGHTMAP_BYTES)
	{
	case 4:
		stride -= (smax<<2);
		bl = blocklights;
		for (i=0 ; i<tmax ; i++, dest += stride)
		{
			for (j=0 ; j<smax ; j++)
			{
				// LordHavoc: .lit support begin
				// LordHavoc: positive lighting (would be 255-t if it were inverse like glquake was)
				t = *bl++ >> 7;if (t > 255) t = 255;*dest++ = t;
				t = *bl++ >> 7;if (t > 255) t = 255;*dest++ = t;
				t = *bl++ >> 7;if (t > 255) t = 255;*dest++ = t;
				*dest++ = 255;
				// LordHavoc: .lit support end
			}
		}
		break;
	case 3:
		stride -= (smax<<2);
		bl = blocklights;
		for (i=0 ; i<tmax ; i++, dest += stride)
		{
			for (j=0 ; j<smax ; j++)
			{
				// LordHavoc: .lit support begin
				// LordHavoc: positive lighting (would be 255-t if it were inverse like glquake was)
				t = *bl++ >> 7;if (t > 255) t = 255;*dest++ = t;
				t = *bl++ >> 7;if (t > 255) t = 255;*dest++ = t;
				t = *bl++ >> 7;if (t > 255) t = 255;*dest++ = t;
				*dest++ = 255;
				// LordHavoc: .lit support end
			}
		}
		break;
	case 2:
		bl = blocklights;
		union luxel {
			unsigned short rgb;
			byte bytes[2];
		};
		for (i=0 ; i<tmax ; i++ ,dest += stride)
		{
			for (j=0 ; j<smax*2 ; j++)
			{
				r = bl[0] >> 7; if (r > 255) r = 255; r = r >> 3;
				g = bl[1] >> 7; if (g > 255) g = 255; g = g >> 3;
				b = bl[2] >> 7; if (b > 255) b = 255; b = b >> 3;
				a = bl[3] >> 7; if (a > 255) a = 255; a = a >> 3;
				
				luxel lx;
				lx.rgb = (a << 15) | (b << 10) | (g << 5) | (r);

				dest[j] = lx.bytes[0];
				j++;
				dest[j] = lx.bytes[1];
				bl += 3;
			}
		}
		break;
	case 1:
		bl = blocklights;
		for (i=0 ; i<tmax ; i++ ,dest += stride)
		{
			for (j=0 ; j<smax ; j++)
			{
				// LordHavoc: .lit support begin
				t = ((bl[0] + bl[1] + bl[2]) * 85) >> 15; // LordHavoc: basically / 3, but faster and combined with >> 7 shift down, note: actual number would be 85.3333...
				bl += 3;
				// LordHavoc: .lit support end
				if (t > 255)
					t = 255;
				dest[j] = t;
			}
		}
		break;
	default:
		Sys_Error ("Bad lightmap format");
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
			Sys_Error ("broken cycle");
		if (++count > 100)
			Sys_Error ("infinite cycle");
	}

	return base;
}


/*
=============================================================

	BRUSH MODELS

=============================================================
*/


extern	int		solidskytexture;
extern	int		alphaskytexture;
extern	float	speedscale;		// for top sky and bottom sky

static inline void DrawGLPolyLM (glpoly_t * poly)
{
	if (r_showtris.value)
	{
		sceGuDisable(GU_TEXTURE_2D);
		sceGuDisable(GU_BLEND);

		// Draw the lines directly.
		sceGumDrawArray(
			GU_LINE_STRIP,
			GU_TEXTURE_32BITF | GU_VERTEX_32BITF ,
			poly->numclippedverts, 0, poly->display_list_verts);

		sceGuEnable(GU_TEXTURE_2D);
		sceGuEnable(GU_BLEND);

	}
	else
	{
		// Draw the poly directly.
		sceGuDrawArray(
			GU_TRIANGLE_FAN,
			GU_TEXTURE_32BITF | GU_VERTEX_32BITF ,
			poly->numclippedverts, 0, poly->display_list_verts);
	}
}

static inline void DrawGLPoly (glpoly_t * poly)
{
	sceGuDrawArray(
			GU_TRIANGLE_FAN,
			GU_TEXTURE_32BITF | GU_VERTEX_32BITF,
			poly->numclippedverts, 0, poly->display_list_verts);
}

static inline void DrawTrisPoly (glpoly_t *p) //Crow_bar
{
    sceGuDisable(GU_TEXTURE_2D);
	// Does this poly need clipped?
	const int				unclipped_vertex_count	= p->numverts;
	const glvert_t* const	unclipped_vertices		= p->verts;
	if (clipping::is_clipping_required(
		unclipped_vertices,
		unclipped_vertex_count))
	{
		// Clip the polygon.
		const glvert_t*	clipped_vertices;
		std::size_t		clipped_vertex_count;
		clipping::clip(
			unclipped_vertices,
			unclipped_vertex_count,
			&clipped_vertices,
			&clipped_vertex_count);

		// Did we have any vertices left?
		if (clipped_vertex_count)
		{
			// Copy the vertices to the display list.
			const std::size_t buffer_size = clipped_vertex_count * sizeof(glvert_t);
			glvert_t* const display_list_vertices = static_cast<glvert_t*>(sceGuGetMemory(buffer_size));
			memcpy(display_list_vertices, clipped_vertices, buffer_size);

			// Draw the clipped vertices.
			sceGuDrawArray(
				GU_LINE_STRIP,
				GU_TEXTURE_32BITF | GU_VERTEX_32BITF,
				clipped_vertex_count, 0, display_list_vertices);
		}
	}
	else
	{
		// Draw the poly directly.
		sceGuDrawArray(
			GU_LINE_STRIP,
			GU_TEXTURE_32BITF | GU_VERTEX_32BITF,
			unclipped_vertex_count, 0, unclipped_vertices);
	}
    sceGuEnable(GU_TEXTURE_2D);
}

void DrawGLPoly_ex (glpoly_t *p)
{
     DrawGLPoly(p);
}

// speed up sin calculations - Ed
extern float turbsin[];
static inline void DrawGLWaterPolyLM (glpoly_t *p)
{
/*
	// Does this poly need clipped?

	const float real_time	= static_cast<float>(realtime);
	const float scale		= (1.0f / 64);
	const float turbscale	= (256.0f / (2.0f * static_cast<float>(M_PI)));

	const int				unclipped_vertex_count	= p->numverts;
	//glvert_t* const	unclipped_vertices		= &(p->verts[p->numverts]);

	glvert_t* const	unclipped_vertices		=
			static_cast<glvert_t*>(sceGuGetMemory(sizeof(glvert_t) * unclipped_vertex_count));

	// Generate each vertex.
		const glvert_t*	src			= p->verts;
		const glvert_t*	last_vertex = src + unclipped_vertex_count;
		glvert_t* dst			= unclipped_vertices;

		while (src != last_vertex)
		{
			// Get the input UVs.
			const float	os = src->st[0];
			const float	ot = src->st[1];

			// Fill in the vertex data.
			dst->st[0] = os;
			dst->st[1] = ot;

			//dst->xyz[0] = src->xyz[0] + 8*sinf(src->xyz[1]*0.05+realtime)*sinf(src->xyz[2]*0.05+realtime);
			//dst->xyz[1] = src->xyz[1] + 8*sinf(src->xyz[0]*0.05+realtime)*sinf(src->xyz[2]*0.05+realtime);
			//dst->xyz[2] = src->xyz[2];

            dst->xyz[0] = dst->xyz[0] + 8*sinf(dst->xyz[1]*0.05+realtime)*sinf(dst->xyz[2]*0.05+realtime);
			dst->xyz[1] = dst->xyz[1] + 8*sinf(dst->xyz[0]*0.05+realtime)*sinf(dst->xyz[2]*0.05+realtime);
			dst->xyz[2] = dst->xyz[2];

			// Next vertex.
			++src;
			++dst;
		}

	if (clipping::is_clipping_required(
		unclipped_vertices,
		unclipped_vertex_count))
	{
		// Clip the polygon.
		const glvert_t*	clipped_vertices;
		std::size_t		clipped_vertex_count;
		clipping::clip(
			unclipped_vertices,
			unclipped_vertex_count,
			&clipped_vertices,
			&clipped_vertex_count);

		// Did we have any vertices left?
		if (clipped_vertex_count)
		{
			// Copy the vertices to the display list.
			const std::size_t buffer_size = clipped_vertex_count * sizeof(glvert_t);
			glvert_t* const display_list_vertices = static_cast<glvert_t*>(sceGuGetMemory(buffer_size));
			memcpy(display_list_vertices, clipped_vertices, buffer_size);

			// Draw the clipped vertices.
			sceGuDrawArray(
				GU_TRIANGLE_FAN,
				GU_TEXTURE_32BITF | GU_VERTEX_32BITF ,
				clipped_vertex_count, 0, display_list_vertices);
		}
	}
	else
	{

		// Draw the poly directly.
		sceGuDrawArray(
			GU_TRIANGLE_FAN,
			GU_TEXTURE_32BITF | GU_VERTEX_32BITF ,
			unclipped_vertex_count, 0, unclipped_vertices);
	}
*/
DrawGLPolyLM (p);
}

static inline void DrawGLWaterPoly (glpoly_t *p)
{
/*
	// Does this poly need clipped?

	const float real_time	= static_cast<float>(realtime);
	const float scale		= (1.0f / 64);
	const float turbscale	= (256.0f / (2.0f * static_cast<float>(M_PI)));

	const int				unclipped_vertex_count	= p->numverts;
//	glvert_t* const	unclipped_vertices		        = p->verts;

	glvert_t* const	unclipped_vertices		=
			static_cast<glvert_t*>(sceGuGetMemory(sizeof(glvert_t) * unclipped_vertex_count));

	// Generate each vertex.
		const glvert_t*	src			= p->verts;
		const glvert_t*	last_vertex = src + unclipped_vertex_count;
		glvert_t* dst			= unclipped_vertices;

		while (src != last_vertex)
		{
			// Get the input UVs.
			const float	os = src->st[0];
			const float	ot = src->st[1];

			// Fill in the vertex data.
			dst->st[0] = os;
			dst->st[1] = ot;

			dst->xyz[0] = src->xyz[0] + 8*sinf(src->xyz[1]*0.05+realtime)*sinf(src->xyz[2]*0.05+realtime);
			dst->xyz[1] = src->xyz[1] + 8*sinf(src->xyz[0]*0.05+realtime)*sinf(src->xyz[2]*0.05+realtime);
			dst->xyz[2] = src->xyz[2];

			//dst->xyz[0] = dst->xyz[0] + 8*sinf(dst->xyz[1]*0.05+realtime)*sinf(dst->xyz[2]*0.05+realtime);
			//dst->xyz[1] = dst->xyz[1] + 8*sinf(dst->xyz[0]*0.05+realtime)*sinf(dst->xyz[2]*0.05+realtime);
			//dst->xyz[2] = dst->xyz[2];
			// Next vertex.
			++src;
			++dst;
		}

	if (clipping::is_clipping_required(
		unclipped_vertices,
		unclipped_vertex_count))
	{
		// Clip the polygon.
		const glvert_t*	clipped_vertices;
		std::size_t		clipped_vertex_count;
		clipping::clip(
			unclipped_vertices,
			unclipped_vertex_count,
			&clipped_vertices,
			&clipped_vertex_count);

		// Did we have any vertices left?
		if (clipped_vertex_count)
		{
			// Copy the vertices to the display list.
			const std::size_t buffer_size = clipped_vertex_count * sizeof(glvert_t);
			glvert_t* const display_list_vertices = static_cast<glvert_t*>(sceGuGetMemory(buffer_size));
			memcpy(display_list_vertices, clipped_vertices, buffer_size);

			// Draw the clipped vertices.
			sceGuDrawArray(
				GU_TRIANGLE_FAN,
				GU_TEXTURE_32BITF | GU_VERTEX_32BITF ,
				clipped_vertex_count, 0, display_list_vertices);
		}
	}
	else
	{

		// Draw the poly directly.
		sceGuDrawArray(
			GU_TRIANGLE_FAN,
			GU_TEXTURE_32BITF | GU_VERTEX_32BITF ,
			unclipped_vertex_count, 0, unclipped_vertices);
	}
*/
DrawGLPoly (p);
}

/*
=============
EmitDetailPolys
=============
void GL_BindDET (int texture_index);
void EmitDetailPolys (void)
{
    texture_t *tex;

	if (!detail_polys)
		return;

	if (tex->dt_texturenum == 0)
		return;

	// For each polygon...
	//Crow_bar multi detail texture
	GL_BindDET(tex->dt_texturenum);

	sceGuBlendFunc (GU_ADD, GU_DST_COLOR, GU_SRC_COLOR, 0, 0);
	sceGuEnable(GU_BLEND);
    sceGuTexFunc(GU_TFX_DECAL, GU_TCC_RGBA);

	for (const glpoly_t* p = detail_polys ; p ; p = p->detail_chain)
	{
		// Allocate memory for this polygon.
		const int		unclipped_vertex_count	= p->numverts;
		glvert_t* const	unclipped_vertices		=
			static_cast<glvert_t*>(sceGuGetMemory(sizeof(glvert_t) * unclipped_vertex_count));

		// Generate each vertex.
		const glvert_t*	src			= p->verts;
		const glvert_t*	last_vertex = src + unclipped_vertex_count;
		glvert_t*		dst			= unclipped_vertices;

		while (src != last_vertex)
		{
			// Fill in the vertex data.
			dst->st[0] = src->st[0];
			dst->st[1] = src->st[1];

			dst->xyz[0] = src->xyz[0];
			dst->xyz[1] = src->xyz[1];
			dst->xyz[2] = src->xyz[2];

			// Next vertex.
			++src;
			++dst;
		}

		// Do these vertices need clipped?
		if (clipping::is_clipping_required(unclipped_vertices, unclipped_vertex_count))
		{
			// Clip the polygon.
			const glvert_t*	clipped_vertices;
			std::size_t		clipped_vertex_count;
			clipping::clip(
				unclipped_vertices,
				unclipped_vertex_count,
				&clipped_vertices,
				&clipped_vertex_count);

			// Any vertices left?
			if (clipped_vertex_count)
			{
				// Copy the vertices to the display list.
				const std::size_t buffer_size = clipped_vertex_count * sizeof(glvert_t);
				glvert_t* const display_list_vertices = static_cast<glvert_t*>(sceGuGetMemory(buffer_size));
				memcpy(display_list_vertices, clipped_vertices, buffer_size);

				// Draw the clipped vertices.
				sceGuDrawArray(
					GU_TRIANGLE_FAN,
					GU_TEXTURE_32BITF | GU_VERTEX_32BITF,
					clipped_vertex_count, 0, display_list_vertices);
			}
		}
		else
		{
			// Draw the vertices.
			sceGuDrawArray(
				GU_TRIANGLE_FAN,
				GU_TEXTURE_32BITF | GU_VERTEX_32BITF,
				unclipped_vertex_count, 0, unclipped_vertices);
		}
	}
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
	sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
    sceGuDisable (GU_BLEND);

	detail_polys = NULL;
}
*/


/*
================
R_BlendLightmaps
================
*/
static void R_BlendLightmaps (void)
{
	int			i;

	if (r_fullbright.value)
		return;

	sceGuDepthMask(GU_TRUE);
	sceGuEnable(GU_BLEND);
	sceGuBlendFunc(GU_ADD, GU_DST_COLOR, GU_SRC_COLOR, 0, 0);
	sceGuDepthFunc(GU_EQUAL);

	if(LIGHTMAP_BYTES == 1)
	   VID_SetPaletteLM();

	if (r_lightmap.value)
		sceGuDisable(GU_BLEND);

	lightmap_face_t * lmface;
	for (i=0 ; i<MAX_LIGHTMAPS ; i++)
	{
		lmface = lightmap_chains[i];
		if (!lmface)
			continue;

		char lm_name[16];
		if (lightmap_modified[i])
		{
			lightmap_modified[i] = qfalse;
			lightmap_rectchange[i].l = BLOCK_WIDTH;
			lightmap_rectchange[i].t = BLOCK_HEIGHT;
			lightmap_rectchange[i].w = 0;
			lightmap_rectchange[i].h = 0;

			snprintf(lm_name, sizeof(lm_name), "lightmap%d",i);
			lightmap_index[i] = GL_LoadTextureLM (lm_name, BLOCK_WIDTH, BLOCK_HEIGHT, lightmaps+(i*BLOCK_WIDTH*BLOCK_HEIGHT*LIGHTMAP_BYTES), LIGHTMAP_BYTES, GU_LINEAR, qtrue, qfalse);
		}
		GL_BindLM (lightmap_index[i]);
		for (; lmface; lmface = lmface->next) {
			msurface_t * face = lmface->face;
			float tscale = face->texinfo->texture->width / (BLOCK_WIDTH * 16.f);
			float sscale = face->texinfo->texture->height / (BLOCK_HEIGHT * 16.f);
			
			sceGuTexScale(tscale, sscale);
			sceGuTexOffset(
				tscale * (float)(-1.f * face->texturemins[0] + face->light_s * 16 + 8) / (float)(face->texinfo->texture->width),
				sscale * (float)(-1.f * face->texturemins[1] + face->light_t * 16 + 8) / (float)(face->texinfo->texture->height)
			);

			DrawGLPolyLM(face->polys);
		}
	}

	if(LIGHTMAP_BYTES == 1)
	   VID_SetPaletteTX();

	sceGuTexOffset(0, 0);
	sceGuTexScale(1, 1);
	sceGuDisable(GU_BLEND);
	sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
	sceGuDepthMask (GU_FALSE);
	sceGuEnable(GU_DEPTH_TEST); // dr_mabuse1981: fix
	sceGuDepthFunc(GU_LEQUAL);
}

int ClipFace (msurface_t * fa)
{
	// skip maths if broad phase tells us we don't need clipping
	if (!(fa->flags & SURF_NEEDSCLIPPING))
	{
		fa->polys->numclippedverts = fa->polys->numverts;
		fa->polys->display_list_verts = fa->polys->verts;
		return fa->polys->numverts;
	}

	// shpuld: moved clipping here to have it in one place only
	int verts_total = 0;
	glpoly_t* poly = fa->polys;
	const int unclipped_vertex_count = poly->numverts;
	const glvert_t* const unclipped_vertices = poly->verts;

	if (clipping::is_clipping_required(
		unclipped_vertices,
		unclipped_vertex_count))
	{
		// Clip the polygon.
		const glvert_t*	clipped_vertices;
		std::size_t		clipped_vertex_count;
		clipping::clip(
			unclipped_vertices,
			unclipped_vertex_count,
			&clipped_vertices,
			&clipped_vertex_count
		);

		verts_total += clipped_vertex_count;

		// Did we have any vertices left?
		if (!clipped_vertex_count)
		{
			poly->numclippedverts = 0;
			return verts_total;
		}

		const std::size_t buffer_size = clipped_vertex_count * sizeof(glvert_t);
		poly->display_list_verts = static_cast<glvert_t*>(sceGuGetMemory(buffer_size));
		memcpy(poly->display_list_verts, clipped_vertices, buffer_size);
		poly->numclippedverts = clipped_vertex_count;
	} else {
		verts_total += unclipped_vertex_count;
		poly->display_list_verts = poly->verts;
		poly->numclippedverts = unclipped_vertex_count;
	}
	return verts_total;
}

/*
================
R_RenderBrushPoly
================
dr_mabuse1981: There was a random bug with rendering brushes, it is now fixed.
*/
void R_RenderBrushPoly (msurface_t *fa)
{
	texture_t	*t;
	byte		*base;
	int			maps;
	glRect_t    *theRect;
	int smax, tmax;

	c_brush_polys++;

	// sky and water use multiple polys per surface,
	// this makes clipping more tricky, but they don't have LMs so no prob.
	if (fa->flags & SURF_DRAWSKY)
	{	
		// warp texture, no lightmaps
		// shpuld: replace with scissor pass and then sky, maybe faster?
		EmitBothSkyLayers (fa);
		return;
	}

	t = R_TextureAnimation (fa->texinfo->texture);
	GL_Bind (t->gl_texturenum);

	if (fa->flags & SURF_DRAWTURB)
	{	// warp texture, no lightmaps
		EmitWaterPolys (fa);
		return;
	}

	// Everything from here only uses 1 poly from fa->polys
	int verts_count = ClipFace(fa);

	if (verts_count <= 0)
		return;

	// cypress -- use our new texflag hack
	if (fa->flags & TEXFLAG_NODRAW)
		return;

	switch(fa->flags) {
		case TEXFLAG_REFLECT:
			EmitReflectivePolys(fa);
			break;
		case TEXFLAG_NORMAL:
		default:
			DrawGLPoly(fa->polys); 
			break;
	}
	// cypress -- end texflags

	// Manage lightmap chain
	if (num_lightmapped_faces < MAX_VISIBLE_LIGHTMAPPED_FACES)
	{
		lightmap_faces[num_lightmapped_faces].face = fa;
		lightmap_faces[num_lightmapped_faces].next = lightmap_chains[fa->lightmaptexturenum];
		lightmap_chains[fa->lightmaptexturenum] = &lightmap_faces[num_lightmapped_faces];
		num_lightmapped_faces++;
	}

	// check for lightmap modification
	for (maps = 0 ; maps < MAXLIGHTMAPS && fa->styles[maps] != 255 ; maps++)
		if (d_lightstylevalue[fa->styles[maps]] != fa->cached_light[maps])
			goto dynamic;

	if (fa->dlightframe == r_framecount || fa->cached_dlight)// dynamic previously
	{
dynamic:
		if (r_dynamic.value)
		{
			lightmap_modified[fa->lightmaptexturenum] = qtrue;
			theRect = &lightmap_rectchange[fa->lightmaptexturenum];

			if (fa->light_t < theRect->t) {
				if (theRect->h)
					theRect->h += theRect->t - fa->light_t;
				theRect->t = fa->light_t;
			}
			if (fa->light_s < theRect->l) {
				if (theRect->w)
					theRect->w += theRect->l - fa->light_s;
				theRect->l = fa->light_s;
			}
			smax = (fa->extents[0]>>4)+1;
			tmax = (fa->extents[1]>>4)+1;
			if ((theRect->w + theRect->l) < (fa->light_s + smax))
				theRect->w = (fa->light_s-theRect->l)+smax;
			if ((theRect->h + theRect->t) < (fa->light_t + tmax))
				theRect->h = (fa->light_t-theRect->t)+tmax;

			base = lightmaps + fa->lightmaptexturenum*LIGHTMAP_BYTES*BLOCK_WIDTH*BLOCK_HEIGHT;
			base += fa->light_t * BLOCK_WIDTH * LIGHTMAP_BYTES + fa->light_s * LIGHTMAP_BYTES;
			R_BuildLightMap (fa, base, BLOCK_WIDTH*LIGHTMAP_BYTES);

		}
	}
}

/*
================
R_MirrorChain
================
*/
void R_MirrorChain (msurface_t *s)
{
	if (mirror)
		return;
	mirror = qtrue;
	mirror_plane = s->plane;
}


/*
================
R_DrawWaterSurfaces
================
*/
void R_DrawWaterSurfaces (void)
{
	int			i;
	msurface_t	*s;
	texture_t	*t;

	if (r_wateralpha.value == 1.0 /*&& gl_texsort.value*/)
		return;

	float alpha1 = r_wateralpha.value;
	float alpha2 = 1 - r_wateralpha.value;

	//
	// go back to the world matrix
	//

    /*glLoadMatrixf (r_world_matrix);*/
	sceGumMatrixMode(GU_VIEW);
	sceGumLoadMatrix(&r_world_matrix);
	sceGumUpdateMatrix();

	sceGumMatrixMode(GU_MODEL);

	  if (r_wateralpha.value < 1.0)
	  {
		sceGuEnable (GU_BLEND);
		sceGuTexFunc(GU_TFX_REPLACE , GU_TCC_RGBA);
		sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX, GU_COLOR(alpha1,alpha1,alpha1,alpha1), GU_COLOR(alpha2,alpha2,alpha2,alpha2));
     }


	/*if (!gl_texsort.value) {
		if (!waterchain)
			return;

		for ( s = waterchain ; s ; s=s->texturechain) {
			GL_Bind (s->texinfo->texture->gl_texturenum);
			EmitWaterPolys (s);
		}

		waterchain = NULL;
	} else*/
	{

		for (i=0 ; i<cl.worldmodel->numtextures ; i++)
		{
			t = cl.worldmodel->textures[i];
			if (!t)
				continue;
			s = t->texturechain;
			if (!s)
				continue;
			if ( !(s->flags & SURF_DRAWTURB ) )
				continue;

			// set modulate mode explicitly

			GL_Bind (t->gl_texturenum);

			for ( ; s ; s=s->texturechain)
				EmitWaterPolys (s);

			t->texturechain = NULL;
		}

	}

	if (r_wateralpha.value < 1.0)
	  {
        sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
		sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
		sceGuColor (GU_RGBA(0xff, 0xff, 0xff, 0xff));
		sceGuDisable (GU_BLEND);
	  }

}

/*
================
DrawTextureChains
================
*/

static void DrawTextureChains (void)
{
	int		i;
	msurface_t	*s;
	texture_t	*t;

	sceGuEnable(GU_ALPHA_TEST);
	sceGuAlphaFunc(GU_GREATER, 0xaa, 0xff);
	sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
	sceGuColor(0xffffffff);

	for (i=0 ; i<cl.worldmodel->numtextures ; i++)
	{
		t = cl.worldmodel->textures[i];
		if (!t)
			continue;
		s = t->texturechain;
		if (!s)
			continue;
		if (i == skytexturenum)
			continue; // R_DrawSkyChain (s);
		else if (i == mirrortexturenum && r_mirroralpha.value != 1.0)
		{
			R_MirrorChain (s);
			continue;
		}
   		else
		{
   			if ((s->flags & SURF_DRAWTURB) && r_wateralpha.value != 1.0)
							continue;	// draw translucent water later
			for ( ; s ; s = s->texturechain)
			{
				R_RenderBrushPoly (s);
			}
		}

		t->texturechain = NULL;
	}

	sceGuAlphaFunc(GU_GREATER, 0, 0xff);
	sceGuDisable(GU_ALPHA_TEST);

	//EmitUnderWaterPolys (); //blubsremoved quartal
	//EmitDetailPolys ();

}

void R_GlowSetupBegin(entity_t *e)
{
  //(matrix transform)& alpha value by distance
}

void R_GlowSetupEnd(entity_t *e)
{
  //Restore matrix
}

/*
=================
R_DrawBrushModel
=================
*/
void R_DrawBrushModel (entity_t *e)
{
	int			k;
	vec3_t		mins, maxs;
	int			i;
	msurface_t	*psurf;
	float		dot;
	mplane_t	*pplane;
	model_t		*clmodel;
	qboolean	rotated;
	qboolean    dlight;//

	dlight = qtrue;//


	currententity = e;
	currenttexture = -1;

	clmodel = e->model;

	int frustum_check;

	if (e->angles[0] || e->angles[1] || e->angles[2])
	{
		rotated = qtrue;
		frustum_check = R_FrustumCheckSphere(e->origin, clmodel->radius);
		
	}
	else
	{
		rotated = qfalse;
		VectorAdd (e->origin, clmodel->mins, mins);
		VectorAdd (e->origin, clmodel->maxs, maxs);
	    frustum_check = R_FrustumCheckBox(mins, maxs);
	}

	if (frustum_check < 0)
		return;


	memset (lightmap_chains, 0, sizeof(lightmap_chains));
	num_lightmapped_faces = 0;

	VectorSubtract (r_refdef.vieworg, e->origin, modelorg);
	if (rotated)
	{
		vec3_t	temp;
		vec3_t	forward, right, up;

		VectorCopy (modelorg, temp);
		AngleVectors (e->angles, forward, right, up);
		modelorg[0] = DotProduct (temp, forward);
		modelorg[1] = -DotProduct (temp, right);
		modelorg[2] = DotProduct (temp, up);
	}

	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];

// calculate dynamic lighting for bmodel if it's not an
// instanced model
	if (clmodel->firstmodelsurface != 0/* && !gl_flashblend.value*/)
	{
		for (k=0 ; k<MAX_DLIGHTS ; k++)
		{
			if ((cl_dlights[k].die < cl.time) ||
				(!cl_dlights[k].radius))
				continue;

			R_MarkLights (&cl_dlights[k], 1<<k,	clmodel->nodes + clmodel->hulls[0].firstclipnode);
		}
	}

	sceGumPushMatrix();

	//Crow_bar half_life render.
	if (ISADDITIVE(e))
	{
		//Con_DPrintf("ISADDITIVE:brush\n");
		float deg = e->renderamt;
		float alpha1 = deg;
		float alpha2 = 1 - deg;
		if(deg <= 0.7)
			sceGuDepthMask(GU_TRUE);
		
		sceGuEnable (GU_BLEND);
		sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX,
		GU_COLOR(alpha1,alpha1,alpha1,alpha1),
		GU_COLOR(alpha2,alpha2,alpha2,alpha2));
		dlight = qfalse;
	}
	else if (ISGLOW(e))
	{
		sceGuTexFunc(GU_TFX_MODULATE , GU_TCC_RGBA);
		sceGuDepthMask(GU_TRUE);
		sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0xFFFFFFFF);
		R_GlowSetupBegin(e);
	}
	// shpuld: these have been broken for who knows how long, both were treated as just fence
	// this is fine, but it's good to be more explicit about it in code, hence I'm commenting them out.
	/*
	else if (ISSOLID(e))
	{
		sceGuEnable(GU_ALPHA_TEST);
		int c = (int)(e->renderamt * 255.0f);
		sceGuAlphaFunc(GU_GREATER, c, 0xff);
		dlight = qfalse;
	}
	else if (ISTEXTURE(e))
	{
		sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
		sceGuColor(GU_RGBA(255, 255, 255, (int)(e->renderamt * 255.0f)));
		dlight = qfalse;
	}
	*/
	else if (ISCOLOR(e))
	{
		sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
		sceGuColor(GU_RGBA((int)(e->rendercolor[0] * 255.0f),
			(int)(e->rendercolor[1] * 255.0f),
			(int)(e->rendercolor[2] * 255.0f), 255));
	}
	else
	{
		sceGuEnable(GU_ALPHA_TEST);
		sceGuAlphaFunc(GU_GREATER, 0xaa, 0xff);
		sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
		sceGuColor(0xffffffff);
	}
	//Con_DPrintf("\n");
	//Con_DPrintf("render mode is:  %i \n", (int)e->rendermode);
	//Con_DPrintf("render mask is:  %i \n", (int)(e->renderamt * 255.0f));
	//Con_DPrintf("render color is: %i %i %i \n", (int)(e->rendercolor[0] * 255.0f),
	//		(int)(e->rendercolor[1] * 255.0f),
	//		(int)(e->rendercolor[2] * 255.0f));


	e->angles[0] = -e->angles[0];	// stupid quake bug
	R_BlendedRotateForEntity  (e, 0, e->scale);  //blend transform
	clipping::begin_brush_model();
	e->angles[0] = -e->angles[0];	// stupid quake bug

	//
	// draw texture
	//
	for (i=0 ; i<clmodel->nummodelsurfaces ; i++, psurf++)
	{
		// find which side of the node we are on
		pplane = psurf->plane;
		dot = DotProduct (modelorg, pplane->normal) - pplane->dist;
		// draw the polygon
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
			(!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
			psurf->flags &= ~SURF_NEEDSCLIPPING;
			psurf->flags |= SURF_NEEDSCLIPPING * (frustum_check > 1);
			R_RenderBrushPoly (psurf);
		}
	}


	if(dlight)
	{
		sceGuDisable(GU_ALPHA_TEST);
		R_BlendLightmaps ();
	}

	if (ISADDITIVE(e))
	{
		float deg = e->renderamt;
		if(deg <= 0.7)
			sceGuDepthMask(GU_FALSE);

		sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
		sceGuDisable (GU_BLEND);
	}
	else if(ISGLOW(e))
	{
		R_GlowSetupEnd(e);
		sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
		sceGuDepthMask(GU_FALSE);
		sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
		sceGuDisable (GU_BLEND);
	}
	else if(ISCOLOR(e))
	{
		sceGuColor(0xffffffff);
		sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
	}
	else
	{
		sceGuAlphaFunc(GU_GREATER, 0, 0xff);
		sceGuDisable(GU_ALPHA_TEST);
	}
	/*
	else if(ISSOLID(e))
	{
		sceGuAlphaFunc(GU_GREATER, 0, 0xff);
		sceGuDisable(GU_ALPHA_TEST);
	}
	else if(ISTEXTURE(e))
	{
		sceGuColor(0xffffffff);
		sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
	}
	*/
	//dr_mabuse1981: commented out, this was the one who caused the epic lag
	//DrawFullBrightTextures (clmodel->surfaces, clmodel->numsurfaces);
	//dr_mabuse1981: commented out, this was the one who caused the epic lag

	clipping::end_brush_model();
	sceGumPopMatrix();
	sceGumUpdateMatrix();
}

/*
=============================================================

	WORLD MODEL

=============================================================
*/

/*
================
R_RecursiveWorldNode
================
*/
void R_RecursiveWorldNode (mnode_t *node, bool nofrustumcheck)
{
	int			c, side;
	mplane_t	*plane;
	msurface_t	*surf, **mark;
	mleaf_t		*pleaf;
	float		dot;

	if (node->contents == CONTENTS_SOLID)
		return;		// solid

	if (node->visframe != r_visframecount)
		return;

	int frustum_check = nofrustumcheck ? 0 : R_FrustumCheckBox (node->minmaxs, node->minmaxs+3);

	if (frustum_check < 0)
		return;

// if a leaf node, draw stuff
	if (node->contents < 0)
	{
		pleaf = (mleaf_t *)node;

		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if (c)
		{
			do
			{
				(*mark)->visframe = r_framecount;
				mark++;
			} while (--c);
		}

	// deal with model fragments in this leaf
		if (pleaf->efrags)
			R_StoreEfrags (&pleaf->efrags);

		return;
	}

// node is just a decision point, so go down the apropriate sides

// find which side of the node we are on
	plane = node->plane;

	switch (plane->type)
	{
	case PLANE_X:
		dot = modelorg[0] - plane->dist;
		break;
	case PLANE_Y:
		dot = modelorg[1] - plane->dist;
		break;
	case PLANE_Z:
		dot = modelorg[2] - plane->dist;
		break;
	default:
		dot = DotProduct (modelorg, plane->normal) - plane->dist;
		break;
	}

	if (dot >= 0)
		side = 0;
	else
		side = 1;

// recurse down the children, front side first
	R_RecursiveWorldNode (node->children[side], !frustum_check);

// draw stuff
	c = node->numsurfaces;

	if (c)
	{
		surf = cl.worldmodel->surfaces + node->firstsurface;

		if (dot < 0 -BACKFACE_EPSILON)
			side = SURF_PLANEBACK;
		else if (dot > BACKFACE_EPSILON)
			side = 0;
		{
			for ( ; c ; c--, surf++)
			{
				if (surf->visframe != r_framecount)
					continue;

				// don't backface underwater surfaces, because they warp
				if ( !(surf->flags & SURF_UNDERWATER) && ( (dot < 0) ^ !!(surf->flags & SURF_PLANEBACK)) )
					continue;		// wrong side

				// if sorting by texture, just store it out
				/*if (gl_texsort.value)*/
				
				if (!mirror
				|| surf->texinfo->texture != cl.worldmodel->textures[mirrortexturenum])
				{
					surf->flags &= ~SURF_NEEDSCLIPPING;
					surf->flags |= SURF_NEEDSCLIPPING * (frustum_check > 1);

					surf->texturechain = surf->texinfo->texture->texturechain;
					surf->texinfo->texture->texturechain = surf;
				}
				/* else if (surf->flags & SURF_DRAWSKY) {
					surf->texturechain = skychain;
					skychain = surf;
				} else if (surf->flags & SURF_DRAWTURB) {
					surf->texturechain = waterchain;
					waterchain = surf;
				} else
					R_DrawSequentialPoly (surf);*/

			}
		}

	}

// recurse down the back side
	R_RecursiveWorldNode (node->children[!side], !frustum_check);
}

void R_AddBrushModelToChains (entity_t * e)
{
	model_t * clmodel = e->model;
	vec3_t mins, maxs;
	VectorAdd (e->origin, clmodel->mins, mins);
	VectorAdd (e->origin, clmodel->maxs, maxs);
	int frustum_check = R_FrustumCheckBox(mins, maxs);
	if (frustum_check < 0)
	{
		return;
	}

	msurface_t * psurf = &clmodel->surfaces[clmodel->firstmodelsurface];

	/* TODO: doesn't work for some reason, needs investigation if dlights ever come back
	if (clmodel->firstmodelsurface != 0)
	{
		for (int k = 0; k < MAX_DLIGHTS; k++)
		{
			if ((cl_dlights[k].die < cl.time) ||
				(!cl_dlights[k].radius))
				continue;

			R_MarkLights (&cl_dlights[k], 1<<k,	clmodel->nodes + clmodel->hulls[0].firstclipnode);
		}
	}
	*/

	for (int j = 0; j < clmodel->nummodelsurfaces; j++, psurf++)
	{
		// find which side of the node we are on
		mplane_t * pplane = psurf->plane;
		float dot = DotProduct (modelorg, pplane->normal) - pplane->dist;
		// draw the polygon
		if (((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) ||
			(!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
			psurf->flags &= ~SURF_NEEDSCLIPPING;
			psurf->flags |= SURF_NEEDSCLIPPING * (frustum_check > 1);

			psurf->texturechain = psurf->texinfo->texture->texturechain;
			psurf->texinfo->texture->texturechain = psurf;
		}
	}
}

void R_AddStaticBrushModelsToChains ()
{
	for (int i = 0; i < cl_numstaticbrushmodels; i++)
	{
		R_AddBrushModelToChains(cl_staticbrushmodels[i]);
	}
}

extern char	skybox_name[32];
/*
=============
R_DrawWorld
=============
*/
void R_DrawWorld (void)
{
	entity_t	ent;

	memset (&ent, 0, sizeof(ent));
	ent.model = cl.worldmodel;

	VectorCopy (r_refdef.vieworg, modelorg);

	currententity = &ent;
	currenttexture = -1;

	/*glColor3f (1,1,1);*/
	memset (lightmap_chains, 0, sizeof(lightmap_chains));
	num_lightmapped_faces = 0;

	R_ClearSkyBox ();

	// cypress -- was a strcmp, changed for speed.
	if (skybox_name[0])
		R_DrawSkyBox();

	R_RecursiveWorldNode (cl.worldmodel->nodes, false);

	R_AddStaticBrushModelsToChains ();

	Fog_SetupFrame(true);
	DrawTextureChains ();
	Fog_SetupFrame(false);
	R_BlendLightmaps ();

	//dr_mabuse1981: commented out, this was the one who caused the epic lag
    //DrawFullBrightTextures (cl.worldmodel->surfaces, cl.worldmodel->numsurfaces);
	//dr_mabuse1981: commented out, this was the one who caused the epic lag
}


/*
===============
R_MarkLeaves
===============
*/
void R_MarkLeaves (void)
{
	byte	*vis;
	mnode_t	*node;
	int		i;

	if (r_oldviewleaf == r_viewleaf || mirror)
		return;

	++r_visframecount;
	r_oldviewleaf = r_viewleaf;

	vis = Mod_LeafPVS (r_viewleaf, cl.worldmodel);

	for (i = 0; i < cl.worldmodel->numleafs; ++i)
	{
		if (vis[i>>3] & (1<<(i&7)))
		{
			node = (mnode_t *)&cl.worldmodel->leafs[i+1];
			do
			{
				if (node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			} while (node);
		}
	}
}



/*
=============================================================================

  LIGHTMAP ALLOCATION

=============================================================================
*/

/*
========================
AllocBlock -- returns a texture number and the position inside it
========================
*/
int AllocBlock (int w, int h, int *x, int *y)
{
	int		i, j;
	int		best, best2;
	int		texnum;

	// ericw -- rather than searching starting at lightmap 0 every time,
	// start at the last lightmap we allocated a surface in.
	// This makes AllocBlock much faster on large levels (can shave off 3+ seconds
	// of load time on a level with 180 lightmaps), at a cost of not quite packing
	// lightmaps as tightly vs. not doing this (uses ~5% more lightmaps)
	for (texnum=last_lightmap_allocated ; texnum<MAX_LIGHTMAPS ; texnum++, last_lightmap_allocated++)
	{
		best = BLOCK_HEIGHT;

		for (i=0 ; i<BLOCK_WIDTH-w ; i++)
		{
			best2 = 0;

			for (j=0 ; j<w ; j++)
			{
				if (allocated[texnum][i+j] >= best)
					break;
				if (allocated[texnum][i+j] > best2)
					best2 = allocated[texnum][i+j];
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
			allocated[texnum][*x + i] = best + h;

		return texnum;
	}

	Sys_Error ("full");
	return 0; //johnfitz -- shut up compiler
}

mvertex_t	*r_pcurrentvertbase;
model_t		*currentmodel;

int	nColinElim;

/*
================
BuildSurfaceDisplayList
================
*/
static void BuildSurfaceDisplayList (msurface_t *fa)
{
	int			i, lindex, lnumverts;//, s_axis, t_axis;
//	float		dist, lastdist, lzi, scale, u, v, frac;
//	unsigned	mask;
//	vec3_t		local, transformed;
	medge_t		*pedges, *r_pedge;
//	mplane_t	*pplane;
//	int			vertpage;//, newverts, newpage, lastvert;
//	qboolean	visible;
	float		*vec;
	float		s, t;
	glpoly_t	*poly;

// reconstruct the polygon
	pedges = currentmodel->edges;
	lnumverts = fa->numedges;
//	vertpage = 0;

	//
	// draw texture
	//
    poly = static_cast<glpoly_t*>(Hunk_Alloc (sizeof(glpoly_t) + (lnumverts - 1) * sizeof(glvert_t)));
	poly->next = fa->polys;
	poly->flags = fa->flags;
	fa->polys = poly;
	poly->numverts = lnumverts;

	for (i=0 ; i<lnumverts ; i++)
	{
		lindex = currentmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
		{
			r_pedge = &pedges[lindex];
			vec = r_pcurrentvertbase[r_pedge->v[0]].position;
		}
		else
		{
			r_pedge = &pedges[-lindex];
			vec = r_pcurrentvertbase[r_pedge->v[1]].position;
		}
		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s /= fa->texinfo->texture->width;

		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t /= fa->texinfo->texture->height;

		VectorCopy(vec, poly->verts[i].xyz);
		poly->verts[i].st[0] = s;
		poly->verts[i].st[1] = t;
	}

	//
	// remove co-linear points - Ed
	//

	// Colinear point removal-start

	lnumverts = poly->numverts;

	if (!gl_keeptjunctions.value && !(fa->flags & SURF_UNDERWATER) )
	{
		int numRemoved = 0;
		int j;

		for (i = 0 ; i < lnumverts ; ++i)
		{
			vec3_t v1, v2;
			const glvert_t *prev, *this_, *next;
//			float f;

			prev = &poly->verts[(i + lnumverts - 1) % lnumverts];
			this_ = &poly->verts[i];
			next = &poly->verts[(i + 1) % lnumverts];

			VectorSubtract( this_->xyz, prev->xyz, v1 );
			VectorNormalize( v1 );
			VectorSubtract( next->xyz, prev->xyz, v2 );
			VectorNormalize( v2 );

			// skip co-linear points
			#define COLINEAR_EPSILON 0.001
			if ((fabsf( v1[0] - v2[0] ) <= COLINEAR_EPSILON) &&
				(fabsf( v1[1] - v2[1] ) <= COLINEAR_EPSILON) &&
				(fabsf( v1[2] - v2[2] ) <= COLINEAR_EPSILON))
			{
				for (j = i + 1; j < lnumverts; j = j + 1)
				{
					poly->verts[j - 1] = poly->verts[j];
				}

				--lnumverts;
				++nColinElim;
				numRemoved++;
				// retry next vertex next time, which is now current vertex
				--i;
			}
		}
	}

	// Colinear point removal-end
	poly->numverts = lnumverts;
}

/*
========================
GL_CreateSurfaceLightmap
========================
*/
static void GL_CreateSurfaceLightmap (msurface_t *surf)
{
	int		smax, tmax;//, s, t, l, i;
	byte	*base;

    if (surf->flags & (SURF_DRAWSKY|SURF_DRAWTURB))
		return;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;

	surf->lightmaptexturenum = AllocBlock (smax, tmax, &surf->light_s, &surf->light_t);

    base = lightmaps + surf->lightmaptexturenum*LIGHTMAP_BYTES*BLOCK_WIDTH*BLOCK_HEIGHT;
	base += (surf->light_t * BLOCK_WIDTH + surf->light_s) * LIGHTMAP_BYTES;
	R_BuildLightMap (surf, base, BLOCK_WIDTH*LIGHTMAP_BYTES);

}


/*
==================
GL_BuildLightmaps

Builds the lightmap texture
with all the surfaces from all brush models
==================
*/
void GL_BuildLightmaps (void)
{
	int		i, j;
	model_t	*m;

	//Con_Printf ("Lightmap surfaces = %i\n", MAX_LIGHTMAPS);
	//Con_Printf ("Lightmap bytes = %i\n", LIGHTMAP_BYTES);

	memset (allocated, 0, sizeof(allocated));

	r_framecount = 1;		// no dlightcache

	last_lightmap_allocated = 0;

	if (!lightmap_textures)
	{
		lightmap_textures = 0;
	}

	for (j=1 ; j<MAX_MODELS ; j++)
	{
		m = cl.model_precache[j];
		if (!m)
			break;
		if (m->name[0] == '*')
			continue;

		r_pcurrentvertbase = m->vertexes;
		currentmodel = m;
		for (i=0 ; i<m->numsurfaces ; i++)
		{
			// todo: investigate why this is done even for turb/sky
			GL_CreateSurfaceLightmap (m->surfaces + i);
			if ( m->surfaces[i].flags & SURF_DRAWTURB )
				continue;

			if ( m->surfaces[i].flags & SURF_DRAWSKY )
				continue;

			BuildSurfaceDisplayList (m->surfaces + i);
		}
	}

	//
	// upload all lightmaps that were filled
	//
	char lm_name[16];
	for (i=0 ; i<MAX_LIGHTMAPS ; i++)
	{
            if (!allocated[i][0])
                break;		// no more used

            lightmap_modified[i] = qfalse;
            lightmap_rectchange[i].l = BLOCK_WIDTH;
            lightmap_rectchange[i].t = BLOCK_HEIGHT;
            lightmap_rectchange[i].w = 0;
            lightmap_rectchange[i].h = 0;

            sprintf(lm_name,"lightmap%d",i);
            lightmap_index[i] = GL_LoadTextureLM (lm_name, BLOCK_WIDTH, BLOCK_HEIGHT, lightmaps+(i*BLOCK_WIDTH*BLOCK_HEIGHT*LIGHTMAP_BYTES), LIGHTMAP_BYTES, GU_LINEAR, qtrue, qfalse);
	}
}
