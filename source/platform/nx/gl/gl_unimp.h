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

extern cvar_t	r_part_spikes;
extern cvar_t 	r_part_gunshots;
extern cvar_t 	r_part_telesplash;
extern cvar_t 	r_part_lightning;
extern cvar_t	r_decal_bullets;
extern cvar_t 	r_decal_explosions;
extern cvar_t	r_part_trails;
extern cvar_t 	r_part_flames;
extern cvar_t 	r_part_muzzleflash;

extern qboolean qmb_initialized;

extern int decal_blood1, decal_blood2, decal_blood3, decal_burn, decal_mark, decal_glow;

/*
---------------------------------
//half-life Render Modes
---------------------------------
*/

void R_SpawnDecalStatic (vec3_t org, int tex, int size);

void QMB_LightningBeam (vec3_t start, vec3_t end);
void QMB_MuzzleFlash (vec3_t org);

void R_ClearSkyBox (void);
void R_DrawSkyBox (void);