// Copyright (C) 1996-1997 Id Software, Inc.
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
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// r_world.c -- Texture-sorted BSP traversal and lightmap passes

#include "../nzportable_def.h"

static r_world_layout_t world_layout;
static qboolean world_separate_arrays, world_hardware_clipping;

typedef struct {
    hyena_buffer_t * vertices, * lightmap, * colors;
    int              count;
} world_page_t;
static world_page_t * world_pages;
static int world_page_count;
static hyena_buffer_t * world_indices;

static void
R_DestroyWorldCache(void)
{
    int i;

    for (i = 0; i < world_page_count; ++i) {
        Hyena_DestroyBuffer(world_pages[i].vertices);
        Hyena_DestroyBuffer(world_pages[i].lightmap);
        Hyena_DestroyBuffer(world_pages[i].colors);
    }
    Hyena_DestroyBuffer(world_indices);
    free(world_pages);
    world_pages      = NULL;
    world_indices    = NULL;
    world_page_count = 0;
}

static void
R_CreateWorldCache(int pages)
{
    R_DestroyWorldCache();
    world_pages = calloc(pages, sizeof(*world_pages));
    if (!world_pages)
        Sys_Error("R_CreateWorldCache: out of memory");
    world_page_count = pages;
    world_indices    = Hyena_CreateBuffer(HYE_INDEX_BUFFER, NULL, 0);
}

static void
R_UploadWorldCachePage(int page, const world_vertex_t * vertices, int count)
{
    world_page_t * cached = world_pages + page;

    cached->count = count;
    if (!world_separate_arrays) {
        vertex_t * base, * lightmap;
        int i;
        // GU cannot select a second UV stream. Keep both packed layouts, without a staging copy.
        cached->vertices = Hyena_CreateBuffer(HYE_ARRAY_BUFFER, NULL, count * sizeof(vertex_t));
        cached->lightmap = Hyena_CreateBuffer(HYE_ARRAY_BUFFER, NULL, count * sizeof(vertex_t));
        base     = Hyena_MapBuffer(cached->vertices);
        lightmap = Hyena_MapBuffer(cached->lightmap);
        for (i = 0; i < count; ++i) {
            memcpy(&base[i].uv, vertices[i].uv, sizeof(base[i].uv));
            memcpy(&base[i].xyz, vertices[i].xyz, sizeof(base[i].xyz));
            memcpy(&lightmap[i].uv, vertices[i].lightmap_uv, sizeof(lightmap[i].uv));
            memcpy(&lightmap[i].xyz, vertices[i].xyz, sizeof(lightmap[i].xyz));
        }
        Hyena_UnmapBuffer(cached->vertices);
        Hyena_UnmapBuffer(cached->lightmap);
    } else {
        cached->vertices = Hyena_CreateBuffer(HYE_ARRAY_BUFFER, vertices, count * sizeof(*vertices));
        if (Hyena_NeedsColorArray()) {
            // Immutable white colors avoid rebuilding a picaGL color stream for each group.
            byte * colors = malloc(count * 4);
            if (!colors)
                Sys_Error("R_UploadWorldCachePage: out of color memory");
            memset(colors, 255, count * 4);
            cached->colors = Hyena_CreateBuffer(HYE_ARRAY_BUFFER, colors, count * 4);
            free(colors);
        }
    }
} /* R_UploadWorldCachePage */

static void
R_BeginWorldIndices(const unsigned short * indices, int count)
{
    // One streamed index range serves every texture group in this pass.
    Hyena_UpdateBuffer(world_indices, indices, count * sizeof(*indices));
}

static void
R_DrawWorldCachePage(int page, int first_index, int count, qboolean lightmap, qboolean alpha_test)
{
    world_page_t * cached = world_pages + page;
    hyena_arrays_t arrays = { 0 };

    arrays.count = cached->count;
    if (!world_separate_arrays) {
        arrays.position.buffer = arrays.uv.buffer = lightmap ? cached->lightmap : cached->vertices;
        arrays.position.data   = (const void *) sizeof(vertex_uv_t);
        arrays.position.stride = arrays.uv.stride = sizeof(vertex_t);
        if (lightmap)
            Hyena_TextureTransform(1, 1, 0, 0);
    } else {
        arrays.position.buffer = arrays.uv.buffer = cached->vertices;
        arrays.position.data   = (const void *) (4 * sizeof(float));
        arrays.uv.data         = (const void *) ((lightmap ? 2 : 0) * sizeof(float));
        arrays.position.stride = arrays.uv.stride = sizeof(world_vertex_t);
        arrays.color.buffer    = cached->colors;
        arrays.color.type      = HYE_UBYTE;
    }
    if (alpha_test && !world_layout.alpha_test_all) {
        Hyena_EnableCapability(HYE_ALPHA_TEST);
        Hyena_AlphaFunction(HYE_GREATER, 0xaa / 255.0f);
    }
    Hyena_DrawElements(HYE_TRIANGLES, &arrays, count, world_indices, first_index * sizeof(unsigned short));
    if (alpha_test && !world_layout.alpha_test_all)
        Hyena_DisableCapability(HYE_ALPHA_TEST);
}

static qboolean
R_WorldSurfaceBatchable(const msurface_t * surface)
{
    return surface->polys && !(surface->flags & world_layout.unbatched_flags);
}

static qboolean
R_WorldSurfaceLightmapped(const msurface_t * surface)
{
    return !(surface->flags & world_layout.unlit_flags);
}

static int
R_CopyWorldSurfaceVertices(const msurface_t * surface, world_vertex_t * vertices, int max_vertices)
{
    const glpoly_t * poly = surface->polys;
    const float * source;
    int i;

    if (!poly)
        return 0;

    if (!vertices)
        return poly->numverts;

    if (max_vertices < poly->numverts)
        return 0;

    source = (const float *) ((const byte *) poly + world_layout.vertices_offset);
    for (i = 0; i < poly->numverts; ++i, source += world_layout.stride) {
        memcpy(vertices[i].uv, source + world_layout.uv_offset, sizeof(vertices[i].uv));
        memcpy(vertices[i].xyz, source + world_layout.position_offset, sizeof(vertices[i].xyz));
        if (world_layout.lightmap_offset >= 0) {
            memcpy(vertices[i].lightmap_uv, source + world_layout.lightmap_offset, sizeof(vertices[i].lightmap_uv));
        } else {
            // Some loaders retain only diffuse UVs. Derive atlas UVs once, when building the cache.
            float s = DotProduct(vertices[i].xyz, surface->texinfo->vecs[0]) + surface->texinfo->vecs[0][3];
            float t = DotProduct(vertices[i].xyz, surface->texinfo->vecs[1]) + surface->texinfo->vecs[1][3];
            vertices[i].lightmap_uv[0] = (s - surface->texturemins[0] + surface->light_s * 16 + 8) / (128 * 16.0f);
            vertices[i].lightmap_uv[1] = (t - surface->texturemins[1] + surface->light_t * 16 + 8) / (128 * 16.0f);
        }
    }
    return poly->numverts;
}

static void
R_BeginWorldSurfaces(void)
{
    if (world_layout.alpha_test_all) {
        Hyena_InvalidateTextureBinding();
        Hyena_EnableCapability(HYE_ALPHA_TEST);
        Hyena_AlphaFunction(HYE_GREATER, 0xaa / 255.0f);
        Hyena_SetTextureMode(HYE_MODULATE);
        Hyena_SetColor(1, 1, 1, 1);
    }
    Hyena_BeginArrays(HYE_ARRAY_POSITION | HYE_ARRAY_UV);
}

static void
R_EndWorldSurfaces(void)
{
    if (world_layout.alpha_test_all) {
        Hyena_AlphaFunction(HYE_GREATER, 0);
        Hyena_DisableCapability(HYE_ALPHA_TEST);
    }
    Hyena_EndArrays();
}

static void
R_BeginLightmaps(void)
{
    Hyena_BeginArrays(HYE_ARRAY_POSITION | HYE_ARRAY_UV);
    // Multiply the diffuse pass at equal depth, without writing depth a second time.
    Hyena_DepthMask(false);
    Hyena_EnableCapability(HYE_BLEND);
    Hyena_SetBlendFunction(HYE_DST_COLOR, HYE_SRC_COLOR);
    Hyena_DepthFunction(HYE_EQUAL);
    if (world_layout.lightmap_debug && world_layout.lightmap_debug->value)
        Hyena_DisableCapability(HYE_BLEND);
}

static void
R_EndLightmaps(void)
{
    if (world_layout.lightmap_offset < 0) {
        Hyena_SetPalette(NULL, 0);
        Hyena_TextureTransform(1, 1, 0, 0);
    }
    Hyena_DisableCapability(HYE_BLEND);
    Hyena_SetBlendFunction(HYE_SRC_ALPHA, HYE_ONE_MINUS_SRC_ALPHA);
    Hyena_DepthMask(true);
    Hyena_EnableCapability(HYE_DEPTH_TEST);
    Hyena_DepthFunction(HYE_LEQUAL);
    Hyena_EndArrays();
}

static void
R_BindWorldLightmap(int texture)
{
    Hyena_BindTexture(texture);
    Hyena_TextureFilter(HYE_FILTER_LINEAR);
}

static void
R_DrawLightmapSurface(msurface_t * surface)
{
    glpoly_t * poly        = surface->polys;
    const float * vertices = (const float *) ((const byte *) poly + world_layout.vertices_offset);
    int count     = poly->numverts;
    int uv_offset = world_layout.lightmap_offset;

    if (world_layout.clipped_vertices_offset) {
        memcpy(&vertices, (const byte *) poly + world_layout.clipped_vertices_offset, sizeof(vertices));
        memcpy(&count, (const byte *) poly + world_layout.clipped_count_offset, sizeof(count));
    }
    if (uv_offset < 0) {
        float sscale = surface->texinfo->texture->width / (128 * 16.0f);
        float tscale = surface->texinfo->texture->height / (128 * 16.0f);
        uv_offset = world_layout.uv_offset;
        Hyena_TextureTransform(sscale, tscale,
          sscale * (-surface->texturemins[0] + surface->light_s * 16 + 8) / surface->texinfo->texture->width,
          tscale * (-surface->texturemins[1] + surface->light_t * 16 + 8) / surface->texinfo->texture->height);
    }
    if (world_layout.wireframe && world_layout.wireframe->value) {
        hyena_arrays_t arrays = { 0 };
        arrays.position.data   = vertices + world_layout.position_offset;
        arrays.uv.data         = vertices + uv_offset;
        arrays.position.stride = arrays.uv.stride = world_layout.stride * sizeof(float);
        arrays.count = count;
        Hyena_DisableCapability(HYE_TEXTURE_2D);
        Hyena_DisableCapability(HYE_BLEND);
        Hyena_FlushMatrices();
        Hyena_DrawArrays(HYE_LINE_STRIP, &arrays, count);
        Hyena_EnableCapability(HYE_TEXTURE_2D);
        Hyena_EnableCapability(HYE_BLEND);
    } else {
        R_DrawSurfaceFan(vertices, count, world_layout.stride, world_layout.position_offset, uv_offset,
          !world_layout.clipped_vertices_offset && (poly->flags & SURF_UNDERWATER), realtime);
    }
} /* R_DrawLightmapSurface */

static void
R_BeginWorldWater(float alpha)
{
    Hyena_BeginArrays(HYE_ARRAY_POSITION | HYE_ARRAY_UV);
    Hyena_LoadViewMatrix((const float *) &r_world_matrix);
    Hyena_EnableCapability(HYE_BLEND);
    Hyena_TextureAlpha(alpha);
}

static void
R_EndWorldWater(void)
{
    Hyena_TextureAlpha(1);
    Hyena_SetTextureMode(HYE_REPLACE);
    Hyena_SetCurrentColor(1, 1, 1, 1);
    Hyena_DisableCapability(HYE_BLEND);
    Hyena_EndArrays();
}

#define WORLD_CACHE_PAGE_VERTICES 65535

typedef struct {
    int page;
    int first_vertex;
    int num_vertices;
    int first_index;
    int base_group;
    int lightmap_group;
} world_surface_cache_t;

typedef struct {
    int          offset;
    int          capacity;
    int          count;
    int          draw_offset;
    msurface_t * surface;
} world_batch_group_t;

static msurface_t * lightmap_chains[MAX_LIGHTMAPS];
static world_surface_cache_t * world_surfaces;
static world_batch_group_t * world_base_groups;
static world_batch_group_t * world_lightmap_groups;
static unsigned short * world_base_indices;
static unsigned short * world_lightmap_indices;
static unsigned short * world_surface_indices;
static int world_cache_pages;
static int world_cache_surfaces;
static int world_cache_textures;
static int world_base_draws, world_lightmap_draws;
static int world_base_draw_indices, world_lightmap_draw_indices;
extern mplane_t frustum[];

static qboolean
R_QueueWorldSurface(msurface_t * surface);

static int
R_WorldTextureIndex(const texture_t * texture)
{
    int i;

    for (i = 0; i < cl.worldmodel->numtextures; ++i)
        if (cl.worldmodel->textures[i] == texture)
            return i;

    return -1;
}

static void
R_DestroyWorldBatch(void)
{
    R_DestroyWorldCache();
    free(world_surfaces);
    free(world_base_groups);
    free(world_lightmap_groups);
    free(world_base_indices);
    free(world_lightmap_indices);
    free(world_surface_indices);
    world_surfaces         = NULL;
    world_base_groups      = NULL;
    world_lightmap_groups  = NULL;
    world_base_indices     = NULL;
    world_lightmap_indices = NULL;
    world_surface_indices  = NULL;
    world_cache_pages      = 0;
    world_cache_surfaces   = 0;
    world_cache_textures   = 0;
}

void
R_BuildWorldBatch(const r_world_layout_t * layout)
{
    world_vertex_t * vertices;
    world_surface_cache_t * cached;
    world_batch_group_t * group;
    msurface_t * surface;
    int * page_vertices;
    int base_indices     = 0;
    int lightmap_indices = 0;
    int surface_indices  = 0;
    int page        = 0;
    int page_vertex = 0;
    int i, count, indices, texture;

    world_layout = *layout;
    world_separate_arrays   = Hyena_SeparateArrays();
    world_hardware_clipping = Hyena_HardwareClipping();
    R_DestroyWorldBatch();
    if (!cl.worldmodel || !cl.worldmodel->numsurfaces || !cl.worldmodel->numtextures)
        return;

    world_cache_surfaces = cl.worldmodel->numsurfaces;
    world_cache_textures = cl.worldmodel->numtextures;
    world_surfaces       = calloc(world_cache_surfaces, sizeof(*world_surfaces));
    if (!world_surfaces)
        Sys_Error("R_BuildWorldBatch: out of surface memory\n");
    for (i = 0; i < world_cache_surfaces; ++i)
        world_surfaces[i].page = -1;

    for (i = 0; i < world_cache_surfaces; ++i) {
        surface = cl.worldmodel->surfaces + i;
        if (!R_WorldSurfaceBatchable(surface))
            continue;
        count   = R_CopyWorldSurfaceVertices(surface, NULL, 0);
        texture = R_WorldTextureIndex(surface->texinfo->texture);
        if (count < 3 || count > WORLD_CACHE_PAGE_VERTICES || texture < 0)
            continue;
        if (page_vertex + count > WORLD_CACHE_PAGE_VERTICES) {
            ++page;
            page_vertex = 0;
        }
        cached                 = world_surfaces + i;
        cached->page           = page;
        cached->first_vertex   = page_vertex;
        cached->num_vertices   = count;
        cached->base_group     = page * world_cache_textures + texture;
        cached->lightmap_group = R_WorldSurfaceLightmapped(surface) ?
          page * MAX_LIGHTMAPS + surface->lightmaptexturenum :
          -1;
        page_vertex += count;
    }
    world_cache_pages = page + (page_vertex != 0);
    if (!world_cache_pages) {
        R_DestroyWorldBatch();
        return;
    }

    page_vertices         = calloc(world_cache_pages, sizeof(*page_vertices));
    world_base_groups     = calloc(world_cache_pages * world_cache_textures, sizeof(*world_base_groups));
    world_lightmap_groups = calloc(world_cache_pages * MAX_LIGHTMAPS, sizeof(*world_lightmap_groups));
    if (!page_vertices || !world_base_groups || !world_lightmap_groups)
        Sys_Error("R_BuildWorldBatch: out of group memory\n");
    for (i = 0; i < world_cache_surfaces; ++i) {
        cached = world_surfaces + i;
        if (cached->page < 0)
            continue;
        indices = (cached->num_vertices - 2) * 3;
        cached->first_index          = surface_indices;
        surface_indices             += indices;
        page_vertices[cached->page] += cached->num_vertices;
        group = world_base_groups + cached->base_group;
        group->capacity += indices;
        group->surface   = cl.worldmodel->surfaces + i;
        if (cached->lightmap_group >= 0) {
            group = world_lightmap_groups + cached->lightmap_group;
            group->capacity += indices;
            group->surface   = cl.worldmodel->surfaces + i;
        }
    }
    for (i = 0; i < world_cache_pages * world_cache_textures; ++i) {
        world_base_groups[i].offset = base_indices;
        base_indices += world_base_groups[i].capacity;
    }
    for (i = 0; i < world_cache_pages * MAX_LIGHTMAPS; ++i) {
        world_lightmap_groups[i].offset = lightmap_indices;
        lightmap_indices += world_lightmap_groups[i].capacity;
    }
    world_base_indices     = malloc(base_indices * sizeof(*world_base_indices));
    world_lightmap_indices = malloc(lightmap_indices * sizeof(*world_lightmap_indices));
    world_surface_indices  = malloc(surface_indices * sizeof(*world_surface_indices));
    if ((base_indices && !world_base_indices) || (lightmap_indices && !world_lightmap_indices) ||
      (surface_indices && !world_surface_indices))
        Sys_Error("R_BuildWorldBatch: out of index memory\n");
    for (i = 0; i < world_cache_surfaces; ++i) {
        unsigned short * out;

        cached = world_surfaces + i;
        if (cached->page < 0)
            continue;
        out = world_surface_indices + cached->first_index;
        for (count = 1; count < cached->num_vertices - 1; ++count) {
            *out++ = cached->first_vertex;
            *out++ = cached->first_vertex + count;
            *out++ = cached->first_vertex + count + 1;
        }
    }

    R_CreateWorldCache(world_cache_pages);
    for (page = 0; page < world_cache_pages; ++page) {
        vertices = malloc(page_vertices[page] * sizeof(*vertices));
        if (!vertices)
            Sys_Error("R_BuildWorldBatch: out of vertex memory\n");
        for (i = 0; i < world_cache_surfaces; ++i) {
            cached = world_surfaces + i;
            if (cached->page != page)
                continue;
            count = R_CopyWorldSurfaceVertices(cl.worldmodel->surfaces + i,
                vertices + cached->first_vertex, cached->num_vertices);
            if (count != cached->num_vertices)
                Sys_Error("R_BuildWorldBatch: surface vertex count changed\n");
        }
        R_UploadWorldCachePage(page, vertices, page_vertices[page]);
        free(vertices);
    }
    free(page_vertices);
} /* R_BuildWorldBatch */

static void
R_ClearWorldBatches(void)
{
    int i;

    world_base_draws        = world_lightmap_draws = 0;
    world_base_draw_indices = world_lightmap_draw_indices = 0;
    for (i = 0; i < world_cache_pages * world_cache_textures; ++i)
        world_base_groups[i].count = 0;
    for (i = 0; i < world_cache_pages * MAX_LIGHTMAPS; ++i)
        world_lightmap_groups[i].count = 0;
}

static void
R_AppendWorldSurface(world_batch_group_t * group, unsigned short * indices,
  const world_surface_cache_t * cached)
{
    int count = (cached->num_vertices - 2) * 3;

    indices += group->offset + group->count;
    memcpy(indices, world_surface_indices + cached->first_index, count * sizeof(*indices));
    group->count += count;
}

static qboolean
R_QueueWorldSurface(msurface_t * surface)
{
    world_surface_cache_t * cached;
    size_t offset;
    size_t first   = (size_t) cl.worldmodel->surfaces;
    size_t current = (size_t) surface;
    int index;

    if (current < first || current >= first + world_cache_surfaces * sizeof(*surface) ||
      (surface->flags & SURF_NEEDSCLIPPING && !world_hardware_clipping))
        return false;

    offset = current - first;
    if (offset % sizeof(*surface))
        return false;

    index  = offset / sizeof(*surface);
    cached = world_surfaces + index;
    if (cached->page < 0)
        return false;

    R_AppendWorldSurface(world_base_groups + cached->base_group, world_base_indices, cached);
    if (cached->lightmap_group >= 0 && !r_fullbright.value) {
        R_AppendWorldSurface(
            world_lightmap_groups + cached->lightmap_group, world_lightmap_indices, cached);
        R_UpdateSurfaceLightmap(surface);
    }
    ++c_brush_polys;
    return true;
}

static void
R_DrawWorldBatches(void)
{
    world_batch_group_t * group;
    texture_t * texture;
    int page, i, count = 0;

    for (i = 0; i < world_cache_pages * world_cache_textures; ++i) {
        group = world_base_groups + i;
        if (!group->count)
            continue;
        memmove(world_base_indices + count, world_base_indices + group->offset,
          group->count * sizeof(*world_base_indices));
        group->draw_offset = count;
        count += group->count;
    }
    if (!count)
        return;

    R_BeginWorldIndices(world_base_indices, count);

    for (page = 0; page < world_cache_pages; ++page) {
        for (i = 0; i < world_cache_textures; ++i) {
            group = world_base_groups + page * world_cache_textures + i;
            if (!group->count)
                continue;
            texture = R_TextureAnimation(group->surface->texinfo->texture);
            Hyena_BindTexture(texture->gl_texturenum);
            R_DrawWorldCachePage(page, group->draw_offset,
              group->count, false, texture->name[0] == '{');
            ++world_base_draws;
            world_base_draw_indices += group->count;
            group->count = 0;
        }
    }
} /* R_DrawWorldBatches */

static void
R_DrawWorldLightmapBatches(void)
{
    world_batch_group_t * group;
    int page, i, count = 0;

    for (i = 0; i < world_cache_pages * MAX_LIGHTMAPS; ++i) {
        group = world_lightmap_groups + i;
        if (!group->count)
            continue;
        memmove(world_lightmap_indices + count, world_lightmap_indices + group->offset,
          group->count * sizeof(*world_lightmap_indices));
        group->draw_offset = count;
        count += group->count;
    }
    if (!count)
        return;

    R_BeginWorldIndices(world_lightmap_indices, count);

    for (page = 0; page < world_cache_pages; ++page) {
        for (i = 0; i < MAX_LIGHTMAPS; ++i) {
            group = world_lightmap_groups + page * MAX_LIGHTMAPS + i;
            if (!group->count)
                continue;
            R_BindWorldLightmap(R_UploadLightmap(i));
            R_DrawWorldCachePage(page, group->draw_offset,
              group->count, true, false);
            ++world_lightmap_draws;
            world_lightmap_draw_indices += group->count;
            group->count = 0;
        }
    }
}

void
R_WorldBatchStats(int * base_batches, int * lightmap_batches,
  int * base_indices, int * lightmap_indices)
{
    *base_batches     = world_base_draws;
    *lightmap_batches = world_lightmap_draws;
    *base_indices     = world_base_draw_indices;
    *lightmap_indices = world_lightmap_draw_indices;
}

void
R_ClearLightmapChains(void)
{
    memset(lightmap_chains, 0, sizeof(lightmap_chains));
}

void
R_ChainLightmap(msurface_t * surface)
{
    surface->texturechain = lightmap_chains[surface->lightmaptexturenum];
    lightmap_chains[surface->lightmaptexturenum] = surface;
}

void
R_BlendLightmaps(void)
{
    int i;
    msurface_t * surface;

    if (r_fullbright.value)
        return;

    for (i = 0; i < MAX_LIGHTMAPS; ++i)
        if (lightmap_chains[i])
            break;
    if (i == MAX_LIGHTMAPS) {
        for (i = 0; i < world_cache_pages * MAX_LIGHTMAPS; ++i)
            if (world_lightmap_groups[i].count)
                break;
        if (i == world_cache_pages * MAX_LIGHTMAPS)
            return;
    }
    R_BeginLightmaps();
    R_DrawWorldLightmapBatches();
    for (i = 0; i < MAX_LIGHTMAPS; ++i) {
        if (!lightmap_chains[i])
            continue;
        R_BindWorldLightmap(R_UploadLightmap(i));
        for (surface = lightmap_chains[i]; surface; surface = surface->texturechain)
            R_DrawLightmapSurface(surface);
    }
    R_EndLightmaps();
}

static int
R_WorldFrustumCheck(mnode_t * node, int planes)
{
    int i, side, intersections = 0;

    for (i = 0; i < planes; ++i) {
        side = BoxOnPlaneSide(node->minmaxs, node->minmaxs + 3, &frustum[i]);
        if (side == 2)
            return -1;

        intersections += side == 3;
    }
    return intersections;
}

static void
R_ChainWorldSurface(msurface_t * surface, int intersections)
{
    surface->flags &= ~SURF_NEEDSCLIPPING;
    if (intersections)
        surface->flags |= SURF_NEEDSCLIPPING;
    if (!(mirrortexturenum >= 0 && r_mirroralpha.value != 1.0f &&
      surface->texinfo->texture == cl.worldmodel->textures[mirrortexturenum]) &&
      R_QueueWorldSurface(surface))
        return;

    surface->texturechain = surface->texinfo->texture->texturechain;
    surface->texinfo->texture->texturechain = surface;
}

static void
R_RecursiveWorldNode(mnode_t * node, int planes)
{
    int i, side, intersections;
    float dot;
    mplane_t * plane;
    msurface_t * surface;
    mleaf_t * leaf;

    if (node->contents == CONTENTS_SOLID || node->visframe != r_visframecount)
        return;

    intersections = R_WorldFrustumCheck(node, planes);
    if (intersections < 0)
        return;

    if (node->contents < 0) {
        leaf = (mleaf_t *) node;
        for (i = 0; i < leaf->nummarksurfaces; ++i)
            leaf->firstmarksurface[i]->visframe = r_framecount;
        if (leaf->efrags)
            R_StoreEfrags(&leaf->efrags);
        return;
    }

    plane = node->plane;
    dot   = plane->type < 3 ? modelorg[plane->type] - plane->dist : DotProduct(modelorg, plane->normal) - plane->dist;
    side  = dot < 0;
    // Descendants of an accepted box cannot cross a frustum plane.
    if (!intersections)
        planes = 0;
    R_RecursiveWorldNode(node->children[side], planes);
    surface = cl.worldmodel->surfaces + node->firstsurface;
    for (i = 0; i < node->numsurfaces; ++i, ++surface) {
        if (surface->visframe != r_framecount)
            continue;
        if (!(surface->flags & SURF_UNDERWATER) && ((dot < 0) ^ !!(surface->flags & SURF_PLANEBACK)))
            continue;
        if (mirror && surface->texinfo->texture == cl.worldmodel->textures[mirrortexturenum])
            continue;
        R_ChainWorldSurface(surface, intersections);
    }
    R_RecursiveWorldNode(node->children[!side], planes);
} /* R_RecursiveWorldNode */

static void
R_AddStaticBrushModelsToChains(int planes)
{
    int i, j, p, side, intersections;
    float dot;
    vec3_t mins, maxs;
    entity_t * entity;
    model_t * model;
    msurface_t * surface;

    for (i = 0; i < cl_numstaticbrushmodels; ++i) {
        entity = cl_staticbrushmodels[i];
        model  = entity->model;
        VectorAdd(entity->origin, model->mins, mins);
        VectorAdd(entity->origin, model->maxs, maxs);
        intersections = 0;
        for (p = 0; p < planes; ++p) {
            side = BoxOnPlaneSide(mins, maxs, &frustum[p]);
            if (side == 2)
                break;
            intersections += side == 3;
        }
        if (p != planes)
            continue;
        surface = model->surfaces + model->firstmodelsurface;
        for (j = 0; j < model->nummodelsurfaces; ++j, ++surface) {
            dot = DotProduct(modelorg, surface->plane->normal) - surface->plane->dist;
            if ((surface->flags & SURF_PLANEBACK) ?
              dot < (float) -BACKFACE_EPSILON : dot > (float) BACKFACE_EPSILON)
                R_ChainWorldSurface(surface, intersections);
        }
    }
}

static void
R_DrawTextureChains(void)
{
    int i;
    texture_t * texture;
    msurface_t * surface, * next;

    R_BeginWorldSurfaces();
    for (i = 0; i < cl.worldmodel->numtextures; ++i) {
        texture = cl.worldmodel->textures[i];
        if (!texture || !texture->texturechain)
            continue;
        if (i == skytexturenum) {
            texture->texturechain = NULL;
            continue;
        }
        surface = texture->texturechain;
        if (i == mirrortexturenum && r_mirroralpha.value != 1.0f) {
            if (!mirror) {
                mirror       = true;
                mirror_plane = surface->plane;
            }
            continue;
        }
        if ((surface->flags & SURF_DRAWTURB) && r_wateralpha.value != 1.0f)
            continue;
        for (; surface; surface = next) {
            next = surface->texturechain;
            R_RenderBrushPoly(surface);
        }
        texture->texturechain = NULL;
    }
    R_DrawWorldBatches();
    R_EndWorldSurfaces();
} /* R_DrawTextureChains */

void
R_DrawWaterSurfaces(void)
{
    int i;
    texture_t * texture;
    msurface_t * surface;

    if (r_wateralpha.value == 1.0f)
        return;

    R_BeginWorldWater(r_wateralpha.value);
    for (i = 0; i < cl.worldmodel->numtextures; ++i) {
        texture = cl.worldmodel->textures[i];
        if (!texture || !texture->texturechain)
            continue;
        surface = texture->texturechain;
        if (!(surface->flags & SURF_DRAWTURB))
            continue;
        Hyena_BindTexture(texture->gl_texturenum);
        for (; surface; surface = surface->texturechain)
            EmitWaterPolys(surface);
        texture->texturechain = NULL;
    }
    R_EndWorldWater();
}

void
R_DrawWorld(void)
{
    entity_t entity;
    int planes = world_layout.frustum_planes;

    memset(&entity, 0, sizeof(entity));
    entity.model = cl.worldmodel;
    VectorCopy(r_refdef.vieworg, modelorg);
    currententity = &entity;
    Hyena_SetColor(1, 1, 1, 1);
    R_ClearLightmapChains();
    R_ClearWorldBatches();
    R_DrawSkyBox();
    R_RecursiveWorldNode(cl.worldmodel->nodes, planes);
    R_AddStaticBrushModelsToChains(planes);
    Fog_SetupFrame(true);
    R_DrawTextureChains();
    Fog_SetupFrame(false);
    R_BlendLightmaps();
}
