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

#include "../../quakedef.h"

entity_t	r_worldentity;

qboolean	r_cache_thrash;		// compatability

vec3_t		modelorg, r_entorigin;
entity_t	*currententity;

int			r_visframecount;	// bumped when going to a new PVS
int			r_framecount;		// used for dlight push checking

mplane_t	frustum[4];

int			c_brush_polys, c_alias_polys;

qboolean	envmap;				// true during envmap command capture 

int			currenttexture = -1;		// to avoid unnecessary texture sets

int			cnttextures[2] = {-1, -1};     // cached

int			particletexture;	// little dot for particles
int			playertextures;		// up to 16 color translated skins

int			mirrortexturenum;	// quake texturenum, not gltexturenum
qboolean	mirror;
mplane_t	*mirror_plane;

//
// view origin
//
vec3_t	vup;
vec3_t	vpn;
vec3_t	vright;
vec3_t	r_origin;

float	r_world_matrix[16];
float	r_base_world_matrix[16];

//
// screen size info
//
refdef_t	r_refdef;

mleaf_t		*r_viewleaf, *r_oldviewleaf;

texture_t	*r_notexture_mip;

int		d_lightstylevalue[256];	// 8.8 fraction of base light value


void R_MarkLeaves (void);

cvar_t	r_norefresh = {"r_norefresh","0"};
cvar_t	r_drawentities = {"r_drawentities","1"};
cvar_t	r_drawviewmodel = {"r_drawviewmodel","1"};
cvar_t	r_speeds = {"r_speeds","0"};
cvar_t	r_fullbright = {"r_fullbright","0"};
cvar_t	r_lightmap = {"r_lightmap","0"};
cvar_t	r_shadows = {"r_shadows","0"};
cvar_t	r_mirroralpha = {"r_mirroralpha","1"};
cvar_t	r_wateralpha = {"r_wateralpha","1"};
cvar_t	r_dynamic = {"r_dynamic","1"};
cvar_t	r_novis = {"r_novis","0"};
cvar_t 	r_skyfog = {"r_skyfog", "1"};

cvar_t	gl_finish = {"gl_finish","0"};
cvar_t	gl_clear = {"gl_clear","0"};
cvar_t	gl_cull = {"gl_cull","1"};
cvar_t	gl_texsort = {"gl_texsort","1"};
cvar_t	gl_smoothmodels = {"gl_smoothmodels","1"};
cvar_t	gl_affinemodels = {"gl_affinemodels","0"};
cvar_t	gl_polyblend = {"gl_polyblend","1"};
cvar_t	gl_flashblend = {"gl_flashblend","1"};
cvar_t	gl_playermip = {"gl_playermip","0"};
cvar_t	gl_nocolors = {"gl_nocolors","0"};
cvar_t	gl_keeptjunctions = {"gl_keeptjunctions","0"};
cvar_t	gl_reporttjunctions = {"gl_reporttjunctions","0"};
cvar_t	gl_doubleeyes = {"gl_doubleeys", "1"};

//QMB
cvar_t  r_explosiontype     = {"r_explosiontype",    "0",qtrue};
cvar_t	r_laserpoint		= {"r_laserpoint",       "0",qtrue};
cvar_t	r_part_explosions	= {"r_part_explosions",  "1",qtrue};
cvar_t	r_part_trails		= {"r_part_trails",      "1",qtrue};
cvar_t	r_part_sparks		= {"r_part_sparks",      "1",qtrue};
cvar_t	r_part_spikes		= {"r_part_spikes",      "1",qtrue};
cvar_t	r_part_gunshots	    = {"r_part_gunshots",    "1",qtrue};
cvar_t	r_part_blood		= {"r_part_blood",       "1",qtrue};
cvar_t	r_part_telesplash	= {"r_part_telesplash",  "1",qtrue};
cvar_t	r_part_blobs		= {"r_part_blobs",       "1",qtrue};
cvar_t	r_part_lavasplash	= {"r_part_lavasplash",  "1",qtrue};
cvar_t	r_part_flames		= {"r_part_flames",      "1",qtrue};
cvar_t	r_part_lightning	= {"r_part_lightning",   "1",qtrue};
cvar_t	r_part_flies		= {"r_part_flies",       "1",qtrue};
cvar_t	r_part_muzzleflash  = {"r_part_muzzleflash", "1",qtrue};
cvar_t	r_flametype	        = {"r_flametype",        "2",qtrue};
//Shpuld
cvar_t  r_model_brightness  = { "r_model_brightness", "1", qtrue};   // Toggle high brightness model lighting

cvar_t	r_farclip	        = {"r_farclip",              "4096"};        //far cliping for q3 models

cvar_t	r_flatlightstyles = {"r_flatlightstyles", "0", qfalse};

extern	cvar_t	gl_ztrick;
extern 	cvar_t 	scr_fov_viewmodel;

/*
=================
R_CullBox

Returns true if the box is completely outside the frustom
=================
*/
qboolean R_CullBox (vec3_t mins, vec3_t maxs)
{
	int		i;

	for (i=0 ; i<4 ; i++)
		if (BoxOnPlaneSide (mins, maxs, &frustum[i]) == 2)
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
=============
R_InterpolateEntity

was R_BlendedRotateForEntity
fenix@io.com: model transform interpolation

modified by blubswillrule
//fixme (come back and fix this once we can test on psp and view the true issue with interpolation)
=============
*/

void R_InterpolateEntity(entity_t *e, int shadow)	// Tomaz - New Shadow
{
	float timepassed;
	float blend;
	vec3_t deltaVec;
	int i;
	
	// positional interpolation
	
	timepassed = realtime - e->translate_start_time;
	
	//notes to self (blubs)
	//-Added this method, and commented out the check for r_i_model_transforms.value
	//tried the snapping interpolation, though it worked, it was still a bit jittery...
	//problem with linear interpolation is we don't know the exact time it should take to move from origin1 to origin2...
	//looks like the rotation interpolation doesn't work all that great either, rotation could benefit from the snapping interpolation that I use
	//if I get this method to work well, make sure we go back and check for r_i_model_transforms again, (because vmodel and other models that don't use interpolation)
	//probably go back and edit animations too as I redo the last 2 textures..
	
	if (e->translate_start_time == 0 || timepassed > 1)
	{
		e->translate_start_time = realtime;
		VectorCopy (e->origin, e->origin1);
		VectorCopy (e->origin, e->origin2);
	}
	
	//our origin has been updated
	if (!VectorCompare (e->origin, e->origin2))
	{
		e->translate_start_time = realtime;
		VectorCopy (e->origin2, e->origin1);
		VectorCopy (e->origin,  e->origin2);
		blend = 0;
	}
	else
	{
		blend =  timepassed / 0.4;//0.1 not sure what this value should be...
		//technically this value should be the total amount of time that we take from 1 position to the next, it's practically how long it should take us to go from one location to the next...
		if (cl.paused || blend > 1)
			blend = 0;
	}
	
	VectorSubtract (e->origin2, e->origin1, deltaVec);

	glTranslatef (e->origin[0] + (blend * deltaVec[0]),  e->origin[1] + (blend * deltaVec[1]),  e->origin[2] + (blend * deltaVec[2]));

	// orientation interpolation (Euler angles, yuck!)
	timepassed = realtime - e->rotate_start_time;
	
	if (e->rotate_start_time == 0 || timepassed > 1)
	{
		e->rotate_start_time = realtime;
		VectorCopy (e->angles, e->angles1);
		VectorCopy (e->angles, e->angles2);
	}
	
	if (!VectorCompare (e->angles, e->angles2))
	{
		e->rotate_start_time = realtime;
		VectorCopy (e->angles2, e->angles1);
		VectorCopy (e->angles,  e->angles2);
		blend = 0;
	}
	else
	{
		blend = timepassed / 0.1;
		if (cl.paused || blend > 1)
			blend = 1;
	}
	
	VectorSubtract (e->angles2, e->angles1, deltaVec);
	
	// always interpolate along the shortest path
	for (i = 0; i < 3; i++)
	{
		if (deltaVec[i] > 180)
		{
			deltaVec[i] -= 360;
		}
		else if (deltaVec[i] < -180)
		{
		    deltaVec[i] += 360;
		}
	}

	glRotatef ((e->angles1[YAW] + ( blend * deltaVec[YAW])) * (M_PI / 180.0f),  0, 0, 1);
	if (shadow == 0)
	{
		glRotatef ((-e->angles1[PITCH] + (-blend * deltaVec[PITCH])) * (M_PI / 180.0f),  0, 1, 0);
    	glRotatef ((e->angles1[ROLL] + ( blend * deltaVec[ROLL])) * (M_PI / 180.0f),  1, 0, 0);
	}
}

/*
=================
R_FrustumCheckBox
Returns 0 if box completely inside frustum
Returns +N with intersected planes count as N
Returns -1 when completely outside frustum
=================
*/
int R_FrustumCheckBox (vec3_t mins, vec3_t maxs)
{
	int i, res;
	int intersections = 0;
	for (i=0 ; i<4 ; i++)
	{
		res = BoxOnPlaneSide (mins, maxs, &frustum[i]);
		if (res == 2) return -1; 
		if (res == 3) ++intersections;
	}

	return intersections;
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

		time = cl.time + currententity->syncbase;

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

	GL_DisableMultitexture();

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
#include "../../anorms.h"
};

vec3_t	shadevector;
float	shadelight, ambientlight;
extern vec3_t lightcolor; // LordHavoc: .lit support to the definitions at the top

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
float	r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "../../anorm_dots.h"
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
	float	vertcolor[4];
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
	float	s, t, l;
	int		i, j;
	int		index;
	trivertx_t	*v, *verts;
	int		list;
	int		*order;
	vec3_t	point;
	float	*normal;
	float	height, lheight;
	int		count;

	lheight = currententity->origin[2] - lightspot[2];

	height = 0;
	verts = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	order = (int *)((byte *)paliashdr + paliashdr->commands);

	height = -lheight + 1.0;

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
		pose += (int)(cl.time / interval) % numposes;
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

	// HACK: if we're a certain distance away, don't bother blending
	// cypress -- Lets not care about Z (up).. chances are they're out of the frustum anyway
	int dist_x = (cl.viewent.origin[0] - e->origin[0]);
	int dist_y = (cl.viewent.origin[1] - e->origin[1]);
	int distance_from_client = (int)((dist_x) * (dist_x) + (dist_y) * (dist_y)); // no use sqrting, just slows us down.

	// They're too far away from us to care about blending their frames.
	if (distance_from_client >= 160000) { // 400 * 400
		// Fix them from jumping from last lerp
		e->pose1 = e->pose2 = paliashdr->frames[frame].firstpose;
		e->frame_interval = 0.1;

		GL_DrawAliasFrame (paliashdr, paliashdr->frames[frame].firstpose);
	} else {
		pose = paliashdr->frames[frame].firstpose;
		numposes = paliashdr->frames[frame].numposes;

		if (numposes > 1)
		{
			e->frame_interval = paliashdr->frames[frame].interval;
			pose += (int)(cl.time / e->frame_interval) % numposes;
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
			blend = (realtime - e->frame_start_time) / e->frame_interval;
		// wierd things start happening if blend passes 1
		if (cl.paused || blend > 1) blend = 1;

		if (blend == 1)
			GL_DrawAliasFrame (paliashdr, pose);
		else
			GL_DrawAliasBlendedFrame (paliashdr, e->pose1, e->pose2, blend);
	}
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

	GL_DisableMultitexture();

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
	int			i, j;
	int			lnum;
	vec3_t		dist;
	float		add;
	model_t		*clmodel;
	vec3_t		mins, maxs;
	aliashdr_t	*paliashdr;
	trivertx_t	*verts, *v;
	int			index;
	float		s, t, an;
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

	GL_DisableMultitexture();
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
	int			i, j;
	int			lnum;
	vec3_t		dist;
	float		add;
	model_t		*clmodel;
	vec3_t		mins, maxs;
	aliashdr_t	*paliashdr;
	trivertx_t	*verts, *v;
	int			index;
	float		s, t, an;
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
		if (cl_dlights[lnum].die >= cl.time)
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

	shadedots = r_avertexnormal_dots[((int)(e->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];
	shadelight = shadelight / 200.0;
	
	an = e->angles[1]/180*M_PI;
	shadevector[0] = cos(-an);
	shadevector[1] = sin(-an);
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

	GL_DisableMultitexture();

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

	if (!strcmp (clmodel->name, "progs/eyes.mdl") && gl_doubleeyes.value) {
		glTranslatef (paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2] - (22 + 8));
// double size of eyes, since they are really hard to see in gl
		glScalef (paliashdr->scale[0]*2, paliashdr->scale[1]*2, paliashdr->scale[2]*2);
	} else {

		// Special handling of view model to keep FOV from altering look.  Pretty good.  Not perfect but rather close.
		if ((e == &cl.viewent || e == &cl.viewent2) && scr_fov_viewmodel.value) {
			float scale = 1.0f / tan (DEG2RAD (scr_fov.value / 2.0f)) * scr_fov_viewmodel.value / 90.0f;
			if (e->scale != ENTSCALE_DEFAULT && e->scale != 0) scale *= ENTSCALE_DECODE(e->scale);
			glTranslatef (paliashdr->scale_origin[0] * scale, paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
			glScalef (paliashdr->scale[0] * scale, paliashdr->scale[1], paliashdr->scale[2]);
		} else {
			float scale = 1.0f;
			if (e->scale != ENTSCALE_DEFAULT && e->scale != 0) scale *= ENTSCALE_DECODE(e->scale);
			glTranslatef (paliashdr->scale_origin[0] * scale, paliashdr->scale_origin[1] * scale, paliashdr->scale_origin[2] * scale);
			glScalef (paliashdr->scale[0] * scale, paliashdr->scale[1] * scale, paliashdr->scale[2] * scale);
		}
		
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

	// we can't dynamically colormap textures, so they are cached
	// seperately for the players.  Heads are just uncolored.
	if (currententity->colormap != vid.colormap && !gl_nocolors.value)
	{
		i = currententity - cl_entities;
		if (i >= 1 && i<=cl.maxclients /* && !strcmp (currententity->model->name, "models/player.mdl") */)
		    GL_Bind(playertextures - 1 + i);
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
		if (dl->die < cl.time)
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

	if (envmap)
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
		if (dl->die < cl.time)
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
============
R_PolyBlend
============
*/
void R_PolyBlend (void)
{
	if (!gl_polyblend.value)
		return;
	if (!v_blend[3])
		return;

	GL_DisableMultitexture();

	glDisable (GL_ALPHA_TEST);
	glEnable (GL_BLEND);
	glDisable (GL_DEPTH_TEST);
	glDisable (GL_TEXTURE_2D);

  glLoadIdentity ();

  glRotatef (-90,  1, 0, 0);	  // put Z going up
  glRotatef (90,  0, 0, 1);	    // put Z going up

	glColor4fv (v_blend);

	glBegin (GL_QUADS);

	glVertex3f (10, 100, 100);
	glVertex3f (10, -100, 100);
	glVertex3f (10, -100, -100);
	glVertex3f (10, 100, -100);
	glEnd ();

	glDisable (GL_BLEND);
	glEnable (GL_TEXTURE_2D);
	glEnable (GL_ALPHA_TEST);
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


void R_SetFrustum (void)
{
	int		i;

	if (r_refdef.fov_x == 90) 
	{
		// front side is visible

		VectorAdd (vpn, vright, frustum[0].normal);
		VectorSubtract (vpn, vright, frustum[1].normal);

		VectorAdd (vpn, vup, frustum[2].normal);
		VectorSubtract (vpn, vup, frustum[3].normal);
	}
	else
	{
		// naievil -- hi, this is floored because any basically non integer(? i think short precision float) value made the rendering all fucked up
		// so hope this helps (it does). This reduces some accuracy but it is not as important
		// rotate VPN right by FOV_X/2 degrees
		RotatePointAroundVector( frustum[0].normal, vup, vpn, floor(-(90-r_refdef.fov_x / 2 )) );
		// rotate VPN left by FOV_X/2 degrees
		RotatePointAroundVector( frustum[1].normal, vup, vpn, floor(90-r_refdef.fov_x / 2) );
		// rotate VPN up by FOV_X/2 degrees
		RotatePointAroundVector( frustum[2].normal, vright, vpn, floor(90-r_refdef.fov_y / 2) );
		// rotate VPN down by FOV_X/2 degrees
		RotatePointAroundVector( frustum[3].normal, vright, vpn, floor(-( 90 - r_refdef.fov_y / 2 )) );
	}

	for (i=0 ; i<4 ; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}
}



/*
===============
R_SetupFrame
===============
*/
void R_SetupFrame (void)
{
	int				edgecount;
	vrect_t			vrect;
	float			w, h;

// don't allow cheats in multiplayer
	if (cl.maxclients > 1)
		Cvar_Set ("r_fullbright", "0");

	R_AnimateLight ();

	r_framecount++;

// build the transformation matrix for the given view angles
	VectorCopy (r_refdef.vieworg, r_origin);

	AngleVectors (r_refdef.viewangles, vpn, vright, vup);

// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_viewleaf = Mod_PointInLeaf (r_origin, cl.worldmodel);

	V_SetContentsColor (r_viewleaf->contents);
	V_CalcBlend ();

	r_cache_thrash = false;

	c_brush_polys = 0;
	c_alias_polys = 0;

}


void MYgluPerspective( GLdouble fovy, GLdouble aspect,
		     GLdouble zNear, GLdouble zFar )
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
void R_SetupGL (void)
{
	float	screenaspect;
	float	yfov;
	int		i;
	extern	int glwidth, glheight;
	int		x, x2, y2, y, w, h;

	//
	// set up viewpoint
	//
	glMatrixMode(GL_PROJECTION);
    glLoadIdentity ();
	x = r_refdef.vrect.x * glwidth/vid.width;
	x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * glwidth/vid.width;
	y = (vid.height-r_refdef.vrect.y) * glheight/vid.height;
	y2 = (vid.height - (r_refdef.vrect.y + r_refdef.vrect.height)) * glheight/vid.height;

	// fudge around because of frac screen scale
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

	if (envmap)
	{
		x = y2 = 0;
		w = h = 256;
	}

	glViewport (glx + x, gly + y2, w, h);
    screenaspect = (float)r_refdef.vrect.width/r_refdef.vrect.height;
    
    gluPerspective (r_refdef.fov_y,  screenaspect,  4,  4096);

	if (mirror)
	{
		if (mirror_plane->normal[2])
			glScalef (1, -1, 1);
		else
			glScalef (-1, 1, 1);
		glCullFace(GL_BACK);
	}
	else
		glCullFace(GL_FRONT);

	glMatrixMode(GL_MODELVIEW);
    glLoadIdentity ();

    glRotatef (-90,  1, 0, 0);	    // put Z going up
    glRotatef (90,  0, 0, 1);	    // put Z going up
    glRotatef (-r_refdef.viewangles[2],  1, 0, 0);
    glRotatef (-r_refdef.viewangles[0],  0, 1, 0);
    glRotatef (-r_refdef.viewangles[1],  0, 0, 1);
    glTranslatef (-r_refdef.vieworg[0],  -r_refdef.vieworg[1],  -r_refdef.vieworg[2]);

	glGetFloatv (GL_MODELVIEW_MATRIX, r_world_matrix);

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
void R_RenderScene (void)
{
	R_SetupFrame ();

	R_SetFrustum ();

	R_SetupGL ();

	R_MarkLeaves ();	// done here so we know if we're in water

	R_DrawWorld ();		// adds static entities to the list

	S_ExtraUpdate ();	// don't let sound get messed up if going slow

	R_DrawEntitiesOnList ();

	GL_DisableMultitexture();

	//R_RenderDlights ();

	R_DrawParticles ();
}


/*
=============
R_Clear
=============
*/
void R_Clear (void)
{
	if (r_mirroralpha.value != 1.0)
	{
		if (gl_clear.value)
			glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		else
			glClear (GL_DEPTH_BUFFER_BIT);
		gldepthmin = 0;
		gldepthmax = 0.5;
		glDepthFunc (GL_LEQUAL);
	}
	else if (gl_ztrick.value)
	{
		static int trickframe;

		if (gl_clear.value)
			glClear (GL_COLOR_BUFFER_BIT);

		trickframe++;
		if (trickframe & 1)
		{
			gldepthmin = 0;
			gldepthmax = 0.49999;
			glDepthFunc (GL_LEQUAL);
		}
		else
		{
			gldepthmin = 1;
			gldepthmax = 0.5;
			glDepthFunc (GL_GEQUAL);
		}
	}
	else
	{
		if (gl_clear.value)
			glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		else
			glClear (GL_DEPTH_BUFFER_BIT);
		gldepthmin = 0;
		gldepthmax = 1;
		glDepthFunc (GL_LEQUAL);
	}

	glDepthRange (gldepthmin, gldepthmax);
}

/*
=============
R_Mirror
=============
*/
void R_Mirror (void)
{
	float		d;
	msurface_t	*s;
	entity_t	*ent;

	if (!mirror)
		return;

	memcpy (r_base_world_matrix, r_world_matrix, sizeof(r_base_world_matrix));

	d = DotProduct (r_refdef.vieworg, mirror_plane->normal) - mirror_plane->dist;
	VectorMA (r_refdef.vieworg, -2*d, mirror_plane->normal, r_refdef.vieworg);

	d = DotProduct (vpn, mirror_plane->normal);
	VectorMA (vpn, -2*d, mirror_plane->normal, vpn);

	r_refdef.viewangles[0] = -asin (vpn[2])/M_PI*180;
	r_refdef.viewangles[1] = atan2 (vpn[1], vpn[0])/M_PI*180;
	r_refdef.viewangles[2] = -r_refdef.viewangles[2];

	ent = &cl_entities[cl.viewentity];
	if (cl_numvisedicts < MAX_VISEDICTS)
	{
		cl_visedicts[cl_numvisedicts] = ent;
		cl_numvisedicts++;
	}

	gldepthmin = 0.5;
	gldepthmax = 1;
	glDepthRange (gldepthmin, gldepthmax);
	glDepthFunc (GL_LEQUAL);

	R_RenderScene ();
	R_DrawWaterSurfaces ();

	gldepthmin = 0;
	gldepthmax = 0.5;
	glDepthRange (gldepthmin, gldepthmax);
	glDepthFunc (GL_LEQUAL);

	// blend on top
	glEnable (GL_BLEND);
	glMatrixMode(GL_PROJECTION);
	if (mirror_plane->normal[2])
		glScalef (1,-1,1);
	else
		glScalef (-1,1,1);
	glCullFace(GL_FRONT);
	glMatrixMode(GL_MODELVIEW);

	glLoadMatrixf (r_base_world_matrix);

	glColor4f (1,1,1,r_mirroralpha.value);
	s = cl.worldmodel->textures[mirrortexturenum]->texturechain;
	for ( ; s ; s=s->texturechain)
		R_RenderBrushPoly (s);
	cl.worldmodel->textures[mirrortexturenum]->texturechain = NULL;
	glDisable (GL_BLEND);
	glColor4f (1,1,1,1);
}

/*
================
R_RenderView

r_refdef must be set before the first call
================
*/
void R_RenderView (void)
{
	double	time1, time2;
	GLfloat colors[4] = {(GLfloat) 0.0, (GLfloat) 0.0, (GLfloat) 1, (GLfloat) 0.20};

	if (r_norefresh.value)
		return;

	if (!r_worldentity.model || !cl.worldmodel)
		Sys_Error ("R_RenderView: NULL worldmodel");

	if (r_speeds.value)
	{
		glFinish ();
		time1 = Sys_FloatTime ();
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

	mirror = false;

	if (gl_finish.value)
		glFinish ();

	R_Clear ();

	// render normal view

	Fog_EnableGFog (); //johnfitz

	R_RenderScene ();
	R_DrawViewModel ();
	R_DrawView2Model ();
	R_DrawWaterSurfaces ();

	Fog_DisableGFog (); //johnfitz

	// render mirror view
	R_Mirror ();

	R_PolyBlend ();

	if (r_speeds.value)
	{
//		glFinish ();
		time2 = Sys_FloatTime ();
		Con_Printf ("%3i ms  %4i wpoly %4i epoly\n", (int)((time2-time1)*1000), c_brush_polys, c_alias_polys); 
	}
}
