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
// r_misc.c

#include "../nzportable_def.h"
#include "r_config.h"

extern cvar_t r_flatlightstyles, gl_finish;
void R_ToggleParticles_f(void);
void R_ToggleDecals_f(void);

int zombie_skins[NUM_ZOMBIE_SKINS];

/*
==================
R_InitOtherTextures
==================
*/
void R_InitOtherTextures(void)
{
    size_t budget = Hyena_TextureBudget();
    for (int i = 0; i < NUM_ZOMBIE_SKINS; i++) {
        if (i && budget && budget < 2 * 1024 * 1024)
            zombie_skins[i] = zombie_skins[0];
        else if (Hyena_PreferredPaletteBits() == 4)
            zombie_skins[i] = loadpcxas4bpp(va("models/ai/zfull.mdl_%i", i), HYE_FILTER_LINEAR);
        else
            zombie_skins[i] = Image_LoadImage(va("models/ai/zfull.mdl_%i", i), IMAGE_PCX, 0, true, false);
        Hyena_KeepTexture(zombie_skins[i]);
    }
}

/*
==================
R_InitTextures
==================
*/
void	R_InitTextures (void)
{
    R_InitCvars();
    Hyena_InitTextureStorage();
	int		x,y, m;
	byte	*dest;

// create a simple checkerboard texture for the default
	r_notexture_mip = Hunk_AllocName (sizeof(texture_t) + 16*16+8*8+4*4+2*2, "notexture");
	
	r_notexture_mip->width = r_notexture_mip->height = 16;
	r_notexture_mip->offsets[0] = sizeof(texture_t);
	r_notexture_mip->offsets[1] = r_notexture_mip->offsets[0] + 16*16;
	r_notexture_mip->offsets[2] = r_notexture_mip->offsets[1] + 8*8;
	r_notexture_mip->offsets[3] = r_notexture_mip->offsets[2] + 4*4;
	
	for (m=0 ; m<4 ; m++)
	{
		dest = (byte *)r_notexture_mip + r_notexture_mip->offsets[m];
		for (y=0 ; y< (16>>m) ; y++)
			for (x=0 ; x< (16>>m) ; x++)
			{
				if (  (y< (8>>m) ) ^ (x< (8>>m) ) )
					*dest++ = 0;
				else
					*dest++ = 0xff;
			}
	}	
}

byte	dottexture[8][8] =
{
	{0,1,1,0,0,0,0,0},
	{1,1,1,1,0,0,0,0},
	{1,1,1,1,0,0,0,0},
	{0,1,1,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
	{0,0,0,0,0,0,0,0},
};
void R_InitParticleTexture (void)
{
	int		x,y;
	byte	data[8][8];

	//
	// particle texture
	//
	for (x=0 ; x<8 ; x++)
	{
		for (y=0 ; y<8 ; y++)
		{
			data[y][x] = dottexture[x][y] ? 15 : 255; // ELUTODO assumes 15 is white
		}
	}

	particletexture = Image_LoadTexture("particletex", 8, 8, (byte *)data, HYE_TEXTURE_INDEX8, HYE_FILTER_LINEAR, 0, true, true, false);
}

/*
===============
R_Envmap_f

Grab six views for environment mapping tests
===============
*/
void R_Envmap_f(void)
{
    byte *buffer = malloc(256 * 256 * 4);
    int face;
    if (!buffer) {
        Con_Printf("Out of environment capture memory\n");
        return;
    }
    Hyena_SelectDrawBuffer(true);
    Hyena_SelectReadBuffer(true);
    envmap = true;
    r_refdef.vrect.x = r_refdef.vrect.y = 0;
    r_refdef.vrect.width = r_refdef.vrect.height = 256;
    for (face = 0; face < 6; face++) {
        r_refdef.viewangles[PITCH] = face == 4 ? -90 : face == 5 ? 90 : 0;
        r_refdef.viewangles[YAW] = face < 4 ? face * 90 : 0;
        r_refdef.viewangles[ROLL] = 0;
        GL_BeginRendering(&glx, &gly, &glwidth, &glheight);
        R_RenderView();
        Hyena_Finish();
        if (!Hyena_ReadPixels(0, 0, 256, 256, buffer)) {
            Con_Printf("Pixel capture is unavailable\n");
            break;
        }
        COM_WriteFile(va("env%i.rgb", face), buffer, 256 * 256 * 4);
    }
    envmap = false;
    Hyena_SelectDrawBuffer(false);
    Hyena_SelectReadBuffer(false);
    GL_EndRendering();
    free(buffer);
}

/*
===============
R_Init
===============
*/
void R_Init(void)
{
    R_InitFallbackTextures();
    Sky_Configure(!Hyena_TextureBudget() || Hyena_TextureBudget() >= 2 * 1024 * 1024, false, true);
    Cmd_AddCommand("timerefresh", R_TimeRefresh_f);
    Cmd_AddCommand("envmap", R_Envmap_f);
    Cmd_AddCommand("pointfile", R_ReadPointFile_f);
    Cmd_AddCommand("toggleparticles", R_ToggleParticles_f);
    Cmd_AddCommand("toggledecals", R_ToggleDecals_f);
    R_InitParticles();
    R_InitOtherTextures();
    Sky_Init();
    Fog_Init();
#ifdef GLTEST
    Test_Init();
#endif
}

/*
===============
R_NewMap
===============
*/
void R_NewMap (void)
{
	int		i;
	
	R_ClearSceneCaches();

	for (i=0 ; i<256 ; i++)
		d_lightstylevalue[i] = 264;		// normal light value

	memset (&r_worldentity, 0, sizeof(r_worldentity));
	r_worldentity.model = cl.worldmodel;

// clear out efrags in case the level hasn't been reloaded
// FIXME: is this one short?
	for (i=0 ; i<cl.worldmodel->numleafs ; i++)
		cl.worldmodel->leafs[i].efrags = NULL;
		 	
	r_viewleaf = NULL;
	R_ClearParticles ();

	R_BuildLightmaps();

	Sky_NewMap (); //johnfitz -- skybox in worldspawn
	Fog_ParseWorldspawn ();

	mirrortexturenum = -1;
	for (i=0 ; i<cl.worldmodel->numtextures ; i++)
	{
		if (!cl.worldmodel->textures[i])
			continue;
		if (!strncmp(cl.worldmodel->textures[i]->name,"window02_1",10) )
			mirrortexturenum = i;
		cl.worldmodel->textures[i]->texturechain = NULL;
	}
	//R_LoadSkys ();
}


/*
====================
R_TimeRefresh_f

For program optimization
====================
*/
void R_TimeRefresh_f(void)
{
    double start, elapsed;
    int i;
    Hyena_SelectDrawBuffer(true);
    Hyena_Finish();
    start = Sys_FloatTime();
    for (i = 0; i < 128; i++) {
        r_refdef.viewangles[YAW] = i / 128.0f * 360;
        R_RenderView();
    }
    Hyena_Finish();
    elapsed = Sys_FloatTime() - start;
    if (elapsed > 0)
        Con_Printf("%f seconds (%f fps)\n", elapsed, 128 / elapsed);
    Hyena_SelectDrawBuffer(false);
    GL_EndRendering();
}

void D_FlushCaches (void)
{
}
