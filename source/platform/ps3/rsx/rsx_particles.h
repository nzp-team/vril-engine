/*
 * Copyright (C) 2026 NZ:P Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#pragma once
#include <math.h>
#include <stddef.h>
#include <string.h>

static inline void
PS3_ParticleBounds(const void * positions, unsigned stride, unsigned count, const float * m,
  const int * viewport, int * bounds, int width, int height)
{
    float lo_x = (float) width, lo_y = (float) height, hi_x = 0, hi_y = 0;
    int y = height - viewport[1] - viewport[3];

    for (unsigned i = 0; i < count; ++i) {
        float p[3];
        memcpy(p, (const char *) positions + (size_t) i * stride, sizeof(p));
        float x  = m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12];
        float cy = m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13];
        float w  = m[3] * p[0] + m[7] * p[1] + m[11] * p[2] + m[15];

        if (!isfinite(x) || !isfinite(cy) || !isfinite(w)) {
            lo_x = viewport[0];
            hi_x = viewport[0] + viewport[2];
            lo_y = y;
            hi_y = y + viewport[3];
            break;
        }

        if (w <= 0.0001f) {
            int any_front = 0;

            for (unsigned j = 0; j < count; ++j) {
                float q[3];
                memcpy(q, (const char *) positions + (size_t) j * stride, sizeof(q));
                float qw = m[3] * q[0] + m[7] * q[1] + m[11] * q[2] + m[15];

                if (!isfinite(qw) || qw > 0.0001f) {
                    any_front = 1;
                    break;
                }
            }

            if (!any_front) {
                bounds[0] = bounds[1] = bounds[2] = bounds[3] = 0;
                return;
            }

            lo_x = viewport[0];
            hi_x = viewport[0] + viewport[2];
            lo_y = y;
            hi_y = y + viewport[3];
            break;
        }

        float sx = viewport[0] + (x / w * 0.5f + 0.5f) * viewport[2];
        float sy = y + (0.5f - cy / w * 0.5f) * viewport[3];
        lo_x = fminf(lo_x, sx);
        hi_x = fmaxf(hi_x, sx);
        lo_y = fminf(lo_y, sy);
        hi_y = fmaxf(hi_y, sy);
    }

    float left = fmaxf(0, fminf(width, viewport[0])),
      right    = fmaxf(0, fminf(width, viewport[0] + viewport[2]));
    float top = fmaxf(0, fminf(height, y)), bottom = fmaxf(0, fminf(height, y + viewport[3]));
    bounds[0] = (int) fmaxf(left, fminf(right, floorf(lo_x - 2)));
    bounds[1] = (int) fmaxf(top, fminf(bottom, floorf(lo_y - 2)));
    bounds[2] = (int) fmaxf(left, fminf(right, ceilf(hi_x + 2)));
    bounds[3] = (int) fmaxf(top, fminf(bottom, ceilf(hi_y + 2)));
}

static inline float
PS3_ParticleTriangleArea(const float p[3][3], const float * m, const int * viewport, int width,
  int height)
{
    float a[8][2], b[8][2];
    int count       = 3;
    float limits[4] = { fmaxf(0, viewport[0]), fminf(width, viewport[0] + viewport[2]),
                        fmaxf(0, height - viewport[1] - viewport[3]),
                        fminf(height, height - viewport[1]) };

    for (int i = 0; i < 3; ++i) {
        float x = m[0] * p[i][0] + m[4] * p[i][1] + m[8] * p[i][2] + m[12];
        float y = m[1] * p[i][0] + m[5] * p[i][1] + m[9] * p[i][2] + m[13];
        float w = m[3] * p[i][0] + m[7] * p[i][1] + m[11] * p[i][2] + m[15];

        if (!isfinite(x) || !isfinite(y) || !isfinite(w) || w <= 0.0001f)
            return 0;

        a[i][0] = viewport[0] + (x / w * .5f + .5f) * viewport[2];
        a[i][1] = height - viewport[1] - viewport[3] + (.5f - y / w * .5f) * viewport[3];
    }

    int inside = 1;

    for (int i = 0; i < 3; ++i)
        if (a[i][0] < limits[0] || a[i][0] > limits[1] || a[i][1] < limits[2] ||
          a[i][1] > limits[3])
            inside = 0;

    if (inside)
        return fabsf((a[1][0] - a[0][0]) * (a[2][1] - a[0][1])
                 - (a[1][1] - a[0][1]) * (a[2][0] - a[0][0]))
               * .5f;

    for (int plane = 0; plane < 4 && count; ++plane) {
        int axis = plane / 2, out = 0;
        float edge = limits[plane];

        for (int i = 0; i < count; ++i) {
            const float * current = a[i], * previous = a[(i + count - 1) % count];
            int ci = (plane & 1) ? current[axis] <= edge : current[axis] >= edge;
            int pi = (plane & 1) ? previous[axis] <= edge : previous[axis] >= edge;

            if (ci != pi) {
                float t = (edge - previous[axis]) / (current[axis] - previous[axis]);
                b[out][0]   = previous[0] + t * (current[0] - previous[0]);
                b[out++][1] = previous[1] + t * (current[1] - previous[1]);
            }

            if (ci) {
                b[out][0]   = current[0];
                b[out++][1] = current[1];
            }
        }

        count = out;
        memcpy(a, b, count * sizeof(a[0]));
    }

    float area = 0;

    for (int i = 0; i < count; ++i) {
        int j = (i + 1) % count;
        area += a[i][0] * a[j][1] - a[i][1] * a[j][0];
    }

    return fabsf(area) * .5f;
}

static inline void
PS3_IndexedParticleBounds(const void * positions, unsigned stride, unsigned vertex_count,
  const unsigned short * indices, unsigned index_count, const float * m,
  const int * viewport, int * bounds, int width, int height, float * pixels)
{
    if (pixels)
        *pixels = 0;

    bounds[0] = width;
    bounds[1] = height;
    bounds[2] = bounds[3] = 0;

    for (unsigned i = 0; i < index_count; i += 3) {
        float triangle[3][3];
        int part[4];

        for (unsigned j = 0; j < 3; ++j) {
            if (indices[i + j] >= vertex_count) {
                bounds[0] = bounds[1] = 0;
                bounds[2] = width;
                bounds[3] = height;
                return;
            }

            memcpy(triangle[j], (const char *) positions + (size_t) indices[i + j] * stride,
              sizeof(triangle[j]));
        }

        PS3_ParticleBounds(triangle, sizeof(triangle[0]), 3, m, viewport, part, width, height);

        if (part[2] <= part[0] || part[3] <= part[1])
            continue;

        if (pixels)
            *pixels += PS3_ParticleTriangleArea(triangle, m, viewport, width, height);

        if (part[0] < bounds[0])
            bounds[0] = part[0];

        if (part[1] < bounds[1])
            bounds[1] = part[1];

        if (part[2] > bounds[2])
            bounds[2] = part[2];

        if (part[3] > bounds[3])
            bounds[3] = part[3];
    }
}

static inline int
PS3_ParticleDepthRects(const int * bounds, int * coverage, int rects[4][4])
{
    int next[4] = { bounds[0] / 2, bounds[1] / 2, (bounds[2] + 1) / 2, (bounds[3] + 1) / 2 };

    if (next[2] <= next[0] || next[3] <= next[1])
        return 0;

    if (coverage[2] <= coverage[0] || coverage[3] <= coverage[1]) {
        memcpy(coverage, next, sizeof(next));
        memcpy(rects[0], next, sizeof(next));
        return 1;
    }

    int left   = next[0] < coverage[0] ? next[0] : coverage[0];
    int top    = next[1] < coverage[1] ? next[1] : coverage[1];
    int right  = next[2] > coverage[2] ? next[2] : coverage[2];
    int bottom = next[3] > coverage[3] ? next[3] : coverage[3];
    int candidates[4][4] = {
        { left, top, right, coverage[1] },
        { left, coverage[3], right, bottom     },
        { left, coverage[1], coverage[0], coverage[3] },
        { coverage[2], coverage[1], right, coverage[3] }
    };
    int count = 0;

    for (int i = 0; i < 4; ++i)
        if (candidates[i][2] > candidates[i][0] && candidates[i][3] > candidates[i][1])
            memcpy(rects[count++], candidates[i], sizeof(next));

    coverage[0] = left;
    coverage[1] = top;
    coverage[2] = right;
    coverage[3] = bottom;
    return count;
}
