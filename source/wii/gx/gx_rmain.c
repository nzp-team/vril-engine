/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2008 Eluan Miranda

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

extern vec3_t lightcolor; // LordHavoc: .lit support to the definitions at the top

entity_t	r_worldentity;

qboolean	r_cache_thrash;		// compatability

vec3_t		modelorg, r_entorigin;
entity_t	*currententity;

int			r_visframecount;	// bumped when going to a new PVS
int			r_framecount;		// used for dlight push checking

mplane_t	frustum[4];

int			c_brush_polys, c_alias_polys;

qboolean	envmap;				// true during envmap command capture 

int			currenttexture0 = -1;		// to avoid unnecessary texture sets
int			currenttexture1 = -1;		// to avoid unnecessary texture sets

int			cnttextures[2] = {-1, -1};     // cached

int			particletexture;	// little dot for particles
int			playertextures[MAX_SCOREBOARD];		// up to 16 color translated skins

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
cvar_t	r_wateralpha = {"r_wateralpha","0.5"};
cvar_t	r_dynamic = {"r_dynamic","1"};
cvar_t	r_novis = {"r_novis","0"};
cvar_t	r_lerpmodels = {"r_lerpmodels", "1"};
cvar_t  r_lerpmove = {"r_lerpmove", "1"};
cvar_t 	r_skyfog = {"r_skyfog", "0"}; // sB sky is fogged naturally

cvar_t	gl_finish = {"gl_finish","0"};
cvar_t	gl_clear = {"gl_clear","0"};
cvar_t	gl_cull = {"gl_cull","1"};
cvar_t	gl_smoothmodels = {"gl_smoothmodels","1"};
cvar_t	gl_affinemodels = {"gl_affinemodels","0"};
cvar_t	gl_polyblend = {"gl_polyblend","1"};
cvar_t	gl_playermip = {"gl_playermip","0"};
cvar_t	gl_nocolors = {"gl_nocolors","0"};
cvar_t	gl_keeptjunctions = {"gl_keeptjunctions","1"}; // sB was 0, but this caused white dots in world geo
cvar_t	gl_reporttjunctions = {"gl_reporttjunctions","0"};
cvar_t	gl_doubleeyes = {"gl_doubleeys", "1"};

//QMB
cvar_t  r_explosiontype     = {"r_explosiontype",    "0",true};
cvar_t	r_laserpoint		= {"r_laserpoint",       "0",true};
cvar_t	r_part_explosions	= {"r_part_explosions",  "1",true};
cvar_t	r_part_trails		= {"r_part_trails",      "1",true};
cvar_t	r_part_sparks		= {"r_part_sparks",      "1",true};
cvar_t	r_part_spikes		= {"r_part_spikes",      "1",true};
cvar_t	r_part_gunshots	    = {"r_part_gunshots",    "1",true};
cvar_t	r_part_blood		= {"r_part_blood",       "1",true};
cvar_t	r_part_telesplash	= {"r_part_telesplash",  "1",true};
cvar_t	r_part_blobs		= {"r_part_blobs",       "1",true};
cvar_t	r_part_lavasplash	= {"r_part_lavasplash",  "1",true};
cvar_t	r_part_flames		= {"r_part_flames",      "1",true};
cvar_t	r_part_lightning	= {"r_part_lightning",   "1",true};
cvar_t	r_part_flies		= {"r_part_flies",       "1",true};
cvar_t	r_part_muzzleflash  = {"r_part_muzzleflash", "1",true};
cvar_t	r_flametype	        = {"r_flametype",        "2",true};

cvar_t	r_farclip	        = {"r_farclip",              "4096"};        //far cliping for QMB

cvar_t	r_flatlightstyles = {"r_flatlightstyles", "0"};
cvar_t  r_model_brightness  = { "r_model_brightness", "1", true};   // Toggle high brightness model lighting

cvar_t	r_overbright = {"r_overbright", "1", true}; //overbrights

//johnfitz -- struct for passing lerp information to drawing functions
typedef struct {
	short pose1;
	short pose2;
	float blend;
	vec3_t origin;
	vec3_t angles;
} lerpdata_t;
//johnfitz

extern	cvar_t	gl_ztrick;
extern 	cvar_t 	scr_fov;
extern 	cvar_t 	scr_fov_viewmodel;

float viewport_size[4];

/*
=================
R_CullBox

Returns true if the box is completely outside the frustom
=================
*/
qboolean R_CullBox (vec3_t mins, vec3_t maxs)
{
	/*
	int		i;

	// ELUTODO: check for failure cases (rendering to an aspect different of that of the quake-calculated frustum, etc
	for (i=0 ; i<4 ; i++)
		if (BoxOnPlaneSide (mins, maxs, &frustum[i]) == 2)
			return true;
	*/
	return false;
}

guVector axis2 = {0,0,1};
guVector axis1 = {0,1,0};
guVector axis0 = {1,0,0};

extern cvar_t cl_crossx;
extern cvar_t cl_crossy;
extern int lock_viewmodel;
extern float goal_crosshair_pitch;
extern float goal_crosshair_yaw;
void wii_apply_weapon_offset (Mtx in, Mtx rot) {
	
	float rotTransX, rotTransY, rotTransZ, adsTransX, adsTransY, adsTransZ;
	
	// rough center for all weapons
	rotTransX = 0; // up/down
	rotTransY = -4; // left/right
	rotTransZ = 12; // forward/backward
	
	// only need to adjust Z axis on ADS translation
	adsTransX = 0;
	adsTransY = 0;
	adsTransZ = -6;
	
	if (cl.stats[STAT_ZOOM] == 1) {
		// apply weapon offset (move origin)
		guMtxTrans(in, adsTransZ, adsTransY,  adsTransX);
		guMtxConcat(model, in, model);
		
		// new rotation matrix rotates towards wiimote pointer
		guMtxRotAxisRad(rot, &axis2, DegToRad(goal_crosshair_yaw*-1));
		guMtxConcat(model, rot, model);
		guMtxRotAxisRad(rot, &axis1, DegToRad(goal_crosshair_pitch));
		guMtxConcat(model, rot, model);	
		
		// undo weapon offset (move origin back to original pos)
		guMtxTrans(in, adsTransZ*-1, adsTransY*-1,  adsTransX*-1);
		guMtxConcat(model, in, model);
		
		// if we're not ads and the viewmodel isn't locked
	} else if (lock_viewmodel != 1) {
		// apply weapon offset (move origin)
		guMtxTrans(in, rotTransZ, rotTransY,  rotTransX);
		guMtxConcat(model, in, model);
		
		// new rotation matrix rotates towards wiimote pointer
		guMtxRotAxisRad(rot, &axis2, DegToRad(goal_crosshair_yaw*-1));
		guMtxConcat(model, rot, model);
		guMtxRotAxisRad(rot, &axis1, DegToRad(goal_crosshair_pitch));
		guMtxConcat(model, rot, model);
		
		// undo weapon offset (move origin back to original pos)
		guMtxTrans(in, rotTransZ*-1, rotTransY*-1,  rotTransX*-1);
		guMtxConcat(model, in, model);
	}
}

void R_RotateForEntity (entity_t *e, unsigned char scale)
{
	Mtx temp;
	Mtx rot;
	
	// setup transform
	guMtxTrans(temp, e->origin[0],  e->origin[1], e->origin[2]);
	guMtxConcat(model, temp, model);
	// perform standard rotation
	guMtxRotAxisRad(temp, &axis2, DegToRad(e->angles[1]));
	guMtxConcat(model, temp, model);
	guMtxRotAxisRad(temp, &axis1, DegToRad(-e->angles[0]));
	guMtxConcat(model, temp, model);
	guMtxRotAxisRad(temp, &axis0, DegToRad(e->angles[2]));
	guMtxConcat(model, temp, model);
	
	if (e == &cl.viewent || e == &cl.viewent2) {
		wii_apply_weapon_offset(temp, rot);
	}
	
	if (scale != ENTSCALE_DEFAULT) {
		float scalefactor = ENTSCALE_DECODE(scale);
		
		guMtxScale (temp, scalefactor, scalefactor, scalefactor);
		guMtxConcat(model, temp, model);
		//glScalef(scalefactor, scalefactor, scalefactor);
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
		//Con_Printf ("R_DrawSprite: no such frame %d\n", frame);
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

	//GL_DisableMultitexture();

    GL_Bind0(frame->gl_texturenum);
	
	if (vid_retromode.value == 1)
		GX_SetMinMag (GX_NEAR, GX_NEAR);
	else
		GX_SetMinMag (GX_LINEAR, GX_LINEAR);
	
	//Fog_DisableGFog ();
	//GX_SetZMode(GX_TRUE, GX_GEQUAL, GX_TRUE);
	QGX_Alpha(false);
	QGX_Blend(true);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);

	VectorMA (e->origin, frame->down, up, point);
	VectorMA (point, frame->left, right, point);
	GX_Position3f32(point[0], point[1], point[2]);
	GX_Color4u8(0xff, 0xff, 0xff, 0xff);
	GX_TexCoord2f32(0, 1);

	VectorMA (e->origin, frame->up, up, point);
	VectorMA (point, frame->left, right, point);
	GX_Position3f32(point[0], point[1], point[2]);
	GX_Color4u8(0xff, 0xff, 0xff, 0xff);
	GX_TexCoord2f32(0, 0);

	VectorMA (e->origin, frame->up, up, point);
	VectorMA (point, frame->right, right, point);
	GX_Position3f32(point[0], point[1], point[2]);
	GX_Color4u8(0xff, 0xff, 0xff, 0xff);
	GX_TexCoord2f32(1, 0);

	VectorMA (e->origin, frame->down, up, point);
	VectorMA (point, frame->right, right, point);
	GX_Position3f32(point[0], point[1], point[2]);
	GX_Color4u8(0xff, 0xff, 0xff, 0xff);
	GX_TexCoord2f32(1, 1);

	GX_End();
	QGX_Alpha(true);
	QGX_Blend(false);
	//GX_SetZMode(GX_TRUE, GX_LEQUAL, GX_TRUE);
	//Fog_EnableGFog ();
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

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
float	r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "../../anorm_dots.h"
;

float	*shadedots = r_avertexnormal_dots[0];

int	lastposenum;

/*
=============
GL_DrawAliasFrame
=============
*/
void GL_DrawAliasFrame (aliashdr_t *paliashdr, lerpdata_t lerpdata)
{
	//float 	l;
	trivertx_t	*verts1, *verts2;
	int		*commands;
	int		count;
	float	blend, iblend;
	//float	u, v;
	qboolean lerping;
	
	if (lerpdata.pose1 != lerpdata.pose2)
	{
		lerping = true;
		verts1  = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
		verts2  = verts1;
		verts1 += lerpdata.pose1 * paliashdr->poseverts;
		verts2 += lerpdata.pose2 * paliashdr->poseverts;
		blend = lerpdata.blend;
		iblend = 1.0f - blend;
	}
	else // poses the same means either 1. the entity has paused its animation, or 2. r_lerpmodels is disabled
	{
		lerping = false;
		verts1  = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
		verts2  = verts1; // avoid bogus compiler warning
		verts1 += lerpdata.pose1 * paliashdr->poseverts;
		blend = iblend = 0; // avoid bogus compiler warning
	}

	commands = (int *)((byte *)paliashdr + paliashdr->commands);
	
	//l = shadedots[verts1->lightnormalindex];
	
	//glColor4f(lightcolor[0]/255, lightcolor[1]/255, lightcolor[2]/255, 1.0f);
	//
	
	while (1)
	{
		// get the vertex count and primitive type
		count = *commands++;
		if (!count)
			break;		// done

		if (count < 0)
		{
			count = -count;
			GX_Begin (GX_TRIANGLEFAN, GX_VTXFMT0, count);
		}
		else
			GX_Begin (GX_TRIANGLESTRIP, GX_VTXFMT0, count);

		do
		{
			//u = ((float *)commands)[0];
			//v = ((float *)commands)[1];

			//glTexCoord2f (u, v);
			//GX_TexCoord2f32(u, v);

			if (lerping) {
				//glVertex3f (verts1->v[0]*iblend + verts2->v[0]*blend,
							//verts1->v[1]*iblend + verts2->v[1]*blend,
							//verts1->v[2]*iblend + verts2->v[2]*blend);
				GX_Position3f32(verts1->v[0]*iblend + verts2->v[0]*blend,
								verts1->v[1]*iblend + verts2->v[1]*blend,
								verts1->v[2]*iblend + verts2->v[2]*blend);
				GX_Color4u8(lightcolor[0], lightcolor[1], lightcolor[2], 0xff);				
				verts1++;
				verts2++;
			} else {
				//glVertex3f (verts1->v[0], verts1->v[1], verts1->v[2]);
				GX_Position3f32(verts1->v[0], verts1->v[1], verts1->v[2]);
				GX_Color4u8(lightcolor[0], lightcolor[1], lightcolor[2], 0xff);
				verts1++;

			}
			GX_TexCoord2f32(((float *)commands)[0], ((float *)commands)[1]);
			
			commands += 2;
			
		} while (--count);

		GX_End ();
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
	//float	s, t, l;
	//int		i, j;
	//int		index;
	
	trivertx_t	/**v,*/ *verts;
	//int		list;
	int		*order;
	vec3_t	point;
	//float	*normal;
	float	height, lheight;
	int		count;

	lheight = currententity->origin[2] - lightspot[2];

	height = 0;
	verts = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	order = (int *)((byte *)paliashdr + paliashdr->commands);

	height = -lheight + 1.0;
	
	//sB should be working now. 
	while (1)
	{
		// get the vertex count and primitive type
		count = *order++;
		if (!count)
			break;		// done
		if (count < 0)
		{
			count = -count;
			//glBegin (GL_TRIANGLE_FAN);
			GX_Begin (GX_TRIANGLEFAN, GX_VTXFMT0, count);
		}
		else
			//glBegin (GL_TRIANGLE_STRIP);
			GX_Begin (GX_TRIANGLESTRIP, GX_VTXFMT0, count);

		do
		{
			// texture coordinates come from the draw list
			// (skipped for shadows) glTexCoord2fv ((float *)order);

			// normals and vertexes come from the frame list
			point[0] = verts->v[0] * paliashdr->scale[0] + paliashdr->scale_origin[0];
			point[1] = verts->v[1] * paliashdr->scale[1] + paliashdr->scale_origin[1];
			point[2] = verts->v[2] * paliashdr->scale[2] + paliashdr->scale_origin[2];

			point[0] -= shadevector[0]*(point[2]+lheight);
			point[1] -= shadevector[1]*(point[2]+lheight);
			point[2] = height;
//			height -= 0.001;
			//glVertex3fv (point);
			
			GX_Position3f32(point[0], point[1], point[2]);
			GX_Color4u8(0xff, 0xff, 0xff, 0xff);
			verts++;	
			order += 2;

		} while (--count);

		//glEnd ();
		GX_End();
	}
	
}



/*
=================
R_SetupAliasFrame -- johnfitz -- rewritten to support lerping
=================
*/
void R_SetupAliasFrame (aliashdr_t *paliashdr, int frame, lerpdata_t *lerpdata)
{
	entity_t		*e = currententity;
	int				posenum, numposes;

	if ((frame >= paliashdr->numframes) || (frame < 0))
	{
		//Con_DPrintf ("R_AliasSetupFrame: no such frame %d for '%s'\n", frame, e->model->name);
		frame = 0;
	}

	posenum = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	if (numposes > 1)
	{
		e->lerptime = paliashdr->frames[frame].interval;
		posenum += (int)(cl.time / e->lerptime) % numposes;
	}
	else
		e->lerptime = 0.1;

	if (e->lerpflags & LERP_RESETANIM) //kill any lerp in progress
	{
		e->lerpstart = 0;
		e->previouspose = posenum;
		e->currentpose = posenum;
		e->lerpflags -= LERP_RESETANIM;
	}
	else if (e->currentpose != posenum) // pose changed, start new lerp
	{
		if (e->lerpflags & LERP_RESETANIM2) //defer lerping one more time
		{
			e->lerpstart = 0;
			e->previouspose = posenum;
			e->currentpose = posenum;
			e->lerpflags -= LERP_RESETANIM2;
		}
		else
		{
			e->lerpstart = cl.time;
			e->previouspose = e->currentpose;
			e->currentpose = posenum;
		}
	}

	//set up values
	if (r_lerpmodels.value && !(e->model->flags & MOD_NOLERP && r_lerpmodels.value != 2))
	{
		if (e->lerpflags & LERP_FINISH && numposes == 1)
			lerpdata->blend = CLAMP (0, (cl.time - e->lerpstart) / (e->lerpfinish - e->lerpstart), 1);
		else
			lerpdata->blend = CLAMP (0, (cl.time - e->lerpstart) / e->lerptime, 1);
		lerpdata->pose1 = e->previouspose;
		lerpdata->pose2 = e->currentpose;
	}
	else //don't lerp
	{
		lerpdata->blend = 1;
		lerpdata->pose1 = posenum;
		lerpdata->pose2 = posenum;
	}
}

/*
=================
R_SetupEntityTransform -- johnfitz -- set up transform part of lerpdata
=================
*/
void R_SetupEntityTransform (entity_t *e, lerpdata_t *lerpdata)
{
	float blend;
	vec3_t d;
	int i;

	// if LERP_RESETMOVE, kill any lerps in progress
	if (e->lerpflags & LERP_RESETMOVE)
	{
		e->movelerpstart = 0;
		VectorCopy (e->origin, e->previousorigin);
		VectorCopy (e->origin, e->currentorigin);
		VectorCopy (e->angles, e->previousangles);
		VectorCopy (e->angles, e->currentangles);
		e->lerpflags -= LERP_RESETMOVE;
	}
	else if (!VectorCompare (e->origin, e->currentorigin) || !VectorCompare (e->angles, e->currentangles)) // origin/angles changed, start new lerp
	{
		e->movelerpstart = cl.time;
		VectorCopy (e->currentorigin, e->previousorigin);
		VectorCopy (e->origin,  e->currentorigin);
		VectorCopy (e->currentangles, e->previousangles);
		VectorCopy (e->angles,  e->currentangles);
	}

	//set up values
	if (r_lerpmove.value && e != &cl.viewent && e->lerpflags & LERP_MOVESTEP)
	{
		if (e->lerpflags & LERP_FINISH)
			blend = CLAMP (0, (cl.time - e->movelerpstart) / (e->lerpfinish - e->movelerpstart), 1);
		else
			blend = CLAMP (0, (cl.time - e->movelerpstart) / 0.1, 1);

		//translation
		VectorSubtract (e->currentorigin, e->previousorigin, d);
		lerpdata->origin[0] = e->previousorigin[0] + d[0] * blend;
		lerpdata->origin[1] = e->previousorigin[1] + d[1] * blend;
		lerpdata->origin[2] = e->previousorigin[2] + d[2] * blend;

		//rotation
		VectorSubtract (e->currentangles, e->previousangles, d);
		for (i = 0; i < 3; i++)
		{
			if (d[i] > 180)  d[i] -= 360;
			if (d[i] < -180) d[i] += 360;
		}
		lerpdata->angles[0] = e->previousangles[0] + d[0] * blend;
		lerpdata->angles[1] = e->previousangles[1] + d[1] * blend;
		lerpdata->angles[2] = e->previousangles[2] + d[2] * blend;
	}
	else //don't lerp
	{
		VectorCopy (e->origin, lerpdata->origin);
		VectorCopy (e->angles, lerpdata->angles);
	}
}

/*
=================
R_DrawZombieLimb

=================
*/
//Blubs Z hacks: need this declaration.
model_t *Mod_FindName (char *name);
extern int zombie_skins[4];
void R_DrawZombieLimb (entity_t *e, int which)
{
	model_t		*clmodel;
	aliashdr_t	*paliashdr;
	entity_t 	*limb_ent;
	lerpdata_t	lerpdata;
	Mtx			temp;
	float		add;

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
	
	for(int g = 0; g < 3; g++)
	{
		if(lightcolor[g] < 12)
			lightcolor[g] = 12;
		if(lightcolor[g] > 125)
			lightcolor[g] = 125;
	}

	// locate the proper data
	paliashdr = (aliashdr_t *)Mod_Extradata(clmodel);//e->model
	c_alias_polys += paliashdr->numtris;

	//GL_DisableMultitexture();
	
	//Shpuld
	if(r_model_brightness.value)
	{
		lightcolor[0] += 32;
		lightcolor[1] += 32;
		lightcolor[2] += 32;
	}
	
	add = 72.0f - (lightcolor[0] + lightcolor[1] + lightcolor[2]);
	if (add > 0.0f)
	{
		lightcolor[0] += add / 3.0f;
		lightcolor[1] += add / 3.0f;
		lightcolor[2] += add / 3.0f;
	}

	//glPushMatrix ();
	guMtxIdentity(model);
	R_RotateForEntity (e, e->scale);

	//glTranslatef (paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
	//glScalef (paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);
	
	guMtxTrans (temp, paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
	guMtxConcat(model, temp, model);
	guMtxScale (temp, paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);
	guMtxConcat(model, temp, model);
	
	guMtxConcat(view,model,modelview);
	GX_LoadPosMtxImm(modelview, GX_PNMTX0);
/*
	if (gl_smoothmodels.value)
		glShadeModel (GL_SMOOTH);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
*/
	GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);
/*
	if (gl_affinemodels.value)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
*/

	R_SetupAliasFrame (paliashdr, e->frame, &lerpdata);
	R_SetupEntityTransform (e, &lerpdata);
	GL_DrawAliasFrame(paliashdr, lerpdata);

	//glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);
/*
	glShadeModel (GL_FLAT);
	if (gl_affinemodels.value)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
*/
	//glPopMatrix ();
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
	Mtx			temp;
	lerpdata_t	lerpdata;

	clmodel = currententity->model;

	VectorAdd (currententity->origin, clmodel->mins, mins);
	VectorAdd (currententity->origin, clmodel->maxs, maxs);

// naievil -- fixme: on psp this is == 2 ? 
	if (R_CullBox (mins, maxs))
		return;

	VectorCopy (currententity->origin, r_entorigin);
	VectorSubtract (r_origin, r_entorigin, modelorg);
	
	QGX_Blend(true);
	QGX_Alpha(false);
	
	R_LightPoint (currententity->origin);

	// //
	// // get lighting information
	// //

	for(int g = 0; g < 3; g++)
	{
		if(lightcolor[g] < 8)
			lightcolor[g] = 8;
		if(lightcolor[g] > 125)
			lightcolor[g] = 125;
	}
	
	if(e->effects & EF_FULLBRIGHT)
	{
		lightcolor[0] = lightcolor[1] = lightcolor[2] = 255;
	}

	//
	// locate the proper data
	//
	paliashdr = (aliashdr_t *)Mod_Extradata (e->model);
	c_alias_polys += paliashdr->numtris;

	//
	// draw all the triangles
	//

	//GL_DisableMultitexture();

    guMtxIdentity(model);
	R_RotateForEntity (e, e->scale);

	guMtxTrans (temp, paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
	guMtxConcat(model, temp, model);
	guMtxScale (temp, paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);
	guMtxConcat(model, temp, model);
	
	guMtxConcat(view,model,modelview);
	GX_LoadPosMtxImm(modelview, GX_PNMTX0);

	anim = (int)(cl.time*10) & 3;
	GL_Bind0(paliashdr->gl_texturenum[e->skinnum][anim]);

	/*
	if (gl_smoothmodels.value)
		glShadeModel (GL_SMOOTH);
	*/

	GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);

	/* ELUTODO
	if (gl_affinemodels.value)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);*/

	R_SetupAliasFrame (paliashdr, e->frame, &lerpdata);
	R_SetupEntityTransform (e, &lerpdata);
	GL_DrawAliasFrame(paliashdr, lerpdata);

	GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);
	
	QGX_Blend(false);
	QGX_Alpha(true);
	/*
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
	*/
}

/*
=================
R_DrawAliasModel

=================
*/
int doZHack;
extern qboolean update_weap_pos;
void R_DrawAliasModel (entity_t *e)
{
	char		specChar;
	//int			i;
	int			lnum;
	vec3_t		dist;
	float		add;
	model_t		*clmodel;
	vec3_t		mins, maxs;
	aliashdr_t	*paliashdr;
	float		an;
	int			anim;
	Mtx			temp, temp2;
	lerpdata_t	lerpdata;

	clmodel = currententity->model;

	VectorAdd (currententity->origin, clmodel->mins, mins);
	VectorAdd (currententity->origin, clmodel->maxs, maxs);

	if (R_CullBox (mins, maxs))
		return;

	specChar = clmodel->name[strlen(clmodel->name) - 5];
	
	VectorCopy (currententity->origin, r_entorigin);
	VectorSubtract (r_origin, r_entorigin, modelorg);

	//
	// get lighting information
	//
	
	R_LightPoint (currententity->origin);
	
	for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
	{
		if (cl_dlights[lnum].die >= cl.time)
		{
			VectorSubtract (currententity->origin,
							cl_dlights[lnum].origin,
							dist);
			add = cl_dlights[lnum].radius - Length(dist);

			if (add > 0)
			{
				lightcolor[0] += add * cl_dlights[lnum].color[0];
				lightcolor[1] += add * cl_dlights[lnum].color[1];
				lightcolor[2] += add * cl_dlights[lnum].color[2];
			}
			
		}
	}
	
	an = e->angles[1]/180*M_PI;
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

	//GL_DisableMultitexture();

	if(specChar == '!' || (e->effects & EF_FULLBRIGHT))
	{
		lightcolor[0] = lightcolor[1] = lightcolor[2] = 255;
	}
	
	//Shpuld
	if(r_model_brightness.value && specChar != '!' && !(e->effects & EF_FULLBRIGHT))
	{		
		lightcolor[0] += 32;
		lightcolor[1] += 32;
		lightcolor[2] += 32;
	}
	
	// always give the gun a little more light
	if (e == &cl.viewent || e == &cl.viewent2)
	{
		if (lightcolor[0] < 64)
			lightcolor[0] = 64;
		if (lightcolor[1] < 64)
			lightcolor[1] = 64;
		if (lightcolor[2] < 64)
			lightcolor[2] = 64;
	}
	
	for(int g = 0; g < 3; g++)
	{
		if(lightcolor[g] < 32)
			lightcolor[g] = 32;
		if(lightcolor[g] > 255)
			lightcolor[g] = 255;
	}
	
	// HACK HACK HACK -- no fullbright colors, so make torches full light
	if (!strcmp (clmodel->name, "progs/flame2.mdl")
		|| !strcmp (clmodel->name, "progs/flame.mdl")
		|| !strcmp (clmodel->name, "progs/lavaball.mdl") 
		|| !strcmp (clmodel->name, "progs/bolt.mdl")
	    || !strcmp (clmodel->name, "models/misc/bolt2.mdl")
	    || !strcmp (clmodel->name, "progs/bolt3.mdl") ) {
		lightcolor[0] = lightcolor[1] = lightcolor[2] = 255;
	}
	guMtxIdentity(model);
	R_RotateForEntity (e, ENTSCALE_DEFAULT);

	if ((e == &cl.viewent || e == &cl.viewent2)/* && scr_fov_viewmodel.value*/) {
		float scale = 1.0f / tan (DEG2RAD (scr_fov.value / 2.0f)) * scr_fov_viewmodel.value / 90.0f;
		if (e->scale != ENTSCALE_DEFAULT && e->scale != 0) 
			scale *= ENTSCALE_DECODE(e->scale);	
		//glTranslatef (paliashdr->scale_origin[0] * scale, paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
		//glScalef (paliashdr->scale[0] * scale, paliashdr->scale[1], paliashdr->scale[2]);
		guMtxTrans (temp, paliashdr->scale_origin[0] * scale, paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
		guMtxConcat(model, temp, model);
		guMtxScale (temp, paliashdr->scale[0] * scale, paliashdr->scale[1], paliashdr->scale[2]);
		guMtxConcat(model, temp, model);
	} else {
		float scale = 1.0f;
		if (e->scale != ENTSCALE_DEFAULT && e->scale != 0)
			scale *= ENTSCALE_DECODE(e->scale);	
		//glTranslatef (paliashdr->scale_origin[0] * scale, paliashdr->scale_origin[1] * scale, paliashdr->scale_origin[2] * scale);
		//glScalef (paliashdr->scale[0] * scale, paliashdr->scale[1] * scale, paliashdr->scale[2] * scale);
		guMtxTrans (temp, paliashdr->scale_origin[0] * scale, paliashdr->scale_origin[1] * scale, paliashdr->scale_origin[2] * scale);
		guMtxConcat(model, temp, model);
		guMtxScale (temp2, paliashdr->scale[0] * scale, paliashdr->scale[1] * scale, paliashdr->scale[2] * scale);
		guMtxConcat(model, temp2, model);
	}

	guMtxConcat(view,model,modelview);
	GX_LoadPosMtxImm(modelview, GX_PNMTX0);
	
	if (specChar == '%')//Zombie body
	{
		switch(e->skinnum)
		{
			case 0:
				GL_Bind0(zombie_skins[0]);
				if (vid_retromode.value == 1)
					GX_SetMinMag (GX_NEAR, GX_NEAR);
				else
					GX_SetMinMag (GX_LINEAR, GX_LINEAR);
				break;
			case 1:
				GL_Bind0(zombie_skins[1]);
				if (vid_retromode.value == 1)
					GX_SetMinMag (GX_NEAR, GX_NEAR);
				else
					GX_SetMinMag (GX_LINEAR, GX_LINEAR);
				break;
			case 2:
				GL_Bind0(zombie_skins[2]);
				if (vid_retromode.value == 1)
					GX_SetMinMag (GX_NEAR, GX_NEAR);
				else
					GX_SetMinMag (GX_LINEAR, GX_LINEAR);
				break;
			case 3:
				GL_Bind0(zombie_skins[3]);
				if (vid_retromode.value == 1)
					GX_SetMinMag (GX_NEAR, GX_NEAR);
				else
					GX_SetMinMag (GX_LINEAR, GX_LINEAR);
				break;
			default: //out of bounds? assuming 0
				Con_Printf("Zombie tex out of bounds: Tex[%i]\n",e->skinnum);
				GL_Bind0(zombie_skins[0]);
				GX_SetMinMag (GX_LINEAR, GX_NEAR);
				break;
		}
	}
	else
	{
		anim = (int)(cl.time*10) & 3;
		GL_Bind0(paliashdr->gl_texturenum[currententity->skinnum][anim]);
		if (vid_retromode.value == 1)
			GX_SetMinMag (GX_NEAR, GX_NEAR);
		else
			GX_SetMinMag (GX_LINEAR, GX_LINEAR);
	}

	/* ELUTODO if (gl_smoothmodels.value)
		glShadeModel (GL_SMOOTH);*/

	GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);

	/* ELUTODO
	if (gl_affinemodels.value)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);*/

	R_SetupAliasFrame (paliashdr, e->frame, &lerpdata);
	R_SetupEntityTransform (e, &lerpdata);
	GL_DrawAliasFrame(paliashdr, lerpdata);

	GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);
	
	if (doZHack == 0 && specChar == '%')//if we're drawing zombie, also draw its limbs in one call
	{
		if(e->z_head)
			R_DrawZombieLimb(e,1);
		if(e->z_larm)
			R_DrawZombieLimb(e,2);
		if(e->z_rarm)
			R_DrawZombieLimb(e,3);
	}

/* ELUTODO
	glShadeModel (GL_FLAT);
	if (gl_affinemodels.value)
		glHint (GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);*/

	/* ELUTODO if (r_shadows.value)
	{
		glPushMatrix ();
		R_RotateForEntity (e);
		glDisable (GL_TEXTURE_2D);
		glEnable (GL_BLEND);
		glColor4f (0,0,0,0.5);
		GL_DrawAliasShadow (paliashdr, lastposenum);
		glEnable (GL_TEXTURE_2D);
		glDisable (GL_BLEND);
		glColor4f (1,1,1,1);
		glPopMatrix ();
	}
*/
}

//==================================================================================

//==================================================================================
int SetFlameModelState (void)
{
	if (!r_part_flames.value && !strcmp(currententity->model->name, "progs/flame0.mdl"))
	{
		currententity->model = cl.model_precache[cl_modelindex[mi_flame1]];
	}
	else if (r_part_flames.value)
	{
		vec3_t	liteorg;

		VectorCopy (currententity->origin, liteorg);
		if (currententity->baseline.modelindex == cl_modelindex[mi_flame0])
		{
			if (r_part_flames.value == 2)
			{
				liteorg[2] += 14;
				QMB_Q3TorchFlame (liteorg, 15);
			}
			else
			{
				liteorg[2] += 5.5;

				if(r_flametype.value == 2)
				  QMB_FlameGt (liteorg, 7, 0.8);
				else
				  QMB_TorchFlame(liteorg);
			}
		}
		else if (currententity->baseline.modelindex == cl_modelindex[mi_flame1])
		{
			if (r_part_flames.value == 2)
			{
				liteorg[2] += 14;
				QMB_Q3TorchFlame (liteorg, 15);
			}
			else
			{
				liteorg[2] += 5.5;

				if(r_flametype.value > 1)
				  QMB_FlameGt (liteorg, 7, 0.8);
				else
			      QMB_TorchFlame(liteorg);

			}
			currententity->model = cl.model_precache[cl_modelindex[mi_flame0]];
		}
		else if (currententity->baseline.modelindex == cl_modelindex[mi_flame2])
		{
			if (r_part_flames.value == 2)
            {
				liteorg[2] += 14;
				QMB_Q3TorchFlame (liteorg, 32);
            }
			else
			{
                liteorg[2] -= 1;

				if(r_flametype.value > 1)
				  QMB_FlameGt (liteorg, 12, 1);
				else
			      QMB_BigTorchFlame(liteorg);
			}
			return -1;	//continue;
		}
        else if (!strcmp(currententity->model->name, "progs/wyvflame.mdl"))
		{
			liteorg[2] -= 1;

			if(r_flametype.value > 1)
			  QMB_FlameGt (liteorg, 12, 1);
			else
			  QMB_BigTorchFlame(liteorg);

			return -1;	//continue;
		}
	}

	return 0;
}


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
		if(specChar == '$')//This is for smooth alpha, draw in the following loop, not this one
		{
			continue;
		}

		switch (currententity->model->type)
		{
		case mod_alias:
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

	GX_LoadPosMtxImm(view, GX_PNMTX0);
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
			
				if (qmb_initialized && SetFlameModelState() == -1)
					continue;
			
				if(specChar == '$') { //mdl model with blended alpha 
					R_DrawTransparentAliasModel(currententity);
				}
				break;
			
			default:
				break;
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
	/*
	float		ambient[4], diffuse[4];
	int			j;
	int			lnum;
	vec3_t		dist;
	float		add;
	dlight_t	*dl;
	int			ambientlight, shadelight;
	*/

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

	// hack the depth range to prevent view model from poking into walls
	GX_SetViewport(viewport_size[0], viewport_size[1], viewport_size[2] > 640 ? 640 : viewport_size[2], viewport_size[3], 0.0f, 0.3f);
	R_DrawAliasModel (currententity);
	GX_SetViewport(viewport_size[0], viewport_size[1], viewport_size[2] > 640 ? 640 : viewport_size[2], viewport_size[3], 0.0f, 1.0f);
}

/*
=============
R_DrawViewModel
=============
*/
void R_DrawViewModel (void)
{
	/*
	float		ambient[4], diffuse[4];
	int			j;
	int			lnum;
	vec3_t		dist;
	float		add;
	dlight_t	*dl;
	int			ambientlight, shadelight;
	*/

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
	
	// hack the depth range to prevent view model from poking into walls
	GX_SetViewport(viewport_size[0], viewport_size[1], viewport_size[2] > 640 ? 640 : viewport_size[2], viewport_size[3], 0.0f, 0.3f);
	R_DrawAliasModel (currententity);
	GX_SetViewport(viewport_size[0], viewport_size[1], viewport_size[2] > 640 ? 640 : viewport_size[2], viewport_size[3], 0.0f, 1.0f);
}

#if 0
/*
============
R_PolyBlend
============
*/
void R_PolyBlend (void)
{
	Mtx temp;

	if (!gl_polyblend.value)
		return;
	if (!v_blend[3] && v_gamma.value == 1.0f)
		return;

	QGX_Alpha(false);
	QGX_Blend(true);
	QGX_ZMode(false);
	GL_Bind0(white_texturenum); // ELUTODO: do not use a texture
	GX_SetTevOp(GX_TEVSTAGE0, GX_MODULATE);

	c_guMtxIdentity(view);
	c_guMtxRotAxisRad(temp, &axis0, DegToRad(-90.0f));		// put Z going up
	c_guMtxConcat(view, temp, view);
	c_guMtxRotAxisRad(temp, &axis2, DegToRad(90.0f));		// put Z going up
	c_guMtxConcat(view, temp, view);
	GX_LoadPosMtxImm(view, GX_PNMTX0);

	// ELUTODO: check if v_blend gets bigger than 1.0f
	if (v_blend[3])
	{
		GX_Begin(GX_QUADS, GX_VTXFMT0, 4);

		GX_Position3f32(10.0f, 100.0f, 100.0f);
		GX_Color4u8(v_blend[0] * 255, v_blend[1] * 255, v_blend[2] * 255, v_blend[3] * 255);
		GX_TexCoord2f32(1.0f, 1.0f);

		GX_Position3f32(10.0f, -100.0f, 100.0f);
		GX_Color4u8(v_blend[0] * 255, v_blend[1] * 255, v_blend[2] * 255, v_blend[3] * 255);
		GX_TexCoord2f32(0.0f, 1.0f);

		GX_Position3f32(10.0f, -100.0f, -100.0f);
		GX_Color4u8(v_blend[0] * 255, v_blend[1] * 255, v_blend[2] * 255, v_blend[3] * 255);
		GX_TexCoord2f32(0.0f, 0.0f);

		GX_Position3f32(10.0f, 100.0f, -100.0f);
		GX_Color4u8(v_blend[0] * 255, v_blend[1] * 255, v_blend[2] * 255, v_blend[3] * 255);
		GX_TexCoord2f32(1.0f, 0.0f);

		GX_End();
	}

	// ELUTODO quick hack
	if (v_gamma.value != 1.0f)
	{
		GX_Begin(GX_QUADS, GX_VTXFMT0, 4);

		GX_Position3f32(10.0f, 100.0f, 100.0f);
		GX_Color4u8(0xff, 0xff, 0xff, (v_gamma.value * -1.0f + 1.0f) * 0xff);
		GX_TexCoord2f32(1.0f, 1.0f);

		GX_Position3f32(10.0f, -100.0f, 100.0f);
		GX_Color4u8(0xff, 0xff, 0xff, (v_gamma.value * -1.0f + 1.0f) * 0xff);
		GX_TexCoord2f32(0.0f, 1.0f);

		GX_Position3f32(10.0f, -100.0f, -100.0f);
		GX_Color4u8(0xff, 0xff, 0xff, (v_gamma.value * -1.0f + 1.0f) * 0xff);
		GX_TexCoord2f32(0.0f, 0.0f);

		GX_Position3f32(10.0f, 100.0f, -100.0f);
		GX_Color4u8(0xff, 0xff, 0xff, (v_gamma.value * -1.0f + 1.0f) * 0xff);
		GX_TexCoord2f32(1.0f, 0.0f);

		GX_End();
	}

	QGX_Blend(false);
	QGX_Alpha(true);
	GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);
}
#else
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
	
	Mtx temp;

	//GL_DisableMultitexture();

	QGX_Alpha(false);
	QGX_Blend(true);
	QGX_ZMode(false);
	/*
	GX_SetNumChans(0);
	GX_SetNumTexGens(0);
	*/
	GX_SetVtxDesc(GX_VA_TEX0, GX_NONE);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORDNULL, GX_TEXMAP_NULL, GX_COLOR0A0);
	GX_SetTevOp(GX_TEVSTAGE0, GX_PASSCLR);

	guMtxIdentity(view);
	guMtxRotAxisRad(temp, &axis0, DegToRad(-90.0f));		// put Z going up
	guMtxConcat(view, temp, view);
	guMtxRotAxisRad(temp, &axis2, DegToRad(90.0f));		// put Z going up
	guMtxConcat(view, temp, view);
	GX_LoadPosMtxImm(view, GX_PNMTX0);
	
	//GX_Begin(GX_QUADS, GX_VTXFMT0, 4);

	//GX_Position3f32(10.0f, 100.0f, 100.0f);
	//GX_Color4u8(v_blend[0]/* * 255*/, v_blend[1]/* * 255*/, v_blend[2]/* * 255*/, v_blend[3]/* * 255*/);
	//GX_TexCoord2f32(1.0f, 1.0f);

	//GX_Position3f32(10.0f, -100.0f, 100.0f);
	//GX_Color4u8(v_blend[0]/* * 255*/, v_blend[1]/* * 255*/, v_blend[2]/* * 255*/, v_blend[3]/* * 255*/);
	//GX_TexCoord2f32(0.0f, 1.0f);

	//GX_Position3f32(10.0f, -100.0f, -100.0f);
	//GX_Color4u8(v_blend[0]/* * 255*/, v_blend[1]/* * 255*/, v_blend[2]/* * 255*/, v_blend[3]/* * 255*/);
	//GX_TexCoord2f32(0.0f, 0.0f);

	//GX_Position3f32(10.0f, 100.0f, -100.0f);
	//GX_Color4u8(v_blend[0]/* * 255*/, v_blend[1]/* * 255*/, v_blend[2]/* * 255*/, v_blend[3]/* * 255*/);
	//GX_TexCoord2f32(1.0f, 0.0f);
	
	// ELUTODO: check if v_blend gets bigger than 1.0f
	if (v_blend[3])
	{
		Con_Printf("polyblending\n");
		
		GX_Begin(GX_QUADS, GX_VTXFMT0, 4);

		GX_Position3f32(10.0f, 100.0f, 100.0f);
		GX_Color4u8(v_blend[0] * 255, v_blend[1] * 255, v_blend[2] * 255, v_blend[3] * 255);
		GX_TexCoord2f32(1.0f, 1.0f);

		GX_Position3f32(10.0f, -100.0f, 100.0f);
		GX_Color4u8(v_blend[0] * 255, v_blend[1] * 255, v_blend[2] * 255, v_blend[3] * 255);
		GX_TexCoord2f32(0.0f, 1.0f);

		GX_Position3f32(10.0f, -100.0f, -100.0f);
		GX_Color4u8(v_blend[0] * 255, v_blend[1] * 255, v_blend[2] * 255, v_blend[3] * 255);
		GX_TexCoord2f32(0.0f, 0.0f);

		GX_Position3f32(10.0f, 100.0f, -100.0f);
		GX_Color4u8(v_blend[0] * 255, v_blend[1] * 255, v_blend[2] * 255, v_blend[3] * 255);
		GX_TexCoord2f32(1.0f, 0.0f);

		GX_End();
	}

	// ELUTODO quick hack
	if (v_gamma.value != 1.0f)
	{
		Con_Printf("polyblending gamma\n");
		
		GX_Begin(GX_QUADS, GX_VTXFMT0, 4);

		GX_Position3f32(10.0f, 100.0f, 100.0f);
		GX_Color4u8(0xff, 0xff, 0xff, (v_gamma.value * -1.0f + 1.0f) * 0xff);
		GX_TexCoord2f32(1.0f, 1.0f);

		GX_Position3f32(10.0f, -100.0f, 100.0f);
		GX_Color4u8(0xff, 0xff, 0xff, (v_gamma.value * -1.0f + 1.0f) * 0xff);
		GX_TexCoord2f32(0.0f, 1.0f);

		GX_Position3f32(10.0f, -100.0f, -100.0f);
		GX_Color4u8(0xff, 0xff, 0xff, (v_gamma.value * -1.0f + 1.0f) * 0xff);
		GX_TexCoord2f32(0.0f, 0.0f);

		GX_Position3f32(10.0f, 100.0f, -100.0f);
		GX_Color4u8(0xff, 0xff, 0xff, (v_gamma.value * -1.0f + 1.0f) * 0xff);
		GX_TexCoord2f32(1.0f, 0.0f);

		GX_End();
	}

	//GX_End();
	/*
	GX_SetNumChans(1);
	GX_SetNumTexGens(1);
	*/
	
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
 	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLOR0A0);
	GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);
	QGX_Blend(false);
	QGX_Alpha(true);
}
#endif

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

	// rotate VPN right by FOV_X/2 degrees
	RotatePointAroundVector( frustum[0].normal, vup, vpn, -(90-r_refdef.fov_x / 2 ) );
	// rotate VPN left by FOV_X/2 degrees
	RotatePointAroundVector( frustum[1].normal, vup, vpn, 90-r_refdef.fov_x / 2 );
	// rotate VPN up by FOV_X/2 degrees
	RotatePointAroundVector( frustum[2].normal, vright, vpn, 90-r_refdef.fov_y / 2 );
	// rotate VPN down by FOV_X/2 degrees
	RotatePointAroundVector( frustum[3].normal, vright, vpn, -( 90 - r_refdef.fov_y / 2 ) );

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

/*
=============
R_SetupGL
=============
*/
void R_SetupGL (void)
{
	float	screenaspect;
	extern	int glwidth, glheight;
	int		x, x2, y2, y, w, h;
	Mtx		temp;
	GXFogAdjTbl table[10];

	//
	// set up viewpoint
	//
	x = r_refdef.vrect.x * glwidth/vid.width;
	x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * glwidth/vid.width;
	y = r_refdef.vrect.y * glheight/vid.height;
	y2 = (r_refdef.vrect.y + r_refdef.vrect.height) * glheight/vid.height;

	// fudge around because of frac screen scale
	if (x > 0)
		x--;
	if (x2 < glwidth)
		x2++;
	if (y > 0)
		y--;
	if (y2 < glheight)
		y2++;

	w = x2 - x;
	h = y2 - y;

	if (envmap)
	{
		x = y2 = 0;
		w = h = 256;
	}

	viewport_size[0] = glx + x;
	viewport_size[1] = gly + y;
	viewport_size[2] = w;
	viewport_size[3] = h;
	//GX_SetViewport(viewport_size[0], viewport_size[1], viewport_size[2], viewport_size[3], 0.0f, 1.0f);
	GX_SetViewport(glx, gly, glwidth > 640 ? 640 : glwidth, glheight, 0.0f, 1.0f);
    screenaspect = (float)r_refdef.vrect.width/r_refdef.vrect.height;
	guPerspective (perspective, r_refdef.fov_y, screenaspect, ZMIN3D, ZMAX3D);

	// ELUTODO: perspective is 4x4, these ops are 4x3
	if (mirror)
	{
		if (mirror_plane->normal[2])
		{
			guMtxScale (temp, 1, -1, 1);
			guMtxConcat(perspective, temp, perspective);
		}
		else {
			guMtxScale (temp, -1, 1, 1);
			guMtxConcat(perspective, temp, perspective);
			GX_SetCullMode(GX_CULL_FRONT);
		}
	}
	else {
		GX_SetCullMode(GX_CULL_BACK);
	}

	GX_LoadProjectionMtx(perspective, GX_PERSPECTIVE);

	guMtxIdentity(view);

	guMtxRotAxisRad(temp, &axis0, DegToRad(-90.0f));		// put Z going up
	guMtxConcat(view, temp, view);
	guMtxRotAxisRad(temp, &axis2, DegToRad(90.0f));		// put Z going up
	guMtxConcat(view, temp, view);

	guMtxRotAxisRad(temp, &axis0, DegToRad(-r_refdef.viewangles[2]));
	guMtxConcat(view, temp, view);
	guMtxRotAxisRad(temp, &axis1, DegToRad(-r_refdef.viewangles[0]));
	guMtxConcat(view, temp, view);
	guMtxRotAxisRad(temp, &axis2, DegToRad(-r_refdef.viewangles[1]));
	guMtxConcat(view, temp, view);


	guMtxTrans(temp, -r_refdef.vieworg[0],  -r_refdef.vieworg[1],  -r_refdef.vieworg[2]);
	guMtxConcat(view, temp, view);
	
	// initialize and set fog table parameters 
	GX_InitFogAdjTable	(table, r_refdef.vrect.width, perspective);
	GX_SetFogRangeAdj(GX_ENABLE, screenaspect, table);
	Fog_EnableGFog ();

	// ELUTODOglGetFloatv (GL_MODELVIEW_MATRIX, r_world_matrix);

	//
	// set drawing parms
	//
	if (!gl_cull.value)
		GX_SetCullMode(GX_CULL_NONE);

	QGX_Blend(false);
	QGX_Alpha(false);
	QGX_ZMode(true);
}

/*
================
R_RenderScene

r_refdef must be set before the first call
================
*/
void R_RenderScene (void)
{
	//GX_SetTevOp(GX_TEVSTAGE0, GX_REPLACE);

	R_SetupFrame ();

	R_SetFrustum ();

	R_SetupGL ();

	R_MarkLeaves ();	// done here so we know if we're in water

	GX_LoadPosMtxImm(view, GX_PNMTX0);
	R_DrawWorld ();		// adds static entities to the list

	S_ExtraUpdate ();	// don't let sound get messed up if going slow

	// for the entities, we load the matrices separately
	R_DrawEntitiesOnList ();

	//GL_DisableMultitexture();

	GX_LoadPosMtxImm(view, GX_PNMTX0);
	// sBTODO: Fix TexCoord mapping for decals 
	//R_DrawDecals();
	R_DrawParticles ();
}


/*
=============
R_Clear
=============
*/
void R_Clear (void)
{
	// Not needed, GX clears the efb while copying to the xfb
}

/*
=============
R_Mirror
=============
*/
void R_Mirror (void)
{
	float		d;
	//msurface_t	*s;
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
/* ELUTODO
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
*/
}

/*
================
R_RenderView

r_refdef must be set before the first call
================
*/
void R_RenderView (void)
{
	double	time1=0, time2=0;

	if (r_norefresh.value)
		return;

	if (!r_worldentity.model || !cl.worldmodel)
		Sys_Error ("R_RenderView: NULL worldmodel");

	if (r_speeds.value)
	{
		// ELUTODO glFinish ();
		time1 = Sys_FloatTime ();
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

	mirror = false;

/* ELUTODO
	if (gl_finish.value)
		glFinish ();
*/

	R_Clear ();

	// render normal view

	R_RenderScene ();
	R_DrawViewModel ();
	R_DrawView2Model ();
	GX_LoadPosMtxImm(view, GX_PNMTX0);
	R_DrawWaterSurfaces ();

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
