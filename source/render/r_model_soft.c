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
// models.c -- model loading and caching

// models are the only shared resource between a client and server running
// on the same machine.

#include "../nzportable_def.h"
extern int r_pixbytes;
extern unsigned short d_8to16table[256];
void R_InitSky(texture_t *texture);

// FIXME: add support for wad3 individual palettes (and pcx lol)
static byte converted_pixels[MAX_SINGLE_PLANE_PIXEL_SIZE];

#include "r_model_local.h"

qboolean Mod_BuildAliasCache(const char *name)
{
    (void)name;
    return false;
}

#define MAXALIASVERTS 2000
#define ALIAS_BASE_SIZE_RATIO (1.0 / 11.0)
texture_t *Mod_CreateTexture(miptex_t *mt)
{
    int j, pixels;
    texture_t *tx;
	if ( (mt->width & 15) || (mt->height & 15) )
		Sys_Error ("Texture %s is not 16 aligned", mt->name);
	pixels = mt->width*mt->height/64*85;

	if (pixels > MAX_SINGLE_PLANE_PIXEL_SIZE) 
	{
		Con_Printf ("Texture %s is too large (%d bytes)\n", mt->name, pixels);
		return NULL;
	}

	tx = Hunk_AllocName (sizeof(texture_t) +pixels, loadname );


	memcpy (tx->name, mt->name, sizeof(tx->name));
	tx->width = mt->width;
	tx->height = mt->height;
	for (j=0 ; j<MIPLEVELS ; j++)
		tx->offsets[j] = mt->offsets[j] + sizeof(texture_t) - sizeof(miptex_t);

	if (loadmodel->bspversion == HL_BSPVERSION) 
	{
		byte *data;

		Con_DPrintf("Loading texture %s\n", tx->name);
		data = WAD3_LoadTexture(mt);

		memset(converted_pixels, 0, pixels);

		if (data) {

			unsigned converted_counter = 0;
			for (unsigned i = 0; i < mt->width * mt->height * 4; i+= 4) {
				converted_pixels[converted_counter] = findclosestpalmatch(data[i], data[i + 1], data[i + 2], data[i + 3]);
				converted_counter++;
			}

			Con_DPrintf("Total: %d  Dim: %d  Diff: %d\n", pixels, mt->width * mt->height, pixels - (mt->width * mt->height));

			// Fill remaining pixels from start
			//for (int i = 0; i < pixels - (mt->width * mt->height); i++) {
			//	converted_pixels[converted_counter] = converted_pixels[converted_counter - (mt->width * mt->height)];
			//	converted_counter++;
			//}

			for (unsigned i = 0; i < mt->height/2; i++) { // 64x64 projection
				for (unsigned j = 0; j < mt->width; j++) {
					//Con_Printf("index: %d\n", i * (mt->width) + j * (mt->width / 64));
					converted_pixels[converted_counter] = converted_pixels[((i * (mt->width) + j * (mt->width / 64)) << 1)];
					converted_counter++;
					converted_pixels[converted_counter] = converted_pixels[((i * (mt->width) + j * (mt->width / 64)) << 1)];
					converted_counter++;
				}
			}
			for (unsigned i = 0; i < mt->height/4; i++) { // 32x32 projection
				for (unsigned j = 0; j < mt->width/2; j++) {
					converted_pixels[converted_counter] = converted_pixels[((i * (mt->width) + j * (mt->width / 32)) << 1)];
					converted_counter++;
					converted_pixels[converted_counter] = converted_pixels[((i * (mt->width) + j * (mt->width / 32)) << 1)];
					converted_counter++;
				}
			}

			for (unsigned i = 0; i < mt->height/8; i++) { // 16x16 projection 
				for (unsigned j = 0; j < mt->width/4; j++) {
					converted_pixels[converted_counter] = converted_pixels[(i * (mt->width) + j * (mt->width / 16)) << 1];
					converted_counter++;
					converted_pixels[converted_counter] = converted_pixels[(i * (mt->width) + j * (mt->width / 16)) << 1];
					converted_counter++;
				}
			}
		}

		memcpy(tx+1,converted_pixels,pixels);

		//free(converted_pixels); 

		free(data);


		//memcpy ( tx+1, mt+1, pixels);
		//tx->paletted = false;
	}
	else
	{ 
		memcpy ( tx+1, mt+1, pixels);
		//tx->paletted = true;
	}
	
	if (!Q_strncmp(mt->name,"sky",3))	
		R_InitSky (tx);

    return tx;
}

void Mod_LoadModelLighting (lump_t *l)
{
	if (!l->filelen)
	{
		loadmodel->lightdata = NULL;
		return;
	}
	loadmodel->lightdata = Hunk_AllocName ( l->filelen, loadname);	
	memcpy (loadmodel->lightdata, Mod_LumpData(l), l->filelen);
}
void * Mod_LoadAliasSkin (void * pin, int *pskinindex, int skinsize,
	aliashdr_t *pheader)
{
	int		i;
	byte	*pskin, *pinskin;
	unsigned short	*pusskin;

	pskin = Hunk_AllocName (skinsize * r_pixbytes, loadname);
	pinskin = (byte *)pin;
	*pskinindex = (byte *)pskin - (byte *)pheader;

	if (r_pixbytes == 1)
	{
		Q_memcpy (pskin, pinskin, skinsize);
	}
	else if (r_pixbytes == 2)
	{
		pusskin = (unsigned short *)pskin;

		for (i=0 ; i<skinsize ; i++)
			pusskin[i] = d_8to16table[pinskin[i]];
	}
	else
	{
		Sys_Error ("Mod_LoadAliasSkin: driver set invalid r_pixbytes: %d\n",
				 r_pixbytes);
	}

	pinskin += skinsize;

	return ((void *)pinskin);
}
void * Mod_LoadAliasSkinGroup (void * pin, int *pskinindex, int skinsize,
	aliashdr_t *pheader)
{
	daliasskingroup_t		*pinskingroup;
	maliasskingroup_t		*paliasskingroup;
	int						i, numskins;
	daliasskininterval_t	*pinskinintervals;
	float					*poutskinintervals;
	void					*ptemp;

	pinskingroup = (daliasskingroup_t *)pin;

	numskins = LittleLong (pinskingroup->numskins);

	paliasskingroup = Hunk_AllocName (sizeof (maliasskingroup_t) +
			(numskins - 1) * sizeof (paliasskingroup->skindescs[0]),
			loadname);

	paliasskingroup->numskins = numskins;

	*pskinindex = (byte *)paliasskingroup - (byte *)pheader;

	pinskinintervals = (daliasskininterval_t *)(pinskingroup + 1);

	poutskinintervals = Hunk_AllocName (numskins * sizeof (float),loadname);

	paliasskingroup->intervals = (byte *)poutskinintervals - (byte *)pheader;

	for (i=0 ; i<numskins ; i++)
	{
		*poutskinintervals = LittleFloat (pinskinintervals->interval);
		if (*poutskinintervals <= 0)
			Sys_Error ("Mod_LoadAliasSkinGroup: interval<=0");

		poutskinintervals++;
		pinskinintervals++;
	}

	ptemp = (void *)pinskinintervals;

	for (i=0 ; i<numskins ; i++)
	{
		ptemp = Mod_LoadAliasSkin (ptemp,
				&paliasskingroup->skindescs[i].skin, skinsize, pheader);
	}

	return ptemp;
}
void * Mod_LoadAliasFrame (void * pin, int *pframeindex, int numv,
	trivertx_t *pbboxmin, trivertx_t *pbboxmax, aliashdr_t *pheader, char *name)
{
	trivertx_t		*pframe, *pinframe;
	int				i, j;
	daliasframe_t	*pdaliasframe;

	pdaliasframe = (daliasframe_t *)pin;

	strcpy (name, pdaliasframe->name);

	for (i=0 ; i<3 ; i++)
	{
	// these are byte values, so we don't have to worry about
	// endianness
		pbboxmin->v[i] = pdaliasframe->bboxmin.v[i];
		pbboxmax->v[i] = pdaliasframe->bboxmax.v[i];
	}

	pinframe = (trivertx_t *)(pdaliasframe + 1);
	pframe = Hunk_AllocName (numv * sizeof(*pframe), loadname);

	*pframeindex = (byte *)pframe - (byte *)pheader;

	for (j=0 ; j<numv ; j++)
	{
		int		k;

	// these are all byte values, so no need to deal with endianness
		pframe[j].lightnormalindex = pinframe[j].lightnormalindex;

		for (k=0 ; k<3 ; k++)
		{
			pframe[j].v[k] = pinframe[j].v[k];
		}
	}

	pinframe += numv;

	return (void *)pinframe;
}
void * Mod_LoadAliasGroup (void * pin, int *pframeindex, int numv,
	trivertx_t *pbboxmin, trivertx_t *pbboxmax, aliashdr_t *pheader, char *name)
{
	daliasgroup_t		*pingroup;
	maliasgroup_t		*paliasgroup;
	int					i, numframes;
	daliasinterval_t	*pin_intervals;
	float				*poutintervals;
	void				*ptemp;
	
	pingroup = (daliasgroup_t *)pin;

	numframes = LittleLong (pingroup->numframes);

	paliasgroup = Hunk_AllocName (sizeof (maliasgroup_t) +
			(numframes - 1) * sizeof (paliasgroup->frames[0]), loadname);

	paliasgroup->numframes = numframes;

	for (i=0 ; i<3 ; i++)
	{
	// these are byte values, so we don't have to worry about endianness
		pbboxmin->v[i] = pingroup->bboxmin.v[i];
		pbboxmax->v[i] = pingroup->bboxmax.v[i];
	}

	*pframeindex = (byte *)paliasgroup - (byte *)pheader;

	pin_intervals = (daliasinterval_t *)(pingroup + 1);

	poutintervals = Hunk_AllocName (numframes * sizeof (float), loadname);

	paliasgroup->intervals = (byte *)poutintervals - (byte *)pheader;

	for (i=0 ; i<numframes ; i++)
	{
		*poutintervals = LittleFloat (pin_intervals->interval);
		if (*poutintervals <= 0.0)
			Sys_Error ("Mod_LoadAliasGroup: interval<=0");

		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void *)pin_intervals;

	for (i=0 ; i<numframes ; i++)
	{
		ptemp = Mod_LoadAliasFrame (ptemp,
									&paliasgroup->frames[i].frame,
									numv,
									&paliasgroup->frames[i].bboxmin,
									&paliasgroup->frames[i].bboxmax,
									pheader, name);
	}

	return ptemp;
}
void Mod_LoadAliasModel (model_t *mod, void *buffer)
{
	int					i;
	mdl_t				*pmodel, *pinmodel;
	stvert_t			*pstverts, *pinstverts;
	aliashdr_t			*pheader;
	mtriangle_t			*ptri;
	dtriangle_t			*pintriangles;
	int					version, numframes, numskins;
	int					size;
	daliasframetype_t	*pframetype;
	daliasskintype_t	*pskintype;
	maliasskindesc_t	*pskindesc;
	int					skinsize;
	int					start, end, total;
	
	start = Hunk_LowMark ();

	pinmodel = (mdl_t *)buffer;

	version = LittleLong (pinmodel->version);
	if (version != ALIAS_VERSION)
		Sys_Error ("%s has wrong version number (%i should be %i)",
				 mod->name, version, ALIAS_VERSION);

//
// allocate space for a working header, plus all the data except the frames,
// skin and group info
//
	size = 	sizeof (aliashdr_t) + (LittleLong (pinmodel->numframes) - 1) *
			 sizeof (pheader->frames[0]) +
			sizeof (mdl_t) +
			LittleLong (pinmodel->numverts) * sizeof (stvert_t) +
			LittleLong (pinmodel->numtris) * sizeof (mtriangle_t);

	pheader = Hunk_AllocName (size, loadname);
	pmodel = (mdl_t *) ((byte *)&pheader[1] +
			(LittleLong (pinmodel->numframes) - 1) *
			 sizeof (pheader->frames[0]));
	
//	mod->cache.data = pheader;
	mod->flags = LittleLong (pinmodel->flags);

//
// endian-adjust and copy the data, starting with the alias model header
//
	pmodel->boundingradius = LittleFloat (pinmodel->boundingradius);
	pmodel->numskins = LittleLong (pinmodel->numskins);
	pmodel->skinwidth = LittleLong (pinmodel->skinwidth);
	pmodel->skinheight = LittleLong (pinmodel->skinheight);

	if (pmodel->skinheight > MAX_LBM_HEIGHT)
		Sys_Error ("model %s has a skin taller than %d", mod->name,
				   MAX_LBM_HEIGHT);

	pmodel->numverts = LittleLong (pinmodel->numverts);

	if (pmodel->numverts <= 0)
		Sys_Error ("model %s has no vertices", mod->name);

	if (pmodel->numverts > MAXALIASVERTS)
		Sys_Error ("model %s has too many vertices", mod->name);

	pmodel->numtris = LittleLong (pinmodel->numtris);

	if (pmodel->numtris <= 0)
		Sys_Error ("model %s has no triangles", mod->name);

	pmodel->numframes = LittleLong (pinmodel->numframes);
	pmodel->size = LittleFloat (pinmodel->size) * ALIAS_BASE_SIZE_RATIO;
	mod->synctype = LittleLong (pinmodel->synctype);
	mod->numframes = pmodel->numframes;

	for (i=0 ; i<3 ; i++)
	{
		pmodel->scale[i] = LittleFloat (pinmodel->scale[i]);
		pmodel->scale_origin[i] = LittleFloat (pinmodel->scale_origin[i]);
		pmodel->eyeposition[i] = LittleFloat (pinmodel->eyeposition[i]);
	}

	numskins = pmodel->numskins;
	numframes = pmodel->numframes;

	if (pmodel->skinwidth & 0x03)
		Sys_Error ("Mod_LoadAliasModel: skinwidth not multiple of 4");

	pheader->model = (byte *)pmodel - (byte *)pheader;

//
// load the skins
//
	skinsize = pmodel->skinheight * pmodel->skinwidth;

	if (numskins < 1)
		Sys_Error ("Mod_LoadAliasModel: Invalid # of skins: %d\n", numskins);

	pskintype = (daliasskintype_t *)&pinmodel[1];

	pskindesc = Hunk_AllocName (numskins * sizeof (maliasskindesc_t),
								loadname);

	pheader->skindesc = (byte *)pskindesc - (byte *)pheader;

	for (i=0 ; i<numskins ; i++)
	{
		aliasskintype_t	skintype;

		skintype = LittleLong (pskintype->type);
		pskindesc[i].type = skintype;

		if (skintype == ALIAS_SKIN_SINGLE)
		{
			pskintype = (daliasskintype_t *)
					Mod_LoadAliasSkin (pskintype + 1,
									   &pskindesc[i].skin,
									   skinsize, pheader);
		}
		else
		{
			pskintype = (daliasskintype_t *)
					Mod_LoadAliasSkinGroup (pskintype + 1,
											&pskindesc[i].skin,
											skinsize, pheader);
		}
	}

//
// set base s and t vertices
//
	pstverts = (stvert_t *)&pmodel[1];
	pinstverts = (stvert_t *)pskintype;

	pheader->stverts = (byte *)pstverts - (byte *)pheader;

	for (i=0 ; i<pmodel->numverts ; i++)
	{
		pstverts[i].onseam = LittleLong (pinstverts[i].onseam);
	// put s and t in 16.16 format
		pstverts[i].s = LittleLong (pinstverts[i].s) << 16;
		pstverts[i].t = LittleLong (pinstverts[i].t) << 16;
	}

//
// set up the triangles
//
	ptri = (mtriangle_t *)&pstverts[pmodel->numverts];
	pintriangles = (dtriangle_t *)&pinstverts[pmodel->numverts];

	pheader->triangles = (byte *)ptri - (byte *)pheader;

	for (i=0 ; i<pmodel->numtris ; i++)
	{
		int		j;

		ptri[i].facesfront = LittleLong (pintriangles[i].facesfront);

		for (j=0 ; j<3 ; j++)
		{
			ptri[i].vertindex[j] =
					LittleLong (pintriangles[i].vertindex[j]);
		}
	}

//
// load the frames
//
	if (numframes < 1)
		Sys_Error ("Mod_LoadAliasModel: Invalid # of frames: %d\n", numframes);

	pframetype = (daliasframetype_t *)&pintriangles[pmodel->numtris];

	for (i=0 ; i<numframes ; i++)
	{
		aliasframetype_t	frametype;

		frametype = LittleLong (pframetype->type);
		pheader->frames[i].type = frametype;

		if (frametype == ALIAS_SINGLE)
		{
			pframetype = (daliasframetype_t *)
					Mod_LoadAliasFrame (pframetype + 1,
										&pheader->frames[i].frame,
										pmodel->numverts,
										&pheader->frames[i].bboxmin,
										&pheader->frames[i].bboxmax,
										pheader, pheader->frames[i].name);
		}
		else
		{
			pframetype = (daliasframetype_t *)
					Mod_LoadAliasGroup (pframetype + 1,
										&pheader->frames[i].frame,
										pmodel->numverts,
										&pheader->frames[i].bboxmin,
										&pheader->frames[i].bboxmax,
										pheader, pheader->frames[i].name);
		}
	}

	mod->type = mod_alias;

// FIXME: do this right
	mod->mins[0] = mod->mins[1] = mod->mins[2] = -16;
	mod->maxs[0] = mod->maxs[1] = mod->maxs[2] = 16;
//
// move the complete, relocatable alias model to the cache
//	
	end = Hunk_LowMark ();
	total = end - start;
	
	Cache_Alloc (&mod->cache, total, loadname);
	if (!mod->cache.data)
		return;
	memcpy (mod->cache.data, pheader, total);

	Hunk_FreeToLowMark (start);
}


void Mod_PrepareSurface(msurface_t *out, int plane)
{
    int i;
    out->fplane = loadmodel->fplanes + plane;
	// set the drawing flags flag
		
		if (!Q_strncmp(out->texinfo->texture->name,"sky",3))	// sky
		{
			out->flags |= (SURF_DRAWSKY | SURF_DRAWTILED);
			return;
		}
		
		if (!Q_strncmp(out->texinfo->texture->name,"*",1))		// turbulent
		{
			out->flags |= (SURF_DRAWTURB | SURF_DRAWTILED);
			for (i=0 ; i<2 ; i++)
			{
				out->extents[i] = 16384;
				out->texturemins[i] = -8192;
			}
			return;
		}
}

void Mod_PreparePlanes(void)
{
    int i;
    mfplane_t *fixed = Hunk_AllocName(loadmodel->numplanes * sizeof(*fixed), loadname);
    loadmodel->fplanes = fixed;
    for (i = 0; i < loadmodel->numplanes; ++i) {
        mplane_t *plane = &loadmodel->planes[i];
        FIXED_FLOATTOFIXED(plane->normal[0], fixed[i].normal[0], 24);
        FIXED_FLOATTOFIXED(plane->normal[1], fixed[i].normal[1], 24);
        FIXED_FLOATTOFIXED(plane->normal[2], fixed[i].normal[2], 24);
        FIXED_FLOATTOFIXED(plane->dist, fixed[i].dist, 12);
        fixed[i].type = plane->type;
        fixed[i].signbits = plane->signbits;
    }
}
void Mod_PrepareNode(mnode_t *node, int plane)
{
    node->fplane = loadmodel->fplanes + plane;
}
mspriteframe_t *Mod_LoadSpritePixels(const char *name, int width, int height, int bytes, const void *pixels)
{
    int i, count = width * height;
    const byte *input = pixels;
    mspriteframe_t *frame = Hunk_AllocName(sizeof(*frame) + count * r_pixbytes, loadname);
    (void)name;
    if (r_pixbytes != 1 && r_pixbytes != 2)
        Sys_Error("Invalid software sprite pixel size: %d", r_pixbytes);
    for (i = 0; i < count; ++i) {
        byte index = bytes == 4 ? findclosestpalmatch(input[i*4], input[i*4+1], input[i*4+2], input[i*4+3]) : input[i];
        if (r_pixbytes == 1)
            frame->pixels[i] = index;
        else
            ((unsigned short *)frame->pixels)[i] = d_8to16table[index];
    }
    return frame;
}


const qboolean mod_loading_progress = false, mod_external_sprites = false, mod_permanent_sprites = false;
const int mod_lighting_channels = 3;
void Mod_ClearModelStorage(void) { }
void Mod_InitializeBrushStorage(void) { }
