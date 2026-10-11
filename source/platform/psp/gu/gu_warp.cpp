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
// gl_warp.c -- water polygons

extern "C"
{
#include "../../../nzportable_def.h"
}

#include "../clipping.hpp"

using namespace quake;

extern	model_t	*loadmodel;

msurface_t	*warpface;

extern cvar_t gl_subdivide_size;

static void BoundPoly (int numverts, float *verts, vec3_t mins, vec3_t maxs)
{
	int		i, j;
	float	*v;

	mins[0] = mins[1] = mins[2] = 9999;
	maxs[0] = maxs[1] = maxs[2] = -9999;
	v = verts;
	for (i=0 ; i<numverts ; i++)
	{
		for (j=0 ; j<3 ; j++, v++)
		{
			if (*v < mins[j])
				mins[j] = *v;
			if (*v > maxs[j])
				maxs[j] = *v;
		}
    }
}

static void SubdividePolygon (int numverts, float *verts)
{
	int		i, j, k;
	vec3_t	mins, maxs;
	float	m;
	float	*v;
	vec3_t	front[64], back[64];
	int		f, b;
	float	dist[64];
	float	frac;
	glpoly_t	*poly;
	float	s, t, subdivide_size;;

	if (numverts > 60)
		Sys_Error ("numverts = %i", numverts);

	subdivide_size = fmax(1, gl_subdivide_size.value);
	BoundPoly (numverts, verts, mins, maxs);

	for (i=0 ; i<3 ; i++)
	{
		m = (mins[i] + maxs[i]) * 0.5;
		m = subdivide_size * floorf (m / subdivide_size + 0.5);
		if (maxs[i] - m < 8)
			continue;
		if (m - mins[i] < 8)
			continue;

		// cut it
		v = verts + i;
		for (j=0 ; j<numverts ; j++, v+= 3)
			dist[j] = *v - m;

		// wrap cases
		dist[j] = dist[0];
		v-=i;
		VectorCopy (verts, v);

		f = b = 0;
		v = verts;
		for (j=0 ; j<numverts ; j++, v+= 3)
		{
			if (dist[j] >= 0)
			{
				VectorCopy (v, front[f]);
				f++;
			}
			if (dist[j] <= 0)
			{
				VectorCopy (v, back[b]);
				b++;
			}
			if (dist[j] == 0 || dist[j+1] == 0)
				continue;
			if ( (dist[j] > 0) != (dist[j+1] > 0) )
			{
				// clip point
				frac = dist[j] / (dist[j] - dist[j+1]);
				for (k=0 ; k<3 ; k++)
					front[f][k] = back[b][k] = v[k] + frac*(v[3+k] - v[k]);
				f++;
				b++;
			}
		}

		SubdividePolygon (f, front[0]);
		SubdividePolygon (b, back[0]);
		return;
	}

	poly = static_cast<glpoly_t*>(Hunk_Alloc (sizeof(glpoly_t) + (numverts - 1) * sizeof(glvert_t)));
	poly->next = warpface->polys;
	warpface->polys = poly;
	poly->numverts = numverts;
	for (i=0 ; i<numverts ; i++, verts+= 3)
	{
		VectorCopy (verts, poly->verts[i].xyz);
		s = DotProduct (verts, warpface->texinfo->vecs[0]);
		t = DotProduct (verts, warpface->texinfo->vecs[1]);
		poly->verts[i].st[0] = s;
		poly->verts[i].st[1] = t;
	}
}

/*
================
GL_SubdivideSurface

Breaks a polygon up along axial 64 unit
boundaries so that turbulent warps
can be done reasonably.
================
*/
void GL_SubdivideSurface (msurface_t *fa)
{
	vec3_t		verts[64];
	int			numverts;
	int			i;
	int			lindex;
	float		*vec;

	warpface = fa;

	//
	// convert edges back to a normal polygon
	//
	numverts = 0;
	for (i=0 ; i<fa->numedges ; i++)
	{
		lindex = loadmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
			vec = loadmodel->vertexes[loadmodel->edges[lindex].v[0]].position;
		else
			vec = loadmodel->vertexes[loadmodel->edges[-lindex].v[1]].position;
		VectorCopy (vec, verts[numverts]);
		numverts++;
	}

	SubdividePolygon (numverts, verts[0]);
}

//=========================================================


#define	TURBSINSIZE	128
#define	TURBSCALE	((float)TURBSINSIZE / (2 * M_PI))

byte turbsin[TURBSINSIZE] =
{
	127, 133, 139, 146, 152, 158, 164, 170, 176, 182, 187, 193, 198, 203, 208, 213,
	217, 221, 226, 229, 233, 236, 239, 242, 245, 247, 249, 251, 252, 253, 254, 254,
	255, 254, 254, 253, 252, 251, 249, 247, 245, 242, 239, 236, 233, 229, 226, 221,
	217, 213, 208, 203, 198, 193, 187, 182, 176, 170, 164, 158, 152, 146, 139, 133,
	127, 121, 115, 108, 102, 96, 90, 84, 78, 72, 67, 61, 56, 51, 46, 41,
	37, 33, 28, 25, 21, 18, 15, 12, 9, 7, 5, 3, 2, 1, 0, 0,
	0, 0, 0, 1, 2, 3, 5, 7, 9, 12, 15, 18, 21, 25, 28, 33,
	37, 41, 46, 51, 56, 61, 67, 72, 78, 84, 90, 96, 102, 108, 115, 121,
};

__inline static float SINTABLE_APPROX (float time)
{
	float	sinlerpf, lerptime, lerp;
	int	sinlerp1, sinlerp2;

	sinlerpf = time * TURBSCALE;
	sinlerp1 = floor(sinlerpf);
	sinlerp2 = sinlerp1 + 1;
	lerptime = sinlerpf - sinlerp1;

	lerp = turbsin[sinlerp1 & (TURBSINSIZE - 1)] * (1 - lerptime) + turbsin[sinlerp2 & (TURBSINSIZE - 1)] * lerptime;
	return -8 + 16 * lerp / 255.0;
}

/*
================
GL_Surface
================
*/
void GL_Surface (msurface_t *fa)
{
	vec3_t		verts[64];
	int			numverts;
	int			i;
	int			lindex;
	float		*vec;
	glpoly_t	*poly;
	//float	texscale;
	float	s, t;

	//texscale = (1.0/32.0);

	//
	// convert edges back to a normal polygon
	//
	numverts = 0;
	for (i=0 ; i<fa->numedges ; i++)
	{
		lindex = loadmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
			vec = loadmodel->vertexes[loadmodel->edges[lindex].v[0]].position;
		else
			vec = loadmodel->vertexes[loadmodel->edges[-lindex].v[1]].position;
		VectorCopy (vec, verts[numverts]);
		numverts++;
	}

	//create the poly
	poly = static_cast<glpoly_t*>(Hunk_Alloc (sizeof(glpoly_t) + (numverts - 1) * sizeof(glvert_t)));
	poly->next = NULL;
	fa->polys = poly;
	poly->numverts = numverts;
	for (i=0, vec=(float *)verts; i<numverts; i++, vec+= 3)
	{
		VectorCopy (vec, poly->verts[i].xyz);
		s = DotProduct(vec, fa->texinfo->vecs[0]);// * texscale;
		t = DotProduct(vec, fa->texinfo->vecs[1]);// * texscale;
		poly->verts[i].st[0] = s;
		poly->verts[i].st[1] = t;
	}
}

/*
=============
EmitFlatPoly
=============
*/
void EmitFlatPoly (msurface_t *fa)
{
	// For each polygon...
	for (const glpoly_t* p = fa->polys; p; p = p->next)
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
			dst->st[0] = src->st[0];    //Tex
			dst->st[1] = src->st[1];

			dst->xyz[0] = src->xyz[0];  //Verts
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
}

/*
=============
EmitWaterPolys

Does a water warp on the pre-fragmented glpoly_t chain
=============
*/
void EmitWaterPolys (msurface_t *fa)
{
	//const float real_time	= static_cast<float>(realtime);
	const float scale		= (1.0f / 64);
/*
	//jkrige - clamp waterripple values
	if(r_waterripple.value>10)
	   r_waterripple.value=10;

	if(r_waterripple.value<0)
	   r_waterripple.value=0;
    //jkrige - clamp waterripple values
*/

	// For each polygon...
	for (const glpoly_t* p = fa->polys; p; p = p->next)
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
			// Get the input UVs.
			const float	os = src->st[0];
			const float	ot = src->st[1];

			// Fill in the vertex data.
			dst->st[0] = (os + SINTABLE_APPROX(ot * 0.125 + cl.time)) * scale;
			dst->st[1] = (ot + SINTABLE_APPROX(os * 0.125 + cl.time)) * scale;

			dst->xyz[0] = src->xyz[0];
			dst->xyz[1] = src->xyz[1];
            dst->xyz[2] = src->xyz[2];
            //dst->xyz[2] = src->xyz[2] + r_waterripple.value * sin(src->xyz[0]*0.05+realtime)*sin(src->xyz[2]*0.05+realtime);
			// Next vertex.
			++src;
			++dst;
		}

		if (!(fa->flags & SURF_NEEDSCLIPPING))
		{
			sceGuDrawArray(
				GU_TRIANGLE_FAN,
				GU_TEXTURE_32BITF | GU_VERTEX_32BITF,
				unclipped_vertex_count, 0, unclipped_vertices);
			return;
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
}

/*
=============
EmitUnderWaterPolys
based on water polys!
By Crow_bar.
=============
*/
/*
void EmitUnderWaterPolys (void)
{
   	const float scale		= (-3 * (0.5 / 64));
    extern glpoly_t	*caustics_polys;

	sceGuEnable(GU_BLEND);
	sceGuBlendFunc (GU_ADD, GU_DST_COLOR, GU_SRC_COLOR, 0, 0);
    sceGuTexFunc(GU_TFX_DECAL, GU_TCC_RGBA);


	// For each polygon...
    for (const glpoly_t* p = caustics_polys ; p ; p = p->caustics_chain)
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
			// Get the input UVs.
			const float	os = src->st[0];
			const float	ot = src->st[1];

			// Fill in the vertex data.
			dst->st[0] = (os + SINTABLE_APPROX(0.465 * (cl.time + ot))) * scale;
			dst->st[1] = (ot + SINTABLE_APPROX(0.465 * (cl.time + os))) * scale;

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

	caustics_polys = NULL;
}
*/

/*
=============
EmitScrollPolys

Does a scroll on the pre-fragmented glpoly_t chain
=============
*/
void EmitScrollPolys (msurface_t *fa)
{
	const float real_time	= static_cast<float>(realtime);
    const float scroll      = (-64 * ((real_time*0.5) - (int)(real_time*0.5)));

	// For each polygon...
	for (const glpoly_t* p = fa->polys; p; p = p->next)
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
			dst->st[0] = src->st[0] + scroll;
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
}


extern int ref_texture;

/*
=============
EmitReflectivePolys

Does a reflective warp on the pre-fragmented glpoly_t chain
=============
*/
void EmitReflectivePolys (msurface_t *fa)
{
	// For each polygon...
	for (const glpoly_t* p = fa->polys; p; p = p->next)
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
			vec3_t	dir;
			VectorSubtract(src->xyz, r_origin, dir);
			dir[2] *= 3;	// flatten the sphere

			const float length = 6 * 63 / sqrtf(DotProduct(dir, dir));

			dir[0] *= length;
			dir[1] *= length;

			dst->st[0] = (dir[0]) * (1.0f / 256.0f);
			dst->st[1] = (dir[1]) * (1.0f / 256.0f);
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
}
