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
// r_main.c

#include "../../../nzportable_def.h"

void R_MarkLeaves (void);

entity_t r_worldentity;

vec3_t		modelorg, r_entorigin;
entity_t	*currententity;

vec3_t r_entorigin;
int r_visframecount; // bumped when going to a new PVS
int r_framecount;    // used for dlight push checking

static mplane_t frustum[4];

int c_lightmaps_uploaded;
int c_brush_polys;
int c_alias_polys;

qboolean envmap; // true during envmap command capture

int			cnttextures[2] = {-1, -1};     // cached

GLuint currenttexture = -1;         // to avoid unnecessary texture sets
GLuint playertextures[MAX_CLIENTS]; // up to 16 color translated skins

int mirrortexturenum; // quake texturenum, not gltexturenum
qboolean mirror;
mplane_t *mirror_plane;

//
// view origin
//
vec3_t vup;
vec3_t vpn;
vec3_t vright;
vec3_t r_origin;

float r_world_matrix[16];

//
// screen size info
//
refdef_t	r_refdef;

mleaf_t *r_viewleaf, *r_oldviewleaf;
texture_t *r_notexture_mip;
int d_lightstylevalue[256]; // 8.8 fraction of base light value

cvar_t r_norefresh = {"r_norefresh", "0"};
cvar_t r_drawentities = {"r_drawentities", "1"};
cvar_t r_drawviewmodel = {"r_drawviewmodel", "1"};
cvar_t r_speeds = {"r_speeds", "0"};
cvar_t r_lightmap = {"r_lightmap", "0"};
cvar_t r_shadows = {"r_shadows", "0"};
cvar_t r_mirroralpha = {"r_mirroralpha", "1"};
cvar_t r_wateralpha = {"r_wateralpha", "1", true};
cvar_t r_dynamic = {"r_dynamic", "1"};
cvar_t r_novis = {"r_novis", "0"};
cvar_t r_skyfog = {"r_skyfog", "1"};

#ifdef QW_HACK
cvar_t r_netgraph = {"r_netgraph", "0"};
#endif
cvar_t r_waterwarp = {"r_waterwarp", "1"};

cvar_t r_fullbright = {"r_fullbright", "0"};
cvar_t gl_keeptjunctions = {"gl_keeptjunctions", "1"};
cvar_t gl_reporttjunctions = {"gl_reporttjunctions", "0"};
cvar_t gl_texsort = {"gl_texsort", "1"};

cvar_t gl_finish = {"gl_finish", "0"};
cvar_t gl_clear = {"gl_clear", "0"};
cvar_t gl_cull = {"gl_cull", "1"};
cvar_t gl_smoothmodels = {"gl_smoothmodels", "1"};
cvar_t gl_affinemodels = {"gl_affinemodels", "0"};
cvar_t gl_polyblend = {"gl_polyblend", "1"};
cvar_t gl_flashblend = {"gl_flashblend", "1"};
cvar_t gl_playermip = {"gl_playermip", "0"};
cvar_t gl_nocolors = {"gl_nocolors", "0"};
cvar_t gl_zfix = {"gl_zfix", "0"};
#ifdef NQ_HACK
cvar_t gl_doubleeyes = {"gl_doubleeyes", "1"};
#endif

cvar_t _gl_allowgammafallback = {"_gl_allowgammafallback", "1"};

cvar_t  r_model_brightness  = { "r_model_brightness", "1", true};   // Toggle high brightness model lighting

#ifdef NQ_HACK
/*
 * incomplete model interpolation support
 * -> default to off and don't save to config for now
 */
cvar_t r_lerpmodels = {"r_lerpmodels", "0", false};
cvar_t r_lerpmove = {"r_lerpmove", "0", false};
#endif

extern 	cvar_t 	scr_fov_viewmodel;

/*
=================
R_CullBox

Returns true if the box is completely outside the frustum
=================
*/
qboolean R_CullBox(vec3_t mins, vec3_t maxs) {
    int i;

    for (i = 0; i < 4; i++)
        /* Not using macro since frustum planes generally not axis aligned */
        if (BoxOnPlaneSide(mins, maxs, &frustum[i]) == 2)
            return true;
    return false;
}

void R_RotateForEntity (entity_t *e, unsigned char scale)
{
    glTranslatef (e->origin[0],  e->origin[1],  e->origin[2]);

    glRotatef (e->angles[1],  0, 0, 1);
    glRotatef (-e->angles[0],  0, 1, 0);
    glRotatef (e->angles[2],  1, 0, 0);

	if (scale != ENTSCALE_DEFAULT && scale != 0) {
		float scalefactor = ENTSCALE_DECODE(scale);
		glScalef(scalefactor, scalefactor, scalefactor);
	}
}

void IgnoreInterpolatioFrame (entity_t *e, aliashdr_t *paliashdr)
{
	if (strcmp(e->old_model, e->model->name) && e->model != NULL)
	{
		strcpy(e->old_model, e->model->name);
        // fenix@io.com: model transform interpolation
        e->frame_start_time     = 0;
        e->translate_start_time = 0;
        e->rotate_start_time    = 0;
        e->pose1    = 0;
        e->pose2    = paliashdr->frames[e->frame].firstpose;
	}
}

/*
=============================================================

  SPRITE MODELS

=============================================================
*/

/*
================
R_GetSpriteFrame
================
*/
mspriteframe_t *R_GetSpriteFrame (entity_t *currententity)
{
	msprite_t		*psprite;
	mspritegroup_t	*pspritegroup;
	mspriteframe_t	*pspriteframe;
	int				i, numframes, frame;
	float			*pintervals, fullinterval, targettime, time;

	psprite = currententity->model->cache.data;
	frame = currententity->frame;

	if ((frame >= psprite->numframes) || (frame < 0))
	{
		Con_Printf ("R_DrawSprite: no such frame %d\n", frame);
		frame = 0;
	}

	if (psprite->frames[frame].type == SPR_SINGLE)
	{
		pspriteframe = psprite->frames[frame].frameptr;
	}
	else
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes-1];

		time = (float)cl.time + currententity->syncbase;

	// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
	// are positive, so we don't have to worry about division by 0
		targettime = time - ((int)(time / fullinterval)) * fullinterval;

		for (i=0 ; i<(numframes-1) ; i++)
		{
			if (pintervals[i] > targettime)
				break;
		}

		pspriteframe = pspritegroup->frames[i];
	}

	return pspriteframe;
}


/*
=================
R_DrawSpriteModel

=================
*/
void R_DrawSpriteModel (entity_t *e)
{
	vec3_t	point;
	mspriteframe_t	*frame;
	float		*up, *right;
	vec3_t		v_forward, v_right, v_up;
	msprite_t		*psprite;
	float 			scale = ENTSCALE_DECODE(e->scale);
	if (scale == 0) scale = 1.0f;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache
	frame = R_GetSpriteFrame (e);
	psprite = currententity->model->cache.data;

	if (psprite->type == SPR_ORIENTED)
	{	// bullet marks on walls
		AngleVectors (currententity->angles, v_forward, v_right, v_up);
		up = v_up;
		right = v_right;
	}
	else
	{	// normal sprite
		up = vup;
		right = vright;
	}

	glColor3f (1,1,1);

  GL_Bind(frame->gl_texturenum);

	Fog_DisableGFog ();

	glDisable (GL_ALPHA_TEST);
	glEnable (GL_BLEND);
	glDepthMask(GL_FALSE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glBegin (GL_QUADS);

	glTexCoord2f (0, 1);
	VectorMA (e->origin, frame->down * scale, up, point);
	VectorMA (point, frame->left * scale, right, point);
	glVertex3fv (point);

	glTexCoord2f (0, 0);
	VectorMA (e->origin, frame->up * scale, up, point);
	VectorMA (point, frame->left * scale, right, point);
	glVertex3fv (point);

	glTexCoord2f (1, 0);
	VectorMA (e->origin, frame->up * scale, up, point);
	VectorMA (point, frame->right * scale, right, point);
	glVertex3fv (point);

	glTexCoord2f (1, 1);
	VectorMA (e->origin, frame->down * scale, up, point);
	VectorMA (point, frame->right * scale, right, point);
	glVertex3fv (point);
	
	glEnd ();
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);

	Fog_EnableGFog ();
}

/*
=============================================================

  ALIAS MODELS

=============================================================
*/


#define NUMVERTEXNORMALS	162

float	r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "../../../anorms.h"
};

vec3_t	shadevector;
float	shadelight, ambientlight;
extern vec3_t lightcolor; // LordHavoc: .lit support to the definitions at the top

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
float	r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "../../../anorm_dots.h"
;

float	*shadedots = r_avertexnormal_dots[0];

int	lastposenum;

/*
=============
GL_DrawAliasFrame -- johnfitz -- rewritten to support colored light, lerping, entalpha, multitexture, and r_drawflat
=============
*/
void GL_DrawAliasFrame (aliashdr_t *paliashdr, int posenum)
{
	trivertx_t *verts;
	int		*commands;
	int		count;
	float	u,v;

	verts = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	commands = (int *)((byte *)paliashdr + paliashdr->commands);

	glColor4f(lightcolor[0]/255, lightcolor[1]/255, lightcolor[2]/255, 1.0f);

	while (1)
	{
		// get the vertex count and primitive type
		count = *commands++;
		if (!count)
			break;		// done

		if (count < 0)
		{
			count = -count;
			glBegin (GL_TRIANGLE_FAN);
		}
		else
			glBegin (GL_TRIANGLE_STRIP);

		do
		{
			u = ((float *)commands)[0];
			v = ((float *)commands)[1];

			glTexCoord2f (u, v);

			commands += 2;

			glVertex3f (verts->v[0], verts->v[1], verts->v[2]);
			verts++;
		} while (--count);

		glEnd ();
	}
}


/*
=============
GL_DrawAliasBlendedFrame

fenix@io.com: model animation interpolation
=============
*/
// fenix@io.com: model animation interpolation
int lastposenum0;
void GL_DrawAliasBlendedFrame (aliashdr_t *paliashdr, int pose1, int pose2, float blend)
{
	// if (r_showtris.value)
	// {
	// 	GL_DrawAliasBlendedWireFrame(paliashdr, pose1, pose2, blend);
	// 	return;
	// }
	trivertx_t* verts1;
	trivertx_t* verts2;
	vec3_t	  d;
	int		*commands;
	int		count;
	float	u,v;

	lastposenum0 = pose1;
	lastposenum  = pose2;

	verts1 = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts2 = verts1;

	verts1 += pose1 * paliashdr->poseverts;
	verts2 += pose2 * paliashdr->poseverts;

	commands = (int *)((byte *)paliashdr + paliashdr->commands);

	glColor4f(lightcolor[0]/255, lightcolor[1]/255, lightcolor[2]/255, 1.0f);

	while (1)
	{
		// get the vertex count and primitive type
		count = *commands++;
		if (!count)
			break;		// done

		if (count < 0)
		{
			count = -count;
			glBegin (GL_TRIANGLE_FAN);
		}
		else
			glBegin (GL_TRIANGLE_STRIP);

		do
		{
			u = ((float *)commands)[0];
			v = ((float *)commands)[1];

			glTexCoord2f (u, v);

			commands += 2;

			VectorSubtract(verts2->v, verts1->v, d);
			glVertex3f (verts1->v[0] + (blend * d[0]), verts1->v[1] + (blend * d[1]), verts1->v[2] + (blend * d[2]));
			verts1++;
			verts2++;
		} while (--count);

		glEnd ();
	}
}

/*
=============
GL_DrawAliasShadow
=============
*/
extern	vec3_t			lightspot;

void GL_DrawAliasShadow (aliashdr_t *paliashdr, int posenum)
{
	trivertx_t	*verts;
	int		*order;
	vec3_t	point;
	float	height, lheight;
	int		count;

	lheight = currententity->origin[2] - lightspot[2];

	height = 0;
	verts = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	order = (int *)((byte *)paliashdr + paliashdr->commands);

	height = -lheight + 1.0f;

	while (1)
	{
		// get the vertex count and primitive type
		count = *order++;
		if (!count)
			break;		// done
		if (count < 0)
		{
			count = -count;
			glBegin (GL_TRIANGLE_FAN);
		}
		else
			glBegin (GL_TRIANGLE_STRIP);

		do
		{
			// texture coordinates come from the draw list
			// (skipped for shadows) glTexCoord2fv ((float *)order);
			order += 2;

			// normals and vertexes come from the frame list
			point[0] = verts->v[0] * paliashdr->scale[0] + paliashdr->scale_origin[0];
			point[1] = verts->v[1] * paliashdr->scale[1] + paliashdr->scale_origin[1];
			point[2] = verts->v[2] * paliashdr->scale[2] + paliashdr->scale_origin[2];

			point[0] -= shadevector[0]*(point[2]+lheight);
			point[1] -= shadevector[1]*(point[2]+lheight);
			point[2] = height;
//			height -= 0.001;
			glVertex3fv (point);

			verts++;
		} while (--count);

		glEnd ();
	}	
}



/*
=================
R_SetupAliasFrame
=================
*/
void R_SetupAliasFrame (int frame, aliashdr_t *paliashdr)
{
	int				pose, numposes;
	float			interval;

	if ((frame >= paliashdr->numframes) || (frame < 0))
	{
		Con_DPrintf ("R_AliasSetupFrame: no such frame %d\n", frame);
		frame = 0;
	}

	pose = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	if (numposes > 1)
	{
		interval = paliashdr->frames[frame].interval;
		pose += (int)((float)cl.time / interval) % numposes;
	}
	
	GL_DrawAliasFrame(paliashdr, pose);
}

/*
=================
R_SetupAliasBlendedFrame

fenix@io.com: model animation interpolation
=================
*/
//double t1, t2, t3;

void R_SetupAliasBlendedFrame (int frame, aliashdr_t *paliashdr, entity_t* e)
{
	int   pose;
	int   numposes;
	float blend;

	if ((frame >= paliashdr->numframes) || (frame < 0))
	{
		Con_DPrintf ("R_AliasSetupFrame: no such frame %d\n", frame);
		frame = 0;
	}

	pose = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	if (numposes > 1)
	{
		e->frame_interval = paliashdr->frames[frame].interval;
		pose += (int)((float)cl.time / e->frame_interval) % numposes;
	}
	else
	{
		/* One tenth of a second is a good for most Quake animations.
		If the nextthink is longer then the animation is usually meant to pause
		(e.g. check out the shambler magic animation in shambler.qc).  If its
		shorter then things will still be smoothed partly, and the jumps will be
		less noticable because of the shorter time.  So, this is probably a good
		assumption. */
		e->frame_interval = 0.1;
	}

	if (e->pose2 != pose)
	{
		e->frame_start_time = realtime;
		e->pose1 = e->pose2;
		e->pose2 = pose;
		blend = 0;
	}
	else
		blend = ((float)realtime - e->frame_start_time) / e->frame_interval;
	// wierd things start happening if blend passes 1
	if (cl.paused || blend > 1) blend = 1;

	if (blend == 1)
		GL_DrawAliasFrame (paliashdr, pose);
	else
		GL_DrawAliasBlendedFrame (paliashdr, e->pose1, e->pose2, blend);
}

/*
=================
R_DrawZombieLimb

=================
*/
//Blubs Z hacks: need this declaration.
model_t *Mod_FindName (char *name);

void R_DrawZombieLimb (entity_t *e, int which)
{
	model_t		*clmodel;
	aliashdr_t	*paliashdr;
	entity_t 	*limb_ent;

	switch(which) {
		case 1:
			limb_ent = &cl_entities[e->z_head];
			break;
		case 2:
			limb_ent = &cl_entities[e->z_larm];
			break;
		case 3:
			limb_ent = &cl_entities[e->z_rarm];
			break;
		default:
			return;
	}

	clmodel = limb_ent->model;

	if (clmodel == NULL)
		return;

	VectorCopy(e->origin, r_entorigin);
	VectorSubtract(r_origin, r_entorigin, modelorg);

	// locate the proper data
	paliashdr = (aliashdr_t *)Mod_Extradata(clmodel);//e->model
	c_alias_polys += paliashdr->numtris;

	//Shpuld
	if(r_model_brightness.value)
	{
		lightcolor[0] += 48;
		lightcolor[1] += 48;
		lightcolor[2] += 48;
	}

	glPushMatrix ();
	R_RotateForEntity (e, e->scale);

	glTranslatef (paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
	glScalef (paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);

	if (gl_smoothmodels.value)
		glShadeModel (GL_SMOOTH);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	if (gl_affinemodels.value)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

	IgnoreInterpolatioFrame(e, paliashdr);
	R_SetupAliasBlendedFrame (currententity->frame, paliashdr, e);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	glShadeModel (GL_FLAT);
	if (gl_affinemodels.value)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	glPopMatrix ();
}

/*
=================
R_DrawTransparentAliasModel

=================
*/
void R_DrawTransparentAliasModel (entity_t *e)
{
	model_t		*clmodel;
	vec3_t		mins, maxs;
	aliashdr_t	*paliashdr;
	int			anim;

	clmodel = currententity->model;

	VectorAdd (currententity->origin, clmodel->mins, mins);
	VectorAdd (currententity->origin, clmodel->maxs, maxs);

// naievil -- fixme: on psp this is == 2 ? 
	if (R_CullBox (mins, maxs))
		return;

	VectorCopy (currententity->origin, r_entorigin);
	VectorSubtract (r_origin, r_entorigin, modelorg);

	// for(int g = 0; g < 3; g++)
	// {
	// 	if(lightcolor[g] < 8)
	// 		lightcolor[g] = 8;
	// 	if(lightcolor[g] > 125)
	// 		lightcolor[g] = 125;
	// }

	// //
	// // get lighting information
	// //

	// ambientlight = shadelight = R_LightPoint (currententity->origin);
	// for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
	// {
	// 	if (cl_dlights[lnum].die >= cl.time)
	// 	{
	// 		VectorSubtract (currententity->origin,
	// 						cl_dlights[lnum].origin,
	// 						dist);
	// 		add = cl_dlights[lnum].radius - Length(dist);

	// 		if (add > 0) {
	// 			ambientlight += add;
	// 			//ZOID models should be affected by dlights as well
	// 			shadelight += add;
	// 		}
	// 	}
	// }

	// // clamp lighting so it doesn't overbright as much
	// if (ambientlight > 128)
	// 	ambientlight = 128;
	// if (ambientlight + shadelight > 192)
	// 	shadelight = 192 - ambientlight;

	// shadedots = r_avertexnormal_dots[((int)(e->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];
	// shadelight = shadelight / 200.0;
	
	// an = e->angles[1]/180*M_PI;
	// shadevector[0] = cos(-an);
	// shadevector[1] = sin(-an);
	// shadevector[2] = 1;
	// VectorNormalize (shadevector);

	//
	// locate the proper data
	//
	paliashdr = (aliashdr_t *)Mod_Extradata (e->model);
	c_alias_polys += paliashdr->numtris;

	//
	// draw all the triangles
	//

	lightcolor[0] = lightcolor[1] = lightcolor[2] = 256.0f;

    glPushMatrix ();
	R_RotateForEntity (e, e->scale);

	glTranslatef (paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
	glScalef (paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);

	anim = (int)(cl.time*10) & 3;
	GL_Bind(paliashdr->gl_texturenum[e->skinnum][anim]);

	if (gl_smoothmodels.value)
		glShadeModel (GL_SMOOTH);

	glEnable(GL_BLEND);
	glDisable (GL_ALPHA_TEST);
	glDepthMask(GL_FALSE);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	if (gl_affinemodels.value)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

	IgnoreInterpolatioFrame(e, paliashdr);
	R_SetupAliasBlendedFrame (currententity->frame, paliashdr, e);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	glDepthMask(GL_TRUE);
	glDisable(GL_BLEND);

	glShadeModel (GL_FLAT);
	if (gl_affinemodels.value)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	glPopMatrix ();

	if (r_shadows.value)
	{
		glPushMatrix ();
		R_RotateForEntity (e, e->scale);
		glDisable (GL_TEXTURE_2D);
		glEnable (GL_BLEND);
		glColor4f (0,0,0,0.5);
		GL_DrawAliasShadow (paliashdr, lastposenum);
		glEnable (GL_TEXTURE_2D);
		glDisable (GL_BLEND);
		glColor4f (1,1,1,1);
		glPopMatrix ();
	}
}

/*
=================
R_DrawAliasModel

=================
*/
int doZHack;
extern int zombie_skins[4];
void R_DrawAliasModel (entity_t *e)
{
	char		specChar;
	int			i;
	int			lnum;
	vec3_t		dist;
	float		add;
	model_t		*clmodel;
	vec3_t		mins, maxs;
	aliashdr_t	*paliashdr;
	float		an;
	int			anim;

	clmodel = currententity->model;

	VectorAdd (currententity->origin, clmodel->mins, mins);
	VectorAdd (currententity->origin, clmodel->maxs, maxs);

// naievil -- fixme: on psp this is == 2 ? 
	if (R_CullBox (mins, maxs))
		return;

	specChar = clmodel->name[strlen(clmodel->name) - 5];

	VectorCopy (currententity->origin, r_entorigin);
	VectorSubtract (r_origin, r_entorigin, modelorg);

	for(int g = 0; g < 3; g++)
	{
		if(lightcolor[g] < 8)
			lightcolor[g] = 8;
		if(lightcolor[g] > 125)
			lightcolor[g] = 125;
	}

	//
	// get lighting information
	//

	ambientlight = shadelight = R_LightPoint (currententity->origin);

	// allways give the gun some light
	if (e == &cl.viewent && ambientlight < 24)
		ambientlight = shadelight = 24;

	for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
	{
		if (cl_dlights[lnum].die >= (float)cl.time)
		{
			VectorSubtract (currententity->origin,
							cl_dlights[lnum].origin,
							dist);
			add = cl_dlights[lnum].radius - Length(dist);

			if (add > 0) {
				ambientlight += add;
				//ZOID models should be affected by dlights as well
				shadelight += add;
			}
		}
	}

	// clamp lighting so it doesn't overbright as much
	if (ambientlight > 128)
		ambientlight = 128;
	if (ambientlight + shadelight > 192)
		shadelight = 192 - ambientlight;

	// ZOID: never allow players to go totally black
	i = currententity - cl_entities;
	if (i >= 1 && i<=cl.maxclients /* && !strcmp (currententity->model->name, "models/player.mdl") */)
		if (ambientlight < 8)
			ambientlight = shadelight = 8;

	// HACK HACK HACK -- no fullbright colors, so make torches full light
	if (!strcmp (clmodel->name, "progs/flame2.mdl")
		|| !strcmp (clmodel->name, "progs/flame.mdl") )
		ambientlight = shadelight = 256;

	shadedots = r_avertexnormal_dots[((int)(e->angles[1] * (SHADEDOT_QUANT / 360.0f))) & (SHADEDOT_QUANT - 1)];
	shadelight = shadelight / 200.0f;
	
	an = e->angles[1]/180*(float)M_PI;
	shadevector[0] = cosf(-an);
	shadevector[1] = sinf(-an);
	shadevector[2] = 1;
	VectorNormalize (shadevector);

	//
	// locate the proper data
	//
	if(doZHack && specChar == '%')
	{
		if(clmodel->name[12] == 'c')
			paliashdr = (aliashdr_t *) Mod_Extradata(Mod_FindName("models/ai/zcfull.mdl"));
		else
			paliashdr = (aliashdr_t *) Mod_Extradata(Mod_FindName("models/ai/zfull.mdl"));
	}
	else
		paliashdr = (aliashdr_t *)Mod_Extradata (e->model);

	c_alias_polys += paliashdr->numtris;

	//
	// draw all the triangles
	//

	//Shpuld
	if(r_model_brightness.value)
	{
		lightcolor[0] += 60;
		lightcolor[1] += 60;
		lightcolor[2] += 60;
	}

	if(specChar == '!' || (e->effects & EF_FULLBRIGHT))
	{
		lightcolor[0] = lightcolor[1] = lightcolor[2] = 256;
	}

	add = 72.0f - (lightcolor[0] + lightcolor[1] + lightcolor[2]);
	if (add > 0.0f)
	{
		lightcolor[0] += add / 3.0f;
		lightcolor[1] += add / 3.0f;
		lightcolor[2] += add / 3.0f;
	}

    glPushMatrix ();
	R_RotateForEntity (e, ENTSCALE_DEFAULT);

	// Special handling of view model to keep FOV from altering look.  Pretty good.  Not perfect but rather close.
	if ((e == &cl.viewent || e == &cl.viewent2) && scr_fov_viewmodel.value) {
		float scale = 1.0f / tanf (DEG2RAD (scr_fov.value / 2.0f)) * scr_fov_viewmodel.value / 90.0f;
		if (e->scale != ENTSCALE_DEFAULT && e->scale != 0) scale *= ENTSCALE_DECODE(e->scale);
		glTranslatef (paliashdr->scale_origin[0] * scale, paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
		glScalef (paliashdr->scale[0] * scale, paliashdr->scale[1], paliashdr->scale[2]);
	} else {
		float scale = 1.0f;
		if (e->scale != ENTSCALE_DEFAULT && e->scale != 0) scale *= ENTSCALE_DECODE(e->scale);
		glTranslatef (paliashdr->scale_origin[0] * scale, paliashdr->scale_origin[1] * scale, paliashdr->scale_origin[2] * scale);
		glScalef (paliashdr->scale[0] * scale, paliashdr->scale[1] * scale, paliashdr->scale[2] * scale);
	}

	if (specChar == '%')//Zombie body
	{
		switch(e->skinnum)
		{
			case 0:
				GL_Bind(zombie_skins[0]);
				break;
			case 1:
				GL_Bind(zombie_skins[1]);
				break;
			case 2:
				GL_Bind(zombie_skins[2]);
				break;
			case 3:
				GL_Bind(zombie_skins[3]);
				break;
			default: //out of bounds? assuming 0
				Con_Printf("Zombie tex out of bounds: Tex[%i]\n",e->skinnum);
				GL_Bind(zombie_skins[0]);
				break;
		}
	}
	else
	{
		anim = (int)(cl.time*10) & 3;
		GL_Bind(paliashdr->gl_texturenum[e->skinnum][anim]);
	}

	if (gl_smoothmodels.value)
		glShadeModel (GL_SMOOTH);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	if (gl_affinemodels.value)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);

	IgnoreInterpolatioFrame(e, paliashdr);
	R_SetupAliasBlendedFrame (currententity->frame, paliashdr, e);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	glShadeModel (GL_FLAT);
	if (gl_affinemodels.value)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);

	glPopMatrix ();

	if (doZHack == 0 && specChar == '%')//if we're drawing zombie, also draw its limbs in one call
	{
		if(e->z_head)
			R_DrawZombieLimb(e,1);
		if(e->z_larm)
			R_DrawZombieLimb(e,2);
		if(e->z_rarm)
			R_DrawZombieLimb(e,3);
	}

	if (r_shadows.value)
	{
		glPushMatrix ();
		R_RotateForEntity (e, e->scale);
		glDisable (GL_TEXTURE_2D);
		glEnable (GL_BLEND);
		glColor4f (0,0,0,0.5);
		GL_DrawAliasShadow (paliashdr, lastposenum);
		glEnable (GL_TEXTURE_2D);
		glDisable (GL_BLEND);
		glColor4f (1,1,1,1);
		glPopMatrix ();
	}

}

//==================================================================================

/*
=============
R_DrawEntitiesOnList
=============
*/
void R_DrawEntitiesOnList (void)
{
	int		i;

	if (!r_drawentities.value)
		return;

	int zHackCount = 0;
	doZHack = 0;
	char specChar;

	// draw sprites seperately, because of alpha blending
	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = cl_visedicts[i];

		specChar = currententity->model->name[strlen(currententity->model->name)-5];

		if(specChar == '(' || specChar == '^')//skip heads and arms: it's faster to do this than a strcmp...
		{
			continue;
		}
		doZHack = 0;
		if(specChar == '%')
		{
			if(zHackCount > 5 || ((currententity->z_head != 0) && (currententity->z_larm != 0) && (currententity->z_rarm != 0)))
			{
				doZHack = 1;
			}
			else
			{
				zHackCount ++;//drawing zombie piece by piece.
			}
		}

		switch (currententity->model->type)
		{
		case mod_alias:
			if(specChar == '$')//This is for smooth alpha, draw in the following loop, not this one
			{
				continue;
			}
			R_DrawAliasModel (currententity);
			break;

		case mod_brush:
			R_DrawBrushModel (currententity);
			break;

		default:
			break;
		}
		doZHack = 0;
	}

	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = cl_visedicts[i];

		if(!(currententity->model))
		{
			continue;
		}

		specChar = currententity->model->name[strlen(currententity->model->name)-5];

		switch (currententity->model->type)
		{
		case mod_sprite:
			R_DrawSpriteModel (currententity);
			break;
		case mod_alias:
			if(specChar == '$')//mdl model with blended alpha
			{
				R_DrawTransparentAliasModel(currententity);
			}
			break;
		default: break;
		}
	}
}

/*
=============
R_DrawView2Model
=============
*/
void R_DrawView2Model (void)
{
	float		ambient[4], diffuse[4];
	int			j;
	int			lnum;
	vec3_t		dist;
	float		add;
	dlight_t	*dl;
	int			ambientlight, shadelight;

	if (!r_drawviewmodel.value)
		return;

	if (chase_active.value)
		return;

	if (envmap)
		return;

	if (!r_drawentities.value)
		return;

	if (cl.stats[STAT_HEALTH] <= 0)
		return;

	currententity = &cl.viewent2;
	if (!currententity->model)
		return;

	j = R_LightPoint (currententity->origin);

	if (j < 24)
		j = 24;		// allways give some light on gun
	ambientlight = j;
	shadelight = j;

// add dynamic lights		
	for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
	{
		dl = &cl_dlights[lnum];
		if (!dl->radius)
			continue;
		if (!dl->radius)
			continue;
		if (dl->die < (float)cl.time)
			continue;

		VectorSubtract (currententity->origin, dl->origin, dist);
		add = dl->radius - Length(dist);
		if (add > 0)
			ambientlight += add;
	}

	ambient[0] = ambient[1] = ambient[2] = ambient[3] = (float)ambientlight / 128;
	diffuse[0] = diffuse[1] = diffuse[2] = diffuse[3] = (float)shadelight / 128;

	// hack the depth range to prevent view model from poking into walls
	glDepthRange (gldepthmin, gldepthmin + 0.3*(gldepthmax-gldepthmin));
	R_DrawAliasModel (currententity);
	glDepthRange (gldepthmin, gldepthmax);
}



/*
=============
R_DrawViewModel
=============
*/
void R_DrawViewModel (void)
{
	float		ambient[4], diffuse[4];
	int			j;
	int			lnum;
	vec3_t		dist;
	float		add;
	dlight_t	*dl;
	int			ambientlight, shadelight;

	if (!r_drawviewmodel.value)
		return;

	if (chase_active.value)
		return;

	if (!r_drawentities.value)
		return;

	if (cl.stats[STAT_HEALTH] <= 0)
		return;

	currententity = &cl.viewent;
	if (!currententity->model)
		return;

	j = R_LightPoint (currententity->origin);

	if (j < 24)
		j = 24;		// allways give some light on gun
	ambientlight = j;
	shadelight = j;

// add dynamic lights		
	for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
	{
		dl = &cl_dlights[lnum];
		if (!dl->radius)
			continue;
		if (!dl->radius)
			continue;
		if (dl->die < (float)cl.time)
			continue;

		VectorSubtract (currententity->origin, dl->origin, dist);
		add = dl->radius - Length(dist);
		if (add > 0)
			ambientlight += add;
	}

	ambient[0] = ambient[1] = ambient[2] = ambient[3] = (float)ambientlight / 128;
	diffuse[0] = diffuse[1] = diffuse[2] = diffuse[3] = (float)shadelight / 128;

	// hack the depth range to prevent view model from poking into walls
	glDepthRange (gldepthmin, gldepthmin + 0.3*(gldepthmax-gldepthmin));
	R_DrawAliasModel (currententity);
	glDepthRange (gldepthmin, gldepthmax);
}
//=============================================================================


/*
 * GL_DrawBlendPoly
 * - Render a polygon covering the whole screen
 * - Used for full-screen color blending and approximated gamma
 * correction
 */
static void GL_DrawBlendPoly(void) {
    glBegin(GL_QUADS);
    glVertex3f(10, 100, 100);
    glVertex3f(10, -100, 100);
    glVertex3f(10, -100, -100);
    glVertex3f(10, 100, -100);
    glEnd();
}

/*
============
R_PolyBlend
============
*/
static void R_PolyBlend(void) {
    float gamma = 1.0;

    if (!VID_IsFullScreen() || (!VID_SetGammaRamp && _gl_allowgammafallback.value)) {
        gamma = v_gamma.value * v_gamma.value;
        if (gamma < 0.25)
            gamma = 0.25;
        else if (gamma > 1.0)
            gamma = 1.0;
    }

    if ((gl_polyblend.value && v_blend[3]) || gamma < 1.0) {
        glDisable(GL_ALPHA_TEST);
        glEnable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_TEXTURE_2D);

        glLoadIdentity();
        glRotatef(-90, 1, 0, 0); // put Z going up
        glRotatef(90, 0, 0, 1);  // put Z going up

        if (gl_polyblend.value && v_blend[3]) {
            glColor4fv(v_blend);
            GL_DrawBlendPoly();
        }
        if (gamma < 1.0) {
            glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA);
            glColor4f(1, 1, 1, gamma);
            GL_DrawBlendPoly();
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }

        glDisable(GL_BLEND);
        glEnable(GL_TEXTURE_2D);
        glEnable(GL_ALPHA_TEST);
    }
}

int SignbitsForPlane (mplane_t *out)
{
	int	bits, j;

	// for fast box on planeside test

	bits = 0;
	for (j=0 ; j<3 ; j++)
	{
		if (out->normal[j] < 0)
			bits |= 1<<j;
	}
	return bits;
}

static void R_SetFrustum(void) {
    int i;

    if (r_refdef.fov_x == 90) {
        // front side is visible

        VectorAdd(vpn, vright, frustum[0].normal);
        VectorSubtract(vpn, vright, frustum[1].normal);

        VectorAdd(vpn, vup, frustum[2].normal);
        VectorSubtract(vpn, vup, frustum[3].normal);
    } else {
        // rotate VPN right by FOV_X/2 degrees
        RotatePointAroundVector(frustum[0].normal, vup, vpn, -(90 - r_refdef.fov_x / 2));
        // rotate VPN left by FOV_X/2 degrees
        RotatePointAroundVector(frustum[1].normal, vup, vpn, 90 - r_refdef.fov_x / 2);
        // rotate VPN up by FOV_X/2 degrees
        RotatePointAroundVector(frustum[2].normal, vright, vpn, 90 - r_refdef.fov_y / 2);
        // rotate VPN down by FOV_X/2 degrees
        RotatePointAroundVector(frustum[3].normal, vright, vpn, -(90 - r_refdef.fov_y / 2));
    }

    for (i = 0; i < 4; i++) {
        frustum[i].type = PLANE_ANYZ; // FIXME - true for all angles?
        frustum[i].dist = DotProduct(r_origin, frustum[i].normal);
        frustum[i].signbits = SignbitsForPlane(&frustum[i]);
    }
}

/*
===============
R_SetupFrame
===============
*/
void V_CalcBlend (void);
void R_SetupFrame(void) {
// don't allow cheats in multiplayer
#ifdef NQ_HACK
    if (cl.maxclients > 1)
        Cvar_Set("r_fullbright", "0");
#endif
#ifdef QW_HACK
    r_fullbright.value = 0;
    r_lightmap.value = 0;
    if (!atoi(Info_ValueForKey(cl.serverinfo, "watervis")))
        r_wateralpha.value = 1;
#endif

    R_AnimateLight();

    r_framecount++;

    // build the transformation matrix for the given view angles
    VectorCopy(r_refdef.vieworg, r_origin);

    AngleVectors(r_refdef.viewangles, vpn, vright, vup);

    // current viewleaf
    r_oldviewleaf = r_viewleaf;
    if (!r_viewleaf)
        r_viewleaf = Mod_PointInLeaf(r_origin, cl.worldmodel);

    // color shifting for water, etc.
    V_SetContentsColor(r_viewleaf->contents);
    V_CalcBlend();

    // reset count of polys for this frame
    c_brush_polys = 0;
    c_alias_polys = 0;
    c_lightmaps_uploaded = 0;
}

void MYgluPerspective( GLdouble fovy, GLdouble aspect, GLdouble zNear, GLdouble zFar )
{
	GLdouble xmin, xmax, ymin, ymax;

	ymax = zNear * tan( fovy * M_PI / 360.0 );
	ymin = -ymax;

	xmin = ymin * aspect;
	xmax = ymax * aspect;

	glFrustum( xmin, xmax, ymin, ymax, zNear, zFar );
}

/*
=============
R_SetupGL
=============
*/
static void R_SetupGL(void) {
    float screenaspect;
    int x, x2, y2, y, w, h;

    //
    // set up viewpoint
    //
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    x = r_refdef.vrect.x * glwidth / vid.width;
    x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * glwidth / vid.width;
    y = (vid.height - r_refdef.vrect.y) * glheight / vid.height;
    y2 = (vid.height - (r_refdef.vrect.y + r_refdef.vrect.height)) * glheight / vid.height;

    // fudge around because of frac screen scale
    // FIXME: well not fix, but figure out why this is done...
    if (x > 0)
        x--;
    if (x2 < glwidth)
        x2++;
    if (y2 < 0)
        y2--;
    if (y < glheight)
        y++;

    w = x2 - x;
    h = y - y2;

    // FIXME: Skybox? Regular Quake sky?
    if (envmap) {
        x = y2 = 0;
        w = h = 256;
    }

    glViewport(glx + x, gly + y2, w, h);
    screenaspect = (float)r_refdef.vrect.width / r_refdef.vrect.height;

    // FIXME - wtf is all this about?
    // yfov =
    // 2*atan((float)r_refdef.vrect.height/r_refdef.vrect.width)*180/M_PI;
    // yfov = (2.0 * tan (scr_fov.value/360*M_PI)) / screenaspect; yfov
    // =
    // 2*atan((float)r_refdef.vrect.height/r_refdef.vrect.width)*(scr_fov.value*2)/M_PI;
    // MYgluPerspective (yfov,  screenaspect,  4,  4096);

    // FIXME - set depth dynamically for improved depth precision in
    // smaller
    //         spaces
    // MYgluPerspective (r_refdef.fov_y, screenaspect, 4, 4096);
    //MYgluPerspective(r_refdef.fov_y, screenaspect, 4, 6144);

	MYgluPerspective (r_refdef.fov_y,  screenaspect,  4,  4096);

    if (mirror) {
        if (mirror_plane->normal[2])
            glScalef(1, -1, 1);
        else
            glScalef(-1, 1, 1);
        glCullFace(GL_BACK);
    } else
        glCullFace(GL_FRONT);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glRotatef(-90, 1, 0, 0); // put Z going up
    glRotatef(90, 0, 0, 1);  // put Z going up
    glRotatef(-r_refdef.viewangles[2], 1, 0, 0);
    glRotatef(-r_refdef.viewangles[0], 0, 1, 0);
    glRotatef(-r_refdef.viewangles[1], 0, 0, 1);
    glTranslatef(-r_refdef.vieworg[0], -r_refdef.vieworg[1], -r_refdef.vieworg[2]);

    glGetFloatv(GL_MODELVIEW_MATRIX, r_world_matrix);

    //
    // set drawing parms
    //
    if (gl_cull.value)
        glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);

    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);
    glEnable(GL_DEPTH_TEST);
}

/*
================
R_RenderScene

r_refdef must be set before the first call
================
*/
static void R_RenderScene(void) {
    R_SetupFrame();
    R_SetFrustum();
    R_SetupGL();
    R_MarkLeaves();  // done here so we know if we're in water
    R_DrawWorld();   // adds static entities to the list
    S_ExtraUpdate(); // don't let sound get messed up if going slow
    R_DrawEntitiesOnList();
    R_DrawParticles();
}

/*
=============
R_Clear
=============
*/
static void R_Clear(void) {
    if (r_mirroralpha.value != 1.0) {
        if (gl_clear.value)
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        else
            glClear(GL_DEPTH_BUFFER_BIT);
        gldepthmin = 0;
        gldepthmax = 0.5;
        glDepthFunc(GL_LEQUAL);
    } else if (gl_ztrick.value) {
        static unsigned int trickframe = 0;

        if (gl_clear.value)
            glClear(GL_COLOR_BUFFER_BIT);

        trickframe++;
        if (trickframe & 1) {
            gldepthmin = 0;
            gldepthmax = 0.49999;
            glDepthFunc(GL_LEQUAL);
        } else {
            gldepthmin = 1;
            gldepthmax = 0.5;
            glDepthFunc(GL_GEQUAL);
        }
    } else {
        if (gl_clear.value)
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        else
            glClear(GL_DEPTH_BUFFER_BIT);
        gldepthmin = 0;
        gldepthmax = 1;
        glDepthFunc(GL_LEQUAL);
    }

    glDepthRange(gldepthmin, gldepthmax);

    if (gl_zfix.value) {
        if (gldepthmax > gldepthmin)
            glPolygonOffset(1, 1);
        else
            glPolygonOffset(-1, -1);
    }
}

/*
================
R_RenderView

r_refdef must be set before the first call
================
*/
void R_RenderView(void) {
    double time1 = 0, time2;

    if (r_norefresh.value)
        return;

    if (!r_worldentity.model || !cl.worldmodel)
        Sys_Error("%s: NULL worldmodel", __func__);

    if (gl_finish.value || r_speeds.value)
        glFinish();

    if (r_speeds.value) {
        time1 = Sys_FloatTime();
        c_brush_polys = 0;
        c_alias_polys = 0;
        c_lightmaps_uploaded = 0;
    }

    mirror = false;

    R_Clear();

    Fog_EnableGFog (); //johnfitz

    // render normal view
    R_RenderScene();
    R_DrawView2Model ();
	R_DrawWaterSurfaces ();

	Fog_DisableGFog (); //johnfitz;

    R_PolyBlend();

    if (r_speeds.value) {
        //              glFinish ();
        time2 = Sys_FloatTime();
        Con_Printf("%3i ms  %4i wpoly %4i epoly %4i dlit\n", (int)((time2 - time1) * 1000), c_brush_polys,
                   c_alias_polys, c_lightmaps_uploaded);
    }
}

// MARK: Vril Graphics Wrapper

void Platform_Graphics_SetTextureMode(int texture_mode)
{
	switch(texture_mode) {
		case GFX_REPLACE:
			glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
			break;
		default:
			Sys_Error("Received unknown texture mode [%d]\n", texture_mode);
			break;
	}
}

void Platform_Graphics_Color(float red, float green, float blue, float alpha)
{
	glColor4f(red, green, blue, alpha);
}
