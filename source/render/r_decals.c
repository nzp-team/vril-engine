/*
 * Copyright (C) 2006, Charles Hollemeersch, Sputnikutah
 * Copyright (C) 2007-2008 Crow_bar.
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
// r_decals.c
#include "../nzportable_def.h"

#define DEFAULT_NUM_DECALS  256 // *4

#define ABSOLUTE_MIN_DECALS 256
#define ABSOLUTE_MAX_DECALS 32768
#define MAX_DECAL_VERTICES  128
#define MAX_DECAL_TRIANGLES 64

typedef struct decal_s {
    vec3_t           origin;
    vec3_t           normal;
    vec3_t           tangent;
    float            radius;
    int              bspdecal;

    struct decal_s * next;
    float            die;
    float            starttime;

    int              srcblend;
    int              dstblend;

    int              texture;

    // geometry of decal
    int              vertexCount, triangleCount;
    vec3_t           vertexArray[MAX_DECAL_VERTICES];
    float            texcoordArray[MAX_DECAL_VERTICES][2];
    int              triangleArray[MAX_DECAL_TRIANGLES][3];
} decal_t;

int decal_blood1, decal_blood2, decal_blood3, decal_q3blood, decal_burn, decal_mark, decal_glow;

static decal_t * decals, * active_decals, * free_decals;
static int r_numdecals;

static plane_t leftPlane, rightPlane, bottomPlane, topPlane, backPlane, frontPlane;

cvar_t r_decaltime = { "r_decaltime", "12" };
cvar_t r_decal_viewdistance = { "r_decal_viewdistance", "1024" };

void
DecalClipLeaf(decal_t * dec, mleaf_t * leaf);
void
DecalWalkBsp_R(decal_t * dec, mnode_t * node);
int
DecalClipPolygonAgainstPlane(plane_t * plane, int vertexCount, vec3_t * vertex, vec3_t * newVertex);
int
DecalClipPolygon(int vertexCount, vec3_t * vertices, vec3_t * newVertex);


float
RandomMinMax(float min, float max)
{
    return min + ((rand() % 10000) / 10000.0) * (max - min);
}

/*
 * ===============
 * R_InitDecals
 * ===============
 */
void
R_InitDecals(void)
{
    int i;

    Cvar_RegisterVariable(&r_decaltime);
    Cvar_RegisterVariable(&r_decal_viewdistance);

    if (!qmb_initialized)
        return;

    if ((i = COM_CheckParm("-decals")) && i + 1 < com_argc) {
        r_numdecals = Q_atoi(com_argv[i + 1]);
        r_numdecals = bound(ABSOLUTE_MIN_DECALS, r_numdecals, ABSOLUTE_MAX_DECALS);
    } else {
        r_numdecals = DEFAULT_NUM_DECALS;
    }

    decals = (decal_t *) Hunk_AllocName(r_numdecals * sizeof(decal_t), "decals");

    decal_blood1  = Image_LoadImage("textures/decals/blood_splat01", IMAGE_TGA, 0, true, false);
    decal_blood2  = Image_LoadImage("textures/decals/blood_splat02", IMAGE_TGA, 0, true, false);
    decal_blood3  = Image_LoadImage("textures/decals/blood_splat03", IMAGE_TGA, 0, true, false);
    decal_q3blood = Image_LoadImage("textures/decals/blood_stain", IMAGE_TGA, 0, true, false);
    decal_burn    = Image_LoadImage("textures/decals/explo_burn01", IMAGE_TGA, 0, true, false);
    decal_mark    = Image_LoadImage("textures/decals/particle_burn01", IMAGE_TGA, 0, true, false);
    decal_glow    = Image_LoadImage("textures/decals/glow2", IMAGE_TGA, 0, true, false);
}

/*
 * ===============
 * R_ClearDecals
 * ===============
 */
static int wadreload = 1;
void
R_ClearDecals(void)
{
    int i;

    if (!qmb_initialized)
        return;

    memset(decals, 0, r_numdecals * sizeof(decal_t));
    free_decals   = &decals[0];
    active_decals = NULL;

    for (i = 0 ; i < r_numdecals ; i++)
        decals[i].next = &decals[i + 1];

    decals[r_numdecals - 1].next = NULL;

    wadreload = 1;
}

void
R_SpawnDecal(vec3_t center, vec3_t normal, vec3_t tangent, int tex, int size, int isbsp)
{
    int a;
    float width, height, depth, d, one_over_w, one_over_h;
    vec3_t binormal, test = { 0.5, 0.5, 0.5 };
    decal_t * dec;

    if (!qmb_initialized)
        return;

    // allocate decal
    if (!free_decals)
        return;

    dec           = free_decals;
    free_decals   = dec->next;
    dec->next     = active_decals;
    active_decals = dec;

    VectorNormalize(test);
    CrossProduct(normal, test, tangent);

    VectorCopy(center, dec->origin);
    VectorCopy(tangent, dec->tangent);
    VectorCopy(normal, dec->normal);
    VectorNormalize(tangent);
    VectorNormalize(normal);
    CrossProduct(normal, tangent, binormal);
    VectorNormalize(binormal);

    width          = RandomMinMax(size * 0.5, size);
    height         = width;
    depth          = width * 0.5;
    dec->radius    = fmax(fmax(width, height), depth);
    dec->starttime = cl.time;
    dec->bspdecal  = isbsp;
    dec->die       = (isbsp ? 0 : cl.time + r_decaltime.value);
    dec->texture   = tex;

    // Calculate boundary planes
    d = DotProduct(center, tangent);
    VectorCopy(tangent, leftPlane.normal);
    leftPlane.dist = -(width * 0.5 - d);
    VectorNegate(tangent, tangent);
    VectorCopy(tangent, rightPlane.normal);
    VectorNegate(tangent, tangent);
    rightPlane.dist = -(width * 0.5 + d);

    d = DotProduct(center, binormal);
    VectorCopy(binormal, bottomPlane.normal);
    bottomPlane.dist = -(height * 0.5 - d);
    VectorNegate(binormal, binormal);
    VectorCopy(binormal, topPlane.normal);
    VectorNegate(binormal, binormal);
    topPlane.dist = -(height * 0.5 + d);

    d = DotProduct(center, normal);
    VectorCopy(normal, backPlane.normal);
    backPlane.dist = -(depth - d);
    VectorNegate(normal, normal);
    VectorCopy(normal, frontPlane.normal);
    VectorNegate(normal, normal);
    frontPlane.dist = -(depth + d);

    // Begin with empty mesh
    dec->vertexCount   = 0;
    dec->triangleCount = 0;

    // Clip decal to bsp
    DecalWalkBsp_R(dec, cl.worldmodel->nodes);

    // This happens when a decal is to far from any surface or the surface is to steeply sloped
    if (dec->triangleCount == 0) { // deallocate decal
        active_decals = dec->next;
        dec->next     = free_decals;
        free_decals   = dec;
        return;
    }

    // Assign texture mapping coordinates
    one_over_w = 1.0F / width;
    one_over_h = 1.0F / height;
    for (a = 0 ; a < dec->vertexCount ; a++) {
        float s, t;
        vec3_t v;

        VectorSubtract(dec->vertexArray[a], center, v);
        s = DotProduct(v, tangent) * one_over_w + 0.5F;
        t = DotProduct(v, binormal) * one_over_h + 0.5F;
        dec->texcoordArray[a][0] = s;
        dec->texcoordArray[a][1] = t;
    }
} /* R_SpawnDecal */

// Revamped by blubs
void
R_SpawnDecalStatic(vec3_t org, int tex, int size)
{
    int i;
    float frac, bestfrac;
    vec3_t tangent, v, bestorg, normal, bestnormal, org2;
    vec3_t tempVec;

    if (!qmb_initialized)
        return;

    VectorClear(bestorg);
    VectorClear(bestnormal);
    VectorClear(tempVec);

    bestfrac = 10;
    for (i = 0 ; i < 26 ; i++) {
        // Reference: i = 0: check straight up, i = 1: check straight down
        // 1 < i < 10: Check sideways in increments of 45 degrees
        // 9 < i < 18: Check angled 45 degrees down in increments of 45 degrees
        // 17 < i : Check angled 45 degrees up in increments of 45 degrees
        org2[0] = (((((i - 2) % 8) < 2) || (((i - 2) % 8) == 7)) ? 1 : 0 ) + ((((i - 2) % 8) > 2 &&
          ((i - 2) % 8) < 6) ? -1 : 0 );
        org2[1] = ((((i - 2) % 8) > 0 && ((i - 2) % 8) < 4) ? 1 : 0 ) + ((((i - 2) % 8) > 4 &&
          ((i - 2) % 8) < 7) ? -1 : 0 );
        org2[2] = ((i == 0) ? 1 : 0) + ((i == 1) ? -1 : 0) + (((i > 9) && (i < 18)) ? 1 : 0) + ((i > 17) ? -1 : 0);

        VectorCopy(org, tempVec);
        VectorMA(tempVec, -0.1, org2, tempVec);

        VectorMA(org, 20, org2, org2);
        TraceLineN(tempVec, org2, v, normal);

        VectorSubtract(org2, tempVec, org2);// goal
        VectorSubtract(v, tempVec, tempVec);// collision

        if (VectorLength(org2) == 0)
            return;

        frac = VectorLength(tempVec) / VectorLength(org2);

        if (frac < 1 && frac < bestfrac) {
            bestfrac = frac;
            VectorCopy(v, bestorg);
            VectorCopy(normal, bestnormal);
            CrossProduct(normal, bestnormal, tangent);
        }
    }

    if (bestfrac < 1)
        R_SpawnDecal(bestorg, bestnormal, tangent, tex, size, 0);
} /* R_SpawnDecalStatic */

// Crow_bar.
void
R_SpawnDecalBSP(vec3_t org, char * texname, int size)
{
    int i;
    float frac, bestfrac;
    vec3_t tangent, v, bestorg, normal, bestnormal, org2;
    vec3_t tempVec;

    if (!qmb_initialized)
        return;

    int tex = Image_LoadImage(va("decals/%s", texname), IMAGE_TGA, 0, false, false);

    VectorClear(bestorg);
    VectorClear(bestnormal);
    VectorClear(tempVec);

    bestfrac = 10;
    for (i = 0 ; i < 26 ; i++) {
        // Reference: i = 0: check straight up, i = 1: check straight down
        // 1 < i < 10: Check sideways in increments of 45 degrees
        // 9 < i < 18: Check angled 45 degrees down in increments of 45 degrees
        // 17 < i : Check angled 45 degrees up in increments of 45 degrees
        org2[0] = (((((i - 2) % 8) < 2) || (((i - 2) % 8) == 7)) ? 1 : 0 ) + ((((i - 2) % 8) > 2 &&
          ((i - 2) % 8) < 6) ? -1 : 0 );
        org2[1] = ((((i - 2) % 8) > 0 && ((i - 2) % 8) < 4) ? 1 : 0 ) + ((((i - 2) % 8) > 4 &&
          ((i - 2) % 8) < 7) ? -1 : 0 );
        org2[2] = ((i == 0) ? 1 : 0) + ((i == 1) ? -1 : 0) + (((i > 9) && (i < 18)) ? 1 : 0) + ((i > 17) ? -1 : 0);

        VectorCopy(org, tempVec);
        VectorMA(tempVec, -0.1, org2, tempVec);

        VectorMA(org, 20, org2, org2);
        TraceLineN(tempVec, org2, v, normal);

        VectorSubtract(org2, tempVec, org2);// goal
        VectorSubtract(v, tempVec, tempVec);// collision

        if (VectorLength(org2) == 0)
            return;

        frac = VectorLength(tempVec) / VectorLength(org2);

        if (frac < 1 && frac < bestfrac) {
            bestfrac = frac;
            VectorCopy(v, bestorg);
            VectorCopy(normal, bestnormal);
            CrossProduct(normal, bestnormal, tangent);
        }
    }

    if (bestfrac < 1)
        R_SpawnDecal(bestorg, bestnormal, tangent, tex, size, 1);
} /* R_SpawnDecalBSP */

void
DecalWalkBsp_R(decal_t * dec, mnode_t * node)
{
    float dist;
    mplane_t * plane;
    mleaf_t * leaf;

    if (node->contents < 0) { // we are in a leaf
        leaf = (mleaf_t *) node;
        DecalClipLeaf(dec, leaf);
        return;
    }

    plane = node->plane;
    dist  = DotProduct(dec->origin, plane->normal) - plane->dist;

    if (dist > dec->radius) {
        DecalWalkBsp_R(dec, node->children[0]);
        return;
    }
    if (dist < -dec->radius) {
        DecalWalkBsp_R(dec, node->children[1]);
        return;
    }

    DecalWalkBsp_R(dec, node->children[0]);
    DecalWalkBsp_R(dec, node->children[1]);
}

qboolean
DecalAddPolygon(decal_t * dec, int vertcount, vec3_t * vertices)
{
    int a, b, count;

    count = dec->vertexCount;
    if (count + vertcount >= MAX_DECAL_VERTICES)
        return false;

    if (dec->triangleCount + vertcount - 2 >= MAX_DECAL_TRIANGLES)
        return false;

    // Add polygon as a triangle fan
    for (a = 2 ; a < vertcount ; a++) {
        dec->triangleArray[dec->triangleCount][0] = count;
        dec->triangleArray[dec->triangleCount][1] = (count + a - 1);
        dec->triangleArray[dec->triangleCount][2] = (count + a );
        dec->triangleCount++;
    }

    // Assign vertex colors
    for (b = 0 ; b < vertcount ; b++) {
        VectorCopy(vertices[b], dec->vertexArray[count]);
        count++;
    }

    dec->vertexCount = count;
    return true;
}

const double decalEpsilon = 0.001;

void
DecalClipLeaf(decal_t * dec, mleaf_t * leaf)
{
    int c;
    vec3_t newVertex[64], t3;
    msurface_t ** surf;

    c    = leaf->nummarksurfaces;
    surf = leaf->firstmarksurface;

    // for all surfaces in the leaf
    for (c = 0 ; c < leaf->nummarksurfaces ; c++, surf++) {
        int i, count;
        msurface_t * surface = *surf;
        for (i = 0; i < surface->numedges && i < 64; ++i) {
            int edge_index   = cl.worldmodel->surfedges[surface->firstedge + i];
            medge_t * edge   = &cl.worldmodel->edges[edge_index < 0 ? -edge_index : edge_index];
            int vertex_index = edge->v[edge_index < 0 ? 1 : 0];
            VectorCopy(cl.worldmodel->vertexes[vertex_index].position, newVertex[i]);
        }
        count = surface->numedges < 64 ? surface->numedges : 64;

        VectorCopy((*surf)->plane->normal, t3);

        if ((*surf)->flags & SURF_PLANEBACK)
            VectorNegate(t3, t3);

        // avoid backfacing and ortogonal facing faces to recieve decal parts
        if (DotProduct(dec->normal, t3) > decalEpsilon) {
            count = DecalClipPolygon(count, newVertex, newVertex);
            if (count != 0 && !DecalAddPolygon(dec, count, newVertex))
                break;
        }
    }
} /* DecalClipLeaf */

int
DecalClipPolygon(int vertexCount, vec3_t * vertices, vec3_t * newVertex)
{
    vec3_t tempVertex[64];

    // Clip against all six planes
    int count = DecalClipPolygonAgainstPlane(&leftPlane, vertexCount, vertices, tempVertex);

    if (count != 0) {
        count = DecalClipPolygonAgainstPlane(&rightPlane, count, tempVertex, newVertex);
        if (count != 0) {
            count = DecalClipPolygonAgainstPlane(&bottomPlane, count, newVertex, tempVertex);
            if (count != 0) {
                count = DecalClipPolygonAgainstPlane(&topPlane, count, tempVertex, newVertex);
                if (count != 0) {
                    count = DecalClipPolygonAgainstPlane(&backPlane, count, newVertex, tempVertex);
                    if (count != 0) {
                        count = DecalClipPolygonAgainstPlane(&frontPlane, count, tempVertex, newVertex);
                    }
                }
            }
        }
    }

    return count;
}

int
DecalClipPolygonAgainstPlane(plane_t * plane, int vertexCount, vec3_t * vertex, vec3_t * newVertex)
{
    int a, b, c, count, negativeCount = 0;
    float t;
    bool negative[65];
    vec3_t v1, v2;

    // Classify vertices
    for (a = 0 ; a < vertexCount ; a++) {
        bool neg = ((DotProduct(plane->normal, vertex[a]) - plane->dist) < 0.0);
        negative[a]    = neg;
        negativeCount += neg;
    }

    // Discard this polygon if it's completely culled
    if (negativeCount == vertexCount)
        return 0;

    count = 0;
    for (b = 0 ; b < vertexCount ; b++) {
        // c is the index of the previous vertex
        c = (b != 0) ? b - 1 : vertexCount - 1;

        if (negative[b]) {
            if (!negative[c]) {
                // Current vertex is on negative side of plane, but previous vertex is on positive side.
                VectorCopy(vertex[c], v1);
                VectorCopy(vertex[b], v2);

                t = (DotProduct(plane->normal, v1) - plane->dist)
                  / (plane->normal[0] * (v1[0] - v2[0])
                  + plane->normal[1] * (v1[1] - v2[1])
                  + plane->normal[2] * (v1[2] - v2[2]));

                VectorScale(v1, (1.0 - t), newVertex[count]);
                VectorMA(newVertex[count], t, v2, newVertex[count]);

                count++;
            }
        } else {
            if (negative[c]) {
                // Current vertex is on positive side of plane, but previous vertex is on negative side.
                VectorCopy(vertex[b], v1);
                VectorCopy(vertex[c], v2);

                t = (DotProduct(plane->normal, v1) - plane->dist)
                  / (plane->normal[0] * (v1[0] - v2[0])
                  + plane->normal[1] * (v1[1] - v2[1])
                  + plane->normal[2] * (v1[2] - v2[2]));

                VectorScale(v1, (1.0 - t), newVertex[count]);
                VectorMA(newVertex[count], t, v2, newVertex[count]);

                count++;
            }

            // Include current vertex
            VectorCopy(vertex[b], newVertex[count]);
            count++;
        }
    }

    // Return number of vertices in clipped polygon
    return count;
} /* DecalClipPolygonAgainstPlane */

/*
 * ===============
 * R_DrawDecals
 * ===============
 */
void
R_DrawDecals(void)
{
    int i, v;
    float dcolor, alpha;
    vec3_t decaldist;
    decal_t * p, * kill;

    if (!qmb_initialized)
        return;

    while ((kill = active_decals) != NULL && kill->die < cl.time && !kill->bspdecal) {
        active_decals = kill->next;
        kill->next    = free_decals;
        free_decals   = kill;
    }

    Hyena_EnableCapability(HYE_BLEND);
    Hyena_SetTextureMode(HYE_MODULATE);
    Hyena_SetShadeMode(HYE_SMOOTH);
    Hyena_SetBlendFunction(HYE_SRC_ALPHA, HYE_ONE_MINUS_SRC_ALPHA);
    Hyena_DepthMask(HYE_FALSE);
    Hyena_SetDepthOffset(-1.0f);

    for (p = active_decals; p; p = p->next) {
        while ((kill = p->next) != NULL && kill->die < cl.time && !kill->bspdecal) {
            p->next     = kill->next;
            kill->next  = free_decals;
            free_decals = kill;
        }

        VectorSubtract(r_refdef.vieworg, p->origin, decaldist);
        if (VectorLength(decaldist) > r_decal_viewdistance.value)
            continue;

        if (!p->bspdecal && p->die - cl.time < 0.5f) {
            dcolor = 1.0f;
            alpha  = bound(0.0f, 2.0f * (p->die - cl.time), 1.0f);
        } else {
            dcolor = bound(0.0f, 1.0f - VectorLength(decaldist) / r_decal_viewdistance.value, 1.0f);
            alpha  = dcolor;
        }

        Hyena_BindTexture(p->texture);
        Hyena_SetColor(dcolor, dcolor, dcolor, alpha);

        for (i = 0; i < p->triangleCount; ++i) {
            vertex_t * vertices;
            Hyena_BeginVertices(HYE_TRIANGLE_FAN);
            vertices = Hyena_AllocateMemoryForVertices(3);
            for (v = 0; v < 3; ++v) {
                int index = p->triangleArray[i][v];
                Hyena_2DTextureCoord(&vertices[v],
                  p->texcoordArray[index][0], p->texcoordArray[index][1]);
                Hyena_VertexXYZ(&vertices[v],
                  p->vertexArray[index][0], p->vertexArray[index][1], p->vertexArray[index][2]);
            }
            Hyena_DrawVertices(vertices, 3, HYE_TEXTURE_32BITFLOAT, HYE_VERTEX_32BITFLOAT);
            Hyena_EndVertices();
        }
    }

    Hyena_SetDepthOffset(0.0f);
    Hyena_DisableCapability(HYE_BLEND);
    Hyena_DepthMask(HYE_TRUE);
    Hyena_SetColor(1.0f, 1.0f, 1.0f, 1.0f);
    Hyena_SetTextureMode(HYE_REPLACE);
    Hyena_SetShadeMode(HYE_FLAT);
} /* R_DrawDecals */
