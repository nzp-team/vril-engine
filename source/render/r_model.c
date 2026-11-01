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

#include "r_model_local.h"

model_t *loadmodel;
char loadname[MAX_QPATH];
byte *mod_base;
byte mod_novis[MAX_MAP_LEAFS/8];
#define MAX_MOD_KNOWN 512
#define NL_PRESENT 0
#define NL_NEEDS_LOADED 1
#define NL_UNREFERENCED 2
model_t mod_known[MAX_MOD_KNOWN];
int mod_numknown;
cvar_t gl_subdivide_size = {"gl_subdivide_size", "128", true};
cvar_t r_loadq3models = {"r_loadq3models", "0", true};
static FILE *mod_bsp_file;
static long mod_bsp_offset;
static int mod_bsp_length;
static byte *mod_lump_buffer;
static size_t mod_lump_capacity;
static model_t *Mod_LoadModel(model_t *model, qboolean crash);
void Mod_Init (void)
{
	Cvar_RegisterVariable (&gl_subdivide_size);
	Cvar_RegisterVariable (&r_loadq3models);
	memset (mod_novis, 0xff, sizeof(mod_novis));
}

void *Mod_Extradata (model_t *mod)
{
	void	*r;
	
	r = Cache_Check (&mod->cache);
	if (r)
		return r;

	Mod_LoadModel (mod, true);
	
	if (!mod->cache.data)
		Sys_Error ("cache fail (%s)", mod->name);
	return mod->cache.data;
}

mleaf_t *Mod_PointInLeaf (vec3_t p, model_t *model)
{
	mnode_t		*node;
	float		d;
	mplane_t	*plane;
	
	if (!model || !model->nodes)
		Sys_Error ("bad model");

	node = model->nodes;
	while (1)
	{
		if (node->contents < 0)
			return (mleaf_t *)node;
		plane = node->plane;
		d = DotProduct (p,plane->normal) - plane->dist;
		if (d > 0)
			node = node->children[0];
		else
			node = node->children[1];
	}
	
	return NULL;	// never reached
}

byte *Mod_DecompressVis (byte *in, model_t *model)
{
	static byte	decompressed[MAX_MAP_LEAFS/8];
	int		c;
	byte	*out;
	int		row;

	row = (model->numleafs+7)>>3;	
	out = decompressed;

	if (!in)
	{	// no vis info, so make all visible
		while (row)
		{
			*out++ = 0xff;
			row--;
		}
		return decompressed;		
	}

	do
	{
		if (*in)
		{
			*out++ = *in++;
			continue;
		}
	
		c = in[1];
		in += 2;
		while (c)
		{
			*out++ = 0;
			c--;
		}
	} while (out - decompressed < row);
	
	return decompressed;
}
byte *Mod_LeafPVS (mleaf_t *leaf, model_t *model)
{
	if (leaf == model->leafs)
		return mod_novis;
	return Mod_DecompressVis (leaf->compressed_vis, model);
}
void Mod_ClearAll(void)
{
    int i;
    for (i = 0; i < mod_numknown; ++i) {
        model_t *model = &mod_known[i];
        model->needload = NL_UNREFERENCED;
        if (model->type == mod_alias && Cache_Check(&model->cache))
            Cache_Free(&model->cache);
        else if (model->type == mod_sprite)
            model->cache.data = NULL;
    }
    Mod_ClearModelStorage();
}
model_t *Mod_FindName (char *name)
{
	int		i;
	model_t	*mod;
	model_t	*avail = NULL;

	if (!name[0])
		Sys_Error ("Mod_ForName: NULL name");
		
//
// search the currently loaded models
//
	for (i=0 , mod=mod_known ; i<mod_numknown ; i++, mod++)
	{
		if (!strcmp (mod->name, name) )
			break;
		if (mod->needload == NL_UNREFERENCED)
			if (!avail || mod->type != mod_alias)
				avail = mod;
	}
			
	if (i == mod_numknown)
	{
		if (mod_numknown == MAX_MOD_KNOWN)
		{
			if (avail)
			{
				mod = avail;
				if (mod->type == mod_alias)
					if (Cache_Check (&mod->cache))
						Cache_Free (&mod->cache);
			}
			else
				Sys_Error ("mod_numknown == MAX_MOD_KNOWN");
		}
		else
			mod_numknown++;
		strcpy (mod->name, name);
		mod->needload = NL_NEEDS_LOADED;
	}

	return mod;
}

void Mod_TouchModel (char *name)
{
	model_t	*mod;
	
	mod = Mod_FindName (name);
	
	if (!mod->needload)
	{
		if (mod->type == mod_alias)
			Cache_Check (&mod->cache);
	}
}
void Mod_ReadLump(lump_t *l, void *dest)
{
    if (!mod_bsp_file) {
        memcpy(dest, mod_base + l->fileofs, l->filelen);
        return;
    }
	if (l->fileofs < 0 || l->filelen < 0 || l->fileofs > mod_bsp_length - l->filelen)
		Sys_Error("Invalid BSP lump in %s", loadmodel->name);
	if (fseek(mod_bsp_file, mod_bsp_offset + l->fileofs, SEEK_SET) ||
		fread(dest, 1, l->filelen, mod_bsp_file) != (size_t)l->filelen)
		Sys_Error("Could not read BSP lump in %s", loadmodel->name);
}

byte *Mod_LumpData(lump_t *l)
{
	if (!mod_bsp_file)
		return mod_base + l->fileofs;
	if (l->fileofs < 0 || l->filelen < 0 || l->fileofs > mod_bsp_length - l->filelen)
		Sys_Error("Invalid BSP lump in %s", loadmodel->name);
	if ((size_t)l->filelen > mod_lump_capacity) {
		byte *data = realloc(mod_lump_buffer, l->filelen);
		if (!data)
			Sys_Error("Out of BSP lump memory");
		mod_lump_buffer = data;
		mod_lump_capacity = l->filelen;
	}
	Mod_ReadLump(l, mod_lump_buffer);
	return mod_lump_buffer;
}

static void *Mod_LoadSpriteFrame(void *data, mspriteframe_t **frame, int number, int version)
{
    dspriteframe_t header, *input = &header;
    memcpy(input, data, sizeof(*input));
    int width = LittleLong(input->width), height = LittleLong(input->height);
    int bytes = version == SPRITE32_VERSION ? 4 : 1;
    int x = LittleLong(input->origin[0]), y = LittleLong(input->origin[1]);
    char name[MAX_OSPATH];
    if (width <= 0 || height <= 0 || width > 4096 || height > 4096)
        Sys_Error("Invalid sprite dimensions in %s", loadmodel->name);
    COM_StripExtension(loadmodel->name, name);
    snprintf(name + strlen(name), sizeof(name) - strlen(name), ".spr_%i", number);
    *frame = Mod_LoadSpritePixels(name, width, height, bytes, (byte *)data + sizeof(*input));
    (*frame)->width = width;
    (*frame)->height = height;
    (*frame)->up = y;
    (*frame)->down = y - height;
    (*frame)->left = x;
    (*frame)->right = x + width;
    return (byte *)data + sizeof(*input) + width * height * bytes;
}

static void * Mod_LoadSpriteGroup (void * pin, mspriteframe_t **ppframe, int framenum, int version)
{
	dspritegroup_t		header, *pingroup;
	mspritegroup_t		*pspritegroup;
	int					i, numframes;
	dspriteinterval_t	*pin_intervals;
	float				*poutintervals;
	void				*ptemp;

	pingroup = &header;
	memcpy(pingroup, pin, sizeof(*pingroup));

	numframes = LittleLong (pingroup->numframes);

	pspritegroup = Hunk_AllocName (sizeof (mspritegroup_t) +
				(numframes - 1) * sizeof (pspritegroup->frames[0]), loadname);

	pspritegroup->numframes = numframes;

	*ppframe = (mspriteframe_t *)pspritegroup;

	pin_intervals = (dspriteinterval_t *)((byte *)pin + sizeof(*pingroup));

	poutintervals = Hunk_AllocName (numframes * sizeof (float), loadname);

	pspritegroup->intervals = poutintervals;

	for (i=0 ; i<numframes ; i++)
	{
		float interval;
		memcpy(&interval, pin_intervals, sizeof(interval));
		*poutintervals = LittleFloat(interval);
		if (*poutintervals <= 0.0f)
			Sys_Error ("interval<=0");

		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void *)pin_intervals;

	for (i=0 ; i<numframes ; i++)
	{
		ptemp = Mod_LoadSpriteFrame (ptemp, &pspritegroup->frames[i], framenum * 100 + i, version);
	}

	return ptemp;
}

static void Mod_LoadSpriteModel (model_t *mod, void *buffer)
{
	int					i;
	int					version;
	dsprite_t			*pin;
	msprite_t			*psprite;
	int					numframes;
	int					size;
	dspriteframetype_t	*pframetype;
	
	pin = (dsprite_t *)buffer;

	version = LittleLong (pin->version);
	if (version != SPRITE_VERSION && version != SPRITE32_VERSION)
		Sys_Error ("%s has wrong version number "
				 "(%i should be %i)", mod->name, version, SPRITE_VERSION);

	numframes = LittleLong (pin->numframes);
	if (numframes < 1)
		Sys_Error("Invalid sprite frame count in %s", mod->name);

	size = sizeof (msprite_t) +	(numframes - 1) * sizeof (psprite->frames);

	psprite = Hunk_AllocName (size, loadname);

	mod->cache.data = psprite;

	psprite->type = LittleLong (pin->type);
	psprite->maxwidth = LittleLong (pin->width);
	psprite->maxheight = LittleLong (pin->height);
	psprite->beamlength = LittleFloat (pin->beamlength);
	mod->synctype = LittleLong (pin->synctype);
	psprite->numframes = numframes;

	mod->mins[0] = mod->mins[1] = -psprite->maxwidth/2.0f;
	mod->maxs[0] = mod->maxs[1] = psprite->maxwidth/2.0f;
	mod->mins[2] = -psprite->maxheight/2.0f;
	mod->maxs[2] = psprite->maxheight/2.0f;
	
//
// load the frames
//
	if (numframes < 1)
		Sys_Error ("Invalid # of frames: %d\n", numframes);

	mod->numframes = numframes;

	pframetype = (dspriteframetype_t *)(pin + 1);

	for (i=0 ; i<numframes ; i++)
	{
		spriteframetype_t	frametype;

		memcpy(&frametype, pframetype, sizeof(frametype));
		frametype = LittleLong(frametype);
		psprite->frames[i].type = frametype;

		if (frametype == SPR_SINGLE)
		{
			pframetype = (dspriteframetype_t *)
					Mod_LoadSpriteFrame (pframetype + 1,
										 &psprite->frames[i].frameptr, i, version);
		}
		else
		{
			pframetype = (dspriteframetype_t *)
					Mod_LoadSpriteGroup (pframetype + 1,
										 &psprite->frames[i].frameptr, i, version);
		}
	}

	mod->type = mod_sprite;
}

static void Mod_LoadQ2SpriteModel (model_t *mod, void *buffer)
{
	int					i;
	int					version;
	dmd2sprite_t		*pin;
	msprite_t			*psprite;
	int					numframes;
	int					size;
	dmd2sprframe_t		*pframetype;
	mspriteframe_t		*frame;
	float origin[2];
	int hunkstart;

	hunkstart = Hunk_LowMark();

	pin = (dmd2sprite_t *)buffer;

	version = LittleLong (pin->version);
	if (version != SPRITE2_VERSION)
	{
		Sys_Error ("%s has wrong version number "
				 "(%i should be %i)", mod->name, version, SPRITE2_VERSION);
		return;
	}

	numframes = LittleLong (pin->numframes);

	size = sizeof (msprite_t) +	(numframes - 1) * sizeof (psprite->frames);

	psprite = Hunk_AllocName (size, loadname);

	mod->cache.data = psprite;

	psprite->type = SPR_VP_PARALLEL;
	psprite->maxwidth = 1;
	psprite->maxheight = 1;
	psprite->beamlength = 1;
	mod->synctype = 1;
	psprite->numframes = numframes;

	mod->mins[0] = mod->mins[1] = -psprite->maxwidth/2;
	mod->maxs[0] = mod->maxs[1] = psprite->maxwidth/2;
	mod->mins[2] = -psprite->maxheight/2;
	mod->maxs[2] = psprite->maxheight/2;

//
// load the frames
//
	if (numframes < 1)
	{
		Sys_Error ("Invalid # of frames: %d\n", numframes);
		Hunk_FreeToLowMark(hunkstart);
		return;
	}

	mod->numframes = numframes;

	pframetype = pin->frames;

	for (i=0 ; i<numframes ; i++)
	{
		spriteframetype_t	frametype;

		frametype = SPR_SINGLE;
		psprite->frames[i].type = frametype;

		frame = psprite->frames[i].frameptr = Mod_LoadSpritePixels(pframetype->name,
            LittleLong(pframetype->width), LittleLong(pframetype->height), 0, NULL);


		frame->width = LittleLong(pframetype->width);
		frame->height = LittleLong(pframetype->height);
		origin[0] = LittleLong (pframetype->origin_x);
		origin[1] = LittleLong (pframetype->origin_y);
        frame->up    = origin[1];
        frame->down  = origin[1] - frame->height;
		frame->left  = -origin[0];
		frame->right = -origin[0] + frame->width;


		pframetype++;
	}

	mod->type = mod_sprite;

	return;
}

static model_t *Mod_LoadModel(model_t *model, qboolean crash)
{
    byte stack[1024];
    void *data;
    int mark;
    if (model->type == mod_alias && Cache_Check(&model->cache)) {
        model->needload = NL_PRESENT;
        return model;
    }
    if (model->type != mod_alias && model->needload == NL_PRESENT)
        return model;
    if (!strcmp(COM_FileExtension(model->name), "bsp")) {
        dheader_t header;
        mod_bsp_length = COM_FOpenFile(model->name, &mod_bsp_file);
        if (!mod_bsp_file) {
            if (crash)
                Sys_Error("%s not found", model->name);
            return NULL;
        }
        mod_bsp_offset = ftell(mod_bsp_file);
        if (fread(&header, 1, sizeof(header), mod_bsp_file) != sizeof(header))
            Sys_Error("Could not read BSP header in %s", model->name);
        COM_FileBase(model->name, loadname);
        loadmodel = model;
        model->needload = NL_PRESENT;
        Mod_LoadBrushModel(model, &header);
        fclose(mod_bsp_file);
        mod_bsp_file = NULL;
        free(mod_lump_buffer);
        mod_lump_buffer = NULL;
        mod_lump_capacity = 0;
        return model;
    }
    mark = Hunk_HighMark();
    data = NULL;
    if (r_loadq3models.value) {
        char alternative[MAX_OSPATH];
        COM_StripExtension(model->name, alternative);
        Q_strlcat(alternative, ".md3", sizeof(alternative));
        data = COM_LoadStackFile(alternative, stack, sizeof(stack));
    }
    if (!data)
        data = COM_LoadStackFile(model->name, stack, sizeof(stack));
    if (!data && crash) {
        data = COM_LoadStackFile("models/missing_model.mdl", stack, sizeof(stack));
        if (data)
            Con_Printf("Missing model %s substituted\n", model->name);
    }
    if (!data) {
        Hunk_FreeToHighMark(mark);
        if (crash)
            Sys_Error("%s not found", model->name);
        return NULL;
    }
    COM_FileBase(model->name, loadname);
    loadmodel = model;
    model->needload = NL_PRESENT;
    switch (LittleLong(*(unsigned *)data)) {
    case IDPOLYHEADER:
        Mod_LoadAliasModel(model, data);
        break;
    case IDSPRITEHEADER:
        Mod_LoadSpriteModel(model, data);
        break;
    case IDSPRITE2HEADER:
        if (!mod_external_sprites)
            Sys_Error("External sprites are unsupported by this renderer");
        Mod_LoadQ2SpriteModel(model, data);
        break;
    default:
        Mod_LoadBrushModel(model, data);
        break;
    }
    Hunk_FreeToHighMark(mark);
    return model;
}

model_t *Mod_ForName (char *name, qboolean crash)
{
	model_t	*mod;
	
	mod = Mod_FindName (name);
	
	return Mod_LoadModel (mod, crash);
}

void Mod_LoadLighting (lump_t *l)
{
	// LordHavoc: .lit support begin
	int i;
	byte *in, *out, *data;
	byte d;
	char litfilename[1024];
	loadmodel->lightdata = NULL;
	
	// Diabolickal HLBSP
	if (loadmodel->bspversion == HL_BSPVERSION)
	{
        if (!l->filelen)
	    {
		  return;
	    }
	    loadmodel->lightdata = (Hunk_AllocName ( l->filelen, loadname));
	    if (mod_bsp_file)
		    Mod_ReadLump(l, loadmodel->lightdata);
		else
		    memcpy (loadmodel->lightdata, mod_base + l->fileofs, l->filelen);
        return;
	}

	// LordHavoc: check for a .lit file
	strcpy(litfilename, loadmodel->name);
	COM_StripExtension(litfilename, litfilename);
	strcat(litfilename, ".lit");
	data = (byte*) COM_LoadHunkFile (litfilename);
	if (data)
	{
		if (data[0] == 'Q' && data[1] == 'L' && data[2] == 'I' && data[3] == 'T')
		{
			i = LittleLong(((int *)data)[1]);
			if (i == 1)
			{
				Con_DPrintf("%s loaded", litfilename);
				loadmodel->lightdata = data + 8;
				return;
			}
			else
				Con_Printf("Unknown .lit file version (%d)\n", i);
		}
		else
			Con_Printf("Corrupt .lit file (old version?), ignoring\n");
	}

	// LordHavoc: no .lit found, expand the white lighting data to color
	if (!l->filelen)
		return;
	loadmodel->lightdata = Hunk_AllocName ( l->filelen*3, litfilename);
	in = loadmodel->lightdata + l->filelen*2; // place the file at the end, so it will not be overwritten until the very last write
	out = loadmodel->lightdata;
	if (mod_bsp_file)
		Mod_ReadLump(l, in);
	else
		memcpy (in, mod_base + l->fileofs, l->filelen);
	for (i = 0;i < l->filelen;i++)
	{
		d = *in++;
		*out++ = d;
		*out++ = d;
		*out++ = d;
	}
	// LordHavoc: .lit support end

}

void Mod_LoadEntities (lump_t *l)
{
	char entfilename[128];

	loadmodel->entities = NULL;

	strcpy(entfilename, loadmodel->name);
	COM_StripExtension(entfilename, entfilename);
	strcat(entfilename, ".ent");
	byte *ent_file = (byte*) COM_LoadHunkFile (entfilename);

	if (ent_file)
	{
      if (ent_file[0] == '{')
	  {
		Con_DPrintf("%s loaded", entfilename);
		loadmodel->entities = (char*)ent_file;
		return;
	  }
	  else
		Con_Printf("Corrupt .ent file, ignoring\n");
	}

    if (!l->filelen)
	{
		//loadmodel->entities = NULL;
		return;
	}

	loadmodel->entities = Hunk_AllocName ( l->filelen, entfilename);
	memcpy(loadmodel->entities, Mod_LumpData(l), l->filelen);
}

void Mod_LoadVisibility (lump_t *l)
{
	if (!l->filelen)
	{
		loadmodel->visdata = NULL;
		return;
	}
	loadmodel->visdata = Hunk_AllocName ( l->filelen, loadname);	
	memcpy (loadmodel->visdata, Mod_LumpData(l), l->filelen);
}

void Mod_LoadVertexes (lump_t *l)
{
	dvertex_t	*in;
	mvertex_t	*out;
	int			i, count;

	in = (void *)(Mod_LumpData(l));
	if (l->filelen % sizeof(*in))
		Sys_Error ("funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadname);	

	loadmodel->vertexes = out;
	loadmodel->numvertexes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->position[0] = LittleFloat (in->point[0]);
		out->position[1] = LittleFloat (in->point[1]);
		out->position[2] = LittleFloat (in->point[2]);
	}
}

void Mod_LoadSubmodels (lump_t *l)
{
	dmodel_t	*in;
	dmodel_t	*out;
	int			i, j, count;

	in = (void *)(Mod_LumpData(l));
	if (l->filelen % sizeof(*in))
		Sys_Error ("funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadname);	

	loadmodel->submodels = out;
	loadmodel->numsubmodels = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{	// spread the mins / maxs by a pixel
			out->mins[j] = LittleFloat (in->mins[j]) - 1;
			out->maxs[j] = LittleFloat (in->maxs[j]) + 1;
			out->origin[j] = LittleFloat (in->origin[j]);
		}
		for (j=0 ; j<MAX_MAP_HULLS ; j++)
			out->headnode[j] = LittleLong (in->headnode[j]);
		out->visleafs = LittleLong (in->visleafs);
		out->firstface = LittleLong (in->firstface);
		out->numfaces = LittleLong (in->numfaces);
	}
}

void Mod_LoadEdges (lump_t *l)
{
	dedge_t *in;
	medge_t *out;
	int 	i, count;

	in = (void *)(Mod_LumpData(l));
	if (l->filelen % sizeof(*in))
		Sys_Error ("funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( (count + 1) * sizeof(*out), loadname);	

	loadmodel->edges = out;
	loadmodel->numedges = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		out->v[0] = (unsigned short)LittleShort(in->v[0]);
		out->v[1] = (unsigned short)LittleShort(in->v[1]);
	}
}

void Mod_LoadTexinfo (lump_t *l)
{
	texinfo_t *in;
	mtexinfo_t *out;
	int 	i, j, k, count;
	int		miptex;
	float	len1, len2;

	in = (void *)(Mod_LumpData(l));
	if (l->filelen % sizeof(*in))
		Sys_Error ("funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadname);	

	loadmodel->texinfo = out;
	loadmodel->numtexinfo = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<2 ; j++) {
			for (k=0 ; k<4 ; k++) {
				out->vecs[j][k] = LittleFloat (in->vecs[j][k]);
			}
		}
		len1 = Length (out->vecs[0]);
		len2 = Length (out->vecs[1]);
		len1 = (len1 + len2)/2;
		if (len1 < 0.32f)
			out->mipadjust = 4;
		else if (len1 < 0.49f)
			out->mipadjust = 3;
		else if (len1 < 0.99f)
			out->mipadjust = 2;
		else
			out->mipadjust = 1;
#if 0
		if (len1 + len2 < 0.001)
			out->mipadjust = 1;		// don't crash
		else
			out->mipadjust = 1 / floor( (len1+len2)/2 + 0.1 );
#endif

		miptex = LittleLong (in->miptex);
		out->flags = LittleLong (in->flags);
	
		if (!loadmodel->textures)
		{
			out->texture = r_notexture_mip;	// checkerboard texture
			out->flags = 0;
		}
		else
		{
			if (miptex >= loadmodel->numtextures)
				Sys_Error ("miptex >= loadmodel->numtextures");
			out->texture = loadmodel->textures[miptex];
			if (!out->texture)
			{
				out->texture = r_notexture_mip; // texture not found
				out->flags = 0;
			}
		}
	}
}

void CalcSurfaceExtents (msurface_t *s)
{
	float	mins[2], maxs[2], val;
	int		i,j, e;
	mvertex_t	*v;
	mtexinfo_t	*tex;
	int		bmins[2], bmaxs[2];

	mins[0] = mins[1] = 999999;
	maxs[0] = maxs[1] = -99999;

	tex = s->texinfo;
	
	for (i=0 ; i<s->numedges ; i++)
	{
		e = loadmodel->surfedges[s->firstedge+i];
		if (e >= 0)
			v = &loadmodel->vertexes[loadmodel->edges[e].v[0]];
		else
			v = &loadmodel->vertexes[loadmodel->edges[-e].v[1]];
		
		for (j=0 ; j<2 ; j++)
		{
			val = v->position[0] * tex->vecs[j][0] + 
				v->position[1] * tex->vecs[j][1] +
				v->position[2] * tex->vecs[j][2] +
				tex->vecs[j][3];
			if (val < mins[j])
				mins[j] = val;
			if (val > maxs[j])
				maxs[j] = val;
		}
	}

	for (i=0 ; i<2 ; i++)
	{	
		bmins[i] = (int)floorf(mins[i]/16);
		bmaxs[i] = (int)ceilf(maxs[i]/16);

		s->texturemins[i] = bmins[i] * 16;
		s->extents[i] = (bmaxs[i] - bmins[i]) * 16;
		if ( !(tex->flags & TEX_SPECIAL) && s->extents[i] > 512 /* 256 */ )
			Sys_Error ("Bad surface extents");
	}
}

void Mod_SetParent (mnode_t *node, mnode_t *parent)
{
	node->parent = parent;
	if (node->contents < 0)
		return;
	Mod_SetParent (node->children[0], node);
	Mod_SetParent (node->children[1], node);
}

void Mod_LoadNodes (lump_t *l)
{
	int			i, j, count, p;
	dnode_t		*in;
	mnode_t 	*out;

	in = (void *)(Mod_LumpData(l));
	if (l->filelen % sizeof(*in))
		Sys_Error ("funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadname);	

	loadmodel->nodes = out;
	loadmodel->numnodes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3+j] = LittleShort (in->maxs[j]);
		}
	
		p = LittleLong(in->planenum);
		out->plane = loadmodel->planes + p;
        Mod_PrepareNode(out, p);

		out->firstsurface = LittleShort (in->firstface);
		out->numsurfaces = LittleShort (in->numfaces);
		
		for (j=0 ; j<2 ; j++)
		{
			p = LittleShort (in->children[j]);
			if (p >= 0)
				out->children[j] = loadmodel->nodes + p;
			else
				out->children[j] = (mnode_t *)(loadmodel->leafs + (-1 - p));
		}
	}
	
	Mod_SetParent (loadmodel->nodes, NULL);	// sets nodes and leafs
}

void Mod_LoadLeafs (lump_t *l)
{
	dleaf_t 	*in;
	mleaf_t 	*out;
	int			i, j, count, p;

	in = (void *)(Mod_LumpData(l));
	if (l->filelen % sizeof(*in))
		Sys_Error ("funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadname);	

	loadmodel->leafs = out;
	loadmodel->numleafs = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		for (j=0 ; j<3 ; j++)
		{
			out->minmaxs[j] = LittleShort (in->mins[j]);
			out->minmaxs[3+j] = LittleShort (in->maxs[j]);
		}

		p = LittleLong(in->contents);
		out->contents = p;

		out->firstmarksurface = loadmodel->marksurfaces +
			LittleShort(in->firstmarksurface);
		out->nummarksurfaces = LittleShort(in->nummarksurfaces);
		
		p = LittleLong(in->visofs);
		if (p == -1)
			out->compressed_vis = NULL;
		else
			out->compressed_vis = loadmodel->visdata + p;
		out->efrags = NULL;
		
		for (j=0 ; j<4 ; j++)
			out->ambient_sound_level[j] = in->ambient_level[j];

		// gl underwater warp
		if (out->contents != CONTENTS_EMPTY)
		{
			for (j=0 ; j<out->nummarksurfaces ; j++)
				out->firstmarksurface[j]->flags |= SURF_UNDERWATER;
		}
	}	
}

void Mod_LoadClipnodes (lump_t *l)
{
	dclipnode_t *in, *out;
	int			i, count;
	hull_t		*hull;

	in = (void *)(Mod_LumpData(l));
	if (l->filelen % sizeof(*in))
		Sys_Error ("funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadname);	

	loadmodel->clipnodes = out;
	loadmodel->numclipnodes = count;

	if (loadmodel->bspversion == HL_BSPVERSION)		//Diabolickal HLBSP
	{
		hull = &loadmodel->hulls[1];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = loadmodel->planes;
		hull->clip_mins[0] = -16;
		hull->clip_mins[1] = -16;
		hull->clip_mins[2] = -36;
		hull->clip_maxs[0] = 16;
		hull->clip_maxs[1] = 16;
		hull->clip_maxs[2] = 36;

		hull = &loadmodel->hulls[2];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = loadmodel->planes;
		hull->clip_mins[0] = -32;
		hull->clip_mins[1] = -32;
		hull->clip_mins[2] = -32;
		hull->clip_maxs[0] = 32;
		hull->clip_maxs[1] = 32;
		hull->clip_maxs[2] = 32;

	    hull = &loadmodel->hulls[3];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = loadmodel->planes;
		hull->clip_mins[0] = -16;
		hull->clip_mins[1] = -16;
		hull->clip_mins[2] = -18;
		hull->clip_maxs[0] = 16;
		hull->clip_maxs[1] = 16;
		hull->clip_maxs[2] = 18;
	} else {
		hull = &loadmodel->hulls[1];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = loadmodel->planes;
		hull->clip_mins[0] = -16;
		hull->clip_mins[1] = -16;
		hull->clip_mins[2] = -24;
		hull->clip_maxs[0] = 16;
		hull->clip_maxs[1] = 16;
		hull->clip_maxs[2] = 32;

		hull = &loadmodel->hulls[2];
		hull->clipnodes = out;
		hull->firstclipnode = 0;
		hull->lastclipnode = count-1;
		hull->planes = loadmodel->planes;
		hull->clip_mins[0] = -32;
		hull->clip_mins[1] = -32;
		hull->clip_mins[2] = -24;
		hull->clip_maxs[0] = 32;
		hull->clip_maxs[1] = 32;
		hull->clip_maxs[2] = 64;
	}

	for (i=0 ; i<count ; i++, out++, in++)
	{
		out->planenum = LittleLong(in->planenum);
		out->children[0] = LittleShort(in->children[0]);
		out->children[1] = LittleShort(in->children[1]);
	}
}

void Mod_MakeHull0 (void)
{
	mnode_t		*in, *child;
	dclipnode_t *out;
	int			i, j, count;
	hull_t		*hull;
	
	hull = &loadmodel->hulls[0];	
	
	in = loadmodel->nodes;
	count = loadmodel->numnodes;
	out = Hunk_AllocName ( count*sizeof(*out), loadname);	

	hull->clipnodes = out;
	hull->firstclipnode = 0;
	hull->lastclipnode = count-1;
	hull->planes = loadmodel->planes;

	for (i=0 ; i<count ; i++, out++, in++)
	{
		out->planenum = in->plane - loadmodel->planes;
		for (j=0 ; j<2 ; j++)
		{
			child = in->children[j];
			if (child->contents < 0)
				out->children[j] = child->contents;
			else
				out->children[j] = child - loadmodel->nodes;
		}
	}
}

void Mod_LoadMarksurfaces (lump_t *l)
{	
	int		i, j, count;
	short		*in;
	msurface_t **out;
	
	in = (void *)(Mod_LumpData(l));
	if (l->filelen % sizeof(*in))
		Sys_Error ("funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadname);	

	loadmodel->marksurfaces = out;
	loadmodel->nummarksurfaces = count;

	for ( i=0 ; i<count ; i++)
	{
		j = LittleShort(in[i]);
		if (j >= loadmodel->numsurfaces)
			Sys_Error ("bad surface number");
		out[i] = loadmodel->surfaces + j;
	}
}

void Mod_LoadSurfedges (lump_t *l)
{	
	int		i, count;
	int		*in, *out;
	
	in = (void *)(Mod_LumpData(l));
	if (l->filelen % sizeof(*in))
		Sys_Error ("funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadname);	

	loadmodel->surfedges = out;
	loadmodel->numsurfedges = count;

	for ( i=0 ; i<count ; i++)
		out[i] = LittleLong (in[i]);
}

void Mod_LoadPlanes (lump_t *l)
{
	int			i, j;
	mplane_t	*out;
	dplane_t 	*in;
	int			count;
	int			bits;
	
	in = (void *)(Mod_LumpData(l));
	if (l->filelen % sizeof(*in))
		Sys_Error ("funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = Hunk_AllocName ( count*sizeof(*out), loadname);	
	
	loadmodel->planes = out;
	loadmodel->numplanes = count;

	for ( i=0 ; i<count ; i++, in++, out++)
	{
		bits = 0;
		for (j=0 ; j<3 ; j++)
		{
			out->normal[j] = LittleFloat (in->normal[j]);
			if (out->normal[j] < 0)
				bits |= 1<<j;
		}

		out->dist = LittleFloat (in->dist);
		out->type = LittleLong (in->type);
		out->signbits = bits;
	}
    Mod_PreparePlanes();
}

float RadiusFromBounds (vec3_t mins, vec3_t maxs)
{
	int		i;
	vec3_t	corner;

	for (i=0 ; i<3 ; i++)
	{
		corner[i] = fabsf(mins[i]) > fabsf(maxs[i]) ? fabsf(mins[i]) : fabsf(maxs[i]);
	}

	return Length (corner);
}

void Mod_LoadFaces (lump_t *l)
{
	dface_t		*in;
	msurface_t 	*out;
	int			i, count, surfnum;
	int			planenum, side;

	in = (dface_t *)(Mod_LumpData(l));
	if (l->filelen % sizeof(*in))
		Sys_Error ("funny lump size in %s",loadmodel->name);
	count = l->filelen / sizeof(*in);
	out = (msurface_t*)Hunk_AllocName ( count*sizeof(*out), loadname);	

	loadmodel->surfaces = out;
	loadmodel->numsurfaces = count;

	for ( surfnum=0 ; surfnum<count ; surfnum++, in++, out++)
	{
		out->firstedge = LittleLong(in->firstedge);
		out->numedges = LittleShort(in->numedges);		
		out->flags = 0;

		planenum = LittleShort(in->planenum);
		side = LittleShort(in->side);
		if (side)
			out->flags |= SURF_PLANEBACK;			

		out->plane = loadmodel->planes + planenum;

		out->texinfo = loadmodel->texinfo + LittleShort (in->texinfo);

		CalcSurfaceExtents (out);
				
	// lighting info

		for (i=0 ; i<MAXLIGHTMAPS ; i++)
			out->styles[i] = in->styles[i];
        i = LittleLong(in->lightofs);
        if (i < 0 || !loadmodel->lightdata)
            out->samples = NULL;
        else
            out->samples = loadmodel->lightdata +
                (loadmodel->bspversion == HL_BSPVERSION ? i : i * mod_lighting_channels);

        Mod_PrepareSurface(out, planenum);
    }
}


static void Mod_LoadTextures(lump_t *lump)
{
    dmiptexlump_t *table;
    lump_t part = *lump;
    int i, j, count, end, offset, num, max, altmax;
    texture_t *tx, *tx2, *anims[10], *altanims[10];
    loadmodel->textures = NULL;
    loadmodel->numtextures = 0;
    if (!lump->filelen)
        return;
    if (lump->filelen < (int)sizeof(count))
        Sys_Error("Invalid BSP texture table");
    part.filelen = sizeof(count);
    Mod_ReadLump(&part, &count);
    count = LittleLong(count);
    if (count < 0 || count > (lump->filelen - (int)sizeof(count)) / (int)sizeof(int))
        Sys_Error("Invalid BSP texture count");
    part.filelen = (count + 1) * sizeof(int);
    table = malloc(part.filelen);
    if (!table)
        Sys_Error("Out of BSP texture table memory");
    Mod_ReadLump(&part, table);
    for (i = 0; i < count; ++i)
        table->dataofs[i] = LittleLong(table->dataofs[i]);
    loadmodel->numtextures = count;
    loadmodel->textures = Hunk_AllocName(count * sizeof(*loadmodel->textures), loadname);
    loading_num_step += count;
    for (i = 0; i < count; ++i) {
        miptex_t *texture;
        offset = table->dataofs[i];
        if (offset == -1)
            continue;
        end = lump->filelen;
        for (j = 0; j < count; ++j)
            if (table->dataofs[j] > offset && table->dataofs[j] < end)
                end = table->dataofs[j];
        if (offset < (int)((count + 1) * sizeof(int)) || offset > end - (int)sizeof(*texture))
            Sys_Error("Invalid BSP texture offset");
        part.fileofs = lump->fileofs + offset;
        part.filelen = end - offset;
        texture = malloc(part.filelen);
        if (!texture)
            Sys_Error("Out of BSP texture memory");
        Mod_ReadLump(&part, texture);
        texture->width = LittleLong(texture->width);
        texture->height = LittleLong(texture->height);
        for (j = 0; j < MIPLEVELS; ++j)
            texture->offsets[j] = LittleLong(texture->offsets[j]);
        loadmodel->textures[i] = Mod_CreateTexture(texture);
        free(texture);
        loading_cur_step++;
        if (mod_loading_progress)
            CL_UpdateLoadingScreen(false);
    }
    free(table);
// sequence the animations
//
	for (i=0 ; i<loadmodel->numtextures ; i++)
	{
		tx = loadmodel->textures[i];
		if (!tx || tx->name[0] != '+')
			continue;
		if (tx->anim_next)
			continue;	// allready sequenced

	// find the number of frames in the animation
		memset (anims, 0, sizeof(anims));
		memset (altanims, 0, sizeof(altanims));

		max = tx->name[1];
		altmax = 0;
		if (max >= 'a' && max <= 'z')
			max -= 'a' - 'A';
		if (max >= '0' && max <= '9')
		{
			max -= '0';
			altmax = 0;
			anims[max] = tx;
			max++;
		}
		else if (max >= 'A' && max <= 'J')
		{
			altmax = max - 'A';
			max = 0;
			altanims[altmax] = tx;
			altmax++;
		}
		else
			Sys_Error ("Bad animating texture %s", tx->name);

		for (j=i+1 ; j<loadmodel->numtextures ; j++)
		{
			tx2 = loadmodel->textures[j];
			if (!tx2 || tx2->name[0] != '+')
				continue;
			if (strcmp (tx2->name+2, tx->name+2))
				continue;

			num = tx2->name[1];
			if (num >= 'a' && num <= 'z')
				num -= 'a' - 'A';
			if (num >= '0' && num <= '9')
			{
				num -= '0';
				anims[num] = tx2;
				if (num+1 > max)
					max = num + 1;
			}
			else if (num >= 'A' && num <= 'J')
			{
				num = num - 'A';
				altanims[num] = tx2;
				if (num+1 > altmax)
					altmax = num+1;
			}
			else
				Sys_Error ("Bad animating texture %s", tx->name);
		}
		
#define	ANIM_CYCLE	2
	// link them all together
		for (j=0 ; j<max ; j++)
		{
			tx2 = anims[j];
			if (!tx2)
				Sys_Error ("Missing frame %i of %s",j, tx->name);
			tx2->anim_total = max * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j+1) * ANIM_CYCLE;
			tx2->anim_next = anims[ (j+1)%max ];
			if (altmax)
				tx2->alternate_anims = altanims[0];
		}
		for (j=0 ; j<altmax ; j++)
		{
			tx2 = altanims[j];
			if (!tx2)
				Sys_Error ("Missing frame %i of %s",j, tx->name);
			tx2->anim_total = altmax * ANIM_CYCLE;
			tx2->anim_min = j * ANIM_CYCLE;
			tx2->anim_max = (j+1) * ANIM_CYCLE;
			tx2->anim_next = altanims[ (j+1)%altmax ];
			if (max)
				tx2->alternate_anims = anims[0];
		}
	}
}

void Mod_LoadBrushModel (model_t *mod, void *buffer)
{
	int			i, j;
	dheader_t	*header;
	dmodel_t 	*bm;
	
	loadmodel->type = mod_brush;
	
	header = (dheader_t *)buffer;

	i = LittleLong (header->version);
	mod->bspversion = i;
	if (i != BSPVERSION && i != HL_BSPVERSION)
		Sys_Error ("%s has wrong version number (%i should be %i)", mod->name, i, BSPVERSION);

// swap all the lumps
	mod_base = (byte *)header;

	for (i=0 ; i<(int)(sizeof(dheader_t)/4) ; i++)
		((int *)header)[i] = LittleLong ( ((int *)header)[i]);

// load into heap
	
	Mod_InitializeBrushStorage();
    loading_num_step = loading_num_step + 16;
	loading_step = 2;

	if (mod_loading_progress)
		CL_UpdateLoadingScreen(true);

	Mod_LoadVertexes (&header->lumps[LUMP_VERTEXES]);

    loading_cur_step++;
	if (mod_loading_progress)
		CL_UpdateLoadingScreen(false);

	Mod_LoadEdges (&header->lumps[LUMP_EDGES]);

    loading_cur_step++;
	if (mod_loading_progress)
		CL_UpdateLoadingScreen(false);

	Mod_LoadSurfedges (&header->lumps[LUMP_SURFEDGES]);

    loading_cur_step++;
	if (mod_loading_progress)
		CL_UpdateLoadingScreen(false);

	Mod_LoadEntities (&header->lumps[LUMP_ENTITIES]);

    loading_cur_step++;
	if (mod_loading_progress)
		CL_UpdateLoadingScreen(false);

	Mod_LoadTextures (&header->lumps[LUMP_TEXTURES]);
	Mod_LoadModelLighting(&header->lumps[LUMP_LIGHTING]);

    loading_cur_step++;
	if (mod_loading_progress)
		CL_UpdateLoadingScreen(false);

	Mod_LoadPlanes (&header->lumps[LUMP_PLANES]);

    loading_cur_step++;
	if (mod_loading_progress)
		CL_UpdateLoadingScreen(false);

	Mod_LoadTexinfo (&header->lumps[LUMP_TEXINFO]);

    loading_cur_step++;
	if (mod_loading_progress)
		CL_UpdateLoadingScreen(false);

	Mod_LoadFaces (&header->lumps[LUMP_FACES]);

    loading_cur_step++;
	if (mod_loading_progress)
		CL_UpdateLoadingScreen(false);

	Mod_LoadMarksurfaces (&header->lumps[LUMP_MARKSURFACES]);

    loading_cur_step++;
	if (mod_loading_progress)
		CL_UpdateLoadingScreen(false);

	Mod_LoadVisibility (&header->lumps[LUMP_VISIBILITY]);

    loading_cur_step++;
	if (mod_loading_progress)
		CL_UpdateLoadingScreen(false);

	Mod_LoadLeafs (&header->lumps[LUMP_LEAFS]);

    loading_cur_step++;
	if (mod_loading_progress)
		CL_UpdateLoadingScreen(false);

	Mod_LoadNodes (&header->lumps[LUMP_NODES]);

    loading_cur_step++;
	if (mod_loading_progress)
		CL_UpdateLoadingScreen(false);

	Mod_LoadClipnodes (&header->lumps[LUMP_CLIPNODES]);

    loading_cur_step++;
	if (mod_loading_progress)
		CL_UpdateLoadingScreen(false);

	Mod_LoadSubmodels (&header->lumps[LUMP_MODELS]);

    loading_cur_step++;
	if (mod_loading_progress)
		CL_UpdateLoadingScreen(false);

	Mod_MakeHull0 ();
	loading_cur_step++;

	loading_step = 2;

    loading_cur_step++;
	if (mod_loading_progress)
		CL_UpdateLoadingScreen(true);
	
	mod->numframes = 2;		// regular and alternate animation
	
//
// set up the submodels (FIXME: this is confusing)
//
	for (i=0 ; i<mod->numsubmodels ; i++)
	{
		bm = &mod->submodels[i];

		mod->hulls[0].firstclipnode = bm->headnode[0];
		for (j=1 ; j<MAX_MAP_HULLS ; j++)
		{
			mod->hulls[j].firstclipnode = bm->headnode[j];
			mod->hulls[j].lastclipnode = mod->numclipnodes-1;
		}
		
		mod->firstmodelsurface = bm->firstface;
		mod->nummodelsurfaces = bm->numfaces;
		
		VectorCopy (bm->maxs, mod->maxs);
		VectorCopy (bm->mins, mod->mins);

		mod->radius = RadiusFromBounds (mod->mins, mod->maxs);

		mod->numleafs = bm->visleafs;

		if (i < mod->numsubmodels-1)
		{	// duplicate the basic information
			char	name[13];

			snprintf (name, sizeof(name)-1, "*%i", i+1);
			loadmodel = Mod_FindName (name);
			*loadmodel = *mod;
			strcpy (loadmodel->name, name);
			mod = loadmodel;
		}
	}
}

void Mod_Print (void)
{
	int		i;
	model_t	*mod;

	Con_Printf ("Cached models:\n");
	for (i=0, mod=mod_known ; i < mod_numknown ; i++, mod++)
	{
		Con_Printf ("%8p : %s\n",mod->cache.data, mod->name);
	}
}