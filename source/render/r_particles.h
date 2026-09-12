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

#ifndef R_PARTICLES_H
#define R_PARTICLES_H

typedef enum trail_type_s
{
	ROCKET_TRAIL,
	GRENADE_TRAIL,
	BLOOD_TRAIL,
	TRACER1_TRAIL,
	SLIGHT_BLOOD_TRAIL,
	NAIL_TRAIL,
	TRACER2_TRAIL,
	VOOR_TRAIL,
	ALT_ROCKET_TRAIL,
	LAVA_TRAIL,
	BUBBLE_TRAIL,
	NEHAHRA_SMOKE,
	RAYGREEN_TRAIL,
	RAYRED_TRAIL,
	RAYBEAMGREEN_TRAIL,
	RAYBEAMRED_TRAIL
} trail_type_t;

void R_ParseParticleEffect (void);
void R_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count);
void R_RocketTrail (vec3_t start, vec3_t end, trail_type_t type);
void R_EntityParticles (entity_t *ent);
void R_BlobExplosion (vec3_t org);
void R_ParticleExplosion (vec3_t org);
void R_ParticleExplosion2 (vec3_t org, int colorStart, int colorLength);
void R_LavaSplash (vec3_t org);
void R_TeleportSplash (vec3_t org);

#ifdef QUAKE2
void R_DarkFieldParticles (entity_t *ent);
#endif

void QMB_InitParticles (void);
void QMB_ClearParticles (void);
void QMB_DrawParticles (void);
void QMB_RunParticleEffect (vec3_t org, vec3_t dir, int color, int count);
void QMB_RocketTrail (vec3_t start, vec3_t end, trail_type_t type);
void QMB_BlobExplosion (vec3_t org);
void QMB_ParticleExplosion (vec3_t org);
void QMB_LavaSplash (vec3_t org);
void QMB_TeleportSplash (vec3_t org);
void QMB_InfernoFlame (vec3_t org);
void QMB_StaticBubble (entity_t *ent);
void QMB_ColorMappedExplosion (vec3_t org, int colorStart, int colorLength);
void QMB_TorchFlame (vec3_t org);
void QMB_FlameGt (vec3_t org, float size, float time);
void QMB_BigTorchFlame (vec3_t org);
void QMB_ShamblerCharge (vec3_t org);
void QMB_LightningBeam (vec3_t start, vec3_t end);
void QMB_EntityParticles (entity_t *ent);
void QMB_MuzzleFlash (vec3_t org, vec3_t muzzle_axis);
void QMB_MuzzleFlashLG (vec3_t org);

extern qboolean qmb_initialized;

#endif