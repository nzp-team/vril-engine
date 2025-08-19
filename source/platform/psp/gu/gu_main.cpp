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
// r_main.c

extern "C"
{
#include "../../../nzportable_def.h"
float TraceLine (vec3_t start, vec3_t end, vec3_t impact, vec3_t normal);
}

//includes
#include <pspgu.h>
#include <pspgum.h>

#include "../clipping.hpp"

using namespace quake;

//prototypes
extern "C" void V_CalcBlend (void);

void R_DrawDecals (void);
void R_RenderDecals (void);
void R_MarkLeaves (void);
// void QMB_LetItRain(void);
void QMB_LaserSight (void);
void VID_SetPaletteH2();
void VID_SetPaletteTX();

// globals
const float piconst = GU_PI / 180.0f;

entity_t	r_worldentity;
entity_t 	*currententity;

qboolean 	envmap;
qboolean 	mirror;

// view origin
vec3_t 		vup;
vec3_t 		vpn;
vec3_t 		vright;
vec3_t 		r_origin;

vec3_t		modelorg, r_entorigin;


int			r_visframecount;	// bumped when going to a new PVS
int			r_framecount;		// used for dlight push checking
int			c_brush_polys, c_alias_polys, c_md3_polys;
int			currenttexture = -1;		// to avoid unnecessary texture sets
int			cnttextures[2] = {-1, -1};     // cached
int			particletexture;	// little dot for particles
int			playertextures;		// up to 16 color translated skins
int			mirrortexturenum;	// quake texturenum, not gltexturenum
int 		game_fps; 			// should probably move this somewhere less.. lame

mplane_t	*mirror_plane;
mplane_t 	frustum[5];

// screen size info
refdef_t 	r_refdef;
mleaf_t 	*r_viewleaf;
mleaf_t 	*r_oldviewleaf;
texture_t 	*r_notexture_mip;

bool    fixlight;
bool    alphafunc2;
bool    alphafunc;

ScePspFMatrix4	r_world_matrix;
ScePspFMatrix4	r_base_world_matrix;
ScePspFMatrix4  md3mult;

int		d_lightstylevalue[256];	// 8.8 fraction of base light value

cvar_t	r_partalpha           = {"r_partalpha",             "0.8",true};
cvar_t	r_norefresh           = {"r_norefresh",               "0"};
cvar_t	r_drawentities        = {"r_drawentities",            "1"};
cvar_t	r_drawviewmodel       = {"r_drawviewmodel",           "1"};
cvar_t	r_speeds              = {"r_speeds",                  "0"};
cvar_t	r_fullbright          = {"r_fullbright",              "0"};
cvar_t	r_lightmap            = {"r_lightmap",                "0"};
cvar_t	r_shadows             = {"r_shadows",                 "0"};
cvar_t	r_mirroralpha         = {"r_mirroralpha",             "1",true};
cvar_t	r_wateralpha          = {"r_wateralpha",            "0.6",true};
cvar_t	r_vsync               = {"r_vsync",                   "0",true};
cvar_t	r_farclip	          = {"r_farclip",              "4096"};        //far cliping for q3 models
cvar_t  r_loadq3models        = {"r_loadq3models",            "0",true}; //replace player model to q3 player
cvar_t  cl_loadmapcfg         = {"cl_loadmapcfg",             "0",true}; //Load individual cfg for map
cvar_t	r_restexf             = {"r_restexf",                 "0",true}; //texture resampler setup
cvar_t  r_texcompr            = {"r_texcompr",                "5",true}; //texture compression setup (default DXT5) warning DXT1 conflicted with switches palettes
cvar_t  r_maxrange            = {"r_maxrange",             "4096"}; //render distance
cvar_t	r_skydis              = {"r_skydis",               "2560",true};
cvar_t  r_skyfog              = {"r_skyfog",                  "1",true};
cvar_t	r_caustics            = {"r_caustics",                "1",true};
cvar_t	r_detail              = {"r_detail",                  "1",true};
cvar_t	r_detail_mipmaps      = {"r_detail_mipmaps",          "1",true};
cvar_t	r_detail_mipmaps_func = {"r_detail_mipmaps_func",     "2",true};
cvar_t	r_detail_mipmaps_bias = {"r_detail_mipmaps_bias",    "-6",true};
cvar_t  r_asynch              = {"r_asynch",                  "0"};
cvar_t  r_i_model_animation   = {"r_i_model_animation",       "1",true}; // Toggle smooth model animation
cvar_t  r_i_model_transform   = {"r_i_model_transform",       "1",true}; // Toggle smooth model movement
cvar_t	r_mipmaps             = {"r_mipmaps",                 "1",true};
cvar_t  r_mipmaps_func        = {"r_mipmaps_func",            "2",true};          // Adjust mip map calculations
cvar_t  r_mipmaps_bias        = {"r_mipmaps_bias",           "-7",true};         // Adjust mip map bias 
cvar_t	r_retro   	          = {"r_retro",  	              "0",true}; // dr_mabuse1981: "retro filter".
cvar_t	r_dynamic             = {"r_dynamic",                 "0"};
cvar_t	r_novis               = {"r_novis",                   "0"};
cvar_t	r_tex_scale_down      = {"r_tex_scale_down",          "1",true};
cvar_t	r_particles_simple    = {"r_particles_simple",        "0",true};
cvar_t	gl_keeptjunctions     = {"gl_keeptjunctions",         "0"};
cvar_t	r_waterripple         = {"r_waterripple",             "2",true};
cvar_t	r_waterwarp           = {"r_waterwarp",               "1",true};
cvar_t	r_fastsky             = {"r_fastsky",                 "1",true};
cvar_t  r_skycolor            = {"r_skycolor",         "64 64 70",true};
cvar_t  r_showbboxes          = {"r_showbboxes",              "0"};
cvar_t  r_showbboxes_full     = {"r_showbboxes_full",         "0",true};
cvar_t	r_showtris            = {"r_showtris",                "0"};
cvar_t	r_showtris_full       = {"r_showtris_full",           "0",true};
cvar_t	r_polyblend	          = {"r_polyblend",               "0",true};
cvar_t r_skyfogblend = {"r_skyfogblend", "0.6", true}; 

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
//Shpuld
cvar_t  r_model_brightness = { "r_model_brightness", "1", true};   // Toggle high brightness model lighting

//cypress
cvar_t 	r_runqmbparticles = {"r_runqmbparticles", 	"1", true};

extern cvar_t cl_maxfps;
extern cvar_t scr_fov_viewmodel;




bool modelIsArm (char *m_name)
{
    if (!strcmp (m_name, "progs/ai/zal.mdl") ||
        !strcmp (m_name, "progs/ai/zar.mdl") ||
        !strcmp (m_name, "progs/ai/bzal.mdl") ||
        !strcmp (m_name, "progs/ai/bzar.mdl") ||
        !strcmp (m_name, "progs/ai/zalc.mdl") ||
        !strcmp (m_name, "progs/ai/zarc.mdl"))
            return true;

    return false;

}
bool modelIsHead (char *m_name)
{
    if (!strcmp (m_name, "progs/ai/zh.mdl") ||
        !strcmp (m_name, "progs/ai/bzh.mdl") ||
        !strcmp (m_name, "progs/ai/zhc.mdl"))
            return true;

    return false;

}
bool modelIsBody (char *m_name)
{
    if (!strcmp (m_name, "progs/ai/zb.mdl") ||
        !strcmp (m_name, "progs/ai/bzb.mdl") ||
        !strcmp (m_name, "progs/ai/zbc.mdl"))
            return true;

    return false;

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
	for (i=0 ; i<5 ; i++)
	{
		res = BoxOnPlaneSide (mins, maxs, &frustum[i]);
		if (res == 2) return -1; 
		if (res == 3) ++intersections;
	}
	return intersections;
}

/*
=================
R_FrustumCheckSphere
=================
*/
int R_FrustumCheckSphere (vec3_t centre, float radius)
{
	int		i, res;
	mplane_t	*p;
	int intersections = 0;

	for (i=0, p=frustum ; i<5 ; i++, p++)
	{
		res = PlaneDiff(centre, p);
		if (res <= -radius) return -1;
		if (res < radius) ++intersections;
	}

	return intersections;
}


/*
=================
R_CullBox
Returns true if the box is completely outside the frustom
=================
*/
int R_CullBox (vec3_t mins, vec3_t maxs)
{
	int		result = 1; // Default to "all inside".
	int		i;

	for (i=0 ; i<4 ; i++)
	{
		const int plane_result = BoxOnPlaneSide(mins, maxs, &frustum[i]);
		if (plane_result == 2)
		{
			return 2;
		}
		else if (plane_result == 3)
		{
			result = 3;
		}
	}

	return result;
}

/*
=================
R_CullSphere

Returns true if the sphere is completely outside the frustum
=================
*/
qboolean R_CullSphere (vec3_t centre, float radius)
{
	int		i;
	mplane_t	*p;

	for (i=0, p=frustum ; i<5 ; i++, p++)
	{
		if (PlaneDiff(centre, p) <= -radius)
			return true;
	}

	return false;
}

/*
=============
R_RotateForEntity
=============
*/
void R_RotateForEntity (entity_t *e, int shadow, unsigned char scale)
{
	// Translate.
	const ScePspFVector3 translation =
	{
		e->origin[0], e->origin[1], e->origin[2]
	};
	sceGumTranslate(&translation);

	// Rotate.
    sceGumRotateZ(e->angles[YAW] * (GU_PI / 180.0f));
	if (shadow == 0)
	{
		sceGumRotateY (-e->angles[PITCH] * (GU_PI / 180.0f));
		sceGumRotateX (e->angles[ROLL] * (GU_PI / 180.0f));
	}

	// Scale.
	if (scale != ENTSCALE_DEFAULT) {
		float scalefactor = ENTSCALE_DECODE(scale);
		const ScePspFVector3 scale =
		{
		scalefactor, scalefactor, scalefactor
		};
		sceGumScale(&scale);
	}

	sceGumUpdateMatrix();
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
	
	// Translate.
	const ScePspFVector3 translation = 
	{
		e->origin[0] + (blend * deltaVec[0]),
		e->origin[1] + (blend * deltaVec[1]),
		e->origin[2] + (blend * deltaVec[2])
	};
	sceGumTranslate(&translation);

	
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

	// Rotate.
	sceGumRotateZ((e->angles1[YAW] + ( blend * deltaVec[YAW])) * (GU_PI / 180.0f));
	if (shadow == 0)
	{
		sceGumRotateY ((-e->angles1[PITCH] + (-blend * deltaVec[PITCH])) * (GU_PI / 180.0f));
		sceGumRotateX ((e->angles1[ROLL] + ( blend * deltaVec[ROLL])) * (GU_PI / 180.0f));
	}

	sceGumUpdateMatrix();
}



/*
=============
R_BlendedRotateForEntity

fenix@io.com: model transform interpolation
=============
*/
void R_BlendedRotateForEntity (entity_t *e, int shadow, unsigned char scale)	// Tomaz - New Shadow
{
    float timepassed;
    float blend;
    vec3_t d;
    int i;

    // positional interpolation

    timepassed = realtime - e->translate_start_time;

    if (e->translate_start_time == 0 || timepassed > 1)
    {
        e->translate_start_time = realtime;
        VectorCopy (e->origin, e->origin1);
        VectorCopy (e->origin, e->origin2);
    }

    if (!VectorCompare (e->origin, e->origin2))
    {
        e->translate_start_time = realtime;
        VectorCopy (e->origin2, e->origin1);
        VectorCopy (e->origin,  e->origin2);
        blend = 0;
    }
    else
    {
        blend =  timepassed / 0.1;
        if (cl.paused || blend > 1)
            blend = 0;
    }

    VectorSubtract (e->origin2, e->origin1, d);

    // Translate.
    const ScePspFVector3 translation = {
    e->origin[0] + (blend * d[0]),
    e->origin[1] + (blend * d[1]),
    e->origin[2] + (blend * d[2])
    };
    sceGumTranslate(&translation);

	// Scale.
	if (scale != ENTSCALE_DEFAULT) {
		float scalefactor = ENTSCALE_DECODE(scale);
		const ScePspFVector3 scale =
		{
		scalefactor, scalefactor, scalefactor
		};
		sceGumScale(&scale);
	}

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

    VectorSubtract (e->angles2, e->angles1, d);

    // always interpolate along the shortest path
    for (i = 0; i < 3; i++)
    {
        if (d[i] > 180)
        {
            d[i] -= 360;
        }
        else if (d[i] < -180)
        {
            d[i] += 360;
        }
    }

	// Rotate.
    sceGumRotateZ((e->angles1[YAW] + ( blend * d[YAW])) * (GU_PI / 180.0f));
	if (shadow == 0)
	{
		sceGumRotateY ((-e->angles1[PITCH] + (-blend * d[PITCH])) * (GU_PI / 180.0f));
		sceGumRotateX ((e->angles1[ROLL] + ( blend * d[ROLL])) * (GU_PI / 180.0f));
	}

	sceGumUpdateMatrix();
}

/*
=============================================================

  SPRITE MODELS

=============================================================
*/

extern vec3_t lightcolor; // LordHavoc: .lit support

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

	psprite = static_cast<msprite_t*>(currententity->model->cache.data);
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
	vec3_t			point, v_forward, v_right, v_up;
	msprite_t		*psprite;
	mspriteframe_t	*frame;
	float			*s_up, *s_right;
	float			angle, sr, cr;
	float 			scale = ENTSCALE_DECODE(e->scale);
	if (scale == 0) scale = 1.0f;
	bool additive = false;
	bool filter = false;

	// don't even bother culling, because it's just a single polygon without a surface cache
	frame = R_GetSpriteFrame (e);
	psprite = static_cast<msprite_t*>(currententity->model->cache.data);

	switch(psprite->type)
	{
	case SPR_VP_PARALLEL_UPRIGHT: //faces view plane, up is towards the heavens
		v_up[0] = 0;
		v_up[1] = 0;
		v_up[2] = 1;
		s_up = v_up;
		s_right = vright;
		break;
	case SPR_FACING_UPRIGHT: //faces camera origin, up is towards the heavens
		VectorSubtract(currententity->origin, r_origin, v_forward);
		v_forward[2] = 0;
		VectorNormalizeFast(v_forward);
		v_right[0] = v_forward[1];
		v_right[1] = -v_forward[0];
		v_right[2] = 0;
		v_up[0] = 0;
		v_up[1] = 0;
		v_up[2] = 1;
		s_up = v_up;
		s_right = v_right;
		break;
	case SPR_VP_PARALLEL: //faces view plane, up is towards the top of the screen
		s_up = vup;
		s_right = vright;
		break;
	case SPR_ORIENTED: //pitch yaw roll are independent of camera
		AngleVectors (currententity->angles, v_forward, v_right, v_up);
		s_up = v_up;
		s_right = v_right;
		break;
	case SPR_VP_PARALLEL_ORIENTED: //faces view plane, but obeys roll value
		angle = DEG2RAD(currententity->angles[ROLL]);
		#ifdef PSP_VFPU
		sr = vfpu_sinf(angle);
		cr = vfpu_cosf(angle);
		#else
		sr = sin(angle);
		cr = cos(angle);
		#endif
		v_right[0] = vright[0] * cr + vup[0] * sr;
		v_up[0] = vright[0] * -sr + vup[0] * cr;
		v_right[1] = vright[1] * cr + vup[1] * sr;
		v_up[1] = vright[1] * -sr + vup[1] * cr;
		v_right[2] = vright[2] * cr + vup[2] * sr;
		v_up[2] = vright[2] * -sr + vup[2] * cr;
		s_up = v_up;
		s_right = v_right;
		break;
	default:
		return;
	}

	if (psprite->beamlength == 10) // we use the beam length of sprites, since they are unused by quake anyway.
		additive = true;

	if (psprite->beamlength == 20)
		filter = true;

	// Bind the texture.
	GL_Bind(frame->gl_texturenum);

	sceGuEnable(GU_BLEND);
	sceGuDepthMask(GU_TRUE);

	Fog_DisableGFog ();

	if (additive)
	{
		sceGuDepthMask(GU_TRUE);
		sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0xFFFFFFFF);
		sceGuTexFunc(GU_TFX_MODULATE , GU_TCC_RGB);
	}
	else if (filter)
	{
		sceGuDepthMask(GU_TRUE);
		sceGuBlendFunc(GU_ADD, GU_FIX, GU_SRC_COLOR, 0, 0);
		sceGuTexFunc(GU_TFX_MODULATE , GU_TCC_RGB);
	}
	else
		sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);

	// Allocate memory for this polygon.
	glvert_t* const	vertices =
		static_cast<glvert_t*>(sceGuGetMemory(sizeof(glvert_t) * 4));

	VectorMA (e->origin, frame->down * scale, s_up, point);
	VectorMA (point, frame->left * scale, s_right, point);

	vertices[0].st[0]	= 0.0f;
	vertices[0].st[1]	= 1.0f;
	vertices[0].xyz[0]	= point[0];
	vertices[0].xyz[1]	= point[1];
	vertices[0].xyz[2]	= point[2];

	VectorMA (e->origin, frame->up * scale, s_up, point);
	VectorMA (point, frame->left * scale, s_right, point);

	vertices[1].st[0]	= 0.0f;
	vertices[1].st[1]	= 0.0f;
	vertices[1].xyz[0]	= point[0];
	vertices[1].xyz[1]	= point[1];
	vertices[1].xyz[2]	= point[2];

	VectorMA (e->origin, frame->up * scale, s_up, point);
	VectorMA (point, frame->right * scale, s_right, point);

	vertices[2].st[0]	= 1.0f;
	vertices[2].st[1]	= 0.0f;
	vertices[2].xyz[0]	= point[0];
	vertices[2].xyz[1]	= point[1];
	vertices[2].xyz[2]	= point[2];

	VectorMA (e->origin, frame->down * scale, s_up, point);
	VectorMA (point, frame->right * scale, s_right, point);

	vertices[3].st[0]	= 1.0f;
	vertices[3].st[1]	= 1.0f;
	vertices[3].xyz[0]	= point[0];
	vertices[3].xyz[1]	= point[1];
	vertices[3].xyz[2]	= point[2];

	// Draw the clipped vertices.
	sceGuDrawArray(	GU_TRIANGLE_FAN,GU_TEXTURE_32BITF | GU_VERTEX_32BITF,4, 0, vertices);

	sceGuDepthMask(GU_FALSE);
	sceGuDisable(GU_BLEND);

	Fog_EnableGFog ();

	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
	sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
}

/*
=============================================================

  ALIAS MODELS

=============================================================
*/


#define NUMVERTEXNORMALS	162

extern "C" float	r_avertexnormals[NUMVERTEXNORMALS][3];
float r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "../../../anorms.h"
};

vec3_t	shadevector;
float	shadelight, ambientlight;

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
float	r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "../../../anorm_dots.h"
;

float	*shadedots = r_avertexnormal_dots[0];

// fenix@io.com: model animation interpolation
int lastposenum0;
//

int	lastposenum;

// fenix@io.com: model transform interpolation
float old_i_model_transform;
//

// void GL_DrawAliasBlendedWireFrame (aliashdr_t *paliashdr, int pose1, int pose2, float blend)
// {
// 	trivertx_t* verts1;
// 	trivertx_t* verts2;
// 	int*        order;
// 	int         count;
// 	vec3_t      d;
// 	vec3_t       point;

// 	lastposenum0 = pose1;
// 	lastposenum  = pose2;

// 	verts1 = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
// 	verts2 = verts1;

// 	verts1 += pose1 * paliashdr->poseverts;
// 	verts2 += pose2 * paliashdr->poseverts;

// 	order = (int *)((byte *)paliashdr + paliashdr->commands);

// 	int numcommands = order[0];
// 	order++;

// 	struct vertex
// 	{
// 		int uvs;
// 		char x, y, z;
// 		char _padding;
// 	};

// 	sceGuColor(GU_COLOR(lightcolor[0], lightcolor[1], lightcolor[2], 1.0f));

// 	// Allocate the vertices.
// 	vertex* const out = static_cast<vertex*>(sceGuGetMemory(sizeof(vertex) * numcommands));
// 	int vertex_index = 0;

// 	//for blubs's alternate BuildTris: 1) Disable while(1) loop 2) Disable the break; 3) replace GU_TRIANGLE_STRIP with GU_TRIANGLES
// 	while (1)
// 	{
// 		// get the vertex count and primitive type
// 		count = *order++;

// 		if (!count) break;
// 		if (count < 0)
// 			count = -count;

// 		for (int start = vertex_index; vertex_index < (start + count); ++vertex_index)
// 		{

// 			// texture coordinates come from the draw list
// 			out[vertex_index].uvs = order[0];
// 			order += 1;

// 			VectorSubtract(verts2->v, verts1->v, d);

// 			// blend the vertex positions from each frame together
// 			point[0] = verts1->v[0] + (blend * d[0]);
// 			point[1] = verts1->v[1] + (blend * d[1]);
// 			point[2] = verts1->v[2] + (blend * d[2]);

// 			out[vertex_index].x = point[0];
// 			out[vertex_index].y = point[1];
// 			out[vertex_index].z = point[2];

// 			++verts1;
// 			++verts2;
// 		}
// 		sceGuDrawArray(GU_LINE_STRIP, GU_TEXTURE_16BIT | GU_VERTEX_8BIT, count, 0, &out[vertex_index - count]);
// 	}

// 	sceGuEnable(GU_TEXTURE_2D);
// 	sceGuColor(0xffffffff);
// }


/*
=============
GL_DrawAliasFrame
=============
*/
void GL_DrawAliasFrame (aliashdr_t *paliashdr, int posenum)
{
	// if (r_showtris.value)
	// {
	// 	GL_DrawAliasBlendedWireFrame(paliashdr, posenum, posenum, 0);
	// 	return;
	// }
	trivertx_t	*verts;
	int		*order;
	int		count;
	int prim;

	lastposenum = posenum;
	verts = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	order = (int *)((byte *)paliashdr + paliashdr->commands);

	int numcommands = order[0];
	order++;

	qboolean isStatic = paliashdr->numposes <= 1 ? true : false;

	struct vertex
	{
		int uvs;
		int xyz;
	};

	sceGuColor(GU_COLOR(lightcolor[0], lightcolor[1], lightcolor[2], 1.0f));


	if (isStatic)
	{
		while (1)
		{
			// get the vertex count and primitive type
			count = *order++;
			if (!count)
				break;		// done

			if (count < 0)
			{
				prim = GU_TRIANGLE_FAN;
				count = -count;
			}
			else
			{
				prim = GU_TRIANGLE_STRIP;
			}

			sceGuDrawArray(prim, GU_TEXTURE_16BIT | GU_VERTEX_8BIT, count, 0, order);
			order += 2 * count;
		}
	}
	else
	{
		// Allocate the vertices.
		vertex* const out = static_cast<vertex*>(sceGuGetMemory(sizeof(vertex) * numcommands));
		int vertex_index = 0;
		//for blubs's alternate BuildTris: 1) Disable while(1) loop 2) Disable the break; 3) replace GU_TRIANGLE_STRIP with GU_TRIANGLES
		while (1)
		{
			// get the vertex count and primitive type
			count = *order++;
			if (!count)
				break;		// done
			if (count < 0)
			{
				prim = GU_TRIANGLE_FAN;
				count = -count;
			}
			else
			{
				prim = GU_TRIANGLE_STRIP;
				//prim = GU_TRIANGLES; //used for blubs' alternate BuildTris with one continual triangle list
			}
			//================================================================== fps: 50 ===============================================
			for (int start = vertex_index; vertex_index < (start + count); ++vertex_index, ++order, ++verts)
			{
				// texture coordinates come from the draw list
				out[vertex_index].uvs = order[0];
				out[vertex_index].xyz = ((int*)verts->v)[0]; // cast to int because trivertx is is 4 bytes
			}
			sceGuDrawArray(prim, GU_TEXTURE_16BIT | GU_VERTEX_8BIT, count, 0, &out[vertex_index - count]);
			
			//================================================================== fps: 50 ===============================================
		}
	}
	sceGuColor(0xffffffff);
}

/*
=============
GL_DrawAliasBlendedFrame

fenix@io.com: model animation interpolation
=============
*/
void GL_DrawAliasBlendedFrame (aliashdr_t *paliashdr, int pose1, int pose2, float blend)
{
	// if (r_showtris.value)
	// {
	// 	GL_DrawAliasBlendedWireFrame(paliashdr, pose1, pose2, blend);
	// 	return;
	// }
	trivertx_t* verts1;
	trivertx_t* verts2;
	int*        order;
	int         count;
	int prim;
	prim = GU_TRIANGLE_FAN;

	lastposenum0 = pose1;
	lastposenum  = pose2;

	verts1 = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts2 = verts1;

	verts1 += pose1 * paliashdr->poseverts;
	verts2 += pose2 * paliashdr->poseverts;

	order = (int *)((byte *)paliashdr + paliashdr->commands);

	int numcommands = order[0];
	order++;

	struct vertex
	{
		int uvs;
		int xyz;
	};

	sceGuColor(GU_COLOR(lightcolor[0], lightcolor[1], lightcolor[2], 1.0f));

	// Allocate the vertices.
	vertex* const out = static_cast<vertex*>(sceGuGetMemory(sizeof(vertex) * numcommands * 2));
	int vertex_index = 0;

	//for blubs's alternate BuildTris: 1) Disable while(1) loop 2) Disable the break; 3) replace GU_TRIANGLE_STRIP with GU_TRIANGLES
	while (1)
	{
		// get the vertex count and primitive type
		count = *order++;

		if (!count) break;
		if (count < 0)
		{
			prim = GU_TRIANGLE_FAN;
			count = -count;
		}
		else
		{
			prim = GU_TRIANGLE_STRIP;
			//prim = GU_TRIANGLES; //used for blubs' alternate BuildTris with one continual triangle list
		}

		for (int start = vertex_index; vertex_index < (start + count * 2); ++vertex_index, ++order, ++verts1, ++verts2)
		{
			out[vertex_index].uvs = order[0];
			out[vertex_index].xyz = ((int*)verts1->v)[0];
			++vertex_index;
			out[vertex_index].uvs = order[0];
			out[vertex_index].xyz = ((int*)verts2->v)[0];
		}
		sceGuMorphWeight(0, 1 - blend);
		sceGuMorphWeight(1, blend);
		sceGuDrawArray(prim, GU_TEXTURE_16BIT | GU_VERTEX_8BIT | GU_VERTICES(2), count, 0, &out[vertex_index - count * 2]);
	}
	sceGuColor(0xffffffff);
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

	GL_DrawAliasFrame (paliashdr, pose);
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
=====================
R_DrawZombieLimb

=====================
*/

void R_DrawZombieLimb (entity_t *e,int which)
{
	//entity_t *e;
	model_t		*clmodel;
	aliashdr_t	*paliashdr;
	entity_t    *limb_ent;

	//e = &cl_entities[ent];
	//clmodel = e->model;

	if(which == 1)
		limb_ent = &cl_entities[e->z_head];
	else if(which == 2)
		limb_ent = &cl_entities[e->z_larm];
	else if(which == 3)
		limb_ent = &cl_entities[e->z_rarm];
	else
		return;
	
	clmodel = limb_ent->model;
	if(clmodel == NULL)
		return;

	VectorCopy (e->origin, r_entorigin);
	VectorSubtract (r_origin, r_entorigin, modelorg);

	// locate the proper data
	paliashdr = (aliashdr_t *)Mod_Extradata (clmodel);//e->model
	c_alias_polys += paliashdr->numtris;

	sceGumPushMatrix();

	
	//movement interpolation by blubs
	R_InterpolateEntity(e,0);
	
	//blubs disabled
	/*if (r_i_model_transform.value)
		R_BlendedRotateForEntity (e, 0);
	else
		R_RotateForEntity (e, 0);*/

	const ScePspFVector3 translation =
	{
		paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]
	};
	sceGumTranslate(&translation);

	const ScePspFVector3 scaling =
	{
		paliashdr->scale[0] * 128.f, paliashdr->scale[1] * 128.f, paliashdr->scale[2] * 128.f
	};
	sceGumScale(&scaling);

	sceGumUpdateMatrix();

	IgnoreInterpolatioFrame(e, paliashdr);

	// Make sure we never try to do blended frame on models with just single frame
	if (r_i_model_animation.value && paliashdr->numposes > 1)
	{
		R_SetupAliasBlendedFrame (e->frame, paliashdr, e);
	}
	else
	{
		R_SetupAliasFrame (e->frame, paliashdr);
	}
	//t3 += Sys_FloatTime();
	sceGumPopMatrix();
	sceGumUpdateMatrix();
}
/*
=================
R_DrawTransparentAliasModel
blubs: used for semitransparent fullbright models (like their sprite counterparts)
=================
*/

void R_DrawTransparentAliasModel (entity_t *e)
{
	model_t		*clmodel;
	vec3_t		mins, maxs;
	aliashdr_t	*paliashdr;
	int			anim;

        clmodel = e->model;

	VectorAdd (e->origin, clmodel->mins, mins);
	VectorAdd (e->origin, clmodel->maxs, maxs);

	if (R_CullBox(mins, maxs) == 2)
		return;
	
	VectorCopy (e->origin, r_entorigin);
	VectorSubtract (r_origin, r_entorigin, modelorg);

	//
	// get lighting information
	// LordHavoc: .lit support begin
	//ambientlight = shadelight = R_LightPoint (e->origin); // LordHavoc: original code, removed shadelight and ambientlight
	R_LightPoint(e->origin); // LordHavoc: lightcolor is all that matters from this
	// LordHavoc: .lit support end
	lightcolor[0] = lightcolor[1] = lightcolor[2] = 256;
	// LordHavoc: .lit support begin
	//shadelight = shadelight / 200.0; // LordHavoc: original code
	VectorScale(lightcolor, 1.0f / 200.0f, lightcolor);
	// LordHavoc: .lit support end

	// locate the proper data//
	paliashdr = (aliashdr_t *)Mod_Extradata (e->model);
	c_alias_polys += paliashdr->numtris;
	// draw all the triangles//
	sceGumPushMatrix();
	R_InterpolateEntity(e,0);
	const ScePspFVector3 translation =
	{
		paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]
	};
	sceGumTranslate(&translation);
	const ScePspFVector3 scaling =
	{
		paliashdr->scale[0] * 128.f, paliashdr->scale[1] * 128.f, paliashdr->scale[2] * 128.f
	};
	sceGumScale(&scaling);

	//for models(pink transparency)
	sceGuEnable(GU_BLEND);
	sceGuEnable(GU_ALPHA_TEST);
	sceGuAlphaFunc(GU_GREATER, 0, 0xff);
	
	sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
	
	//st1x:now quake transparency is working
	//force_fullbright
	//sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
	
	sceGuShadeModel(GU_SMOOTH);
	sceGumUpdateMatrix();
	IgnoreInterpolatioFrame(e, paliashdr);
	anim = (int)(cl.time*10) & 3;
	GL_Bind(paliashdr->gl_texturenum[e->skinnum][anim]);
	//Rendering block
	if (r_i_model_animation.value)
	{
		R_SetupAliasBlendedFrame (e->frame, paliashdr, e);
	}
	else
	{
		R_SetupAliasFrame (e->frame, paliashdr);
	}
	sceGumPopMatrix();
	sceGumUpdateMatrix();
	
	//st1x:now quake transparency is working
	sceGuAlphaFunc(GU_GREATER, 0, 0xff);
	sceGuDisable(GU_ALPHA_TEST);
	sceGuTexFunc(GU_TFX_REPLACE , GU_TCC_RGBA);
	sceGuShadeModel(GU_FLAT);

//	else if(ISGLOW(e))
	{
		sceGuDepthMask(GU_FALSE);
		//sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
		//sceGuDisable (GU_BLEND);
	}
	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
	sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
	
	sceGuDisable(GU_BLEND);
}


/*
=================
R_DrawAliasModel

=================
*/

int partial_zombies_drawn;
aliashdr_t * zcfull_mdl;
aliashdr_t * zfull_mdl;

void R_DrawAliasModel (entity_t *e)
{
	char			specChar;
	model_t		*clmodel;
	vec3_t		mins, maxs;
	aliashdr_t	*paliashdr;
	int			anim;
	bool 		force_fullbright;

    clmodel = e->model;

	VectorAdd (e->origin, clmodel->mins, mins);
	VectorAdd (e->origin, clmodel->maxs, maxs);

	if (R_CullBox(mins, maxs) == 2)
		return;

	//=============================================================================================== 97% at this point
	if(ISADDITIVE(e))
	{
		float deg = e->renderamt;
		float alpha_val  = deg;
		float alpha_val2 = 1 - deg;

		if(deg <= 0.7)
		 sceGuDepthMask(GU_TRUE);

		sceGuEnable (GU_BLEND);
		sceGuBlendFunc(GU_ADD, GU_FIX, GU_FIX,
		GU_COLOR(alpha_val,alpha_val,alpha_val,alpha_val),
		GU_COLOR(alpha_val2,alpha_val2,alpha_val2,alpha_val2));
	}
	else if(ISGLOW(e))
	{
		sceGuDepthMask(GU_TRUE);
		sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_FIX, 0, 0xFFFFFFFF);
		sceGuTexFunc(GU_TFX_MODULATE , GU_TCC_RGBA);
	}
	else if (ISSOLID(e))
	{
		sceGuEnable(GU_ALPHA_TEST);
		float c = (e->renderamt) * 255.0f;
		sceGuAlphaFunc(GU_GREATER, 0x88, c);
	}
	force_fullbright = false;

	VectorCopy (e->origin, r_entorigin);
	VectorSubtract (r_origin, r_entorigin, modelorg);

	//blubswillrule: disabled dynamic lights
	/*for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
	{
		if (cl_dlights[lnum].die >= cl.time)
		{
			VectorSubtract (e->origin,cl_dlights[lnum].origin,dist);
			add = cl_dlights[lnum].radius - Length(dist);

			if (add > 0)
			{
				lightcolor[0] += add * cl_dlights[lnum].color[0];
				lightcolor[1] += add * cl_dlights[lnum].color[1];
				lightcolor[2] += add * cl_dlights[lnum].color[2];
			}
			// LordHavoc: .lit support end
		}
	}*/

	specChar = clmodel->name[strlen(clmodel->name) - 5];
	if(specChar == '!' || e->effects & EF_FULLBRIGHT)
	{
		lightcolor[0] = lightcolor[1] = lightcolor[2] = 256;
		force_fullbright = true;
	}
	if(specChar == '@')
	{
		alphafunc = true;
	}
	if(specChar == '&')
	{
		lightcolor[0] = lightcolor[1] = lightcolor[2] = 256;
		force_fullbright = true;
		alphafunc = true;
	}

	//
	// get lighting information
	//

	if (!force_fullbright)
	{
		// LordHavoc: .lit support begin
		//ambientlight = shadelight = R_LightPoint (e->origin); // LordHavoc: original code, removed shadelight and ambientlight
		R_LightPoint(e->origin); // LordHavoc: lightcolor is all that matters from this
		// LordHavoc: .lit support end
		
		//Shpuld
		if(r_model_brightness.value)
		{
			lightcolor[0] += 32;
			lightcolor[1] += 32;
			lightcolor[2] += 32;
		}

		for(int g = 0; g < 3; g++)
		{
			if(lightcolor[g] < 8)
			lightcolor[g] = 8;
			if(lightcolor[g] > 125)
			lightcolor[g] = 125;
		}
	}
	
	//t3 += Sys_FloatTime();
	// shadedots = r_avertexnormal_dots[((int)(e->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];

	// LordHavoc: .lit support begin
	//shadelight = shadelight / 200.0; // LordHavoc: original code
	VectorScale(lightcolor, 1.0f / 200.0f, lightcolor);
	// LordHavoc: .lit support end

	//
	// locate the proper data
	//
	bool draw_partial_zombie = false;
	if (specChar == '%') {
		bool limbs_in_tact = (e->z_head != 0) && (e->z_larm != 0) && (e->z_rarm != 0);
		if (partial_zombies_drawn > 5 || limbs_in_tact) {
			if(clmodel->name[12] == 'c')
				paliashdr = zcfull_mdl;
			else
				paliashdr = zfull_mdl;
		} else {
			partial_zombies_drawn += 1;
			draw_partial_zombie = true;
			paliashdr = (aliashdr_t *)Mod_Extradata (e->model);
		}
	}
	else
		paliashdr = (aliashdr_t *)Mod_Extradata (e->model);

	c_alias_polys += paliashdr->numtris;

	//
	// draw all the triangles
	//
	sceGumPushMatrix();

	R_InterpolateEntity(e,0);
	

	// Special handling of view model to keep FOV from altering look.  Pretty good.  Not perfect but rather close.
	if ((e == &cl.viewent || e == &cl.viewent2) && scr_fov_viewmodel.value) {
		float scale = 1.0f / tan (DEG2RAD (scr_fov.value / 2.0f)) * scr_fov_viewmodel.value / 90.0f;
		if (e->scale != ENTSCALE_DEFAULT && e->scale != 0) scale *= ENTSCALE_DECODE(e->scale);

		const ScePspFVector3 translation = {
			paliashdr->scale_origin[0] * scale, paliashdr->scale_origin[1], paliashdr->scale_origin[2]
		};
		const ScePspFVector3 scaling = {
			paliashdr->scale[0] * scale * 128.f, paliashdr->scale[1] * 128.f, paliashdr->scale[2] * 128.f
		};

		sceGumTranslate(&translation);
		sceGumScale(&scaling);
	} else {
		float scale = 1.0f;
		if (e->scale != ENTSCALE_DEFAULT && e->scale != 0) scale *= ENTSCALE_DECODE(e->scale);

		const ScePspFVector3 translation = {
			paliashdr->scale_origin[0] * scale, paliashdr->scale_origin[1] * scale, paliashdr->scale_origin[2] * scale
		};
		const ScePspFVector3 scaling = {
			paliashdr->scale[0] * (scale * 128.0f), paliashdr->scale[1] * (scale * 128.0f), paliashdr->scale[2] * (scale * 128.0f)
		};

		sceGumTranslate(&translation);
		sceGumScale(&scaling);
	}

	//============================================================================================================================= 83% at this point

	// we can't dynamically colormap textures, so they are cached
	// seperately for the players.  Heads are just uncolored.
	//if (e->colormap != vid.colormap && 0 /* && !gl_nocolors.value*/)
	//{
	//	i = e - cl_entities;
	//	if (i >= 1 && i<=cl.maxclients /*&& !strcmp (e->model->name, "models/player.mdl")*/)
	//	{
	//	    GL_Bind(playertextures - 1 + i);
	//	}
	//}

	//for models(pink transparency)
	if (alphafunc)
	{
		sceGuEnable(GU_ALPHA_TEST);
		sceGuAlphaFunc(GU_GREATER, 0, 0xff);
		sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
	}
	//st1x:now quake transparency is working
	if (force_fullbright)
		sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
	else
		sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
	//for models (blue transparency)
	if (alphafunc2 || alphafunc)
	{
		sceGuEnable(GU_ALPHA_TEST);
		sceGuAlphaFunc(GU_GREATER, 0xaa, 0xff);
		sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
	}
	sceGuShadeModel(GU_SMOOTH);

	sceGumUpdateMatrix();

	IgnoreInterpolatioFrame(e, paliashdr);

	anim = (int)(cl.time*10) & 3;
	GL_Bind(paliashdr->gl_texturenum[e->skinnum][anim]);

	//===================================================================================================== 80% at this point
	//Rendering block
	if (r_i_model_animation.value)
	{
		R_SetupAliasBlendedFrame (e->frame, paliashdr, e);
	}
	else
	{
		R_SetupAliasFrame (e->frame, paliashdr);
	}
	sceGumPopMatrix();
	sceGumUpdateMatrix();
	
	if (draw_partial_zombie && specChar == '%')//if we're drawing zombie, also draw its limbs in one call
	{
		if(e->z_head)
			R_DrawZombieLimb(e,1);
		if(e->z_larm)
			R_DrawZombieLimb(e,2);
		if(e->z_rarm)
			R_DrawZombieLimb(e,3);
	}
	//st1x:now quake transparency is working
	sceGuAlphaFunc(GU_GREATER, 0, 0xff);
	sceGuDisable(GU_ALPHA_TEST);
	sceGuTexFunc(GU_TFX_REPLACE , GU_TCC_RGBA);
	sceGuShadeModel(GU_FLAT);

	//Blubswillrule: disabled the next two calls, they look like duplicates
	//sceGuShadeModel(GU_FLAT);
	//sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
	if (ISADDITIVE(e))
	{
		float deg = e->renderamt;
		if(deg <= 0.7)
			sceGuDepthMask(GU_FALSE);
		//sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
		//sceGuDisable (GU_BLEND);
	}
	else if(ISSOLID(e))
	{
		sceGuAlphaFunc(GU_GREATER, 0, 0xff);
		sceGuDisable(GU_ALPHA_TEST);
	}
	else if(ISGLOW(e))
	{
		sceGuDepthMask(GU_FALSE);
		//sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
		//sceGuDisable (GU_BLEND);
	}
	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
	sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
	sceGuDisable(GU_BLEND);
}

extern cvar_t	scr_fov;

/*
=============
R_DrawNullModel
From pspq2
=============
*/
void R_DrawNullModel(void)
{
	R_LightPoint(currententity->origin);
	sceGumPushMatrix();
	sceGuDisable(GU_TEXTURE_2D);
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
	sceGuShadeModel (GU_SMOOTH);
	R_RotateForEntity(currententity, 0, currententity->scale);
	typedef struct VERT_t
	{
		float x, y, z;
	} VERT;
	VERT* v;
	sceGuColor(0x0099FF);
	v = (VERT*)sceGuGetMemory(sizeof(VERT) * 6);
	v[0].x =  0.0f; v[0].y =  0.0f; v[0].z =  9.0f;
	v[1].x =  9.0f; v[1].y =  0.0f; v[1].z =  0.0f;
	v[2].x =  0.0f; v[2].y = -9.0f; v[2].z =  0.0f;
	v[3].x = -9.0f; v[3].y =  0.0f; v[3].z =  0.0f;
	v[4].x =  0.0f; v[4].y =  9.0f; v[4].z =  0.0f;
	v[5].x =  9.0f; v[5].y =  0.0f; v[5].z =  0.0f;
	sceGumDrawArray(r_showtris.value ? GU_LINE_STRIP : GU_TRIANGLE_FAN, GU_VERTEX_32BITF | GU_TRANSFORM_3D, 6, 0, v);
	sceGuColor(0x0000FF);
	v = (VERT*)sceGuGetMemory(sizeof(VERT) * 6);
	v[0].x =  0.0f; v[0].y =  0.0f; v[0].z = -9.0f;
	v[1].x =  9.0f; v[1].y =  0.0f; v[1].z =  0.0f;
	v[2].x =  0.0f; v[2].y =  9.0f; v[2].z =  0.0f;
	v[3].x = -9.0f; v[3].y =  0.0f; v[3].z =  0.0f;
	v[4].x =  0.0f; v[4].y = -9.0f; v[4].z =  0.0f;
	v[5].x =  9.0f; v[5].y =  0.0f; v[5].z =  0.0f;
	sceGumDrawArray(r_showtris.value ? GU_LINE_STRIP : GU_TRIANGLE_FAN, GU_VERTEX_32BITF | GU_TRANSFORM_3D, 6, 0, v);
	sceGuTexFunc(GU_TFX_REPLACE , GU_TCC_RGBA);
	sceGuColor(0xFFFFFF);
	sceGuEnable(GU_TEXTURE_2D);
	sceGumPopMatrix();
}
/*
================
R_EmitWireBox -- johnfitz -- draws one axis aligned bounding box
================
*/
void R_EmitWireBox (vec3_t mins, vec3_t maxs,qboolean line_strip)
{
	// Allocate the vertices.
	struct vertex
	{
	   float x, y, z;
	};
	vertex* const out = static_cast<vertex*>(sceGuGetMemory(sizeof(vertex) * 10));
	out[0].x = mins[0]; out[0].y = mins[1]; out[0].z = mins[2];
	out[1].x = mins[0]; out[1].y = mins[1]; out[1].z = maxs[2];
	out[2].x = maxs[0]; out[2].y = mins[1]; out[2].z = mins[2];
	out[3].x = maxs[0]; out[3].y = mins[1]; out[3].z = maxs[2];
	out[4].x = maxs[0]; out[4].y = maxs[1]; out[4].z = mins[2];
	out[5].x = maxs[0]; out[5].y = maxs[1]; out[5].z = maxs[2];

	out[6].x = mins[0]; out[6].y = maxs[1]; out[6].z = mins[2];
	out[7].x = mins[0]; out[7].y = maxs[1]; out[7].z = maxs[2];
	out[8].x = mins[0]; out[8].y = mins[1]; out[8].z = mins[2];
	out[9].x = mins[0]; out[9].y = mins[1]; out[9].z = maxs[2];
	sceGuDrawArray(line_strip ? GU_LINE_STRIP : GU_TRIANGLE_STRIP,GU_VERTEX_32BITF, 10, 0, out);
}

void R_DrawLine(vec3_t start,vec3_t end, vec3_t rgb)
{

	//Do Before!
	sceGuDisable (GU_TEXTURE_2D);
	sceGuDisable (GU_CULL_FACE);
	sceGuColor(GU_COLOR(rgb[0],rgb[1],rgb[2],1));

	// Allocate the vertices.
	struct vertex
	{
	   float x, y, z;
	};
	vertex* const out = static_cast<vertex*>(sceGuGetMemory(sizeof(vertex) * 2));//10

	out[0].x = start[0]; out[0].y = start[1]; out[0].z = start[2];
	out[1].x = end[0]; out[1].y = end[1]; out[1].z = end[2];

	sceGuDrawArray(GU_LINE_STRIP,GU_VERTEX_32BITF, 2, 0, out);

	//Do After!
	sceGuColor(GU_COLOR(1,1,1,1));
	sceGuEnable (GU_TEXTURE_2D);
	sceGuEnable (GU_CULL_FACE);

}


/*
=============
R_DrawEntitiesOnList
=============
*/
extern int numgltextures;
// The values in these are indices in current frame cl_visedicts, pointing to first entity of linked list
short entity_batches[MAX_GLTEXTURES];
short trans_entity_batches[MAX_GLTEXTURES];
// are human rights btw

void R_DrawEntitiesOnList (void)
{
	if (!r_drawentities.value)
		return;
	// Because 0 is a valid index for a visedict, we use short to get access to -1...
	// Setting min size out of paranoia
	int batches_max_size = MAX(256, sizeof(short) * numgltextures);
	memset(entity_batches, -1, batches_max_size);
	memset(trans_entity_batches, -1, batches_max_size);

	// acceleration so we dont have to iterate all the textures,
	// when we encounter a new texture, add it to this list.
	// trans ents added from end and opaque ents added from front.
	short textures_used[MAX_VISEDICTS];
	int num_batches = 0;
	int num_trans_batches = 0;
	
	partial_zombies_drawn = 0;
	char specChar;

	int anim = (int)(cl.time*10) & 3;

   	// Batch alias and spr entities by their texture index
	for (int i = 0; i < cl_numvisedicts; i++)
	{
		currententity = cl_visedicts[i];

		if (currententity == &cl_entities[cl.viewentity])
			currententity->angles[0] *= 0.3;

		if(!(currententity->model))
		{
			R_DrawNullModel();
			continue;
		}
		
		
		specChar = currententity->model->name[strlen(currententity->model->name)-5];
		
		// zombie limb special characters, draw them with the body together
		if(specChar == '(' || specChar == '^')
			continue;

		short textureindex = 0;
		short prev_visedict = 0;
		
		currententity->next_visedict = -1;

		switch (currententity->model->type)
		{
		case mod_alias:
			{
				aliashdr_t * paliashdr = (aliashdr_t *)Mod_Extradata (currententity->model);
				textureindex = paliashdr->gl_texturenum[currententity->skinnum][anim];
				if(specChar == '$') { // alpha blended
					prev_visedict = trans_entity_batches[textureindex];
					if (prev_visedict < 0) {
						textures_used[MAX_VISEDICTS - 1 - num_trans_batches] = textureindex;
						num_trans_batches += 1;
					} 
					currententity->next_visedict = prev_visedict;
					trans_entity_batches[textureindex] = i;
					break;
				} else { // opaque
					prev_visedict = entity_batches[textureindex];
					if (prev_visedict < 0) {
						textures_used[num_batches] = textureindex;
						num_batches += 1;
					} 
					currententity->next_visedict = prev_visedict;
					entity_batches[textureindex] = i;
				}
			}
			break;
		
		case mod_sprite:
			{
				mspriteframe_t* spriteframe = R_GetSpriteFrame(currententity);
				textureindex = spriteframe->gl_texturenum;
				prev_visedict = trans_entity_batches[textureindex];
				if (prev_visedict < 0) {
					textures_used[MAX_VISEDICTS - 1 - num_trans_batches] = textureindex;
					num_trans_batches += 1;
				} 
				currententity->next_visedict = prev_visedict;
				trans_entity_batches[textureindex] = i;
			}
			break;

		case mod_brush:
			R_DrawBrushModel (currententity);
			break;

		default:
			break;
		}
	}

	// iterate and render opaque mdls
	// TODO: go through model drawing to see if there's things we can move to here to reduce repetition
	for (int i = 0; i < num_batches; i++) {
		short textureindex = textures_used[i];
		short index = entity_batches[textureindex];
		while (index >= 0) {
			currententity = cl_visedicts[index];
			R_DrawAliasModel(currententity);
			index = currententity->next_visedict;
		}
	}

	// iterate and render transparent mdls and all sprites
	for (int i = 0; i < num_trans_batches; i++) {
		short textureindex = textures_used[MAX_VISEDICTS - 1 - i];
		short index = trans_entity_batches[textureindex];
		while (index >= 0) {
			currententity = cl_visedicts[index];
			if (currententity->model->type == mod_alias) {
				R_DrawTransparentAliasModel(currententity);
			}
			else
				R_DrawSpriteModel(currententity);
			index = currententity->next_visedict;
		}
	}
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
    // fenix@io.com: model transform interpolation
    float old_i_model_transform;


	if (!r_drawviewmodel.value)
		return;

	if (chase_active.value)
		return;

	if (envmap)
		return;

	if (!r_drawentities.value)
		return;

	/*if (cl.items & IT_INVISIBILITY)
		return;*/

	if (cl.stats[STAT_HEALTH] < 0)
		return;

	currententity = &cl.viewent;

	if (!currententity->model)
		return;

	// Tomaz - QC Alpha Scale Begin
	currententity->renderamt  = cl_entities[cl.viewentity].renderamt;
    currententity->rendermode = cl_entities[cl.viewentity].rendermode;
    currententity->rendercolor[0] = cl_entities[cl.viewentity].rendercolor[0];
    currententity->rendercolor[1] = cl_entities[cl.viewentity].rendercolor[1];
    currententity->rendercolor[2] = cl_entities[cl.viewentity].rendercolor[2];
    // Tomaz - QC Alpha Scale End


/*
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
*/
/*
	ambient[0] = ambient[1] = ambient[2] = ambient[3] = (float)ambientlight / 128;
	diffuse[0] = diffuse[1] = diffuse[2] = diffuse[3] = (float)shadelight / 128;
*/

	// hack the depth range to prevent view model from poking into walls
	sceGuDepthRange(0, 19660);

	//Blubs' tests for vmodel clipping
		//sceGuDisable(GU_CLIP_PLANES);
		//sceGuDisable(GU_DEPTH_TEST);
		//sceGuDisable(GU_CULL_FACE);
		//sceGuDisable(GU_SCISSOR_TEST);
	switch (currententity->model->type)
	{
		case mod_alias:
			// fenix@io.com: model transform interpolation
		old_i_model_transform = r_i_model_transform.value;
		r_i_model_transform.value = false;
		R_DrawAliasModel (currententity);
		r_i_model_transform.value = old_i_model_transform;
			break;
		default:
			Con_Printf("Not drawing view model of type %i\n", currententity->model->type);
			break;
	}
		//sceGuEnable(GU_SCISSOR_TEST);
		//sceGuEnable(GU_DEPTH_TEST);
		//sceGuEnable(GU_CLIP_PLANES);
		//sceGuEnable(GU_CULL_FACE);
	sceGuDepthRange(0, 65535);


	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
	sceGuDisable(GU_BLEND);
}

/*
=============
R_DrawView2Model
=============
*/
void R_DrawView2Model (void)
{
    float old_i_model_transform;


	if (!r_drawviewmodel.value)
		return;

	if (chase_active.value)
		return;

	if (envmap)
		return;

	if (!r_drawentities.value)
		return;

	if (cl.stats[STAT_HEALTH] < 0)
		return;

	currententity = &cl.viewent2;

	if (!currententity->model)
		return;

	// Tomaz - QC Alpha Scale Begin
	currententity->renderamt  = cl_entities[cl.viewentity].renderamt;
    currententity->rendermode = cl_entities[cl.viewentity].rendermode;
    currententity->rendercolor[0] = cl_entities[cl.viewentity].rendercolor[0];
    currententity->rendercolor[1] = cl_entities[cl.viewentity].rendercolor[1];
    currententity->rendercolor[2] = cl_entities[cl.viewentity].rendercolor[2];

	// hack the depth range to prevent view model from poking into walls
	sceGuDepthRange(0, 19660);
    switch (currententity->model->type)
	{
	case mod_alias:
		// fenix@io.com: model transform interpolation
        old_i_model_transform = r_i_model_transform.value;
        r_i_model_transform.value = false;
        R_DrawAliasModel (currententity);
        r_i_model_transform.value = old_i_model_transform;
		break;
	default:
		Con_Printf("Not drawing view model of type %i\n", currententity->model->type);
		break;
	}
	sceGuDepthRange(0, 65535);

	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    sceGuDisable(GU_BLEND);
}


/*
============
R_PolyBlend
============
*/
void R_PolyBlend (void)
{
	if (!r_polyblend.value)
		 return;

	if (!v_blend[3])
		return;

	sceGuEnable  (GU_BLEND);
	sceGuDisable (GU_TEXTURE_2D);
	sceGuTexFunc (GU_TFX_MODULATE, GU_TCC_RGBA);

    sceGumLoadIdentity();

	sceGuColor(GU_COLOR(v_blend[0], v_blend[1], v_blend[2], v_blend[3]));
    struct vertex
	{
	   short x, y, z;
	};
	vertex* const out = static_cast<vertex*>(sceGuGetMemory(sizeof(vertex) * 2));

	out[0].x = 0;
	out[0].y = 0;
	out[0].z = 0;

	out[1].x = 480;
	out[1].y = 272;
	out[1].z = 0;

	sceGuDrawArray(GU_SPRITES, GU_VERTEX_16BIT | GU_TRANSFORM_2D, 2, 0, out);
	sceGuColor(0xffffffff);

	sceGuDisable (GU_BLEND);
	sceGuEnable  (GU_TEXTURE_2D);
	sceGuTexFunc (GU_TFX_REPLACE, GU_TCC_RGBA);
}


static int SignbitsForPlane (mplane_t *out)
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


/*
===============
R_RenderScene

r_refdef must be set before the first call
===============
*/
void R_RenderScene (void)
{
	int 		i;
	float vecx_point_transform = 90 - r_refdef.fov_x * 0.5;
	float vecy_point_transform = 90 - r_refdef.fov_y * 0.5;
	float screenaspect;
	extern int glwidth, glheight;
	int x, x2, y2, y, w, h;
	float fovx, fovy; //johnfitz
	int contents; //johnfitz

	if (cl.maxclients > 1)
		Cvar_Set ("r_fullbright", "0");

	//xaa - opt
	int w_ratio = glwidth/vid.width;
	int h_ratio = glheight/vid.height;
	vrect_t* renderrect = &r_refdef.vrect;

	//setupframe
	Fog_SetupFrame(false);
	R_AnimateLight();
	++r_framecount;

	VectorCopy (r_refdef.vieworg, r_origin);
	AngleVectors (r_refdef.viewangles, vpn, vright, vup);

	// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_viewleaf = Mod_PointInLeaf (r_origin, cl.worldmodel);

	V_SetContentsColor (r_viewleaf->contents);
	V_CalcBlend ();

	c_brush_polys = 0;
	c_alias_polys = 0;
    c_md3_polys = 0;

	//setfrustum
	// disabled as well

	// near plane
	VectorCopy(vpn, frustum[0].normal);
	// sides
	RotatePointAroundVector(frustum[1].normal, vup, vpn, -vecx_point_transform);
	RotatePointAroundVector(frustum[2].normal, vup, vpn, vecx_point_transform);
	// up/down
	RotatePointAroundVector(frustum[3].normal, vright, vpn, vecy_point_transform);
	RotatePointAroundVector(frustum[4].normal, vright, vpn, -vecy_point_transform);

	for (i=0 ; i<5 ; i++) {
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}

//setupgl
	// set up viewpoint
	sceGumMatrixMode(GU_PROJECTION);
	sceGumLoadIdentity();

	x = renderrect->x * w_ratio;
	x2 = (renderrect->x + renderrect->width) * w_ratio;
	y = (vid.height-renderrect->y) * h_ratio;
	y2 = (vid.height - (renderrect->y + renderrect->height)) * h_ratio;

	// fudge around because of frac screen scale
	if (x > 0)				x--;
	if (x2 < glwidth)		x2++;
	if (y2 < 0)				y2--;
	if (y < glheight)		y++;

	w = x2 - x;
	h = y - y2;

	if (envmap)
	{
		x = y2 = 0;
		w = h = 256;
	}

	sceGuViewport(
		glx,
		gly + (glheight >> 1) - y2 - (h >> 1), //xaa - try to skip some divides (/2) here
		w,
		h);
	sceGuScissor(x, glheight - y2 - h, w, h);

    screenaspect = (float)renderrect->width/renderrect->height;

	 //johnfitz -- warp view for underwater
	fovx = screenaspect;
	fovy = r_refdef.fov_y;
	if (r_waterwarp.value)
	{
		contents = Mod_PointInLeaf (r_origin, cl.worldmodel)->contents;
		if (contents == CONTENTS_WATER ||
			contents == CONTENTS_SLIME ||
			contents == CONTENTS_LAVA)
		{
			//variance should be a percentage of width, where width = 2 * tan(fov / 2)
			//otherwise the effect is too dramatic at high FOV and too subtle at low FOV
			//what a mess!
			//fovx = atan(tan(DEG2RAD(r_refdef.fov_x) / 2) * (0.97 + sin(cl.time * 1) * 0.04)) * 2 / M_PI_DIV_180;
			#ifdef PSP_VFPU
			fovy = RAD2DEG(vfpu_atanf(vfpu_tanf(DEG2RAD(r_refdef.fov_y) / 2) * (1.03 - vfpu_sinf(cl.time * 2) * 0.04)));
			#else
			fovy = RAD2DEG(atan(tan(DEG2RAD(r_refdef.fov_y) / 2) * (1.03 - sin(cl.time * 2) * 0.04)));
			#endif
		}
	}

	float farplanedist = (r_refdef.fog_start == 0 || r_refdef.fog_end < 0) ? 4096 : (r_refdef.fog_end + 16); 
	sceGumPerspective(fovy, fovx, 4, farplanedist);

	if (mirror)
	{
		if (mirror_plane->normal[2])
		{
			const ScePspFVector3 scaling = {1, -1, 1};
	        sceGumScale(&scaling);
		}
		else
		{
			const ScePspFVector3 scaling = {-1, 1, 1};
	        sceGumScale(&scaling);
		}
	}

	sceGumUpdateMatrix();
	sceGumMatrixMode(GU_VIEW);
	sceGumLoadIdentity();

	sceGumRotateX(-90 * piconst);
	sceGumRotateZ(90 * piconst);
	sceGumRotateX(-r_refdef.viewangles[2] * piconst);
	sceGumRotateY(-r_refdef.viewangles[0] * piconst);
	sceGumRotateZ(-r_refdef.viewangles[1] * piconst);

    /*glTranslatef (-r_refdef.vieworg[0],  -r_refdef.vieworg[1],  -r_refdef.vieworg[2]);*/
	const ScePspFVector3 translation = {
		-r_refdef.vieworg[0],
		-r_refdef.vieworg[1],
		-r_refdef.vieworg[2]
	};
	sceGumTranslate(&translation);

	/*glGetFloatv (GL_MODELVIEW_MATRIX, r_world_matrix);*/
	sceGumStoreMatrix(&r_world_matrix);
	sceGumUpdateMatrix();

	sceGumMatrixMode(GU_MODEL);


	// the value 2.2 is a bit of a sweet spot found by trial and error
	// the bigger the number, the more polys skip clipping entirely and less polys also rejected
	// the smaller the number, the more polys get clipped and by extension more polys also rejected
	// at a sweet spot we want to maximize rejected polys but minimize (clipped - rejected) amount at the same time
	// it's a balancing act and every change has to be benchmarked. between 2.5, 2.2 and 2.0, 2.2 was the fastest.
	clipping::begin_frame(fovy, fmin(160.0f, fovy * 2.2f), fovx);

	sceGumMatrixMode(GU_PROJECTION);
	sceGumLoadIdentity();
	sceGumPerspective(fovy, fovx, 4, farplanedist);
	sceGumUpdateMatrix();
	sceGumMatrixMode(GU_MODEL);

	// set drawing parms
	sceGuDisable(GU_BLEND);
	sceGuDisable(GU_ALPHA_TEST);
	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);

	R_MarkLeaves ();	// done here so we know if we're in water
	Fog_EnableGFog (); //johnfitz
	R_DrawWorld ();		// adds static entities to the list
	S_ExtraUpdate ();	// don't let sound get messed up if going slow
	
	R_DrawEntitiesOnList();
	
	R_DrawDecals();
	R_DrawParticles ();
	Fog_DisableGFog (); //johnfitz
}


/*
=============
R_Clear
=============
*/
extern char	skybox_name[32];
void R_Clear (void)
{
    if(r_refdef.fog_end > 0 && r_skyfog.value)
    {
        sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
        sceGuClearColor(GU_COLOR(r_refdef.fog_red * 0.01f,r_refdef.fog_green * 0.01f,r_refdef.fog_blue * 0.01f,1.0f));
    }
    else
	{
		if (!skybox_name[0]) {
			byte *sky_color = StringToRGB (r_skycolor.string);
			sceGuClearColor(GU_RGBA(sky_color[0], sky_color[1], sky_color[2], 255));
		}

		sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
	}

	if(r_shadows.value)
	{
	  sceGuClearStencil(GU_TRUE);
	  sceGuClear(GU_STENCIL_BUFFER_BIT);
	}
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

	r_base_world_matrix = r_world_matrix;

	d = DotProduct (r_refdef.vieworg, mirror_plane->normal) - mirror_plane->dist;
	VectorMA (r_refdef.vieworg, -2*d, mirror_plane->normal, r_refdef.vieworg);

	d = DotProduct (vpn, mirror_plane->normal);
	VectorMA (vpn, -2*d, mirror_plane->normal, vpn);

	r_refdef.viewangles[0] = -asinf(vpn[2])/M_PI*180;
	r_refdef.viewangles[1] = atan2f(vpn[1], vpn[0])/M_PI*180;
	r_refdef.viewangles[2] = -r_refdef.viewangles[2];

	ent = &cl_entities[cl.viewentity];
	if (cl_numvisedicts < MAX_VISEDICTS)
	{
		cl_visedicts[cl_numvisedicts] = ent;
		cl_numvisedicts++;
	}

	R_RenderScene ();
	R_DrawWaterSurfaces ();

	// blend on top
	sceGuEnable (GU_BLEND);

    sceGumMatrixMode(GU_PROJECTION);
	sceGumLoadIdentity();

	if (mirror_plane->normal[2])
	{
	    const ScePspFVector3 scaling = {1, -1, 1};
		sceGumScale(&scaling);
  	}
	else
	{
		const ScePspFVector3 scaling = {-1, 1, 1};
		sceGumScale(&scaling);
	}

    //sceGuFrontFace(GU_CW);

    sceGumMatrixMode(GU_VIEW);
	sceGumLoadIdentity();

	sceGumMatrixMode(GU_MODEL);

	sceGumStoreMatrix(&r_base_world_matrix);
	sceGumUpdateMatrix();

	sceGuColor(GU_COLOR(1,1,1,r_mirroralpha.value));

	s = cl.worldmodel->textures[mirrortexturenum]->texturechain;
	for ( ; s ; s=s->texturechain)
		R_RenderBrushPoly (s);
	cl.worldmodel->textures[mirrortexturenum]->texturechain = NULL;

	sceGuDisable (GU_BLEND);
	sceGuColor (0xffffffff);
}

/*
================
R_RenderView

r_refdef must be set before the first call
================
*/
void R_RenderView (void)
{
	c_brush_polys = 0;
	c_alias_polys = 0;

	mirror = false;

	R_Clear ();

	// render normal view
	R_RenderScene ();

	R_DrawViewModel ();
	R_DrawView2Model ();
	R_DrawWaterSurfaces ();

	// render mirror view
	R_Mirror ();
	R_PolyBlend ();

    //Crow_bar fixed
	if (r_speeds.value)
	{
		Con_Printf ("%4i world poly\n",  c_brush_polys);
		Con_Printf ("%4i entity poly\n",  c_alias_polys);
	}
}

// MARK: Vril Graphics Wrapper

void Platform_Graphics_SetTextureMode(int texture_mode)
{
	switch(texture_mode) {
		case GFX_REPLACE:
			sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
			break;
		default:
			Sys_Error("Received unknown texture mode [%d]\n", texture_mode);
			break;
	}
}

void Platform_Graphics_Color(float red, float green, float blue, float alpha)
{
	sceGuColor(GU_COLOR(red, green, blue, alpha));
}