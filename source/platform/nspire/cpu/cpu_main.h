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

extern cvar_t	gl_polyblend;

extern qboolean qmb_initialized;

extern int decal_blood1, decal_blood2, decal_blood3, decal_burn, decal_mark, decal_glow;

/*
---------------------------------
half-life Render Modes. Crow_bar
---------------------------------
*/

#define TEX_COLOR    1
#define TEX_TEXTURE  2
#define TEX_GLOW     3
#define TEX_SOLID    4
#define TEX_ADDITIVE 5
#define TEX_LMPOINT  6 //for light point

#define ISCOLOR(ent)    ((ent)->rendermode == TEX_COLOR    && ((ent)->rendercolor[0] <= 1|| \
                                                               (ent)->rendercolor[1] <= 1|| \
															   (ent)->rendercolor[2] <= 1))

#define ISTEXTURE(ent)  ((ent)->rendermode == TEX_TEXTURE  && (ent)->renderamt > 0 && (ent)->renderamt <= 1)
#define ISGLOW(ent)     ((ent)->rendermode == TEX_GLOW     && (ent)->renderamt > 0 && (ent)->renderamt <= 1)
#define ISSOLID(ent)    ((ent)->rendermode == TEX_SOLID    && (ent)->renderamt > 0 && (ent)->renderamt <= 1)
#define ISADDITIVE(ent) ((ent)->rendermode == TEX_ADDITIVE && (ent)->renderamt > 0 && (ent)->renderamt <= 1)

#define ISLMPOINT(ent)  ((ent)->rendermode == TEX_LMPOINT  && ((ent)->rendercolor[0] <= 1|| \
                                                               (ent)->rendercolor[1] <= 1|| \
															   (ent)->rendercolor[2] <= 1))
/*
---------------------------------
//half-life Render Modes
---------------------------------
*/

void R_SpawnDecalStatic (vec3_t org, int tex, int size);

void QMB_LightningBeam (vec3_t start, vec3_t end);
void QMB_MuzzleFlash (vec3_t org);

void Sky_LoadSkyBox(char* name);

void Fog_ParseServerMessage (void);