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

#include "cpu_main.h"

cvar_t	r_part_spikes		= {"r_part_spikes",      "0",qfalse};
cvar_t	r_part_gunshots	    = {"r_part_gunshots",    "0",qfalse};
cvar_t	r_part_telesplash	= {"r_part_telesplash",  "0",qfalse};
cvar_t	r_part_lightning	= {"r_part_lightning",   "0",qfalse};
cvar_t 	r_part_trails 		= {"r_part_trails", 	 "0",qfalse};
cvar_t 	r_part_flames 		= {"r_part_flames", 	 "0",qfalse};
cvar_t 	r_part_muzzleflash	= {"r_part_muzzleflash", "0",qfalse};

cvar_t	gl_polyblend 		= {"gl_polyblend","0", qfalse};


void Sky_LoadSkyBox(char* name)
{
	// naievil -- TODO: implement me
	Con_Printf("Sky_LoadSkyBox - not implemented!\n");
	return;
}

void Fog_ParseServerMessage (void)
{
	// naievil -- TODO: implement me
	Con_Printf("Fog_ParseServerMessage - not implemented!\n");
	return;
}