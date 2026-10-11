// Copyright (C) 2026 NZ:P Team
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
//
// See the GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

// r_sky.h -- Shared skybox rendering and loading

#ifndef _R_SKY_H_
#define _R_SKY_H_

typedef struct {
    byte  color[4];
    float xyz[3];
} sky_fog_vertex_t;

extern char skybox_name[32];
extern int skytexturenum;
extern cvar_t r_skyfogblend;

void
Sky_Configure(qboolean enabled, qboolean preserve_alpha_test, qboolean reset_blend);
void
Sky_Init(void);
void
Sky_ClearTextures(void);
void
Sky_NewMap(void);
void
Sky_LoadSkyBox(char * name);
void
R_DrawSkyBox(void);

#endif // ifndef _R_SKY_H_
