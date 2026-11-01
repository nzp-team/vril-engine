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
// r_surf.c: surface-related refresh code

#include "../nzportable_def.h"
#include "r_config.h"

#define LIGHTMAP_MAX_SIZE 256
static unsigned lightmap_values[33 * 33 * 3];
static byte lightmap_scale[256];
static unsigned lightmap_scale_value = ~0u;
int r_lightmap_size;
static int lightmap_pages, last_lightmap_allocated;
static hyena_texture_format_t lightmap_format;
static unsigned short *allocated[MAX_LIGHTMAPS];
static byte *lightmaps[MAX_LIGHTMAPS], lightmap_alpha[MAX_LIGHTMAPS];
int lightmap_bytes, active_lightmaps;
int lightmap_index[MAX_LIGHTMAPS];
qboolean lightmap_modified[MAX_LIGHTMAPS];
typedef struct { unsigned short l, t, w, h; } glRect_t;
static glRect_t lightmap_rectchange[MAX_LIGHTMAPS];
static qboolean brush_lightmapped = true;
cvar_t r_lightmap_fastalloc = {"r_lightmap_fastalloc", R_DEFAULT_LIGHTMAP_FASTALLOC};

mvertex_t *r_pcurrentvertbase;
model_t *currentmodel;
int nColinElim;

void R_ClearLightmaps(void)
{
    int i;
    if (lightmap_pages)
        Hyena_Finish();
    for (i = 0; i < MAX_LIGHTMAPS; i++) {
        if (lightmaps[i]) {
            Hyena_DestroyTexture(lightmap_index[i]);
            Hyena_FreeTexturePixels(lightmaps[i]);
        }
        free(allocated[i]);
        allocated[i] = NULL;
        lightmaps[i] = NULL;
        lightmap_index[i] = -1;
        lightmap_alpha[i] = 0;
    }
    lightmap_pages = last_lightmap_allocated = 0;
}

static void R_ReserveLightmapPages(int pages)
{
    size_t bytes = (size_t)r_lightmap_size * r_lightmap_size * lightmap_bytes;
    while (lightmap_pages < pages) {
        byte *storage = Hyena_AllocateTexturePixels(bytes);
        if (!storage)
            Sys_Error("R_ReserveLightmapPages: out of memory at page %d (%u KB)\n",
                lightmap_pages + 1, (unsigned)bytes / 1024);
        memset(storage, 0, bytes);
        lightmaps[lightmap_pages++] = storage;
    }
}
void R_BuildLightMap(msurface_t *surface, byte *dest, int stride)
{
    int width = (surface->extents[0] >> 4) + 1, height = (surface->extents[1] >> 4) + 1;
    int x, y, maps, active = 0, value;
    unsigned scale = 0, *values, *pixel;
    const byte *sample = NULL;
    qboolean dynamic = surface->dlightframe == r_framecount;
    qboolean fullbright = r_fullbright.value || !cl.worldmodel->lightdata;
    if (width > 33 || height > 33) Sys_Error("Lightmap surface exceeds 512 units");
    surface->cached_dlight = dynamic;
    if (surface->samples) {
        for (maps = 0; maps < MAXLIGHTMAPS && surface->styles[maps] != 255; maps++) {
            unsigned current = d_lightstylevalue[surface->styles[maps]];
            surface->cached_light[maps] = current;
            if (current) { sample = surface->samples + maps * width * height * 3; scale = current; active++; }
        }
    }
    if (fullbright || (!dynamic && !active)) {
        byte fill = fullbright ? 255 : 0;
        for (y = 0; y < height; y++, dest += stride) {
            if (lightmap_format == HYE_TEXTURE_RGBA8) {
                for (x = 0; x < width; x++) {
                    dest[x * 4] = dest[x * 4 + 1] = dest[x * 4 + 2] = fill;
                    dest[x * 4 + 3] = 255;
                }
            } else {
                memset(dest, fill, width);
                if (lightmap_format == HYE_TEXTURE_PLANAR_RGB8) {
                    memset(dest + r_lightmap_size * r_lightmap_size, fill, width);
                    memset(dest + r_lightmap_size * r_lightmap_size * 2, fill, width);
                }
            }
        }
        return;
    }
    if (!dynamic && active == 1) {
        if (lightmap_scale_value != scale) {
            for (x = 0; x < 256; x++) lightmap_scale[x] = MIN((x * scale) >> 7, 255);
            lightmap_scale_value = scale;
        }
        if (lightmap_format == HYE_TEXTURE_PLANAR_RGB8) {
            for (y = 0; y < height; y++, dest += stride)
                for (x = 0; x < width; x++, sample += 3) {
                    dest[x] = lightmap_scale[sample[0]];
                    dest[x + r_lightmap_size * r_lightmap_size] = lightmap_scale[sample[1]];
                    dest[x + 2 * r_lightmap_size * r_lightmap_size] = lightmap_scale[sample[2]];
                }
        } else if (lightmap_format == HYE_TEXTURE_INDEX8) {
            for (y = 0; y < height; y++, dest += stride)
                for (x = 0; x < width; x++, sample += 3)
                    dest[x] = MIN(((sample[0] + sample[1] + sample[2]) * scale * 85) >> 15, 255);
        } else {
            for (y = 0; y < height; y++, dest += stride)
                for (x = 0; x < width; x++, sample += 3) {
                    dest[x * 4] = lightmap_scale[sample[0]];
                    dest[x * 4 + 1] = lightmap_scale[sample[1]];
                    dest[x * 4 + 2] = lightmap_scale[sample[2]];
                    dest[x * 4 + 3] = 255;
                }
        }
        return;
    }
    values = Hyena_LightmapScratch(width * height * 3);
    if (!values) values = lightmap_values;
    R_BuildLightmapValues(surface, values, R_LIGHTMAP_DARK_LIGHTS, Hyena_AccumulateLightmap);
    pixel = values;
    if (lightmap_format == HYE_TEXTURE_PLANAR_RGB8) {
        for (y = 0; y < height; y++, dest += stride)
            for (x = 0; x < width; x++, pixel += 3) {
                value = (int)pixel[0] >> 7; dest[x] = bound(0, value, 255);
                value = (int)pixel[1] >> 7; dest[x + r_lightmap_size * r_lightmap_size] = bound(0, value, 255);
                value = (int)pixel[2] >> 7; dest[x + 2 * r_lightmap_size * r_lightmap_size] = bound(0, value, 255);
            }
    } else if (lightmap_format == HYE_TEXTURE_INDEX8) {
        for (y = 0; y < height; y++, dest += stride)
            for (x = 0; x < width; x++, pixel += 3)
                dest[x] = MIN(((pixel[0] + pixel[1] + pixel[2]) * 85) >> 15, 255);
    } else {
        for (y = 0; y < height; y++, dest += stride)
            for (x = 0; x < width; x++, pixel += 3) {
                value = (int)pixel[0] >> 7; dest[x * 4] = bound(0, value, 255);
                value = (int)pixel[1] >> 7; dest[x * 4 + 1] = bound(0, value, 255);
                value = (int)pixel[2] >> 7; dest[x * 4 + 2] = bound(0, value, 255);
                dest[x * 4 + 3] = 255;
            }
    }
}

texture_t *R_TextureAnimation (texture_t *base)
{
	int		reletive;
	int		count;

	if (currententity->frame)
	{
		if (base->alternate_anims)
			base = base->alternate_anims;
	}
	
	if (!base->anim_total)
		return base;

	reletive = r_texture_frame % base->anim_total;

	count = 0;	
	while (base->anim_min > reletive || base->anim_max <= reletive)
	{
		base = base->anim_next;
		if (!base)
			Sys_Error ("broken cycle");
		if (++count > 100)
			Sys_Error ("infinite cycle");
	}

	return base;
}

int AllocBlock(int width, int height, int *x, int *y, qboolean alpha)
{
    int page, column, j, pass, best, best2;
    int hint = r_lightmap_fastalloc.value ? last_lightmap_allocated : 0;
    byte group = lightmap_format == HYE_TEXTURE_PLANAR_RGB8 && alpha ? 2 : 1;
    if (width < 1 || height < 1 || width > r_lightmap_size || height > r_lightmap_size)
        Sys_Error("Invalid lightmap block %dx%d", width, height);
    for (pass = 0; pass < 2; pass++) {
        int first = pass ? 0 : hint, end = pass ? hint : MAX_LIGHTMAPS;
        for (page = first; page < end; page++) {
            if (lightmap_alpha[page] && lightmap_alpha[page] != group) continue;
            best = r_lightmap_size;
            for (column = 0; column <= r_lightmap_size - width; column++) {
                best2 = 0;
                for (j = 0; j < width; j++) {
                    int used = allocated[page] ? allocated[page][column + j] : 0;
                    if (used >= best) break;
                    if (used > best2) best2 = used;
                }
                if (j == width) { *x = column; *y = best = best2; }
            }
            if (best + height > r_lightmap_size) continue;
            if (!allocated[page]) {
                allocated[page] = calloc(r_lightmap_size, sizeof(*allocated[page]));
                if (!allocated[page]) Sys_Error("Out of lightmap allocation memory");
            }
            for (j = 0; j < width; j++) allocated[page][*x + j] = best + height;
            lightmap_alpha[page] = group;
            last_lightmap_allocated = page;
            return page;
        }
        if (!hint) break;
    }
    Sys_Error("Lightmap atlas is full");
    return -1;
}

static int R_CreateLightmap(int page, qboolean update)
{
    char name[32];
    hyena_texture_desc_t texture = {0};
    snprintf(name, sizeof(name), "lightmap%d", page);
    texture.identifier = name;
    texture.width = texture.height = r_lightmap_size;
    texture.pixels = lightmaps[page];
    texture.format = lightmap_format;
    texture.filter = HYE_FILTER_LINEAR;
    texture.lightmap = true;
    texture.alpha = lightmap_alpha[page] == 2;
    texture.update = update;
    texture.row_stride = r_lightmap_size * (lightmap_format == HYE_TEXTURE_PLANAR_RGB8 ? 1 : lightmap_bytes);
    texture.update_x = lightmap_rectchange[page].l;
    texture.update_y = lightmap_rectchange[page].t;
    texture.update_width = lightmap_rectchange[page].w;
    texture.update_height = lightmap_rectchange[page].h;
    return Hyena_CreateTexture(&texture);
}

int R_UploadLightmap(int page)
{
    if (lightmap_modified[page]) {
        lightmap_index[page] = R_CreateLightmap(page, true);
        lightmap_modified[page] = false;
        lightmap_rectchange[page] = (glRect_t){r_lightmap_size, r_lightmap_size, 0, 0};
    }
    return lightmap_index[page];
}

void R_UpdateSurfaceLightmap(msurface_t *surface)
{
    unsigned bits = surface->dlightframe == r_framecount ? surface->dlightbits : 0;
    int maps, width, height, page = surface->lightmaptexturenum;
    qboolean reused = surface->lightmap_updateframe == host_framecount + 1;
    glRect_t *rect;
    byte *base;
    surface->lightmap_updateframe = host_framecount + 1;
    if (!brush_lightmapped) return;
    for (maps = 0; maps < MAXLIGHTMAPS && surface->styles[maps] != 255; maps++)
        if (d_lightstylevalue[surface->styles[maps]] != surface->cached_light[maps]) break;
    if (maps == MAXLIGHTMAPS || surface->styles[maps] == 255) {
        if (!bits && !surface->cached_dlight) return;
        if (R_ReuseDynamicLightmap(surface, bits)) return;
    }
    if (!r_dynamic.value) return;
    if (reused) Hyena_PrepareTextureUpdate(lightmap_index[page]);
    lightmap_modified[page] = true;
    rect = &lightmap_rectchange[page];
    if (surface->light_t < rect->t) { if (rect->h) rect->h += rect->t - surface->light_t; rect->t = surface->light_t; }
    if (surface->light_s < rect->l) { if (rect->w) rect->w += rect->l - surface->light_s; rect->l = surface->light_s; }
    width = (surface->extents[0] >> 4) + 1;
    height = (surface->extents[1] >> 4) + 1;
    if (rect->w + rect->l < surface->light_s + width) rect->w = surface->light_s - rect->l + width;
    if (rect->h + rect->t < surface->light_t + height) rect->h = surface->light_t - rect->t + height;
    base = lightmaps[page] + (surface->light_t * r_lightmap_size + surface->light_s) * (lightmap_format == HYE_TEXTURE_PLANAR_RGB8 ? 1 : lightmap_bytes);
    R_BuildLightMap(surface, base, r_lightmap_size * (lightmap_format == HYE_TEXTURE_PLANAR_RGB8 ? 1 : lightmap_bytes));
    surface->cached_dlightbits = bits;
    surface->cached_dlightframe = r_framecount;
}

int R_ClipSurface(msurface_t *surface)
{
    glpoly_t *poly = surface->polys;
    hyena_arrays_t source = {0}, clipped;
    poly->numclippedverts = poly->numverts;
    poly->display_list_verts = poly->verts[0];
    if (!Hyena_HardwareClipping() && (surface->flags & SURF_NEEDSCLIPPING)) {
        source.position.data = R_PolyPosition(poly, 0);
        source.uv.data = R_PolyUV(poly, 0);
        source.position.stride = source.uv.stride = VERTEXSIZE * sizeof(float);
        source.count = poly->numverts;
        Hyena_ClipArrays(&source, &clipped);
        poly->numclippedverts = clipped.count;
        poly->display_list_verts = (float *)clipped.uv.data;
    }
    return poly->numclippedverts;
}

void DrawGLPoly(glpoly_t *poly)
{
    const float *vertices = poly->display_list_verts ? poly->display_list_verts : poly->verts[0];
    int count = poly->display_list_verts ? poly->numclippedverts : poly->numverts;
    R_DrawSurfaceFan(vertices, count, VERTEXSIZE, R_WORLD_POSITION, R_WORLD_UV, false, 0);
}

void DrawGLWaterPoly(glpoly_t *poly)
{
    R_DrawSurfaceFan(poly->verts[0], poly->numverts, VERTEXSIZE, R_WORLD_POSITION, R_WORLD_UV, true, realtime);
}

void R_RenderBrushPoly(msurface_t *surface)
{
    texture_t *texture;
    qboolean alpha;
    if (surface->flags & (SURF_DRAWSKY | TEXFLAG_NODRAW)) return;
    c_brush_polys++;
    texture = R_TextureAnimation(surface->texinfo->texture);
    Hyena_BindTexture(texture->gl_texturenum);
    alpha = texture->name[0] == '{';
    if (alpha) {
        Hyena_EnableCapability(HYE_ALPHA_TEST);
        Hyena_AlphaFunction(HYE_GREATER, 170.0f / 255);
        Hyena_SetTextureMode(HYE_MODULATE);
        Hyena_SetColor(1, 1, 1, 1);
    }
    if (surface->flags & SURF_DRAWTURB) EmitWaterPolys(surface);
    else if (R_ClipSurface(surface)) {
        if (surface->flags & TEXFLAG_REFLECT) EmitReflectivePolys(surface);
        else if (surface->flags & SURF_UNDERWATER) DrawGLWaterPoly(surface->polys);
        else DrawGLPoly(surface->polys);
        if (brush_lightmapped && !(surface->flags & TEXFLAG_LIGHT)) {
            R_ChainLightmap(surface);
            R_UpdateSurfaceLightmap(surface);
        }
    }
    if (alpha) {
        Hyena_DisableCapability(HYE_ALPHA_TEST);
        Hyena_SetTextureMode(HYE_REPLACE);
    }
}

void R_DrawBrushModel(entity_t *entity)
{
    model_t *model = entity->model;
    vec3_t mins, maxs;
    qboolean rotated = entity->angles[0] || entity->angles[1] || entity->angles[2];
    int i, classification;
    currententity = entity;
    for (i = 0; i < 3; i++) {
        mins[i] = entity->origin[i] + (rotated ? -model->radius : model->mins[i]);
        maxs[i] = entity->origin[i] + (rotated ? model->radius : model->maxs[i]);
    }
    classification = R_FrustumCheckBox(mins, maxs);
    if (classification < 0) return;
    R_ClearLightmapChains();
    VectorSubtract(r_refdef.vieworg, entity->origin, modelorg);
    if (rotated) {
        vec3_t temp, forward, right, up;
        VectorCopy(modelorg, temp);
        AngleVectors(entity->angles, forward, right, up);
        modelorg[0] = DotProduct(temp, forward);
        modelorg[1] = -DotProduct(temp, right);
        modelorg[2] = DotProduct(temp, up);
    }
    if (model->firstmodelsurface && !gl_flashblend.value) {
        for (i = 0; i < MAX_DLIGHTS; i++)
            if (cl_dlights[i].die >= (float)cl.time && cl_dlights[i].radius)
                R_MarkLights(&cl_dlights[i], 1u << i, model->nodes + model->hulls[0].firstclipnode);
    }
    Hyena_PushMatrix();
    Hyena_SetColor(1, 1, 1, 1);
    Hyena_SetTextureMode(HYE_MODULATE);
    brush_lightmapped = !ISADDITIVE(entity);
    if (ISADDITIVE(entity)) {
        Hyena_EnableCapability(HYE_BLEND);
        Hyena_BlendConstant(entity->renderamt);
        if (entity->renderamt <= 0.7f) Hyena_DepthMask(false);
    } else if (ISGLOW(entity)) {
        Hyena_EnableCapability(HYE_BLEND);
        Hyena_SetBlendFunction(HYE_SRC_ALPHA, HYE_ONE);
        Hyena_DepthMask(false);
    } else if (ISCOLOR(entity))
        Hyena_SetColor(entity->rendercolor[0], entity->rendercolor[1], entity->rendercolor[2], 1);
    entity->angles[PITCH] = -entity->angles[PITCH];
    R_BlendedRotateForEntity(entity, 0, entity->scale);
    Hyena_BeginModelClipping();
    entity->angles[PITCH] = -entity->angles[PITCH];
    Fog_SetupFrame(true);
    for (i = 0; i < model->nummodelsurfaces; i++) {
        msurface_t *surface = model->surfaces + model->firstmodelsurface + i;
        float distance = DotProduct(modelorg, surface->plane->normal) - surface->plane->dist;
        if ((surface->flags & SURF_PLANEBACK) ? distance < -BACKFACE_EPSILON : distance > BACKFACE_EPSILON) {
            surface->flags &= ~SURF_NEEDSCLIPPING;
            if (classification > 1) surface->flags |= SURF_NEEDSCLIPPING;
            R_RenderBrushPoly(surface);
        }
    }
    Fog_SetupFrame(false);
    if (brush_lightmapped) R_BlendLightmaps();
    brush_lightmapped = true;
    Hyena_EndModelClipping();
    Hyena_PopMatrix();
    Hyena_DepthMask(true);
    Hyena_DisableCapability(HYE_BLEND);
    Hyena_DisableCapability(HYE_ALPHA_TEST);
    Hyena_SetBlendFunction(HYE_SRC_ALPHA, HYE_ONE_MINUS_SRC_ALPHA);
    Hyena_SetTextureMode(HYE_REPLACE);
    Hyena_SetColor(1, 1, 1, 1);
}

void BuildSurfaceDisplayList (msurface_t *fa)
{
	int			i, lindex, lnumverts;
	medge_t		*pedges, *r_pedge;
	float		*vec;
	float		s, t;
	glpoly_t	*poly;

// reconstruct the polygon
	pedges = currentmodel->edges;
	lnumverts = fa->numedges;

	//
	// draw texture
	//
	poly = Hunk_Alloc (sizeof(glpoly_t) + (lnumverts-4) * VERTEXSIZE*sizeof(float));
	poly->next = fa->polys;
	poly->flags = fa->flags;
	fa->polys = poly;
	poly->numverts = lnumverts;

	for (i=0 ; i<lnumverts ; i++)
	{
		lindex = currentmodel->surfedges[fa->firstedge + i];

		if (lindex > 0)
		{
			r_pedge = &pedges[lindex];
			vec = r_pcurrentvertbase[r_pedge->v[0]].position;
		}
		else
		{
			r_pedge = &pedges[-lindex];
			vec = r_pcurrentvertbase[r_pedge->v[1]].position;
		}
		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s /= fa->texinfo->texture->width;

		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t /= fa->texinfo->texture->height;

		VectorCopy (vec, R_PolyPosition(poly, i));
		R_PolyUV(poly, i)[0] = s;
		R_PolyUV(poly, i)[1] = t;

		//
		// lightmap texture coordinates
		//
		s = DotProduct (vec, fa->texinfo->vecs[0]) + fa->texinfo->vecs[0][3];
		s -= fa->texturemins[0];
		s += fa->light_s*16;
		s += 8;
		s /= r_lightmap_size*16; //fa->texinfo->texture->width;

		t = DotProduct (vec, fa->texinfo->vecs[1]) + fa->texinfo->vecs[1][3];
		t -= fa->texturemins[1];
		t += fa->light_t*16;
		t += 8;
		t /= r_lightmap_size*16; //fa->texinfo->texture->height;

		if (R_WORLD_LIGHTMAP_UV >= 0) poly->verts[i][R_WORLD_LIGHTMAP_UV] = s;
		if (R_WORLD_LIGHTMAP_UV >= 0) poly->verts[i][R_WORLD_LIGHTMAP_UV + 1] = t;
	}

	//
	// remove co-linear points - Ed
	//
	if (!gl_keeptjunctions.value && !(fa->flags & SURF_UNDERWATER) )
	{
		for (i = 0 ; i < lnumverts ; ++i)
		{
			vec3_t v1, v2;
			float *prev, *this, *next;

			prev = R_PolyPosition(poly, (i + lnumverts - 1) % lnumverts);
			this = R_PolyPosition(poly, i);
			next = R_PolyPosition(poly, (i + 1) % lnumverts);

			VectorSubtract( this, prev, v1 );
			VectorNormalize( v1 );
			VectorSubtract( next, prev, v2 );
			VectorNormalize( v2 );

			// skip co-linear points
			#define COLINEAR_EPSILON 0.001f
			if ((fabsf( v1[0] - v2[0] ) <= COLINEAR_EPSILON) &&
				(fabsf( v1[1] - v2[1] ) <= COLINEAR_EPSILON) && 
				(fabsf( v1[2] - v2[2] ) <= COLINEAR_EPSILON))
			{
				int j;
				for (j = i + 1; j < lnumverts; ++j)
				{
					int k;
					for (k = 0; k < VERTEXSIZE; ++k)
						poly->verts[j - 1][k] = poly->verts[j][k];
				}
				--lnumverts;
				++nColinElim;
				// retry next vertex next time, which is now current vertex
				--i;
			}
		}
	}
    poly->numverts = lnumverts;
    if (R_WORLD_LIGHTMAP_UV >= 0 && !(fa->flags & (SURF_DRAWSKY | SURF_DRAWTURB))) {
        float u = floorf(R_PolyUV(poly, 0)[0]), v = floorf(R_PolyUV(poly, 0)[1]);
        for (i = 0; i < lnumverts; i++) {
            R_PolyUV(poly, i)[0] -= u;
            R_PolyUV(poly, i)[1] -= v;
        }
    }
}

static void R_CreateSurfaceLightmap(msurface_t *surface)
{
    int width, height;
    if (surface->flags & (SURF_DRAWSKY | SURF_DRAWTURB)) return;
    width = (surface->extents[0] >> 4) + 1;
    height = (surface->extents[1] >> 4) + 1;
    surface->lightmaptexturenum = AllocBlock(width, height, &surface->light_s, &surface->light_t,
        surface->texinfo->texture->name[0] == '{');
}

void R_BuildLightmaps(void)
{
    int i, j, pages;
    model_t *model;
    R_ClearLightmaps();
    r_lightmap_size = Hyena_PreferredLightmapSize();
    lightmap_format = COM_CheckParm("-lm_1") ? HYE_TEXTURE_INDEX8 : Hyena_PreferredLightmapFormat();
    if (COM_CheckParm("-lm_2")) Cvar_SetValue("hyena_lightmap_16bit", 1);
    lightmap_bytes = lightmap_format == HYE_TEXTURE_PLANAR_RGB8 ? 3 : lightmap_format == HYE_TEXTURE_INDEX8 ? 1 : 4;
    if (r_lightmap_size < 33 || r_lightmap_size > LIGHTMAP_MAX_SIZE)
        Sys_Error("Invalid lightmap texture size %d", r_lightmap_size);
    r_framecount = 1;
    for (j = 1; j < MAX_MODELS && (model = cl.model_precache[j]) != NULL; j++) {
        if (model->name[0] == '*') continue;
        for (i = 0; i < model->numsurfaces; i++) R_CreateSurfaceLightmap(model->surfaces + i);
    }
    for (pages = 0; pages < MAX_LIGHTMAPS && lightmap_alpha[pages]; pages++);
    R_ReserveLightmapPages(pages);
    for (j = 1; j < MAX_MODELS && (model = cl.model_precache[j]) != NULL; j++) {
        if (model->name[0] == '*') continue;
        r_pcurrentvertbase = model->vertexes;
        currentmodel = model;
        for (i = 0; i < model->numsurfaces; i++) {
            msurface_t *surface = model->surfaces + i;
            byte *base;
            int stride = r_lightmap_size * (lightmap_format == HYE_TEXTURE_PLANAR_RGB8 ? 1 : lightmap_bytes);
            if (surface->flags & (SURF_DRAWSKY | SURF_DRAWTURB)) continue;
            base = lightmaps[surface->lightmaptexturenum] + surface->light_t * stride + surface->light_s * (lightmap_format == HYE_TEXTURE_PLANAR_RGB8 ? 1 : lightmap_bytes);
            R_BuildLightMap(surface, base, stride);
            BuildSurfaceDisplayList(surface);
        }
    }
    for (i = 0; i < pages; i++) {
        lightmap_modified[i] = false;
        lightmap_rectchange[i] = (glRect_t){r_lightmap_size, r_lightmap_size, 0, 0};
        lightmap_index[i] = R_CreateLightmap(i, false);
    }
    {
        const r_world_layout_t layout = { offsetof(glpoly_t, verts), VERTEXSIZE, R_WORLD_POSITION, R_WORLD_UV, R_WORLD_LIGHTMAP_UV,
            Hyena_HardwareClipping() ? 0 : offsetof(glpoly_t, display_list_verts), offsetof(glpoly_t, numclippedverts),
            SURF_DRAWSKY | SURF_DRAWTURB | SURF_UNDERWATER | TEXFLAG_NODRAW | TEXFLAG_REFLECT,
            TEXFLAG_LIGHT, Hyena_HardwareClipping() ? 4 : 5, !Hyena_HardwareClipping(), &r_lightmap, &r_showtris };
        R_BuildWorldBatch(&layout);
    }
}
