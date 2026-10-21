/*
 * Copyright (C) 2026 NZP Team.
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
 */
// r_fov.h -- FOV calculation and culling
#pragma once

#include <math.h>

typedef struct {
    float world_zoom;
    float viewmodel_zoom;
} scr_zoom_t;

static inline float SCR_BoundFov(float fov)
{
    if (!(fov >= 10.0f)) return 10.0f;
    return fov > 170.0f ? 170.0f : fov;
}

static inline float SCR_LerpZoom(float current, float target, float fraction)
{
    float result = current + (target - current) * fraction;
    return fabsf(result - target) < 0.01f ? target : result;
}

static inline void SCR_UpdateZoom(scr_zoom_t *state, float world, float viewmodel, int zoom, float amount, float dt, int active)
{
    float world_target = 0.0f, viewmodel_target = 0.0f;
    float fraction = 1.0f - expf(-17.260924f * (dt > 0.0f ? dt : 0.0f));

    world = SCR_BoundFov(world);
    viewmodel = viewmodel == 0.0f ? 0.0f : SCR_BoundFov(viewmodel);
    if (!active) {
        state->world_zoom = state->viewmodel_zoom = 0.0f;
        return;
    }
    if (zoom == 1) {
        world_target = world - SCR_BoundFov(world - amount);
        if (viewmodel != 0.0f)
            viewmodel_target = viewmodel - SCR_BoundFov(viewmodel - amount);
    } else if (zoom == 2) {
        state->world_zoom = world - 30.0f;
        state->viewmodel_zoom = viewmodel == 0.0f ? 0.0f : viewmodel - 30.0f;
        return;
    }
    state->world_zoom = SCR_LerpZoom(state->world_zoom, world_target, fraction);
    state->viewmodel_zoom = viewmodel == 0.0f ? 0.0f : SCR_LerpZoom(state->viewmodel_zoom, viewmodel_target, fraction);
}

static inline float SCR_ViewmodelScale(float world, float viewmodel)
{
    if (viewmodel == 0.0f)
        return 1.0f;
    return viewmodel / (90.0f * tanf(world * (3.14159265358979323846f / 360.0f)));
}
