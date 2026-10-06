/*
 * Copyright (C) 1996-1997 Id Software, Inc.
 * Copyright (C) 2026 NZ:P Team
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
// r_surface.c -- Shared surface submission

#include "../nzportable_def.h"

extern int r_visframecount;
extern mleaf_t * r_viewleaf, * r_oldviewleaf;

void
R_DrawSurfaceFan(const float * source, int count, int stride,
  int position_offset, int texture_offset, qboolean warp, double time)
{
    hyena_arrays_t arrays = { 0 };

    if (count < 3)
        return;

    arrays.count = count;
    static float * warped_positions, * warped_uvs;
    static int capacity;
    int i;
    if (warp) {
        // Reuse planar scratch; picaGL can submit it without another interleaving copy.
        if (count > capacity) {
            warped_positions = realloc(warped_positions, count * 3 * sizeof(float));
            warped_uvs       = realloc(warped_uvs, count * 2 * sizeof(float));
            if (!warped_positions || !warped_uvs)
                Sys_Error("R_DrawSurfaceFan: out of memory");
            capacity = count;
        }
        for (i = 0; i < count; ++i) {
            const float * in = source + i * stride;
            warped_uvs[i * 2]       = in[texture_offset];
            warped_uvs[i * 2 + 1]   = in[texture_offset + 1];
            warped_positions[i * 3] = in[position_offset] + 8 * sinf(in[position_offset + 1] * 0.05f + (float) time)
              * sinf(in[position_offset + 2] * 0.05f + (float) time);
            warped_positions[i * 3 + 1] = in[position_offset + 1] + 8 * sinf(in[position_offset] * 0.05f + (float) time)
              * sinf(in[position_offset + 2] * 0.05f + (float) time);
            warped_positions[i * 3 + 2] = in[position_offset + 2];
        }
        arrays.uv.data       = warped_uvs;
        arrays.position.data = warped_positions;
    } else {
        arrays.uv.data       = source + texture_offset;
        arrays.position.data = source + position_offset;
        arrays.uv.stride     = arrays.position.stride = stride * sizeof(float);
    }
    Hyena_DrawArrays(HYE_TRIANGLE_FAN, &arrays, count);
} /* R_DrawSurfaceFan */

void
R_MarkVisibleLeaves(model_t * model, mleaf_t * leaf, int frame, qboolean all_visible)
{
    int i;
    const byte * visible = all_visible ? NULL : Mod_LeafPVS(leaf, model);
    mnode_t * node;

    for (i = 0; i < model->numleafs; ++i) {
        if (!all_visible && !(visible[i >> 3] & (1 << (i & 7))))
            continue;
        node = (mnode_t *) &model->leafs[i + 1];
        while (node && node->visframe != frame) {
            node->visframe = frame;
            node = node->parent;
        }
    }
}

void
R_MarkLeaves(qboolean all_visible, qboolean mirror_view)
{
    if ((r_oldviewleaf == r_viewleaf && !all_visible) || mirror_view)
        return;

    ++r_visframecount;
    r_oldviewleaf = r_viewleaf;
    R_MarkVisibleLeaves(cl.worldmodel, r_viewleaf, r_visframecount, all_visible);
}
