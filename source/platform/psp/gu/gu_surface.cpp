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

#include <pspgum.h>

extern "C"
{
#include "../../../nzportable_def.h"
}

#ifdef PSP_VFPU
#include <pspmath.h>
#endif

extern int LIGHTMAP_BYTES;

#include "../clipping.hpp"
#include "gu_fullbright.h"

using namespace quake;

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

int			allocated[MAX_LIGHTMAPS][BLOCK_WIDTH];

// the lightmap texture data needs to be kept in
// main memory so texsubimage can update properly
byte		*lightmaps;
int 		lightmap_index[MAX_LIGHTMAPS];

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

    //unsigned *blcr, *blcg, *blcb;

	surf->cached_dlight = (surf->dlightframe == r_framecount) ? true : false;

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


static inline void DrawGLPoly (glpoly_t * poly)
{
	R_DrawSurfaceFan((const float *)poly->display_list_verts,
		poly->numclippedverts, 5, 2, 0, false, 0);
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
void EmitDetailPolys (void)
{
    texture_t *tex;

	if (!detail_polys)
		return;

	if (tex->dt_texturenum == 0)
		return;

	// For each polygon...
	//Crow_bar multi detail texture
	Hyena_BindTextureLod(tex->dt_texturenum, (int)r_detail_mipmaps_func.value, r_detail_mipmaps_bias.value,
            0.4f, r_detail_mipmaps.value > 0, r_retro.value != 0);

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
int R_UploadLightmap(int i)
{
    char name[16];

    if (lightmap_modified[i]) {
        lightmap_modified[i] = false;
        lightmap_rectchange[i].l = BLOCK_WIDTH;
        lightmap_rectchange[i].t = BLOCK_HEIGHT;
        lightmap_rectchange[i].w = 0;
        lightmap_rectchange[i].h = 0;
        snprintf(name, sizeof(name), "lightmap%d", i);
        lightmap_index[i] = Hyena_LoadLightmap(name, BLOCK_WIDTH, BLOCK_HEIGHT,
            lightmaps + i * BLOCK_WIDTH * BLOCK_HEIGHT * LIGHTMAP_BYTES,
            LIGHTMAP_BYTES == 1 ? HYE_TEXTURE_INDEX8 : HYE_TEXTURE_RGBA8, true);
    }
    return lightmap_index[i];
}

void
R_UpdateSurfaceLightmap(msurface_t *surface)
{
    byte *base;
    glRect_t *rect;
    int maps, smax, tmax;

    for (maps = 0; maps < MAXLIGHTMAPS && surface->styles[maps] != 255; ++maps)
        if (d_lightstylevalue[surface->styles[maps]] != surface->cached_light[maps])
            break;
    if (maps == MAXLIGHTMAPS || surface->styles[maps] == 255) {
        if (surface->dlightframe != r_framecount && !surface->cached_dlight)
            return;
    }
    if (!r_dynamic.value)
        return;

    lightmap_modified[surface->lightmaptexturenum] = true;
    rect = &lightmap_rectchange[surface->lightmaptexturenum];
    if (surface->light_t < rect->t) {
        if (rect->h)
            rect->h += rect->t - surface->light_t;
        rect->t = surface->light_t;
    }
    if (surface->light_s < rect->l) {
        if (rect->w)
            rect->w += rect->l - surface->light_s;
        rect->l = surface->light_s;
    }
    smax = (surface->extents[0] >> 4) + 1;
    tmax = (surface->extents[1] >> 4) + 1;
    if (rect->w + rect->l < surface->light_s + smax)
        rect->w = surface->light_s - rect->l + smax;
    if (rect->h + rect->t < surface->light_t + tmax)
        rect->h = surface->light_t - rect->t + tmax;
    base = lightmaps + surface->lightmaptexturenum * LIGHTMAP_BYTES * BLOCK_WIDTH * BLOCK_HEIGHT;
    base += surface->light_t * BLOCK_WIDTH * LIGHTMAP_BYTES + surface->light_s * LIGHTMAP_BYTES;
    R_BuildLightMap(surface, base, BLOCK_WIDTH * LIGHTMAP_BYTES);
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

	c_brush_polys++;

	// cypress -- use our new texflag hack
	if (fa->flags & TEXFLAG_NODRAW)
		return;
	
	if (fa->flags & SURF_DRAWSKY)
		return;

	t = R_TextureAnimation (fa->texinfo->texture);
	Hyena_BindTexture(t->gl_texturenum);

	if (fa->flags & SURF_DRAWTURB)
	{	// warp texture, no lightmaps
		EmitWaterPolys (fa);
		return;
	}

	// Everything from here only uses 1 poly from fa->polys
	int verts_count = ClipFace(fa);

	if (verts_count <= 0)
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

	R_ChainLightmap(fa);
	R_UpdateSurfaceLightmap(fa);
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
	vec3_t		mins, maxs;
	int			i;
	msurface_t	*psurf;
	float		dot;
	mplane_t	*pplane;
	model_t		*clmodel;
	qboolean	rotated;
	qboolean    dlight;//

	dlight = true;//


	currententity = e;
	currenttexture = -1;

	clmodel = e->model;

	int frustum_check;

	if (e->angles[0] || e->angles[1] || e->angles[2])
	{
		rotated = true;
		frustum_check = R_FrustumCheckSphere(e->origin, clmodel->radius);
		
	}
	else
	{
		rotated = false;
		VectorAdd (e->origin, clmodel->mins, mins);
		VectorAdd (e->origin, clmodel->maxs, maxs);
	    frustum_check = R_FrustumCheckBox(mins, maxs);
	}

	if (frustum_check < 0)
		return;


	R_ClearLightmapChains();

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
	// shpuld: remove dynamic lighting stuff for extra cycles
	// if (clmodel->firstmodelsurface != 0/* && !gl_flashblend.value*/)
	/*
	{
		for (k=0 ; k<MAX_DLIGHTS ; k++)
		{
			if ((cl_dlights[k].die < cl.time) ||
			(!cl_dlights[k].radius))
			continue;
			
			R_MarkLights (&cl_dlights[k], 1<<k,	clmodel->nodes + clmodel->hulls[0].firstclipnode);
		}
	}
	*/

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
		dlight = false;
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
		dlight = false;
	}
	else if (ISTEXTURE(e))
	{
		sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
		sceGuColor(GU_RGBA(255, 255, 255, (int)(e->renderamt * 255.0f)));
		dlight = false;
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
	// Allocate the CPU atlas for the selected source format.
	size_t atlas_size = MAX_LIGHTMAPS * BLOCK_WIDTH * BLOCK_HEIGHT * LIGHTMAP_BYTES;
	byte *atlas = (byte *)realloc(lightmaps, atlas_size);
	if (!atlas)
		Sys_Error("Out of lightmap atlas memory\n");
	lightmaps = atlas;
	memset(lightmaps, 0, atlas_size);

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

            lightmap_modified[i] = false;
            lightmap_rectchange[i].l = BLOCK_WIDTH;
            lightmap_rectchange[i].t = BLOCK_HEIGHT;
            lightmap_rectchange[i].w = 0;
            lightmap_rectchange[i].h = 0;

            sprintf(lm_name,"lightmap%d",i);
            lightmap_index[i] = Hyena_LoadLightmap(lm_name, BLOCK_WIDTH, BLOCK_HEIGHT, lightmaps+(i*BLOCK_WIDTH*BLOCK_HEIGHT*LIGHTMAP_BYTES), LIGHTMAP_BYTES == 1 ? HYE_TEXTURE_INDEX8 : HYE_TEXTURE_RGBA8, true);
	}
	{
        const r_world_layout_t layout = { offsetof(glpoly_t, verts), 5, 2, 0, -1,
            offsetof(glpoly_t, display_list_verts), offsetof(glpoly_t, numclippedverts),
            SURF_DRAWSKY | SURF_DRAWTURB | SURF_UNDERWATER | TEXFLAG_NODRAW | TEXFLAG_REFLECT,
            0, 5, true, &r_lightmap, &r_showtris };
        R_BuildWorldBatch(&layout);
    }
}
