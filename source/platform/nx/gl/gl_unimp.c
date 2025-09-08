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

#include "../../../nzportable_def.h"

cvar_t	r_part_spikes		= {"r_part_spikes",      "0",qfalse};
cvar_t	r_part_gunshots	    = {"r_part_gunshots",    "0",qfalse};
cvar_t	r_part_telesplash	= {"r_part_telesplash",  "0",qfalse};
cvar_t	r_part_lightning	= {"r_part_lightning",   "0",qfalse};
cvar_t 	r_decal_bullets 	= {"r_decal_bullets",    "0",qfalse};
cvar_t 	r_decal_explosions 	= {"r_decal_explosions", "0",qfalse};
cvar_t 	r_part_trails 		= {"r_part_trails", 	 "0",qfalse};
cvar_t 	r_part_flames 		= {"r_part_flames", 	 "0",qfalse};
cvar_t 	r_part_muzzleflash	= {"r_part_muzzleflash", "0",qfalse};

cvar_t	gl_polyblend 		= {"gl_polyblend","0", qfalse};

qboolean		qmb_initialized = qfalse;

int decal_blood1, decal_blood2, decal_blood3, decal_burn, decal_mark, decal_glow;

void R_SpawnDecalStatic (vec3_t org, int tex, int size)
{
	// naievil -- TODO: implement me
	return;
}

void QMB_LightningBeam (vec3_t start, vec3_t end)
{
	// naievil -- TODO: implement me
	return;
}

void QMB_MuzzleFlash (vec3_t org)
{
	// naievil -- TODO: implement me
	return;
}

void Sky_LoadSkyBox(char* name)
{
	// naievil -- TODO: implement me
	Con_Printf("Sky_LoadSkyBox - not implemented!\n");
	return;
}