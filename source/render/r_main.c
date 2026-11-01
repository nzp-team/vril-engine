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
// r_main.c -- Shared scene renderer


#include "../nzportable_def.h"
#include "r_config.h"

entity_t	r_worldentity;
float r_world_matrix[16] __attribute__((aligned(16)));
static float r_base_world_matrix[16] __attribute__((aligned(16)));
static float r_projection_fovy, r_projection_aspect, r_projection_far;
static float r_depthmin, r_depthmax;
cvar_t r_profile = {"r_profile", "0"};
cvar_t r_entity_sort = {"r_entity_sort", "1"};
cvar_t r_sprite_batch = {"r_sprite_batch", "1"};

qboolean	r_cache_thrash;

vec3_t		modelorg, r_entorigin;
entity_t	*currententity;

int			r_visframecount;
int			r_framecount;
int			r_texture_frame;

mplane_t	frustum[5];

int			c_brush_polys, c_alias_polys;

qboolean	envmap;

int			cnttextures[2] = {-1, -1};

int			particletexture;

int			mirrortexturenum;
qboolean	mirror;
mplane_t	*mirror_plane;

vec3_t	vup;
vec3_t	vpn;
vec3_t	vright;
vec3_t	r_origin;

refdef_t	r_refdef;

mleaf_t		*r_viewleaf, *r_oldviewleaf;

texture_t	*r_notexture_mip;

int		d_lightstylevalue[256];

cvar_t	r_norefresh = {"r_norefresh","0"};
cvar_t	r_drawentities = {"r_drawentities","1"};
cvar_t	r_drawviewmodel = {"r_drawviewmodel","1"};
cvar_t	r_speeds = {"r_speeds","0"};
cvar_t	r_fullbright = {"r_fullbright","0"};
cvar_t	r_lightmap = {"r_lightmap","0"};
cvar_t	r_shadows = {"r_shadows","0"};

cvar_t	r_dynamic = {"r_dynamic","1"};
cvar_t	r_novis = {"r_novis","0"};

cvar_t	r_skycolor = {"r_skycolor", "64 64 70", true};

cvar_t	gl_finish = {"gl_finish","0"};

cvar_t	gl_keeptjunctions = {"gl_keeptjunctions","0"};

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

cvar_t  r_model_brightness  = { "r_model_brightness", "1",true};

cvar_t	r_farclip	        = {"r_farclip",              "4096"};

cvar_t	r_mirroralpha = {"r_mirroralpha","1",true};
cvar_t	r_wateralpha = {"r_wateralpha",R_DEFAULT_WATERALPHA,true};
cvar_t 	r_skyfog = {"r_skyfog", "1",true};
cvar_t	gl_clear = {"gl_clear","0"};
cvar_t	gl_cull = {"gl_cull","1"};
cvar_t	gl_smoothmodels = {"gl_smoothmodels","1"};
cvar_t	gl_affinemodels = {"gl_affinemodels","0"};
cvar_t	gl_polyblend = {"gl_polyblend","1"};
cvar_t	gl_flashblend = {"gl_flashblend","1"};
cvar_t	gl_playermip = {"gl_playermip","0"};
cvar_t	gl_nocolors = {"gl_nocolors","0"};
cvar_t	gl_reporttjunctions = {"gl_reporttjunctions","0"};
cvar_t	gl_doubleeyes = {"gl_doubleeys", "1"};
cvar_t	r_flatlightstyles = {"r_flatlightstyles", "0", false};
cvar_t	r_retro   	        = {"r_retro",			"1",true};
cvar_t	r_dithering 		= {"r_dithering","1",true};
cvar_t	r_partalpha           = {"r_partalpha",             "0.8",true};
cvar_t	r_vsync               = {"r_vsync", R_DEFAULT_VSYNC,true};
cvar_t  cl_loadmapcfg         = {"cl_loadmapcfg",             "0",true};
cvar_t	r_restexf             = {"r_restexf",                 "0",true};
cvar_t  r_texcompr            = {"r_texcompr",                "5",true};
cvar_t  r_maxrange            = {"r_maxrange",             "4096"};
cvar_t	r_caustics            = {"r_caustics",                "1",true};
cvar_t	r_detail              = {"r_detail",                  "1",true};
cvar_t	r_detail_mipmaps      = {"r_detail_mipmaps",          "1",true};
cvar_t	r_detail_mipmaps_func = {"r_detail_mipmaps_func",     "2",true};
cvar_t	r_detail_mipmaps_bias = {"r_detail_mipmaps_bias",    "-6",true};
cvar_t  r_asynch              = {"r_asynch",                  "0"};
cvar_t	r_mipmaps             = {"r_mipmaps",                 "1",true};
cvar_t  r_mipmaps_func        = {"r_mipmaps_func",            "2",true};
cvar_t  r_mipmaps_bias        = {"r_mipmaps_bias",           "-7",true};
cvar_t	r_tex_scale_down      = {"r_tex_scale_down",          "1",true};
cvar_t	r_particles_simple    = {"r_particles_simple",        "0",true};
cvar_t	r_waterripple         = {"r_waterripple",             "2",true};
cvar_t	r_waterwarp           = {"r_waterwarp",               "1",true};
cvar_t  r_showbboxes          = {"r_showbboxes",              "0"};
cvar_t  r_showbboxes_full     = {"r_showbboxes_full",         "0",true};
cvar_t	r_showtris            = {"r_showtris",                "0"};
cvar_t	r_showtris_full       = {"r_showtris_full",           "0",true};
cvar_t	r_polyblend	          = {"r_polyblend",               "1",true};

cvar_t gl_ztrick = {"gl_ztrick", "0"};
extern 	cvar_t 	scr_fov_viewmodel;

cvar_t r_i_model_animation = {"r_i_model_animation", "1", true};
cvar_t r_i_model_transform = {"r_i_model_transform", "1", true};

void R_InitCvars(void)
{
    static qboolean initialized;
    if (initialized)
        return;
    initialized = true;
    Cvar_RegisterVariable(&r_i_model_animation);
    Cvar_RegisterVariable(&r_i_model_transform);
    Cvar_RegisterVariable(&r_profile);
    Cvar_RegisterVariable(&r_entity_sort);
    Cvar_RegisterVariable(&r_sprite_batch);
    Cvar_RegisterVariable(&r_norefresh);
    Cvar_RegisterVariable(&r_lightmap);
    Cvar_RegisterVariable(&r_fullbright);
    Cvar_RegisterVariable(&r_drawentities);
    Cvar_RegisterVariable(&r_drawviewmodel);
    Cvar_RegisterVariable(&r_shadows);
    Cvar_RegisterVariable(&r_mirroralpha);
    Cvar_RegisterVariable(&r_wateralpha);
    Cvar_RegisterVariable(&r_dynamic);
    Cvar_RegisterVariable(&r_lightmap_fastalloc);
    Cvar_RegisterVariable(&r_novis);
    Cvar_RegisterVariable(&r_speeds);
    Cvar_RegisterVariable(&r_farclip);
    Cvar_RegisterVariable(&gl_finish);
    Cvar_RegisterVariable(&gl_clear);
    Cvar_RegisterVariable(&gl_ztrick);
    Cvar_RegisterVariable(&gl_cull);
    Cvar_RegisterVariable(&gl_smoothmodels);
    Cvar_RegisterVariable(&gl_affinemodels);
    Cvar_RegisterVariable(&gl_polyblend);
    Cvar_RegisterVariable(&gl_flashblend);
    Cvar_RegisterVariable(&gl_playermip);
    Cvar_RegisterVariable(&gl_nocolors);
    Cvar_RegisterVariable(&gl_keeptjunctions);
    Cvar_RegisterVariable(&gl_reporttjunctions);
    Cvar_RegisterVariable(&gl_doubleeyes);
    Cvar_RegisterVariable(&r_explosiontype);
    Cvar_RegisterVariable(&r_laserpoint);
    Cvar_RegisterVariable(&r_part_explosions);
    Cvar_RegisterVariable(&r_part_trails);
    Cvar_RegisterVariable(&r_part_sparks);
    Cvar_RegisterVariable(&r_part_gunshots);
    Cvar_RegisterVariable(&r_part_blood);
    Cvar_RegisterVariable(&r_part_telesplash);
    Cvar_RegisterVariable(&r_part_blobs);
    Cvar_RegisterVariable(&r_part_lavasplash);
    Cvar_RegisterVariable(&r_part_flames);
    Cvar_RegisterVariable(&r_part_lightning);
    Cvar_RegisterVariable(&r_part_flies);
    Cvar_RegisterVariable(&r_part_muzzleflash);
    Cvar_RegisterVariable(&r_model_brightness);
    Cvar_RegisterVariable(&r_skyfog);
    Cvar_RegisterVariable(&r_skycolor);
    Cvar_RegisterVariable(&r_flatlightstyles);
    Cvar_RegisterVariable(&r_retro);
    Cvar_RegisterVariable(&r_dithering);
    Cvar_RegisterVariable(&r_hlbsponly);
    Cvar_RegisterVariable(&r_partalpha);
    Cvar_RegisterVariable(&r_restexf);
    Cvar_RegisterVariable(&r_texcompr);
    Cvar_RegisterVariable(&r_caustics);
    Cvar_RegisterVariable(&r_detail);
    Cvar_RegisterVariable(&r_detail_mipmaps);
    Cvar_RegisterVariable(&r_detail_mipmaps_func);
    Cvar_RegisterVariable(&r_detail_mipmaps_bias);
    Cvar_RegisterVariable(&r_waterripple);
    Cvar_RegisterVariable(&r_tex_scale_down);
    Cvar_RegisterVariable(&r_particles_simple);
    Cvar_RegisterVariable(&r_vsync);
    Cvar_RegisterVariable(&r_mipmaps);
    Cvar_RegisterVariable(&r_mipmaps_func);
    Cvar_RegisterVariable(&r_mipmaps_bias);
    Cvar_RegisterVariable(&r_maxrange);
    Cvar_RegisterVariable(&r_asynch);
    Cvar_RegisterVariable(&r_waterwarp);
    Cvar_RegisterVariable(&r_showbboxes);
    Cvar_RegisterVariable(&r_showbboxes_full);
    Cvar_RegisterVariable(&r_polyblend);
    Cvar_RegisterVariable(&r_showtris);
    Cvar_RegisterVariable(&r_showtris_full);
    Cvar_RegisterVariable(&cl_loadmapcfg);
    Cvar_RegisterVariable(&r_part_spikes);
    Cvar_SetValue("gl_clear", 1);
}

qboolean R_CullBox (vec3_t mins, vec3_t maxs)
{
	int		i;

	for (i=0 ; i<4 ; i++)
		if (BoxOnPlaneSide (mins, maxs, &frustum[i]) == 2)
			return true;
	return false;
}

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

int SignbitsForPlane (mplane_t *out)
{
	int	bits, j;

	bits = 0;
	for (j=0 ; j<3 ; j++)
	{
		if (out->normal[j] < 0)
			bits |= 1<<j;
	}
	return bits;
}

void R_DrawAliasGeometry(aliashdr_t *header, const trivertx_t *first, const trivertx_t *second, float blend)
{
    const int *commands = (const int *)((const byte *)header + header->commands);
    const unsigned short *remap = header->command_remap ?
        (const unsigned short *)((const byte *)header + header->command_remap) : NULL;
    hyena_morph_mesh_t mesh;
    if (header->command_words) {
        R_DrawAliasCommands(commands + 1, first, second, blend, header->numposes <= 1, header->command_words);
        return;
    }
    mesh.key = header->model;
    mesh.commands = commands;
    mesh.remap = remap;
    mesh.poses = (const byte *)header + header->posedata;
    mesh.vertices = header->poseverts;
    mesh.command_vertices = header->command_vertices;
    mesh.poses_count = header->numposes;
    if (!Hyena_DrawMorphMesh(&mesh, (const byte *)first, (const byte *)second, blend))
        R_DrawAliasMesh(header->model, commands, remap, header->poseverts, first, second, blend);
}

void R_DrawAliasShadowGeometry(aliashdr_t *header, const trivertx_t *pose, const float *direction, float height)
{
    const int *commands = (const int *)((const byte *)header + header->commands);
    const unsigned short *remap = header->command_remap ?
        (const unsigned short *)((const byte *)header + header->command_remap) : NULL;
    if (!header->command_words)
        R_DrawAliasShadowMesh(header->model, commands, remap, header->poseverts, pose,
            header->scale, header->scale_origin, direction, height);
}

void R_SetFrustum (void)
{
	int		i;

	float sx = sinf(r_refdef.fov_x * (float)M_PI / 360.0f);
	float cx = cosf(r_refdef.fov_x * (float)M_PI / 360.0f);
	float sy = sinf(r_refdef.fov_y * (float)M_PI / 360.0f);
	float cy = cosf(r_refdef.fov_y * (float)M_PI / 360.0f);

	for (i = 0; i < 3; i++) {
		frustum[0].normal[i] = vpn[i] * sx + vright[i] * cx;
		frustum[1].normal[i] = vpn[i] * sx - vright[i] * cx;
		frustum[2].normal[i] = vpn[i] * sy + vup[i] * cy;
		frustum[3].normal[i] = vpn[i] * sy - vup[i] * cy;
	}

	VectorCopy(vpn, frustum[4].normal);
	for (i=0 ; i<5 ; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}
}

void R_SetupFrame (void)
{

	if (cl.maxclients > 1)
		Cvar_Set ("r_fullbright", "0");

	Fog_SetupFrame(false);
	R_AnimateLight ();

	r_framecount++;
	r_texture_frame = (int)(cl.time * 10);

	VectorCopy (r_refdef.vieworg, r_origin);

	AngleVectors (r_refdef.viewangles, vpn, vright, vup);

	r_oldviewleaf = r_viewleaf;
	r_viewleaf = Mod_PointInLeaf (r_origin, cl.worldmodel);

	V_SetContentsColor (r_viewleaf->contents);
	V_CalcBlend ();

	r_cache_thrash = false;

	c_brush_polys = 0;
	c_alias_polys = 0;

}

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

void IgnoreInterpolatioFrame (entity_t *e, aliashdr_t *paliashdr)
{
	if (e->model != NULL && strcmp(e->old_model, e->model->name))
	{
		int frame = e->frame;
		if (frame < 0 || frame >= paliashdr->numframes)
			frame = 0;
		strcpy(e->old_model, e->model->name);

        e->frame_start_time     = 0;
        e->translate_start_time = 0;
        e->rotate_start_time    = 0;
        e->pose1    = paliashdr->frames[frame].firstpose;
        e->pose2    = e->pose1;
	}
}


typedef struct {
    qboolean transparent, fullbright, alpha_test, smooth, affine;
    float mode, amount;
} render_alias_state_t;

static void R_BeginAlias(const render_alias_state_t *state)
{
    Hyena_SetShadeMode(state->smooth ? HYE_SMOOTH : HYE_FLAT);
    Hyena_SetTextureMode(HYE_MODULATE);
    Hyena_PerspectiveCorrection(!state->affine);
    if (state->mode == TEX_ADDITIVE && state->amount > 0 && state->amount <= 1) {
        Hyena_EnableCapability(HYE_BLEND);
        Hyena_BlendConstant(state->amount);
        if (state->amount <= 0.7f)
            Hyena_DepthMask(false);
    } else if (state->mode == TEX_GLOW && state->amount > 0 && state->amount <= 1) {
        Hyena_EnableCapability(HYE_BLEND);
        Hyena_SetBlendFunction(HYE_SRC_ALPHA, HYE_ONE);
        Hyena_DepthMask(false);
    } else if (state->transparent) {
        Hyena_EnableCapability(HYE_BLEND);
        Hyena_SetBlendFunction(HYE_SRC_ALPHA, HYE_ONE_MINUS_SRC_ALPHA);
        Hyena_DepthMask(false);
    }
    if (state->alpha_test || state->mode == TEX_SOLID || state->transparent) {
        Hyena_EnableCapability(HYE_ALPHA_TEST);
        Hyena_AlphaFunction(HYE_GREATER, state->transparent ? 0 : 170.0f / 255);
    }
}

static void R_EndAlias(void)
{
    Hyena_AlphaFunction(HYE_GREATER, 0);
    Hyena_DisableCapability(HYE_ALPHA_TEST);
    Hyena_SetTextureMode(HYE_REPLACE);
    Hyena_SetShadeMode(HYE_FLAT);
    Hyena_PerspectiveCorrection(true);
    Hyena_DepthMask(true);
    Hyena_SetBlendFunction(HYE_SRC_ALPHA, HYE_ONE_MINUS_SRC_ALPHA);
    Hyena_DisableCapability(HYE_BLEND);
}

static void R_DrawAliasPose(aliashdr_t *header, int first, int second, float blend)
{
    const trivertx_t *pose1, *pose2;
    if (second < 0 || second >= header->numposes)
        second = 0;
    if (first < 0 || first >= header->numposes)
        first = second;
    lastposenum0 = first;
    lastposenum = second;
    pose1 = R_AliasPose(header, first);
    pose2 = R_AliasPose(header, second);
    Hyena_SetColor(lightcolor[0] / R_ALIAS_LIGHT_DIVISOR,
        lightcolor[1] / R_ALIAS_LIGHT_DIVISOR, lightcolor[2] / R_ALIAS_LIGHT_DIVISOR, 1);
    R_DrawAliasGeometry(header, pose1, pose1 == pose2 ? NULL : pose2, blend);
}

void R_SetupAliasFrame (int frame, aliashdr_t *paliashdr)
{
	int				pose, numposes;
	float			interval;

	if ((frame >= paliashdr->numframes) || (frame < 0))
	{

		frame = 0;
	}

	pose = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	if (numposes > 1)
	{
		interval = paliashdr->frames[frame].interval;
		pose += (int)((float)cl.time / interval) % numposes;
	}

	lastposenum = pose;
	R_DrawAliasPose(paliashdr, pose, pose, 0);
}

void R_SetupAliasBlendedFrame (int frame, aliashdr_t *paliashdr, entity_t* e)
{
	int   pose;
	int   numposes;
	float blend;

	if ((frame >= paliashdr->numframes) || (frame < 0))
	{

		frame = 0;
	}
	if (!r_i_model_animation.value || paliashdr->numposes <= 1) {
		R_SetupAliasFrame(frame, paliashdr);
		e->pose1 = e->pose2 = lastposenum;
		e->frame_start_time = realtime;
		return;
	}
	if (e->pose1 < 0 || e->pose1 >= paliashdr->numposes ||
		e->pose2 < 0 || e->pose2 >= paliashdr->numposes)
	{
		e->pose1 = e->pose2 = paliashdr->frames[frame].firstpose;
		e->frame_start_time = realtime;
	}

	float dist_x = (cl.viewent.origin[0] - e->origin[0]);
	float dist_y = (cl.viewent.origin[1] - e->origin[1]);
	float distance_from_client = dist_x * dist_x + dist_y * dist_y;

	if (distance_from_client >= scr_alias_lerp_distance_squared) {

		e->pose1 = e->pose2 = paliashdr->frames[frame].firstpose;
		e->frame_interval = 0.1;

		R_DrawAliasPose(paliashdr, paliashdr->frames[frame].firstpose, paliashdr->frames[frame].firstpose, 0);
	} else {
		pose = paliashdr->frames[frame].firstpose;
		numposes = paliashdr->frames[frame].numposes;

		if (numposes > 1)
		{
			e->frame_interval = paliashdr->frames[frame].interval;
			pose += (int)((float)cl.time / e->frame_interval) % numposes;
		}
		else
		{

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

		if (cl.paused || blend > 1) blend = 1;

		if (blend == 1)
			R_DrawAliasPose(paliashdr, pose, pose, 0);
		else
			R_DrawAliasPose(paliashdr, e->pose1, e->pose2, blend);
	}
}

float r_avertexnormals[162][3] = {
#include "../anorms.h"
};
float r_avertexnormal_dots[16][256] =
#include "../anorm_dots.h"
;
extern vec3_t lightspot;
vec3_t shadevector;
float ambientlight, shadelight, *shadedots = r_avertexnormal_dots[0];
int lastposenum, lastposenum0, doZHack;

qboolean R_CullAliasEntity(const entity_t *e)
{
    vec3_t mins, maxs;
    const model_t *model = e->model;
    float scale = e->scale && e->scale != ENTSCALE_DEFAULT ? ENTSCALE_DECODE(e->scale) : 1;
    int i;
    qboolean smoothing = r_i_model_transform.value && model->type == mod_alias &&
        e != &cl.viewent && e != &cl.viewent2 && e->translate_start_time && e->rotate_start_time;
    qboolean moving = smoothing && (!VectorCompare(e->origin1, e->origin2) || !VectorCompare(e->origin2, e->origin));
    qboolean rotating = smoothing && (!VectorCompare(e->angles1, e->angles) || !VectorCompare(e->angles2, e->angles));

    float radius = model->radius;
    if ((rotating || e->angles[0] || e->angles[2]) && radius <= 0) {
        vec3_t extent;
        for (i = 0; i < 3; i++)
            extent[i] = fmaxf(fabsf(model->mins[i]), fabsf(model->maxs[i]));
        radius = sqrtf(DotProduct(extent, extent));
    }
    scale = fabsf(scale);
    for (i = 0; i < 3; i++) {
        float minimum = model->mins[i], maximum = model->maxs[i];
        if (rotating || e->angles[0] || e->angles[2]) {
            minimum = -radius;
            maximum = radius;
        } else if (e->angles[1] && i != 2) {
            float radius = fmaxf(fabsf(model->mins[0]), fabsf(model->maxs[0])) +
                fmaxf(fabsf(model->mins[1]), fabsf(model->maxs[1]));
            minimum = -radius;
            maximum = radius;
        }
        mins[i] = e->origin[i] + minimum * scale;
        maxs[i] = e->origin[i] + maximum * scale;
        if (moving && fabsf(e->origin[i] - e->origin2[i]) <= 100) {
            mins[i] = fminf(mins[i], fminf(e->origin1[i], e->origin2[i]) + minimum * scale);
            maxs[i] = fmaxf(maxs[i], fmaxf(e->origin1[i], e->origin2[i]) + maximum * scale);
        }
    }
    return R_CullBox(mins, maxs);
}

int R_FrustumCheckSphere(vec3_t center, float radius)
{
    int i, intersections = 0;
    for (i = 0; i < 5; i++) {
        float distance = DotProduct(center, frustum[i].normal) - frustum[i].dist;
        if (distance <= -radius)
            return -1;
        if (distance < radius)
            intersections++;
    }
    return intersections;
}

qboolean R_CullSphere(vec3_t center, float radius)
{
    return R_FrustumCheckSphere(center, radius) < 0;
}


static render_sprite_vertex_t sprite_batch[192];
static int sprite_count, sprite_texture = -1, sprite_blend;

static model_t *full_models[2];

void R_ClearSceneCaches(void)
{
    memset(full_models, 0, sizeof(full_models));
    sprite_count = 0;
    sprite_texture = -1;
    R_ClearEntityLights();
}

aliashdr_t *R_AliasModel(entity_t *entity, qboolean full)
{
    model_t *model = entity->model;
    if (full) {
        int crawler = strlen(model->name) > 12 && model->name[12] == 'c';
        if (!full_models[crawler])
            full_models[crawler] = Mod_ForName(crawler ? "models/ai/zcfull.mdl" : "models/ai/zfull.mdl", true);
        model = full_models[crawler];
    }
    return (aliashdr_t *)Mod_Extradata(model);
}

static void R_FlushSpriteBatch(void)
{
    if (!sprite_count)
        return;
    Hyena_BindTexture(sprite_texture);
    Hyena_SetTextureMode(HYE_MODULATE);
    Fog_DisableGFog();
    Hyena_AlphaFunction(HYE_GREATER, 0);
    Hyena_EnableCapability(HYE_ALPHA_TEST);
    Hyena_EnableCapability(HYE_BLEND);
    Hyena_DepthMask(false);
    Hyena_SetBlendFunction(HYE_SRC_ALPHA, sprite_blend == 1 ? HYE_ONE : HYE_ONE_MINUS_SRC_ALPHA);
    hyena_arrays_t arrays = {0};
    arrays.position.data = sprite_batch[0].position;
    arrays.position.stride = sizeof(*sprite_batch);
    arrays.uv.data = sprite_batch[0].uv;
    arrays.uv.stride = sizeof(*sprite_batch);
    arrays.color.data = sprite_batch[0].color;
    arrays.color.stride = sizeof(*sprite_batch);
    arrays.position.type = arrays.uv.type = arrays.color.type = HYE_FLOAT;
    arrays.flags = HYE_COPY_VERTICES;
    arrays.count = sprite_count;
    if (sprite_blend == 2)
        Hyena_SetBlendFunction(HYE_ZERO, HYE_SRC_COLOR);
    Hyena_DrawArrays(HYE_TRIANGLES, &arrays, sprite_count);
    Hyena_DisableCapability(HYE_ALPHA_TEST);
    Hyena_DepthMask(true);
    Hyena_DisableCapability(HYE_BLEND);
    Hyena_SetTextureMode(HYE_REPLACE);
    Hyena_SetBlendFunction(HYE_SRC_ALPHA, HYE_ONE_MINUS_SRC_ALPHA);
    Hyena_SetColor(1, 1, 1, 1);
    Fog_EnableGFog();
    sprite_count = 0;
    sprite_texture = -1;
}

void R_DrawSpriteModel (entity_t *e)
{
	extern vec3_t lightcolor;
	static const int corners[6] = {0, 1, 2, 0, 2, 3};
	static const float uv[4][2] = {{0, 1}, {0, 0}, {1, 0}, {1, 1}};
	render_sprite_vertex_t quad[4] = {0};
	mspriteframe_t *frame = R_GetSpriteFrame(e);
	msprite_t *sprite = e->model->cache.data;
	vec3_t forward, oriented_right, oriented_up, mins, maxs;
	const float *up = vup, *right = vright;
	float heights[4], widths[4];
	float scale = ENTSCALE_DECODE(e->scale);
	int i, axis;

	if (frame->gl_texturenum < 0)
		return;
	if (scale == 0)
		scale = 1;
	switch (sprite->type) {
	case SPR_ORIENTED:
		AngleVectors(e->angles, forward, oriented_right, oriented_up);
		up = oriented_up;
		right = oriented_right;
		break;
	case SPR_VP_PARALLEL_UPRIGHT:
		VectorClear(oriented_up);
		oriented_up[2] = 1;
		up = oriented_up;
		break;
	case SPR_FACING_UPRIGHT:
		VectorSubtract(e->origin, r_origin, forward);
		forward[2] = 0;
		VectorNormalize(forward);
		oriented_right[0] = forward[1];
		oriented_right[1] = -forward[0];
		oriented_right[2] = 0;
		VectorClear(oriented_up);
		oriented_up[2] = 1;
		up = oriented_up;
		right = oriented_right;
		break;
	case SPR_VP_PARALLEL_ORIENTED: {
		float angle = e->angles[ROLL] * (float)M_PI / 180.0f;
		float sine = sinf(angle), cosine = cosf(angle);
		for (axis = 0; axis < 3; axis++) {
			oriented_right[axis] = vright[axis] * cosine + vup[axis] * sine;
			oriented_up[axis] = -vright[axis] * sine + vup[axis] * cosine;
		}
		up = oriented_up;
		right = oriented_right;
		break;
	}
	default:
		break;
	}
	heights[0] = heights[3] = frame->down * scale;
	heights[1] = heights[2] = frame->up * scale;
	widths[0] = widths[1] = frame->left * scale;
	widths[2] = widths[3] = frame->right * scale;
	for (i = 0; i < 4; i++) {
		for (axis = 0; axis < 3; axis++) {
			float value = e->origin[axis] + heights[i] * up[axis] + widths[i] * right[axis];
			quad[i].position[axis] = value;
			if (!i || value < mins[axis])
				mins[axis] = value;
			if (!i || value > maxs[axis])
				maxs[axis] = value;
		}
		quad[i].position[3] = 1;
		memcpy(quad[i].uv, uv[i], sizeof(uv[i]));
	}
	if (R_CullBox(mins, maxs))
		return;
	if (!(e->effects & EF_FULLBRIGHT))
		R_EntityLight(e->origin);
	for (i = 0; i < 4; i++) {
		for (axis = 0; axis < 3; axis++)
			quad[i].color[axis] = e->effects & EF_FULLBRIGHT ? 1 : lightcolor[axis] / 255.0f;
		quad[i].color[3] = 1;
	}
	int blend = sprite->beamlength == 10 ? 1 : sprite->beamlength == 20 ? 2 : 0;
	if (sprite_texture != frame->gl_texturenum || sprite_blend != blend || sprite_count + 6 > 192)
		R_FlushSpriteBatch();
	sprite_texture = frame->gl_texturenum;
	sprite_blend = blend;
	for (i = 0; i < 6; i++)
		sprite_batch[sprite_count++] = quad[corners[i]];
	if (!r_sprite_batch.value)
		R_FlushSpriteBatch();
}

typedef struct {
    int visedict, texture, full;
} render_entity_batch_t;

static int R_CompareEntities(const void *left, const void *right)
{
    const render_entity_batch_t *a = left, *b = right;
    if (a->texture != b->texture)
        return a->texture < b->texture ? -1 : 1;
    return a->visedict - b->visedict;
}
static void R_AliasTransform(entity_t *entity, aliashdr_t *header, qboolean limb)
{
    vec3_t origin, scale;
    float entity_scale = !entity->scale || entity->scale == ENTSCALE_DEFAULT ? 1 : ENTSCALE_DECODE(entity->scale);
    int axis;
    R_RotateForEntity(entity, 0, limb ? entity->scale : ENTSCALE_DEFAULT);
    VectorCopy(header->scale_origin, origin);
    VectorCopy(header->scale, scale);
    if (!limb) {
        if (!strcmp(entity->model->name, "progs/eyes.mdl") && gl_doubleeyes.value) {
            origin[2] -= 30;
            VectorScale(scale, 2, scale);
        } else if ((entity == &cl.viewent || entity == &cl.viewent2) && scr_render_viewmodel_fov) {
            float view_scale = SCR_GetViewmodelScale() * entity_scale;
            origin[0] *= view_scale;
            scale[0] *= view_scale;
        } else {
            VectorScale(origin, entity_scale, origin);
            VectorScale(scale, entity_scale, scale);
        }
    }
    for (axis = 0; axis < 3; ++axis)
        scale[axis] *= header->vertex_scale;
    Hyena_MatrixTranslate(origin[0], origin[1], origin[2]);
    Hyena_MatrixScale(scale[0], scale[1], scale[2]);
    Hyena_UpdateMatrices();
}

static void R_RenderAliasPose(entity_t *entity, aliashdr_t *header, qboolean limb,
    const render_alias_state_t *state)
{
    c_alias_polys += header->numtris;
    Hyena_PushMatrix();
    R_AliasTransform(entity, header, limb);
    R_BeginAlias(state);
    IgnoreInterpolatioFrame(entity, header);
    R_SetupAliasBlendedFrame(entity->frame, header, entity);
    R_EndAlias();
    Hyena_PopMatrix();
}

static void R_RenderAliasLimb(entity_t *entity, int number)
{
    entity_t *limb;
    aliashdr_t *header;
    render_alias_state_t state = {0};
    if (number <= 0 || number >= MAX_EDICTS)
        return;
    limb = &cl_entities[number];
    if (!limb->model)
        return;
    header = (aliashdr_t *)Mod_Extradata(limb->model);
    if (r_model_brightness.value) {
        lightcolor[0] += R_ALIAS_LIMB_BRIGHTNESS;
        lightcolor[1] += R_ALIAS_LIMB_BRIGHTNESS;
        lightcolor[2] += R_ALIAS_LIMB_BRIGHTNESS;
    }
    state.smooth = gl_smoothmodels.value != 0;
    state.affine = gl_affinemodels.value != 0;
    R_RenderAliasPose(entity, header, true, &state);
}

static void R_RenderAliasShadow(entity_t *entity, aliashdr_t *header, int pose)
{
    if (!r_shadows.value || header->command_words)
        return;
    Hyena_PushMatrix();
    R_RotateForEntity(entity, 0, entity->scale);
    Hyena_DisableCapability(HYE_TEXTURE_2D);
    Hyena_EnableCapability(HYE_BLEND);
    Hyena_SetColor(0, 0, 0, 0.5f);
    R_DrawAliasShadowGeometry(header, R_AliasPose(header, bound(0, pose, header->numposes - 1)),
        shadevector, entity->origin[2] - lightspot[2]);
    Hyena_EnableCapability(HYE_TEXTURE_2D);
    Hyena_DisableCapability(HYE_BLEND);
    Hyena_SetColor(1, 1, 1, 1);
    Hyena_PopMatrix();
}

static void R_RenderAlias(entity_t *entity, qboolean transparent)
{
    render_alias_state_t state = {0};
    aliashdr_t *header;
    size_t length = strlen(entity->model->name);
    char special = length >= 5 ? entity->model->name[length - 5] : 0;
    int skin, axis, pose;
    float add;
    extern int zombie_skins[NUM_ZOMBIE_SKINS];

    currententity = entity;
    VectorCopy(entity->origin, r_entorigin);
    VectorSubtract(r_origin, r_entorigin, modelorg);
    double light_start = r_profile.value ? Sys_FloatTime() : 0;
    R_SetupAliasLighting(entity);
    if (r_profile.value)
        Hyena_AddLightTime(Sys_FloatTime() - light_start);
    state.fullbright = transparent || R_AliasFullbright(entity) || special == '&';
    if (state.fullbright)
        lightcolor[0] = lightcolor[1] = lightcolor[2] = 256;
    else {
        if (r_model_brightness.value)
            for (axis = 0; axis < 3; ++axis)
                lightcolor[axis] += R_ALIAS_BRIGHTNESS;
        if (R_ALIAS_LIGHT_MAX) {
            for (axis = 0; axis < 3; ++axis)
                lightcolor[axis] = bound(8, lightcolor[axis], R_ALIAS_LIGHT_MAX);
        } else {
            add = 72 - lightcolor[0] - lightcolor[1] - lightcolor[2];
            if (add > 0)
                for (axis = 0; axis < 3; ++axis)
                    lightcolor[axis] += add / 3;
        }
    }
    header = R_AliasModel(entity, !transparent && doZHack && special == '%');
    skin = entity->skinnum;
    if (!transparent && special == '%') {
        if ((unsigned)skin >= NUM_ZOMBIE_SKINS) {
            Con_Printf("Zombie tex out of bounds: Tex[%i]\n", skin);
            skin = 0;
        }
        Hyena_BindTexture(zombie_skins[skin]);
    } else
        Hyena_BindTexture(header->gl_texturenum[bound(0, skin, header->numskins - 1)][r_texture_frame & 3]);
    state.transparent = transparent;
    state.alpha_test = special == '@' || special == '&';
    state.smooth = gl_smoothmodels.value != 0;
    state.affine = gl_affinemodels.value != 0;
    state.mode = entity->rendermode;
    state.amount = entity->renderamt;
    R_RenderAliasPose(entity, header, transparent, &state);
    pose = lastposenum;
    if (!transparent && !doZHack && special == '%') {
        R_RenderAliasLimb(entity, entity->z_head);
        R_RenderAliasLimb(entity, entity->z_larm);
        R_RenderAliasLimb(entity, entity->z_rarm);
    }
    R_RenderAliasShadow(entity, header, pose);
}

void R_DrawAliasModel(entity_t *entity)
{
    if (entity != &cl.viewent && entity != &cl.viewent2 && R_CullAliasEntity(entity))
        return;
    R_RenderAlias(entity, false);
}

void R_DrawTransparentAliasModel(entity_t *entity)
{
    if (!R_CullAliasEntity(entity))
        R_RenderAlias(entity, true);
}




void R_DrawEntitiesOnList(void)
{
    render_entity_batch_t batches[MAX_VISEDICTS];
    int i, count = 0, partial = 0;
    extern int zombie_skins[NUM_ZOMBIE_SKINS];

    if (!r_drawentities.value)
        return;

    for (i = 0; i < cl_numvisedicts; i++) {
        entity_t *entity = cl_visedicts[i];
        aliashdr_t *header;
        char special;
        size_t length;
        currententity = entity;
        if (!entity->model) {
            R_DrawNullModel();
            continue;
        }
        length = strlen(entity->model->name);
        special = length >= 5 ? entity->model->name[length - 5] : 0;
        if (special == '(' || special == '^')
            continue;
        if (entity->model->type == mod_brush) {
            R_DrawBrushModel(entity);
            continue;
        }
        if (entity->model->type != mod_alias || special == '$' ||
            (ISADDITIVE(entity) || ISGLOW(entity)))
            continue;
        if (R_CullAliasEntity(entity))
            continue;
        batches[count].visedict = i;
        batches[count].full = special == '%' && (partial > 5 ||
            (entity->z_head && entity->z_larm && entity->z_rarm));
        if (special == '%' && !batches[count].full)
            partial++;
        if (special == '%') {
            int skin = entity->skinnum;
            batches[count].texture = zombie_skins[(unsigned)skin < NUM_ZOMBIE_SKINS ? skin : 0];
        } else {
            header = (aliashdr_t *)Mod_Extradata(entity->model);
            batches[count].texture = header->gl_texturenum[bound(0, entity->skinnum, header->numskins - 1)][r_texture_frame & 3];
        }
        count++;
    }
    if (count > 1 && r_entity_sort.value)
        qsort(batches, count, sizeof(batches[0]), R_CompareEntities);
    for (i = 0; i < count; i++) {
        currententity = cl_visedicts[batches[i].visedict];
        doZHack = batches[i].full;
        R_RenderAlias(currententity, false);
    }
    doZHack = 0;
    for (i = 0; i < cl_numvisedicts; i++) {
        entity_t *entity = cl_visedicts[i];
        size_t length;
        currententity = entity;
        if (!entity->model)
            continue;
        length = strlen(entity->model->name);
        if (entity->model->type == mod_sprite)
            R_DrawSpriteModel(entity);
        else if (entity->model->type == mod_alias &&
            ((length >= 5 && entity->model->name[length - 5] == '$') ||
             (ISADDITIVE(entity) || ISGLOW(entity)))) {
            R_FlushSpriteBatch();
            R_DrawTransparentAliasModel(entity);
        }
    }
    R_FlushSpriteBatch();
}

static void R_DrawViewEntity(entity_t *entity)
{
    if (!r_drawviewmodel.value || chase_active.value || envmap ||
        !r_drawentities.value || cl.stats[STAT_HEALTH] <= 0 || !entity->model)
        return;
    currententity = entity;
    entity->renderamt = cl_entities[cl.viewentity].renderamt;
    entity->rendermode = cl_entities[cl.viewentity].rendermode;
    VectorCopy(cl_entities[cl.viewentity].rendercolor, entity->rendercolor);
    Hyena_SetDepthRange(0, 0.3f);
    R_RenderAlias(entity, false);
    Hyena_ResetDepthRange();
}

void R_DrawViewModel(void) { R_DrawViewEntity(&cl.viewent); }
void R_DrawView2Model(void) { R_DrawViewEntity(&cl.viewent2); }


static void R_DepthBounds(float near_value, float far_value)
{
    r_depthmin = near_value;
    r_depthmax = far_value;
    Hyena_DepthBounds(near_value, far_value);
    Hyena_ResetDepthRange();
}

static void R_Perspective(float fovy, float aspect, float near_value, float far_value)
{
    float ymax = near_value * tanf(fovy * (float)M_PI / 360);
    Hyena_Frustum(-ymax * aspect, ymax * aspect, -ymax, ymax, near_value, far_value);
}

void R_SetupGL(void)
{
    int x, x2, y, y2, width, height, contents;
    float fovy = r_refdef.fov_y;
    x = r_refdef.vrect.x * glwidth / vid.width;
    x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * glwidth / vid.width;
    y = (vid.height - r_refdef.vrect.y) * glheight / vid.height;
    y2 = (vid.height - r_refdef.vrect.y - r_refdef.vrect.height) * glheight / vid.height;
    if (x > 0) x--;
    if (x2 < glwidth) x2++;
    if (y2 < 0) y2--;
    if (y < glheight) y++;
    width = x2 - x;
    height = y - y2;
    if (envmap) {
        x = y2 = 0;
        width = height = 256;
    }
    if (r_waterwarp.value && cl.worldmodel) {
        contents = Mod_PointInLeaf(r_origin, cl.worldmodel)->contents;
        if (contents == CONTENTS_WATER || contents == CONTENTS_SLIME || contents == CONTENTS_LAVA)
            fovy = atanf(tanf(fovy * (float)M_PI / 360) * (1.03f - sinf((float)cl.time * 2) * 0.04f)) * 360 / (float)M_PI;
    }
    r_projection_fovy = fovy;
    r_projection_aspect = (float)r_refdef.vrect.width / r_refdef.vrect.height;
    r_projection_far = r_refdef.fog_start != 0 && r_refdef.fog_end > 0 ? r_refdef.fog_end + 16 : r_farclip.value;
    if (r_projection_far < R_NEAR_CLIP + 1)
        r_projection_far = R_NEAR_CLIP + 1;
    Hyena_MatrixMode(HYE_PROJECTION);
    Hyena_LoadIdentity();
    Hyena_Viewport(glx + x, gly + y2, width, height);
    R_Perspective(fovy, r_projection_aspect, R_NEAR_CLIP, r_projection_far);
    if (mirror)
        Hyena_MatrixScale(mirror_plane->normal[2] ? 1 : -1, mirror_plane->normal[2] ? -1 : 1, 1);
    Hyena_CullFace(mirror);
    Hyena_UpdateMatrices();
    Hyena_MatrixMode(HYE_VIEW);
    Hyena_LoadIdentity();
    Hyena_MatrixRotate(-90, 1, 0, 0);
    Hyena_MatrixRotate(90, 0, 0, 1);
    Hyena_MatrixRotate(-r_refdef.viewangles[ROLL], 1, 0, 0);
    Hyena_MatrixRotate(-r_refdef.viewangles[PITCH], 0, 1, 0);
    Hyena_MatrixRotate(-r_refdef.viewangles[YAW], 0, 0, 1);
    Hyena_MatrixTranslate(-r_refdef.vieworg[0], -r_refdef.vieworg[1], -r_refdef.vieworg[2]);
    Hyena_GetMatrix(r_world_matrix);
    Hyena_UpdateMatrices();
    if (Hyena_SeparateViewMatrix()) {
        Hyena_MatrixMode(HYE_MODELVIEW);
        Hyena_LoadIdentity();
    }
    Hyena_ClipFrustum(fovy, r_projection_aspect, R_NEAR_CLIP);
    Hyena_UpdateMatrices();
    if (gl_cull.value) Hyena_EnableCapability(HYE_CULL_FACE);
    else Hyena_DisableCapability(HYE_CULL_FACE);
    Hyena_DisableCapability(HYE_BLEND);
    Hyena_DisableCapability(HYE_ALPHA_TEST);
    Hyena_EnableCapability(HYE_DEPTH_TEST);
}

void R_Clear(void)
{
    static int trickframe;
    qboolean color = gl_clear.value;
    if (r_refdef.fog_end > 0 && r_skyfog.value) {
        Hyena_ClearColor(r_refdef.fog_red * 0.01f, r_refdef.fog_green * 0.01f, r_refdef.fog_blue * 0.01f, 1);
        color = true;
    } else if (!skybox_name[0]) {
        int red = 64, green = 64, blue = 70;
        sscanf(r_skycolor.string, "%d %d %d", &red, &green, &blue);
        Hyena_ClearColor(bound(0, red, 255) / 255.0f, bound(0, green, 255) / 255.0f, bound(0, blue, 255) / 255.0f, 1);
        color = true;
    }
    if (r_mirroralpha.value != 1) {
        Hyena_Clear(color, true, r_shadows.value != 0);
        R_DepthBounds(0, 0.5f);
    } else if (gl_ztrick.value) {
        Hyena_Clear(color, false, r_shadows.value != 0);
        if (++trickframe & 1) R_DepthBounds(0, 0.49999f);
        else R_DepthBounds(1, 0.5f);
    } else {
        Hyena_Clear(color, true, r_shadows.value != 0);
        R_DepthBounds(0, 1);
    }
    Hyena_DepthFunction(r_depthmin > r_depthmax ? HYE_GEQUAL : HYE_LEQUAL);
}

void R_PolyBlend(void)
{
    static const float points[4][3] = {{10, 100, 100}, {10, -100, 100}, {10, -100, -100}, {10, 100, -100}};
    hyena_arrays_t arrays = {0};
    if (!gl_polyblend.value || !r_polyblend.value || !v_blend[3])
        return;
    Hyena_DisableCapability(HYE_ALPHA_TEST);
    Hyena_EnableCapability(HYE_BLEND);
    Hyena_DisableCapability(HYE_DEPTH_TEST);
    Hyena_DisableCapability(HYE_TEXTURE_2D);
    Hyena_MatrixMode(HYE_VIEW);
    Hyena_PushMatrix();
    Hyena_LoadIdentity();
    Hyena_MatrixRotate(-90, 1, 0, 0);
    Hyena_MatrixRotate(90, 0, 0, 1);
    if (Hyena_SeparateViewMatrix()) {
        Hyena_UpdateMatrices();
        Hyena_MatrixMode(HYE_MODELVIEW);
        Hyena_PushMatrix();
        Hyena_LoadIdentity();
    }
    Hyena_UpdateMatrices();
    Hyena_SetColor(v_blend[0], v_blend[1], v_blend[2], v_blend[3]);
    arrays.position.data = points;
    arrays.position.stride = 3 * sizeof(float);
    arrays.count = 4;
    arrays.flags = HYE_COPY_VERTICES;
    Hyena_DrawArrays(HYE_QUADS, &arrays, 4);
    Hyena_PopMatrix();
    if (Hyena_SeparateViewMatrix()) {
        Hyena_MatrixMode(HYE_VIEW);
        Hyena_PopMatrix();
        Hyena_MatrixMode(HYE_MODELVIEW);
    }
    Hyena_DisableCapability(HYE_BLEND);
    Hyena_EnableCapability(HYE_TEXTURE_2D);
    Hyena_EnableCapability(HYE_DEPTH_TEST);
    Hyena_EnableCapability(HYE_ALPHA_TEST);
    Hyena_SetColor(1, 1, 1, 1);
}

static void R_ProfileScene(int stage)
{
    static double times[6], totals[5], last_report;
    static int samples;
    int i;
    if (!r_profile.value)
        return;
    times[stage] = Sys_FloatTime();
    if (stage != 5)
        return;
    for (i = 0; i < 5; i++)
        totals[i] += times[i + 1] - times[i];
    samples++;
    if (times[5] - last_report >= 2) {
        Con_SafePrintf("Scene: setup %.2f, world %.2f, sound %.2f, entities %.2f, effects %.2f ms\n",
            totals[0] * 1000 / samples, totals[1] * 1000 / samples, totals[2] * 1000 / samples,
            totals[3] * 1000 / samples, totals[4] * 1000 / samples);
        memset(totals, 0, sizeof(totals));
        samples = 0;
        last_report = times[5];
    }
}

void R_Mirror(void)
{
    float distance;
    msurface_t *surface;
    if (!mirror)
        return;
    memcpy(r_base_world_matrix, r_world_matrix, sizeof(r_base_world_matrix));
    distance = DotProduct(r_refdef.vieworg, mirror_plane->normal) - mirror_plane->dist;
    VectorMA(r_refdef.vieworg, -2 * distance, mirror_plane->normal, r_refdef.vieworg);
    distance = DotProduct(vpn, mirror_plane->normal);
    VectorMA(vpn, -2 * distance, mirror_plane->normal, vpn);
    r_refdef.viewangles[PITCH] = -asinf(vpn[2]) / (float)M_PI * 180;
    r_refdef.viewangles[YAW] = atan2f(vpn[1], vpn[0]) / (float)M_PI * 180;
    r_refdef.viewangles[ROLL] = -r_refdef.viewangles[ROLL];
    if (cl_numvisedicts < MAX_VISEDICTS)
        cl_visedicts[cl_numvisedicts++] = &cl_entities[cl.viewentity];
    R_DepthBounds(0.5f, 1);
    Hyena_DepthFunction(HYE_LEQUAL);
    R_RenderScene();
    R_DrawWaterSurfaces();
    R_DepthBounds(0, 0.5f);
    Hyena_DepthFunction(HYE_LEQUAL);
    Hyena_EnableCapability(HYE_BLEND);
    Hyena_MatrixMode(HYE_PROJECTION);
    Hyena_MatrixScale(mirror_plane->normal[2] ? 1 : -1, mirror_plane->normal[2] ? -1 : 1, 1);
    Hyena_CullFace(false);
    Hyena_MatrixMode(HYE_VIEW);
    Hyena_LoadMatrix(r_base_world_matrix);
    if (Hyena_SeparateViewMatrix()) {
        Hyena_UpdateMatrices();
        Hyena_MatrixMode(HYE_MODELVIEW);
        Hyena_LoadIdentity();
    }
    Hyena_SetColor(1, 1, 1, r_mirroralpha.value);
    for (surface = cl.worldmodel->textures[mirrortexturenum]->texturechain; surface; surface = surface->texturechain)
        R_RenderBrushPoly(surface);
    cl.worldmodel->textures[mirrortexturenum]->texturechain = NULL;
    Hyena_DisableCapability(HYE_BLEND);
    Hyena_SetColor(1, 1, 1, 1);
}


void R_RenderScene(void)
{
    R_ProfileScene(0);
    R_SetupFrame();
    R_SetFrustum();
    R_SetupGL();
    R_MarkLeaves(r_novis.value != 0, mirror);
    R_ProfileScene(1);
    R_DrawWorld();
    R_ProfileScene(2);
    S_ExtraUpdate();
    R_ProfileScene(3);
    R_DrawEntitiesOnList();
    R_ProfileScene(4);
    R_DrawDecals();
    R_DrawParticles();
    R_ProfileScene(5);
}

void R_RenderView(void)
{
    double start = 0;
    if (r_norefresh.value)
        return;
    if (!r_worldentity.model || !cl.worldmodel)
        Sys_Error("NULL worldmodel");
    if (gl_finish.value || r_speeds.value)
        Hyena_Finish();
    if (r_speeds.value)
        start = Sys_FloatTime();
    mirror = false;
    R_Clear();
    Fog_EnableGFog();
    R_RenderScene();
    Hyena_MatrixMode(HYE_PROJECTION);
    Hyena_PushMatrix();
    Hyena_LoadIdentity();
    R_Perspective(r_projection_fovy, r_projection_aspect, 0.1f, r_projection_far);
    Hyena_ClipFrustum(r_projection_fovy, r_projection_aspect, 0.1f);
    Hyena_UpdateMatrices();
    Hyena_MatrixMode(HYE_MODELVIEW);
    R_DrawViewModel();
    R_DrawView2Model();
    Hyena_ClipFrustum(r_projection_fovy, r_projection_aspect, R_NEAR_CLIP);
    Hyena_MatrixMode(HYE_PROJECTION);
    Hyena_PopMatrix();
    Hyena_UpdateMatrices();
    Hyena_MatrixMode(HYE_MODELVIEW);
    R_DrawWaterSurfaces();
    Fog_DisableGFog();
    R_Mirror();
    R_PolyBlend();
    if (r_speeds.value) {
        int base_batches, lightmap_batches, base_indices, lightmap_indices;
        R_WorldBatchStats(&base_batches, &lightmap_batches, &base_indices, &lightmap_indices);
        Con_Printf("%3i ms  %4i wpoly %4i epoly  %i/%i world batches  %i/%i indices\n",
            (int)((Sys_FloatTime() - start) * 1000), c_brush_polys, c_alias_polys,
            base_batches, lightmap_batches, base_indices, lightmap_indices);
    }
}

static void R_TransformEntity(const vec3_t origin, const vec3_t angles, unsigned char scale)
{
    float amount = scale && scale != ENTSCALE_DEFAULT ? ENTSCALE_DECODE(scale) : 1;
    Hyena_MatrixTranslate(origin[0], origin[1], origin[2]);
    Hyena_MatrixRotate(angles[YAW], 0, 0, 1);
    Hyena_MatrixRotate(-angles[PITCH], 0, 1, 0);
    Hyena_MatrixRotate(angles[ROLL], 1, 0, 0);
    if (amount != 1)
        Hyena_MatrixScale(amount, amount, amount);
    Hyena_UpdateMatrices();
}

static void R_BlendEntityVector(const vec3_t from, const vec3_t to, float blend, qboolean angles, vec3_t result)
{
    int i;
    for (i = 0; i < 3; i++) {
        float delta = to[i] - from[i];
        if (angles) {
            if (delta > 180)
                delta -= 360;
            else if (delta < -180)
                delta += 360;
        }
        result[i] = blend >= 1 ? to[i] : from[i] + blend * delta;
    }
}

static void R_InterpolateEntity(entity_t *entity, vec3_t origin, vec3_t angles)
{
    float now = (float)realtime;
    float position_blend, angle_blend;
    int i;
    qboolean reset = cl.paused || !entity->translate_start_time || !entity->rotate_start_time ||
        now < entity->translate_start_time || now < entity->rotate_start_time ||
        now - entity->translate_start_time > 1 || now - entity->rotate_start_time > 1;

    for (i = 0; i < 3; i++)
        if (fabsf(entity->origin[i] - entity->origin2[i]) > 100)
            reset = true;
    if (reset) {
        VectorCopy(entity->origin, entity->origin1);
        VectorCopy(entity->origin, entity->origin2);
        VectorCopy(entity->angles, entity->angles1);
        VectorCopy(entity->angles, entity->angles2);
        entity->translate_start_time = entity->rotate_start_time = now;
        VectorCopy(entity->origin, origin);
        VectorCopy(entity->angles, angles);
        return;
    }
    if (VectorCompare(entity->origin1, entity->origin2) && VectorCompare(entity->origin2, entity->origin) &&
        VectorCompare(entity->angles1, entity->angles2) && VectorCompare(entity->angles2, entity->angles)) {
        entity->translate_start_time = entity->rotate_start_time = now;
        VectorCopy(entity->origin, origin);
        VectorCopy(entity->angles, angles);
        return;
    }
    position_blend = bound(0, (now - entity->translate_start_time) / 0.1f, 1);
    angle_blend = bound(0, (now - entity->rotate_start_time) / 0.1f, 1);
    R_BlendEntityVector(entity->origin1, entity->origin2, position_blend, false, origin);
    R_BlendEntityVector(entity->angles1, entity->angles2, angle_blend, true, angles);
    if (!VectorCompare(entity->origin, entity->origin2)) {
        VectorCopy(origin, entity->origin1);
        VectorCopy(entity->origin, entity->origin2);
        entity->translate_start_time = now;
    }
    if (!VectorCompare(entity->angles, entity->angles2)) {
        VectorCopy(angles, entity->angles1);
        VectorCopy(entity->angles, entity->angles2);
        entity->rotate_start_time = now;
    }
}

void R_RotateForEntity(entity_t *entity, int shadow, unsigned char scale)
{
    vec3_t origin, angles;
    if (r_i_model_transform.value && entity->model && entity->model->type == mod_alias &&
        entity != &cl.viewent && entity != &cl.viewent2) {
        if (strcmp(entity->old_model, entity->model->name))
            entity->translate_start_time = entity->rotate_start_time = 0;
        R_InterpolateEntity(entity, origin, angles);
    } else {
        entity->translate_start_time = entity->rotate_start_time = 0;
        VectorCopy(entity->origin, origin);
        VectorCopy(entity->angles, angles);
    }
    if (entity == &cl_entities[cl.viewentity])
        angles[PITCH] *= 0.3f;
    if (shadow)
        angles[PITCH] = angles[ROLL] = 0;
    R_TransformEntity(origin, angles, scale);
}

void R_BlendedRotateForEntity(entity_t *entity, int shadow, unsigned char scale)
{
    vec3_t origin, angles;
    if (r_i_model_transform.value)
        R_InterpolateEntity(entity, origin, angles);
    else {
        VectorCopy(entity->origin, origin);
        VectorCopy(entity->angles, angles);
    }
    if (entity == &cl_entities[cl.viewentity])
        angles[PITCH] *= 0.3f;
    if (shadow)
        angles[PITCH] = angles[ROLL] = 0;
    R_TransformEntity(origin, angles, scale);
}

void R_DrawNullModel(void)
{
    static const float points[2][6][3] = {
        {{0,0,9},{9,0,0},{0,-9,0},{-9,0,0},{0,9,0},{9,0,0}},
        {{0,0,-9},{9,0,0},{0,9,0},{-9,0,0},{0,-9,0},{9,0,0}}
    };
    hyena_arrays_t arrays = {0};
    int side;
    Hyena_PushMatrix();
    R_RotateForEntity(currententity, 0, currententity->scale);
    Hyena_DisableCapability(HYE_TEXTURE_2D);
    arrays.position.stride = 3 * sizeof(float);
    arrays.count = 6;
    arrays.flags = HYE_COPY_VERTICES;
    for (side = 0; side < 2; side++) {
        arrays.position.data = points[side];
        Hyena_SetColor(1, side ? 0 : 0.6f, 0, 1);
        Hyena_DrawArrays(r_showtris.value ? HYE_LINE_STRIP : HYE_TRIANGLE_FAN, &arrays, 6);
    }
    Hyena_EnableCapability(HYE_TEXTURE_2D);
    Hyena_SetColor(1, 1, 1, 1);
    Hyena_PopMatrix();
}

void R_DrawLine(vec3_t start, vec3_t end, vec3_t color)
{
    float points[2][3];
    hyena_arrays_t arrays = {0};
    VectorCopy(start, points[0]);
    VectorCopy(end, points[1]);
    arrays.position.data = points;
    arrays.position.stride = sizeof(points[0]);
    arrays.count = 2;
    arrays.flags = HYE_COPY_VERTICES;
    Hyena_DisableCapability(HYE_TEXTURE_2D);
    Hyena_DisableCapability(HYE_CULL_FACE);
    Hyena_SetColor(color[0], color[1], color[2], 1);
    Hyena_DrawArrays(HYE_LINE_STRIP, &arrays, 2);
    Hyena_SetColor(1, 1, 1, 1);
    Hyena_EnableCapability(HYE_TEXTURE_2D);
    Hyena_EnableCapability(HYE_CULL_FACE);
}

void R_EmitWireBox(vec3_t mins, vec3_t maxs, qboolean line_strip)
{
    float points[10][3];
    static const byte corners[10] = {0,4,1,5,3,7,2,6,0,4};
    hyena_arrays_t arrays = {0};
    int i, axis;
    for (i = 0; i < 10; i++)
        for (axis = 0; axis < 3; axis++)
            points[i][axis] = corners[i] & (1 << axis) ? maxs[axis] : mins[axis];
    arrays.position.data = points;
    arrays.position.stride = sizeof(points[0]);
    arrays.count = 10;
    arrays.flags = HYE_COPY_VERTICES;
    Hyena_DrawArrays(line_strip ? HYE_LINE_STRIP : HYE_TRIANGLE_STRIP, &arrays, 10);
}
