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
// r_warp.c -- water polygons

#include "../nzportable_def.h"

extern	model_t	*loadmodel;

msurface_t	*warpface;

extern cvar_t gl_subdivide_size;

void BoundPoly (int numverts, float *verts, vec3_t mins, vec3_t maxs)
{
	int		i, j;
	float	*v;

	mins[0] = mins[1] = mins[2] = 9999;
	maxs[0] = maxs[1] = maxs[2] = -9999;
	v = verts;
	for (i=0 ; i<numverts ; i++)
		for (j=0 ; j<3 ; j++, v++)
		{
			if (*v < mins[j])
				mins[j] = *v;
			if (*v > maxs[j])
				maxs[j] = *v;
		}
}

void SubdividePolygon (int numverts, float *verts)
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
	float	s, t;
    float subdivide_size = MAX(1, gl_subdivide_size.value);

	if (numverts < 3 || numverts > 60)
		Sys_Error ("numverts = %i", numverts);

	BoundPoly (numverts, verts, mins, maxs);

	for (i=0 ; i<3 ; i++)
	{
		m = (mins[i] + maxs[i]) * 0.5f;
		m = subdivide_size * floorf (m/subdivide_size + 0.5f);
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

	poly = Hunk_Alloc (sizeof(glpoly_t) + (numverts-4) * VERTEXSIZE*sizeof(float));
	poly->next = warpface->polys;
	warpface->polys = poly;
	poly->numverts = numverts;
	for (i=0 ; i<numverts ; i++, verts+= 3)
	{
		VectorCopy (verts, R_PolyPosition(poly, i));
		s = DotProduct (verts, warpface->texinfo->vecs[0]);
		t = DotProduct (verts, warpface->texinfo->vecs[1]);
		R_PolyUV(poly, i)[0] = s;
		R_PolyUV(poly, i)[1] = t;
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
void R_SubdivideSurface (msurface_t *fa)
{
	vec3_t		verts[64];
	int			numverts;

	warpface = fa;

	//
	// convert edges back to a normal polygon
	//
	numverts = 0;
	if (fa->numedges < 3 || fa->numedges > 60)
        Sys_Error("Invalid water polygon size: %i", fa->numedges);
	for (int i=0 ; i<fa->numedges ; i++)
	{
		float *vec;
		int lindex = loadmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
			vec = loadmodel->vertexes[loadmodel->edges[lindex].v[0]].position;
		else
			vec = loadmodel->vertexes[loadmodel->edges[-lindex].v[1]].position;
		VectorCopy (vec, verts[numverts]);
		numverts++;
	}

	SubdividePolygon (numverts, verts[0]);
}


void R_SurfacePolygon(msurface_t *surface)
{
    glpoly_t *poly = Hunk_Alloc(sizeof(*poly) + (surface->numedges - 4) * sizeof(poly->verts[0]));
    int i;
    surface->polys = poly;
    poly->numverts = surface->numedges;
    for (i = 0; i < surface->numedges; i++) {
        int edge = loadmodel->surfedges[surface->firstedge + i];
        float *position = loadmodel->vertexes[loadmodel->edges[abs(edge)].v[edge > 0 ? 0 : 1]].position;
        VectorCopy(position, R_PolyPosition(poly, i));
        R_PolyUV(poly, i)[0] = DotProduct(position, surface->texinfo->vecs[0]);
        R_PolyUV(poly, i)[1] = DotProduct(position, surface->texinfo->vecs[1]);
    }
}

static float turbsin[] = {
#include "r_warp_sin.h"
};

static float R_WarpSine(float time)
{
    float phase = time * (256.0f / (2 * (float)M_PI));
    int index = (int)floorf(phase);
    float fraction = phase - index;
    return turbsin[index & 255] * (1 - fraction) + turbsin[(index + 1) & 255] * fraction;
}

static void R_EmitWarpPolygon(msurface_t *surface, int effect)
{
    static vertex_t *vertices;
    static int capacity;
    glpoly_t *poly;
    for (poly = surface->polys; poly; poly = poly->next) {
        hyena_arrays_t arrays = {0};
        if (poly->numverts < 3)
            continue;
        int i;
        if (poly->numverts > capacity) {
            vertex_t *resized = realloc(vertices, poly->numverts * sizeof(*vertices));
            if (!resized)
                Sys_Error("R_EmitWarpPolygon: out of memory");
            vertices = resized;
            capacity = poly->numverts;
        }
        for (i = 0; i < poly->numverts; i++) {
            const float *position = R_PolyPosition(poly, i), *uv = R_PolyUV(poly, i);
            float s = uv[0], t = uv[1];
            if (effect == 1) {
                s = (uv[0] + R_WarpSine(uv[1] * 0.125f + (float)cl.time)) / 64;
                t = (uv[1] + R_WarpSine(uv[0] * 0.125f + (float)cl.time)) / 64;
            } else if (effect == 2) {
                float time = (float)realtime * 0.5f;
                s -= 64 * (time - floorf(time));
            } else if (effect == 3) {
                vec3_t direction;
                float length;
                VectorSubtract(position, r_origin, direction);
                direction[2] *= 3;
                length = sqrtf(DotProduct(direction, direction));
                if (length > 0) {
                    s = direction[0] * (378.0f / 256) / length;
                    t = direction[1] * (378.0f / 256) / length;
                } else s = t = 0;
            }
            memcpy(&vertices[i].xyz, position, 3 * sizeof(float));
            vertices[i].uv.u = s;
            vertices[i].uv.v = t;
        }
        arrays.position.data = &vertices[0].xyz;
        arrays.uv.data = &vertices[0].uv;
        arrays.position.stride = arrays.uv.stride = sizeof(*vertices);
        arrays.count = poly->numverts;
        arrays.flags = HYE_CLIP_POLYGON | HYE_COPY_VERTICES;
        Hyena_DrawArrays(HYE_TRIANGLE_FAN, &arrays, poly->numverts);
    }
}

void EmitFlatPoly(msurface_t *surface) { R_EmitWarpPolygon(surface, 0); }
void EmitWaterPolys(msurface_t *surface) { R_EmitWarpPolygon(surface, 1); }
void EmitScrollPolys(msurface_t *surface) { R_EmitWarpPolygon(surface, 2); }
void EmitReflectivePolys(msurface_t *surface) { R_EmitWarpPolygon(surface, 3); }
