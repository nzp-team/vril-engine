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

#include "../../../nzportable_def.h"

extern cvar_t r_flatlightstyles;

int decal_blood1, decal_blood2, decal_blood3, decal_q3blood, decal_burn, decal_mark, decal_glow;
int zombie_skins[4];

/*
==================
R_InitOtherTextures
==================
*/
void	R_InitOtherTextures (void)
{
	/*
	//static decals
	decal_blood1  = Image_LoadImage ("textures/decals/blood_splat01", 0, 0, false, 0, false, true);
	decal_blood2  = Image_LoadImage ("textures/decals/blood_splat02", 0, 0, false, 0, false, true);
	decal_blood3  = Image_LoadImage ("textures/decals/blood_splat03", 0, 0, false, 0, false, true);
    decal_q3blood = Image_LoadImage ("textures/decals/blood_stain", 0, 0, false, 0, false, true);
	decal_burn	  = Image_LoadImage ("textures/decals/explo_burn01", 0, 0, false, 0, false, true);
	decal_mark	  = Image_LoadImage ("textures/decals/particle_burn01", 0, 0, false, 0, false, true);
	decal_glow	  = Image_LoadImage ("textures/decals/glow2", 0, 0, false, 0, false, true);
	*/

	// external zombie skins
	zombie_skins[0] = Image_LoadImage ("models/ai/zfull.mdl_0", IMAGE_PCX, 0, true, false);
	zombie_skins[1] = Image_LoadImage ("models/ai/zfull.mdl_1", IMAGE_PCX, 0, true, false);
	zombie_skins[2] = Image_LoadImage ("models/ai/zfull.mdl_2", IMAGE_PCX, 0, true, false);
	zombie_skins[3] = Image_LoadImage ("models/ai/zfull.mdl_3", IMAGE_PCX, 0, true, false);
}

/*
==================
R_InitTextures
==================
*/
void	R_InitTextures (void)
{
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

	particletexture = GL_LoadTexture("particletex", 8, 8, (byte *)data, false, true, 1, true);
}

/*
===============
R_Envmap_f

Grab six views for environment mapping tests
===============
*/
void R_Envmap_f (void)
{
	byte	buffer[256*256*4];

	glDrawBuffer  (GL_FRONT);
	glReadBuffer  (GL_FRONT);
	envmap = true;

	r_refdef.vrect.x = 0;
	r_refdef.vrect.y = 0;
	r_refdef.vrect.width = 256;
	r_refdef.vrect.height = 256;

	r_refdef.viewangles[0] = 0;
	r_refdef.viewangles[1] = 0;
	r_refdef.viewangles[2] = 0;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	glReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env0.rgb", buffer, sizeof(buffer));		

	r_refdef.viewangles[1] = 90;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	glReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env1.rgb", buffer, sizeof(buffer));		

	r_refdef.viewangles[1] = 180;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	glReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env2.rgb", buffer, sizeof(buffer));		

	r_refdef.viewangles[1] = 270;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	glReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env3.rgb", buffer, sizeof(buffer));		

	r_refdef.viewangles[0] = -90;
	r_refdef.viewangles[1] = 0;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	glReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env4.rgb", buffer, sizeof(buffer));		

	r_refdef.viewangles[0] = 90;
	r_refdef.viewangles[1] = 0;
	GL_BeginRendering (&glx, &gly, &glwidth, &glheight);
	R_RenderView ();
	glReadPixels (0, 0, 256, 256, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	COM_WriteFile ("env5.rgb", buffer, sizeof(buffer));		

	envmap = false;
	glDrawBuffer  (GL_BACK);
	glReadBuffer  (GL_BACK);
	GL_EndRendering ();
}

/*
===============
R_Init
===============
*/
void R_Init (void)
{	
	extern cvar_t gl_finish;

	Cmd_AddCommand ("timerefresh", R_TimeRefresh_f);	
	Cmd_AddCommand ("envmap", R_Envmap_f);	
	Cmd_AddCommand ("pointfile", R_ReadPointFile_f);	

	Cvar_RegisterVariable (&r_norefresh);
	Cvar_RegisterVariable (&r_lightmap);
	Cvar_RegisterVariable (&r_fullbright);
	Cvar_RegisterVariable (&r_drawentities);
	Cvar_RegisterVariable (&r_drawviewmodel);
	Cvar_RegisterVariable (&r_shadows);
	Cvar_RegisterVariable (&r_mirroralpha);
	Cvar_RegisterVariable (&r_wateralpha);
	Cvar_RegisterVariable (&r_dynamic);
	Cvar_RegisterVariable (&r_novis);
	Cvar_RegisterVariable (&r_speeds);

	Cvar_RegisterVariable (&r_farclip);

	Cvar_RegisterVariable (&gl_finish);
	Cvar_RegisterVariable (&gl_clear);
	Cvar_RegisterVariable (&gl_texsort);

	Cvar_SetValue("gl_clear", 1);

 	if (gl_mtexable)
		Cvar_SetValue ("gl_texsort", 0.0);

	Cvar_RegisterVariable (&gl_cull);
	Cvar_RegisterVariable (&gl_smoothmodels);
	Cvar_RegisterVariable (&gl_affinemodels);
	Cvar_RegisterVariable (&gl_polyblend);
	Cvar_RegisterVariable (&gl_flashblend);
	Cvar_RegisterVariable (&gl_playermip);
	Cvar_RegisterVariable (&gl_nocolors);

	Cvar_RegisterVariable (&gl_keeptjunctions);
	Cvar_RegisterVariable (&gl_reporttjunctions);

	Cvar_RegisterVariable (&gl_doubleeyes);

	Cvar_RegisterVariable (&r_explosiontype);
	Cvar_RegisterVariable (&r_laserpoint);
	Cvar_RegisterVariable (&r_part_explosions);
	Cvar_RegisterVariable (&r_part_trails);
	Cvar_RegisterVariable (&r_part_sparks);
	Cvar_RegisterVariable (&r_part_gunshots);
	Cvar_RegisterVariable (&r_part_blood);
	Cvar_RegisterVariable (&r_part_telesplash);
	Cvar_RegisterVariable (&r_part_blobs);
	Cvar_RegisterVariable (&r_part_lavasplash);
	Cvar_RegisterVariable (&r_part_flames);
	Cvar_RegisterVariable (&r_part_lightning);
	Cvar_RegisterVariable (&r_part_flies);
	Cvar_RegisterVariable (&r_part_muzzleflash);
	Cvar_RegisterVariable (&r_flametype);
	Cvar_RegisterVariable (&r_model_brightness);
	Cvar_RegisterVariable (&r_skyfog);

	Cvar_RegisterVariable (&r_flatlightstyles);

	R_InitParticles ();
	R_InitOtherTextures ();
	//R_InitParticleTexture ();

	Sky_Init (); //johnfitz
	Fog_Init (); //johnfitz

#ifdef GLTEST
	Test_Init ();
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

	GL_BuildLightmaps ();

	Sky_NewMap (); //johnfitz -- skybox in worldspawn
	Fog_ParseWorldspawn ();

	// identify sky texture
	skytexturenum = -1;
	mirrortexturenum = -1;
	for (i=0 ; i<cl.worldmodel->numtextures ; i++)
	{
		if (!cl.worldmodel->textures[i])
			continue;
		if (!strncmp(cl.worldmodel->textures[i]->name,"sky",3) )
			skytexturenum = i;
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
void R_TimeRefresh_f (void)
{
	int			i;
	float		start, stop, time;

	glDrawBuffer  (GL_FRONT);
	glFinish ();

	start = Sys_FloatTime ();
	for (i=0 ; i<128 ; i++)
	{
		r_refdef.viewangles[1] = i/128.0*360.0;
		R_RenderView ();
	}

	glFinish ();
	stop = Sys_FloatTime ();
	time = stop-start;
	Con_Printf ("%f seconds (%f fps)\n", (double)time, (double)128/(double)time);

	glDrawBuffer  (GL_BACK);
	GL_EndRendering ();
}

void D_FlushCaches (void)
{
}


