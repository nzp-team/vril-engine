/*
 * Copyright (C) 1996-1997 Id Software, Inc.
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

#define MAXCLIPPLANES 11

#define TOP_RANGE     16
#define BOTTOM_RANGE  96

typedef struct entity_s {
    qboolean         forcelink;

    int              update_type;

    entity_state_t   baseline;

    double           msgtime;
    vec3_t           msg_origins[2];
    vec3_t           origin;
    vec3_t           msg_angles[2];
    vec3_t           angles;

    float            renderamt;
    float            rendermode;
    float            rendercolor[3];

    unsigned char    scale;
    struct model_s * model;
    char             old_model[128];
    struct efrag_s * efrag;
    int              frame;
    float            syncbase;
    byte *           colormap;
    int              effects;
    int              skinnum;
    int              visframe;

    float            translate_start_time;
    vec3_t           origin1;
    vec3_t           origin2;

    float            rotate_start_time;
    vec3_t           angles1;
    vec3_t           angles2;

    int              dlightframe;
    int              dlightbits;

    int              trivial_accept;
    struct mnode_s * topnode;

    float            frame_start_time;
    float            frame_interval;
    int              pose1;
    int              pose2;

    int              modelindex;

    int              z_head;
    int              z_larm;
    int              z_rarm;
} entity_t;

typedef struct {
    vrect_t vrect;

    vrect_t aliasvrect;
    int     vrectright, vrectbottom;
    int     aliasvrectright, aliasvrectbottom;
    float   vrectrightedge;

    float   fvrectx, fvrecty;
    float   fvrectx_adj, fvrecty_adj;
    int     vrect_x_adj_shift20;
    int     vrectright_adj_shift20;
    float   fvrectright_adj, fvrectbottom_adj;

    float   fvrectright;
    float   fvrectbottom;
    float   horizontalFieldOfView;

    float   xOrigin;
    float   yOrigin;

    vec3_t  vieworg;
    vec3_t  viewangles;

    float   fov_x, fov_y;

    int     ambientlight;

    float   fog_start;
    float   fog_end;
    float   fog_red;
    float   fog_green;
    float   fog_blue;
} refdef_t;

extern int reinit_surfcache;

extern refdef_t r_refdef;
extern vec3_t r_origin, vpn, vright, vup;

extern struct texture_s * r_notexture_mip;

void
R_Init(void);
void
R_InitTextures(void);
void
R_RenderView(void);
void
R_ViewChanged(vrect_t * pvrect, int lineadj, float aspect);

void
R_NewMap(void);

extern int reinit_surfcache;
extern qboolean r_cache_thrash;

int
D_SurfaceCacheForRes(int width, int height);
void
D_FlushCaches(void);
void
D_DeleteSurfaceCache(void);
void
D_InitCaches(void * buffer, int size);
void
R_SetVrect(vrect_t * pvrect, vrect_t * pvrectin, int lineadj);
