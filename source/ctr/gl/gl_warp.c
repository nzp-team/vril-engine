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
// gl_warp.c -- sky and water polygons

#include "../../quakedef.h"

extern	model_t	*loadmodel;

int		skytexturenum;

int		solidskytexture;
int		alphaskytexture;
float	speedscale;		// for top sky and bottom sky

int	    skytexorder[5] = {0,2,1,3,4};
int	    skyimage[5]; // Where sky images are stored
char	skybox_name[32] = ""; //name of current skybox, or "" if no skybox
// cut off down for half skybox
char	*suf[5] = {"rt", "bk", "lf", "ft", "up" };

msurface_t	*warpface;

extern cvar_t gl_subdivide_size;

void BoundPoly (int numverts, float *verts, vec3_t mins, vec3_t maxs)
{
	int		i, j;
	float	*v;

	mins[0] = mins[1] = mins[2] = 9999;
	maxs[0] = maxs[1] = maxs[2] = -9999;
	v = verts;
	for (i=0 ; i<numverts ; i++)
		for (j=0 ; j<3 ; j++, v++)
		{
			if (*v < mins[j])
				mins[j] = *v;
			if (*v > maxs[j])
				maxs[j] = *v;
		}
}

void SubdividePolygon (int numverts, float *verts)
{
	int		i, j, k;
	vec3_t	mins, maxs;
	float	m;
	float	*v;
	vec3_t	front[64], back[64];
	int		f, b;
	float	dist[64];
	float	frac;
	glpoly_t	*poly;
	float	s, t;

	if (numverts > 60)
		Sys_Error ("numverts = %i", numverts);

	BoundPoly (numverts, verts, mins, maxs);

	for (i=0 ; i<3 ; i++)
	{
		m = (mins[i] + maxs[i]) * 0.5;
		m = gl_subdivide_size.value * floor (m/gl_subdivide_size.value + 0.5);
		if (maxs[i] - m < 8)
			continue;
		if (m - mins[i] < 8)
			continue;

		// cut it
		v = verts + i;
		for (j=0 ; j<numverts ; j++, v+= 3)
			dist[j] = *v - m;

		// wrap cases
		dist[j] = dist[0];
		v-=i;
		VectorCopy (verts, v);

		f = b = 0;
		v = verts;
		for (j=0 ; j<numverts ; j++, v+= 3)
		{
			if (dist[j] >= 0)
			{
				VectorCopy (v, front[f]);
				f++;
			}
			if (dist[j] <= 0)
			{
				VectorCopy (v, back[b]);
				b++;
			}
			if (dist[j] == 0 || dist[j+1] == 0)
				continue;
			if ( (dist[j] > 0) != (dist[j+1] > 0) )
			{
				// clip point
				frac = dist[j] / (dist[j] - dist[j+1]);
				for (k=0 ; k<3 ; k++)
					front[f][k] = back[b][k] = v[k] + frac*(v[3+k] - v[k]);
				f++;
				b++;
			}
		}

		SubdividePolygon (f, front[0]);
		SubdividePolygon (b, back[0]);
		return;
	}

	poly = Hunk_Alloc (sizeof(glpoly_t) + (numverts-4) * VERTEXSIZE*sizeof(float));
	poly->next = warpface->polys;
	warpface->polys = poly;
	poly->numverts = numverts;
	for (i=0 ; i<numverts ; i++, verts+= 3)
	{
		VectorCopy (verts, poly->verts[i]);
		s = DotProduct (verts, warpface->texinfo->vecs[0]);
		t = DotProduct (verts, warpface->texinfo->vecs[1]);
		poly->verts[i][3] = s;
		poly->verts[i][4] = t;
	}
}

/*
================
GL_SubdivideSurface

Breaks a polygon up along axial 64 unit
boundaries so that turbulent and sky warps
can be done reasonably.
================
*/
void GL_SubdivideSurface (msurface_t *fa)
{
	vec3_t		verts[64];
	int			numverts;
	int			i;
	int			lindex;
	float		*vec;
	texture_t	*t;

	warpface = fa;

	//
	// convert edges back to a normal polygon
	//
	numverts = 0;
	for (i=0 ; i<fa->numedges ; i++)
	{
		lindex = loadmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
			vec = loadmodel->vertexes[loadmodel->edges[lindex].v[0]].position;
		else
			vec = loadmodel->vertexes[loadmodel->edges[-lindex].v[1]].position;
		VectorCopy (vec, verts[numverts]);
		numverts++;
	}

	SubdividePolygon (numverts, verts[0]);
}

//=========================================================



// speed up sin calculations - Ed
float	turbsin[] =
{
	#include "gl_warp_sin.h"
};
#define TURBSCALE (256.0 / (2 * M_PI))

/*
=============
EmitWaterPolys

Does a water warp on the pre-fragmented glpoly_t chain
=============
*/
void EmitWaterPolys (msurface_t *fa)
{
	glpoly_t	*p;
	float		*v;
	int			i;
	float		s, t, os, ot;


	for (p=fa->polys ; p ; p=p->next)
	{
		glBegin (GL_POLYGON);
		for (i=0,v=p->verts[0] ; i<p->numverts ; i++, v+=VERTEXSIZE)
		{
			os = v[3];
			ot = v[4];

			s = os + turbsin[(int)((ot*0.125+realtime) * TURBSCALE) & 255];
			s *= (1.0/64);

			t = ot + turbsin[(int)((os*0.125+realtime) * TURBSCALE) & 255];
			t *= (1.0/64);

			glTexCoord2f (s, t);
			glVertex3fv (v);
		}
		glEnd ();
	}
}




/*
=============
EmitSkyPolys
=============
*/
void EmitSkyPolys (msurface_t *fa)
{
	glpoly_t	*p;
	float		*v;
	int			i;
	float	s, t;
	vec3_t	dir;
	float	length;

	for (p=fa->polys ; p ; p=p->next)
	{
		glBegin (GL_POLYGON);
		for (i=0,v=p->verts[0] ; i<p->numverts ; i++, v+=VERTEXSIZE)
		{
			VectorSubtract (v, r_origin, dir);
			dir[2] *= 3;	// flatten the sphere

			length = dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2];
			length = sqrt (length);
			length = 6*63/length;

			dir[0] *= length;
			dir[1] *= length;

			s = (speedscale + dir[0]) * (1.0/128);
			t = (speedscale + dir[1]) * (1.0/128);

			glTexCoord2f (s, t);
			glVertex3fv (v);
		}
		glEnd ();
	}
}

/*
===============
EmitBothSkyLayers

Does a sky warp on the pre-fragmented glpoly_t chain
This will be called for brushmodels, the world
will have them chained together.
===============
*/
void EmitBothSkyLayers (msurface_t *fa)
{
	int			i;
	int			lindex;
	float		*vec;

	GL_DisableMultitexture();

	GL_Bind (solidskytexture);
	speedscale = realtime*8;
	speedscale -= (int)speedscale & ~127 ;

	EmitSkyPolys (fa);

	glEnable (GL_BLEND);
	GL_Bind (alphaskytexture);
	speedscale = realtime*16;
	speedscale -= (int)speedscale & ~127 ;

	EmitSkyPolys (fa);

	glDisable (GL_BLEND);
}

#ifndef QUAKE2
/*
=================
R_DrawSkyChain
=================
*/
void R_DrawSkyChain (msurface_t *s)
{
	msurface_t	*fa;

	GL_DisableMultitexture();

	// used when gl_texsort is on
	GL_Bind(solidskytexture);
	speedscale = realtime*8;
	speedscale -= (int)speedscale & ~127 ;

	for (fa=s ; fa ; fa=fa->texturechain)
		EmitSkyPolys (fa);

	glEnable (GL_BLEND);
	GL_Bind (alphaskytexture);
	speedscale = realtime*16;
	speedscale -= (int)speedscale & ~127 ;

	for (fa=s ; fa ; fa=fa->texturechain)
		EmitSkyPolys (fa);

	glDisable (GL_BLEND);
}

#endif

/*
=================================================================

  Quake 2 environment sky

=================================================================
*/

/*
==================
Sky_LoadSkyBox
==================
*/
//char	*suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};
void Sky_LoadSkyBox(char* name)
{
	if (strcmp(skybox_name, name) == 0)
		return; //no change

	//turn off skybox if sky is set to ""
	if (name[0] == '0') {
		skybox_name[0] = 0;
		return;
	}

	// Do sides one way and top another, bottom is not done
    for (int i = 0; i < 4; i++)
    {
        int mark = Hunk_LowMark ();

		if(!(skyimage[i] = loadtextureimage (va("gfx/env/%s%s", name, suf[i]), 0, 0, false, false)) &&
           !(skyimage[i] = loadtextureimage (va("gfx/env/%s_%s", name, suf[i]), 0, 0, false, false)))
		{
			Con_Printf("Sky: %s[%s] not found, used std\n", name, suf[i]);
		    if(!(skyimage[i] = loadtextureimage (va("gfx/env/skybox%s", suf[i]), 0, 0, false, false)))
		    {
			    Sys_Error("STD SKY NOT FOUND!");
			}

		}
        Hunk_FreeToLowMark (mark);
    }

	int mark = Hunk_LowMark ();
	if(!(skyimage[4] = loadtextureimage (va("gfx/env/%sup", name), 0, 0, false, false)) &&
		!(skyimage[4] = loadtextureimage (va("gfx/env/%s_up", name), 0, 0, false, false)))
	{
		Con_Printf("Sky: %s[%s] not found, used std\n", name, suf[4]);
		if(!(skyimage[4] = loadtextureimage (va("gfx/env/skybox%s", suf[4]), 0, 0, false, false)))
		{
			Sys_Error("STD SKY NOT FOUND!");
		}

	}
	Hunk_FreeToLowMark (mark);

	strcpy(skybox_name, name);
}

/*
=================
Sky_NewMap
=================
*/
void Sky_NewMap (void)
{
	char	key[128], value[4096];
	char	*data;

    //purge old sky textures
    //UnloadSkyTexture ();

	//
	// initially no sky
	//
	Sky_LoadSkyBox (""); //not used

	//
	// read worldspawn (this is so ugly, and shouldn't it be done on the server?)
	//
	data = cl.worldmodel->entities;
	if (!data)
		return; //FIXME: how could this possibly ever happen? -- if there's no
	// worldspawn then the sever wouldn't send the loadmap message to the client

	data = COM_Parse(data);

	if (!data) //should never happen
		return; // error

	if (com_token[0] != '{') //should never happen
		return; // error

	while (1)
	{
		data = COM_Parse(data);

		if (!data)
			return; // error

		if (com_token[0] == '}')
			break; // end of worldspawn

		if (com_token[0] == '_')
			strcpy(key, com_token + 1);
		else
			strcpy(key, com_token);
		while (key[strlen(key)-1] == ' ') // remove trailing spaces
			key[strlen(key)-1] = 0;

		data = COM_Parse(data);
		if (!data)
			return; // error

		strcpy(value, com_token);

        if (!strcmp("sky", key))
            Sky_LoadSkyBox(value);
        else if (!strcmp("skyname", key)) //half-life
            Sky_LoadSkyBox(value);
        else if (!strcmp("qlsky", key)) //quake lives
            Sky_LoadSkyBox(value);
	}
}

/*
=================
Sky_SkyCommand_f
=================
*/
void Sky_SkyCommand_f (void)
{
	switch (Cmd_Argc())
	{
	case 1:
		Con_Printf("\"sky\" is \"%s\"\n", skybox_name);
		break;
	case 2:
		Sky_LoadSkyBox(Cmd_Argv(1));
		break;
	default:
		Con_Printf("usage: sky <skyname>\n");
	}
}

/*
=============
Sky_Init
=============
*/
void Sky_Init (void)
{
	int		i;

	Cmd_AddCommand ("sky",Sky_SkyCommand_f);

	for (i=0; i<5; i++)
		skyimage[i] = NULL;
}

static vec3_t	skyclip[6] = {
	{1,1,0},
	{1,-1,0},
	{0,-1,1},
	{0,1,1},
	{1,0,1},
	{-1,0,1}
};
int	c_sky;

// 1 = s, 2 = t, 3 = 2048
static int	st_to_vec[6][3] =
{
	{3,-1,2},
	{-3,1,2},

	{1,3,2},
	{-1,-3,2},

	{-2,-1,3},		// 0 degrees yaw, look straight up
	{2,-1,-3}		// look straight down

//	{-1,2,3},
//	{1,2,-3}
};

// s = [0]/[2], t = [1]/[2]
static int	vec_to_st[6][3] =
{
	{-2,3,1},
	{2,3,-1},

	{1,3,2},
	{-1,3,-2},

	{-2,-1,3},
	{-2,1,-3}

//	{-1,2,3},
//	{1,2,-3}
};

static float	skymins[2][6], skymaxs[2][6];

/*
==============
R_ClearSkyBox
==============
*/
void R_ClearSkyBox (void)
{
	int		i;

	for (i=0 ; i<5 ; i++)
	{
		skymins[0][i] = skymins[1][i] = 9999;
		skymaxs[0][i] = skymaxs[1][i] = -9999;
	}
}


void MakeSkyVec (float s, float t, int axis)
{
	vec3_t		v, b;
	int			j, k;

	b[0] = s*2048;
	b[1] = t*2048;
	b[2] = 2048;

	for (j=0 ; j<3 ; j++)
	{
		k = st_to_vec[axis][j];
		if (k < 0)
			v[j] = -b[-k - 1];
		else
			v[j] = b[k - 1];
		v[j] += r_origin[j];
	}

	// avoid bilerp seam
	s = (s+1)*0.5;
	t = (t+1)*0.5;

	if (s < 1.0/512)
		s = 1.0/512;
	else if (s > 511.0/512)
		s = 511.0/512;
	if (t < 1.0/512)
		t = 1.0/512;
	else if (t > 511.0/512)
		t = 511.0/512;

	t = 1.0 - t;
	glTexCoord2f (s, t);
	glVertex3fv (v);
}

/*
==============
R_DrawSkyBox
==============
*/

float skynormals[5][3] = {
	{ 1.f, 0.f, 0.f },
	{ -1.f, 0.f, 0.f },
	{ 0.f, 1.f, 0.f },
	{ 0.f, -1.f, 0.f },
	{ 0.f, 0.f, 1.f }
};

float skyrt[5][3] = {
	{ 0.f, -1.f, 0.f },
	{ 0.f, 1.f, 0.f },
	{ 1.f, 0.f, 0.f },
	{ -1.f, 0.f, 0.f },
	{ 0.f, -1.f, 0.f }
};

float skyup[5][3] = {
	{ 0.f, 0.f, 1.f },
	{ 0.f, 0.f, 1.f },
	{ 0.f, 0.f, 1.f },
	{ 0.f, 0.f, 1.f },
	{ -1.f, 0.f, 0.f }
};

void R_DrawSkyBox (void)
{
	int		i, j, k;
	vec3_t	v;
	float	s, t;

	//Fog_DisableGFog();
	//Fog_SetColorForSkyS();

	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glDepthMask(GL_FALSE);
	glDisable(GL_DEPTH_TEST);

	float skydepth = 1000.0f;

	for (i=0 ; i<5 ; i++)
	{
		const int vertex_count = 4;
		glvert_t sky_vertices[vertex_count];

		// check if poly needs to be drawn at all
		float dot = DotProduct(skynormals[i], vpn);
		// < 0 check would work at fov 90 or less, just guess a value that's high enough?
		if (dot < -0.25f) continue;

		GL_Bind(skyimage[skytexorder[i]]);

		// if direction is not up, cut "down" vector to zero to only render half cube
		//float upnegfact = i == 4 ? 1.0f : 0.0f;
		float upnegfact = 1.0f;

		float skyboxtexsize = 256.f;
		// move ever so slightly less towards forward to make edges overlap a bit, just to not have shimmering pixels between sky edges
		float forwardfact = 0.99f;

		glBegin(GL_QUADS);

		sky_vertices[0].s = 0.5f / skyboxtexsize;
		sky_vertices[0].t = (skyboxtexsize - .5f) / skyboxtexsize;
		sky_vertices[0].x = r_origin[0] + (forwardfact * skynormals[i][0] - skyrt[i][0] - skyup[i][0] * upnegfact) * skydepth;
		sky_vertices[0].y = r_origin[1] + (forwardfact * skynormals[i][1] - skyrt[i][1] - skyup[i][1] * upnegfact) * skydepth;
		sky_vertices[0].z = r_origin[2] + (forwardfact * skynormals[i][2] - skyrt[i][2] - skyup[i][2] * upnegfact) * skydepth;
		v[0] = sky_vertices[0].x;
		v[1] = sky_vertices[0].y;
		v[2] = sky_vertices[0].z;
		glTexCoord2f (sky_vertices[0].s, sky_vertices[0].t);
		glVertex3fv (v);

		sky_vertices[1].s = 0.5f / skyboxtexsize;
		sky_vertices[1].t = 0.5f / skyboxtexsize;
		sky_vertices[1].x = r_origin[0] + (forwardfact * skynormals[i][0] - skyrt[i][0] + skyup[i][0]) * skydepth;
		sky_vertices[1].y = r_origin[1] + (forwardfact * skynormals[i][1] - skyrt[i][1] + skyup[i][1]) * skydepth;
		sky_vertices[1].z = r_origin[2] + (forwardfact * skynormals[i][2] - skyrt[i][2] + skyup[i][2]) * skydepth;
		v[0] = sky_vertices[1].x;
		v[1] = sky_vertices[1].y;
		v[2] = sky_vertices[1].z;
		glTexCoord2f (sky_vertices[1].s, sky_vertices[1].t);
		glVertex3fv (v);

		sky_vertices[2].s = (skyboxtexsize - .5f) / skyboxtexsize;
		sky_vertices[2].t = 0.5f / skyboxtexsize;
		sky_vertices[2].x = r_origin[0] + (forwardfact * skynormals[i][0] + skyrt[i][0] + skyup[i][0]) * skydepth;
		sky_vertices[2].y = r_origin[1] + (forwardfact * skynormals[i][1] + skyrt[i][1] + skyup[i][1]) * skydepth;
		sky_vertices[2].z = r_origin[2] + (forwardfact * skynormals[i][2] + skyrt[i][2] + skyup[i][2]) * skydepth;
		v[0] = sky_vertices[2].x;
		v[1] = sky_vertices[2].y;
		v[2] = sky_vertices[2].z;
		glTexCoord2f (sky_vertices[2].s, sky_vertices[2].t);
		glVertex3fv (v);

		sky_vertices[3].s = (skyboxtexsize - .5f) / skyboxtexsize;
		sky_vertices[3].t = (skyboxtexsize - .5f) / skyboxtexsize;
		sky_vertices[3].x = r_origin[0] + (forwardfact * skynormals[i][0] + skyrt[i][0] - skyup[i][0] * upnegfact) * skydepth;
		sky_vertices[3].y = r_origin[1] + (forwardfact * skynormals[i][1] + skyrt[i][1] - skyup[i][1] * upnegfact) * skydepth;
		sky_vertices[3].z = r_origin[2] + (forwardfact * skynormals[i][2] + skyrt[i][2] - skyup[i][2] * upnegfact) * skydepth;
		v[0] = sky_vertices[3].x;
		v[1] = sky_vertices[3].y;
		v[2] = sky_vertices[3].z;
		glTexCoord2f (sky_vertices[3].s, sky_vertices[3].t);
		glVertex3fv (v);

		glEnd();
	}

	glDepthMask(GL_TRUE);
	glEnable(GL_DEPTH_TEST);

	//Fog_SetColorForSkyE(); //setup for Sky
	//Fog_EnableGFog(); //setup for Sky
}

//===============================================================

/*
=============
R_InitSky

A sky texture is 256*128, with the right side being a masked overlay
==============
*/
void R_InitSky (miptex_t *mt)
{
	int			i, j, p;
	byte		*src;
	unsigned	trans[128*128];
	unsigned	transpix;
	int			r, g, b;
	unsigned	*rgba;
	extern	int			skytexturenum;

	src = (byte *)mt + mt->offsets[0];

	// make an average value for the back to avoid
	// a fringe on the top level

	r = g = b = 0;
	for (i=0 ; i<128 ; i++)
		for (j=0 ; j<128 ; j++)
		{
			p = src[i*256 + j + 128];
			rgba = &d_8to24table[p];
			trans[(i*128) + j] = *rgba;
			r += ((byte *)rgba)[0];
			g += ((byte *)rgba)[1];
			b += ((byte *)rgba)[2];
		}

	((byte *)&transpix)[0] = r/(128*128);
	((byte *)&transpix)[1] = g/(128*128);
	((byte *)&transpix)[2] = b/(128*128);
	((byte *)&transpix)[3] = 0;


	if (!solidskytexture)
		solidskytexture = texture_extension_number++;
	GL_Bind (solidskytexture );
	glTexImage2D (GL_TEXTURE_2D, 0, gl_solid_format, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);


	for (i=0 ; i<128 ; i++)
		for (j=0 ; j<128 ; j++)
		{
			p = src[i*256 + j];
			if (p == 0)
				trans[(i*128) + j] = transpix;
			else
				trans[(i*128) + j] = d_8to24table[p];
		}

	if (!alphaskytexture)
		alphaskytexture = texture_extension_number++;
	GL_Bind(alphaskytexture);
	glTexImage2D (GL_TEXTURE_2D, 0, gl_alpha_format, 128, 128, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

