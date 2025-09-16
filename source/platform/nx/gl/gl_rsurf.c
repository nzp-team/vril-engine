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
// gl_rsurf.c: surface-related refresh code

#include "../../../nzportable_def.h"

/*
 * Can't decide on water warp strategy yet
 * - Warp stuff on the other side of the water surface
 * - or just always warp underwater surfaces...
 * - issues with moving bsp models...
 */
//#define WATER_WARP_THROUGH_SURFACE

#ifdef WATER_WARP_THROUGH_SURFACE
/* FIXME - this version of the test is broken somehow... */
#define WATER_WARP_TEST(surf)                                                                                          \
    (r_waterwarp.value && !((surf)->flags & SURF_DONTWARP) &&                                                          \
     ((r_viewleaf->contents == CONTENTS_EMPTY && ((surf)->flags & SURF_UNDERWATER)) ||                                 \
      (r_viewleaf->contents != CONTENTS_EMPTY && !((surf)->flags & SURF_UNDERWATER))))
#else
#define WATER_WARP_TEST(surf) (r_waterwarp.value && ((surf)->flags & SURF_UNDERWATER))
#endif

int		lightmap_bytes;		// 1, 2, or 4

unsigned		blocklights[3*18*18]; // LordHavoc: .lit support (*3 for RGB) to the definitions at the top

#define	BLOCK_WIDTH		128
#define	BLOCK_HEIGHT	128

int			active_lightmaps;

typedef struct glRect_s {
	unsigned char l,t,w,h;
} glRect_t;

glpoly_t	*lightmap_polys[MAX_LIGHTMAPS];
qboolean	lightmap_modified[MAX_LIGHTMAPS];
glRect_t	lightmap_rectchange[MAX_LIGHTMAPS];
int 		lightmap_index[MAX_LIGHTMAPS];

int			allocated[MAX_LIGHTMAPS][BLOCK_WIDTH];

// the lightmap texture data needs to be kept in
// main memory so texsubimage can update properly
byte		lightmaps[4*MAX_LIGHTMAPS*BLOCK_WIDTH*BLOCK_HEIGHT];

// For gl_texsort 0
msurface_t  *skychain = NULL;
msurface_t  *waterchain = NULL;

extern char	skybox_name[32];

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
	int32_t		smax, tmax;
	int32_t		t;
	int			i, j, size;
	uint8_t		*lightmap;
	unsigned	scale;
	int32_t			maps;
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


extern	int		solidskytexture;
extern	int		alphaskytexture;
extern	float	speedscale;		// for top sky and bottom sky

void DrawGLWaterPoly (glpoly_t *p);
void DrawGLWaterPolyLightmap (glpoly_t *p);

/*
================
DrawGLWaterPoly

Warp the vertex coordinates
================
*/
void DrawGLWaterPoly (glpoly_t *p)
{
	int		i;
	float	*v;
	vec3_t	nv;

	glBegin (GL_TRIANGLE_FAN);
	v = p->verts[0];
	for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
	{
		glTexCoord2f (v[3], v[4]);

		nv[0] = v[0] + 8*sinf(v[1]*0.05f+(float)realtime)*sinf(v[2]*0.05f+(float)realtime);
		nv[1] = v[1] + 8*sinf(v[0]*0.05f+(float)realtime)*sinf(v[2]*0.05f+(float)realtime);
		nv[2] = v[2];

		glVertex3fv (nv);
	}
	glEnd ();
}

void DrawGLWaterPolyLightmap (glpoly_t *p)
{
	int		i;
	float	*v;
	vec3_t	nv;

	glBegin (GL_TRIANGLE_FAN);
	v = p->verts[0];
	for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
	{
		glTexCoord2f (v[5], v[6]);

		nv[0] = v[0] + 8*sinf(v[1]*0.05f+(float)realtime)*sinf(v[2]*0.05f+(float)realtime);
		nv[1] = v[1] + 8*sinf(v[0]*0.05f+(float)realtime)*sinf(v[2]*0.05f+(float)realtime);
		nv[2] = v[2];

		glVertex3fv (nv);
	}
	glEnd ();
}

/*
================
DrawGLPoly
================
*/
void DrawGLPoly (glpoly_t *p)
{
	int		i;
	float	*v;

	glBegin (GL_POLYGON);
	v = p->verts[0];
	for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
	{
		glTexCoord2f (v[3], v[4]);
		glVertex3fv (v);
	}
	glEnd ();
}

// rbaldwin2 -- This is based on DrawGLWaterPolyLightmap and designed to be cheaper
void DrawGLPolyLightmap (glpoly_t *p)
{
	int		i;
	float	*v;
	vec3_t	nv;

	glBegin (GL_TRIANGLE_FAN);
	v = p->verts[0];
	for (i=0 ; i<p->numverts ; i++, v+= VERTEXSIZE)
	{
		glTexCoord2f (v[5], v[6]);

		nv[0] = v[0];
		nv[1] = v[1];
		nv[2] = v[2];

		glVertex3fv (nv);
	}
	glEnd ();
}

/*
================
R_BlendLightmaps
================
*/
void R_BlendLightmaps (void)
{
	int			i;
	glpoly_t	*p;

	if (r_fullbright.value) {
		return;
	} 

	glDepthMask(GL_FALSE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_DST_COLOR, GL_SRC_COLOR);
	glDepthFunc(GL_EQUAL);
	
	for (i=0 ; i<MAX_LIGHTMAPS ; i++)
	{
		p = lightmap_polys[i];
		if (!p)
			continue;

		char lm_name[16];
		if (lightmap_modified[i])
		{
			lightmap_modified[i] = false;
			lightmap_rectchange[i].l = BLOCK_WIDTH;
			lightmap_rectchange[i].t = BLOCK_HEIGHT;
			lightmap_rectchange[i].w = 0;
			lightmap_rectchange[i].h = 0;

			sprintf(lm_name,"lightmap%d",i);
			lightmap_index[i] = GL_LoadLMTexture (lm_name, BLOCK_WIDTH, BLOCK_HEIGHT, lightmaps+(i*BLOCK_WIDTH*BLOCK_HEIGHT*lightmap_bytes), true);
		}
		GL_Bind(lightmap_index[i]);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		for ( ; p ; p=p->chain)
		{
			if (p->flags & SURF_UNDERWATER)
				DrawGLWaterPolyLightmap (p);
			else
			{
				DrawGLPolyLightmap(p);
			}
		}
	}

	glDisable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDepthMask (GL_TRUE);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
}

/*
================
R_RenderBrushPoly
================
*/
void R_RenderBrushPoly (msurface_t *fa)
{
	texture_t	*t;
	uint8_t		*base;
	int32_t		maps;
	glRect_t    *theRect;
	int32_t		smax, tmax;

	c_brush_polys++;

	if (fa->flags & SURF_DRAWSKY)
	{	
		if (strcmp(skybox_name, "") == 0)
			EmitBothSkyLayers (fa);
		return;
	}
		
	t = R_TextureAnimation (fa->texinfo->texture);
	GL_Bind (t->gl_texturenum);

	bool choosealpha = t->name[0] == '{' ? true : false; // naievil -- need to choose alpha mode for certain textures
	if(choosealpha) {
		glEnable(GL_ALPHA_TEST);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glColor4f(1, 1, 1, 1);
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

	// add the poly to the proper lightmap chain

	fa->polys->chain = lightmap_polys[fa->lightmaptexturenum];
	lightmap_polys[fa->lightmaptexturenum] = fa->polys;

	// check for lightmap modification
	for (maps = 0 ; maps < MAXLIGHTMAPS && fa->styles[maps] != 255 ; maps++)
		if (d_lightstylevalue[fa->styles[maps]] != fa->cached_light[maps])
			goto dynamic;

	if (fa->dlightframe == r_framecount	// dynamic this frame
		|| fa->cached_dlight)			// dynamic previously
	{
dynamic:
		if (r_dynamic.value)
		{
			lightmap_modified[fa->lightmaptexturenum] = true;
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
			base = lightmaps + fa->lightmaptexturenum*lightmap_bytes*BLOCK_WIDTH*BLOCK_HEIGHT;
			base += fa->light_t * BLOCK_WIDTH * lightmap_bytes + fa->light_s * lightmap_bytes;
			R_BuildLightMap (fa, base, BLOCK_WIDTH*lightmap_bytes);
		}
	}

	if(choosealpha) {
		glDisable(GL_ALPHA_TEST);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
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
	mirror = true;
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

	if (r_wateralpha.value == 1.0f && gl_texsort.value)
		return;

	//
	// go back to the world matrix
	//

    glLoadMatrixf (r_world_matrix);

	if (r_wateralpha.value < 1.0f) {
		glEnable (GL_BLEND);
		glColor4f (1,1,1,r_wateralpha.value);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	}

	if (!gl_texsort.value) {
		if (!waterchain)
			return;

		for ( s = waterchain ; s ; s=s->texturechain) {
			GL_Bind (s->texinfo->texture->gl_texturenum);
			EmitWaterPolys (s);
		}
		
		waterchain = NULL;
	} else {

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

	if (r_wateralpha.value < 1.0f) {
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

		glColor4f (1,1,1,1);
		glDisable (GL_BLEND);
	}

}

/*
================
DrawTextureChains
================
*/
void DrawTextureChains (void)
{
	int			i;
	msurface_t	*s;
	texture_t	*t;

	if (!gl_texsort.value) {
		if (skychain) {
			//R_DrawSkyChain(skychain);
			skychain = NULL;
		}

		return;
	} 

	for (i=0 ; i<cl.worldmodel->numtextures ; i++)
	{
		t = cl.worldmodel->textures[i];
		if (!t)
			continue;
		s = t->texturechain;
		if (!s)
			continue;
		else if (i == mirrortexturenum && r_mirroralpha.value != 1.0f)
		{
			R_MirrorChain (s);
			continue;
		}
		else
		{
			if ((s->flags & SURF_DRAWTURB) && r_wateralpha.value != 1.0f)
				continue;	// draw translucent water later
			for ( ; s ; s=s->texturechain)
				R_RenderBrushPoly (s);
		}

		t->texturechain = NULL;
	}
}
/*
=================
R_DrawBrushModel
=================
*/
void R_DrawBrushModel (entity_t *e)
{
	int32_t		k;
	vec3_t		mins, maxs;
	int32_t		i;
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

	Platform_Graphics_Color(1,1,1,1);
	memset (lightmap_polys, 0, sizeof(lightmap_polys));

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
================
R_RecursiveWorldNode
================
*/
void R_RecursiveWorldNode (mnode_t *node)
{
	int32_t		c, side;
	mplane_t	*plane;
	msurface_t	*surf, **mark;
	mleaf_t		*pleaf;
	float		dot;

	if (node->contents == CONTENTS_SOLID)
		return;		// solid

	if (node->visframe != r_visframecount)
		return;
	if (R_CullBox (node->minmaxs, node->minmaxs+3))
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
	R_RecursiveWorldNode (node->children[side]);

// draw stuff
	c = node->numsurfaces;

	if (c)
	{
		surf = cl.worldmodel->surfaces + node->firstsurface;

		if (dot < (float)-BACKFACE_EPSILON)
			side = SURF_PLANEBACK;
		else if (dot > (float)BACKFACE_EPSILON)
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
				if (gl_texsort.value)
				{
					if (!mirror
					|| surf->texinfo->texture != cl.worldmodel->textures[mirrortexturenum])
					{
						surf->texturechain = surf->texinfo->texture->texturechain;
						surf->texinfo->texture->texturechain = surf;
					}
				} /*else if (surf->flags & SURF_DRAWSKY) {
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
	R_RecursiveWorldNode (node->children[!side]);
}


void R_AddBrushModelToChains (entity_t * e)
{
	model_t * clmodel = e->model;
	vec3_t mins, maxs;
	VectorAdd (e->origin, clmodel->mins, mins);
	VectorAdd (e->origin, clmodel->maxs, maxs);

	if (R_CullBox(mins, maxs))
	{
		return;
	}

	msurface_t * psurf = &clmodel->surfaces[clmodel->firstmodelsurface];

	/*
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
		if (((psurf->flags & SURF_PLANEBACK) && (dot < (float)-BACKFACE_EPSILON)) ||
			(!(psurf->flags & SURF_PLANEBACK) && (dot > (float)BACKFACE_EPSILON)))
		{
			// psurf->flags &= ~SURF_NEEDSCLIPPING;
			// psurf->flags |= SURF_NEEDSCLIPPING * (frustum_check > 1);

			psurf->texturechain = psurf->texinfo->texture->texturechain;
			psurf->texinfo->texture->texturechain = psurf;
		}
	}
}

void R_AddStaticBrushModelsToChains ()
{
	// Con_Printf("static models %d\n", cl_numstaticbrushmodels);
	for (int i = 0; i < cl_numstaticbrushmodels; i++)
	{
		// if (i >= 1) return;
		R_AddBrushModelToChains(cl_staticbrushmodels[i]);
	}
}

/*
=============
R_DrawWorld
=============
*/
void R_DrawWorld(void) {
    entity_t	ent;

	memset (&ent, 0, sizeof(ent));
	ent.model = cl.worldmodel;

	VectorCopy (r_refdef.vieworg, modelorg);

	currententity = &ent;

	Platform_Graphics_Color(1,1,1,1);
	memset (lightmap_polys, 0, sizeof(lightmap_polys));

	R_ClearSkyBox ();
	if (strcmp(skybox_name, "") != 0)
		R_DrawSkyBox();

	R_RecursiveWorldNode (cl.worldmodel->nodes);

	R_AddStaticBrushModelsToChains (); // shpuld
	
	Fog_SetupFrame(true);
	DrawTextureChains ();
	Fog_SetupFrame (false); //johnfitz

	R_BlendLightmaps();
}

/*
===============
R_MarkLeaves
===============
*/
void R_MarkLeaves (void)
{
	uint8_t	*vis;
	mnode_t	*node;
	int		i;
	uint8_t	solid[4096];

	if (r_oldviewleaf == r_viewleaf && !r_novis.value)
		return;
	
	if (mirror)
		return;

	r_visframecount++;
	r_oldviewleaf = r_viewleaf;

	if (r_novis.value)
	{
		vis = solid;
		memset (solid, 0xff, (cl.worldmodel->numleafs+7)>>3);
	}
	else
		vis = Mod_LeafPVS (r_viewleaf, cl.worldmodel);
		
	for (i=0 ; i<cl.worldmodel->numleafs ; i++)
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
	int			i, j;
	int32_t		best, best2;
	int			texnum;

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
	int32_t	i, lindex, lnumverts;
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
	int32_t	smax, tmax;
	uint8_t		*base;

	if (surf->flags & (SURF_DRAWSKY|SURF_DRAWTURB))
		return;

	smax = (surf->extents[0]>>4)+1;
	tmax = (surf->extents[1]>>4)+1;

	surf->lightmaptexturenum = AllocBlock (smax, tmax, &surf->light_s, &surf->light_t);
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

	r_framecount = 1;		// no dlightcache

	gl_lightmap_format = GL_RGBA;
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
		lightmap_index[i] = GL_LoadLMTexture (lm_name, BLOCK_WIDTH, BLOCK_HEIGHT, lightmaps+(i*BLOCK_WIDTH*BLOCK_HEIGHT*lightmap_bytes), false);
	}
}
