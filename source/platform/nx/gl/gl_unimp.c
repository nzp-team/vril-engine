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

cvar_t	r_part_spikes		= {"r_part_spikes",      "0",false};
cvar_t	r_part_gunshots	    = {"r_part_gunshots",    "0",false};
cvar_t	r_part_telesplash	= {"r_part_telesplash",  "0",false};
cvar_t	r_part_lightning	= {"r_part_lightning",   "0",false};
cvar_t 	r_decal_bullets 	= {"r_decal_bullets",    "0",false};
cvar_t 	r_decal_explosions 	= {"r_decal_explosions", "0",false};
cvar_t 	r_part_trails 		= {"r_part_trails", 	 "0",false};
cvar_t 	r_part_flames 		= {"r_part_flames", 	 "0",false};
cvar_t 	r_part_muzzleflash	= {"r_part_muzzleflash", "0",false};

qboolean		qmb_initialized = false;

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

void M_OSK_Draw (void)
{
	// TODO: Implement.
	Con_Printf("M_OSK_Draw: unimplemented\n");
	return;
}