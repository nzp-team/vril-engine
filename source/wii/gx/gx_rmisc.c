/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2008 Eluan Miranda

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

#include "../../nzportable_def.h"

cvar_t		gl_cshiftpercent = {"gl_cshiftpercent", "100", false};

int zombie_skins[4];
int decal_blood1, decal_blood2, decal_blood3, decal_q3blood, decal_burn, decal_mark, decal_glow;

extern cvar_t r_flatlightstyles;

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

	particletexture = GL_LoadTexture("particletex", 8, 8, (byte *)data, false, true, true, 1);
}

/*
==================
R_InitOtherTextures
==================
*/
void R_InitOtherTextures (void)
{
	
	//static decals
	decal_blood1  = loadtextureimage ("textures/decals/blood_splat01", 0, 0, true, false, true);
	decal_blood2  = loadtextureimage ("textures/decals/blood_splat02", 0, 0, true, false, true);
	decal_blood3  = loadtextureimage ("textures/decals/blood_splat03", 0, 0, true, false, true);
    decal_q3blood = loadtextureimage ("textures/decals/blood_stain", 0, 0, true, false, true);
	decal_burn	  = loadtextureimage ("textures/decals/explo_burn01", 0, 0, true, false, true);
	decal_mark	  = loadtextureimage ("textures/decals/particle_burn01", 0, 0, true, false, true);
	decal_glow	  = loadtextureimage ("textures/decals/glow2", 0, 0, true, false, true);

	// external zombie skins
	
	zombie_skins[0] = loadtextureimage ("models/ai/zfull.mdl_0", 0, 0, true, false, true);
	zombie_skins[1] = loadtextureimage ("models/ai/zfull.mdl_1", 0, 0, true, false, true);
	zombie_skins[2] = loadtextureimage ("models/ai/zfull.mdl_2", 0, 0, true, false, true);
	zombie_skins[3] = loadtextureimage ("models/ai/zfull.mdl_3", 0, 0, true, false, true);
	
}

/*
===============
R_Envmap_f

Grab six views for environment mapping tests
===============
*/
/* ELUTODO
void R_Envmap_f (void)
{
	byte	buffer[256*256*4];
	char	name[1024];

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
*/
/*
===============
R_Init
===============
*/
void R_Init (void)
{	
	extern cvar_t gl_finish;

	Cmd_AddCommand ("timerefresh", R_TimeRefresh_f);	
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
	Cvar_RegisterVariable (&r_lerpmodels);
	Cvar_RegisterVariable (&r_lerpmove);
	Cvar_RegisterVariable (&r_speeds);

	Cvar_RegisterVariable (&gl_finish);
	Cvar_RegisterVariable (&gl_clear);

	Cvar_RegisterVariable (&gl_cull);
	Cvar_RegisterVariable (&gl_smoothmodels);
	Cvar_RegisterVariable (&gl_affinemodels);
	Cvar_RegisterVariable (&gl_polyblend);
	Cvar_RegisterVariable (&gl_playermip);
	Cvar_RegisterVariable (&gl_nocolors);

	Cvar_RegisterVariable (&gl_keeptjunctions);
	Cvar_RegisterVariable (&gl_reporttjunctions);

	Cvar_RegisterVariable (&gl_doubleeyes);

	Cvar_RegisterVariable (&gl_cshiftpercent);
	
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
	Cvar_RegisterVariable (&r_skyfog);
	Cvar_RegisterVariable (&r_model_brightness);
	
	Cvar_RegisterVariable (&r_farclip);
	
	Cvar_RegisterVariable (&r_flatlightstyles);
	Cvar_RegisterVariable (&r_overbright);

	R_InitParticles ();
	R_InitDecals ();
	R_InitOtherTextures ();
	//R_InitParticleTexture ();
	
	Sky_Init (); //johnfitz
	Fog_Init (); //johnfitz

#ifdef GLTEST
	Test_Init ();
#endif

	memset(playertextures, 0xFF, sizeof(playertextures));
}

/*
===============
R_TranslatePlayerSkin

Translates a skin texture by the per-player color lookup
===============
*/
void R_TranslatePlayerSkin (int playernum)
{
	/*
	int		top, bottom;
	byte	translate[256];
	unsigned	translate32[256];
	int		i, j, s;
	model_t	*model;
	aliashdr_t *paliashdr;
	byte	*original;
	unsigned	pixels[512*256], *out;
	unsigned	scaled_width, scaled_height;
	int			inwidth, inheight;
	byte		*inrow;
	unsigned	frac, fracstep;

	//GL_DisableMultitexture();

	//top = cl.scores[playernum].colors & 0xf0;
	//bottom = (cl.scores[playernum].colors &15)<<4;
	
	top = 0xf0;
	bottom = (15)<<4;

	for (i=0 ; i<256 ; i++)
		translate[i] = i;

	for (i=0 ; i<16 ; i++)
	{
		if (top < 128)	// the artists made some backwards ranges.  sigh.
			translate[TOP_RANGE+i] = top+i;
		else
			translate[TOP_RANGE+i] = top+15-i;
				
		if (bottom < 128)
			translate[BOTTOM_RANGE+i] = bottom+i;
		else
			translate[BOTTOM_RANGE+i] = bottom+15-i;
	}

	//
	// locate the original skin pixels
	//
	currententity = &cl_entities[1+playernum];
	model = currententity->model;
	if (!model)
		return;		// player doesn't have a model yet
	if (model->type != mod_alias)
		return; // only translate skins on alias models

	paliashdr = (aliashdr_t *)Mod_Extradata (model);
	s = paliashdr->skinwidth * paliashdr->skinheight;
	if (currententity->skinnum < 0 || currententity->skinnum >= paliashdr->numskins) {
		Con_Printf("(%d): Invalid player skin #%d\n", playernum, currententity->skinnum);
		original = (byte *)paliashdr + paliashdr->texels[0];
	} else
		original = (byte *)paliashdr + paliashdr->texels[currententity->skinnum];
	if (s & 3)
		Sys_Error ("R_TranslateSkin: s&3");

	inwidth = paliashdr->skinwidth;
	inheight = paliashdr->skinheight;

	// because this happens during gameplay, do it fast
	// instead of sending it through gl_update 8

	scaled_width = gl_max_size.value < 512 ? gl_max_size.value : 512;
	scaled_height = gl_max_size.value < 256 ? gl_max_size.value : 256;

	for (i=0 ; i<256 ; i++)
		translate32[i] = d_8to24table[translate[i]];

	out = pixels;
	fracstep = inwidth*0x10000/scaled_width;
	for (i=0 ; i<scaled_height ; i++, out += scaled_width)
	{
		inrow = original + inwidth*(i*inheight/scaled_height);
		frac = fracstep >> 1;
		for (j=0 ; j<scaled_width ; j+=4)
		{
			out[j] = translate32[inrow[frac>>16]];
			frac += fracstep;
			out[j+1] = translate32[inrow[frac>>16]];
			frac += fracstep;
			out[j+2] = translate32[inrow[frac>>16]];
			frac += fracstep;
			out[j+3] = translate32[inrow[frac>>16]];
			frac += fracstep;
		}
	}

	// ELUTODO: skin changes, cache mismatches, ugly hacks
	if (playertextures[playernum] < 0 || playertextures[playernum] >= MAX_GLTEXTURES)
	{
		playertextures[playernum] = GL_LoadTexture (va("player%d_skin", playernum), scaled_width, scaled_height, (u8 *)pixels, true, true, true, 1); // HACK HACK HACK
	}

	GL_Update32 (&gltextures[playertextures[playernum]], pixels, scaled_width, scaled_height, true, true);
	*/
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
	R_ClearDecals();

	GL_BuildLightmaps ();
	
	Sky_NewMap (); //johnfitz -- skybox in worldspawn
	Fog_NewMap (); // johnfitz -- global fog in worldspawn
	
	//Fog_EnableGFog ();

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

/* ELUTODO
	glDrawBuffer  (GL_FRONT);
	glFinish ();
*/

	start = Sys_FloatTime ();
	for (i=0 ; i<128 ; i++)
	{
		r_refdef.viewangles[1] = i/128.0*360.0;
		R_RenderView ();
	}

	// ELUTODO glFinish ();
	stop = Sys_FloatTime ();
	time = stop-start;
	Con_Printf ("%f seconds (%f fps)\n", time, 128/time);

	// ELUTODO glDrawBuffer  (GL_BACK);
	//GL_EndRendering ();
}