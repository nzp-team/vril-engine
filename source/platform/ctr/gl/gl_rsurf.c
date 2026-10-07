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

#include "../../../nzportable_def.h"

#ifndef GL_RGBA4
#define	GL_RGBA4	0
#endif


int		lightmap_bytes;		// 1, 2, or 4

unsigned		blocklights[3*18*18]; // LordHavoc: .lit support (*3 for RGB) to the definitions at the top

#define	BLOCK_WIDTH		256
#define	BLOCK_HEIGHT	256

int			active_lightmaps;

typedef struct glRect_s {
	unsigned short l,t,w,h;
} glRect_t;

qboolean	lightmap_modified[MAX_LIGHTMAPS];
glRect_t	lightmap_rectchange[MAX_LIGHTMAPS];
int 		lightmap_index[MAX_LIGHTMAPS];

int			allocated[MAX_LIGHTMAPS][BLOCK_WIDTH];

// the lightmap texture data needs to be kept in
// main memory so texsubimage can update properly
byte		*lightmaps;
static int	lightmap_pages;

static void R_ReserveLightmapPages (int pages)
{
	byte *storage;
	size_t old_size, new_size;

	if (pages <= lightmap_pages)
		return;
	old_size = (size_t)lightmap_pages * BLOCK_WIDTH * BLOCK_HEIGHT * 4;
	new_size = (size_t)pages * BLOCK_WIDTH * BLOCK_HEIGHT * 4;
	storage = realloc (lightmaps, new_size);
	if (!storage)
		Sys_Error ("R_ReserveLightmapPages: out of memory\\n");
	lightmaps = storage;
	memset (lightmaps + old_size, 0, new_size - old_size);
	lightmap_pages = pages;
}


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
	//johnfitz -- lit support via lordhavoc
	float		cred, cgreen, cblue, brightness;
	unsigned	*bl;
	//johnfitz

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	tex = surf->texinfo;

	for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
	{
		if ( !(surf->dlightbits & (1U<<lnum) ) )
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

		//johnfitz -- lit support via lordhavoc
		bl = blocklights;
		cred = cl_dlights[lnum].color[0] * 256.0f;
		cgreen = cl_dlights[lnum].color[1] * 256.0f;
		cblue = cl_dlights[lnum].color[2] * 256.0f;
		//johnfitz
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
				//johnfitz -- lit support via lordhavoc
				{
					brightness = rad - dist;
					bl[0] += (int) (brightness * cred);
					bl[1] += (int) (brightness * cgreen);
					bl[2] += (int) (brightness * cblue);
				}
				bl += 3;
				//johnfitz
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

	surf->cached_dlight = (surf->dlightframe == r_framecount);

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;
	size = smax*tmax;
	lightmap = surf->samples;

// set to full bright if no light data
	if (r_fullbright.value || !cl.worldmodel->lightdata)
	{
		// LordHavoc: .lit support begin
		bl = blocklights;
		for (i=0 ; i<size ; i++)
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
	for (i=0 ; i<size ; i++)
	{
		*bl++ = 0;
		*bl++ = 0;
		*bl++ = 0;
	}
	// LordHavoc: .lit support end	
	
// add all the lightmaps
	if (lightmap){
		for (maps = 0; maps < MAXLIGHTMAPS && surf->styles[maps] != 255; maps++)
		{
			scale = (float)d_lightstylevalue[surf->styles[maps]];
			surf->cached_light[maps] = scale;	// 8.8 fraction
			// LordHavoc: .lit support begin
			bl = blocklights;
			for (i=0 ; i<size ; i++)
			{
				*bl++ += *lightmap++ * scale;
				*bl++ += *lightmap++ * scale;
				*bl++ += *lightmap++ * scale;
			}
			// LordHavoc: .lit support end
		}
	}
	
// add all the dynamic lights
	if (surf->dlightframe == r_framecount)
		R_AddDynamicLights (surf);

// bound, invert, and shift
store:
	stride -= (smax<<2);
	bl = blocklights;
	for (i=0 ; i<tmax ; i++, dest += stride)
	{
		for (j=0 ; j<smax ; j++)
		{
			// LordHavoc: .lit support begin
			// LordHavoc: positive lighting (would be 255-t if it were inverse like glquake was)
			t = bl[0] >> 7;if (t > 255) t = 255;*dest++ = t;
			t = bl[1] >> 7;if (t > 255) t = 255;*dest++ = t;
			t = bl[2] >> 7;if (t > 255) t = 255;*dest++ = t;
			bl += 3;
			*dest++ = 255;
			// LordHavoc: .lit support end
		}
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


void DrawGLWaterPoly (glpoly_t *p);

/*
================
DrawGLWaterPoly

Warp the vertex coordinates
================
*/
void DrawGLWaterPoly (glpoly_t *p)
{
	R_DrawSurfaceFan(p->verts[0], p->numverts, VERTEXSIZE, 0, 3, true, realtime);
}

/*
================
DrawGLPoly
================
*/
void DrawGLPoly (glpoly_t *p)
{
	R_DrawSurfaceFan(p->verts[0], p->numverts, VERTEXSIZE, 0, 3, false, 0);
}

/*
================
R_BlendLightmaps
================
*/
int R_UploadLightmap(int i)
{
    char name[16];

    if (lightmap_modified[i]) {
        glRect_t rect = lightmap_rectchange[i];

        lightmap_modified[i] = false;
        lightmap_rectchange[i].l = BLOCK_WIDTH;
        lightmap_rectchange[i].t = BLOCK_HEIGHT;
        lightmap_rectchange[i].w = 0;
        lightmap_rectchange[i].h = 0;
        snprintf(name, sizeof(name), "lightmap%d", i);
        lightmap_index[i] = Hyena_UpdateLightmap(name, BLOCK_WIDTH, BLOCK_HEIGHT,
            lightmaps + i * BLOCK_WIDTH * BLOCK_HEIGHT * lightmap_bytes,
            HYE_TEXTURE_RGBA8, rect.l, rect.t, rect.w, rect.h,
            BLOCK_WIDTH * lightmap_bytes);
    }
    return lightmap_index[i];
}

void
R_UpdateSurfaceLightmap(msurface_t *surface)
{
    byte *base;
    glRect_t *rect;
    int maps, smax, tmax;

    for (maps = 0; maps < MAXLIGHTMAPS && surface->styles[maps] != 255; ++maps) {
        if (d_lightstylevalue[surface->styles[maps]] != surface->cached_light[maps])
            break;
	}
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
    base = lightmaps + surface->lightmaptexturenum * lightmap_bytes * BLOCK_WIDTH * BLOCK_HEIGHT;
    base += surface->light_t * BLOCK_WIDTH * lightmap_bytes + surface->light_s * lightmap_bytes;
    R_BuildLightMap(surface, base, BLOCK_WIDTH * lightmap_bytes);
}

/*
================
R_RenderBrushPoly
================
*/
void R_RenderBrushPoly (msurface_t *fa)
{
	texture_t	*t;

	c_brush_polys++;

	if (fa->flags & SURF_DRAWSKY)
		return;
		
	t = R_TextureAnimation (fa->texinfo->texture);
	Hyena_BindTexture(t->gl_texturenum);

	bool choosealpha = t->name[0] == '{' ? true : false; // naievil -- need to choose alpha mode for certain textures
	if(choosealpha) {
		glEnable(GL_ALPHA_TEST);
		glAlphaFunc(GL_GREATER, 0xaa);
		glEnable(GL_MODULATE);
		Hyena_SetColor(1, 1, 1, 1);
	}

	if (fa->flags & SURF_DRAWTURB)
	{	// warp texture, no lightmaps
		EmitWaterPolys (fa);
		return;
	}

	if (fa->flags & TEXFLAG_NODRAW)
		return;
	
	if (fa->flags & TEXFLAG_LIGHT) {
		DrawGLPoly(fa->polys);
		return;
	}

	if (fa->flags & SURF_UNDERWATER)
		DrawGLWaterPoly (fa->polys);
	else
		DrawGLPoly (fa->polys);

	R_ChainLightmap(fa);
	R_UpdateSurfaceLightmap(fa);

	if(choosealpha) {
		glDisable(GL_ALPHA_TEST);
		glDisable(GL_MODULATE);
	}

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

	currententity = e;

	clmodel = e->model;

	if (e->angles[0] || e->angles[1] || e->angles[2])
	{
		rotated = true;
		for (i=0 ; i<3 ; i++)
		{
			mins[i] = e->origin[i] - clmodel->radius;
			maxs[i] = e->origin[i] + clmodel->radius;
		}
	}
	else
	{
		rotated = false;
		VectorAdd (e->origin, clmodel->mins, mins);
		VectorAdd (e->origin, clmodel->maxs, maxs);
	}

	if (R_CullBox (mins, maxs))
		return;

	Hyena_SetColor(1,1,1,1);
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
	if (clmodel->firstmodelsurface != 0 && !gl_flashblend.value)
	{
		for (k=0 ; k<MAX_DLIGHTS ; k++)
		{
			if ((cl_dlights[k].die < (float)cl.time) ||
				(!cl_dlights[k].radius))
				continue;

			R_MarkLights (&cl_dlights[k], 1<<k,
				clmodel->nodes + clmodel->hulls[0].firstclipnode);
		}
	}

    glPushMatrix ();

    // naievil -- fixme
    /*
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
	else if (ISSOLID(e))
	{
		sceGuEnable(GU_ALPHA_TEST);
		int c = (int)(e->renderamt * 255.0f);
		sceGuAlphaFunc(GU_GREATER, c, 0xff);
		dlight = false;
	}
	else if (ISGLOW(e))
	{
		sceGuTexFunc(GU_TFX_MODULATE , GU_TCC_RGBA);
		sceGuDepthMask(GU_TRUE);
		sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0xFFFFFFFF);
		R_GlowSetupBegin(e);
	}
	else if (ISTEXTURE(e))
	{
		sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
		sceGuColor(GU_RGBA(255, 255, 255, (int)(e->renderamt * 255.0f)));
		dlight = false;
	}
	else if (ISCOLOR(e))
	{
		sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
		sceGuColor(GU_RGBA((int)(e->rendercolor[0] * 255.0f),
			(int)(e->rendercolor[1] * 255.0f),
			(int)(e->rendercolor[2] * 255.0f), 255));
	}
	*/

	e->angles[0] = -e->angles[0];	// stupid quake bug
	R_RotateForEntity (e, e->scale);
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
		if (((psurf->flags & SURF_PLANEBACK) && (dot < (float)-BACKFACE_EPSILON)) ||
			(!(psurf->flags & SURF_PLANEBACK) && (dot > (float)BACKFACE_EPSILON)))
		{
			R_RenderBrushPoly (psurf);
		}
	}

	R_BlendLightmaps();
	glPopMatrix ();
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

	for (texnum=0 ; texnum<MAX_LIGHTMAPS ; texnum++)
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
	return -1; //johnfitz -- shut up compiler
}


mvertex_t	*r_pcurrentvertbase;
model_t		*currentmodel;

int	nColinElim;

/*
================
BuildSurfaceDisplayList
================
*/
void BuildSurfaceDisplayList (msurface_t *fa)
{
	int			i, lindex, lnumverts;
	medge_t		*pedges, *r_pedge;
	float		*vec;
	float		s, t;
	glpoly_t	*poly;

// reconstruct the polygon
	pedges = currentmodel->edges;
	lnumverts = fa->numedges;

	//
	// draw texture
	//
	poly = Hunk_Alloc (sizeof(glpoly_t) + (lnumverts-4) * VERTEXSIZE*sizeof(float));
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

		VectorCopy (vec, poly->verts[i]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;

		//
		// lightmap texture coordinates
		//
		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s -= fa->texturemins[0];
		s += fa->light_s*16;
		s += 8;
		s /= BLOCK_WIDTH*16; //fa->texinfo->texture->width;

		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t -= fa->texturemins[1];
		t += fa->light_t*16;
		t += 8;
		t /= BLOCK_HEIGHT*16; //fa->texinfo->texture->height;

		poly->verts[i][5] = s;
		poly->verts[i][6] = t;
	}

	//
	// remove co-linear points - Ed
	//
	if (!gl_keeptjunctions.value && !(fa->flags & SURF_UNDERWATER) )
	{
		for (i = 0 ; i < lnumverts ; ++i)
		{
			vec3_t v1, v2;
			float *prev, *this, *next;

			prev = poly->verts[(i + lnumverts - 1) % lnumverts];
			this = poly->verts[i];
			next = poly->verts[(i + 1) % lnumverts];

			VectorSubtract( this, prev, v1 );
			VectorNormalize( v1 );
			VectorSubtract( next, prev, v2 );
			VectorNormalize( v2 );

			// skip co-linear points
			#define COLINEAR_EPSILON 0.001f
			if ((fabsf( v1[0] - v2[0] ) <= COLINEAR_EPSILON) &&
				(fabsf( v1[1] - v2[1] ) <= COLINEAR_EPSILON) && 
				(fabsf( v1[2] - v2[2] ) <= COLINEAR_EPSILON))
			{
				int j;
				for (j = i + 1; j < lnumverts; ++j)
				{
					int k;
					for (k = 0; k < VERTEXSIZE; ++k)
						poly->verts[j - 1][k] = poly->verts[j][k];
				}
				--lnumverts;
				++nColinElim;
				// retry next vertex next time, which is now current vertex
				--i;
			}
		}
	}
	poly->numverts = lnumverts;

}

/*
========================
GL_CreateSurfaceLightmap
========================
*/
void GL_CreateSurfaceLightmap (msurface_t *surf)
{
	int		smax, tmax;
	byte	*base;

	if (surf->flags & (SURF_DRAWSKY|SURF_DRAWTURB))
		return;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;

	surf->lightmaptexturenum = AllocBlock (smax, tmax, &surf->light_s, &surf->light_t);
	R_ReserveLightmapPages (surf->lightmaptexturenum + 1);
	base = lightmaps + surf->lightmaptexturenum*lightmap_bytes*BLOCK_WIDTH*BLOCK_HEIGHT;
	base += (surf->light_t * BLOCK_WIDTH + surf->light_s) * lightmap_bytes;
	R_BuildLightMap (surf, base, BLOCK_WIDTH*lightmap_bytes);
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

	memset (allocated, 0, sizeof(allocated));
	free (lightmaps);
	lightmaps = NULL;
	lightmap_pages = 0;

	r_framecount = 1;		// no dlightcache

	lightmap_bytes = 4;

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
	for (i=0 ; i<MAX_LIGHTMAPS ; i++)
	{
		if (!allocated[i][0])
			break;		// no more used
		lightmap_modified[i] = false;
		lightmap_rectchange[i].l = BLOCK_WIDTH;
		lightmap_rectchange[i].t = BLOCK_HEIGHT;
		lightmap_rectchange[i].w = 0;
		lightmap_rectchange[i].h = 0;

		char lm_name[16];

		sprintf(lm_name,"lightmap%d",i);
		lightmap_index[i] = Hyena_LoadLightmap(lm_name, BLOCK_WIDTH, BLOCK_HEIGHT, lightmaps+(i*BLOCK_WIDTH*BLOCK_HEIGHT*lightmap_bytes), HYE_TEXTURE_RGBA8, false);
	}
	{
        const r_world_layout_t layout = { offsetof(glpoly_t, verts), VERTEXSIZE, 0, 3, 5, 0, 0,
            SURF_DRAWSKY | SURF_DRAWTURB | SURF_UNDERWATER | TEXFLAG_NODRAW,
            TEXFLAG_LIGHT, 4, false, NULL, NULL };
        R_BuildWorldBatch(&layout);
    }
}
