/*
 * Copyright (C) 1996-2001 Id Software, Inc.
 * Copyright (C) 2002-2009 John Fitzgibbons and others
 * Copyright (C) 2010-2014 QuakeSpasm developers
 * Copyright (C) 2023 NZ:P Team
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
// r_entity_fragments.c -- Entity fragment code

#include "../nzportable_def.h"

mnode_t * r_pefragtopnode;

#define MAX_ENTITY_FRAGMENTS 640


/*
 * ===============================================================================
 *
 *                  ENTITY FRAGMENT FUNCTIONS
 *
 * ericw -- GLQuake only uses efrags for static entities, and they're never
 * removed, so I trimmed out unused functionality and fields in efrag_t.
 *
 * Now, efrags are just a linked list for each leaf of the static
 * entities that touch that leaf. The efrags are hunk-allocated so there is no
 * fixed limit.
 *
 * This is inspired by MH's tutorial, and code from RMQEngine.
 * http://forums.insideqc.com/viewtopic.php?t=1930
 *
 * ===============================================================================
 */

vec3_t r_emins, r_emaxs;

static entity_t * r_addent;


#define EXTRA_EFRAGS 128

// based on RMQEngine
static efrag_t *
R_GetEfrag(void)
{
    // we could just Hunk_Alloc a single efrag_t and return it, but since
    // the struct is so small (2 pointers) allocate groups of them
    // to avoid wasting too much space on the hunk allocation headers.
    if (cl.free_efrags) {
        efrag_t * ef = cl.free_efrags;
        cl.free_efrags = ef->leafnext;
        ef->leafnext   = NULL;

        cl.num_efrags++;

        return ef;
    } else   {
        int i;

        cl.free_efrags = (efrag_t *) Hunk_AllocName(EXTRA_EFRAGS * sizeof(efrag_t), "efrags");

        for (i = 0; i < EXTRA_EFRAGS - 1; i++)
            cl.free_efrags[i].leafnext = &cl.free_efrags[i + 1];

        cl.free_efrags[i].leafnext = NULL;

        // call recursively to get a newly allocated free efrag
        return R_GetEfrag();
    }
}

/*
 * ===================
 * R_SplitEntityOnNode
 * ===================
 */
void
R_SplitEntityOnNode(mnode_t * node)
{
    efrag_t * ef;
    mplane_t * splitplane;
    mleaf_t * leaf;
    int sides;

    if (node->contents == CONTENTS_SOLID) {
        return;
    }

    // add an efrag if the node is a leaf

    if (node->contents < 0) {
        if (!r_pefragtopnode)
            r_pefragtopnode = node;

        leaf = (mleaf_t *) node;

        // grab an efrag off the free list
        ef         = R_GetEfrag();
        ef->entity = r_addent;

        // set the leaf links
        ef->leafnext = leaf->efrags;
        leaf->efrags = ef;

        return;
    }

    // NODE_MIXED

    splitplane = node->plane;
    sides      = BOX_ON_PLANE_SIDE(r_emins, r_emaxs, splitplane);

    if (sides == 3) {
        // split on this plane
        // if this is the first splitter of this bmodel, remember it
        if (!r_pefragtopnode)
            r_pefragtopnode = node;
    }

    // recurse down the contacted sides
    if (sides & 1)
        R_SplitEntityOnNode(node->children[0]);

    if (sides & 2)
        R_SplitEntityOnNode(node->children[1]);
} /* R_SplitEntityOnNode */

/*
 * ===========
 * R_AddEfrags
 * ===========
 */
void
R_AddEfrags(entity_t * ent)
{
    model_t * entmodel;
    vec_t scalefactor;

    if (!ent->model)
        return;

    r_addent = ent;

    r_pefragtopnode = NULL;

    entmodel    = ent->model;
    scalefactor = ENTSCALE_DECODE(ent->scale);
    if (scalefactor != 1.0f) {
        VectorMA(ent->origin, scalefactor, entmodel->mins, r_emins);
        VectorMA(ent->origin, scalefactor, entmodel->maxs, r_emaxs);
    } else   {
        VectorAdd(ent->origin, entmodel->mins, r_emins);
        VectorAdd(ent->origin, entmodel->maxs, r_emaxs);
    }

    R_SplitEntityOnNode(cl.worldmodel->nodes);

    ent->topnode = r_pefragtopnode;

    if (cl.num_efrags > MAX_ENTITY_FRAGMENTS)
        Sys_Error("Maximum amount of entity fragments (%d)", MAX_ENTITY_FRAGMENTS);
}

/*
 * ================
 * R_StoreEfrags -- johnfitz -- pointless switch statement removed.
 * ================
 */
void
R_StoreEfrags(efrag_t ** ppefrag)
{
    entity_t * pent;
    efrag_t * pefrag;

    while ((pefrag = *ppefrag) != NULL) {
        pent = pefrag->entity;

        if ((pent->visframe != r_framecount) && (cl_numvisedicts < MAX_VISEDICTS)) {
            cl_visedicts[cl_numvisedicts++] = pent;
            pent->visframe = r_framecount;
        }

        ppefrag = &pefrag->leafnext;
    }
}
