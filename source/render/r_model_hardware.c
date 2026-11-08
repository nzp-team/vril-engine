/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2026 NZ:P Team.

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

const qboolean mod_loading_progress = true, mod_external_sprites = true;
const qboolean mod_permanent_sprites = R_KEEP_SPRITES;
const int mod_lighting_channels = 3;
static void Mod_PrepareAlias(aliashdr_t *header);
static qboolean mod_building_cache;
aliashdr_t	*pheader;

stvert_t	stverts[MAXALIASVERTS];
mtriangle_t	triangles[MAXALIASTRIS];

// a pose is a single set of vertexes.  a frame may be
// an animating sequence of poses
trivertx_t	*poseverts[MAXALIASFRAMES];
int			posenum;

byte		**player_8bit_texels_tbl;
byte		*player_8bit_texels;
void * Mod_LoadAliasFrame (void * pin, maliasframedesc_t *frame)
{
	trivertx_t		*pinframe;
	int				i;
	daliasframe_t	*pdaliasframe;
	
	pdaliasframe = (daliasframe_t *)pin;

	memcpy(frame->name, pdaliasframe->name, sizeof(frame->name));
	frame->name[sizeof(frame->name) - 1] = 0;
	frame->firstpose = posenum;
	frame->numposes = 1;

	for (i=0 ; i<3 ; i++)
	{
	// these are byte values, so we don't have to worry about
	// endianness
		frame->bboxmin.v[i] = pdaliasframe->bboxmin.v[i];
		frame->bboxmax.v[i] = pdaliasframe->bboxmax.v[i];
	}

	pinframe = (trivertx_t *)(pdaliasframe + 1);

	poseverts[posenum] = pinframe;
	posenum++;

	pinframe += pheader->numverts;

	return (void *)pinframe;
}

void *Mod_LoadAliasGroup (void * pin,  maliasframedesc_t *frame)
{
	daliasgroup_t		*pingroup;
	int					i, numframes;
	daliasinterval_t	*pin_intervals;
	void				*ptemp;
	
	pingroup = (daliasgroup_t *)pin;

	numframes = LittleLong (pingroup->numframes);

	frame->firstpose = posenum;
	frame->numposes = numframes;

	for (i=0 ; i<3 ; i++)
	{
	// these are byte values, so we don't have to worry about endianness
		frame->bboxmin.v[i] = pingroup->bboxmin.v[i];
		frame->bboxmax.v[i] = pingroup->bboxmax.v[i];
	}

	pin_intervals = (daliasinterval_t *)(pingroup + 1);

	frame->interval = LittleFloat (pin_intervals->interval);

	pin_intervals += numframes;

	ptemp = (void *)pin_intervals;

	for (i=0 ; i<numframes ; i++)
	{
		poseverts[posenum] = (trivertx_t *)((daliasframe_t *)ptemp + 1);
		posenum++;

		ptemp = (trivertx_t *)((daliasframe_t *)ptemp + 1) + pheader->numverts;
	}

	return ptemp;
}

typedef struct
{
	short		x, y;
} floodfill_t;

extern unsigned d_8to24table[];

// must be a power of 2
#define FLOODFILL_FIFO_SIZE 0x1000
#define FLOODFILL_FIFO_MASK (FLOODFILL_FIFO_SIZE - 1)

#define FLOODFILL_STEP( off, dx, dy ) \
{ \
	if (pos[off] == fillcolor) \
	{ \
		pos[off] = 255; \
		fifo[inpt].x = x + (dx), fifo[inpt].y = y + (dy); \
		inpt = (inpt + 1) & FLOODFILL_FIFO_MASK; \
	} \
	else if (pos[off] != 255) fdc = pos[off]; \
}


void Mod_FloodFillSkin( byte *skin, int skinwidth, int skinheight )
{
	byte				fillcolor = *skin; // assume this is the pixel to fill
	floodfill_t			fifo[FLOODFILL_FIFO_SIZE];
	int					inpt = 0, outpt = 0;
	int					filledcolor = -1;
	int					i;

	if (filledcolor == -1)
	{
		filledcolor = 0;
		// attempt to find opaque black
		for (i = 0; i < 256; ++i)
			if (d_8to24table[i] == (255 << 0)) // alpha 1.0
			{
				filledcolor = i;
				break;
			}
	}

	// can't fill to filled color or to transparent color (used as visited marker)
	if ((fillcolor == filledcolor) || (fillcolor == 255))
	{
		//printf( "not filling skin from %d to %d\n", fillcolor, filledcolor );
		return;
	}

	fifo[inpt].x = 0, fifo[inpt].y = 0;
	inpt = (inpt + 1) & FLOODFILL_FIFO_MASK;

	while (outpt != inpt)
	{
		int			x = fifo[outpt].x, y = fifo[outpt].y;
		int			fdc = filledcolor;
		byte		*pos = &skin[x + skinwidth * y];

		outpt = (outpt + 1) & FLOODFILL_FIFO_MASK;

		if (x > 0)				FLOODFILL_STEP( -1, -1, 0 );
		if (x < skinwidth - 1)	FLOODFILL_STEP( 1, 1, 0 );
		if (y > 0)				FLOODFILL_STEP( -skinwidth, 0, -1 );
		if (y < skinheight - 1)	FLOODFILL_STEP( skinwidth, 0, 1 );
		skin[x + skinwidth * y] = fdc;
	}
}

static qboolean Mod_IsZombieModel (const char *name)
{
	return !strcmp(name, "models/ai/zb%.mdl") ||
		!strcmp(name, "models/ai/zbc%.mdl") ||
		!strcmp(name, "models/ai/zfull.mdl") ||
		!strcmp(name, "models/ai/zcfull.mdl") ||
		!strcmp(name, "models/ai/zh^.mdl") ||
		!strcmp(name, "models/ai/zhc^.mdl") ||
		!strcmp(name, "models/ai/zal(.mdl") ||
		!strcmp(name, "models/ai/zalc(.mdl") ||
		!strcmp(name, "models/ai/zar(.mdl") ||
		!strcmp(name, "models/ai/zarc(.mdl");
}

void *Mod_LoadAllSkins(int count, daliasskintype_t *input)
{
    int skin, frame, repeat, size = pheader->skinwidth * pheader->skinheight;
    qboolean paletted = Hyena_PreferredPaletteBits() == 4;
    qboolean viewmodel = strstr(loadmodel->name, "/v_") != NULL;
    qboolean gun = !strncmp(loadmodel->name, "models/weapons", 14);
    size_t budget = Hyena_TextureBudget();
    if (count < 1 || count > MAX_SKINS) Sys_Error("Invalid # of skins: %d", count);
    if (count == 1 && LittleLong(input->type) == ALIAS_SKIN_SINGLE && Mod_IsZombieModel(loadmodel->name)) {
        extern int zombie_skins[NUM_ZOMBIE_SKINS];
        pheader->numskins = NUM_ZOMBIE_SKINS;
        for (skin = 0; skin < NUM_ZOMBIE_SKINS; skin++)
            for (frame = 0; frame < 4; frame++) pheader->gl_texturenum[skin][frame] = zombie_skins[skin];
        return (byte *)(input + 1) + size;
    }
    for (skin = 0; skin < count; skin++) {
        int frames = 1;
        byte *pixels = (byte *)(input + 1);
        qboolean group = LittleLong(input->type) != ALIAS_SKIN_SINGLE;
        if (group) {
            daliasskingroup_t *header = (daliasskingroup_t *)pixels;
            frames = LittleLong(header->numskins);
            if (frames < 1) Sys_Error("Invalid alias skin group");
            pixels = (byte *)((daliasskininterval_t *)(header + 1) + frames);
        }
        for (frame = 0; frame < frames; frame++, pixels += size) {
            char name[MAX_OSPATH], external[MAX_OSPATH], base[MAX_QPATH];
            int texture = -1;
            COM_StripExtension(loadmodel->name, base);
            if (group) {
                snprintf(external, sizeof(external), "%s_%d_%d", base, skin, frame);
                snprintf(name, sizeof(name), "%s_%d_%d", loadmodel->name, skin, frame);
            } else {
                snprintf(external, sizeof(external), "%s.mdl_%d", base, skin);
                snprintf(name, sizeof(name), "%s_%d", loadmodel->name, skin);
            }
            if (!mod_building_cache && cls.state != ca_dedicated && !vid_headless) {
                Mod_FloodFillSkin(pixels, pheader->skinwidth, pheader->skinheight);
                if (!strcmp(loadmodel->name, "models/weapons/m1911/v_biatch_left.mdl") ||
                    !strcmp(loadmodel->name, "models/weapons/m1911/v_biatch_right.mdl"))
                    strcpy(external, "models/weapons/m1911/v_biatch.mdl_0");
                if (gun && skin && budget && budget < 2 * 1024 * 1024)
                    texture = Image_LoadImage("models/weapons/v_papskin", IMAGE_PCX, HYE_FILTER_LINEAR, true, false);
                if (texture < 0 && paletted && !viewmodel) texture = loadpcxas4bpp(external, HYE_FILTER_LINEAR);
                if (texture < 0) texture = Image_LoadImage(external, IMAGE_TGA | (paletted && !viewmodel ? 0 : IMAGE_PCX), HYE_FILTER_LINEAR, R_KEEP_ALIAS_TEXTURES, false);
                if (texture < 0 && paletted && !viewmodel)
                    texture = Hyena_LoadPalettedTexture(name, pheader->skinwidth, pheader->skinheight, pixels,
                        (byte *)d_8to24table, 4, NULL, HYE_FILTER_LINEAR, 0, true, true);
                if (texture < 0) texture = Image_LoadTexture(name, pheader->skinwidth, pheader->skinheight, pixels,
                    HYE_TEXTURE_INDEX8, HYE_FILTER_LINEAR, 0, true, true, paletted);
            }
            pheader->gl_texturenum[skin][frame & 3] = texture;
        }
        repeat = frames;
        for (; frame < 4; frame++) pheader->gl_texturenum[skin][frame] = pheader->gl_texturenum[skin][frame - repeat];
        input = (daliasskintype_t *)pixels;
    }
    return input;
}

static void Mod_AliasBounds(model_t *model, aliashdr_t *header)
{
	int pose, vertex, axis;
	for (axis = 0; axis < 3; axis++) {
		model->mins[axis] = 9999999;
		model->maxs[axis] = -9999999;
	}
	for (pose = 0; pose < posenum; pose++) {
		for (vertex = 0; vertex < header->numverts; vertex++) {
			for (axis = 0; axis < 3; axis++) {
				float value = ((const byte *)&poseverts[pose][vertex])[axis] * header->scale[axis] + header->scale_origin[axis];
				if (value < model->mins[axis])
					model->mins[axis] = value;
				if (value > model->maxs[axis])
					model->maxs[axis] = value;
			}
		}
	}
	model->radius = RadiusFromBounds(model->mins, model->maxs);
}

// Prepared alias data contains relative offsets. Process-local model pointers
// and texture handles are cleared on disk and rebound for each load.

typedef struct {
    float    mins[3], maxs[3];
    uint32_t bytes, reserved;
} alias_cache_header_t;

static uint32_t
Mod_AliasCacheVariant(void)
{
    const uint32_t fields[] = { 0x4d444c02u,                   sizeof(aliashdr_t), sizeof(maliasframedesc_t),
                                sizeof(trivertx_t),            sizeof(void *),     LittleLong(1) == 1,
                                Hyena_PackedArraysSupported(), Hyena_MorphMeshesSupported() };

    return AssetCache_HashWords(fields, sizeof(fields) / sizeof(fields[0]));
}

static qboolean
Mod_AliasSourceValid(const void * buffer, size_t size)
{
    const mdl_t * input = buffer;
    const byte * cursor, * end = (const byte *) buffer + size;
    int skins, width, height, vertices, triangles_count, frames, i, j, poses = 0;

    if (size < sizeof(*input) || LittleLong(input->ident) != IDPOLYHEADER ||
      LittleLong(input->version) != ALIAS_VERSION)
        return false;

    skins           = LittleLong(input->numskins);
    width           = LittleLong(input->skinwidth);
    height          = LittleLong(input->skinheight);
    vertices        = LittleLong(input->numverts);
    triangles_count = LittleLong(input->numtris);
    frames          = LittleLong(input->numframes);
    if (skins < 1 || skins > MAX_SKINS || width < 1 || width > 4096 || (width & 3) ||
      height < 1 || height > MAX_LBM_HEIGHT || vertices < 1 || vertices > MAXALIASVERTS ||
      triangles_count < 1 || triangles_count > MAXALIASTRIS || frames < 1 || frames > MAXALIASFRAMES)
        return false;

    for (i = 0; i < 3; ++i)
        if (!isfinite(LittleFloat(input->scale[i])) || !isfinite(LittleFloat(input->scale_origin[i])) ||
          !isfinite(LittleFloat(input->eyeposition[i]))) return false;

    cursor = (const byte *) (input + 1);
    for (i = 0; i < skins; ++i) {
        int type, count = 1;
        size_t bytes;
        if (end - cursor < 4) return false;

        memcpy(&type, cursor, 4);
        type    = LittleLong(type);
        cursor += 4;
        if (type == ALIAS_SKIN_GROUP) {
            if (end - cursor < 4) return false;

            memcpy(&count, cursor, 4);
            count   = LittleLong(count);
            cursor += 4;
            if (count < 1 || count > MAXALIASFRAMES || (size_t) (end - cursor) < (size_t) count * 4)
                return false;

            cursor += count * 4;
        } else if (type != ALIAS_SKIN_SINGLE) return false;

        bytes = (size_t) width * height * count;
        if (bytes > (size_t) (end - cursor)) return false;

        cursor += bytes;
    }
    if ((size_t) (end - cursor) < (size_t) vertices * sizeof(stvert_t)
      + (size_t) triangles_count * sizeof(dtriangle_t)) return false;

    cursor += vertices * sizeof(stvert_t);
    for (i = 0; i < triangles_count; ++i) {
        const dtriangle_t * triangle = (const dtriangle_t *) cursor;
        for (j = 0; j < 3; ++j) {
            int index = LittleLong(triangle->vertindex[j]);
            if (index < 0 || index >= vertices) return false;
        }
        cursor += sizeof(*triangle);
    }
    for (i = 0; i < frames; ++i) {
        int type, count = 1;
        size_t bytes;
        if (end - cursor < 4) return false;

        memcpy(&type, cursor, 4);
        type    = LittleLong(type);
        cursor += 4;
        if (type == ALIAS_GROUP) {
            float interval;
            if ((size_t) (end - cursor) < sizeof(daliasgroup_t)) return false;

            memcpy(&count, cursor, 4);
            count   = LittleLong(count);
            cursor += sizeof(daliasgroup_t);
            if (count < 1 || count > MAXALIASFRAMES || (size_t) (end - cursor) < (size_t) count * 4)
                return false;

            memcpy(&interval, cursor, 4);
            if (!(LittleFloat(interval) > 0)) return false;

            cursor += count * 4;
        } else if (type != ALIAS_SINGLE) return false;

        bytes = (size_t) count * (sizeof(daliasframe_t) + vertices * sizeof(trivertx_t));
        if (bytes > (size_t) (end - cursor) || count > MAXALIASFRAMES - poses) return false;

        poses += count;
        for (j = 0; j < count; ++j) {
            cursor += sizeof(daliasframe_t) + vertices * sizeof(trivertx_t);
        }
    }
    return true;
} /* Mod_AliasSourceValid */

static qboolean
Mod_AliasCacheRange(int offset, size_t bytes, size_t size, size_t minimum)
{
    return offset >= 0 && (size_t) offset >= minimum && !(offset & 1) &&
           (size_t) offset <= size && bytes <= size - offset;
}

static size_t
Mod_AliasDataSize(const aliashdr_t * header)
{
    int physical = header->numposes, i;

    if (header->posemap) {
        const unsigned short * map = (const unsigned short *) ((const byte *) header + header->posemap);
        physical = 0;
        for (i = 0; i < header->numposes; ++i)
            if (map[i] >= physical) physical = map[i] + 1;
    }
    if (header->posedata)
        return header->posedata + (size_t) physical * header->poseverts * sizeof(trivertx_t);

    return header->commands + (size_t) header->command_words * sizeof(int);
}

static qboolean
Mod_AliasCacheValid(const aliashdr_t * header, size_t size, const mdl_t * input)
{
    size_t minimum;
    const int * commands;
    int i, cursor, words, vertices = 0, physical = header->numposes;
    qboolean packed = Hyena_PackedArraysSupported();

    if (size < sizeof(*header) || header->model || header->numframes != LittleLong(input->numframes) ||
      header->numframes < 1 || header->numframes > MAXALIASFRAMES ||
      header->numverts != LittleLong(input->numverts) || header->numtris != LittleLong(input->numtris) ||
      header->skinwidth != LittleLong(input->skinwidth) || header->skinheight != LittleLong(input->skinheight) ||
      header->numposes < 1 || header->numposes > MAXALIASFRAMES || header->poseverts < 1 ||
      header->poseverts > MAXALIASTRIS * 3 || header->command_vertices < 3 ||
      header->command_vertices > MAXALIASTRIS * 3 || header->numskins < 1 || header->numskins > MAX_SKINS ||
      header->vertex_scale != (packed ? 128 : 1)) return false;

    minimum = sizeof(*header) + (header->numframes - 1) * sizeof(header->frames[0]);
    if (minimum > size || !Mod_AliasCacheRange(header->commands, 4, size, minimum) ||
      (header->commands & 3)) return false;

    words = min((size - header->commands) / 4, MAXALIASTRIS * 7 + 2);
    if (packed) {
        if (header->command_words < 2 || header->command_words > words) return false;

        words = header->command_words;
    } else if (header->command_words) return false;

    for (i = 0; i < header->numframes; ++i) {
        const maliasframedesc_t * frame = &header->frames[i];
        if (frame->firstpose < 0 || frame->numposes < 1 || frame->firstpose > header->numposes - frame->numposes ||
          !memchr(frame->name, 0, sizeof(frame->name))) return false;
    }
    commands = (const int *) ((const byte *) header + header->commands);
    cursor   = packed ? 1 : 0;
    if (packed && commands[0] != header->command_words) return false;

    while (cursor < words) {
        int count = commands[cursor++], stride = packed && header->numposes > 1 ? 1 : 2;
        if (!count) break;
        if (count < -MAXALIASTRIS * 3 || count > MAXALIASTRIS * 3 || abs(count) < 3 ||
          abs(count) > (words - cursor) / stride) return false;

        vertices += abs(count);
        cursor   += abs(count) * stride;
    }
    if ((packed && cursor != words) || !cursor || commands[cursor - 1] || vertices != header->command_vertices)
        return false;

    if (header->command_remap) {
        const unsigned short * remap;
        if (!Mod_AliasCacheRange(header->command_remap, (size_t) vertices * 2, size, minimum)) return false;

        remap = (const unsigned short *) ((const byte *) header + header->command_remap);
        for (i = 0; i < vertices; ++i) if (remap[i] >= header->poseverts) return false;
    }
    if (header->posemap) {
        const unsigned short * map;
        if (!Mod_AliasCacheRange(header->posemap, (size_t) header->numposes * 2, size, minimum)) return false;

        map      = (const unsigned short *) ((const byte *) header + header->posemap);
        physical = 0;
        for (i = 0; i < header->numposes; ++i) {
            if (map[i] >= header->numposes) return false;

            if (map[i] >= physical) physical = map[i] + 1;
        }
    }
    if (packed && header->numposes == 1)
        return !header->posedata && !header->posemap && !header->command_remap;

    return !(header->posedata & 3) && Mod_AliasCacheRange(header->posedata,
             (size_t) physical * header->poseverts * sizeof(trivertx_t), size, minimum);
} /* Mod_AliasCacheValid */

static qboolean
Mod_RestoreAlias(model_t * model, const mdl_t * input, uint32_t crc)
{
    asset_cache_data_t entry;
    alias_cache_header_t prefix;
    const aliashdr_t * stored;
    int i;

    if (!AssetCache_Load(ASSET_CACHE_MODEL, model->name, crc, Mod_AliasCacheVariant(), &entry)) return false;

    if (entry.size < sizeof(prefix) + sizeof(*stored)) goto invalid;
    memcpy(&prefix, entry.data, sizeof(prefix));
    stored = (const aliashdr_t *) ((const byte *) entry.data + sizeof(prefix));
    if (prefix.bytes != entry.size - sizeof(prefix) || !Mod_AliasCacheValid(stored, prefix.bytes, input))
        goto invalid;
    for (i = 0; i < 3; ++i)
        if (!isfinite(prefix.mins[i]) || !isfinite(prefix.maxs[i]) || prefix.mins[i] > prefix.maxs[i]) goto invalid;
    if (mod_building_cache) { free(entry.data); return true; }
    pheader = Hunk_AllocName(prefix.bytes, loadname);
    memcpy(pheader, stored, prefix.bytes);
    pheader->model = model;
    Mod_LoadAllSkins(LittleLong(input->numskins), (daliasskintype_t *) (input + 1));
    model->flags     = LittleLong(input->flags);
    model->synctype  = LittleLong(input->synctype);
    model->numframes = pheader->numframes;
    model->type      = mod_alias;
    memcpy(model->mins, prefix.mins, sizeof(prefix.mins));
    memcpy(model->maxs, prefix.maxs, sizeof(prefix.maxs));
    model->radius = RadiusFromBounds(model->mins, model->maxs);
    Cache_Alloc(&model->cache, prefix.bytes, loadname);
    memcpy(model->cache.data, pheader, prefix.bytes);
    free(entry.data);
    return true;

invalid:
    free(entry.data);
    return false;
} /* Mod_RestoreAlias */

static qboolean
Mod_StoreAlias(model_t * model, uint32_t crc, size_t bytes)
{
    asset_cache_data_t entry    = { 0 };
    alias_cache_header_t prefix = { { 0 }, { 0 }, 0, 0 };
    aliashdr_t * stored;

    prefix.bytes = bytes;
    memcpy(prefix.mins, model->mins, sizeof(prefix.mins));
    memcpy(prefix.maxs, model->maxs, sizeof(prefix.maxs));
    entry.size = sizeof(prefix) + bytes;
    entry.data = malloc(entry.size);
    if (!entry.data) return false;

    memcpy(entry.data, &prefix, sizeof(prefix));
    stored = (aliashdr_t *) ((byte *) entry.data + sizeof(prefix));
    memcpy(stored, pheader, bytes);
    stored->model = NULL;
    memset(stored->gl_texturenum, 0, sizeof(stored->gl_texturenum));
    qboolean stored_ok = AssetCache_Store(ASSET_CACHE_MODEL, model->name, crc, Mod_AliasCacheVariant(), &entry);
    free(entry.data);
    return stored_ok;
}

static qboolean Mod_LoadAliasModelInternal(model_t *mod, void *buffer)
{
	int					i, j;
	mdl_t				*pinmodel;
	stvert_t			*pinstverts;
	dtriangle_t			*pintriangles;
	int					version, numframes;
	int					size;
	daliasframetype_t	*pframetype;
	daliasskintype_t	*pskintype;
	int					start, total;
    uint32_t source_crc = 0;
    qboolean use_cache = AssetCache_Enabled(), stored_ok = false;

// some models are special
	// NOTE: comparing not only with player.mdl, but with all models
	// begin with "player" coz we need to support DME models as well!
	if (!strncmp(mod->name, "progs/player", 12))
		mod->modhint = MOD_PLAYER;
	else if (!strcmp(mod->name, "progs/eyes.mdl"))
		mod->modhint = MOD_EYES;
	else if (!strcmp(mod->name, "progs/flame0.mdl") ||
		 !strcmp(mod->name, "progs/flame.mdl") ||
		 !strcmp(mod->name, "progs/flame2.mdl"))
		mod->modhint = MOD_FLAME;
	else if (!strcmp(mod->name, "progs/bolt.mdl") ||
		 !strcmp(mod->name, "models/misc/bolt2.mdl") ||
		 !strcmp(mod->name, "progs/bolt3.mdl"))
		mod->modhint = MOD_THUNDERBOLT;
	else if (!strcmp(mod->name, "progs/VModels/v_Colt.mdl") || //JUKKI Add nzp weapons here please plox
		 !strcmp(mod->name, "progs/VModels/v_kar.mdl") ||
		 !strcmp(mod->name, "progs/VModels/v_thomp.mdl"))
		mod->modhint = MOD_WEAPON;
	else if (!strcmp(mod->name, "progs/lavaball.mdl"))
		mod->modhint = MOD_LAVABALL;
	else if (!strcmp(mod->name, "progs/spike.mdl") ||
		 !strcmp(mod->name, "progs/s_spike.mdl"))
		mod->modhint = MOD_SPIKE;
	else if (!strcmp(mod->name, "progs/shambler.mdl"))
		mod->modhint = MOD_SHAMBLER;
	else
		mod->modhint = MOD_NORMAL;
	
    if (!Mod_AliasSourceValid(buffer, com_filesize))
        Sys_Error("Invalid or truncated alias model: %s", mod->name);
    if (use_cache)
        source_crc = AssetCache_CRC32(buffer, com_filesize);
    start = Hunk_LowMark();
    pinmodel = (mdl_t *)buffer;
    if (use_cache && Mod_RestoreAlias(mod, pinmodel, source_crc)) {
        Hunk_FreeToLowMark(start);
        return true;
    }

	version = LittleLong (pinmodel->version);
	if (version != ALIAS_VERSION)
		Sys_Error ("%s has wrong version number (%i should be %i)",
				 mod->name, version, ALIAS_VERSION);

//
// allocate space for a working header, plus all the data except the frames,
// skin and group info
//
	size = 	sizeof (aliashdr_t) 
			+ (LittleLong (pinmodel->numframes) - 1) *
			sizeof (pheader->frames[0]);
	pheader = Hunk_AllocName (size, loadname);
	pheader->model = mod;
	pheader->vertex_scale = 1;
	
	mod->flags = LittleLong (pinmodel->flags);

//
// endian-adjust and copy the data, starting with the alias model header
//
	pheader->boundingradius = LittleFloat (pinmodel->boundingradius);
	pheader->numskins = LittleLong (pinmodel->numskins);
	pheader->skinwidth = LittleLong (pinmodel->skinwidth);
	pheader->skinheight = LittleLong (pinmodel->skinheight);

	if (pheader->skinheight > MAX_LBM_HEIGHT)
		Sys_Error ("model %s has a skin taller than %d", mod->name,
				   MAX_LBM_HEIGHT);

	pheader->numverts = LittleLong (pinmodel->numverts);

	if (pheader->numverts <= 0)
		Sys_Error ("model %s has no vertices", mod->name);

	//Con_Printf("numverts: %d\n", pheader->numverts);
	if (pheader->numverts > MAXALIASVERTS)
		Sys_Error ("model %s has too many vertices", mod->name);

	pheader->numtris = LittleLong (pinmodel->numtris);

	if (pheader->numtris <= 0)
		Sys_Error ("model %s has no triangles", mod->name);

	pheader->numframes = LittleLong (pinmodel->numframes);
	numframes = pheader->numframes;
	if (numframes < 1)
		Sys_Error ("Invalid # of frames: %d\n", numframes);

	pheader->size = LittleFloat (pinmodel->size) * ALIAS_BASE_SIZE_RATIO;
	mod->synctype = LittleLong (pinmodel->synctype);
	mod->numframes = pheader->numframes;

	for (i=0 ; i<3 ; i++)
	{
		pheader->scale[i] = LittleFloat (pinmodel->scale[i]);
		pheader->scale_origin[i] = LittleFloat (pinmodel->scale_origin[i]);
		pheader->eyeposition[i] = LittleFloat (pinmodel->eyeposition[i]);
	}


//
// load the skins
//
	pskintype = (daliasskintype_t *)&pinmodel[1];
	pskintype = Mod_LoadAllSkins(pheader->numskins, pskintype);

//
// load base s and t vertices
//
	pinstverts = (stvert_t *)pskintype;

	for (i=0 ; i<pheader->numverts ; i++)
	{
		stverts[i].onseam = LittleLong (pinstverts[i].onseam);
		stverts[i].s = LittleLong (pinstverts[i].s);
		stverts[i].t = LittleLong (pinstverts[i].t);
	}

//
// load triangle lists
//
	pintriangles = (dtriangle_t *)&pinstverts[pheader->numverts];

	for (i=0 ; i<pheader->numtris ; i++)
	{
		triangles[i].facesfront = LittleLong (pintriangles[i].facesfront);

		for (j=0 ; j<3 ; j++)
		{
			triangles[i].vertindex[j] =
					LittleLong (pintriangles[i].vertindex[j]);
		}
	}

//
// load the frames
//
	posenum = 0;
	pframetype = (daliasframetype_t *)&pintriangles[pheader->numtris];

	for (i=0 ; i<numframes ; i++)
	{
		aliasframetype_t	frametype;

		frametype = LittleLong (pframetype->type);

		if (frametype == ALIAS_SINGLE)
		{
			pframetype = (daliasframetype_t *)
					Mod_LoadAliasFrame (pframetype + 1, &pheader->frames[i]);
		}
		else
		{
			pframetype = (daliasframetype_t *)
					Mod_LoadAliasGroup (pframetype + 1, &pheader->frames[i]);
		}
	}

	pheader->numposes = posenum;

	mod->type = mod_alias;

	Mod_AliasBounds(mod, pheader);

	//
	// build the draw lists
	//
	Mod_PrepareAlias(pheader);
	R_BuildAliasMesh(mod, pheader);

//
// move the complete, relocatable alias model to the cache
//	
    total = Mod_AliasDataSize(pheader);
    if (use_cache)
        stored_ok = Mod_StoreAlias(mod, source_crc, total);
    if (mod_building_cache) {
        Hunk_FreeToLowMark(start);
        return stored_ok;
    }
	
	Cache_Alloc (&mod->cache, total, loadname);
	if (!mod->cache.data)
		return false;
	memcpy (mod->cache.data, pheader, total);

	Hunk_FreeToLowMark (start);
    return stored_ok;
}


void Mod_LoadAliasModel(model_t *mod, void *buffer)
{
    Mod_LoadAliasModelInternal(mod, buffer);
}

qboolean Mod_BuildAliasCache(const char *name)
{
    model_t model = {0};
    model_t *saved_model = loadmodel;
    aliashdr_t *saved_header = pheader;
    char saved_name[MAX_QPATH];
    int saved_size = com_filesize, length;
    qboolean prepared;
    byte *data;
    FILE *file;
    if (!AssetCache_Enabled() || strlen(name) >= sizeof(model.name)) return false;
    length = COM_FOpenFile((char *)name, &file);
    if (!file) return false;
    if (length <= 0 || length > 16 * 1024 * 1024 || !(data = malloc(length))) {
        fclose(file); return false;
    }
    if (fread(data, 1, length, file) != (size_t)length || !Mod_AliasSourceValid(data, length)) {
        free(data); fclose(file); return false;
    }
    fclose(file);
    Q_strlcpy(model.name, name, sizeof(model.name));
    Q_strlcpy(saved_name, loadname, sizeof(saved_name));
    COM_FileBase(model.name, loadname);
    loadmodel = &model;
    mod_building_cache = true;
    prepared = Mod_LoadAliasModelInternal(&model, data);
    mod_building_cache = false;
    loadmodel = saved_model; pheader = saved_header;
    Q_strlcpy(loadname, saved_name, sizeof(saved_name));
    com_filesize = saved_size;
    free(data);
    return prepared;
}



void Mod_ClearModelStorage(void)
{
    int i;
    R_ClearLightmaps();
    R_ClearSceneCaches();
    Hyena_ClearMorphMeshes();
    R_ClearAliasTopologyCache();
    Hyena_DestroyTextures();
    Sky_ClearTextures();
    for (i = 0; i < MAX_LIGHTMAPS; ++i)
        lightmap_index[i] = -1;
}

mspriteframe_t *Mod_LoadSpritePixels(const char *name, int width, int height, int bytes, const void *pixels)
{
    mspriteframe_t *frame = Hunk_AllocName(sizeof(*frame), loadname);
    frame->gl_texturenum = -1;
    if (cls.state == ca_dedicated || vid_headless)
        return frame;
    frame->gl_texturenum = Image_LoadImage((char *)name, IMAGE_TGA, HYE_FILTER_LINEAR, true, false);
    if (frame->gl_texturenum < 0 && pixels)
        frame->gl_texturenum = Image_LoadTexture((char *)name, width, height, (byte *)pixels,
            bytes == 4 ? HYE_TEXTURE_RGBA8 : HYE_TEXTURE_INDEX8, HYE_FILTER_LINEAR, 0,
            true, true, mod_permanent_sprites);
    return frame;
}


static void Mod_PrepareAlias(aliashdr_t *header)
{
    int pose, vertex, axis;
    if (!Hyena_PackedArraysSupported()) return;
    header->vertex_scale = 128;
    for (axis = 0; axis < 3; axis++) {
        header->scale_origin[axis] += header->scale[axis] * 128;
        header->eyeposition[axis] += header->scale[axis] * 128;
    }
    for (pose = 0; pose < header->numposes; pose++)
        for (vertex = 0; vertex < header->numverts; vertex++) {
            byte raw[3];
            memcpy(raw, poseverts[pose][vertex].v, sizeof(raw));
            for (axis = 0; axis < 3; axis++) poseverts[pose][vertex].v[axis] = raw[axis] - 128;
        }
}

void Mod_InitializeBrushStorage(void) { }
void Mod_LoadModelLighting(lump_t *lump) { Mod_LoadLighting(lump); }
void Mod_PreparePlanes(void) { }
void Mod_PrepareNode(mnode_t *node, int plane) { (void)node; (void)plane; }

texture_t *Mod_CreateTexture(miptex_t *input)
{
    texture_t *texture;
    char name[MAX_OSPATH];
    byte *pixels = NULL;
    int i, levels = r_mipmaps.value > 0 ? 3 : 0;
    if ((input->width & 15) || (input->height & 15)) Sys_Error("Texture %s is not 16 aligned", input->name);
    texture = Hunk_AllocName(sizeof(*texture), loadname);
    memcpy(texture->name, input->name, sizeof(texture->name));
    texture->width = input->width;
    texture->height = input->height;
    texture->fullbright = texture->dt_texturenum = -1;
    for (i = 0; i < MIPLEVELS; i++) texture->offsets[i] = input->offsets[i] + sizeof(*texture) - sizeof(*input);
    texture->gl_texturenum = -1;
    if (cls.state == ca_dedicated || vid_headless || !strncmp(input->name, "sky", 3)) return texture;
    snprintf(name, sizeof(name), "textures/%s", input->name);
    texture->gl_texturenum = Image_LoadImage(name, IMAGE_TGA | IMAGE_PNG | IMAGE_JPG, HYE_FILTER_LINEAR, false, true);
    if (texture->gl_texturenum < 0) {
        if (loadmodel->bspversion == HL_BSPVERSION) {
            if (Hyena_PreferredPaletteBits() == 4) texture->gl_texturenum = WAD3_LoadTextureClut4(input);
            if (texture->gl_texturenum < 0) {
                pixels = WAD3_LoadTexture(input);
                if (pixels) texture->gl_texturenum = Image_LoadTexture(input->name, input->width, input->height,
                    pixels, HYE_TEXTURE_RGBA8, HYE_FILTER_LINEAR, levels, input->name[0] == '{', false, false);
                free(pixels);
            }
        } else
            texture->gl_texturenum = Image_LoadTexture(input->name, input->width, input->height,
                (byte *)input + input->offsets[0], HYE_TEXTURE_INDEX8, HYE_FILTER_LINEAR, levels, true, false, true);
    }
    if (loadmodel->bspversion != HL_BSPVERSION && input->offsets[0] && input->name[0] != '*') {
        int count = input->width * input->height;
        byte *raw = (byte *)input + input->offsets[0];
        for (i = 0; i < count; i++) if (raw[i] >= 224 && raw[i] != 255) break;
        if (i < count) {
            pixels = malloc(count);
            if (!pixels) Sys_Error("Out of fullbright texture memory");
            for (i = 0; i < count; i++) pixels[i] = raw[i] >= 224 ? raw[i] : 255;
            snprintf(name, sizeof(name), "fullbright_mask_%s", input->name);
            texture->fullbright = Image_LoadTexture(name, input->width, input->height, pixels,
                HYE_TEXTURE_INDEX8, HYE_FILTER_LINEAR, levels, true, false, true);
            free(pixels);
        }
    }
    if (texture->gl_texturenum < 0) texture->gl_texturenum = nonetexture;
    return texture;
}

void Mod_PrepareSurface(msurface_t *surface, int plane)
{
    const char *name = surface->texinfo->texture->name;
    int i;
    (void)plane;
    if (!strncmp(name, "sky", 3)) surface->flags |= SURF_DRAWSKY | SURF_DRAWTILED;
    else if (name[0] == '*') {
        surface->flags |= SURF_DRAWTURB | SURF_DRAWTILED;
        for (i = 0; i < 2; i++) { surface->extents[i] = 16384; surface->texturemins[i] = -8192; }
        if (Hyena_HardwareClipping()) R_SubdivideSurface(surface);
        else R_SurfacePolygon(surface);
    } else if (!Q_strncasecmp((char *)name, "nodraw", 6)) surface->flags |= TEXFLAG_NODRAW;
    else if (!strncmp(name, "env", 3) || !strncmp(name, "glass", 5)) surface->flags |= TEXFLAG_REFLECT;
    else if (strstr(name, "light")) surface->flags |= TEXFLAG_LIGHT;
    else surface->flags |= TEXFLAG_NORMAL;
}
