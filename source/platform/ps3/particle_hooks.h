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

#define R_DrawParticles PS3_SharedDrawParticles
#include "../../nzportable_def.h"

void
PS3_ParticleBlend(int source, int destination, int half_resolution);

#define PS3_HALFRES_TYPE(id)                                                                       \
        ((id) == p_fire || (id) == p_fire2 || (id) == p_inferno_flame || (id) == p_inferno_trail ||    \
        (id) == p_smoke || (id) == p_raysmoke || (id) == p_dpfire || (id) == p_dpsmoke ||             \
        (id) == p_flame || (id) == p_lavatrail || (id) == p_torch_flame || (id) == p_q3flame)

#define Hyena_SetBlendFunction(source, destination)                                                \
        PS3_ParticleBlend((source), (destination), PS3_HALFRES_TYPE(pt->id))
