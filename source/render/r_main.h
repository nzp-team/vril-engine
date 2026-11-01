/*
 * Copyright (C) 1996-2001 Id Software, Inc.
 * Copyright (C) 2002-2009 John Fitzgibbons and others
 * Copyright (C) 2010-2014 QuakeSpasm developers
 * Copyright (C) 2023 NZ:P Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */
// r_main.h -- Agnostic render header
#ifndef _RENDER_MAIN_H_
#define _RENDER_MAIN_H_

#include "r_types.h"
#include "r_hyena.h"
#include "r_color_quantization.h"
#include "r_entity_fragments.h"
#include "r_light.h"
#include "r_fog.h"
#include "r_mdl.h"
#include "r_surface.h"
#include "r_sky.h"

typedef struct {
    float position[4], uv[4], color[4];
} __attribute__((aligned(16))) render_sprite_vertex_t;

extern cvar_t r_mirroralpha, r_wateralpha, r_skyfog, gl_clear, gl_cull;
extern cvar_t gl_smoothmodels, gl_affinemodels, gl_polyblend, gl_flashblend, gl_playermip;
extern cvar_t gl_nocolors, gl_reporttjunctions, gl_doubleeyes, r_flatlightstyles, r_retro;
extern cvar_t r_dithering, r_partalpha, r_vsync, r_loadq3models, cl_loadmapcfg;
extern cvar_t r_restexf, r_texcompr, r_maxrange, r_caustics, r_detail;
extern cvar_t r_detail_mipmaps, r_detail_mipmaps_func, r_detail_mipmaps_bias, r_asynch, r_mipmaps;
extern cvar_t r_mipmaps_func, r_mipmaps_bias, r_tex_scale_down, r_particles_simple, r_waterripple;
extern cvar_t r_waterwarp, r_showbboxes, r_showbboxes_full, r_showtris, r_showtris_full;
extern cvar_t r_polyblend;

extern cvar_t r_profile, r_entity_sort, r_sprite_batch, gl_ztrick;
extern float r_world_matrix[16];
extern entity_t *currententity;
extern int r_texture_frame, doZHack;
extern vec3_t shadevector;
extern float ambientlight, shadelight, *shadedots;
extern float r_avertexnormals[162][3], r_avertexnormal_dots[16][256];
extern int lastposenum, lastposenum0;

qboolean R_CullBox(vec3_t mins, vec3_t maxs);
void R_RotateForEntity(entity_t *entity, int shadow, unsigned char scale);
void R_BlendedRotateForEntity(entity_t *entity, int shadow, unsigned char scale);
void R_InitCvars(void);
void R_DrawNullModel(void);
void R_BuildLightmaps(void);
extern cvar_t r_i_model_animation, r_i_model_transform;
void R_ClearSceneCaches(void);
aliashdr_t *R_AliasModel(entity_t *entity, qboolean full);
qboolean R_CullAliasEntity(const entity_t *entity);
void R_DrawAliasModel(entity_t *entity);
void R_DrawTransparentAliasModel(entity_t *entity);
void R_SetupAliasFrame(int frame, aliashdr_t *header);
void R_SetupAliasBlendedFrame(int frame, aliashdr_t *header, entity_t *entity);
void IgnoreInterpolatioFrame(entity_t *entity, aliashdr_t *header);
void R_DrawEntitiesOnList(void);
void R_DrawSpriteModel(entity_t *entity);
mspriteframe_t *R_GetSpriteFrame(entity_t *entity);
void R_DrawViewModel(void);
void R_DrawView2Model(void);
void R_RenderScene(void);
void R_RenderView(void);
void R_SetFrustum(void);
void R_SetupGL(void);
void R_Clear(void);
void R_Mirror(void);
void R_PolyBlend(void);
int R_FrustumCheckBox(vec3_t mins, vec3_t maxs);
int R_FrustumCheckSphere(vec3_t center, float radius);
qboolean R_CullSphere(vec3_t center, float radius);
int SignbitsForPlane(mplane_t *plane);

void R_InitDecals(void);
void R_ClearDecals(void);
void R_DrawDecals(void);
void R_SpawnDecal(vec3_t center, vec3_t normal, vec3_t tangent, int texture, int size, int bsp_decal);
void R_SpawnDecalStatic(vec3_t origin, int texture, int size);
void R_SpawnDecalBSP(vec3_t origin, char *texture_name, int size);

#endif // _RENDER_MAIN_H_
