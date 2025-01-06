extern"C" {
    #include "../quakedef.h"
    void PR_CheckEmptyString (char *s);

}

#include <pspgu.h>
#include <pspgum.h>


#include "nlohmann/json.hpp"
using json = nlohmann::json;


#include "video_hardware_iqm.hpp"


extern vec3_t lightcolor; // LordHavoc: .lit support
extern char	skybox_name[32];
extern void Fog_DisableGFog();
extern void Fog_EnableGFog();
// extern char *PF_VarString (int	first);
extern char pr_string_temp[PR_MAX_TEMPSTRING];


#define VERT_BONES 4
#define TRI_VERTS 3
#define SUBMESH_BONES 8


// Enable any of these to turn on debug prints
// #define IQMDEBUG_LOADIQM_LOADMESH
// #define IQMDEBUG_LOADIQM_MESHSPLITTING //
// #define IQMDEBUG_LOADIQM_BONEINFO
#define IQMDEBUG_LOADIQM_ANIMINFO //
// #define IQMDEBUG_LOADIQM_DEBUGSUMMARY
#define IQMDEBUG_LOADIQM_RELOCATE
#define IQMDEBUG_SKEL_TRACELINE
// #define IQMDEBUG_CALC_MODEL_BOUNDS


// Special IQM animation event
#define IQM_ANIM_EVENT_CODE_MOVE_DIST 2



skeletal_skeleton_t sv_skeletons[MAX_SKELETONS]; // Server-side skeleton objects
skeletal_skeleton_t cl_skeletons[MAX_SKELETONS]; // Client-side skeleton objects
int sv_n_skeletons = 0;
int cl_n_skeletons = 0;



void free_skeletal_model(skeletal_model_t *skel_model);
void calc_skel_model_bounds_for_rest_pose(skeletal_model_t *skel_model, vec3_t model_mins, vec3_t model_maxs);
void calc_skel_model_bounds_for_anim(skeletal_model_t *skel_model, skeletal_model_t *anim_model, vec3_t model_mins, vec3_t model_maxs);
void cl_get_skel_model_bounds(int model_idx, int animmodel_idx, vec3_t mins, vec3_t maxs);
void sv_get_skel_model_bounds(int model_idx, int animmodel_idx, vec3_t mins, vec3_t maxs);
void _get_skel_model_bounds(skeletal_model_t *skel_model, skeletal_model_t *skel_anim, int model_idx, int animmodel_idx, vec3_t mins, vec3_t maxs);


// 
// Converts a 3x4 mat3x4_t to a 4x4 ScePspFMatrix4
// 
void Matrix3x4_to_ScePspFMatrix4(ScePspFMatrix4 *out, mat3x4_t *in) {
    out->x.x = (*in)[0][0];     out->y.x = (*in)[0][1];     out->z.x = (*in)[0][2];     out->w.x = (*in)[0][3];
    out->x.y = (*in)[1][0];     out->y.y = (*in)[1][1];     out->z.y = (*in)[1][2];     out->w.y = (*in)[1][3];
    out->x.z = (*in)[2][0];     out->y.z = (*in)[2][1];     out->z.z = (*in)[2][2];     out->w.z = (*in)[2][3];
    out->x.w = 0.0f;            out->y.w = 0.0f;            out->z.w = 0.0f;            out->w.w = 1.0f;
}

// 
// Applies the transformation `transform` to the AABB `in` and produces 
// an updated world-space AABB `out` that wraps the `in` AABB
// See: https://realtimecollisiondetection.net/books/rtcd/
//
void transform_AABB(vec3_t in_mins, vec3_t in_maxs, mat3x4_t transform, vec3_t out_mins, vec3_t out_maxs) {
    for(int i = 0; i < 3; i++) {
        out_mins[i] = transform[i][3];
        out_maxs[i] = transform[i][3];
        for(int j = 0; j < 3; j++) {
            float e = transform[i][j] * in_mins[j];
            float f = transform[i][j] * in_maxs[j];
            if(e < f) {
                out_mins[i] += e;
                out_maxs[i] += f;
            }
            else {
                out_mins[i] += f;
                out_maxs[i] += e;
            }
        }
    }
}




// 
// Returns true if `bone_idx` is in list `bone_list`
// 
bool bone_in_list(uint8_t bone_idx, const uint8_t *bone_list, const int n_bones) {
    for(int i = 0; i < n_bones; i++) {
        if(bone_idx == bone_list[i]) {
            return true;
        }
    }
    return false;
}

//
// Returns true if list `bone_list_a` is a subset of `bone_list_b`
//
bool bone_list_is_subset(const uint8_t *bone_list_a, const int bone_list_a_n_bones, const uint8_t *bone_list_b, const int bone_list_b_n_bones) {
    // Early-out, cannot be subset if it has more bones
    // NOTE - This assumes the lists do not contain duplicate bones.
    if(bone_list_a_n_bones > bone_list_b_n_bones) {
        return false;
    }
    for(int i = 0; i < bone_list_a_n_bones; i++) {
        bool bone_in_list = false;
        for(int j = 0; j < bone_list_b_n_bones; j++) {
            if(bone_list_a[i] == bone_list_b[j]) {
                bone_in_list = true;
                break;
            }
        }
        // If any bone was missing from list b, not a subset
        if(!bone_in_list) {
            return false;
        }
    }
    // All `a` bones were found in `b`, `a` is a subset of `b`.
    return true;
}

//
// Returns the number of bones that are in `bone_list_a` but not in `bone_list_b`
//  i.e. len(set(bone_list_a) - set(bone_list_b))
//
int bone_list_set_difference(const uint8_t *bone_list_a, const int bone_list_a_n_bones, const uint8_t *bone_list_b, const int bone_list_b_n_bones) {
    int n_missing_bones = 0;

    for(int i = 0; i < bone_list_a_n_bones; i++) {
        bool bone_in_list = false;
        for(int j = 0; j < bone_list_b_n_bones; j++) {
            if(bone_list_a[i] == bone_list_b[j]) {
                bone_in_list = true;
                break;
            }
        }
        // If bone was missing from list b, count it
        if(!bone_in_list) {
            n_missing_bones += 1;
        }
    }
    return n_missing_bones;
}

//
// Performs the set union of bones in both `bone_list_a` and `bone_list_b`
// Writes the union into `bone_list_a`
//  i.e. len(set(bone_list_a) + set(bone_list_b))
//
// WARNING: Assumes `bone_list_a` has the capacity for the union of both lists.
//
int bone_list_set_union(uint8_t *bone_list_a, int bone_list_a_n_bones, const uint8_t *bone_list_b, const int bone_list_b_n_bones) {
    for(int i = 0; i < bone_list_b_n_bones; i++) {
        bool bone_already_in_a = false;

        for(int j = 0; j < bone_list_a_n_bones; j++) {
            if(bone_list_a[j] == bone_list_b[i]) {
                bone_already_in_a = true;
                break;
            }
        }
        if(bone_already_in_a) {
            continue;
        }

        bone_list_a[bone_list_a_n_bones] = bone_list_b[i];
        bone_list_a_n_bones += 1;
    }
    return bone_list_a_n_bones;
}



#define UINT16_T_MAX_VAL     65535
#define UINT16_T_MIN_VAL     0
#define INT16_T_MAX_VAL      32767
#define INT16_T_MIN_VAL     -32768
#define UINT8_T_MAX_VAL      255
#define UINT8_T_MIN_VAL      0
#define INT8_T_MAX_VAL       127
#define INT8_T_MIN_VAL      -128

// 
// Maps a float in [-1,1] to a int16_t in [-32768, 32767]
//
int16_t float_to_int16(float x) {
    // Rescale to [0,1]
    x = ((x+1) * 0.5);
    // Interpolate between min and max value
    int16_t val = (int16_t) (INT16_T_MIN_VAL + x * (INT16_T_MAX_VAL - INT16_T_MIN_VAL));
    // Clamp to bounds
    return std::min( (int16_t) INT16_T_MAX_VAL, std::max( (int16_t) INT16_T_MIN_VAL, val ));
}

// 
// Maps a float in [0,1] to a uint16_t in [0, 65535]
//
uint16_t float_to_uint16(float x) {
    // Interpolate between min and max value
    uint16_t val = (int16_t) (UINT16_T_MIN_VAL + x * (UINT16_T_MAX_VAL - UINT16_T_MIN_VAL));
    // Clamp to bounds
    return std::min( (uint16_t) UINT16_T_MAX_VAL, std::max( (uint16_t) UINT16_T_MIN_VAL, val ));
}

// 
// Maps a float in [-1,1] to a int8_t in [-128, 127]
//
int8_t float_to_int8(float x) {
    // Rescale to [0,1]
    x = ((x+1) * 0.5);
    // Interpolate between min and max value
    int8_t val = (int8_t) (INT8_T_MIN_VAL + x * (INT8_T_MAX_VAL - INT8_T_MIN_VAL));
    // Clamp to bounds
    return std::min( (int8_t) INT8_T_MAX_VAL, std::max( (int8_t) INT8_T_MIN_VAL, val ));
}

//
// Applies the inverse of an ofs and a scale
//
float apply_inv_ofs_scale(float x, float ofs, float scale) {
    return (x - ofs) / scale;
}

// 
// Maps a float in [0,1] to a uint8_t in [0, 255]
//
uint8_t float_to_uint8(float x) {
    // Interpolate between min and max value
    uint8_t val = (uint8_t) (UINT8_T_MIN_VAL + x * (UINT8_T_MAX_VAL - UINT8_T_MIN_VAL));
    // Clamp to bounds
    return std::min( (uint8_t) UINT8_T_MAX_VAL, std::max( (uint8_t) UINT8_T_MIN_VAL, val ));
}




// 
// Splits a `skeletal_mesh_t` mesh into one or more `skeletal_mesh_t` submeshes that reference no more than 8 bones
//
void submesh_skeletal_mesh(skeletal_mesh_t *mesh) {
#ifdef IQMDEBUG_LOADIQM_MESHSPLITTING
    Con_Printf("=========== Submesh started =============\n");
#endif // IQMDEBUG_LOADIQM_MESHSPLITTING

    // --------------------------------------------------------------------
    // Build the set of bones referenced by each triangle
    // --------------------------------------------------------------------
    uint8_t *tri_n_bones = (uint8_t*) malloc(sizeof(uint8_t) * mesh->n_tris); // Contains the number of bones that the i-th triangle references
    uint8_t *tri_bones = (uint8_t*) malloc(sizeof(uint8_t) * TRI_VERTS * VERT_BONES * mesh->n_tris); // Contains the list of bones referenced by the i-th triangle


    for(uint32_t tri_idx = 0; tri_idx < mesh->n_tris; tri_idx++) {
        // Initialize this triangle's bone list to 0s
        for(int tri_bone_idx = 0; tri_bone_idx < TRI_VERTS * VERT_BONES; tri_bone_idx++) {
            tri_bones[(tri_idx * TRI_VERTS * VERT_BONES) + tri_bone_idx] = 0;
        }
        tri_n_bones[tri_idx] = 0;

        for(uint32_t tri_vert_idx = 0; tri_vert_idx < TRI_VERTS; tri_vert_idx++ ) {
            uint32_t vert_idx = mesh->tri_verts[(tri_idx * TRI_VERTS) + tri_vert_idx];
            // Loop through the vertex's referenced bones
            for(int vert_bone_idx = 0; vert_bone_idx < VERT_BONES; vert_bone_idx++) {
                uint8_t bone_idx = mesh->vert_bone_idxs[vert_idx * VERT_BONES + vert_bone_idx];
                float bone_weight = mesh->vert_bone_weights[vert_idx * VERT_BONES + vert_bone_idx];

                if(bone_weight > 0) {
                    // Verify the bone is not already in this triangle's bone list
                    if(!bone_in_list(bone_idx, &tri_bones[tri_idx * TRI_VERTS * VERT_BONES], tri_n_bones[tri_idx])) {
                        tri_bones[(tri_idx * TRI_VERTS * VERT_BONES) + tri_n_bones[tri_idx]] = bone_idx;
                        tri_n_bones[tri_idx] += 1;
                    }
                }
            }
        }
    }
    // --------------------------------------------------------------------


    // // ========================================================================================================================
    // // ========================================================================================================================
    // // ---
    // // Let's just print the triangles, their bones, and their set assignment, see if that's what's crashing
    // // Next up: just skip to mesh[3] to see if it crashes on its own, or if the other meshes are putting us in a bad state
    // // ---
    // // Okay, we're still crashing here... interesting...

    // int n_prints = 0;

    // for(uint32_t tri_idx = 0; tri_idx < mesh->n_tris; tri_idx++) {
    //     Con_Printf("\t\tn_prints: %d\n", n_prints);
    //     Con_Printf("\t\tIQMDEBUG triangle %d, triangle set: %d, bone-list (%d): [ ", tri_idx, 0, tri_n_bones[tri_idx]); // IQMFIXME
    //     n_prints++;

    //     // --------------------------------------------------------------------
    //     // This works fine...
    //     // --------------------------------------------------------------------
    //     // Con_Printf("%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d]\n", 
    //     //     tri_bones[(tri_idx * TRI_VERTS * VERT_BONES) + 0],
    //     //     tri_bones[(tri_idx * TRI_VERTS * VERT_BONES) + 1],
    //     //     tri_bones[(tri_idx * TRI_VERTS * VERT_BONES) + 2],
    //     //     tri_bones[(tri_idx * TRI_VERTS * VERT_BONES) + 3],
    //     //     tri_bones[(tri_idx * TRI_VERTS * VERT_BONES) + 4],
    //     //     tri_bones[(tri_idx * TRI_VERTS * VERT_BONES) + 5],
    //     //     tri_bones[(tri_idx * TRI_VERTS * VERT_BONES) + 6],
    //     //     tri_bones[(tri_idx * TRI_VERTS * VERT_BONES) + 7],
    //     //     tri_bones[(tri_idx * TRI_VERTS * VERT_BONES) + 8],
    //     //     tri_bones[(tri_idx * TRI_VERTS * VERT_BONES) + 9],
    //     //     tri_bones[(tri_idx * TRI_VERTS * VERT_BONES) + 10],
    //     //     tri_bones[(tri_idx * TRI_VERTS * VERT_BONES) + 11]
    //     // );
    //     // --------------------------------------------------------------------

    //     // --------------------------------------------------------------------
    //     // --------------------------------------------------------------------
    //     // Weird... commenting this whole loop out fixes the crash... let's bring back some of it.
    //     for(uint32_t tri_bone_idx = 0; tri_bone_idx < TRI_VERTS * VERT_BONES; tri_bone_idx++) {
    //         // --
    //         // Check if somehow stack is running out
    //         // if(tri_bone_idx == 0) {
    //         //     Con_Printf("(&tri_bone_idx: %p) ", (void*) &tri_bone_idx);
    //         // }
    //         // --
    //         // // Only print the triangle's list of valid bones
    //         // if(tri_bone_idx >= tri_n_bones[tri_idx]) {
    //         //     break;
    //         // }
    //         // Print separator between used and unused indices
    //         if(tri_bone_idx == tri_n_bones[tri_idx]) {
    //             Con_Printf("// "); n_prints++;
    //         }

    //         int arr_ofs = (tri_idx * TRI_VERTS * VERT_BONES) + tri_bone_idx;
    //         Con_Printf("[%d] = ", arr_ofs); n_prints++;
    //         Con_Printf("%d, ", tri_bones[arr_ofs]); n_prints++;

    //         // I think this is the offending line...
    //         // Con_Printf(" (%d) ", (tri_idx * TRI_VERTS * VERT_BONES) + tri_bone_idx); // Still crashes, so not related to dtypes... hmmm
    //         // -- Switched tri_bone_idx to uint32_t --
    //         // Con_Printf(" (%d) ", 0); // This works fine, so it may not be print-related after all...
    //         // Con_Printf(" (%d) ", (tri_idx * TRI_VERTS * VERT_BONES) + tri_bone_idx); // This crashes... interesting
    //         // Crashes at ofs: 136
    //         // Crashes at triangle: 144
    //         // Con_Printf("%d, ", tri_bones[(tri_idx * TRI_VERTS * VERT_BONES) + tri_bone_idx]);
    //     }
    //     Con_Printf("]\n"); n_prints++;// IQMFIXME
    //     // --------------------------------------------------------------------
    // }
    // return; // IQMDEBUG - Skip everything after this function, should still crash
    // // Wow this still crashes... why?
    // // ========================================================================================================================
    // // ========================================================================================================================


    // // Debug print a few triangle bone lists:
    // for(int j = 0; j < 10; j++) {
    //     Con_Printf("Mesh: %d tri: %d bones (%d): ", i, j, tri_n_bones[j]);
    //     for(int k = 0; k < tri_n_bones[j]; k++) {
    //         Con_Printf("%d, ", tri_bones[j * TRI_VERTS * VERT_BONES + k]);
    //     }
    //     Con_Printf("\n");
    // }
    // break;

    // --------------------------------------------------------------------
    // Assign each triangle in the mesh to a submesh idx
    // --------------------------------------------------------------------
    int8_t *tri_submesh_idx = (int8_t*) malloc(sizeof(int8_t) * mesh->n_tris); // Contains the set the i-th triangle belongs to
    const int SET_DISCARD = -2; // Discarded triangles
    const int SET_UNASSIGNED = -1; // Denotes unassigned triangles
    for(uint32_t j = 0; j < mesh->n_tris; j++) {
        tri_submesh_idx[j] = SET_UNASSIGNED;
    }

    int cur_submesh_idx = -1;

    while(true) {
        // Find the unassigned triangle that uses the most bones:
        int cur_tri = -1;
        for(uint32_t tri_idx = 0; tri_idx < mesh->n_tris; tri_idx++) {
            // If this triangle isn't `UNASSIGNED`, skip it
            if(tri_submesh_idx[tri_idx] != SET_UNASSIGNED) {
                continue;
            }
            // If we haven't found one yet, set it
            if(cur_tri == -1) {
                cur_tri = tri_idx;
                continue;
            }
            // If this triangle references more bones, update cur_tri
            if(tri_n_bones[tri_idx] > tri_n_bones[cur_tri]) {
                cur_tri = tri_idx;
            }
        }

        // If we didn't find any triangles, stop submesh algorithm. We're done.
        if(cur_tri == -1) {
            break;
        }


        // // ========================================================================================================================
        // // ========================================================================================================================
        // // ---
        // // Let's just print the triangles, their bones, and their set assignment, see if that's what's crashing
        // // Next up: just skip to mesh[3] to see if it crashes on its own, or if the other meshes are putting us in a bad state
        // // ---
        // // Okay, we're still crashing here... interesting...
        // for(uint32_t tri_idx = 0; tri_idx < mesh->n_tris; tri_idx++) {
        //     Con_Printf("\t\tIQMDEBUG triangle %d, triangle set: %d, bone-list (%d): [ ", tri_idx, tri_submesh_idx[tri_idx], tri_n_bones[tri_idx]); // IQMFIXME
        //     for(int tri_bone_idx = 0; tri_bone_idx < TRI_VERTS * VERT_BONES; tri_bone_idx++) {
        //         // Print separator between used and unused indices
        //         if(tri_bone_idx == tri_n_bones[tri_idx]) {
        //             Con_Printf("// ");
        //         }
        //         Con_Printf("%d, ", tri_bones[(tri_idx * TRI_VERTS * VERT_BONES) + tri_bone_idx]);
        //     }
        //     Con_Printf("]\n"); // IQMFIXME
        // }
        // break; // IQMDEBUG - Skip everything after this function, should still crash
        // // ---
        // // ========================================================================================================================
        // // ========================================================================================================================




        // Verify the triangle doesn't have more than the max bones allowed:
        if(tri_n_bones[cur_tri] > SUBMESH_BONES) {
            Con_Printf("Warning: Mesh Triangle %d references %d bones, which is more than the maximum allowed for any mesh (%d). Skipping triangle...\n", cur_tri, tri_n_bones[cur_tri], SUBMESH_BONES);
            // Discard it
            tri_submesh_idx[cur_tri] = SET_DISCARD;
            continue;
        }


        cur_submesh_idx += 1;
        int cur_submesh_n_bones = 0;
        uint8_t *cur_submesh_bones = (uint8_t*) malloc(sizeof(uint8_t) * SUBMESH_BONES);
#ifdef IQMDEBUG_LOADIQM_MESHSPLITTING
        Con_Printf("Creating submesh %d for mesh\n", cur_submesh_idx);
#endif // IQMDEBUG_LOADIQM_MESHSPLITTING


        // Add the triangle to the current submesh:
        tri_submesh_idx[cur_tri] = cur_submesh_idx;

        // Add the triangle's bones to the current submesh:
        for(int submesh_bone_idx = 0; submesh_bone_idx < tri_n_bones[cur_tri]; submesh_bone_idx++) {
            cur_submesh_bones[submesh_bone_idx] = tri_bones[(cur_tri * TRI_VERTS * VERT_BONES) + submesh_bone_idx];
            cur_submesh_n_bones += 1;
        }

#ifdef IQMDEBUG_LOADIQM_MESHSPLITTING
        Con_Printf("\tstart submesh bones (%d): [", cur_submesh_n_bones);
        for(int submesh_bone_idx = 0; submesh_bone_idx < cur_submesh_n_bones; submesh_bone_idx++) {
            Con_Printf("%d, ", cur_submesh_bones[submesh_bone_idx]);
        }
        Con_Printf("]\n");
#endif // IQMDEBUG_LOADIQM_MESHSPLITTING

        // Add all unassigned triangles from the main mesh that references bones in this submesh's bone list
        for(uint32_t tri_idx = 0; tri_idx < mesh->n_tris; tri_idx++) {
            // If this triangle isn't `UNASSIGNED`, skip it
            if(tri_submesh_idx[tri_idx] != SET_UNASSIGNED) {
                continue;
            }

            // if(i == 0) {
            //     Con_Printf("Mesh %d submesh %d checking tri %d\n",i,cur_submesh_idx,tri_idx);
            //     Con_Printf("\tTri bones: (%d), ", tri_n_bones[tri_idx]);
            //     for(int tri_bone_idx = 0; tri_bone_idx < tri_n_bones[tri_idx]; tri_bone_idx++) {
            //         Con_Printf("%d,", tri_bones[(tri_idx * TRI_VERTS * VERT_BONES) + tri_bone_idx]);
            //     }
            //     Con_Printf("\n");
            // }

            // If this triangle's bones is not a subset of the current submesh bone list, skip it
            if(!bone_list_is_subset(&tri_bones[tri_idx * TRI_VERTS * VERT_BONES], tri_n_bones[tri_idx], cur_submesh_bones, cur_submesh_n_bones)) {
                continue;
            }

            // Otherwise, it is a subset, add it to the current submesh
            tri_submesh_idx[tri_idx] = cur_submesh_idx;
        }



#ifdef IQMDEBUG_LOADIQM_MESHSPLITTING
        {
            // Print how many triangles belong to the current submesh
            int cur_submesh_n_tris = 0;
            int n_assigned_tris = 0;
            for(uint32_t tri_idx = 0; tri_idx < mesh->n_tris; tri_idx++) {
                if(tri_submesh_idx[tri_idx] != SET_UNASSIGNED) {
                    n_assigned_tris++;
                }
                if(tri_submesh_idx[tri_idx] == cur_submesh_idx) {
                    cur_submesh_n_tris++;
                }
            }
            Con_Printf("\tcur submesh (%d) n_tris: %d/%d, total unassigned: %d/%d\n", cur_submesh_idx, cur_submesh_n_tris, mesh->n_tris, n_assigned_tris, mesh->n_tris);
        }
#endif // IQMDEBUG_LOADIQM_MESHSPLITTING


        // Repeat until there are no unassigned triangles remaining:
        while(true) {
            // Get triangle with the minimum number of bones not in the current submesh bone list
            cur_tri = -1;
            int cur_tri_n_missing_bones = 0;
            for(uint32_t tri_idx = 0; tri_idx < mesh->n_tris; tri_idx++) {
                // If this triangle isn't `UNASSIGNED`, skip it
                if(tri_submesh_idx[tri_idx] != SET_UNASSIGNED) {
                    continue;
                }
                // Count the number of bones referenced by this triangle that are not in the current submesh bone list
                int n_missing_bones = bone_list_set_difference(&tri_bones[tri_idx * TRI_VERTS * VERT_BONES], tri_n_bones[tri_idx], cur_submesh_bones, cur_submesh_n_bones);
                if(cur_tri == -1 || n_missing_bones < cur_tri_n_missing_bones) {
                    cur_tri = tri_idx;
                    cur_tri_n_missing_bones = n_missing_bones;
                }
                // If only missing one bone, we cannot do better than this. Stop here
                if(n_missing_bones == 1) {
                    break;
                }
            }

            // If no triangle found, stop. We're done.
            if(cur_tri == -1) {
#ifdef IQMDEBUG_LOADIQM_MESHSPLITTING
                Con_Printf("\tNo more unassigned triangles. Done with mesh.\n");
#endif // IQMDEBUG_LOADIQM_MESHSPLITTING
                break;
            }

            // If this triangle pushes us past the submesh-bone limit, we are done with this submesh. Move onto the next one.
            if(cur_submesh_n_bones + cur_tri_n_missing_bones > SUBMESH_BONES) {
#ifdef IQMDEBUG_LOADIQM_MESHSPLITTING
                Con_Printf("\tReached max number of bones allowed. Done with submesh.\n");
#endif // IQMDEBUG_LOADIQM_MESHSPLITTING
                break;
            }

#ifdef IQMDEBUG_LOADIQM_MESHSPLITTING
            Con_Printf("\tNext loop using triangle: %d, missing bones: %d\n", cur_tri, cur_tri_n_missing_bones);
            Con_Printf("\tCur submesh idx: %d\n", cur_submesh_idx);
            Con_Printf("\tCut submesh bones: (%d) [", cur_submesh_n_bones);
            for(int i = 0; i < cur_submesh_n_bones; i++) {
                Con_Printf("%d, ", cur_submesh_bones[i]);
            }
            Con_Printf("]\n");
#endif // IQMDEBUG_LOADIQM_MESHSPLITTING


            // Assign the triangle to the current submesh
            tri_submesh_idx[cur_tri] = cur_submesh_idx;
            // Con_Printf("\tIQMDEBUG Computing bone list set union\n"); // IQMFIXME

            // Add this triangle's bones to the current submesh list of bones
            cur_submesh_n_bones = bone_list_set_union( cur_submesh_bones, cur_submesh_n_bones, &tri_bones[cur_tri * TRI_VERTS * VERT_BONES], tri_n_bones[cur_tri]);

#ifdef IQMDEBUG_LOADIQM_MESHSPLITTING
            Con_Printf("\tcur submesh bones (%d): [", cur_submesh_n_bones);
            for(int submesh_bone_idx = 0; submesh_bone_idx < cur_submesh_n_bones; submesh_bone_idx++) {
                Con_Printf("%d, ", cur_submesh_bones[submesh_bone_idx]);
            }
            Con_Printf("]\n");
#endif // IQMDEBUG_LOADIQM_MESHSPLITTING

            // Con_Printf("\t\tIQMDEBUG Looking for unassigned triangles: \n");


            // // ========================================================================================================================
            // // ========================================================================================================================
            // // ---
            // // Let's just print the triangles, their bones, and their set assignment, see if that's what's crashing
            // // Next up: just skip to mesh[3] to see if it crashes on its own, or if the other meshes are putting us in a bad state
            // // ---
            // // Okay, we're still crashing here... interesting...
            // for(uint32_t tri_idx = 0; tri_idx < mesh->n_tris; tri_idx++) {
            //     Con_Printf("\t\tIQMDEBUG triangle %d, triangle set: %d, bone-list (%d): [ ", tri_idx, tri_submesh_idx[tri_idx], tri_n_bones[tri_idx]); // IQMFIXME
            //     for(int tri_bone_idx = 0; tri_bone_idx < TRI_VERTS * VERT_BONES; tri_bone_idx++) {
            //         // Print separator between used and unused indices
            //         if(tri_bone_idx == tri_n_bones[tri_idx]) {
            //             Con_Printf("// ");
            //         }
            //         Con_Printf("%d, ", tri_bones[(tri_idx * TRI_VERTS * VERT_BONES) + tri_bone_idx]);
            //     }
            //     Con_Printf("]\n"); // IQMFIXME
            // }
            // // ---
            // break; // IQMDEBUG - Skip everything after this function, should still crash
            // continue; // IQMDEBUG - Skip the following check, this crashes!
            // // ========================================================================================================================
            // // ========================================================================================================================


            // Add all unassigned triangles from the main mesh that reference bones in this submesh's bone list
            for(uint32_t tri_idx = 0; tri_idx < mesh->n_tris; tri_idx++) {
                // If this triangle isn't `UNASSIGNED`, skip it
                if(tri_submesh_idx[tri_idx] != SET_UNASSIGNED) {
                    continue;
                }
                // Con_Printf("\t\tIQMDEBUG triangle %d, triangle set: %d\n", tri_idx, tri_submesh_idx[tri_idx]); // IQMFIXME

                // if(i == 0) {
                //     Con_Printf("Mesh %d submesh %d checking tri %d\n",i,cur_submesh_idx,tri_idx);
                //     Con_Printf("\tTri bones: (%d), ", tri_n_bones[tri_idx]);
                //     for(int tri_bone_idx = 0; tri_bone_idx < tri_n_bones[tri_idx]; tri_bone_idx++) {
                //         Con_Printf("%d,", tri_bones[(tri_idx * TRI_VERTS * VERT_BONES) + tri_bone_idx]);
                //     }
                //     Con_Printf("\n");
                // }

                // Con_Printf("\t\tIQMDEBUG checking if triangle %d bone-list is subset\n", tri_idx); // IQMFIXME

                // ----
                // Con_Printf("\t\tIQMDEBUG triangle %d bone-list (%d): [", tri_idx, tri_n_bones[tri_idx]); // IQMFIXME
                // for(int tri_bone_idx = 0; tri_bone_idx < TRI_VERTS * VERT_BONES; tri_bone_idx++) {
                //     // Print separator between used and unused indices
                //     if(tri_bone_idx == tri_n_bones[tri_idx]) {
                //         Con_Printf("// ");
                //     }
                //     Con_Printf("%d, ", tri_bones[(tri_idx * TRI_VERTS * VERT_BONES) + tri_bone_idx]);
                // }
                // Con_Printf("]\n"); // IQMFIXME
                // ----



                // If this triangle's bones is not a subset of the current submesh bone list, skip it
                if(!bone_list_is_subset(&tri_bones[tri_idx * TRI_VERTS * VERT_BONES], tri_n_bones[tri_idx], cur_submesh_bones, cur_submesh_n_bones)) {
                    // Con_Printf("\t\tIQMDEBUG triangle %d bone-list is not subset, skipping\n", tri_idx);
                    continue;
                }
                // Con_Printf("\t\tIQMDEBUG triangle %d bone-list is subset, assigning to current submesh mesh: %d\n", tri_idx, cur_submesh_idx);

                // Otherwise, it is a subset, add it to the current submesh
                tri_submesh_idx[tri_idx] = cur_submesh_idx;
            }


            // Con_Printf("\t\tIQMDEBUG to count n_assigned / cur_submesh triangles\n"); // IQMFIXME

#ifdef IQMDEBUG_LOADIQM_MESHSPLITTING
            {
                // Print how many triangles belong to the current submesh
                int cur_submesh_n_tris = 0;
                int n_assigned_tris = 0;
                for(uint32_t tri_idx = 0; tri_idx < mesh->n_tris; tri_idx++) {
                    if(tri_submesh_idx[tri_idx] != SET_UNASSIGNED) {
                        n_assigned_tris++;
                    }
                    if(tri_submesh_idx[tri_idx] == cur_submesh_idx) {
                        cur_submesh_n_tris++;
                    }
                }
                Con_Printf("\tDone adding new tris for cur triangle\n");
                Con_Printf("\tcur submesh (%d) n_tris: %d/%d, total assigned: %d/%d\n", cur_submesh_idx, cur_submesh_n_tris, mesh->n_tris, n_assigned_tris, mesh->n_tris);
            }
#endif // IQMDEBUG_LOADIQM_MESHSPLITTING
        }

        // Con_Printf("IQMDEBUG - About to free submesh bones\n");
        free(cur_submesh_bones);
        // Con_Printf("IQMDEBUG - Done freeing submesh bones\n");
    }

    int n_submeshes = cur_submesh_idx + 1;
    mesh->n_submeshes = n_submeshes;
    // Con_Printf("IQMDEBUG - About to malloc submeshes array\n");
    mesh->submeshes = (skeletal_mesh_t *) malloc(sizeof(skeletal_mesh_t) * n_submeshes);
    // Con_Printf("IQMDEBUG - Done mallocing submeshes array\n");


    for(int submesh_idx = 0; submesh_idx < n_submeshes; submesh_idx++) {
#ifdef IQMDEBUG_LOADIQM_MESHSPLITTING
        Con_Printf("Reconstructing submesh %d/%d for mesh\n", submesh_idx+1, n_submeshes);
#endif // IQMDEBUG_LOADIQM_MESHSPLITTING
        // Count the number of triangles that have been assigned to this submesh
        int submesh_tri_count = 0;
        for(uint32_t tri_idx = 0; tri_idx < mesh->n_tris; tri_idx++) {
            if(tri_submesh_idx[tri_idx] == submesh_idx) {
                submesh_tri_count++;
            }
        }

        // Allocate enough memory to fit theoretical max amount of unique vertes this model can reference (given we know its triangle count)
        uint16_t *submesh_mesh_vert_idxs = (uint16_t*) malloc(sizeof(uint16_t) * TRI_VERTS * submesh_tri_count); // Indices into mesh list of vertices
        // Allocate enough memoery to fit 3 vertex indices per triangle
        uint16_t *submesh_tri_verts = (uint16_t*) malloc(sizeof(uint16_t) * TRI_VERTS * submesh_tri_count);
        uint32_t submesh_n_tris = 0;
        uint32_t submesh_n_verts = 0;

        // ----------------------------------------------------------------
        // Build this submesh's list of triangle indices, vertex list, and
        // triangle vertex indices
        // ----------------------------------------------------------------
        for(uint32_t mesh_tri_idx = 0; mesh_tri_idx < mesh->n_tris; mesh_tri_idx++) {
            // Skip triangles that don't belong to this submesh
            if(tri_submesh_idx[mesh_tri_idx] != submesh_idx) {
                continue;
            }

            // Add the triangle to our submesh's list of triangles
            int submesh_tri_idx = submesh_n_tris;
            submesh_n_tris += 1;

            // Add each of the triangle's verts to the submesh list of verts
            // If that vertex is already in the submesh, use that index instead
            for(int tri_vert_idx = 0; tri_vert_idx < TRI_VERTS; tri_vert_idx++) {
                int mesh_vert_idx = mesh->tri_verts[(mesh_tri_idx * TRI_VERTS) + tri_vert_idx];
                // FIXME - This is a pointer into the full model vert indices list... Do we instead want the index into the mesh's vertex list?

                // Check if this vertex is already in the submesh
                int submesh_vert_idx = -1;
                for(uint32_t j = 0; j < submesh_n_verts; j++) {
                    if(submesh_mesh_vert_idxs[j] == mesh_vert_idx) {
                        submesh_vert_idx = j;
                        break;
                    }
                }
                // If we didn't find the vertex in the submesh vertex list, add it
                if(submesh_vert_idx == -1) {
                    submesh_vert_idx = submesh_n_verts;
                    submesh_mesh_vert_idxs[submesh_n_verts] = mesh_vert_idx;
                    submesh_n_verts += 1;
                }

                // Store the submesh vert idx for this triangle
                submesh_tri_verts[(submesh_tri_idx * TRI_VERTS) + tri_vert_idx] = submesh_vert_idx;
            }
        }
        // ----------------------------------------------------------------

        mesh->submeshes[submesh_idx].n_verts = submesh_n_verts;
        mesh->submeshes[submesh_idx].n_tris = submesh_n_tris;
        mesh->submeshes[submesh_idx].vert_rest_positions = (vec3_t*) malloc(sizeof(vec3_t) * submesh_n_verts);
        mesh->submeshes[submesh_idx].vert_uvs = (vec2_t*) malloc(sizeof(vec2_t) * submesh_n_verts);

#ifdef IQM_LOAD_NORMALS
        mesh->submeshes[submesh_idx].vert_rest_normals = (vec3_t*) malloc(sizeof(vec3_t) * submesh_n_verts);
#endif // IQM_LOAD_NORMALS
    
        // mesh->submeshes[submesh_idx].vert_bone_weights = (float*) malloc(sizeof(float) * VERT_BONES * submesh_n_verts);
        // mesh->submeshes[submesh_idx].vert_bone_idxs = (uint8_t*) malloc(sizeof(uint8_t) * VERT_BONES * submesh_n_verts);
        mesh->submeshes[submesh_idx].vert_bone_weights = nullptr;
        mesh->submeshes[submesh_idx].vert_bone_idxs = nullptr;
        mesh->submeshes[submesh_idx].vert16s = nullptr;
        mesh->submeshes[submesh_idx].vert8s = nullptr;
        mesh->submeshes[submesh_idx].vert_skinning_weights = (float*) malloc(sizeof(float) * SUBMESH_BONES * submesh_n_verts);
        mesh->submeshes[submesh_idx].verts_ofs[0] = 0.0f;
        mesh->submeshes[submesh_idx].verts_ofs[1] = 0.0f;
        mesh->submeshes[submesh_idx].verts_ofs[2] = 0.0f;
        mesh->submeshes[submesh_idx].verts_scale[0] = 1.0f;
        mesh->submeshes[submesh_idx].verts_scale[1] = 1.0f;
        mesh->submeshes[submesh_idx].verts_scale[2] = 1.0f;
        mesh->submeshes[submesh_idx].n_submeshes = 0;
        mesh->submeshes[submesh_idx].submeshes = nullptr;
        mesh->submeshes[submesh_idx].geomset = 0;
        mesh->submeshes[submesh_idx].geomid = 0;
        mesh->submeshes[submesh_idx].material_name = nullptr;
        mesh->submeshes[submesh_idx].material_idx = -1;



        for(uint32_t vert_idx = 0; vert_idx < submesh_n_verts; vert_idx++) {
            uint16_t mesh_vert_idx = submesh_mesh_vert_idxs[vert_idx]; 
            mesh->submeshes[submesh_idx].vert_rest_positions[vert_idx][0] = mesh->vert_rest_positions[mesh_vert_idx][0];
            mesh->submeshes[submesh_idx].vert_rest_positions[vert_idx][1] = mesh->vert_rest_positions[mesh_vert_idx][1];
            mesh->submeshes[submesh_idx].vert_rest_positions[vert_idx][2] = mesh->vert_rest_positions[mesh_vert_idx][2];
            mesh->submeshes[submesh_idx].vert_uvs[vert_idx][0] = mesh->vert_uvs[mesh_vert_idx][0];
            mesh->submeshes[submesh_idx].vert_uvs[vert_idx][1] = mesh->vert_uvs[mesh_vert_idx][1];
    
#ifdef IQM_LOAD_NORMALS
            mesh->submeshes[submesh_idx].vert_rest_normals[vert_idx][0] = mesh->vert_rest_normals[mesh_vert_idx][0];
            mesh->submeshes[submesh_idx].vert_rest_normals[vert_idx][1] = mesh->vert_rest_normals[mesh_vert_idx][1];
            mesh->submeshes[submesh_idx].vert_rest_normals[vert_idx][2] = mesh->vert_rest_normals[mesh_vert_idx][2];
#endif // IQM_LOAD_NORMALS

            // for(int k = 0; k < VERT_BONES; k++) {
            //     mesh->submeshes[submesh_idx].vert_bone_weights[(vert_idx * VERT_BONES) + k] = mesh->vert_bone_weights[(mesh_vert_idx * VERT_BONES) + k];
            //     mesh->submeshes[submesh_idx].vert_bone_idxs[(vert_idx * VERT_BONES) + k] = mesh->vert_bone_idxs[(mesh_vert_idx * VERT_BONES) + k];
            // }

            // Initialize all bone skinning weights to 0.0f
            for(int bone_idx = 0; bone_idx < SUBMESH_BONES; bone_idx++) {
                mesh->submeshes[submesh_idx].vert_skinning_weights[vert_idx * SUBMESH_BONES + bone_idx] = 0.0f;
            }
        }


        // FIXME - Why write them if we immediately free them? Are these used?
        // free(mesh->submeshes[submesh_idx].vert_bone_idxs);
        // free(mesh->submeshes[submesh_idx].vert_bone_weights);
        // mesh->submeshes[submesh_idx].vert_bone_idxs = nullptr;
        // mesh->submeshes[submesh_idx].vert_bone_weights = nullptr;

        // ----------------------------------------------------------------
        // Build the submesh's list of bones indices, and the vertex skinning weights
        // ----------------------------------------------------------------
        int n_submesh_bones = 0;
        uint8_t *submesh_bones = (uint8_t*) malloc(sizeof(uint8_t) * SUBMESH_BONES);
        // Initialize to all 0s
        for(int bone_idx = 0; bone_idx < SUBMESH_BONES; bone_idx++) {
            submesh_bones[bone_idx] = 0;
        }

        // For every vertex
        for(uint32_t vert_idx = 0; vert_idx < submesh_n_verts; vert_idx++) {
            int mesh_vert_idx = submesh_mesh_vert_idxs[vert_idx];

            // For every bone that belongs to that vertex
            for(int vert_bone_idx = 0; vert_bone_idx < VERT_BONES; vert_bone_idx++) {
                uint8_t bone_idx = mesh->vert_bone_idxs[mesh_vert_idx * VERT_BONES + vert_bone_idx];
                float bone_weight = mesh->vert_bone_weights[mesh_vert_idx * VERT_BONES + vert_bone_idx];
                if(bone_weight > 0.0f) {
                    int vert_bone_submesh_idx = -1;
                    // Search the submesh's list of bones for this bone
                    for(int submesh_bone_idx = 0; submesh_bone_idx < n_submesh_bones; submesh_bone_idx++) {
                        if(bone_idx == submesh_bones[submesh_bone_idx]) {
                            vert_bone_submesh_idx = submesh_bone_idx;
                            break;
                        }
                    }

                    // If we didn't find the bone, add it to the submesh's list of bones
                    if(vert_bone_submesh_idx == -1) {
                        submesh_bones[n_submesh_bones] = bone_idx;
                        vert_bone_submesh_idx = n_submesh_bones;
                        n_submesh_bones += 1;
                    }

                    // Set the vertex's corresponding weight entry for that bone
                    mesh->submeshes[submesh_idx].vert_skinning_weights[vert_idx * SUBMESH_BONES + vert_bone_submesh_idx] = bone_weight;
                }
            }
        }


        // TEMP HACK DEBUG
        // Set each vertex bone skinning weights to [1.0, 0.0, 0.0, ...]
        // for(uint32_t vert_idx = 0; vert_idx < submesh_n_verts; vert_idx++) {
        //     mesh->submeshes[submesh_idx].vert_skinning_weights[vert_idx * SUBMESH_BONES + 0] = 1.0f;
        //     for(int submesh_bone_idx = 1; submesh_bone_idx < SUBMESH_BONES; submesh_bone_idx++) {
        //         mesh->submeshes[submesh_idx].vert_skinning_weights[vert_idx * SUBMESH_BONES + submesh_bone_idx] = 0.0f;
        //     }
        // }

        // Save the submesh's list of bone indices
        mesh->submeshes[submesh_idx].n_skinning_bones = n_submesh_bones;
        for(int bone_idx = 0; bone_idx < SUBMESH_BONES; bone_idx++) {
            mesh->submeshes[submesh_idx].skinning_bone_idxs[bone_idx] = submesh_bones[bone_idx];
        }
        // ----------------------------------------------------------------


        // ----------------------------------------------------------------
        // Set the triangle vertex indices
        // ----------------------------------------------------------------
        mesh->submeshes[submesh_idx].tri_verts = (uint16_t*) malloc(sizeof(uint16_t) * TRI_VERTS * submesh_n_tris);
        for(uint32_t tri_idx = 0; tri_idx < submesh_n_tris; tri_idx++) {
            mesh->submeshes[submesh_idx].tri_verts[tri_idx * TRI_VERTS + 0] = submesh_tri_verts[tri_idx * TRI_VERTS + 0];
            mesh->submeshes[submesh_idx].tri_verts[tri_idx * TRI_VERTS + 1] = submesh_tri_verts[tri_idx * TRI_VERTS + 1];
            mesh->submeshes[submesh_idx].tri_verts[tri_idx * TRI_VERTS + 2] = submesh_tri_verts[tri_idx * TRI_VERTS + 2];
        }
        // ----------------------------------------------------------------


        // -––-----------––-----------––-----------––-----------––---------
#ifdef IQMDEBUG_LOADIQM_MESHSPLITTING
        Con_Printf("Mesh submesh %d bones (%d): [", submesh_idx, n_submesh_bones);
        for(int j = 0; j < SUBMESH_BONES; j++) {
            Con_Printf("%d, ", submesh_bones[j]);
        }
        Con_Printf("]\n");
#endif // IQMDEBUG_LOADIQM_MESHSPLITTING

        // // TODO - Print all verts for this submesh
        // for(uint32_t vert_idx = 0; vert_idx < submesh_n_verts; vert_idx++) {
        //     int mesh_vert_idx = submesh_mesh_vert_idxs[vert_idx];
        //     Con_Printf("\tvert %d, bones {%d:%.2f, %d:%.2f, %d:%.2f, %d:%.2f} --> [%d:%.2f, %d:%.2f, %d:%.2f, %d:%.2f, %d:%.2f, %d:%.2f, %d:%.2f, %d:%.2f]\n", 
        //         vert_idx, 
        //         skel_model->meshes[i].vert_bone_idxs[mesh_vert_idx * VERT_BONES + 0], skel_model->meshes[i].vert_bone_weights[mesh_vert_idx * VERT_BONES + 0],
        //         skel_model->meshes[i].vert_bone_idxs[mesh_vert_idx * VERT_BONES + 1], skel_model->meshes[i].vert_bone_weights[mesh_vert_idx * VERT_BONES + 1],
        //         skel_model->meshes[i].vert_bone_idxs[mesh_vert_idx * VERT_BONES + 2], skel_model->meshes[i].vert_bone_weights[mesh_vert_idx * VERT_BONES + 2],
        //         skel_model->meshes[i].vert_bone_idxs[mesh_vert_idx * VERT_BONES + 3], skel_model->meshes[i].vert_bone_weights[mesh_vert_idx * VERT_BONES + 3],
        //         submesh_bones[0], skel_model->meshes[i].submeshes[submesh_idx].skinning_verts[vert_idx].bone_weights[0],
        //         submesh_bones[1], skel_model->meshes[i].submeshes[submesh_idx].skinning_verts[vert_idx].bone_weights[1],
        //         submesh_bones[2], skel_model->meshes[i].submeshes[submesh_idx].skinning_verts[vert_idx].bone_weights[2],
        //         submesh_bones[3], skel_model->meshes[i].submeshes[submesh_idx].skinning_verts[vert_idx].bone_weights[3],
        //         submesh_bones[4], skel_model->meshes[i].submeshes[submesh_idx].skinning_verts[vert_idx].bone_weights[4],
        //         submesh_bones[5], skel_model->meshes[i].submeshes[submesh_idx].skinning_verts[vert_idx].bone_weights[5],
        //         submesh_bones[6], skel_model->meshes[i].submeshes[submesh_idx].skinning_verts[vert_idx].bone_weights[6],
        //         submesh_bones[7], skel_model->meshes[i].submeshes[submesh_idx].skinning_verts[vert_idx].bone_weights[7]
        //     );
        // }
        // -––-----------––-----------––-----------––-----------––---------

#ifdef IQMDEBUG_LOADIQM_MESHSPLITTING
        Con_Printf("About to free submesh data structures...\n");
#endif // IQMDEBUG_LOADIQM_MESHSPLITTING
        free(submesh_mesh_vert_idxs);
        free(submesh_tri_verts);
    }

    free(tri_n_bones);
    free(tri_bones);
    free(tri_submesh_idx);
    // --------------------------------------------------------------------
}




//
// Parses an IQM Vertex array and converts all values to floats
//
void iqm_parse_float_array(const void *iqm_data, const iqm_vert_array_t *vert_array, float *out, size_t n_elements, size_t element_len, float *default_value) {
    if(vert_array == nullptr) {
        return;
    }
    iqm_dtype dtype = (iqm_dtype) vert_array->format;
    size_t iqm_values_to_read = (size_t) vert_array->size;
    if(vert_array->ofs == 0) {
        iqm_values_to_read = 0;
        dtype = iqm_dtype::IQM_DTYPE_FLOAT;
    }
    const uint8_t *iqm_array_data = (const uint8_t*) iqm_data + vert_array->ofs;


    // Special cases:
    if(dtype == iqm_dtype::IQM_DTYPE_FLOAT && element_len == iqm_values_to_read) {
        memcpy(out, (const float*) iqm_array_data, sizeof(float) * element_len * n_elements);
        return;
    }
    if(dtype == iqm_dtype::IQM_DTYPE_HALF) {
        iqm_values_to_read = 0;
    }

    // For all other dtype cases, parse each value from IQM:
    for(size_t i = 0; i < n_elements; i++) {
        // Read the first `iqm_values_to_read` values for vector `i`
        for(size_t j = 0; j < element_len && j < iqm_values_to_read; j++) {
            switch(dtype) {
                default:
                    iqm_values_to_read = 0;
                    break;
                case iqm_dtype::IQM_DTYPE_BYTE:
                    out[i * element_len + j] = ((const int8_t*)iqm_array_data)[i * iqm_values_to_read + j] * (1.0f/127);
                    break;
                case iqm_dtype::IQM_DTYPE_UBYTE:
                    out[i * element_len + j] = ((const uint8_t*)iqm_array_data)[i * iqm_values_to_read + j] * (1.0f/255);
                    break;
                case iqm_dtype::IQM_DTYPE_SHORT:
                    out[i * element_len + j] = ((const int16_t*)iqm_array_data)[i * iqm_values_to_read + j] * (1.0f/32767);
                    break;
                case iqm_dtype::IQM_DTYPE_USHORT:
                    out[i * element_len + j] = ((const uint16_t*)iqm_array_data)[i * iqm_values_to_read + j] * (1.0f/65535);
                    break;
                case iqm_dtype::IQM_DTYPE_INT:
                    out[i * element_len + j] = ((const int32_t*)iqm_array_data)[i * iqm_values_to_read + j]  / ((float)0x7fffffff);
                    break;
                case iqm_dtype::IQM_DTYPE_UINT:
                    out[i * element_len + j] = ((const uint32_t*)iqm_array_data)[i * iqm_values_to_read + j] / ((float)0xffffffffu);
                    break;
                case iqm_dtype::IQM_DTYPE_FLOAT:
                    out[i * element_len + j] = ((const float*)iqm_array_data)[i * iqm_values_to_read + j];
                    break;
                case iqm_dtype::IQM_DTYPE_DOUBLE:
                    out[i * element_len + j] = ((const double*)iqm_array_data)[i * iqm_values_to_read + j];
                    break;
            }
        }
        // Pad the remaining `element_len - iqm_values_to_read` values for vector `i`
        for(size_t j = iqm_values_to_read; j < element_len; j++) {
            out[i * element_len + j] = default_value[j];
        }
    }
}


//
// Parses an IQM Vertex array and converts all values to uint8_t
//
void iqm_parse_uint8_array(const void *iqm_data, const iqm_vert_array_t *vert_array, uint8_t *out, size_t n_elements, size_t element_len, uint8_t max_value) {
    if(vert_array == nullptr) {
        return;
    }
    iqm_dtype dtype = (iqm_dtype) vert_array->format;
    size_t iqm_values_to_read = (size_t) vert_array->size;
    if(vert_array->ofs == 0) {
        iqm_values_to_read = 0;
        dtype = iqm_dtype::IQM_DTYPE_UBYTE;
    }
    const uint8_t *iqm_array_data = (const uint8_t*) iqm_data + vert_array->ofs;



    // Special cases:
    if(dtype == iqm_dtype::IQM_DTYPE_FLOAT && element_len == iqm_values_to_read) {
        memcpy(out, (const float*) iqm_array_data, sizeof(float) * element_len * n_elements);
        return;
    }
    if(dtype == iqm_dtype::IQM_DTYPE_HALF) {
        iqm_values_to_read = 0;
    }

    // For all other dtype cases, parse each value from IQM:
    for(size_t i = 0; i < n_elements; i++) {
        // Read the first `iqm_values_to_read` values for vector `i`
        for(size_t j = 0; j < element_len && j < iqm_values_to_read; j++) {

            uint8_t in_val;
            switch(dtype) {
                case iqm_dtype::IQM_DTYPE_FLOAT:    // Skip, these values don't make sense
                case iqm_dtype::IQM_DTYPE_DOUBLE:   // Skip, these values don't make sense
                default:
                    in_val = 0;
                    iqm_values_to_read = 0;
                    break;
                case iqm_dtype::IQM_DTYPE_BYTE:     // Interpret as signed
                case iqm_dtype::IQM_DTYPE_UBYTE:
                    in_val = ((const uint8_t*)iqm_array_data)[i * iqm_values_to_read + j];
                    break;
                case iqm_dtype::IQM_DTYPE_SHORT:    // Interpret as signed
                case iqm_dtype::IQM_DTYPE_USHORT:
                    in_val = (uint8_t) ((const uint16_t*)iqm_array_data)[i * iqm_values_to_read + j];
                    break;
                case iqm_dtype::IQM_DTYPE_INT:      // Interpret as signed
                case iqm_dtype::IQM_DTYPE_UINT:
                    in_val = (uint8_t) ((const uint32_t*)iqm_array_data)[i * iqm_values_to_read + j];
                    break;
            }

            if(in_val >= max_value) {
                // TODO - Mark invalid, return that array had invalid values
                in_val = 0;
            }
            out[i * element_len + j] = in_val;
        }
        // Pad the remaining `element_len - iqm_values_to_read` values for vector `i`
        for(size_t j = iqm_values_to_read; j < element_len; j++) {
            out[i * element_len + j] = 0;
        }
    }
}


static const void *iqm_find_extension(const uint8_t *iqm_data, size_t iqm_data_size, const char *extension_name, size_t *extension_size) {
    const iqm_header_t *iqm_header = (const iqm_header_t*) iqm_data;
    const iqm_extension_t *iqm_extension;

    iqm_extension = (const iqm_extension_t *) (iqm_data + iqm_header->ofs_extensions);

    for(uint16_t i = 0; i < iqm_header->n_extensions; i++) {
        // If past end of file, stop.
        if((const uint8_t *)iqm_extension > (iqm_data + iqm_data_size)) {
            break;
        }
        // If extension name is invalid, stop.
        if(iqm_extension->name > iqm_header->n_text) {
            break;
        }
        // If extension data pointer is past end of file, stop.
        if(iqm_extension->ofs_data + iqm_extension->n_data > iqm_data_size) {
            break;
        }
        // If name matches what we're looking for, return this extension
        const char *cur_extension_name = (const char*) ((iqm_data + iqm_header->ofs_text) + iqm_extension->name);
        if(strcmp(cur_extension_name, extension_name) == 0) {
            *extension_size = iqm_extension->n_data;
            return iqm_data + iqm_extension->ofs_data;
        }
        
        // Advance to next entry in linked list
        iqm_extension = (const iqm_extension_t *) (iqm_data + iqm_extension->ofs_extensions);
    }

    *extension_size = 0;
    return nullptr;
}




//
// Builds and returns a 3x4 transform matrix from the corresponding translation vector, rotation quaternion, and scale vector
//
void Matrix3x4_scale_rotate_translate(mat3x4_t out, const vec3_t scale, const quat_t rotation, const vec3_t translation) {
    // First scale, then rotate, then translate:
    mat3x4_t rotate_translate_mat;
    Matrix3x4_FromOriginQuat(rotate_translate_mat, rotation, translation);
    mat3x4_t scale_mat;
    Matrix3x4_LoadIdentity(scale_mat);
    scale_mat[0][0] = scale[0];
    scale_mat[1][1] = scale[1];
    scale_mat[2][2] = scale[2];
    Matrix3x4_ConcatTransforms(out, rotate_translate_mat, scale_mat);
}

//
// Builds and returns a 3x4 transform matrix from the corresponding translation vector, and rotation quaternion
//
void Matrix3x4_rotate_translate(mat3x4_t out, const quat_t rotation, const vec3_t translation) {
    // First scale, then rotate, then translate:
    mat3x4_t rotate_translate_mat;
    Matrix3x4_FromOriginQuat(out, rotation, translation);
}


const matrix3x4 matrix3x4_zero = {
    { 0, 0, 0, 0},
    { 0, 0, 0, 0},
    { 0, 0, 0, 0},
};
#define Matrix3x4_LoadZero( mat )		Matrix3x4_Copy( mat, matrix3x4_zero )
const matrix3x3 matrix3x3_zero = {
    { 0, 0, 0 },
    { 0, 0, 0 },
    { 0, 0, 0 },
};
const matrix3x3 matrix3x3_identity = {
    { 1, 0, 0 },
    { 0, 1, 0 },
    { 0, 0, 1 },
};
#define Matrix3x3_Copy( out, in )		memcpy( out, in, sizeof( matrix3x3 ))
#define Matrix3x3_LoadZero( mat )		Matrix3x3_Copy( mat, matrix3x3_zero )
#define Matrix3x3_LoadIdentity( mat )		Matrix3x3_Copy( mat, matrix3x3_identity )



//
// Builds and returns a 3x4 transform matrix from the corresponding translation vector and scale vector
//
void Matrix3x4_scale_translate(mat3x4_t out, const vec3_t scale, const vec3_t translation) {
    // First scale, then translate:
    mat3x4_t translation_mat;
    Matrix3x4_LoadIdentity(translation_mat);
    translation_mat[0][3] = translation[0];
    translation_mat[1][3] = translation[1];
    translation_mat[2][3] = translation[2];
    mat3x4_t scale_mat;
    Matrix3x4_LoadIdentity(scale_mat);
    scale_mat[0][0] = scale[0];
    scale_mat[1][1] = scale[1];
    scale_mat[2][2] = scale[2];
    Matrix3x4_ConcatTransforms(out, translation_mat, scale_mat);
}


// 
// Slightly more expensive version of `Matrix3x4_Invert_Simple` that supports non-uniform scaling
// This inversion method assumes the transformation is affine
// 
// https://stackoverflow.com/a/2625420
// https://stackoverflow.com/a/18504573
//
void Matrix3x4_Invert_Affine( matrix3x4 out, const matrix3x4 in )
{
    // ----------------------------------------------------
    // Invert the upper-left 3x3 matrix
    // ----------------------------------------------------
    float det = in[0][0] * (in[1][1] * in[2][2] - in[1][2] * in[2][1]) -
                in[0][1] * (in[1][0] * in[2][2] - in[1][2] * in[2][0]) +
                in[0][2] * (in[1][0] * in[2][1] - in[1][1] * in[2][0]);
    Matrix3x4_LoadZero(out);
    // If determinant is close to 0, return 0 matrix:
    if(fabs(det) < 1e-5) {
        return;
    }
    float inv_det = 1.0f / det;
    out[0][0] = (in[1][1] * in[2][2] - in[2][1] * in[1][2]) * inv_det;
    out[1][0] = (in[2][0] * in[1][2] - in[1][0] * in[2][2]) * inv_det;
    out[2][0] = (in[1][0] * in[2][1] - in[2][0] * in[1][1]) * inv_det;
    out[0][1] = (in[2][1] * in[0][2] - in[0][1] * in[2][2]) * inv_det;
    out[1][1] = (in[0][0] * in[2][2] - in[2][0] * in[0][2]) * inv_det;
    out[2][1] = (in[2][0] * in[0][1] - in[0][0] * in[2][1]) * inv_det;
    out[0][2] = (in[0][1] * in[1][2] - in[1][1] * in[0][2]) * inv_det;
    out[1][2] = (in[1][0] * in[0][2] - in[0][0] * in[1][2]) * inv_det;
    out[2][2] = (in[0][0] * in[1][1] - in[1][0] * in[0][1]) * inv_det;
    // ----------------------------------------------------

    // Invert the rightmost translation column vector
	out[0][3] = -1.0f * (in[0][3] * out[0][0] + in[1][3] * out[0][1] + in[2][3] * out[0][2]);
	out[1][3] = -1.0f * (in[0][3] * out[1][0] + in[1][3] * out[1][1] + in[2][3] * out[1][2]);
	out[2][3] = -1.0f * (in[0][3] * out[2][0] + in[1][3] * out[2][1] + in[2][3] * out[2][2]);
}



// 
// Inverts and transposes the upper-left 3x3 matrix from a 3x4 matrix
// https://stackoverflow.com/a/18504573
// 
void Matrix3x3_Invert_Transposed_Matrix3x4(matrix3x3 out, const matrix3x4 in) {
    float det = in[0][0] * (in[1][1] * in[2][2] - in[1][2] * in[2][1]) -
                in[0][1] * (in[1][0] * in[2][2] - in[1][2] * in[2][0]) +
                in[0][2] * (in[1][0] * in[2][1] - in[1][1] * in[2][0]);
    // If determinant is close to 0, return 0 matrix:
    if(fabs(det) < 1e-5) {
        Matrix3x3_LoadZero(out);
    }
    float inv_det = 1.0f / det;
    // Normal inverse:
    // out[0][0] = (in[1][1] * in[2][2] - in[2][1] * in[1][2]) * inv_det;
    // out[1][0] = (in[2][0] * in[1][2] - in[1][0] * in[2][2]) * inv_det;
    // out[2][0] = (in[1][0] * in[2][1] - in[2][0] * in[1][1]) * inv_det;
    // out[0][1] = (in[2][1] * in[0][2] - in[0][1] * in[2][2]) * inv_det;
    // out[1][1] = (in[0][0] * in[2][2] - in[2][0] * in[0][2]) * inv_det;
    // out[2][1] = (in[2][0] * in[0][1] - in[0][0] * in[2][1]) * inv_det;
    // out[0][2] = (in[0][1] * in[1][2] - in[1][1] * in[0][2]) * inv_det;
    // out[1][2] = (in[1][0] * in[0][2] - in[0][0] * in[1][2]) * inv_det;
    // out[2][2] = (in[0][0] * in[1][1] - in[1][0] * in[0][1]) * inv_det;
    // Transposed inverse:
    out[0][0] = (in[1][1] * in[2][2] - in[2][1] * in[1][2]) * inv_det;
    out[0][1] = (in[2][0] * in[1][2] - in[1][0] * in[2][2]) * inv_det;
    out[0][2] = (in[1][0] * in[2][1] - in[2][0] * in[1][1]) * inv_det;
    out[1][0] = (in[2][1] * in[0][2] - in[0][1] * in[2][2]) * inv_det;
    out[1][1] = (in[0][0] * in[2][2] - in[2][0] * in[0][2]) * inv_det;
    out[1][2] = (in[2][0] * in[0][1] - in[0][0] * in[2][1]) * inv_det;
    out[2][0] = (in[0][1] * in[1][2] - in[1][1] * in[0][2]) * inv_det;
    out[2][1] = (in[1][0] * in[0][2] - in[0][0] * in[1][2]) * inv_det;
    out[2][2] = (in[0][0] * in[1][1] - in[1][0] * in[0][1]) * inv_det;
}


// 
// When vertex positions are quantized as int8 or int16s, vertices are only 
// allowed to span values in the range: [-1,1].
// To work around this, we will scale all vertices for each submesh along each 
// axis to make the vertices fit within this [-1,1] box.
// 
// First, calculate a bounding box that fits all vertices for a submesh
// Then, offset the bounding box so it's centered on [0,0,0]
// Finally, scale the bounding box so it spans [-1,1] on each dimension.
// 
// This method calculates the offset and scale necessary to achieve the above 
// effect.
// 
void calc_quantization_grid_scale_ofs(skeletal_mesh_t *mesh) {
    // ----------------------------------------------------------------------
    // 8-bit and 16-bit vertices are quantized onto a grid
    // Find the most compact grid to quantize the vertices onto
    // ----------------------------------------------------------------------
    // NOTE - This creates a quantization grid per-mesh that wraps the mesh 
    // NOTE   in the rest position. For software skinning, the mesh moves 
    // NOTE   beyond this quantization grid, causing problems.
    // NOTE - Though this could work for hardware skinning setups.
#ifdef IQMDEBUG_LOADIQM_LOADMESH
    Con_Printf("Calculating quantization ofs / scale for mesh...\n");
#endif // IQMDEBUG_LOADIQM_LOADMESH

    // Compute mins / maxs along each axis (x,y,z)
    // Then compute ofs and scale to have all verts on that axis be centered on 
    // the origin and fit within [-1, 1]
    for(int axis=0; axis < 3; axis++) {
        float model_min = mesh->vert_rest_positions[0][axis];
        float model_max = mesh->vert_rest_positions[0][axis];
        for(uint32_t vert_idx = 0; vert_idx < mesh->n_verts; vert_idx++) {
            model_min = std::min(mesh->vert_rest_positions[vert_idx][axis], model_min);
            model_max = std::max(mesh->vert_rest_positions[vert_idx][axis], model_max);
        }
        mesh->verts_ofs[axis] = ((model_max - model_min) / 2.0f) + model_min;
        mesh->verts_scale[axis] = (model_max - model_min) / 2.0f;
    }

#ifdef IQMDEBUG_LOADIQM_LOADMESH
    Con_Printf("Done calculating quantization ofs / scale for mesh\n");
#endif // IQMDEBUG_LOADIQM_LOADMESH
    // ----------------------------------------------------------------------
}

// 
// Using temporary data loaded by `load_iqm_file`, writes the vertex structs that:
//      - Use 8-bit vertex sizes
//      - Remove triangle indices to have a raw list of unindexed vertices
// 
void write_skinning_vert8_structs(skeletal_mesh_t *mesh) {
    int n_unindexed_verts = mesh->n_tris * TRI_VERTS;
    mesh->n_verts = n_unindexed_verts;
    mesh->vert8s = (skel_vertex_i8_t*) malloc(sizeof(skel_vertex_i8_t) * n_unindexed_verts);

    for(uint32_t tri_idx = 0; tri_idx < mesh->n_tris; tri_idx++) {
        for(int tri_vert_idx = 0; tri_vert_idx < TRI_VERTS; tri_vert_idx++) {
            int unindexed_vert_idx = tri_idx * 3 + tri_vert_idx;
            uint16_t indexed_vert_idx = mesh->tri_verts[tri_idx * 3 + tri_vert_idx];

            // int8 vertex with int8 weights
            // Con_Printf("Writing vert8 for vert %d/%d (for triangle: %d/%d)\n", unindexed_vert_idx+1, n_unindexed_verts, tri_idx+1,mesh->n_tris);
            // Con_Printf("\twriting position...\n");
            mesh->vert8s[unindexed_vert_idx].x = float_to_int8(apply_inv_ofs_scale(mesh->vert_rest_positions[indexed_vert_idx][0], mesh->verts_ofs[0], mesh->verts_scale[0]));
            mesh->vert8s[unindexed_vert_idx].y = float_to_int8(apply_inv_ofs_scale(mesh->vert_rest_positions[indexed_vert_idx][1], mesh->verts_ofs[1], mesh->verts_scale[1]));
            mesh->vert8s[unindexed_vert_idx].z = float_to_int8(apply_inv_ofs_scale(mesh->vert_rest_positions[indexed_vert_idx][2], mesh->verts_ofs[2], mesh->verts_scale[2]));
            // Con_Printf("\twriting uv...\n");
            mesh->vert8s[unindexed_vert_idx].u = float_to_int8(mesh->vert_uvs[indexed_vert_idx][0]);
            mesh->vert8s[unindexed_vert_idx].v = float_to_int8(mesh->vert_uvs[indexed_vert_idx][1]);

#ifdef IQM_LOAD_NORMALS
            // Con_Printf("\twriting normal...\n");
            mesh->vert8s[unindexed_vert_idx].nor_x = float_to_int8(mesh->vert_rest_normals[indexed_vert_idx][0]);
            mesh->vert8s[unindexed_vert_idx].nor_y = float_to_int8(mesh->vert_rest_normals[indexed_vert_idx][1]);
            mesh->vert8s[unindexed_vert_idx].nor_z = float_to_int8(mesh->vert_rest_normals[indexed_vert_idx][2]);
#endif // IQM_LOAD_NORMALS

            // Con_Printf("\twriting bone weights...\n");
            for(int bone_idx = 0; bone_idx < SUBMESH_BONES; bone_idx++) {
                mesh->vert8s[unindexed_vert_idx].bone_weights[bone_idx] = float_to_int8(mesh->vert_skinning_weights[indexed_vert_idx * SUBMESH_BONES + bone_idx]);
            }
        }
    }
}

// 
// Using temporary data loaded by `load_iqm_file`, writes the vertex structs that:
//      - Use 16-bit vertex sizes
//      - Remove triangle indices to have a raw list of unindexed vertices
// 
void write_skinning_vert16_structs(skeletal_mesh_t *mesh) {
    int n_unindexed_verts = mesh->n_tris * TRI_VERTS;
    mesh->n_verts = n_unindexed_verts;
    mesh->vert16s = (skel_vertex_i16_t*) malloc(sizeof(skel_vertex_i16_t) * n_unindexed_verts);

    for(uint32_t tri_idx = 0; tri_idx < mesh->n_tris; tri_idx++) {
        for(int tri_vert_idx = 0; tri_vert_idx < TRI_VERTS; tri_vert_idx++) {
            int unindexed_vert_idx = tri_idx * 3 + tri_vert_idx;
            uint16_t indexed_vert_idx = mesh->tri_verts[tri_idx * 3 + tri_vert_idx];

            // int16 vertex with int8 weights
            // Con_Printf("Writing vert16 for vert %d/%d (for triangle: %d/%d)\n", unindexed_vert_idx+1, n_unindexed_verts, tri_idx+1,mesh->n_tris);
            // Con_Printf("\twriting position...\n");
            mesh->vert16s[unindexed_vert_idx].x = float_to_int16(apply_inv_ofs_scale(mesh->vert_rest_positions[indexed_vert_idx][0], mesh->verts_ofs[0], mesh->verts_scale[0]));
            mesh->vert16s[unindexed_vert_idx].y = float_to_int16(apply_inv_ofs_scale(mesh->vert_rest_positions[indexed_vert_idx][1], mesh->verts_ofs[1], mesh->verts_scale[1]));
            mesh->vert16s[unindexed_vert_idx].z = float_to_int16(apply_inv_ofs_scale(mesh->vert_rest_positions[indexed_vert_idx][2], mesh->verts_ofs[2], mesh->verts_scale[2]));
            // Con_Printf("\twriting uv...\n");
            mesh->vert16s[unindexed_vert_idx].u = float_to_int16(mesh->vert_uvs[indexed_vert_idx][0]);
            mesh->vert16s[unindexed_vert_idx].v = float_to_int16(mesh->vert_uvs[indexed_vert_idx][1]);

#ifdef IQM_LOAD_NORMALS
            // Con_Printf("\twriting normal...\n");
            mesh->vert16s[unindexed_vert_idx].nor_x = float_to_int16(mesh->vert_rest_normals[indexed_vert_idx][0]);
            mesh->vert16s[unindexed_vert_idx].nor_y = float_to_int16(mesh->vert_rest_normals[indexed_vert_idx][1]);
            mesh->vert16s[unindexed_vert_idx].nor_z = float_to_int16(mesh->vert_rest_normals[indexed_vert_idx][2]);
#endif // IQM_LOAD_NORMALS

            // Con_Printf("\twriting bone weights...\n");
            for(int bone_idx = 0; bone_idx < SUBMESH_BONES; bone_idx++) {
                mesh->vert16s[unindexed_vert_idx].bone_weights[bone_idx] = float_to_int8(mesh->vert_skinning_weights[indexed_vert_idx * SUBMESH_BONES + bone_idx]);
            }
        }
    }
}


// 
// Util function that frees a pointer and sets it to nullptr
// 
template<typename T>
void free_pointer_and_clear(T* &ptr) {
    if(ptr != nullptr) {
        free(ptr);
        ptr = nullptr;
    }
}


// 
// Returns the amount of memory needed to store a string, including null terminator
// Additionally, pads an extra byte to get 2-byte aligned values 
// (Since relocatable model tightly packs strings together, we must keep things 2-byte aligned)
// 
uint32_t safe_strsize(char *str) {
    if(str == nullptr) {
        return 0;
    }

    uint32_t n_bytes = sizeof(char) * (strlen(str) + 1);

    // FIXME - Make 4-byte aligned
    // if(n_bytes % 2 == 1) {
    //     n_bytes += 1;
    // }
    n_bytes += (4 - (n_bytes % 4)) % 4;
    return n_bytes;
}



//
// Given pointer to raw IQM model bytes buffer, loads the IQM model
// 
skeletal_model_t *load_iqm_file(void *iqm_data) {
    const iqm_header_t *iqm_header = (const iqm_header_t*) iqm_data;

    if(memcmp(iqm_header->magic, IQM_MAGIC, sizeof(iqm_header->magic))) {
        return nullptr; 
    }
    if(iqm_header->version != IQM_VERSION_2) {
        return nullptr;
    }

    // We won't know what the `iqm_data` buffer length is, but this is a useful check
    // if(iqm_header->filesize != file_len) {
    //     return nullptr;
    // }
    size_t file_len = iqm_header->filesize;

    const iqm_vert_array_t *iqm_verts_pos = nullptr;
    const iqm_vert_array_t *iqm_verts_uv = nullptr;
    const iqm_vert_array_t *iqm_verts_nor = nullptr;
    const iqm_vert_array_t *iqm_verts_tan = nullptr;
    const iqm_vert_array_t *iqm_verts_color = nullptr;
    const iqm_vert_array_t *iqm_verts_bone_idxs = nullptr;
    const iqm_vert_array_t *iqm_verts_bone_weights = nullptr;

#ifdef IQMDEBUG_LOADIQM_LOADMESH
    Con_Printf("\tParsing vert attribute arrays\n");
#endif // IQMDEBUG_LOADIQM_LOADMESH

    const iqm_vert_array_t *vert_arrays = (const iqm_vert_array_t*)((uint8_t*) iqm_data + iqm_header->ofs_vert_arrays);
    for(unsigned int i = 0; i < iqm_header->n_vert_arrays; i++) {
        if((iqm_vert_array_type) vert_arrays[i].type == iqm_vert_array_type::IQM_VERT_POS) {
            iqm_verts_pos = &vert_arrays[i];
        }
        else if((iqm_vert_array_type) vert_arrays[i].type == iqm_vert_array_type::IQM_VERT_UV) {
            iqm_verts_uv = &vert_arrays[i];
        }
        else if((iqm_vert_array_type) vert_arrays[i].type == iqm_vert_array_type::IQM_VERT_NOR) {
            iqm_verts_nor = &vert_arrays[i];
        }
        else if((iqm_vert_array_type) vert_arrays[i].type == iqm_vert_array_type::IQM_VERT_TAN) {
            // Only use tangents if float and if each tangent is a 4D vector:
            if((iqm_dtype) vert_arrays[i].format == iqm_dtype::IQM_DTYPE_FLOAT && vert_arrays[i].size == 4) {
                iqm_verts_tan = &vert_arrays[i];
            }
            else {
                Con_Printf("Warning: IQM vertex tangent array (idx: %d, type: %d, fmt: %d, size: %d) is not 4D float array.\n", i, vert_arrays[i].type, vert_arrays[i].format, vert_arrays[i].size);
            }
        }
        else if((iqm_vert_array_type) vert_arrays[i].type == iqm_vert_array_type::IQM_VERT_COLOR) {
            iqm_verts_color = &vert_arrays[i];
        }
        else if((iqm_vert_array_type) vert_arrays[i].type == iqm_vert_array_type::IQM_VERT_BONE_IDXS) {
            iqm_verts_bone_idxs = &vert_arrays[i];
        }
        else if((iqm_vert_array_type) vert_arrays[i].type == iqm_vert_array_type::IQM_VERT_BONE_WEIGHTS) {
            iqm_verts_bone_weights = &vert_arrays[i];
        }
        else {
            Con_Printf("Warning: Unrecognized IQM vertex array type (idx: %d, type: %d, fmt: %d, size: %d)\n", i, vert_arrays[i].type, vert_arrays[i].format, vert_arrays[i].size);
        }
    }

    // ------------------------------------------------------------------------
    // Check for minimum set of required fields for IQM models that have meshes:
    // ------------------------------------------------------------------------
    if(iqm_header->n_meshes > 0) {
        bool loading_failed = false;
        if(iqm_verts_pos == nullptr) {
            Con_Printf("Error: IQM file model has meshes but no vertex positions data array.\n");
            loading_failed = true;
        }
        if(iqm_verts_uv == nullptr) {
            Con_Printf("Error: IQM file model has meshes but no vertex UV data array.\n");
            loading_failed = true;
        }
        // TODO - Technically we could allow for support of IQM models without 
        // TODO   normals / bone weights/ bone indices, but at present all of 
        // TODO   the IQM code assumes we have them. 

#ifdef IQM_LOAD_NORMALS
        if(iqm_verts_nor == nullptr) {
            Con_Printf("Error: IQM file model has meshes but no vertex normal data array.\n");
            loading_failed = true;
        }
#endif // IQM_LOAD_NORMALS

        if(iqm_verts_bone_weights == nullptr) {
            Con_Printf("Error: IQM file model has meshes but no vertex bone weights data array.\n");
            loading_failed = true;
        }
        if(iqm_verts_bone_idxs == nullptr) {
            Con_Printf("Error: IQM file model has meshes but no vertex bone indices data array.\n");
            loading_failed = true;
        }
        if(loading_failed) {
            return nullptr;
        }
    }
    // ------------------------------------------------------------------------

#ifdef IQMDEBUG_LOADIQM_LOADMESH
    Con_Printf("\tCreating skeletal model object\n");
#endif // IQMDEBUG_LOADIQM_LOADMESH
    skeletal_model_t *skel_model = (skeletal_model_t*) malloc(sizeof(skeletal_model_t));
    skel_model->n_meshes = iqm_header->n_meshes;
    skel_model->meshes = (skeletal_mesh_t*) malloc(sizeof(skeletal_mesh_t) * skel_model->n_meshes);

    vec2_t *verts_uv = (vec2_t*) malloc(sizeof(vec2_t) * iqm_header->n_verts);
    vec3_t *verts_pos = (vec3_t*) malloc(sizeof(vec3_t) * iqm_header->n_verts);
    vec3_t *verts_nor = (vec3_t*) malloc(sizeof(vec3_t) * iqm_header->n_verts);


    // ------------------------------------------------------------------------
    // Convert verts_pos / verts_uv datatypes to floats 
    // ------------------------------------------------------------------------
    vec3_t default_vert = {0,0,0};
    vec2_t default_uv = {0,0};
    vec3_t default_nor = {0,0,1.0f};

#ifdef IQMDEBUG_LOADIQM_LOADMESH
    Con_Printf("\tParsing vertex attribute arrays\n");
    Con_Printf("Verts pos: %d\n", iqm_verts_pos);
    Con_Printf("Verts uv: %d\n", iqm_verts_uv);
    Con_Printf("Verts nor: %d\n", iqm_verts_nor);
    Con_Printf("Verts bone weights: %d\n", iqm_verts_bone_weights);
    Con_Printf("Verts bone idxs: %d\n", iqm_verts_bone_idxs);
#endif // IQMDEBUG_LOADIQM_LOADMESH
    iqm_parse_float_array(iqm_data, iqm_verts_pos, (float*) verts_pos, iqm_header->n_verts, 3, (float*) &default_vert);
    iqm_parse_float_array(iqm_data, iqm_verts_uv,  (float*) verts_uv,  iqm_header->n_verts, 2, (float*) &default_uv);
    iqm_parse_float_array(iqm_data, iqm_verts_nor, (float*) verts_nor, iqm_header->n_verts, 3, (float*) &default_nor);

    float *verts_bone_weights = (float*)   malloc(sizeof(float) * 4 * iqm_header->n_verts);
    uint8_t *verts_bone_idxs  = (uint8_t*) malloc(sizeof(uint8_t) * 4 * iqm_header->n_verts);
    float default_bone_weights[] = {0.0f, 0.0f, 0.0f, 0.0f};
    iqm_parse_float_array(iqm_data, iqm_verts_bone_weights, verts_bone_weights,  iqm_header->n_verts, 4, (float*) &default_bone_weights);
    iqm_parse_uint8_array(iqm_data, iqm_verts_bone_idxs,    verts_bone_idxs,     iqm_header->n_verts, 4, (uint8_t) std::min( (int) iqm_header->n_joints, (int) IQM_MAX_BONES));

    // TODO - Parse / set other vertex attributes we care about:
    // - vertex normals <-  iqm_verts_nor
    // - vertex tangents <- iqm_verts_tan
    // - vertex colors <-   iqm_verts_color
    // ------------------------------------------------------------------------

    const iqm_mesh_t *iqm_meshes = (const iqm_mesh_t*)((uint8_t*) iqm_data + iqm_header->ofs_meshes);

    for(uint32_t i = 0; i < iqm_header->n_meshes; i++) {
        skeletal_mesh_t *mesh = &skel_model->meshes[i];
        const char *material_name = (const char*) (((uint8_t*) iqm_data + iqm_header->ofs_text) + iqm_meshes[i].material);
        Con_Printf("Mesh[%d]: \"%s\"\n", i, material_name);

        // mesh->material_name = nullptr;
        mesh->material_name = (char*) malloc(sizeof(char) * (strlen(material_name) + 1));
        strcpy(mesh->material_name, material_name);
        mesh->material_idx = -1; // Indicate that material has not yet been found in list of materials, we'll find it later

        mesh->geomset = 0;
        mesh->geomid = 0;
        uint32_t first_vert = iqm_meshes[i].first_vert_idx;
        uint32_t first_tri = iqm_meshes[i].first_tri;
        // skel_model->meshes[i].first_vert = first_vert;
        uint32_t n_verts = iqm_meshes[i].n_verts;
        mesh->n_verts = n_verts;
        mesh->vert_rest_positions = (vec3_t*) malloc(sizeof(vec3_t) * n_verts);

#ifdef IQM_LOAD_NORMALS
        mesh->vert_rest_normals = (vec3_t*) malloc(sizeof(vec3_t) * n_verts);
#endif // IQM_LOAD_NORMALS

        mesh->vert_uvs = (vec2_t*) malloc(sizeof(vec2_t) * n_verts);
        mesh->vert_bone_weights = (float*) malloc(sizeof(float)  * VERT_BONES * n_verts);  // 4 bone weights per vertex
        mesh->vert_bone_idxs = (uint8_t*) malloc(sizeof(uint8_t) * VERT_BONES * n_verts);  // 4 bone indices per vertex
        mesh->vert_skinning_weights = nullptr;
        mesh->verts_ofs[0] = 0.0f;
        mesh->verts_ofs[1] = 0.0f;
        mesh->verts_ofs[2] = 0.0f;
        mesh->verts_scale[0] = 1.0f;
        mesh->verts_scale[1] = 1.0f;
        mesh->verts_scale[2] = 1.0f;
        mesh->vert16s = nullptr;
        mesh->vert8s = nullptr;
        mesh->n_submeshes = 0;
        mesh->submeshes = nullptr;
        mesh->n_skinning_bones = 0;
        for(int j = 0; j < SUBMESH_BONES; j++) {
            mesh->skinning_bone_idxs[j] = 0;
        }




        for(uint32_t j = 0; j < skel_model->meshes[i].n_verts; j++) {
            // Write static fields:
            mesh->vert_rest_positions[j][0] = verts_pos[first_vert + j][0];
            mesh->vert_rest_positions[j][1] = verts_pos[first_vert + j][1];
            mesh->vert_rest_positions[j][2] = verts_pos[first_vert + j][2];
            mesh->vert_uvs[j][0] = verts_uv[first_vert + j][0];
            mesh->vert_uvs[j][1] = verts_uv[first_vert + j][1];

#ifdef IQM_LOAD_NORMALS
            mesh->vert_rest_normals[j][0] = verts_nor[first_vert + j][0];
            mesh->vert_rest_normals[j][1] = verts_nor[first_vert + j][1];
            mesh->vert_rest_normals[j][2] = verts_nor[first_vert + j][2];
#endif // IQM_LOAD_NORMALS

            for(int k = 0; k < 4; k++) {
                mesh->vert_bone_weights[j * 4 + k] = verts_bone_weights[(first_vert + j) * 4 + k];
                mesh->vert_bone_idxs[j * 4 + k] = verts_bone_idxs[(first_vert + j) * 4 + k];
            }
        }

        // skel_model->meshes[i].n_tris = iqm_meshes[i].n_tris;
        mesh->n_tris = iqm_meshes[i].n_tris;
        mesh->tri_verts = (uint16_t*) malloc(sizeof(uint16_t) * 3 * mesh->n_tris);

        for(uint32_t j = 0; j < skel_model->meshes[i].n_tris; j++) {
            uint16_t vert_a = ((iqm_tri_t*)((uint8_t*) iqm_data + iqm_header->ofs_tris))[first_tri + j].vert_idxs[0] - first_vert;
            uint16_t vert_b = ((iqm_tri_t*)((uint8_t*) iqm_data + iqm_header->ofs_tris))[first_tri + j].vert_idxs[1] - first_vert;
            uint16_t vert_c = ((iqm_tri_t*)((uint8_t*) iqm_data + iqm_header->ofs_tris))[first_tri + j].vert_idxs[2] - first_vert;
            mesh->tri_verts[j*3 + 0] = vert_a;
            mesh->tri_verts[j*3 + 1] = vert_b;
            mesh->tri_verts[j*3 + 2] = vert_c;
        }

        // --
        // Split the mesh into one or more submeshes
        // --
        // IQM meshes can reference an arbitrary number of bones, but PSP can 
        // only reference 8 bones per mesh. So all meshes that reference more 
        // than 8 bones must be split into submeshes that reference 8 bones or
        // fewer each
        // --
        
        // continue; // IQMFIXME -- Okay, skipping everything after this works fine, I've tested it. Let's put it after and verify that we still crash
        submesh_skeletal_mesh(mesh);
        // continue; // IQMFIXME -- This should crash. Checking... No longer crashes... thanks, printing...
#ifdef IQMDEBUG_LOADIQM_LOADMESH
        Con_Printf("Done splitting mesh into submeshes\n");
#endif // IQMDEBUG_LOADIQM_LOADMESH
        // --


        for(int submesh_idx = 0; submesh_idx < mesh->n_submeshes; submesh_idx++) {
#ifdef IQMDEBUG_LOADIQM_LOADMESH
            Con_Printf("Writing vert structs for mesh %d/%d submesh %d/%d\n", i+1, iqm_header->n_meshes, submesh_idx+1, mesh->n_submeshes);
#endif // IQMDEBUG_LOADIQM_LOADMESH
            //  Cast float32 temp vertex data to vert8s / vert16s
            //  Remove triangle indices
            calc_quantization_grid_scale_ofs(&mesh->submeshes[submesh_idx]);
            // TODO - Avoid building both vert8 and vert16 structs?
            write_skinning_vert8_structs(&mesh->submeshes[submesh_idx]);
            write_skinning_vert16_structs(&mesh->submeshes[submesh_idx]);
#ifdef IQMDEBUG_LOADIQM_LOADMESH
            Con_Printf("Done writing vert structs for mesh %d/%d submesh %d/%d\n", i+1, iqm_header->n_meshes, submesh_idx+1, mesh->n_submeshes);
#endif // IQMDEBUG_LOADIQM_LOADMESH
        }


        // Deallocate mesh's loading temporary structs (not used by drawing code)
        for(int submesh_idx = 0; submesh_idx < mesh->n_submeshes; submesh_idx++) {
#ifdef IQMDEBUG_LOADIQM_LOADMESH
            Con_Printf("Clearing temporary memory for mesh %d/%d submesh %d/%d\n", i+1, iqm_header->n_meshes, submesh_idx+1, mesh->n_submeshes);
#endif // IQMDEBUG_LOADIQM_LOADMESH
            skeletal_mesh_t *submesh = &mesh->submeshes[submesh_idx];
            free_pointer_and_clear( submesh->vert_rest_positions);
            free_pointer_and_clear( submesh->vert_uvs);

#ifdef IQM_LOAD_NORMALS
            free_pointer_and_clear( submesh->vert_rest_normals);
#endif // IQM_LOAD_NORMALS

            free_pointer_and_clear( submesh->vert_bone_weights); // NOTE - Unused by submeshes, but included for completeness
            free_pointer_and_clear( submesh->vert_bone_idxs);    // NOTE - Unused by submeshes, but included for completeness
            free_pointer_and_clear( submesh->vert_skinning_weights);
            free_pointer_and_clear( submesh->tri_verts);
#ifdef IQMDEBUG_LOADIQM_LOADMESH
            Con_Printf("Done clearing temporary memory for mesh %d/%d submesh %d/%d\n", i+1, iqm_header->n_meshes, submesh_idx+1, mesh->n_submeshes);
#endif // IQMDEBUG_LOADIQM_LOADMESH
        }
        
#ifdef IQMDEBUG_LOADIQM_LOADMESH
        Con_Printf("Clearing temporary memory for mesh %d/%d\n", i+1, iqm_header->n_meshes);
        Con_Printf("\tClearing vert_rest_positions...\n", i+1, iqm_header->n_meshes);
#endif // IQMDEBUG_LOADIQM_LOADMESH
        free_pointer_and_clear( mesh->vert_rest_positions);


#ifdef IQM_LOAD_NORMALS
#ifdef IQMDEBUG_LOADIQM_LOADMESH
        Con_Printf("\tClearing vert_rest_normals...\n", i+1, iqm_header->n_meshes);
#endif // IQMDEBUG_LOADIQM_LOADMESH
        free_pointer_and_clear( mesh->vert_rest_normals);
#endif // IQM_LOAD_NORMALS


#ifdef IQMDEBUG_LOADIQM_LOADMESH
        Con_Printf("\tClearing vert_uvs...\n", i+1, iqm_header->n_meshes);
#endif // IQMDEBUG_LOADIQM_LOADMESH
        free_pointer_and_clear( mesh->vert_uvs);
#ifdef IQMDEBUG_LOADIQM_LOADMESH
        Con_Printf("\tClearing vert_bone_weights...\n", i+1, iqm_header->n_meshes);
#endif // IQMDEBUG_LOADIQM_LOADMESH
        free_pointer_and_clear( mesh->vert_bone_weights);
#ifdef IQMDEBUG_LOADIQM_LOADMESH
        Con_Printf("\tClearing vert_bone_idxs...\n", i+1, iqm_header->n_meshes);
#endif // IQMDEBUG_LOADIQM_LOADMESH
        free_pointer_and_clear( mesh->vert_bone_idxs);
#ifdef IQMDEBUG_LOADIQM_LOADMESH
        Con_Printf("\tClearing vert_skinning_weights...\n", i+1, iqm_header->n_meshes);
#endif // IQMDEBUG_LOADIQM_LOADMESH
        free_pointer_and_clear( mesh->vert_skinning_weights);   // NOTE - Unused by meshes, but included for completeness
#ifdef IQMDEBUG_LOADIQM_LOADMESH
        Con_Printf("\tClearing tri_verts...\n", i+1, iqm_header->n_meshes);
#endif // IQMDEBUG_LOADIQM_LOADMESH
        free_pointer_and_clear( mesh->tri_verts);
#ifdef IQMDEBUG_LOADIQM_LOADMESH
        Con_Printf("Done clearing temporary memory for mesh %d/%d\n", i+1, iqm_header->n_meshes);
#endif // IQMDEBUG_LOADIQM_LOADMESH
    }

    // Deallocate iqm parsing buffers
    free(verts_pos);
    free(verts_uv);
    free(verts_nor);
    free(verts_bone_weights);
    free(verts_bone_idxs);

    Con_Printf("IQMDEBUG Got to here...\n");
    // return nullptr; // IQMFIXME -- This works fine. Tested, but only because I disabled stuff above... Let's fully get here


    // --------------------------------------------------
    // Parse bones and their rest transforms
    // --------------------------------------------------
#ifdef IQMDEBUG_LOADIQM_BONEINFO
    Con_Printf("Parsing joints...\n");
#endif // IQMDEBUG_LOADIQM_BONEINFO
    skel_model->n_bones = iqm_header->n_joints ? iqm_header->n_joints : iqm_header->n_poses;
    skel_model->bone_name = (char**) malloc(sizeof(char*) * skel_model->n_bones);
    skel_model->bone_parent_idx = (int16_t*) malloc(sizeof(int16_t) * skel_model->n_bones);
    skel_model->bone_rest_pos = (vec3_t*) malloc(sizeof(vec3_t) * skel_model->n_bones);
    skel_model->bone_rest_rot = (quat_t*) malloc(sizeof(quat_t) * skel_model->n_bones);
    skel_model->bone_rest_scale = (vec3_t*) malloc(sizeof(vec3_t) * skel_model->n_bones);

    const iqm_joint_quaternion_t *iqm_joints = (const iqm_joint_quaternion_t*) ((uint8_t*) iqm_data + iqm_header->ofs_joints);
    for(uint32_t i = 0; i < iqm_header->n_joints; i++) {
        const char *joint_name = (const char*) (((uint8_t*) iqm_data + iqm_header->ofs_text) + iqm_joints[i].name);
        skel_model->bone_name[i] = (char*) malloc(sizeof(char) * (strlen(joint_name) + 1));
        strcpy(skel_model->bone_name[i], joint_name);
        skel_model->bone_parent_idx[i] = iqm_joints[i].parent_joint_idx;
        skel_model->bone_rest_pos[i][0] = iqm_joints[i].translate[0];
        skel_model->bone_rest_pos[i][1] = iqm_joints[i].translate[1];
        skel_model->bone_rest_pos[i][2] = iqm_joints[i].translate[2];
        skel_model->bone_rest_rot[i][0] = iqm_joints[i].rotate[0];
        skel_model->bone_rest_rot[i][1] = iqm_joints[i].rotate[1];
        skel_model->bone_rest_rot[i][2] = iqm_joints[i].rotate[2];
        skel_model->bone_rest_rot[i][3] = iqm_joints[i].rotate[3];
        skel_model->bone_rest_scale[i][0] = iqm_joints[i].scale[0];
        skel_model->bone_rest_scale[i][1] = iqm_joints[i].scale[1];
        skel_model->bone_rest_scale[i][2] = iqm_joints[i].scale[2];
        // -- 
    }
#ifdef IQMDEBUG_LOADIQM_BONEINFO
    Con_Printf("\tParsed %d bones.\n", skel_model->n_bones);
#endif // IQMDEBUG_LOADIQM_BONEINFO


    // --------------------------------------------------
    // Set default skeleton hitbox values
    // These will be calculated properly by`calc_skel_model_bounds_for_rest_pose` 
    // and `calc_skel_model_bounds_for_anim`
    // --------------------------------------------------
    VectorSet(skel_model->rest_pose_bone_hitbox_mins, -1, -1, -1);
    VectorSet(skel_model->rest_pose_bone_hitbox_maxs,  1,  1,  1);
    VectorSet(skel_model->anim_bone_hitbox_mins, -1, -1, -1);
    VectorSet(skel_model->anim_bone_hitbox_maxs,  1,  1,  1);
    // --------------------------------------------------

    // --------------------------------------------------
    // Set default hitbox values for all bones
    // (These values may be overridden by JSON file)
    // --------------------------------------------------
    skel_model->bone_hitbox_enabled = (bool*) malloc(sizeof(bool) * skel_model->n_bones);
    skel_model->bone_hitbox_ofs = (vec3_t*) malloc(sizeof(vec3_t) * skel_model->n_bones);
    skel_model->bone_hitbox_scale = (vec3_t*) malloc(sizeof(vec3_t) * skel_model->n_bones);
    skel_model->bone_hitbox_tag = (int*) malloc(sizeof(int) * skel_model->n_bones);
    for(uint32_t i = 0; i < skel_model->n_bones; i++) {
        skel_model->bone_hitbox_enabled[i] = false;
        skel_model->bone_hitbox_ofs[i][0] = 0.0f;
        skel_model->bone_hitbox_ofs[i][1] = 0.0f;
        skel_model->bone_hitbox_ofs[i][2] = 0.0f;
        skel_model->bone_hitbox_scale[i][0] = 1.0f;
        skel_model->bone_hitbox_scale[i][1] = 1.0f;
        skel_model->bone_hitbox_scale[i][2] = 1.0f;
        skel_model->bone_hitbox_tag[i] = 0;
    }


#ifdef IQMDEBUG_LOADIQM_BONEINFO
    for(uint32_t i = 0; i < skel_model->n_bones; i++) {
        Con_Printf("Parsed bone: %i, \"%s\" (parent bone: %d)\n", i, skel_model->bone_name[i], skel_model->bone_parent_idx[i]);
        Con_Printf("\tPos: (%.2f, %.2f, %.2f)\n", skel_model->bone_rest_pos[i][0], skel_model->bone_rest_pos[i][1], skel_model->bone_rest_pos[i][2]);
        Con_Printf("\tRot: (%.2f, %.2f, %.2f, %.2f)\n", skel_model->bone_rest_rot[i][0], skel_model->bone_rest_rot[i][1], skel_model->bone_rest_rot[i][2], skel_model->bone_rest_rot[i][3]);
        Con_Printf("\tScale: (%.2f, %.2f, %.2f)\n", skel_model->bone_rest_scale[i][0], skel_model->bone_rest_scale[i][1], skel_model->bone_rest_scale[i][2]);
        Con_Printf("\tDefault hitbox enabled: %d\n", skel_model->bone_hitbox_enabled[i]);
        Con_Printf("\tDefault hitbox ofs: (%.2f, %.2f, %.2f)\n", skel_model->bone_hitbox_ofs[i][0], skel_model->bone_hitbox_ofs[i][1], skel_model->bone_hitbox_ofs[i][2]);
        Con_Printf("\tDefault hitbox scale: (%.2f, %.2f, %.2f)\n", skel_model->bone_hitbox_scale[i][0], skel_model->bone_hitbox_scale[i][1], skel_model->bone_hitbox_scale[i][2]);
        Con_Printf("\tDefault hitbox tag: %d\n", skel_model->bone_hitbox_tag[i]);
    }
#endif // IQMDEBUG_LOADIQM_BONEINFO


    for(uint32_t i = 0; i < skel_model->n_bones; i++) {
        // i-th bone's parent index must be less than i
        if((int) i <= skel_model->bone_parent_idx[i]) {
            Con_Printf("Error: IQM file bones are sorted incorrectly. Bone %d is located before its parent bone %d.\n", i, skel_model->bone_parent_idx[i]);
            // FIXME - Deallocate all allocated memory
            return nullptr;
        }
    }


    // Check if we have a bone named `fps_cam`
    skel_model->fps_cam_bone_idx = -1;
    for(uint32_t i = 0; i < skel_model->n_bones; i++) {
        if(!strcmp(skel_model->bone_name[i], "fps_cam")) {
            skel_model->fps_cam_bone_idx = i;
            break;
        }
    }
    // --------------------------------------------------




    // --------------------------------------------------
    // Calculate static bone transforms that will be reused often
    // --------------------------------------------------
    // Build the transform that takes us from model-space to each bone's bone-space for the rest pose.
    // This is static, so compute once and cache

    // First build the bone-space --> model-space transform for each bone (We will invert it later)
    skel_model->bone_rest_transforms = (mat3x4_t*) malloc(sizeof(mat3x4_t) * skel_model->n_bones);
    skel_model->inv_bone_rest_transforms = (mat3x4_t*) malloc(sizeof(mat3x4_t) * skel_model->n_bones);
    for(uint32_t i = 0; i < skel_model->n_bones; i++) {
        // Rest bone-space transform (relative to parent)
        Matrix3x4_scale_rotate_translate(
            skel_model->bone_rest_transforms[i],
            skel_model->bone_rest_scale[i],
            skel_model->bone_rest_rot[i],
            skel_model->bone_rest_pos[i]
        );
        // If we have a parent, concat parent transform to get model-space transform
        int parent_bone_idx = skel_model->bone_parent_idx[i];
        if(parent_bone_idx >= 0) {
            mat3x4_t temp;
            Matrix3x4_ConcatTransforms( temp, skel_model->bone_rest_transforms[parent_bone_idx], skel_model->bone_rest_transforms[i]);
            Matrix3x4_Copy(skel_model->bone_rest_transforms[i], temp);
        }
        // If we don't have a parent, the bone-space transform _is_ the model-space transform
    }
    // Next, invert the transforms to get the model-space --> bone-space transform
    // NOTE - `Matrix3x4_Invert_Simple` assumes uniform scaling
    // NOTE - If we want non-uniform scaling, use `Matrix3x4_Invert_Affine`
    for(uint32_t i = 0; i < skel_model->n_bones; i++) {
        Matrix3x4_Invert_Simple( skel_model->inv_bone_rest_transforms[i], skel_model->bone_rest_transforms[i]);
    }
    // --------------------------------------------------


    // --------------------------------------------------
    // Parse all frames (poses)
    // --------------------------------------------------
    skel_model->n_frames = iqm_header->n_frames;
#ifdef IQMDEBUG_LOADIQM_ANIMINFO
    Con_Printf("\tBones: %d\n",skel_model->n_bones);
    Con_Printf("\tFrames: %d\n",iqm_header->n_frames);
    Con_Printf("\tPoses: %d\n",iqm_header->n_poses);
#endif // IQMDEBUG_LOADIQM_ANIMINFO
    skel_model->frames_move_dist = (float *) malloc(sizeof(float) * skel_model->n_frames);
    skel_model->frames_bone_pos = (vec3_t *) malloc(sizeof(vec3_t) * skel_model->n_bones * skel_model->n_frames);
    skel_model->frames_bone_rot = (quat_t *) malloc(sizeof(quat_t) * skel_model->n_bones * skel_model->n_frames);
    skel_model->frames_bone_scale = (vec3_t *) malloc(sizeof(vec3_t) * skel_model->n_bones * skel_model->n_frames);



    const uint16_t *frames_data = (const uint16_t*)((uint8_t*) iqm_data + iqm_header->ofs_frames);
    int frames_data_ofs = 0;
    const iqm_pose_quaternion_t *iqm_poses = (const iqm_pose_quaternion_t*) ((uint8_t*) iqm_data + iqm_header->ofs_poses);

    // Iterate over actual frames in IQM file:
    for(uint32_t i = 0; i < iqm_header->n_frames; i++) {

        // Initialize frame movement distance to 0 for all frames
        skel_model->frames_move_dist[i] = 0.0f;

        // Iterate over pose (a pose is a bone orientation, one pose per bone)
        for(uint32_t j = 0; j < iqm_header->n_poses; j++) {
            // Read data for all 10 channels
            float pose_data[10] = {0};
            for(uint32_t k = 0; k < 10; k++) {
                pose_data[k] = iqm_poses[j].channel_ofs[k];
                if(iqm_poses[j].mask & (1 << k)) {
                    pose_data[k] += frames_data[frames_data_ofs++] * iqm_poses[j].channel_scale[k];
                }
            }
            int frame_bone_idx = i * skel_model->n_bones + j;
            skel_model->frames_bone_pos[frame_bone_idx][0] = pose_data[0];
            skel_model->frames_bone_pos[frame_bone_idx][1] = pose_data[1];
            skel_model->frames_bone_pos[frame_bone_idx][2] = pose_data[2];
            skel_model->frames_bone_rot[frame_bone_idx][0] = pose_data[3];
            skel_model->frames_bone_rot[frame_bone_idx][1] = pose_data[4];
            skel_model->frames_bone_rot[frame_bone_idx][2] = pose_data[5];
            skel_model->frames_bone_rot[frame_bone_idx][3] = pose_data[6];
            skel_model->frames_bone_scale[frame_bone_idx][0] = pose_data[7];
            skel_model->frames_bone_scale[frame_bone_idx][1] = pose_data[8];
            skel_model->frames_bone_scale[frame_bone_idx][2] = pose_data[9];
        }
    }
    // --------------------------------------------------

    // --------------------------------------------------
    // Parse animations (framegroups)
    // --------------------------------------------------
    skel_model->n_framegroups = iqm_header->n_anims;
    skel_model->framegroup_name = (char**) malloc(sizeof(char*) * skel_model->n_framegroups);
    skel_model->framegroup_start_frame = (uint32_t*) malloc(sizeof(uint32_t) * skel_model->n_framegroups);
    skel_model->framegroup_n_frames = (uint32_t*) malloc(sizeof(uint32_t) * skel_model->n_framegroups);
    skel_model->framegroup_fps = (float*) malloc(sizeof(float) * skel_model->n_framegroups);
    skel_model->framegroup_loop = (bool*) malloc(sizeof(bool) * skel_model->n_framegroups);

    if(iqm_header->n_anims > 0) {
        const iqm_anim_t *iqm_framegroups = (const iqm_anim_t*)((uint8_t*) iqm_data + iqm_header->ofs_anims);
        for(uint32_t i = 0; i < iqm_header->n_anims; i++) {
            const char* framegroup_name = (const char*) ((uint8_t*) iqm_data + iqm_header->ofs_text + iqm_framegroups[i].name);
            skel_model->framegroup_name[i] = (char*) malloc(sizeof(char) * (strlen(framegroup_name) + 1));
            strcpy(skel_model->framegroup_name[i], framegroup_name);
            skel_model->framegroup_start_frame[i] = iqm_framegroups[i].first_frame;
            skel_model->framegroup_n_frames[i] = iqm_framegroups[i].n_frames;
            skel_model->framegroup_fps[i] = iqm_framegroups[i].framerate;
            // skel_model->framegroup_fps[i] = 20.0f;
            skel_model->framegroup_loop[i] = iqm_framegroups[i].flags & (uint32_t) iqm_anim_flag::IQM_ANIM_FLAG_LOOP;
            // FOR DEBUG ONLY: Force-loop all animations:
            skel_model->framegroup_loop[i] = true;
        }
    }
#ifdef IQMDEBUG_LOADIQM_ANIMINFO
    Con_Printf("\tParsed %d framegroups.\n", skel_model->n_framegroups);
#endif // IQMDEBUG_LOADIQM_ANIMINFO
    // --------------------------------------------------


    // --------------------------------------------------
    // Initialize material default values
    // --------------------------------------------------
    skel_model->n_materials = 0;
    skel_model->material_names = nullptr;
    skel_model->material_n_skins = nullptr;
    skel_model->materials = nullptr;
    // --------------------------------------------------



    // --------------------------------------------------
    // Parse IQM per-frame mins/maxs to compute overall mins/maxs
    // --------------------------------------------------
    const iqm_bounds_t *iqm_frame_bounds = (const iqm_bounds_t *)((uint8_t*) iqm_data + iqm_header->ofs_bounds);
    if(iqm_header->ofs_bounds != 0) {
        // Compute overall model mins / maxes by finding the most extreme points for all frames:
        for(uint16_t i = 0; i < iqm_header->n_frames; i++) {
            for(int j = 0; j < 3; j++) {
                skel_model->mins[0] = fmin(skel_model->mins[0], iqm_frame_bounds[i].mins[0]);
                skel_model->maxs[0] = fmax(skel_model->maxs[0], iqm_frame_bounds[i].maxs[0]);
            }
        }
    }
    // --------------------------------------------------

    // --------------------------------------------------
    // Parse FTE_MESH IQM extension
    // --------------------------------------------------
    size_t iqm_fte_ext_mesh_size;
    const iqm_ext_fte_mesh_t *iqm_fte_ext_mesh = (const iqm_ext_fte_mesh_t *) iqm_find_extension((uint8_t*) iqm_data, file_len, "FTE_MESH", &iqm_fte_ext_mesh_size);

    if(iqm_fte_ext_mesh != nullptr) {
        for(uint16_t i = 0; i < iqm_header->n_meshes; i++) {
            // TODO - How / where do I want to stash these?
            // Con_Printf("IQM FTE Extension \"FTE_MESH \" mesh %d\n", i);
            // Con_Printf("\tcontents %d\n", iqm_fte_ext_mesh[i].contents);
            // Con_Printf("\tsurfaceflags %d\n", iqm_fte_ext_mesh[i].surfaceflags);
            // Con_Printf("\tsurfaceid %d\n", iqm_fte_ext_mesh[i].surfaceid);
            // Con_Printf("\tgeomset %d\n", iqm_fte_ext_mesh[i].geomset);
            // Con_Printf("\tgeomid %d\n", iqm_fte_ext_mesh[i].geomid);
            // Con_Printf("\tmin_dist %d\n", iqm_fte_ext_mesh[i].min_dist); // LOD min distance
            // Con_Printf("\tmax_dist %d\n", iqm_fte_ext_mesh[i].max_dist); // LOD max distance
            skel_model->meshes[i].geomset = iqm_fte_ext_mesh[i].geomset;
            skel_model->meshes[i].geomid = iqm_fte_ext_mesh[i].geomid;
        }
    }
    // --------------------------------------------------


    // --------------------------------------------------
    // Parse FTE_EVENT IQM extension
    // --------------------------------------------------
    Con_Printf("About to parse IQM FTE events\n");
    size_t iqm_fte_ext_event_size;
    const iqm_ext_fte_event_t *iqm_fte_ext_events = (const iqm_ext_fte_event_t*) iqm_find_extension((uint8_t*)iqm_data, file_len, "FTE_EVENT", &iqm_fte_ext_event_size);
    uint16_t iqm_fte_ext_event_n_events = iqm_fte_ext_event_size / sizeof(iqm_ext_fte_event_t);
    Con_Printf("FTE_EVENTS parsed size %d\n", iqm_fte_ext_event_size);
    Con_Printf("num FTE_EVENTS %d\n", iqm_fte_ext_event_n_events);
    Con_Printf("FTE event struct %d\n", sizeof(iqm_ext_fte_event_t));

    skel_model->framegroup_n_events = nullptr;
    skel_model->framegroup_event_time = nullptr;
    skel_model->framegroup_event_code = nullptr;
    skel_model->framegroup_event_data_str = nullptr;

    if(iqm_fte_ext_events != nullptr) {
        // Count the number of events for each framegroup:
        skel_model->framegroup_n_events = (uint16_t*) malloc(sizeof(uint16_t) * skel_model->n_framegroups);
        for(uint16_t i = 0; i < skel_model->n_framegroups; i++) {
            skel_model->framegroup_n_events[i] = 0;
        }
        for(uint16_t i = 0; i < iqm_fte_ext_event_n_events; i++) {
            int framegroup_idx = iqm_fte_ext_events[i].anim;
            uint32_t event_code = iqm_fte_ext_events[i].event_code;

            // Movement anim events will be stored separately, skip them here
            if(event_code == IQM_ANIM_EVENT_CODE_MOVE_DIST) {
                continue;
            }
            skel_model->framegroup_n_events[framegroup_idx]++;
        }

        // Allocate memory for all framegroup arrays
        skel_model->framegroup_event_time = (float**) malloc(sizeof(float*) * skel_model->n_framegroups);
        skel_model->framegroup_event_code = (uint32_t**) malloc(sizeof(uint32_t*) * skel_model->n_framegroups);
        skel_model->framegroup_event_data_str = (char***) malloc(sizeof(char**) * skel_model->n_framegroups);

        // Allocate enough memory in each framegroup array to hold events in a list:
        for(uint16_t i = 0; i < skel_model->n_framegroups; i++) {
            if(skel_model->framegroup_n_events[i] == 0) {
                skel_model->framegroup_event_time[i] = nullptr;
                skel_model->framegroup_event_code[i] = nullptr;
                skel_model->framegroup_event_data_str[i] = nullptr;
            }
            else {
                skel_model->framegroup_event_time[i] = (float*) malloc(sizeof(float) * skel_model->framegroup_n_events[i]);
                skel_model->framegroup_event_code[i] = (uint32_t*) malloc(sizeof(uint32_t) * skel_model->framegroup_n_events[i]);
                skel_model->framegroup_event_data_str[i] = (char**) malloc(sizeof(char*) * skel_model->framegroup_n_events[i]);
                // Set each char array pointer to nullptr, we'll use this to identify empty indices
                for(int j = 0; j < skel_model->framegroup_n_events[i]; j++) {
                    skel_model->framegroup_event_data_str[i][j] = nullptr;
                }
            }
        }

        for(uint16_t i = 0; i < iqm_fte_ext_event_n_events; i++) {
            const char *iqm_event_data_str = (const char*) ((uint8_t*) iqm_data + iqm_header->ofs_text + iqm_fte_ext_events[i].event_data_str);

            int event_framegroup_idx = iqm_fte_ext_events[i].anim;
            float event_time = iqm_fte_ext_events[i].timestamp;
            uint32_t event_code = iqm_fte_ext_events[i].event_code;

            // Movement anim events will be stored separately, skip them here
            if(event_code == IQM_ANIM_EVENT_CODE_MOVE_DIST) {
                continue;
            }

            // Allocate data for this string and copy the char array
            char *event_data_str = (char*) malloc(sizeof(char) * (strlen(iqm_event_data_str) + 1));
            strcpy(event_data_str, iqm_event_data_str);

            // If invalid framegroup idx, skip the event
            if(event_framegroup_idx < 0 || event_framegroup_idx >= skel_model->n_framegroups) {
                Con_Printf("WARNING: Unable to load IQM event. Framegroup idx %d invalid for a model with %d framegroups.\n", event_framegroup_idx, skel_model->n_framegroups);
                continue;
            }

            // Find the correct index to insert the event into in this framegroup's event list
            int event_idx = 0;
            for(; event_idx < skel_model->framegroup_n_events[event_framegroup_idx]; event_idx++) {
                // If this index is empty, insert here
                if(skel_model->framegroup_event_data_str[event_framegroup_idx][event_idx] == nullptr) {
                    break;
                }
                // If the event at this index has an event time > the current event, insert here
                else if(skel_model->framegroup_event_time[event_framegroup_idx][event_idx] > event_time) {
                    break;
                }
            }
            // Con_Printf("Event %d inserting at framegroup %d index %d\n", i, event_framegroup_idx, event_idx);

            // If event_idx exceeds the length of the array, warn and skip (This should never happen)
            if(event_idx >= skel_model->framegroup_n_events[event_framegroup_idx]) {
                Con_Printf("WARNING: Unable to load IQM event for time %f for framegroup %d.\n", i, event_framegroup_idx);
                continue;
            }

            // Shift up the events in the array.
            // Starting from the end of the array, move up all events after the insertion `event_idx`
            for(int j = skel_model->framegroup_n_events[event_framegroup_idx] - 2; j >= event_idx; j--) {
                if(skel_model->framegroup_event_data_str[event_framegroup_idx][j] == nullptr) {
                    continue;
                }
                // Con_Printf("\tframegroup %d moving event at idx %d to idx %d\n", event_framegroup_idx, j, j+1);
                skel_model->framegroup_event_time[event_framegroup_idx][j+1] = skel_model->framegroup_event_time[event_framegroup_idx][j];
                skel_model->framegroup_event_data_str[event_framegroup_idx][j+1] = skel_model->framegroup_event_data_str[event_framegroup_idx][j];
                skel_model->framegroup_event_code[event_framegroup_idx][j+1] = skel_model->framegroup_event_code[event_framegroup_idx][j];
            }

            // Insert the new event at the chosen index:
            skel_model->framegroup_event_time[event_framegroup_idx][event_idx] = event_time;
            skel_model->framegroup_event_data_str[event_framegroup_idx][event_idx] = event_data_str;
            skel_model->framegroup_event_code[event_framegroup_idx][event_idx] = event_code;
        }
    }
    // --------------------------------------------------




    // --------------------------------------------------
    // Parse movement speed IQM Animation events for each animation
    // --------------------------------------------------

    if(iqm_fte_ext_events != nullptr) {
        for(uint16_t i = 0; i < iqm_fte_ext_event_n_events; i++) {
            const char *iqm_event_data_str = (const char*) ((uint8_t*) iqm_data + iqm_header->ofs_text + iqm_fte_ext_events[i].event_data_str);
            int event_framegroup_idx = iqm_fte_ext_events[i].anim;
            float event_time = iqm_fte_ext_events[i].timestamp;
            uint32_t event_code = iqm_fte_ext_events[i].event_code;

            // Skip non-movement-distance anim events
            if(event_code != IQM_ANIM_EVENT_CODE_MOVE_DIST) {
                continue;
            }

            // If walk distance is 0 or failed to parse, skip
            float event_walk_dist = atof(iqm_event_data_str);
            if(event_walk_dist == 0.0f) {
                continue;
            }

            if(event_framegroup_idx < 0 || event_framegroup_idx >= skel_model->n_framegroups) {
                continue;
            }

            int framegroup_n_frames = skel_model->framegroup_n_frames[event_framegroup_idx];
            float framegroup_duration = framegroup_n_frames / skel_model->framegroup_fps[event_framegroup_idx];

            // Calculate the nearest frame index to deposit this movement distance
            int event_framegroup_frame_idx = (int) (event_time * skel_model->framegroup_fps[event_framegroup_idx]) % framegroup_n_frames;

            Con_Printf("Walkdist event %d, data: %s\n", i, iqm_event_data_str);

            Con_Printf("Walkdist event framegroup %d deposit dist %f at time %f (frames: %d, fps: %f, calculated frame: %d)\n", 
                event_framegroup_idx, 
                event_walk_dist,
                event_time,
                framegroup_n_frames,
                skel_model->framegroup_fps[event_framegroup_idx],
                event_framegroup_frame_idx 
            );

            skel_model->frames_move_dist[event_framegroup_frame_idx] += event_walk_dist;
        }
    }

    for(int framegroup_idx = 0; framegroup_idx < skel_model->n_framegroups; framegroup_idx++) {
        int start_frame_idx = skel_model->framegroup_start_frame[framegroup_idx];
        int end_frame_idx = start_frame_idx + skel_model->framegroup_n_frames[framegroup_idx];
        for(int frame_idx = start_frame_idx; frame_idx < end_frame_idx; frame_idx++) {
            Con_Printf("Walkdist framegroup %d frame %d dist: %f\n", framegroup_idx, frame_idx, skel_model->frames_move_dist[frame_idx]);
        }
    }

    // Calculate inclusive cumulative sum for walk distances
    for(int framegroup_idx = 0; framegroup_idx < skel_model->n_framegroups; framegroup_idx++) {
        int start_frame_idx = skel_model->framegroup_start_frame[framegroup_idx];
        int end_frame_idx = start_frame_idx + skel_model->framegroup_n_frames[framegroup_idx];

        for(int frame_idx = start_frame_idx + 1; frame_idx < end_frame_idx; frame_idx++) {
            skel_model->frames_move_dist[frame_idx] += skel_model->frames_move_dist[frame_idx - 1];

            Con_Printf("Walkdist framegroup %d frame %d cumsumdist: %f\n", framegroup_idx, frame_idx, skel_model->frames_move_dist[frame_idx]);
        }
    }

    
    
    // --------------------------------------------------





    // // --------------------------------------------------
    // // Parse FTE_SKIN IQM extension
    // // --------------------------------------------------
    // // size_t iqm_fte_ext_skin_size;
    // // const uint32_t *iqm_fte_ext_skin_n_skins = (const uint32_t*) iqm_find_extension(iqm_data, file_len, "FTE_SKINS", &iqm_fte_ext_skin_size);

    // // // const iqm_ext_fte_skin_t *iqm_fte_ext_skins
    // // if(iqm_fte_ext_skin_n_skins != nullptr) {

    // //     if(iqm_fte_ext_skin_size >= (sizeof(uint32_t) * 2 * iqm_header->n_meshes)) {
    // //         const uint32_t n_skin_skinframes = iqm_fte_ext_skin_n_skins[0];
    // //         const uint32_t n_skin_meshskins = iqm_fte_ext_skin_n_skins[1];
    // //         size_t expected_skin_size = sizeof(iqm_ext_fte_skin_t) + sizeof(uint32_t) * iqm_header->n_meshes + sizeof(iqm_ext_fte_skin_t) * n_skin_skinframes + sizeof(iqm_ext_fte_skin_meshskin_t) * n_skin_meshskins;
    // //         if(iqm_fte_ext_skin_size == expected_skin_size) {
    // //             const iqm_ext_fte_skin_skinframe_t *skinframes = (const iqm_ext_fte_skin_skinframe_t *) (iqm_fte_ext_skin_n_skins + 2 + iqm_header->n_meshes);
    // //             const iqm_ext_fte_skin_meshskin_t *meshskins = (const iqm_ext_fte_skin_meshskin_t *) (skinframes + n_skin_skinframes);

    // //             // TODO - Print these? Not sure if we'll even want them... or if we should support them?
    // //         }
    // //     }
    // // }
    // // --------------------------------------------------


    // ------------------------------------------------------------------------
    // IQM File requirements error checking:
    // ------------------------------------------------------------------------
    //
    // Currently we allow for three types of IQM model files:
    //  1. IQM model with: meshes, skeleton, animation framegroups
    //  2. IQM model with: meshes, skeleton
    //  3. IQM model with: skeleton, animation framegroups
    // 
    //  IQM models with only meshes, only skeletons, or only framegroups 
    //  are currently not supported.
    // 
    bool has_meshes = (skel_model->n_meshes > 0);
    bool has_skeleton = (skel_model->n_bones > 0);
    bool has_animations = (skel_model->n_frames > 0) && (skel_model->n_framegroups > 0);
    bool loading_failed = false;
    if(!has_skeleton) {
        Con_Printf("Error: IQM file model does not have skeleton data.\n");
        loading_failed = true;
    }
    if(!has_meshes && !has_animations) {
        Con_Printf("Error: IQM file model has neither mesh data nor animation data. At least one of these is required.\n");
        loading_failed = true;
    }
    if(loading_failed) {
        free_skeletal_model(skel_model);
        return nullptr;
    }
    // ------------------------------------------------------------------------

    return skel_model;
}


// 
// Returns the total number of bytes required to store a skeletal_model_t
// including all data pointed to via pointers 
// 
uint32_t count_skel_model_n_bytes(skeletal_model_t *skel_model) {
    uint32_t skel_model_n_bytes = 0;
    skel_model_n_bytes += sizeof(skeletal_model_t);
    skel_model_n_bytes += sizeof(skeletal_mesh_t) * skel_model->n_meshes;
    for(int i = 0; i < skel_model->n_meshes; i++) {
        skeletal_mesh_t *mesh = &skel_model->meshes[i];
        skel_model_n_bytes += safe_strsize(mesh->material_name);
        skel_model_n_bytes += sizeof(skeletal_mesh_t) * mesh->n_submeshes;
        for(int j = 0; j < mesh->n_submeshes; j++) {
            skeletal_mesh_t *submesh = &mesh->submeshes[j];
            if(submesh->vert8s != nullptr) {
                skel_model_n_bytes += sizeof(skel_vertex_i8_t) * submesh->n_verts;
            }
            if(submesh->vert16s != nullptr) {
                skel_model_n_bytes += sizeof(skel_vertex_i16_t) * submesh->n_verts;
            }
        }
    }
    // -- bones --
    skel_model_n_bytes += sizeof(char*) * skel_model->n_bones;
    skel_model_n_bytes += sizeof(int16_t) * skel_model->n_bones;
    skel_model_n_bytes += sizeof(vec3_t) * skel_model->n_bones;
    skel_model_n_bytes += sizeof(quat_t) * skel_model->n_bones;
    skel_model_n_bytes += sizeof(vec3_t) * skel_model->n_bones;
    for(int i = 0; i < skel_model->n_bones; i++) {
        skel_model_n_bytes += safe_strsize(skel_model->bone_name[i]);
    }
    // -- bone hitbox info --
    skel_model_n_bytes += sizeof(bool) * skel_model->n_bones;
    skel_model_n_bytes += sizeof(vec3_t) * skel_model->n_bones;
    skel_model_n_bytes += sizeof(vec3_t) * skel_model->n_bones;
    skel_model_n_bytes += sizeof(int) * skel_model->n_bones;
    // -- cached bone rest transforms --
    skel_model_n_bytes += sizeof(mat3x4_t) * skel_model->n_bones;
    skel_model_n_bytes += sizeof(mat3x4_t) * skel_model->n_bones;
    // -- animation frames -- 
    skel_model_n_bytes += sizeof(vec3_t) * skel_model->n_bones * skel_model->n_frames;
    skel_model_n_bytes += sizeof(quat_t) * skel_model->n_bones * skel_model->n_frames;
    skel_model_n_bytes += sizeof(vec3_t) * skel_model->n_bones * skel_model->n_frames;
    skel_model_n_bytes += sizeof(float) * skel_model->n_frames;
    // -- animation framegroups --
    skel_model_n_bytes += sizeof(char*) * skel_model->n_framegroups;
    skel_model_n_bytes += sizeof(uint32_t) * skel_model->n_framegroups;
    skel_model_n_bytes += sizeof(uint32_t) * skel_model->n_framegroups;
    skel_model_n_bytes += sizeof(float) * skel_model->n_framegroups;
    skel_model_n_bytes += sizeof(bool) * skel_model->n_framegroups;
    for(int i = 0; i < skel_model->n_framegroups; i++) {
        skel_model_n_bytes += safe_strsize(skel_model->framegroup_name[i]);
    }
    // -- materials --
    skel_model_n_bytes += sizeof(char*) * skel_model->n_materials;
    skel_model_n_bytes += sizeof(int) * skel_model->n_materials;
    skel_model_n_bytes += sizeof(skeletal_material_t*) * skel_model->n_materials;
    for(int i = 0; i < skel_model->n_materials; i++) {
        skel_model_n_bytes += safe_strsize(skel_model->material_names[i]);
        skel_model_n_bytes += sizeof(skeletal_material_t) * skel_model->material_n_skins[i];
    }
    // -- FTE Anim Events --
    if(skel_model->framegroup_n_events != nullptr) {
        skel_model_n_bytes += sizeof(uint16_t) * skel_model->n_framegroups;
        skel_model_n_bytes += sizeof(float*) * skel_model->n_framegroups;
        skel_model_n_bytes += sizeof(char**) * skel_model->n_framegroups;
        skel_model_n_bytes += sizeof(uint32_t*) * skel_model->n_framegroups;
        for(int i = 0; i < skel_model->n_framegroups; i++) {
            skel_model_n_bytes += sizeof(float) * skel_model->framegroup_n_events[i];
            skel_model_n_bytes += sizeof(char*) * skel_model->framegroup_n_events[i];
            skel_model_n_bytes += sizeof(uint32_t) * skel_model->framegroup_n_events[i];
            for(int j = 0; j < skel_model->framegroup_n_events[i]; j++) {
                skel_model_n_bytes += safe_strsize(skel_model->framegroup_event_data_str[i][j]);
            }
        }
    }
    return skel_model_n_bytes;
}


// 
// Util function for flattening a region of memory
// Given a region of memory `buffer` that can fit `n_bytes`
// 1. Copies `n_bytes` bytes from src_ptr into `buffer`
// 2. Assigns the `dest_ptr` to the start location of that memory
// 3. Increments the `buffer` pointer to point to the next free location of memory
// 
template<typename T>
void flatten_member(T* &dest_ptr, T *src_ptr, size_t n_bytes, uint8_t* &buffer) {
    // If src is nullptr, set dest to null_ptr and don't do anything else
    if(src_ptr == nullptr) {
        dest_ptr = nullptr;
        return;
    }
    // Pad requested memory to two bytes
    // if(n_bytes % 2 == 1) {
    //     n_bytes += 1;
    // }
    // Pad to 4-bytes
    n_bytes += (4 - (n_bytes % 4)) % 4;
    memcpy(buffer, src_ptr, n_bytes);
    dest_ptr = (T*) buffer;
    buffer += n_bytes;
}

// 
// Util function to use after flattening a region of memory
// Takes a pointer to memory and sets it to the offset to that region of memory
// relative to the start of the buffer
// 
template<typename T1, typename T2>
void set_member_to_offset(T1* &dest_ptr, T2 *buffer_start) {
    // If the pointer is nullptr, don't offset (leave it as 0)
    if(dest_ptr == nullptr) {
        return;
    }
    dest_ptr = (T1*) ((uint8_t*) dest_ptr - (uint8_t*) buffer_start);
}

template<typename T1, typename T2>
T1 *get_member_from_offset(T1 *member_ptr, T2 *buffer_start) {
    return (T1*) ((uint8_t*) buffer_start + (int) member_ptr);
}


//
// Copies the data contained in `skel_model` into `relocatable_skel_model` in 
// such a way that the model memory is fully relocatable. (i.e. all data is 
// laid out contiguously in memory, and pointers contain offsets relative to the 
// start of the memory block)
//
// NOTE - Assumes `relocatable_skel_model` is large enough to fully fit the skel_model's data
// NOTE - I'm horrified by this code too. There has got to be a better way to pull this off...
//
void make_skeletal_model_relocatable(skeletal_model_t *relocatable_skel_model, skeletal_model_t *skel_model) {
    // ------------–------------–------------–------------–------------–-------
    // Memcpy each piece of the skeletal model into the corresponding section
    // ------------–------------–------------–------------–------------–-------

    // Con_Printf("make_skeletal_model_relocatable 1.1\n");

    uint8_t *ptr = (uint8_t*) relocatable_skel_model;
    flatten_member(relocatable_skel_model, skel_model, sizeof(skeletal_model_t), ptr);
    flatten_member(relocatable_skel_model->meshes, skel_model->meshes, sizeof(skeletal_mesh_t) * skel_model->n_meshes, ptr);


    // Con_Printf("make_skeletal_model_relocatable 1.2\n");


    for(int i = 0; i < skel_model->n_meshes; i++) {
        // Con_Printf("make_skeletal_model_relocatable 1.2.1 i=%d\n", i);

        flatten_member(relocatable_skel_model->meshes[i].material_name, skel_model->meshes[i].material_name, safe_strsize(skel_model->meshes[i].material_name), ptr);
        
        // Con_Printf("make_skeletal_model_relocatable 1.2.2\n");
        flatten_member(relocatable_skel_model->meshes[i].submeshes, skel_model->meshes[i].submeshes, sizeof(skeletal_mesh_t) * skel_model->meshes[i].n_submeshes, ptr);

        // Con_Printf("make_skeletal_model_relocatable 1.2.3\n");
        for(int j = 0; j < skel_model->meshes[i].n_submeshes; j++) {

            // Con_Printf("make_skeletal_model_relocatable 1.2.3.1 j=%d\n", j);
            // Con_Printf("mesh[%d].submesh[%d].n_verts = %d\n", i, j, (void*) skel_model->meshes[i].submeshes[j].n_verts);
            // Con_Printf("mesh[%d].submesh[%d].vert8s = %p\n", i, j, (void*) skel_model->meshes[i].submeshes[j].vert8s);

            // IQMFIXME - Crashes here

            // if(skel_model->meshes[i].submeshes[j].vert8s != nullptr) {
                flatten_member(relocatable_skel_model->meshes[i].submeshes[j].vert8s, skel_model->meshes[i].submeshes[j].vert8s, sizeof(skel_vertex_i8_t) * skel_model->meshes[i].submeshes[j].n_verts, ptr);
            // }

            // Con_Printf("make_skeletal_model_relocatable 1.2.3.2 j=%d\n", j);
            // if(skel_model->meshes[i].submeshes[j].vert16s != nullptr) {
                flatten_member(relocatable_skel_model->meshes[i].submeshes[j].vert16s, skel_model->meshes[i].submeshes[j].vert16s, sizeof(skel_vertex_i16_t) * skel_model->meshes[i].submeshes[j].n_verts, ptr);
            // }
        }
        // Con_Printf("make_skeletal_model_relocatable 1.2.4\n");
    }


    // Con_Printf("make_skeletal_model_relocatable 1.3\n");


    // -- bones --
    flatten_member(relocatable_skel_model->bone_name, skel_model->bone_name, sizeof(char*) * skel_model->n_bones, ptr);
    flatten_member(relocatable_skel_model->bone_parent_idx, skel_model->bone_parent_idx, sizeof(int16_t) * skel_model->n_bones, ptr);
    flatten_member(relocatable_skel_model->bone_rest_pos, skel_model->bone_rest_pos, sizeof(vec3_t) * skel_model->n_bones, ptr);
    flatten_member(relocatable_skel_model->bone_rest_rot, skel_model->bone_rest_rot, sizeof(vec4_t) * skel_model->n_bones, ptr);
    flatten_member(relocatable_skel_model->bone_rest_scale, skel_model->bone_rest_scale, sizeof(vec3_t) * skel_model->n_bones, ptr);
    for(int i = 0; i < skel_model->n_bones; i++) {
        flatten_member(relocatable_skel_model->bone_name[i], skel_model->bone_name[i], safe_strsize(skel_model->bone_name[i]), ptr);
    }

    // Con_Printf("make_skeletal_model_relocatable 1.4\n");

    // -- bone hitbox info --
    flatten_member(relocatable_skel_model->bone_hitbox_enabled, skel_model->bone_hitbox_enabled, sizeof(bool) * skel_model->n_bones, ptr);
    flatten_member(relocatable_skel_model->bone_hitbox_ofs,     skel_model->bone_hitbox_ofs,     sizeof(vec3_t) * skel_model->n_bones, ptr);
    flatten_member(relocatable_skel_model->bone_hitbox_scale,   skel_model->bone_hitbox_scale,   sizeof(vec3_t) * skel_model->n_bones, ptr);
    flatten_member(relocatable_skel_model->bone_hitbox_tag,     skel_model->bone_hitbox_tag,     sizeof(int) * skel_model->n_bones, ptr);

    // Con_Printf("make_skeletal_model_relocatable 1.5\n");

    // -- cached bone rest transforms --
    flatten_member(relocatable_skel_model->bone_rest_transforms, skel_model->bone_rest_transforms, sizeof(mat3x4_t) * skel_model->n_bones, ptr);
    flatten_member(relocatable_skel_model->inv_bone_rest_transforms, skel_model->inv_bone_rest_transforms, sizeof(mat3x4_t) * skel_model->n_bones, ptr);

    // Con_Printf("make_skeletal_model_relocatable 1.6\n");

    // -- frames --
    flatten_member(relocatable_skel_model->frames_bone_pos, skel_model->frames_bone_pos, sizeof(vec3_t) * skel_model->n_bones * skel_model->n_frames, ptr);
    flatten_member(relocatable_skel_model->frames_bone_rot, skel_model->frames_bone_rot, sizeof(quat_t) * skel_model->n_bones * skel_model->n_frames, ptr);
    flatten_member(relocatable_skel_model->frames_bone_scale, skel_model->frames_bone_scale, sizeof(vec3_t) * skel_model->n_bones * skel_model->n_frames, ptr);
    flatten_member(relocatable_skel_model->frames_move_dist, skel_model->frames_move_dist, sizeof(float) * skel_model->n_frames, ptr);

    // Con_Printf("make_skeletal_model_relocatable 1.7\n");

    // -- framegroups --
    flatten_member(relocatable_skel_model->framegroup_name, skel_model->framegroup_name, sizeof(char*) * skel_model->n_framegroups, ptr);
    flatten_member(relocatable_skel_model->framegroup_start_frame, skel_model->framegroup_start_frame, sizeof(uint32_t) * skel_model->n_framegroups, ptr);
    flatten_member(relocatable_skel_model->framegroup_n_frames, skel_model->framegroup_n_frames, sizeof(uint32_t) * skel_model->n_framegroups, ptr);
    flatten_member(relocatable_skel_model->framegroup_fps, skel_model->framegroup_fps, sizeof(float) * skel_model->n_framegroups, ptr);
    flatten_member(relocatable_skel_model->framegroup_loop, skel_model->framegroup_loop, sizeof(bool) * skel_model->n_framegroups, ptr);
    for(int i = 0; i < skel_model->n_framegroups; i++) {
        flatten_member(relocatable_skel_model->framegroup_name[i], skel_model->framegroup_name[i], safe_strsize(skel_model->framegroup_name[i]), ptr);
    }

    // Con_Printf("make_skeletal_model_relocatable 1.8\n");

    // -- materials --
    flatten_member(relocatable_skel_model->material_names, skel_model->material_names, sizeof(char*) * skel_model->n_materials, ptr);
    flatten_member(relocatable_skel_model->material_n_skins, skel_model->material_n_skins, sizeof(int) * skel_model->n_materials, ptr);
    flatten_member(relocatable_skel_model->materials, skel_model->materials, sizeof(skeletal_material_t*) * skel_model->n_materials, ptr);
    for(int i = 0; i < skel_model->n_materials; i++) {
        flatten_member(relocatable_skel_model->material_names[i], skel_model->material_names[i], safe_strsize(skel_model->material_names[i]), ptr);
        flatten_member(relocatable_skel_model->materials[i], skel_model->materials[i], sizeof(skeletal_material_t) * skel_model->material_n_skins[i], ptr);
    }

    // Con_Printf("make_skeletal_model_relocatable 1.9\n");

    // -- FTE Anim Events --
    flatten_member(relocatable_skel_model->framegroup_n_events, skel_model->framegroup_n_events, sizeof(uint16_t) * skel_model->n_framegroups, ptr);
    flatten_member(relocatable_skel_model->framegroup_event_time, skel_model->framegroup_event_time, sizeof(float*) * skel_model->n_framegroups, ptr);
    flatten_member(relocatable_skel_model->framegroup_event_code, skel_model->framegroup_event_code, sizeof(uint32_t *) * skel_model->n_framegroups, ptr);
    flatten_member(relocatable_skel_model->framegroup_event_data_str, skel_model->framegroup_event_data_str, sizeof(char**) * skel_model->n_framegroups, ptr);
    if(skel_model->framegroup_n_events != nullptr) {
        for(int i = 0; i < skel_model->n_framegroups; i++) {
            flatten_member(relocatable_skel_model->framegroup_event_time[i], skel_model->framegroup_event_time[i], sizeof(float) * skel_model->framegroup_n_events[i], ptr);
            flatten_member(relocatable_skel_model->framegroup_event_code[i], skel_model->framegroup_event_code[i], sizeof(uint32_t) * skel_model->framegroup_n_events[i], ptr);
            flatten_member(relocatable_skel_model->framegroup_event_data_str[i], skel_model->framegroup_event_data_str[i], sizeof(char*) * skel_model->framegroup_n_events[i], ptr);
            for(int j = 0; j < skel_model->framegroup_n_events[i]; j++) {
                flatten_member(relocatable_skel_model->framegroup_event_data_str[i][j], skel_model->framegroup_event_data_str[i][j], safe_strsize(skel_model->framegroup_event_data_str[i][j]), ptr);
            }
        }
    }
    // ------------–------------–------------–------------–------------–-------

    // Con_Printf("make_skeletal_model_relocatable 1.10\n");


    // ------------–------------–------------–------------–------------–-------
    // Clean up all pointers to be relative to model start location in memory
    // NOTE - Must deconstruct in reverse order to avoid undoing offsets
    // ------------–------------–------------–------------–------------–-------
    // -- FTE Anim Events --
    if(skel_model->framegroup_n_events != nullptr) {
        for(int i = 0; i < skel_model->n_framegroups; i++) {
            for(int j = 0; j < skel_model->framegroup_n_events[i]; j++) {
                set_member_to_offset(relocatable_skel_model->framegroup_event_data_str[i][j], relocatable_skel_model);
            }
            set_member_to_offset(relocatable_skel_model->framegroup_event_data_str[i], relocatable_skel_model);
            set_member_to_offset(relocatable_skel_model->framegroup_event_code[i], relocatable_skel_model);
            set_member_to_offset(relocatable_skel_model->framegroup_event_time[i], relocatable_skel_model);
        }
    }
    set_member_to_offset(relocatable_skel_model->framegroup_event_data_str, relocatable_skel_model);
    set_member_to_offset(relocatable_skel_model->framegroup_event_code, relocatable_skel_model);
    set_member_to_offset(relocatable_skel_model->framegroup_event_time, relocatable_skel_model);
    set_member_to_offset(relocatable_skel_model->framegroup_n_events, relocatable_skel_model);

    // Con_Printf("make_skeletal_model_relocatable 1.11\n");


    // -- materials --
    for(int i = 0; i < skel_model->n_materials; i++) {
        set_member_to_offset(relocatable_skel_model->materials[i], relocatable_skel_model);
        set_member_to_offset(relocatable_skel_model->material_names[i], relocatable_skel_model);
    }
    set_member_to_offset(relocatable_skel_model->materials, relocatable_skel_model);
    set_member_to_offset(relocatable_skel_model->material_n_skins, relocatable_skel_model);
    set_member_to_offset(relocatable_skel_model->material_names, relocatable_skel_model);

    // Con_Printf("make_skeletal_model_relocatable 1.12\n");

    // -- framegroups --
    for(int i = 0; i < skel_model->n_framegroups; i++) {
        set_member_to_offset(relocatable_skel_model->framegroup_name[i], relocatable_skel_model);
    }
    set_member_to_offset(relocatable_skel_model->framegroup_loop, relocatable_skel_model);
    set_member_to_offset(relocatable_skel_model->framegroup_fps, relocatable_skel_model);
    set_member_to_offset(relocatable_skel_model->framegroup_n_frames, relocatable_skel_model);
    set_member_to_offset(relocatable_skel_model->framegroup_start_frame, relocatable_skel_model);
    set_member_to_offset(relocatable_skel_model->framegroup_name, relocatable_skel_model);

    // Con_Printf("make_skeletal_model_relocatable 1.13\n");

    // -- frames --
    set_member_to_offset(relocatable_skel_model->frames_move_dist, relocatable_skel_model);
    set_member_to_offset(relocatable_skel_model->frames_bone_scale, relocatable_skel_model);
    set_member_to_offset(relocatable_skel_model->frames_bone_rot, relocatable_skel_model);
    set_member_to_offset(relocatable_skel_model->frames_bone_pos, relocatable_skel_model);

    // Con_Printf("make_skeletal_model_relocatable 1.14\n");
    // -- cached bone rest transforms --
    set_member_to_offset(relocatable_skel_model->inv_bone_rest_transforms, relocatable_skel_model);
    set_member_to_offset(relocatable_skel_model->bone_rest_transforms, relocatable_skel_model);

    // Con_Printf("make_skeletal_model_relocatable 1.15\n");
    // -- bone hitbox info --
    set_member_to_offset(relocatable_skel_model->bone_hitbox_tag,     relocatable_skel_model);
    set_member_to_offset(relocatable_skel_model->bone_hitbox_scale,   relocatable_skel_model);
    set_member_to_offset(relocatable_skel_model->bone_hitbox_ofs,     relocatable_skel_model);
    set_member_to_offset(relocatable_skel_model->bone_hitbox_enabled, relocatable_skel_model);

    // Con_Printf("make_skeletal_model_relocatable 1.16\n");
    // -- bones --
    for(int i = 0; i < skel_model->n_bones; i++) {
        set_member_to_offset(relocatable_skel_model->bone_name[i], relocatable_skel_model);
    }
    set_member_to_offset(relocatable_skel_model->bone_rest_scale, relocatable_skel_model);
    set_member_to_offset(relocatable_skel_model->bone_rest_rot, relocatable_skel_model);
    set_member_to_offset(relocatable_skel_model->bone_rest_pos, relocatable_skel_model);
    set_member_to_offset(relocatable_skel_model->bone_parent_idx, relocatable_skel_model);
    set_member_to_offset(relocatable_skel_model->bone_name, relocatable_skel_model);

    for(int i = 0; i < skel_model->n_meshes; i++) {
        for(int j = 0; j < skel_model->meshes[i].n_submeshes; j++) {
            set_member_to_offset(relocatable_skel_model->meshes[i].submeshes[j].vert8s, relocatable_skel_model);
            set_member_to_offset(relocatable_skel_model->meshes[i].submeshes[j].vert16s, relocatable_skel_model);
        }
        set_member_to_offset(relocatable_skel_model->meshes[i].submeshes, relocatable_skel_model);
        set_member_to_offset(relocatable_skel_model->meshes[i].material_name, relocatable_skel_model);
    }
    set_member_to_offset(relocatable_skel_model->meshes, relocatable_skel_model);

    // Con_Printf("make_skeletal_model_relocatable 1.17\n");
    // ------------–------------–------------–------------–------------–-------
}


//
// Completely deallocates a skeletal_model_t struct, pointers and all.
// NOTE - This should not be used on a relocatable skeletal_model_t object.
//
void free_skeletal_model(skeletal_model_t *skel_model) {
    // Free fields in reverse order to avoid losing references
    // -- FTE Anim Events --
    if(skel_model->framegroup_n_events != nullptr) {
        for(int i = 0; i < skel_model->n_framegroups; i++) {
            for(int j = 0; j < skel_model->framegroup_n_events[i]; j++) {
                free_pointer_and_clear(skel_model->framegroup_event_data_str[i][j]);
            }
            free_pointer_and_clear(skel_model->framegroup_event_data_str[i]);
            free_pointer_and_clear(skel_model->framegroup_event_code[i]);
            free_pointer_and_clear(skel_model->framegroup_event_time[i]);
        }
    }
    free_pointer_and_clear(skel_model->framegroup_event_data_str);
    free_pointer_and_clear(skel_model->framegroup_event_code);
    free_pointer_and_clear(skel_model->framegroup_event_time);
    free_pointer_and_clear(skel_model->framegroup_n_events);
    // -- materials --
    for(int i = 0; i < skel_model->n_materials; i++) {
        free_pointer_and_clear(skel_model->materials[i]);
        free_pointer_and_clear(skel_model->material_names[i]);
    }
    free_pointer_and_clear(skel_model->materials);
    free_pointer_and_clear(skel_model->material_n_skins);
    free_pointer_and_clear(skel_model->material_names);
    // -- framegroups --
    for(int i = 0; i < skel_model->n_framegroups; i++) {
        free_pointer_and_clear(skel_model->framegroup_name[i]);
    }
    free_pointer_and_clear(skel_model->framegroup_loop);
    free_pointer_and_clear(skel_model->framegroup_fps);
    free_pointer_and_clear(skel_model->framegroup_n_frames);
    free_pointer_and_clear(skel_model->framegroup_start_frame);
    free_pointer_and_clear(skel_model->framegroup_name);
    // -- frames --
    free_pointer_and_clear(skel_model->frames_bone_scale);
    free_pointer_and_clear(skel_model->frames_bone_rot);
    free_pointer_and_clear(skel_model->frames_bone_pos);
    free_pointer_and_clear(skel_model->frames_move_dist);
    // -- cached bone rest transforms --
    free_pointer_and_clear(skel_model->inv_bone_rest_transforms);
    free_pointer_and_clear(skel_model->bone_rest_transforms);
    // -- bone hitbox info --
    free_pointer_and_clear(skel_model->bone_hitbox_tag);
    free_pointer_and_clear(skel_model->bone_hitbox_scale);
    free_pointer_and_clear(skel_model->bone_hitbox_ofs);
    free_pointer_and_clear(skel_model->bone_hitbox_enabled);
    // -- bones --
    for(int i = 0; i < skel_model->n_bones; i++) {
        free_pointer_and_clear(skel_model->bone_name[i]);
    }
    free_pointer_and_clear(skel_model->bone_name);
    free_pointer_and_clear(skel_model->bone_parent_idx);
    free_pointer_and_clear(skel_model->bone_rest_pos);
    free_pointer_and_clear(skel_model->bone_rest_rot);
    free_pointer_and_clear(skel_model->bone_rest_scale);
    for(int i = 0; i < skel_model->n_meshes; i++) {
        for(int j = 0; j < skel_model->meshes[i].n_submeshes; j++) {
            // These submesh struct members are expected to be nullptr:
            free_pointer_and_clear( skel_model->meshes[i].submeshes[j].vert_rest_positions);
            free_pointer_and_clear( skel_model->meshes[i].submeshes[j].vert_uvs);

#ifdef IQM_LOAD_NORMALS
            free_pointer_and_clear( skel_model->meshes[i].submeshes[j].vert_rest_normals);
#endif // IQM_LOAD_NORMALS

            free_pointer_and_clear( skel_model->meshes[i].submeshes[j].vert_bone_weights);
            free_pointer_and_clear( skel_model->meshes[i].submeshes[j].vert_bone_idxs);
            free_pointer_and_clear( skel_model->meshes[i].submeshes[j].vert_skinning_weights);
            free_pointer_and_clear( skel_model->meshes[i].submeshes[j].tri_verts);
            free_pointer_and_clear( skel_model->meshes[i].submeshes[j].submeshes);
            free_pointer_and_clear( skel_model->meshes[i].submeshes[j].material_name);

            // These submesh struct members are expected to be allocated:
            free_pointer_and_clear( skel_model->meshes[i].submeshes[j].vert8s);     // Only free if not nullptr
            free_pointer_and_clear( skel_model->meshes[i].submeshes[j].vert16s);    // Only free if not nullptr
        }
        
        // These mesh struct members are expected to be nullptr:
        free_pointer_and_clear( skel_model->meshes[i].vert_rest_positions);
        free_pointer_and_clear( skel_model->meshes[i].vert_uvs);

#ifdef IQM_LOAD_NORMALS
        free_pointer_and_clear( skel_model->meshes[i].vert_rest_normals);
#endif // IQM_LOAD_NORMALS

        free_pointer_and_clear( skel_model->meshes[i].vert_bone_weights);
        free_pointer_and_clear( skel_model->meshes[i].vert_bone_idxs);
        free_pointer_and_clear( skel_model->meshes[i].vert_skinning_weights);
        free_pointer_and_clear( skel_model->meshes[i].tri_verts);
        free_pointer_and_clear( skel_model->meshes[i].vert16s);
        free_pointer_and_clear( skel_model->meshes[i].vert8s);

        // These mesh struct members are expected to be allocated:
        free_pointer_and_clear( skel_model->meshes[i].material_name);
        free_pointer_and_clear( skel_model->meshes[i].submeshes);
    }
    free(skel_model->meshes);
    free(skel_model);
}



// 
// Given a JSON object representing a material, parses the JSON to fill in 
// the material values
// 
void load_material_json_info(skeletal_material_t *material, json material_info, const char *json_file_name, const char *material_name) {
    // Defaults
    material->texture_idx = 0; // FIXME - Default to missing texture?
    material->transparent = false;
    material->fullbright = false;
    material->fog = true;
    material->add_color = false;
    material->add_color_red = 0.0f;
    material->add_color_green = 0.0f;
    material->add_color_blue = 0.0f;
    material->add_color_alpha = 1.0f;

    if(material_info.is_null()) {
        return;
    }
    if(!material_info.is_object()) {
        return;
    }

    if(material_info.contains("texture") && material_info["texture"].is_string()) {
        std::string texture_file_str = material_info["texture"];
        char *texture_file = texture_file_str.data();
        material->texture_idx = loadtextureimage(texture_file, 0, 0, qtrue, GU_LINEAR);
    }
    if(material_info.contains("transparent") && material_info["transparent"].is_boolean()) {
        material->transparent = (bool) material_info["transparent"].get<bool>();
    }
    if(material_info.contains("fullbright") && material_info["fullbright"].is_boolean()) {
        material->fullbright = (bool) material_info["fullbright"].get<bool>();
    }
    if(material_info.contains("fog") && material_info["fog"].is_boolean()) {
        material->fog = (bool) material_info["fog"].get<bool>();
    }
    if(material_info.contains("shade_smooth") && material_info["shade_smooth"].is_boolean()) {
        material->shade_smooth = (bool) material_info["shade_smooth"].get<bool>();
    }
    if(material_info.contains("add_color") && material_info["add_color"].is_array()) {
        // Verify that each array entry is a number
        bool numbers_valid = true;
        for(int i = 0; i < material_info["add_color"].size(); i++) {
            if(!material_info["add_color"][i].is_number()) {
                Con_Printf("Warning - IQM JSON file \"%s\" material \"%s\" \"add_color\" array index %d entry is non-numeric.\n", json_file_name, material_name, i);
                numbers_valid = false;
                break;
            }
        }
        if(numbers_valid) {
            if(material_info["add_color"].size() == 3) {
                material->add_color = true;
                material->add_color_red   = (float) material_info["add_color"][0].get<float>();
                material->add_color_green = (float) material_info["add_color"][1].get<float>();
                material->add_color_blue  = (float) material_info["add_color"][2].get<float>();
            }
            else if(material_info["add_color"].size() == 4) {
                material->add_color = true;
                material->add_color_red   = (float) material_info["add_color"][0].get<float>();
                material->add_color_green = (float) material_info["add_color"][1].get<float>();
                material->add_color_blue  = (float) material_info["add_color"][2].get<float>();
                material->add_color_alpha = (float) material_info["add_color"][3].get<float>();
            }
            else {
                Con_Printf("Warning - IQM JSON file \"%s\" material \"%s\" \"add_color\" array has length %d, but should be 3 (for RGB) or 4 (for RGBA)\n", json_file_name, material_name, material_info["add_color"].size());
            }
        }
    }
}


//
// For a given IQM model file, attempts to load its corresponding "{filename}.iqm.json" 
// file for additional config / info, if it exists
// 
// NOTE - This function expects `skel_model` to contain absolute pointers. Not relative.
// NOTE   (i.e.: this function is called before `make_skeletal_model_relocatable`)
//
void load_iqm_file_json_info(skeletal_model_t *skel_model, const char *iqm_file_path) {
    char iqm_info_json_file[256];
    sprintf(iqm_info_json_file, "%s.json", iqm_file_path);
    Con_Printf("Loading IQM model info JSON file: \"%s\"\n", iqm_info_json_file);
    const char *iqm_info_json_str = (const char*) COM_LoadFile(iqm_info_json_file, 0);
    // If unable to load JSON file contents, stop
    if(iqm_info_json_str == nullptr) {
        return;
    }
    Con_Printf("Loaded JSON file contents.\n");


    // if(!json::accept(iqm_info_json_str)) {
    //     Con_Printf("JSON accepted!\n");
    // }
    // else {
    //     Con_Printf("JSON not accepted.\n");
    // }

    // --------------------------------------------------------------------
    // Parse without error description
    // --------------------------------------------------------------------
    // json json_data = json::parse(iqm_info_json_str, nullptr, false, true); // excpetions=false, ignore_comments=true

    // if(!json_data.is_discarded()) {
    //     Con_Printf("JSON parsed successfully!\n");
    // }
    // else {
    //     Con_Printf("Error parsing json file.\n");
    // }
    // --------------------------------------------------------------------


    // --------------------------------------------------------------------
    // Parse with error descriptions
    // --------------------------------------------------------------------
    json json_data;
    try {
        json_data = json::parse(iqm_info_json_str, nullptr, true, true); // excpetions=true, ignore_comments=true
        Con_Printf("JSON parse success!\n");
    }
    catch(json::parse_error &json_parse_error) {
        std::string error_msg = json_parse_error.what();
        Con_Printf("JSON error: \"%s\"\n", error_msg.c_str());
        return;
    }
    // --------------------------------------------------------------------

    char **bone_names = skel_model->bone_name;

    if(json_data.contains("bone_hitbox_tag") && json_data["bone_hitbox_tag"].is_object()) {
        json bone_hitbox_tags_json_data = json_data["bone_hitbox_tag"];
        for(int i = 0; i < skel_model->n_bones; i++) {
            if(bone_hitbox_tags_json_data.contains(bone_names[i])) {
                if(!bone_hitbox_tags_json_data[bone_names[i]].is_number_integer()) {
                    Con_Printf("Warning - IQM JSON file \"%s\" has non-integer \"bone_hitbox_tag\" value for bone \"%s\"\n", iqm_info_json_file, bone_names[i]);
                    continue;
                }
                skel_model->bone_hitbox_tag[i] = (int) bone_hitbox_tags_json_data[bone_names[i]];
            }
        }
    }


    if(json_data.contains("bone_hitbox_enabled") && json_data["bone_hitbox_enabled"].is_object()) {
        json bone_hitbox_enabled_json_data = json_data["bone_hitbox_enabled"];
        for(int i = 0; i < skel_model->n_bones; i++) {
            if(bone_hitbox_enabled_json_data.contains(bone_names[i])) {
                if(!bone_hitbox_enabled_json_data[bone_names[i]].is_boolean()) {
                    Con_Printf("Warning - IQM JSON file \"%s\" has non-boolean \"bone_hitbox_enabled\" value for bone \"%s\"\n", iqm_info_json_file, bone_names[i]);
                    continue;
                }
                skel_model->bone_hitbox_enabled[i] = (bool) bone_hitbox_enabled_json_data[bone_names[i]];
            }
        }
    }


    if(json_data.contains("bone_hitbox_ofs") && json_data["bone_hitbox_ofs"].is_object()) {
        json bone_hitbox_ofs_json_data = json_data["bone_hitbox_ofs"];
        for(int i = 0; i < skel_model->n_bones; i++) {
            if(bone_hitbox_ofs_json_data.contains(bone_names[i])) {
                // Check that the entry is an array
                if(!bone_hitbox_ofs_json_data[bone_names[i]].is_array()) {
                    Con_Printf("Warning - IQM JSON file \"%s\" has non-array \"bone_hitbox_ofs\" value for bone \"%s\"\n", iqm_info_json_file, bone_names[i]);
                    continue;
                }
                // Check that the array is of length 3
                if(bone_hitbox_ofs_json_data[bone_names[i]].size() != 3) {
                    Con_Printf("Warning - IQM JSON file \"%s\" has \"bone_hitbox_ofs\" array of size != 3 for bone \"%s\"\n", iqm_info_json_file, bone_names[i]);
                    continue;
                }
                // Check to make sure all three entries are numbers
                bool numbers_valid = true;
                for(int j = 0; j < 3; j++) {
                    if(!bone_hitbox_ofs_json_data[bone_names[i]][0].is_number()) {
                        Con_Printf("Warning - IQM JSON file \"%s\" has \"bone_hitbox_ofs\" array index %d entry is non-numeric for bone \"%s\"\n", iqm_info_json_file, j, bone_names[i]);
                        numbers_valid = false;
                        break;
                    }
                }
                if(!numbers_valid) {
                    continue;
                }
                for(int j = 0; j < 3; j++) {
                    skel_model->bone_hitbox_ofs[i][j] = (float) bone_hitbox_ofs_json_data[bone_names[i]][j];
                }
            }
        }
    }
    
    if(json_data.contains("bone_hitbox_scale") && json_data["bone_hitbox_scale"].is_object()) {
        json bone_hitbox_scale_json_data = json_data["bone_hitbox_scale"];
        for(int i = 0; i < skel_model->n_bones; i++) {
            if(bone_hitbox_scale_json_data.contains(bone_names[i])) {
                // Check that the entry is an array
                if(!bone_hitbox_scale_json_data[bone_names[i]].is_array()) {
                    Con_Printf("Warning - IQM JSON file \"%s\" has non-array \"bone_hitbox_scale\" value for bone \"%s\"\n", iqm_info_json_file, bone_names[i]);
                    continue;
                }
                // Check that the array is of length 3
                if(bone_hitbox_scale_json_data[bone_names[i]].size() != 3) {
                    Con_Printf("Warning - IQM JSON file \"%s\" has \"bone_hitbox_scale\" array of size != 3 for bone \"%s\"\n", iqm_info_json_file, bone_names[i]);
                    continue;
                }
                // Check to make sure all three entries are numbers
                bool numbers_valid = true;
                for(int j = 0; j < 3; j++) {
                    if(!bone_hitbox_scale_json_data[bone_names[i]][0].is_number()) {
                        Con_Printf("Warning - IQM JSON file \"%s\" has \"bone_hitbox_scale\" array index %d entry is non-numeric for bone \"%s\"\n", iqm_info_json_file, j, bone_names[i]);
                        numbers_valid = false;
                        break;
                    }
                }
                if(!numbers_valid) {
                    continue;
                }
                for(int j = 0; j < 3; j++) {
                    skel_model->bone_hitbox_scale[i][j] = (float) bone_hitbox_scale_json_data[bone_names[i]][j];
                }
            }
        }
    }

    if(json_data.contains("materials") && json_data["materials"].is_object() && json_data["materials"].size() > 0) {
        int n_materials = json_data["materials"].size();
        Con_Printf("Found %d materials\n", n_materials);

        skel_model->n_materials = n_materials;
        skel_model->material_names = (char**) malloc(sizeof(char*) * n_materials);
        skel_model->material_n_skins = (int*) malloc(sizeof(int) * n_materials);
        skel_model->materials = (skeletal_material_t**) malloc(sizeof(skeletal_material_t*) * n_materials);

        int material_idx = 0;
        for(json::iterator item = json_data["materials"].begin(); item != json_data["materials"].end(); ++item, ++material_idx) {
            std::string material_name_str = item.key();
            const char *material_name = material_name_str.c_str();
            Con_Printf("material iter key: \"%s\"\n", material_name);
            json material_val = item.value();

            skel_model->material_names[material_idx] = (char*) malloc(sizeof(char) * (strlen(material_name) + 1));
            strcpy( skel_model->material_names[material_idx], material_name);
            
            if(material_val.is_array()) {
                int n_skins = material_val.size();
                skel_model->material_n_skins[material_idx] = n_skins;
                skel_model->materials[material_idx] = (skeletal_material_t*) malloc(sizeof(skeletal_material_t) * n_skins);
                for(int skin_idx = 0; skin_idx < material_val.size(); skin_idx++) {
                    load_material_json_info(&(skel_model->materials[material_idx][skin_idx]), material_val[skin_idx], iqm_info_json_file, material_name);
                }
            }
            else if(material_val.is_object()) {
                int n_skins = 1;
                skel_model->material_n_skins[material_idx] = n_skins;
                skel_model->materials[material_idx] = (skeletal_material_t*) malloc(sizeof(skeletal_material_t) * n_skins);
                // Load the one skin:
                int skin_idx = 0;
                load_material_json_info(&(skel_model->materials[material_idx][skin_idx]), material_val, iqm_info_json_file, material_name);
            }
            else {
                // Load a single default / error material for this material name
                int n_skins = 1;
                skel_model->material_n_skins[material_idx] = n_skins;
                skel_model->materials[material_idx] = (skeletal_material_t*) malloc(sizeof(skeletal_material_t) * n_skins);
                int skin_idx = 0;
                load_material_json_info(&(skel_model->materials[material_idx][skin_idx]), nullptr, iqm_info_json_file, material_name);
            }
        }

        // For each mesh, find the material_idx for the material it references, if not found set to 0
        for(int i = 0; i < skel_model->n_meshes; i++) {
            skel_model->meshes[i].material_idx = 0; // Default value
            for(int j = 0; j < skel_model->n_materials; j++) {
                if(!strcmp(skel_model->meshes[i].material_name, skel_model->material_names[j])) {
                    skel_model->meshes[i].material_idx = j;
                    break;
                }
            }
        }
    }
}


/*
=================
Mod_LoadIQMModel
=================
*/
extern char loadname[];
void Mod_LoadIQMModel (model_t *model, void *buffer) {

    Con_Printf("Loading IQM model: \"%s\"\n", model->name);
    skeletal_model_t *skel_model = load_iqm_file(buffer);
    load_iqm_file_json_info(skel_model, model->name);

    // ------------------------------------------------------------------------
    // Debug - printing the skel_model contents
    // ------------------------------------------------------------------------
#ifdef IQMDEBUG_LOADIQM_DEBUGSUMMARY
    Con_Printf("------------------------------------------------------------\n");
    Con_Printf("Debug printing skeletal model \"%s\"\n", loadname);
    Con_Printf("------------------------------------------------------------\n");
    Con_Printf("skel_model->n_meshes = %d\n", skel_model->n_meshes);
    for(int i = 0; i < skel_model->n_meshes; i++) {
        Con_Printf("---------------------\n");
        Con_Printf("skel_model->meshes[%d].n_verts = %d\n", i, skel_model->meshes[i].n_verts);
        Con_Printf("skel_model->meshes[%d].n_tris = %d\n", i, skel_model->meshes[i].n_tris);
        Con_Printf("skel_model->meshes[%d].vert_rest_positions = %d\n", i, skel_model->meshes[i].vert_rest_positions);
        Con_Printf("skel_model->meshes[%d].vert_uvs = %d\n", i, skel_model->meshes[i].vert_uvs);

#ifdef IQM_LOAD_NORMALS
        Con_Printf("skel_model->meshes[%d].vert_rest_normals = %d\n", i, skel_model->meshes[i].vert_rest_normals);
#endif // IQM_LOAD_NORMALS

        Con_Printf("skel_model->meshes[%d].vert_bone_weights = %d\n", i, skel_model->meshes[i].vert_bone_weights);
        Con_Printf("skel_model->meshes[%d].vert_bone_idxs = %d\n", i, skel_model->meshes[i].vert_bone_idxs);

        // IQMFIXME - Crashed here
        Con_Printf("skel_model->meshes[%d].vert_skinning_weights = %d\n", i, skel_model->meshes[i].vert_skinning_weights);
        Con_Printf("skel_model->meshes[%d].tri_verts = %d\n", i, skel_model->meshes[i].tri_verts);
        Con_Printf("skel_model->meshes[%d].vert16s = %d\n", i, skel_model->meshes[i].vert16s);
        Con_Printf("skel_model->meshes[%d].vert8s = %d\n", i, skel_model->meshes[i].vert8s);
        Con_Printf("skel_model->meshes[%d].geomset = %d\n", i, skel_model->meshes[i].geomset);
        Con_Printf("skel_model->meshes[%d].geomid = %d\n", i, skel_model->meshes[i].geomid);
        Con_Printf("skel_model->meshes[%d].material_name = \"%s\"\n", i, skel_model->meshes[i].material_name);
        Con_Printf("skel_model->meshes[%d].material_idx = %d\n", i, skel_model->meshes[i].material_idx);
        // --
        Con_Printf("skel_model->meshes[%d].n_skinning_bones = %d\n", i, skel_model->meshes[i].n_skinning_bones);
        Con_Printf("\tskel_model->meshes[%d].skinning_bone_idxs = [", i);
        for(int k = 0; k < 8; k++) {
            Con_Printf("%d, ", skel_model->meshes[i].skinning_bone_idxs[k]);
        }
        Con_Printf("]\n");
        // --
        Con_Printf("\tskel_model->meshes[%d].verts_ofs = [", i);
        for(int k = 0; k < 3; k++) {
            Con_Printf("%f, ", skel_model->meshes[i].verts_ofs[k]);
        }
        Con_Printf("]\n");
        // --
        Con_Printf("\tskel_model->meshes[%d].verts_scale = [", i);
        for(int k = 0; k < 3; k++) {
            Con_Printf("%f, ", skel_model->meshes[i].verts_scale[k]);
        }
        Con_Printf("]\n");
        // --
        Con_Printf("skel_model->meshes[%d].n_submeshes = %d\n", i, skel_model->meshes[i].n_submeshes);
        Con_Printf("skel_model->meshes[%d].submeshes = %d\n", i, skel_model->meshes[i].submeshes);

        for(int j = 0; j < skel_model->meshes[i].n_submeshes; j++) {
            Con_Printf("\t----------\n");
            Con_Printf("\tskel_model->meshes[%d].submeshes[%d].n_verts = %d\n", i, j, skel_model->meshes[i].submeshes[j].n_verts);
            Con_Printf("\tskel_model->meshes[%d].submeshes[%d].n_tris = %d\n", i, j, skel_model->meshes[i].submeshes[j].n_tris);
            Con_Printf("\tskel_model->meshes[%d].submeshes[%d].vert_rest_positions = %d\n", i, j, skel_model->meshes[i].submeshes[j].vert_rest_positions);
            Con_Printf("\tskel_model->meshes[%d].submeshes[%d].vert_uvs = %d\n", i, j, skel_model->meshes[i].submeshes[j].vert_uvs);

#ifdef IQM_LOAD_NORMALS
            Con_Printf("\tskel_model->meshes[%d].submeshes[%d].vert_rest_normals = %d\n", i, j, skel_model->meshes[i].submeshes[j].vert_rest_normals);
#endif // IQM_LOAD_NORMALS

            Con_Printf("\tskel_model->meshes[%d].submeshes[%d].vert_bone_weights = %d\n", i, j, skel_model->meshes[i].submeshes[j].vert_bone_weights);
            Con_Printf("\tskel_model->meshes[%d].submeshes[%d].vert_bone_idxs = %d\n", i, j, skel_model->meshes[i].submeshes[j].vert_bone_idxs);
            Con_Printf("\tskel_model->meshes[%d].submeshes[%d].vert_skinning_weights = %d\n", i, j, skel_model->meshes[i].submeshes[j].vert_skinning_weights);
            Con_Printf("\tskel_model->meshes[%d].submeshes[%d].tri_verts = %d\n", i, j, skel_model->meshes[i].submeshes[j].tri_verts);
            Con_Printf("\tskel_model->meshes[%d].submeshes[%d].vert16s = %d\n", i, j, skel_model->meshes[i].submeshes[j].vert16s);
            Con_Printf("\tskel_model->meshes[%d].submeshes[%d].vert8s = %d\n", i, j, skel_model->meshes[i].submeshes[j].vert8s);
            Con_Printf("\tskel_model->meshes[%d].submeshes[%d].n_skinning_bones = %d\n", i, j, skel_model->meshes[i].submeshes[j].n_skinning_bones);
            // --
            Con_Printf("\tskel_model->meshes[%d].submeshes[%d].skinning_bone_idxs = [", i, j);
            for(int k = 0; k < 8; k++) {
                Con_Printf("%d, ", skel_model->meshes[i].submeshes[j].skinning_bone_idxs[k]);
            }
            Con_Printf("]\n");
            // --
            Con_Printf("\tskel_model->meshes[%d].submeshes[%d].verts_ofs = [", i, j);
            for(int k = 0; k < 3; k++) {
                Con_Printf("%f, ", skel_model->meshes[i].submeshes[j].verts_ofs[k]);
            }
            Con_Printf("]\n");
            // --
            Con_Printf("\tskel_model->meshes[%d].submeshes[%d].verts_scale = [", i, j);
            for(int k = 0; k < 3; k++) {
                Con_Printf("%f, ", skel_model->meshes[i].submeshes[j].verts_scale[k]);
            }
            Con_Printf("]\n");
            // --
            Con_Printf("\tskel_model->meshes[%d].submeshes[%d].n_submeshes = %d\n", i, j, skel_model->meshes[i].submeshes[j].n_submeshes);
            Con_Printf("\tskel_model->meshes[%d].submeshes[%d].submeshes = %d\n", i, j, skel_model->meshes[i].submeshes[j].submeshes);
            Con_Printf("\t----------\n");
        }
        Con_Printf("---------------------\n");
    }
    for(uint32_t i = 0; i < skel_model->n_bones; i++) {
        Con_Printf("Parsed bone: %i, \"%s\" (parent bone: %d)\n", i, skel_model->bone_name[i], skel_model->bone_parent_idx[i]);
        Con_Printf("\tPos: (%.2f, %.2f, %.2f)\n", skel_model->bone_rest_pos[i][0], skel_model->bone_rest_pos[i][1], skel_model->bone_rest_pos[i][2]);
        Con_Printf("\tRot: (%.2f, %.2f, %.2f, %.2f)\n", skel_model->bone_rest_rot[i][0], skel_model->bone_rest_rot[i][1], skel_model->bone_rest_rot[i][2], skel_model->bone_rest_rot[i][3]);
        Con_Printf("\tScale: (%.2f, %.2f, %.2f)\n", skel_model->bone_rest_scale[i][0], skel_model->bone_rest_scale[i][1], skel_model->bone_rest_scale[i][2]);
        Con_Printf("\thitbox enabled: %d\n", skel_model->bone_hitbox_enabled[i]);
        Con_Printf("\thitbox ofs: (%.2f, %.2f, %.2f)\n", skel_model->bone_hitbox_ofs[i][0], skel_model->bone_hitbox_ofs[i][1], skel_model->bone_hitbox_ofs[i][2]);
        Con_Printf("\thitbox scale: (%.2f, %.2f, %.2f)\n", skel_model->bone_hitbox_scale[i][0], skel_model->bone_hitbox_scale[i][1], skel_model->bone_hitbox_scale[i][2]);
        Con_Printf("\thitbox tag: %d\n", skel_model->bone_hitbox_tag[i]);
    }

    for(int i = 0; i < skel_model->n_materials; i++) {
        Con_Printf("---------------------\n");
        Con_Printf("skel_model->material_names[%d] = %s\n", i, skel_model->material_names[i]);
        Con_Printf("skel_model->material_n_skins[%d] = %d\n", i, skel_model->material_n_skins[i]);
        for(int j = 0; j < skel_model->material_n_skins[i]; j++) {
            Con_Printf("\tskel_model->materials[%d][%d].texture_idx = %d\n", i, j, skel_model->materials[i][j].texture_idx);
            Con_Printf("\tskel_model->materials[%d][%d].transparent = %d\n", i, j, skel_model->materials[i][j].transparent);
            Con_Printf("\tskel_model->materials[%d][%d].fullbright = %d\n", i, j, skel_model->materials[i][j].fullbright);
            Con_Printf("\tskel_model->materials[%d][%d].fog = %d\n", i, j, skel_model->materials[i][j].fog);
            Con_Printf("\tskel_model->materials[%d][%d].shade_smooth = %d\n", i, j, skel_model->materials[i][j].shade_smooth);
            Con_Printf("\tskel_model->materials[%d][%d].add_color = %d\n", i, j, skel_model->materials[i][j].add_color);
            Con_Printf("\tskel_model->materials[%d][%d].add_color val = [%f, %f, %f, %f]\n", i, j, skel_model->materials[i][j].add_color_red, skel_model->materials[i][j].add_color_green, skel_model->materials[i][j].add_color_blue, skel_model->materials[i][j].add_color_alpha);
        }
    }


    Con_Printf("skel_model->n_framegroups = %d\n", skel_model->n_framegroups);

    for(int i = 0; i < skel_model->n_framegroups; i++) {
        Con_Printf("---------------------\n");
        Con_Printf("skel_model->framegroup_name[%d] = \"%s\"\n", i, skel_model->framegroup_name[i]);
        Con_Printf("skel_model->framegroup_start_frame[%d] = %d\n", i, skel_model->framegroup_start_frame[i]);
        Con_Printf("skel_model->framegroup_n_frames[%d] = %d\n", i, skel_model->framegroup_n_frames[i]);
        Con_Printf("skel_model->framegroup_fps[%d] = %f\n", i, skel_model->framegroup_fps[i]);
        Con_Printf("skel_model->framegroup_loop[%d] = %d\n", i, skel_model->framegroup_loop[i]);

        if(skel_model->framegroup_n_events != nullptr) {
            Con_Printf("skel_model->framegroup_n_events[%d] = %d\n", i, skel_model->framegroup_n_events[i]);
            for(int j = 0; j < skel_model->framegroup_n_events[i]; j++) {
                Con_Printf("\tskel_model->framegroup_event_time[%d][%d] = %f\n", i, j, skel_model->framegroup_event_time[i][j]);
                Con_Printf("\tskel_model->framegroup_event_code[%d][%d] = %d\n", i, j, skel_model->framegroup_event_code[i][j]);
                Con_Printf("\tskel_model->framegroup_event_data_str[%d][%d] = \"%s\"\n", i, j, skel_model->framegroup_event_data_str[i][j]);
            }
        }
    }

    if(skel_model->fps_cam_bone_idx >= 0) {
        Con_Printf("------------------------------------------------------------\n");
        Con_Printf("Printing fps_cam_bone animation data:\n");
        for(int i = 0; i < skel_model->n_framegroups; i++) {
            for(int frame_idx = skel_model->framegroup_start_frame[i]; frame_idx < skel_model->framegroup_start_frame[i] + skel_model->framegroup_n_frames[i]; frame_idx++) {
                vec3_t *frame_pos = &(skel_model->frames_bone_pos[      skel_model->n_bones * frame_idx + skel_model->fps_cam_bone_idx]);
                quat_t *frame_rot = &(skel_model->frames_bone_rot[      skel_model->n_bones * frame_idx + skel_model->fps_cam_bone_idx]);
                vec3_t *frame_scale = &(skel_model->frames_bone_scale[  skel_model->n_bones * frame_idx + skel_model->fps_cam_bone_idx]);

                Con_Printf("Camera bone transform (bone %d, framegroup %d, frame %d): pos: (%.2f, %.2f, %.2f), rot: (%.2f, %.2f, %.2f, %.2f), scale: (%.2f, %.2f, %.2f)\n",
                    skel_model->fps_cam_bone_idx, i, frame_idx,
                    (*frame_pos)[0], (*frame_pos)[1], (*frame_pos)[2], 
                    (*frame_rot)[0], (*frame_rot)[1], (*frame_rot)[2], (*frame_rot)[3], 
                    (*frame_scale)[0], (*frame_scale)[1], (*frame_scale)[2]
                );
            }
        }
        Con_Printf("------------------------------------------------------------\n");
    }
#endif // IQMDEBUG_LOADIQM_DEBUGSUMMARY
    // ------------------------------------------------------------------------


    // TODO - Need to update this function to work with latest version of skeletal_model_t
#ifdef IQMDEBUG_LOADIQM_RELOCATE
    Con_Printf("About to calculate skeletal model size...\n");
#endif // IQMDEBUG_LOADIQM_RELOCATE
    uint32_t skel_model_n_bytes = count_skel_model_n_bytes(skel_model);
#ifdef IQMDEBUG_LOADIQM_RELOCATE
    Con_Printf("Done calculating skeletal model size\n");
    Con_Printf("Skeletal model size (before padding): %d\n", skel_model_n_bytes);
#endif // IQMDEBUG_LOADIQM_RELOCATE
    // skeletal_model_t *relocatable_skel_model = (skeletal_model_t*) malloc(skel_model_n_bytes); // TODO - Pad to 2-byte alignment?
    // Pad to two bytes:
    // Pad to four bytes:
    skel_model_n_bytes += (skel_model_n_bytes % 4);
#ifdef IQMDEBUG_LOADIQM_RELOCATE
    Con_Printf("Skeletal model size (after padding): %d\n", skel_model_n_bytes);
#endif // IQMDEBUG_LOADIQM_RELOCATE
    // if(skel_model_n_bytes % 2 == 1) {
    //     skel_model_n_bytes += 1;
    // }
    //
    // FIXME - Make sure every memory location is offset by two bytes? Might require modifying flattenmemory method
    //
#ifdef IQMDEBUG_LOADIQM_RELOCATE
    Con_Printf("Allocating memory for skeletal model\n");
#endif // IQMDEBUG_LOADIQM_RELOCATE
    skeletal_model_t *relocatable_skel_model = (skeletal_model_t*) Hunk_AllocName(skel_model_n_bytes, loadname);
#ifdef IQMDEBUG_LOADIQM_RELOCATE
    Con_Printf("Done allocating memory for skeletal model\n");
#endif // IQMDEBUG_LOADIQM_RELOCATE
#ifdef IQMDEBUG_LOADIQM_RELOCATE
    Con_Printf("Flattening skeletal model to make relocatable\n");
#endif // IQMDEBUG_LOADIQM_RELOCATE
    make_skeletal_model_relocatable(relocatable_skel_model, skel_model);
#ifdef IQMDEBUG_LOADIQM_RELOCATE
    Con_Printf("Done flattening skeletal model to make relocatable\n");
#endif // IQMDEBUG_LOADIQM_RELOCATE
    model->type = mod_iqm;


    Con_Printf("About to calculate rest pose mins / maxs\n");
    calc_skel_model_bounds_for_rest_pose(relocatable_skel_model, relocatable_skel_model->rest_pose_bone_hitbox_mins, relocatable_skel_model->rest_pose_bone_hitbox_maxs);
    Con_Printf("Done calculating rest pose mins / maxs:\n");
    Con_Printf("\tmins: [%f, %f, %f]\n", relocatable_skel_model->rest_pose_bone_hitbox_mins[0], relocatable_skel_model->rest_pose_bone_hitbox_mins[1], relocatable_skel_model->rest_pose_bone_hitbox_mins[2]);
    Con_Printf("\tmaxs: [%f, %f, %f]\n", relocatable_skel_model->rest_pose_bone_hitbox_maxs[0], relocatable_skel_model->rest_pose_bone_hitbox_maxs[1], relocatable_skel_model->rest_pose_bone_hitbox_maxs[2]);
    Con_Printf("About to calculate anim pose mins / maxs\n");
    calc_skel_model_bounds_for_anim(relocatable_skel_model, relocatable_skel_model, relocatable_skel_model->anim_bone_hitbox_mins, relocatable_skel_model->anim_bone_hitbox_maxs);
    Con_Printf("Done calculating anim pose mins / maxs:\n");
    Con_Printf("\tmins: [%f, %f, %f]\n", relocatable_skel_model->anim_bone_hitbox_mins[0], relocatable_skel_model->anim_bone_hitbox_mins[1], relocatable_skel_model->anim_bone_hitbox_mins[2]);
    Con_Printf("\tmaxs: [%f, %f, %f]\n", relocatable_skel_model->anim_bone_hitbox_maxs[0], relocatable_skel_model->anim_bone_hitbox_maxs[1], relocatable_skel_model->anim_bone_hitbox_maxs[2]);


#ifdef IQMDEBUG_LOADIQM_RELOCATE
    Con_Printf("Copying final skeletal model struct... ");
#endif // IQMDEBUG_LOADIQM_RELOCATE

    if (!model->cache.data) {
        Cache_Alloc(&model->cache, skel_model_n_bytes, loadname);
    }
    if (!model->cache.data) {
        return;
    }

    memcpy_vfpu(model->cache.data, (void*) relocatable_skel_model, skel_model_n_bytes);

#ifdef IQMDEBUG_LOADIQM_RELOCATE
    Con_Printf("DONE\n");
#endif // IQMDEBUG_LOADIQM_RELOCATE

#ifdef IQMDEBUG_LOADIQM_RELOCATE
    Con_Printf("About to free temp skeletal model struct...");
#endif // IQMDEBUG_LOADIQM_RELOCATE

    free_skeletal_model(skel_model);
    skel_model = nullptr;

#ifdef IQMDEBUG_LOADIQM_RELOCATE
    Con_Printf("DONE\n");
#endif // IQMDEBUG_LOADIQM_RELOCATE
}






void set_skeleton_bone_states_from_model(skeletal_skeleton_t *skeleton, skeletal_model_t *skel_model) {
    bool *model_bone_hitbox_enabled = get_member_from_offset(skel_model->bone_hitbox_enabled, skel_model);
    vec3_t *model_bone_hitbox_ofs = get_member_from_offset(skel_model->bone_hitbox_ofs, skel_model);
    vec3_t *model_bone_hitbox_scale = get_member_from_offset(skel_model->bone_hitbox_scale, skel_model);
    int *model_bone_hitbox_tag = get_member_from_offset(skel_model->bone_hitbox_tag, skel_model);
    for(int i = 0; i < skel_model->n_bones; i++) {
        skeleton->bone_hitbox_enabled[i] = model_bone_hitbox_enabled[i];
        skeleton->bone_hitbox_ofs[i][0] = model_bone_hitbox_ofs[i][0];
        skeleton->bone_hitbox_ofs[i][1] = model_bone_hitbox_ofs[i][1];
        skeleton->bone_hitbox_ofs[i][2] = model_bone_hitbox_ofs[i][2];
        skeleton->bone_hitbox_scale[i][0] = model_bone_hitbox_scale[i][0];
        skeleton->bone_hitbox_scale[i][1] = model_bone_hitbox_scale[i][1];
        skeleton->bone_hitbox_scale[i][2] = model_bone_hitbox_scale[i][2];
        skeleton->bone_hitbox_tag[i] = model_bone_hitbox_tag[i];
    }
}


// 
// Allocates enough memory in this skeleton to store bone states
//
void init_skeleton(skeletal_skeleton_t *skeleton, int skel_model_idx, skeletal_model_t *skel_model) {
    skeleton->modelindex = skel_model_idx;
    skeleton->model = skel_model;
    skeleton->anim_model = nullptr;
    skeleton->bone_transforms = (mat3x4_t*) malloc(sizeof(mat3x4_t) * skel_model->n_bones);
    skeleton->bone_rest_to_pose_transforms = (mat3x4_t*) malloc(sizeof(mat3x4_t) * skel_model->n_bones);

#ifdef IQM_LOAD_NORMALS
    skeleton->bone_rest_to_pose_normal_transforms = (mat3x3_t*) malloc(sizeof(mat3x3_t) * skel_model->n_bones);
#endif // IQM_LOAD_NORMALS

    skeleton->anim_bone_idx = (int32_t*) malloc(sizeof(int32_t) * skel_model->n_bones);

    // Initialize bone enabled states / sizes / ofs / tags from model data
    skeleton->bone_hitbox_enabled = (bool*) malloc(sizeof(bool) * skel_model->n_bones);
    skeleton->bone_hitbox_ofs = (vec3_t*) malloc(sizeof(vec3_t) * skel_model->n_bones);
    skeleton->bone_hitbox_scale = (vec3_t*) malloc(sizeof(vec3_t) * skel_model->n_bones);
    skeleton->bone_hitbox_tag = (int*) malloc(sizeof(int) * skel_model->n_bones);

    set_skeleton_bone_states_from_model(skeleton, skel_model);
}

void free_skeleton(skeletal_skeleton_t *skeleton) {
    skeleton->model = nullptr;
    skeleton->anim_model = nullptr;
    free_pointer_and_clear(skeleton->bone_transforms);
    free_pointer_and_clear(skeleton->bone_rest_to_pose_transforms);

#ifdef IQM_LOAD_NORMALS
    free_pointer_and_clear(skeleton->bone_rest_to_pose_normal_transforms);
#endif // IQM_LOAD_NORMALS

    free_pointer_and_clear(skeleton->anim_bone_idx);
    free_pointer_and_clear(skeleton->bone_hitbox_enabled);
    free_pointer_and_clear(skeleton->bone_hitbox_ofs);
    free_pointer_and_clear(skeleton->bone_hitbox_scale);
    free_pointer_and_clear(skeleton->bone_hitbox_tag);
}


// ------------------------------------------------------------------------
// Calculate skeleton bone -> animation bone mappings
// 
// Skeletons can pull animation data from any animation file, and each 
// skeleton bone is matched to the animation bone by bone name
//
// ------------------------------------------------------------------------
void map_anim_bones_to_skel_bones(skeletal_model_t *skel_model, skeletal_model_t *anim_model, int32_t *skel_bone_to_anim_bone_mapping) {
    // If skeleton and anim model are the same, mapping is: [0, 1, 2, ...]
    if(skel_model == anim_model) {
        for(int32_t i = 0; i < skel_model->n_bones; i++) {
            skel_bone_to_anim_bone_mapping[i] = i;
        }
    }
    // If skeleton model and anim model differ, search through bone names 
    // to find the index that each skeleton bone should pull animation data from
    else {
        char **anim_model_bone_names = get_member_from_offset(anim_model->bone_name, anim_model);
        char **skel_model_bone_names = get_member_from_offset(skel_model->bone_name, skel_model);

        for(uint32_t i = 0; i < skel_model->n_bones; i++) {
            // Default to -1, indicating that this bone's corresponding animation bone has not yet been found
            skel_bone_to_anim_bone_mapping[i] = -1;
            char *skel_model_bone_name = get_member_from_offset(skel_model_bone_names[i], skel_model);
            for(uint32_t j = 0; j < anim_model->n_bones; j++) {
                char *anim_model_bone_name = get_member_from_offset(anim_model_bone_names[j], anim_model);
                if(!strcmp( skel_model_bone_name, anim_model_bone_name)) {
                    skel_bone_to_anim_bone_mapping[i] = j;
                    break;
                }
            }
        }
    }
}



void build_skeleton(skeletal_skeleton_t *skeleton, skeletal_model_t *anim_model, int framegroup_idx, float frametime) {
    if(skeleton == nullptr || anim_model == nullptr) {
        return;
    }
    // ------------------------------------------------------------------------
    // Calculate skeleton bone -> animation bone mappings
    // 
    // Skeletons can pull animation data from any animation file, and each 
    // skeleton bone is matched to the animation bone by bone name
    //
    // ------------------------------------------------------------------------
    // Check if we already have the mapping for this model, if so, skip recalculating it
    if(skeleton->anim_model != anim_model) {
        map_anim_bones_to_skel_bones(skeleton->model, anim_model, skeleton->anim_bone_idx);
        skeleton->anim_model = anim_model;
    }
    // ------------------------------------------------------------------------

    if(framegroup_idx < 0 || framegroup_idx >= anim_model->n_framegroups) {
        return;
    }

    uint32_t *anim_model_framegroup_start_frame = get_member_from_offset(anim_model->framegroup_start_frame, anim_model);
    uint32_t *anim_model_framegroup_n_frames = get_member_from_offset(anim_model->framegroup_n_frames, anim_model);
    float *anim_model_framegroup_fps = get_member_from_offset(anim_model->framegroup_fps, anim_model);
    bool *anim_model_framegroup_loop = get_member_from_offset(anim_model->framegroup_loop, anim_model);


    // Find the two nearest frames to interpolate between
    int frame1_idx = (int) floor(frametime * anim_model_framegroup_fps[framegroup_idx]);
    int frame2_idx = frame1_idx + 1;
    // float delta_frametime = 1.0f / source_model->framegroup_fps[framegroup_idx];
    // float lerpfrac = fmod(frametime, delta_frametime) / delta_frametime;

    if(anim_model_framegroup_loop[framegroup_idx]) {
        frame1_idx = frame1_idx % anim_model_framegroup_n_frames[framegroup_idx];
        frame2_idx = frame2_idx % anim_model_framegroup_n_frames[framegroup_idx];
    }
    else {
        frame1_idx = std::min(std::max(0, frame1_idx), (int) anim_model_framegroup_n_frames[framegroup_idx] - 1);
        frame2_idx = std::min(std::max(0, frame2_idx), (int) anim_model_framegroup_n_frames[framegroup_idx] - 1);
    }

    float lerpfrac;
    if(frame1_idx == frame2_idx) {
        lerpfrac = 0.0f;
    }
    else {
        // FIXME - I suspect there's a simpler way to calculate this... that still relies on frame1_time
        float delta_frametime = 1.0f / anim_model_framegroup_fps[framegroup_idx];
        float anim_duration = delta_frametime * anim_model_framegroup_n_frames[framegroup_idx];
        float frame1_time = frame1_idx * delta_frametime;
        lerpfrac = (fmod(frametime,anim_duration) - frame1_time) / delta_frametime;
        // Floating point math can make it go slightly out of bounds:
        lerpfrac = std::min(std::max(0.0f, lerpfrac), 1.0f);
    }
    frame1_idx = anim_model_framegroup_start_frame[framegroup_idx] + frame1_idx;
    frame2_idx = anim_model_framegroup_start_frame[framegroup_idx] + frame2_idx;
    // TODO - If lerpfrac is 0.0 or 1.0, don't interpolate, just copy the bone poses

    // Con_Printf("build_skel: framegroup %d frames lerp(%d,%d,%.2f) for t=%.2f\n", framegroup_idx, frame1_idx, frame2_idx, lerpfrac, frametime);
    int16_t *bone_parent_idx = get_member_from_offset(skeleton->model->bone_parent_idx, skeleton->model);
    mat3x4_t *bone_rest_transforms = get_member_from_offset(skeleton->model->bone_rest_transforms, skeleton->model);
    mat3x4_t *inv_bone_rest_transforms = get_member_from_offset(skeleton->model->inv_bone_rest_transforms, skeleton->model);

    vec3_t *anim_model_frames_bone_pos = get_member_from_offset(anim_model->frames_bone_pos, anim_model);
    quat_t *anim_model_frames_bone_rot = get_member_from_offset(anim_model->frames_bone_rot, anim_model);
    vec3_t *anim_model_frames_bone_scale = get_member_from_offset(anim_model->frames_bone_scale, anim_model);

    // Build the transform that takes us from model-space to each bone's bone-space for the current pose.
    for(uint32_t i = 0; i < skeleton->model->n_bones; i++) {
        const int anim_bone_idx = skeleton->anim_bone_idx[i];
        // If this bone is not present in the model's animation data, skip it.
        if(anim_bone_idx == -1) {
            Matrix3x4_Copy(skeleton->bone_transforms[i], bone_rest_transforms[i]);
            continue;
        }

        vec3_t *frame1_pos = &(anim_model_frames_bone_pos[anim_model->n_bones * frame1_idx + anim_bone_idx]);
        vec3_t *frame2_pos = &(anim_model_frames_bone_pos[anim_model->n_bones * frame2_idx + anim_bone_idx]);
        quat_t *frame1_rot = &(anim_model_frames_bone_rot[anim_model->n_bones * frame1_idx + anim_bone_idx]);
        quat_t *frame2_rot = &(anim_model_frames_bone_rot[anim_model->n_bones * frame2_idx + anim_bone_idx]);
        vec3_t *frame1_scale = &(anim_model_frames_bone_scale[anim_model->n_bones * frame1_idx + anim_bone_idx]);
        vec3_t *frame2_scale = &(anim_model_frames_bone_scale[anim_model->n_bones * frame2_idx + anim_bone_idx]);

        // Get local bone transforms (relative to parent space)
        vec3_t bone_local_pos;
        quat_t bone_local_rot;
        vec3_t bone_local_scale;
        VectorInterpolate(  *frame1_pos,     lerpfrac,   *frame2_pos,     bone_local_pos);
        QuaternionSlerp(    *frame1_rot,     *frame2_rot, lerpfrac,       bone_local_rot);
        VectorInterpolate(  *frame1_scale,   lerpfrac,   *frame2_scale,   bone_local_scale);

        // Current pose bone-space transform (relative to parent)
        Matrix3x4_scale_rotate_translate(skeleton->bone_transforms[i], bone_local_scale, bone_local_rot, bone_local_pos);

        // if(i == skeleton->model->fps_cam_bone_idx) {
        //     Con_Printf("------------------------------------------\n");
        //     Con_Printf("build_skeleton for fps_cam_bone_idx\n");
        //     Con_Printf("\tPos: (%.2f,%.2f,%.2f) = lerp(a=(%.2f,%.2f,%.2f),b=(%.2f,%.2f,%.2f),t=%.2f)\n",
        //         bone_local_pos[0], bone_local_pos[1], bone_local_pos[2], 
        //         *frame1_pos[0], *frame1_pos[1], *frame1_pos[2], 
        //         *frame2_pos[0], *frame2_pos[1], *frame2_pos[2], 
        //         lerpfrac
        //     );
        //     Con_Printf("\tRot: (%.2f,%.2f,%.2f,%.2f) = lerp(a=(%.2f,%.2f,%.2f,%.2f),b=(%.2f,%.2f,%.2f,%.2f),t=%.2f)\n",
        //         bone_local_rot[0], bone_local_rot[1], bone_local_rot[2], bone_local_rot[3], 
        //         *frame1_rot[0], *frame1_rot[1], *frame1_rot[2], *frame1_rot[3], 
        //         *frame2_rot[0], *frame2_rot[1], *frame2_rot[2], *frame2_rot[3], 
        //         lerpfrac
        //     );
        //     Con_Printf("\tScale: (%.2f,%.2f,%.2f) = lerp(a=(%.2f,%.2f,%.2f),b=(%.2f,%.2f,%.2f),t=%.2f)\n",
        //         bone_local_scale[0], bone_local_scale[1], bone_local_scale[2], 
        //         *frame1_scale[0], *frame1_scale[1], *frame1_scale[2], 
        //         *frame2_scale[0], *frame2_scale[1], *frame2_scale[2], 
        //         lerpfrac
        //     );
        //     Con_Printf("Bone local-space transform matrix:\n");
        //     mat3x4_t *print_mat = &(skeleton->bone_transforms[i]);
        //     Con_Printf("\t\t\t[%.2f, %.2f, %.2f, %.2f]\n", *print_mat[0][0], *print_mat[0][1], *print_mat[0][2], *print_mat[0][3]);
        //     Con_Printf("\t\t\t[%.2f, %.2f, %.2f, %.2f]\n", *print_mat[1][0], *print_mat[1][1], *print_mat[1][2], *print_mat[1][3]);
        //     Con_Printf("\t\t\t[%.2f, %.2f, %.2f, %.2f]\n", *print_mat[2][0], *print_mat[2][1], *print_mat[2][2], *print_mat[2][3]);
        //     Con_Printf("\t\t\t[%.2f, %.2f, %.2f, %.2f]\n", 0.0f, 0.0f, 0.0f, 1.0f);
        //     Con_Printf("------------------------------------------\n");
        // }

        // Con_Printf("\tBuild skel bone posed transform components %d:\n", i);
        // Con_Printf("\t\tPos: [%f, %f, %f]\n", bone_local_pos[0], bone_local_pos[1], bone_local_pos[2]);
        // Con_Printf("\t\tRot: [%f, %f, %f, %f]\n", bone_local_rot[0], bone_local_rot[1], bone_local_rot[2], bone_local_rot[3]);
        // Con_Printf("\t\tScale: [%f, %f, %f]\n", bone_local_scale[0], bone_local_scale[1], bone_local_scale[2]);
        // Con_Printf("\tBuild skel bone posed transform %d:\n", i);
        // Con_Printf("\t\t[%f, %f, %f, %f]\n", skeleton->bone_transforms[i][0][0], skeleton->bone_transforms[i][0][1], skeleton->bone_transforms[i][0][2], skeleton->bone_transforms[i][0][3]);
        // Con_Printf("\t\t[%f, %f, %f, %f]\n", skeleton->bone_transforms[i][1][0], skeleton->bone_transforms[i][1][1], skeleton->bone_transforms[i][1][2], skeleton->bone_transforms[i][1][3]);
        // Con_Printf("\t\t[%f, %f, %f, %f]\n", skeleton->bone_transforms[i][2][0], skeleton->bone_transforms[i][2][1], skeleton->bone_transforms[i][2][2], skeleton->bone_transforms[i][2][3]);
        // Con_Printf("\t\t[%f, %f, %f, %f]\n", 0.0f, 0.0f, 0.0f, 1.0f);


        // If we have a parent, concat parent transform to get model-space transform
        int parent_bone_idx = bone_parent_idx[i];
        if(parent_bone_idx >= 0) {
            mat3x4_t temp; 
            Matrix3x4_ConcatTransforms(temp, skeleton->bone_transforms[parent_bone_idx], skeleton->bone_transforms[i]);
            Matrix3x4_Copy(skeleton->bone_transforms[i], temp);
        }
        // If we don't have a parent, the bone-space transform _is_ the model-space transform

        // Con_Printf("\tBuild skel bone posed transform %d with parent concat:\n", i);
        // Con_Printf("\t\t[%f, %f, %f, %f]\n", skeleton->bone_transforms[i][0][0], skeleton->bone_transforms[i][0][1], skeleton->bone_transforms[i][0][2], skeleton->bone_transforms[i][0][3]);
        // Con_Printf("\t\t[%f, %f, %f, %f]\n", skeleton->bone_transforms[i][1][0], skeleton->bone_transforms[i][1][1], skeleton->bone_transforms[i][1][2], skeleton->bone_transforms[i][1][3]);
        // Con_Printf("\t\t[%f, %f, %f, %f]\n", skeleton->bone_transforms[i][2][0], skeleton->bone_transforms[i][2][1], skeleton->bone_transforms[i][2][2], skeleton->bone_transforms[i][2][3]);
        // Con_Printf("\t\t[%f, %f, %f, %f]\n", 0.0f, 0.0f, 0.0f, 1.0f);
    }

    // Now that all bone transforms have been computed for the current pose, multiply in the inverse rest pose transforms
    // These transforms will take the vertices from model-space to each bone's local-space:
    for(uint32_t i = 0; i < anim_model->n_bones; i++) {
        Matrix3x4_ConcatTransforms( skeleton->bone_rest_to_pose_transforms[i], skeleton->bone_transforms[i], inv_bone_rest_transforms[i]);


#ifdef IQM_LOAD_NORMALS
        // Invert-transpose the upper-left 3x3 matrix to get the transform that is applied to vertex normals
        // FIXME - Do we even need this? We make no user of this matrix on dquake
        Matrix3x3_Invert_Transposed_Matrix3x4(skeleton->bone_rest_to_pose_normal_transforms[i], skeleton->bone_rest_to_pose_transforms[i]);
#endif // IQM_LOAD_NORMALS


        // Con_Printf("\tBuild skel final bone transform %d:\n", i);
        // Con_Printf("\t\t[%f, %f, %f, %f]\n", skeleton->bone_rest_to_pose_transforms[i][0][0], skeleton->bone_rest_to_pose_transforms[i][0][1], skeleton->bone_rest_to_pose_transforms[i][0][2], skeleton->bone_rest_to_pose_transforms[0][3]);
        // Con_Printf("\t\t[%f, %f, %f, %f]\n", skeleton->bone_rest_to_pose_transforms[i][1][0], skeleton->bone_rest_to_pose_transforms[i][1][1], skeleton->bone_rest_to_pose_transforms[i][1][2], skeleton->bone_rest_to_pose_transforms[1][3]);
        // Con_Printf("\t\t[%f, %f, %f, %f]\n", skeleton->bone_rest_to_pose_transforms[i][2][0], skeleton->bone_rest_to_pose_transforms[i][2][1], skeleton->bone_rest_to_pose_transforms[i][2][2], skeleton->bone_rest_to_pose_transforms[2][3]);
        // Con_Printf("\t\t[%f, %f, %f, %f]\n", 0.0f, 0.0f, 0.0f, 1.0f);
    }
}


void build_skeleton_for_start_time(skeletal_skeleton_t *skeleton, skeletal_model_t *anim_model, int framegroup_idx, float anim_starttime, float anim_speed) {
    float anim_frametime;
    // If anim_speed is 0, `anim_starttime` instead contains the `anim_frametime` that the animation will be paused at
    if(anim_speed == 0.0f) {
        anim_frametime = anim_starttime;
    }
    else {
        anim_frametime = (sv.time - anim_starttime) * anim_speed;
    }
    build_skeleton(skeleton, anim_model, framegroup_idx, anim_frametime);
}


void bind_submesh_bones(skeletal_mesh_t *submesh, skeletal_skeleton_t *skeleton) {
    // Transform matrix that undoes int16 / int8 quantization scale + ofs
    ScePspFMatrix4 undo_quantization;
    gumLoadIdentity(&undo_quantization);
    ScePspFVector3 undo_ofs = {submesh->verts_ofs[0],submesh->verts_ofs[1],submesh->verts_ofs[2]};
    ScePspFVector3 undo_scale = {submesh->verts_scale[0],submesh->verts_scale[1],submesh->verts_scale[2]};
    gumTranslate(&undo_quantization, &undo_ofs);
    gumScale(&undo_quantization, &undo_scale);

    // If no skeleton provided, only undo quantization (don't pose the model)
    if(skeleton == nullptr) {
        for(int submesh_bone_idx = 0; submesh_bone_idx < submesh->n_skinning_bones; submesh_bone_idx++) {
            sceGuBoneMatrix(submesh_bone_idx, &undo_quantization);
        }
        return;
    }
    
    for(int submesh_bone_idx = 0; submesh_bone_idx < submesh->n_skinning_bones; submesh_bone_idx++) {
        // Get the index into the skeleton list of bones:
        int bone_idx = submesh->skinning_bone_idxs[submesh_bone_idx];
        // mat3x4_t *bone_mat3x4 = &bone_rest_transforms[bone_idx];
        mat3x4_t *bone_mat3x4 = &(skeleton->bone_rest_to_pose_transforms[bone_idx]);


        // Con_Printf("\tBind submesh bone %d (skel bone: %d)\n", submesh_bone_idx, bone_idx);
        // Con_Printf("\t\t[%f, %f, %f, %f]\n", skeleton->bone_rest_to_pose_transforms[bone_idx][0][0], skeleton->bone_rest_to_pose_transforms[bone_idx][0][1], skeleton->bone_rest_to_pose_transforms[bone_idx][0][2], skeleton->bone_rest_to_pose_transforms[bone_idx][0][3]);
        // Con_Printf("\t\t[%f, %f, %f, %f]\n", skeleton->bone_rest_to_pose_transforms[bone_idx][1][0], skeleton->bone_rest_to_pose_transforms[bone_idx][1][1], skeleton->bone_rest_to_pose_transforms[bone_idx][1][2], skeleton->bone_rest_to_pose_transforms[bone_idx][1][3]);
        // Con_Printf("\t\t[%f, %f, %f, %f]\n", skeleton->bone_rest_to_pose_transforms[bone_idx][2][0], skeleton->bone_rest_to_pose_transforms[bone_idx][2][1], skeleton->bone_rest_to_pose_transforms[bone_idx][2][2], skeleton->bone_rest_to_pose_transforms[bone_idx][2][3]);
        // Con_Printf("\t\t[%f, %f, %f, %f]\n", 0.0f, 0.0f, 0.0f, 1.0f);
        // Con_Printf("\t---\n");
        // Con_Printf("\t\t[%f, %f, %f, %f]\n", (*bone_mat3x4)[0][0], (*bone_mat3x4)[0][1], (*bone_mat3x4)[0][2], (*bone_mat3x4)[0][3]);
        // Con_Printf("\t\t[%f, %f, %f, %f]\n", (*bone_mat3x4)[1][0], (*bone_mat3x4)[1][1], (*bone_mat3x4)[1][2], (*bone_mat3x4)[1][3]);
        // Con_Printf("\t\t[%f, %f, %f, %f]\n", (*bone_mat3x4)[2][0], (*bone_mat3x4)[2][1], (*bone_mat3x4)[2][2], (*bone_mat3x4)[2][3]);
        // Con_Printf("\t\t[%f, %f, %f, %f]\n", 0.0f, 0.0f, 0.0f, 1.0f);


        // Translate the mat3x4_t bone transform matrix to ScePspFMatrix4
        ScePspFMatrix4 bone_mat;
        // Matrix3x4_to_ScePspFMatrix4(bone_mat, skeleton->bone_rest_to_pose_transforms[bone_idx]);
        Matrix3x4_to_ScePspFMatrix4(&bone_mat, bone_mat3x4);

        // Con_Printf("\t---\n");
        // Con_Printf("\t\t[%f, %f, %f, %f]\n", bone_mat.x.x, bone_mat.y.x, bone_mat.z.x, bone_mat.w.x);
        // Con_Printf("\t\t[%f, %f, %f, %f]\n", bone_mat.x.y, bone_mat.y.y, bone_mat.z.y, bone_mat.w.y);
        // Con_Printf("\t\t[%f, %f, %f, %f]\n", bone_mat.x.z, bone_mat.y.z, bone_mat.z.z, bone_mat.w.z);
        // Con_Printf("\t\t[%f, %f, %f, %f]\n", bone_mat.x.w, bone_mat.y.w, bone_mat.z.w, bone_mat.w.w);

        // bone_mat = bone_mat * undo_quantization
        gumMultMatrix(&bone_mat, &bone_mat, &undo_quantization);
        sceGuBoneMatrix(submesh_bone_idx, &bone_mat);
    }
}


// 
// Calculate collision of ray with unit-size AABB (Centered at origin, size: 1.0)
// 
// https://tavianator.com/cgit/dimension.git/tree/libdimension/bvh/bvh.c#n196
//
bool ray_aabb_intersection(vec3_t ray_pos, vec3_t ray_dir, float &tmin) {
    vec3_t box_mins = {-0.5, -0.5, -0.5};
    vec3_t box_maxs = {0.5, 0.5, 0.5};

    vec3_t inv_ray_dir = {
        1.0f / ray_dir[0], 
        1.0f / ray_dir[1], 
        1.0f / ray_dir[2]
    };

    // float tmin, tmax;
    float tmax;

    float tx1 = (box_mins[0] - ray_pos[0]) * inv_ray_dir[0];
    float tx2 = (box_maxs[0] - ray_pos[0]) * inv_ray_dir[0];
    tmin = std::min(tx1, tx2);
    tmax = std::max(tx1, tx2);

    float ty1 = (box_mins[1] - ray_pos[1]) * inv_ray_dir[1];
    float ty2 = (box_maxs[1] - ray_pos[1]) * inv_ray_dir[1];
    tmin = std::max(tmin, std::min(ty1, ty2));
    tmax = std::min(tmax, std::max(ty1, ty2));

    float tz1 = (box_mins[2] - ray_pos[2]) * inv_ray_dir[2];
    float tz2 = (box_maxs[2] - ray_pos[2]) * inv_ray_dir[2];
    tmin = std::max(tmin, std::min(tz1, tz2));
    tmax = std::min(tmax, std::max(tz1, tz2));

    // return tmax >= std::max(0.0f, tmin);
    return tmax >= std::max(0.0f, tmin);
}


//
// Returns the model index for a given model name, if it exists
// If it doesn't exist, or isn't precached, index 0 is returned.
// NOTE: Index 0 technically corresponds to the world bsp model. But for the 
// NOTE  context of this function, it indicates an invalid value, for compat
// NOTE  with FTE's convention
//
void PF_getmodelindex(void) {
    // Con_Printf("PF_getmodelindex start\n");
    // char *model_name = G_STRING(OFS_PARM0);
    char *model_name = PF_VarString(0);
    int model_idx = 0; // Default return value for a model not found

    for(int i = 1; (sv.model_precache[i] != NULL) && (i < MAX_MODELS); i++) {
        if(!strcmp(sv.model_precache[i], model_name)) {
            model_idx = i;
            break;
        }
    }
    G_FLOAT(OFS_RETURN) = model_idx;
    // Con_Printf("PF_getmodelindex end\n");
}


void sv_skel_clear_all() {
    for(int i = 0; i < MAX_SKELETONS; i++) {
        sv_skeletons[i].in_use = false;
        sv_skeletons[i].modelindex = 0;
        sv_skeletons[i].model = nullptr;
        sv_skeletons[i].anim_modelindex = 0;
        sv_skeletons[i].anim_framegroup = 0;
        sv_skeletons[i].anim_starttime = 0.0f;
        sv_skeletons[i].anim_speed = 0.0f;
        sv_skeletons[i].last_build_time = 0.0f;
        sv_skeletons[i].bone_hitbox_enabled = nullptr;
        sv_skeletons[i].bone_hitbox_ofs = nullptr;
        sv_skeletons[i].bone_hitbox_scale = nullptr;
        sv_skeletons[i].bone_hitbox_tag = nullptr;
        sv_skeletons[i].anim_model = nullptr;
        sv_skeletons[i].bone_transforms = nullptr;
        sv_skeletons[i].bone_rest_to_pose_transforms = nullptr;

#ifdef IQM_LOAD_NORMALS
        sv_skeletons[i].bone_rest_to_pose_normal_transforms = nullptr;
#endif // IQM_LOAD_NORMALS

        sv_skeletons[i].anim_bone_idx = nullptr;
    }
    sv_n_skeletons = 0;
}


char *sv_skel_get_bonename(int skel_idx, int bone_idx) {
    if(skel_idx < 0 || skel_idx >= MAX_SKELETONS) {
        return nullptr;
    }

    if(sv_skeletons[skel_idx].in_use == false) {
        return nullptr;
    }

    if(bone_idx < 0 || bone_idx >= sv_skeletons[skel_idx].model->n_bones) {
        return nullptr;
    }
    
    skeletal_skeleton_t *sv_skeleton = &(sv_skeletons[skel_idx]);
    char **bone_names = get_member_from_offset(sv_skeleton->model->bone_name, sv_skeleton->model);
    char *bone_name = get_member_from_offset(bone_names[bone_idx], sv_skeleton->model);
    return bone_name;
}



void PF_skel_get_bonename(void) {
    Con_Printf("PF_skel_get_bonename start\n");
    int skel_idx = G_FLOAT(OFS_PARM0);
    int bone_idx = G_FLOAT(OFS_PARM1);
    char *bone_name = sv_skel_get_bonename(skel_idx, bone_idx);

    // ------------------------------------
    // Engine --> QC string handling
    // ------------------------------------
    int bone_name_len = strlen(bone_name);
    if(bone_name_len >= PR_MAX_TEMPSTRING) {
        bone_name_len = PR_MAX_TEMPSTRING - 1;
    }
    if(bone_name == nullptr || bone_name_len < 0) {
        bone_name_len = 0;
    }
    strncpy(pr_string_temp, bone_name, bone_name_len);
    // Null-terminate the string
    pr_string_temp[bone_name_len] = 0;
    // Send offset to temp string in global strings list
	G_INT(OFS_RETURN) = pr_string_temp - pr_strings;
    // ------------------------------------
    Con_Printf("PF_skel_get_bonename end\n");
};



int16_t sv_skel_get_boneparent(int skel_idx, int bone_idx) {
    if(skel_idx < 0 || skel_idx >= MAX_SKELETONS) {
        return -1;
    }

    if(sv_skeletons[skel_idx].in_use == false) {
        return -1;
    }

    if(bone_idx < 0 || bone_idx >= sv_skeletons[skel_idx].model->n_bones) {
        return -1;
    }
    
    skeletal_skeleton_t *sv_skeleton = &(sv_skeletons[skel_idx]);
    int16_t *bone_parent_idxs = get_member_from_offset(sv_skeleton->model->bone_parent_idx, sv_skeleton->model);
    return bone_parent_idxs[bone_idx];
}

void PF_skel_get_boneparent(void) {
    Con_Printf("PF_skel_get_boneparent start\n");
    int skel_idx = G_FLOAT(OFS_PARM0);
    int bone_idx = G_FLOAT(OFS_PARM1);
    int16_t parent_bone_idx = sv_skel_get_boneparent(skel_idx, bone_idx);
    G_FLOAT(OFS_RETURN) = parent_bone_idx;
    Con_Printf("PF_skel_get_boneparent end\n");
};



int16_t sv_skel_find_bone(int skel_idx, char *bone_name) {
    if(skel_idx < 0 || skel_idx >= MAX_SKELETONS) {
        return -1;
    }

    if(sv_skeletons[skel_idx].in_use == false) {
        return -1;
    }

    int16_t bone_idx = -1;
    skeletal_skeleton_t *sv_skeleton = &(sv_skeletons[skel_idx]);
    char **bone_names = get_member_from_offset(sv_skeleton->model->bone_name, sv_skeleton->model);

    for(int i = 0; i < sv_skeleton->model->n_bones; i++) {
        char *cur_bone_name = get_member_from_offset(bone_names[i], sv_skeleton->model);
        if(!strcmp( cur_bone_name, bone_name)) {
            bone_idx = i;
            break;
        }
    }
    return bone_idx;
}


void PF_skel_find_bone(void) {
    Con_Printf("PF_skel_find_bone start\n");
    int skel_idx = G_FLOAT(OFS_PARM0);
    // char *bone_name = G_STRING(OFS_PARM1);
    char *bone_name = PF_VarString(1);
    int16_t bone_idx = sv_skel_find_bone(skel_idx, bone_name);
    G_FLOAT(OFS_RETURN) = bone_idx;
    Con_Printf("PF_skel_find_bone end\n");
}


#define SKEL_BONE_SELECT_TYPE_BY_IDX 0
#define SKEL_BONE_SELECT_TYPE_BY_HITBOX_TAG 1



void PF_skel_set_bone_hitbox_enabled(void) {
    Con_Printf("PF_skel_set_bone_hitbox_enabled start\n");
    int skel_idx = G_FLOAT(OFS_PARM0);
    int bones_spec = G_FLOAT(OFS_PARM1);
    bool val = G_FLOAT(OFS_PARM2);
    int bones_spec_type = G_FLOAT(OFS_PARM3);

    if(skel_idx < 0 || skel_idx >= MAX_SKELETONS) {
        return;
    }
    if(sv_skeletons[skel_idx].in_use == false) {
        return;
    }

    // If selecting bone by bone index, set that bone
    if(bones_spec_type == SKEL_BONE_SELECT_TYPE_BY_IDX) {
        int bone_idx = bones_spec;
        if(bone_idx < 0 || bone_idx >= sv_skeletons[skel_idx].model->n_bones) {
            return;
        }
        sv_skeletons[skel_idx].bone_hitbox_enabled[bone_idx] = val;
    }
    // If selecting bones by tag value, set value for bones whose tag value matches
    else if(bones_spec_type == SKEL_BONE_SELECT_TYPE_BY_HITBOX_TAG) {
        int bone_tag = bones_spec;

        for(int i = 0; i < sv_skeletons[skel_idx].model->n_bones; i++) {
            if(sv_skeletons[skel_idx].bone_hitbox_tag[i] == bone_tag) {
                sv_skeletons[skel_idx].bone_hitbox_enabled[i] = val;
            }
        }
    }
    Con_Printf("PF_skel_set_bone_hitbox_enabled end\n");
}

void PF_skel_set_bone_hitbox_ofs(void) {
    Con_Printf("PF_skel_set_bone_hitbox_ofs start\n");
    int skel_idx = G_FLOAT(OFS_PARM0);
    int bones_spec = G_FLOAT(OFS_PARM1);
    float *ofs_vec = G_VECTOR(OFS_PARM2);
    int bones_spec_type = G_FLOAT(OFS_PARM3);

    if(skel_idx < 0 || skel_idx >= MAX_SKELETONS) {
        return;
    }
    if(sv_skeletons[skel_idx].in_use == false) {
        return;
    }

    // If selecting bone by bone index, set that bone
    if(bones_spec_type == SKEL_BONE_SELECT_TYPE_BY_IDX) {
        int bone_idx = bones_spec;
        if(bone_idx < 0 || bone_idx >= sv_skeletons[skel_idx].model->n_bones) {
            return;
        }
        sv_skeletons[skel_idx].bone_hitbox_ofs[bone_idx][0] = ofs_vec[0];
        sv_skeletons[skel_idx].bone_hitbox_ofs[bone_idx][1] = ofs_vec[1];
        sv_skeletons[skel_idx].bone_hitbox_ofs[bone_idx][2] = ofs_vec[2];
    }
    // If selecting bones by tag value, set value for bones whose tag value matches
    else if(bones_spec_type == SKEL_BONE_SELECT_TYPE_BY_HITBOX_TAG) {
        int bone_tag = bones_spec;
        for(int i = 0; i < sv_skeletons[skel_idx].model->n_bones; i++) {
            if(sv_skeletons[skel_idx].bone_hitbox_tag[i] == bone_tag) {
                sv_skeletons[skel_idx].bone_hitbox_ofs[i][0] = ofs_vec[0];
                sv_skeletons[skel_idx].bone_hitbox_ofs[i][1] = ofs_vec[1];
                sv_skeletons[skel_idx].bone_hitbox_ofs[i][2] = ofs_vec[2];
            }
        }
    }
    Con_Printf("PF_skel_set_bone_hitbox_ofs end\n");
}

void PF_skel_set_bone_hitbox_scale(void) {
    Con_Printf("PF_skel_set_bone_hitbox_scale start\n");
    int skel_idx = G_FLOAT(OFS_PARM0);
    int bones_spec = G_FLOAT(OFS_PARM1);
    float *scale_vec = G_VECTOR(OFS_PARM2);
    int bones_spec_type = G_FLOAT(OFS_PARM3);

    if(skel_idx < 0 || skel_idx >= MAX_SKELETONS) {
        return;
    }
    if(sv_skeletons[skel_idx].in_use == false) {
        return;
    }

    // If selecting bone by bone index, set that bone
    if(bones_spec_type == SKEL_BONE_SELECT_TYPE_BY_IDX) {
        int bone_idx = bones_spec;
        if(bone_idx < 0 || bone_idx >= sv_skeletons[skel_idx].model->n_bones) {
            return;
        }
        sv_skeletons[skel_idx].bone_hitbox_scale[bone_idx][0] = scale_vec[0];
        sv_skeletons[skel_idx].bone_hitbox_scale[bone_idx][1] = scale_vec[1];
        sv_skeletons[skel_idx].bone_hitbox_scale[bone_idx][2] = scale_vec[2];
    }
    // If selecting bones by tag value, set value for bones whose tag value matches
    else if(bones_spec_type == SKEL_BONE_SELECT_TYPE_BY_HITBOX_TAG) {
        int bone_tag = bones_spec;
        for(int i = 0; i < sv_skeletons[skel_idx].model->n_bones; i++) {
            if(sv_skeletons[skel_idx].bone_hitbox_tag[i] == bone_tag) {
                sv_skeletons[skel_idx].bone_hitbox_scale[i][0] = scale_vec[0];
                sv_skeletons[skel_idx].bone_hitbox_scale[i][1] = scale_vec[1];
                sv_skeletons[skel_idx].bone_hitbox_scale[i][2] = scale_vec[2];
            }
        }
    }
    Con_Printf("PF_skel_set_bone_hitbox_scale end\n");
};


void PF_skel_set_bone_hitbox_tag(void) {
    Con_Printf("PF_skel_set_bone_hitbox_tag start\n");
    int skel_idx = G_FLOAT(OFS_PARM0);
    int bones_spec = G_FLOAT(OFS_PARM1);
    int tag_value = G_FLOAT(OFS_PARM2);
    int bones_spec_type = G_FLOAT(OFS_PARM3);

    if(skel_idx < 0 || skel_idx >= MAX_SKELETONS) {
        return;
    }
    if(sv_skeletons[skel_idx].in_use == false) {
        return;
    }

    // If selecting bone by bone index, set that bone
    if(bones_spec_type == SKEL_BONE_SELECT_TYPE_BY_IDX) {
        int bone_idx = bones_spec;
        if(bone_idx < 0 || bone_idx >= sv_skeletons[skel_idx].model->n_bones) {
            return;
        }
        sv_skeletons[skel_idx].bone_hitbox_tag[bone_idx] = tag_value;
    }
    // If selecting bones by tag value, set value for bones whose tag value matches
    else if(bones_spec_type == SKEL_BONE_SELECT_TYPE_BY_HITBOX_TAG) {
        int bone_tag = bones_spec;
        for(int i = 0; i < sv_skeletons[skel_idx].model->n_bones; i++) {
            if(sv_skeletons[skel_idx].bone_hitbox_tag[i] == bone_tag) {
                sv_skeletons[skel_idx].bone_hitbox_tag[i] = tag_value;
            }
        }
    }
    Con_Printf("PF_skel_set_bone_hitbox_tag end\n");
}

void PF_skel_reset_bone_hitboxes(void) {
    Con_Printf("PF_skel_reset_bone_hitboxes start\n");
    int skel_idx = G_FLOAT(OFS_PARM0);
    if(skel_idx < 0 || skel_idx >= MAX_SKELETONS) {
        return;
    }
    if(sv_skeletons[skel_idx].in_use == false) {
        return;
    }
    set_skeleton_bone_states_from_model(&(sv_skeletons[skel_idx]), sv_skeletons[skel_idx].model);
    Con_Printf("PF_skel_reset_bone_hitboxes end\n");
}



//
// Returns the skeletal model with index `model_idx`
// Returns nullptr if invalid index or if not a skeletal model.
//
skeletal_model_t *sv_get_skel_model_for_modelidx(int model_idx) {

    if(model_idx < 0 || model_idx >= MAX_MODELS) {
        Con_DPrintf("Model idx out of bounds: %d, return -1\n", model_idx);
        return nullptr;
    }

    // Safety check to make sure model at index is valid
    // char *model_name = sv.model_precache[model_idx];
    // model_t *model = Mod_ForName(model_name, qfalse);

    // No safety check
    model_t *model = sv.models[model_idx];

    if(model->type != mod_iqm) {
        Con_DPrintf("Model type is not IQM: model: %d, type: %d != %d, return -1\n", model_idx, model->type, mod_iqm);
        return nullptr;
    }
    skeletal_model_t *skel_model = static_cast<skeletal_model_t*>(Mod_Extradata(model));
    return skel_model;
}


//
// Returns the skeletal model with index `model_idx`
// Returns nullptr otherwise if invalid or if not a skeletal model.
//
skeletal_model_t *cl_get_skel_model_for_modelidx(int model_idx) {

    if(model_idx < 0 || model_idx >= MAX_MODELS) {
        Con_DPrintf("Model idx out of bounds: %d, return -1\n", model_idx);
        return nullptr;
    }

    // Safety check to make sure model at index is valid
    model_t *model = cl.model_precache[model_idx];
    if(model == nullptr) {
        Con_DPrintf("Model NULL at index: %d, return -1\n", model_idx);
        return nullptr;
    }
    if(model->type != mod_iqm) {
        Con_DPrintf("Model type is not IQM: model: %d != %d, type: %d != %d, return -1\n", model_idx, model->type, mod_iqm);
        return nullptr;
    }
    skeletal_model_t *skel_model = static_cast<skeletal_model_t*>(Mod_Extradata(model));
    return skel_model;
}



//
// Returns the duration of an IQM animation framegroup in seconds
//
void PF_getframeduration(void) {
    // Con_Printf("PF_getframeduration start\n");
    int anim_modelindex = G_FLOAT(OFS_PARM0);
    int anim_framegroup_idx = G_FLOAT(OFS_PARM1);

    skeletal_model_t *anim_skel_model = sv_get_skel_model_for_modelidx(anim_modelindex);
    if(anim_skel_model == nullptr) {
        Con_Printf("PF_skeleton_build: Invalid skeletal animation model\n");
        G_FLOAT(OFS_RETURN) = 0.0f;
        return;
    }

    if(anim_framegroup_idx < 0 || anim_framegroup_idx >= anim_skel_model->n_framegroups) {
        G_FLOAT(OFS_RETURN) = 0.0f;
        return;
    }

    uint32_t *framegroup_n_frames = get_member_from_offset(anim_skel_model->framegroup_n_frames, anim_skel_model);
    float *framegroup_fps = get_member_from_offset(anim_skel_model->framegroup_fps, anim_skel_model);


    float anim_duration = framegroup_n_frames[anim_framegroup_idx] / framegroup_fps[anim_framegroup_idx];
    G_FLOAT(OFS_RETURN) = anim_duration;
    // Con_Printf("PF_getframeduration end\n");
}

void run_model_event_callback(float timestamp, int event_code, char *event_data, func_t callback_func) {

    Con_Printf("Running callback with args (t=%f, c=%d, d=\"%s\")\n",timestamp, event_code, event_data);
    G_FLOAT(OFS_PARM0) = timestamp;
    G_FLOAT(OFS_PARM1) = event_code;

    // ------------------------------------
    // Engine --> QC string handling
    // ------------------------------------
    int event_data_len = strlen(event_data);
    if(event_data_len >= PR_MAX_TEMPSTRING) {
        event_data_len = PR_MAX_TEMPSTRING - 1;
    }
    if(event_data == nullptr || event_data_len < 0) {
        event_data_len = 0;
    }
    strncpy(pr_string_temp, event_data, event_data_len);
    // Null-terminate the string
    pr_string_temp[event_data_len] = 0;
    // Con_Printf("\ttempstring content: \"%s\"\n", pr_string_temp);
    G_INT(OFS_PARM2) = pr_string_temp - pr_strings;

    // ----------------------
    // char temp_str[256];
    // int event_data_len = 0;
    // if(event_data != nullptr) {
    //     event_data_len = strlen(event_data);
    //     if(event_data_len >= 256) {
    //         event_data_len = 255;
    //     }
    //     if(event_data_len < 0) {
    //         event_data_len = 0;
    //     }
    //     strncpy(temp_str, event_data, event_data_len);
    // }
    // // Null-terminate the string
    // temp_str[event_data_len] = 0;
    // G_INT(OFS_PARM2) = temp_str - pr_strings;
    // ----------------------

    // Send offset to temp string in global strings list
    // -----------
    // This works
    // -----------
    // G_INT(OFS_RETURN) = pr_string_temp - pr_strings;
    // -----------
    // G_INT(OFS_PARM1) = pr_string_temp - pr_strings;

	// strcpy(tempc, Cmd_Argv(G_FLOAT(OFS_PARM0)));
    // G_INT(OFS_PARM3) = local_str - pr_strings;

    // G_STRING(OFS_PARM2) = pr_string_temp; // Compile error
    // G_STRING(OFS_PARM2) = pr_string_temp - pr_strings; // Compile error
    // G_INT(OFS_PARM3) = pr_string_temp - pr_strings;
    // G_INT(OFS_PARM4) = pr_string_temp - pr_strings;
    // ------------------------------------


    // TODO - Set context entity so "self" / time is correct
    // pr_global_struct->time = sv.time;
	// pr_global_struct->self = EDICT_TO_PROG(sv_player);
    PR_ExecuteProgram(callback_func);
    // Con_Printf("\tafter -- tempstring content: \"%s\"\n", pr_string_temp);
}





// 
// Returns the total distance walked between the beginning of frame 0 and the beginning of frame `frame_idx`
//
float _dist_walked_at_frame_idx(int frame_idx, float *frames_move_dist, int n_frames, bool anim_looped) {
    if(frame_idx <= 0) {
        return 0.0f;
    }

    if(!anim_looped) {
        if(frame_idx >= n_frames) {
            return frames_move_dist[n_frames - 1];
        }
        return frames_move_dist[frame_idx - 1];
    }

    // Add in the total distance walked for each loop iteration of the full animation
    float dist_walked = frames_move_dist[n_frames - 1] * (int) (frame_idx / n_frames);
    // Add in the total distance walked for the final loop iteration
    int cur_loop_frame = frame_idx % n_frames;
    if(cur_loop_frame >= 0) {
        dist_walked += frames_move_dist[cur_loop_frame - 1];
    }
    return dist_walked;
}

//
// Returns the total distance walked between t=0 and t=`t`
//
float _dist_walked_at_frametime(float t, float *frames_move_dist, float framerate, int n_frames, bool anim_looped) {
    int frame_idx_before = (int) (t * framerate);
    int frame_idx_after = frame_idx_before + 1;

    float frametime_before = frame_idx_before / framerate;


    // Let's double check this math...
    // Let's define the distance walked at some frame is the distance walked at the _start_ of the frame


    float dist_walked_before = _dist_walked_at_frame_idx(frame_idx_before, frames_move_dist, n_frames, anim_looped);
    float dist_walked_after = _dist_walked_at_frame_idx(frame_idx_after, frames_move_dist, n_frames, anim_looped);

    // Lerp between `dist_walked_before` and `dist_walked_after`
    float lerp_frac = (t - frametime_before) * framerate;

    float dist_walked = dist_walked_before + (dist_walked_after - dist_walked_before) * lerp_frac;

    // Con_Printf("\t_dist_walked_at_frametime: %f (fps: %f) frames: [%d, %d], time at frame before: %f, dist: (%f, %f), lerp: %f, dist: %f\n", 
    //     t, framerate, frame_idx_before, frame_idx_after, frametime_before, 
    //     dist_walked_before, dist_walked_after, 
    //     lerp_frac, dist_walked);
    return dist_walked;
}


//
// Returns the distance walked between two frametimes
//
void PF_getmovedistance(void) {

    int anim_modelindex = G_FLOAT(OFS_PARM0);
    int anim_framegroup_idx = G_FLOAT(OFS_PARM1);
    float t_start = G_FLOAT(OFS_PARM2);
    float t_stop = G_FLOAT(OFS_PARM3);
    // TODO - param to control looping behavior?
    G_FLOAT(OFS_RETURN) = 0.0f;


    if(t_start >= t_stop) {
        return;
    }

    skeletal_model_t *anim_skel_model = sv_get_skel_model_for_modelidx(anim_modelindex);
    if(anim_skel_model == nullptr) {
        return;
    }

    if(anim_framegroup_idx < 0 || anim_framegroup_idx >= anim_skel_model->n_framegroups) {
        return;
    }

    uint32_t *framegroup_start_frame = get_member_from_offset(anim_skel_model->framegroup_start_frame, anim_skel_model);
    uint32_t *framegroup_n_frames = get_member_from_offset(anim_skel_model->framegroup_n_frames, anim_skel_model);
    float *framegroup_fps = get_member_from_offset(anim_skel_model->framegroup_fps, anim_skel_model);
    bool *framegroup_loop = get_member_from_offset(anim_skel_model->framegroup_loop, anim_skel_model);

    // Movement distance for all frames (cumsum-ed within each framegroup)
    float *frames_move_dist = get_member_from_offset(anim_skel_model->frames_move_dist, anim_skel_model);
    // Get the array for this framegroup's values
    float *anim_frames_move_dist = frames_move_dist + framegroup_start_frame[anim_framegroup_idx];

    float anim_framerate = framegroup_fps[anim_framegroup_idx];
    bool anim_looped = framegroup_loop[anim_framegroup_idx];
    uint32_t anim_n_frames = framegroup_n_frames[anim_framegroup_idx];
    float anim_duration = anim_n_frames / framegroup_fps[anim_framegroup_idx];


    float dist_start = _dist_walked_at_frametime(t_start, anim_frames_move_dist, anim_framerate, anim_n_frames, anim_looped);
    float dist_stop = _dist_walked_at_frametime(t_stop, anim_frames_move_dist, anim_framerate, anim_n_frames, anim_looped);


    // Return total distance walked between start and stop time:
    float dist_walked = dist_stop - dist_start;
    // Con_Printf("Anim %d.%d across (%f,%f) walked %f\n", anim_modelindex, anim_framegroup_idx, t_start, t_stop, dist_walked);
    // Con_Printf("\tDist walked at t=%f: %f\n", t_start, dist_start);
    // Con_Printf("\tDist walked at t=%f: %f\n", t_stop, dist_stop);

    G_FLOAT(OFS_RETURN) = dist_walked;
}




//
// Calls QuakeC callback function once for each IQM FTE Animation Event between
// [start_time, stop_time]
//
void PF_processmodelevents(void) {
    Con_Printf("PF_processmodelevents start\n");
    int anim_modelindex = G_FLOAT(OFS_PARM0);
    int anim_framegroup_idx = G_FLOAT(OFS_PARM1);
    float t_start = G_FLOAT(OFS_PARM2);
    float t_stop = G_FLOAT(OFS_PARM3);
    func_t callback_func = G_INT(OFS_PARM4);

    // Don't fire the same event multiple times (FTE-equivalence)
    if(t_start >= t_stop) {
        return;
    }

    skeletal_model_t *anim_skel_model = sv_get_skel_model_for_modelidx(anim_modelindex);
    if(anim_skel_model == nullptr) {
        return;
    }

    if(anim_framegroup_idx < 0 || anim_framegroup_idx >= anim_skel_model->n_framegroups) {
        return;
    }

    // Get animation events data
    uint16_t *framegroup_n_events = get_member_from_offset(anim_skel_model->framegroup_n_events, anim_skel_model);
    if(framegroup_n_events == nullptr) {
        return;
    }
    uint16_t anim_n_events = framegroup_n_events[anim_framegroup_idx];
    if(anim_n_events <= 0) {
        return;
    }
    float **framegroup_event_time = get_member_from_offset(anim_skel_model->framegroup_event_time, anim_skel_model);
    uint32_t **framegroup_event_code = get_member_from_offset(anim_skel_model->framegroup_event_code, anim_skel_model);
    char ***framegroup_event_data_str = get_member_from_offset(anim_skel_model->framegroup_event_data_str, anim_skel_model);
    // --
    float *anim_event_times = get_member_from_offset(framegroup_event_time[anim_framegroup_idx], anim_skel_model);
    uint32_t *anim_event_codes = get_member_from_offset(framegroup_event_code[anim_framegroup_idx], anim_skel_model);
    char **anim_event_data_strs = get_member_from_offset(framegroup_event_data_str[anim_framegroup_idx], anim_skel_model);

    // Get animation playback data
    uint32_t *framegroup_n_frames = get_member_from_offset(anim_skel_model->framegroup_n_frames, anim_skel_model);
    float *framegroup_fps = get_member_from_offset(anim_skel_model->framegroup_fps, anim_skel_model);
    bool *framegroup_loop = get_member_from_offset(anim_skel_model->framegroup_loop, anim_skel_model);
    float anim_duration = framegroup_n_frames[anim_framegroup_idx] / framegroup_fps[anim_framegroup_idx]; // Animation duration in seconds
    bool anim_looped = framegroup_loop[anim_framegroup_idx];

    if(anim_looped == false) {
        // Iterate through this framegroup's events, dispatching the callback for any events in the search window
        for(int event_idx = 0; event_idx < anim_n_events; event_idx++) {
            float event_time = anim_event_times[event_idx];
            if(event_time >= t_start && event_time <= t_stop) {
                uint32_t event_code = anim_event_codes[event_idx];
                char *event_data_str = get_member_from_offset(anim_event_data_strs[event_idx], anim_skel_model);
                run_model_event_callback( event_time, event_code, event_data_str, callback_func);
            }
        }
    }
    else {
        // Loop through every pass through the animation from t_start to t_stop
        // Offset for each event time to handle looping
        // NOTE - This is not compliant with FTE-implementation. FTE does not handle events in subsequent playthroughs of a looped animation.
        // NOTE   Additional code is required in QC to achieve this in FTE.
        int cur_anim_playthrough = floorf(t_start / anim_duration); 
        float event_time;
        // Iterate through this framegroup's events, dispatching the callback for any events in the search window
        for(int event_idx = 0; event_idx < anim_n_events; event_idx++) {
            event_time = anim_event_times[event_idx] + (cur_anim_playthrough * anim_duration);
            if(event_time > t_stop) {
                break;
            }
            if(event_time >= t_start && event_time <= t_stop) {
                uint32_t event_code = anim_event_codes[event_idx];
                char *event_data_str = get_member_from_offset(anim_event_data_strs[event_idx], anim_skel_model);
                run_model_event_callback( event_time, event_code, event_data_str, callback_func);
            }
            // If on last event, but we're not caught up to t_stop, repeat search from first event
            if(event_idx == anim_n_events - 1) {
                event_idx = 0;
                cur_anim_playthrough++;
            }
        }
    }
    Con_Printf("PF_processmodelevents end\n");
}




//
// Returns an index for an unused server-side skeleton state buffer
//
void PF_skeleton_create(void) {
    // Con_Printf("PF_skeleton_create start\n");
    int skel_idx = -1;
    int skel_model_idx = G_FLOAT(OFS_PARM0);

    // Con_Printf("PF_skeleton_create: For modelindex: %d\n", skel_model_idx);
    skeletal_model_t *skel_model = sv_get_skel_model_for_modelidx(skel_model_idx);
    if(skel_model == nullptr) {
        G_FLOAT(OFS_RETURN) = skel_idx;
        Con_Printf("PF_skeleton_create: Invalid IQM model\n");
        return;
    }

    for(int i = 0; i < MAX_SKELETONS; i++) {
        if(!sv_skeletons[i].in_use) {
            sv_skeletons[i].in_use = true;
            init_skeleton(&(sv_skeletons[i]), skel_model_idx, skel_model);
            skel_idx = i;
            // Con_Printf("Found unused skeleton: %d\n", skel_idx);
            break;
        }
    }
    // Con_Printf("PF_skeleton_create (skel_idx: %d)\n", skel_idx);
    G_FLOAT(OFS_RETURN) = skel_idx;
    // Con_Printf("PF_skeleton_create end\n");
}

//
// Clears the skeleton state buffer at that index
//
void PF_skeleton_destroy(void) {
    // Con_Printf("PF_skeleton_destroy start\n");
    int skel_idx = G_FLOAT(OFS_PARM0);
    if(skel_idx < 0 || skel_idx >= MAX_SKELETONS) {
        return;
    }
    if(sv_skeletons[skel_idx].in_use) {
        sv_skeletons[skel_idx].in_use = false;
        free_skeleton(&(sv_skeletons[skel_idx]));
    }
    // Con_Printf("PF_skeleton_destroy (skel_idx: %d)\n", skel_idx);
    // Con_Printf("PF_skeleton_destroy end\n");
}

//
// Sets the skeleton state at some index and builds required transform matrices.
//
void PF_skeleton_build(void) {
    // Con_Printf("PF_skeleton_build start\n");
    int skel_idx = G_FLOAT(OFS_PARM0);
    int anim_modelindex = G_FLOAT(OFS_PARM1);
    int anim_framegroup_idx = G_FLOAT(OFS_PARM2);
    float anim_starttime = G_FLOAT(OFS_PARM3);
    float anim_speed = G_FLOAT(OFS_PARM4);
    // Con_Printf("PF_skeleton_build (skel_idx: %d, model: %d, framegroup: %d, start: %.2f, speed: %.2f)\n", skel_idx, anim_modelindex, anim_framegroup_idx, anim_starttime, anim_speed);

    if(skel_idx < 0 || skel_idx >= MAX_SKELETONS) {
        return;
    }

    skeletal_skeleton_t *sv_skeleton = &(sv_skeletons[skel_idx]);
    if(!sv_skeleton->in_use) {
        return;
    }

    skeletal_model_t *anim_skel_model = sv_get_skel_model_for_modelidx(anim_modelindex);
    if(anim_skel_model == nullptr) {
        Con_Printf("PF_skeleton_build: Invalid skeletal animation model\n");
        return;
    }

    // Con_Printf("PF_skeleton_build: skel_idx: %d, skel model: %d, skel anim: %d\n", skel_idx, sv_skeleton->modelindex, sv_skeleton->anim_modelindex);

    // TODO - Add skeleton-owning entity as an arg to this function
    // TODO - Update the entity's skeleton state variables so they are networked to clients

    build_skeleton_for_start_time(sv_skeleton, anim_skel_model, anim_framegroup_idx, anim_starttime, anim_speed);
    sv_skeleton->anim_modelindex = anim_modelindex;
    // Con_Printf("PF_skeleton_build end\n");
}


// 
// Precalculates a skeletal model's mins / maxs for animations from a given skeletal model
//
void PF_skel_register_anim(void) {
    Con_Printf("PF_skel_register_anim start\n");

    char *skel_model_name = G_STRING(OFS_PARM0);
    char *skel_model_anims_name = G_STRING(OFS_PARM1);
    PR_CheckEmptyString(skel_model_name);
    PR_CheckEmptyString(skel_model_anims_name);

    int skel_model_modelidx = -1;
    int skel_model_anims_modelidx = -1;

    // Find the skeletal-model model indexes
    for(int i = 0; i < MAX_MODELS; i++) {
        if(!strcmp(sv.model_precache[i], skel_model_name)) {
            skel_model_modelidx = i;
            break;
        }
    }
    for(int i = 0; i < MAX_MODELS; i++) {
        if(!strcmp(sv.model_precache[i], skel_model_anims_name)) {
            skel_model_anims_modelidx = i;
            break;
        }
    }
    if(skel_model_modelidx < 0 || skel_model_anims_modelidx < 0) {
        return;
    }

    Con_Printf("Registering animation %d for model %d\n", skel_model_anims_modelidx, skel_model_modelidx);

    vec3_t mins, maxs;
    // Calculate the mins / maxs bounds for this skeletal model `skel_model_modelidx` 
    // on all animation framegroups in `skel_model_anims_modelidx`
    // Stashes the resulting mins / maxs in global `skel_model_bounds_idxs` array
    // for instant lookup on subsequent calls for this model / anim
    sv_get_skel_model_bounds( skel_model_modelidx, skel_model_anims_modelidx, mins, maxs);

    Con_Printf("PF_skel_register_anim end\n");
}




//
// Collide a line segment from `start` to `end` in world-space against a skeletal ent's bone hitboxes
//
//  Parameters:
//  ent -- The entity with a skeletal model to collision-check against
//  ray_start -- traceline start pos in world-space
//  ray_end -- traceline end pos in world-space
//  trace_fraction -- Current smallest trace_fraction, if skeletal ent AABB trace_fraction is larger, it is skipped
//                    intersected bone hitboxes whose trace_fraction is larger than the initial value are skipped
//                    trace_fraction is updated to the smallest trace_fraction of all intersected bone hitboxes
//  trace_bone_idx -- Set to the bone index for the bone that was intersected and had the smallest trace_fraction
//  trace_bone_hitbox_tag -- Set to the bone tag value for the bone that was intersected and had the smallest trace_fraction
//
//  Return Value:
//      Returns whether or not the normal entity AABB collision detection should be skipped
//      qtrue: if the entity has a valid skeleton that we checked against, and traceline code should skip normal entity AABB collision detection
//      qfalse: if the entity did not have a valid skeleton, and traceline code should resume normal entity AABB collision detection
//
qboolean sv_intersect_skeletal_model_ent(edict_t *ent, vec3_t ray_start, vec3_t ray_end, float *trace_fraction, int *trace_bone_idx, int *trace_bone_hitbox_tag) {

    if(ent == nullptr) {
        // No ent
        // Revert to normal ent traceline collision detection
        // Con_Printf("sv_intersect_skeletal_model: ent invalid.\n");
        return qfalse;
    }
    int sv_skel_idx = (int) ent->v.skeletonindex;
    // Con_Printf("sv_intersect_skeletal_model for ent (skelidx: %d) (t=%.2f)\n", sv_skel_idx, sv.time);
    // Con_Printf("sv_intersect_skeletal_model for ent: \"%s\" (skelidx: %d) (t=%.2f)\n", pr_strings + ent->v.classname, sv_skel_idx, sv.time);
    if(sv_skel_idx < 0 || sv_skel_idx >= MAX_SKELETONS) {
        // No skeleton
        // Revert to normal ent traceline collision detection
        Con_Printf("sv_intersect_skeletal_model: skelidx %d invalid.\n", sv_skel_idx);
        return qfalse;
    }

    skeletal_skeleton_t *sv_skeleton = &(sv_skeletons[sv_skel_idx]);
    if(!sv_skeleton->in_use) {
        Con_Printf("sv_intersect_skeletal_model: skelidx %d not in use.\n", sv_skel_idx);
        // Skeleton exists, but has not been created properly
        // Revert to normal ent traceline collision detection
        return qfalse;
    }

    if(sv_skeleton->model == nullptr || sv_skeleton->anim_model == nullptr) {
        Con_Printf("sv_intersect_skeletal_model: skelidx %d has no model or anim_model.\n", sv_skel_idx);
        // Skeleton exists and was created, but has not been built
        // Revert to normal ent traceline collision detection
        return qfalse;
    }

    // ------------------------------------------------------------------------
    // Early-out test 1: Skeletal Model world-space AABB
    // 
    // Before looping over bone hitboxes, check if the ray intersects an axis-
    // aligned bounding box that fits all bones for the skeleton's animation
    // 
    // ------------------------------------------------------------------------
    // Get skeleton model-space AABB
    vec3_t skel_model_mins;
    vec3_t skel_model_maxs;
    // Con_Printf("sv_intersect_skeletal_model: skel model: %d, skel anim: %d\n", sv_skeleton->modelindex, sv_skeleton->anim_modelindex);
    sv_get_skel_model_bounds( sv_skeleton->modelindex, sv_skeleton->anim_modelindex, skel_model_mins, skel_model_maxs);


    // Con_Printf("sv_intersect_skeletal_model skel bounds: (mins: [%.2f, %.2f, %.2f], maxs: [%.2f, %.2f, %.2f])\n",
    //     skel_model_mins[0], skel_model_mins[1], skel_model_mins[2],
    //     skel_model_maxs[0], skel_model_maxs[1], skel_model_maxs[2]
    // );


    // float ent_scale = (ent->scale == ENTSCALE_DEFAULT || ent->scale == 0) ? 1.0f : ENTSCALE_DECODE(ent->scale); // Client-side scale parsing
    float ent_scale = (ent->v.scale == ENTSCALE_DEFAULT || ent->v.scale == 0) ? 1.0f : ent->v.scale; // Server-side scale parsing
    mat3x4_t model_to_world_transform;
    Matrix3x4_CreateFromEntity(model_to_world_transform, ent->v.angles, ent->v.origin, ent_scale);

    // Get skeleton world-space AABB given ent's rotation / translation / scale
    vec3_t skel_world_mins;
    vec3_t skel_world_maxs;
    transform_AABB(skel_model_mins, skel_model_maxs, model_to_world_transform, skel_world_mins, skel_world_maxs);

    // Get mins / maxs for ray
    vec3_t ray_mins;
    vec3_t ray_maxs;
    VectorMin(ray_start, ray_end, ray_mins); // FIXME - duplicate code, precompute and pass in to function?
    VectorMax(ray_start, ray_end, ray_maxs); // FIXME - duplicate code, precompute and pass in to function?

    // If traceline can't hit skeleton world-space AABB, stop early
    if( ray_maxs[0] < skel_world_mins[0] || ray_mins[0] > skel_world_maxs[0] || 
        ray_maxs[1] < skel_world_mins[1] || ray_mins[1] > skel_world_maxs[1] || 
        ray_maxs[2] < skel_world_mins[2] || ray_mins[2] > skel_world_maxs[2]) {
        // Con_Printf("sv_intersect_skeletal_model: skelidx %d traceline missed world-space AABB.\n", sv_skel_idx);
        // Con_Printf("\tskel model aabb: mins: [%.1f, %.1f, %.1f] maxs: [%.1f, %.1f, %.1f]\n", skel_model_mins[0], skel_model_mins[1], skel_model_mins[2], skel_model_maxs[0], skel_model_maxs[1], skel_model_maxs[2] );
        // Con_Printf("\tskel world aabb: mins: [%.1f, %.1f, %.1f] maxs: [%.1f, %.1f, %.1f]\n", skel_world_mins[0], skel_world_mins[1], skel_world_mins[2], skel_world_maxs[0], skel_world_maxs[1], skel_world_maxs[2] );
        // Con_Printf("\tray world aabb:  mins: [%.1f, %.1f, %.1f] maxs: [%.1f, %.1f, %.1f]\n", ray_mins[0], ray_mins[1], ray_mins[2], ray_maxs[0], ray_maxs[1], ray_maxs[2] );


        // Con_Printf("sv_intersect_skeletal_model: worldAABB: 0\n");


        // Indicate that normal traceline checking code should be skipped for this ent
        return qtrue;
    }
    // ------------------------------------------------------------------------

    vec3_t ray_ofs;
    VectorSubtract(ray_end, ray_start, ray_ofs);
    // We don't need normalized ray_ofs as ray_dir
    // vec3_t ray_dir;
    // float inv_ray_length = rsqrt(DotProduct(ray_ofs,ray_ofs)); // 1.0f / len(ray_ofs)
    // VectorScale(ray_ofs, inv_ray_length, ray_dir);



    // ------------------------------------------------------------------------
    // Early-out test 2: Skeletal Model model-space AABB
    // 
    // Before looping over bone hitboxes, check if the ray intersects the
    // skeleton's world-space bounding box
    // 
    // If it doesn't, skip checking against bones
    // If it does, but its trace_fraction is larger than the current `trace_fraction` value , skip checking against bones (farther away than a known traceline hit)
    // If it does, and its trace_fraction is smaller than current, then we'll need iterate over bones
    // 
    // ------------------------------------------------------------------------
    // Calculate the scale and ofs needed for a unit-bbox to have mins=`skel_model_mins` and maxs=`skel_model_maxs`
    vec3_t skel_hitbox_scale;
    vec3_t skel_hitbox_ofs;
    VectorSubtract(skel_model_maxs, skel_model_mins, skel_hitbox_scale);
    VectorAdd(skel_model_mins, skel_model_maxs, skel_hitbox_ofs);
    VectorScale(skel_hitbox_ofs, 0.5, skel_hitbox_ofs);

    // Make transform matrix that applies both ofs / scale to a unit-bbox
    mat3x4_t skel_hitbox_transform;
    Matrix3x4_scale_translate(skel_hitbox_transform, skel_hitbox_scale, skel_hitbox_ofs);

    // Concat the ent's model-space to world-space transform so bbox has ent's location and orientation
    mat3x4_t skel_hitbox_to_world_transform;
    Matrix3x4_ConcatTransforms(skel_hitbox_to_world_transform, model_to_world_transform, skel_hitbox_transform);

    // Invert it to get matrix that takes us from world-space to unit-bbox space
    mat3x4_t world_to_skel_hitbox_transform;
    Matrix3x4_Invert_Affine(world_to_skel_hitbox_transform, skel_hitbox_to_world_transform);

    // Get the start and delta position of the ray in unit-bbox space
    vec3_t ray_start_skel_hitbox_space;
    vec3_t ray_ofs_skel_hitbox_space;
    Matrix3x4_VectorTransform(world_to_skel_hitbox_transform, ray_start, ray_start_skel_hitbox_space);
    Matrix3x4_VectorRotate(world_to_skel_hitbox_transform, ray_ofs, ray_ofs_skel_hitbox_space);

    float skel_hitbox_trace_fraction; // Fraction of the ray ofs from start to hit
    bool skel_hitbox_hit = ray_aabb_intersection(ray_start_skel_hitbox_space, ray_ofs_skel_hitbox_space, skel_hitbox_trace_fraction);

    // If skeleton hitbox wasn't hit, or its trace_fraction is larger than current, stop
    if(!skel_hitbox_hit || skel_hitbox_trace_fraction > *trace_fraction) {
        // Con_Printf("sv_intersect_skeletal_model: worldAABB: 1  modelAABB: 0\n");
        // Con_Printf("sv_intersect_skeletal_model: skelidx %d traceline missed model-space AABB.\n", sv_skel_idx);
        // Con_Printf("\ttracelint hit: %d\n", skel_hitbox_hit);
        // Con_Printf("\thitbox trace_fraction: %f, current: %f\n", skel_hitbox_trace_fraction, *trace_fraction);

        // Indicate that normal traceline checking code should be skipped for this ent
        return qtrue;
    }

    // Con_Printf("sv_intersect_skeletal_model: worldAABB: 1  modelAABB: 1\n");

    // Con_Printf("sv_intersect_skeletal_model: early-out-2 - 8.\n");
    // ------------------------------------------------------------------------

    // For checking duplicate zombie intersections
    // Con_Printf("sv_intersect_skeletal_model_ent -- For skelidx %d at time: %.2f\n", sv_skel_idx, realtime);






    mat3x4_t *bone_transforms = sv_skeleton->bone_transforms;
    bool *bone_hitbox_enabled = sv_skeleton->bone_hitbox_enabled;
    vec3_t *bone_hitbox_ofs = sv_skeleton->bone_hitbox_ofs;
    vec3_t *bone_hitbox_scale = sv_skeleton->bone_hitbox_scale;
    int *bone_hitbox_tag = sv_skeleton->bone_hitbox_tag;

    // Pull bone hitbox info from model
    // char **bone_names = get_member_from_offset(sv_skeleton->model->bone_name, sv_skeleton->model);
    // bool *bone_hitbox_enabled = get_member_from_offset(sv_skeleton->model->bone_hitbox_enabled, sv_skeleton->model);
    // vec3_t *bone_hitbox_ofs = get_member_from_offset(sv_skeleton->model->bone_hitbox_ofs, sv_skeleton->model);
    // vec3_t *bone_hitbox_scale = get_member_from_offset(sv_skeleton->model->bone_hitbox_scale, sv_skeleton->model);
    // int *bone_hitbox_tag = get_member_from_offset(sv_skeleton->model->bone_hitbox_tag, sv_skeleton->model);


    // ------------------------------------------------------------------------
    // Loop over bone hitboxes, keeping track of which one was closest to start
    // ------------------------------------------------------------------------
    // float closest_hit_bone_dist;
    float closest_hit_bone_ray_frac;
    int closest_hit_bone_idx = -1;
    for(int i = 0; i < sv_skeleton->model->n_bones; i++) {
        if(!bone_hitbox_enabled[i]) {
            continue;
        }
        mat3x4_t bone_hitbox_transform;
        mat3x4_t bone_transform;
        // Apply bone AABB scale / ofs:
        Matrix3x4_scale_translate(bone_hitbox_transform, bone_hitbox_scale[i], bone_hitbox_ofs[i]);
        // Apply bone's current pose transform:
        Matrix3x4_ConcatTransforms(bone_transform, bone_transforms[i], bone_hitbox_transform);
        mat3x4_t bone_to_world_transform;
        Matrix3x4_ConcatTransforms(bone_to_world_transform, model_to_world_transform, bone_transform);
        mat3x4_t world_to_bone_transform;
        Matrix3x4_Invert_Affine(world_to_bone_transform, bone_to_world_transform);

        vec3_t ray_start_bone_space;
        vec3_t ray_ofs_bone_space;
        // vec3_t ray_dir_bone_space_nor;
        Matrix3x4_VectorTransform(world_to_bone_transform, ray_start, ray_start_bone_space);
        // // Matrix3x4_VectorTransform(world_to_bone_transform, ray_dir, ray_dir_bone_space);
        // Matrix3x4_VectorRotate(world_to_bone_transform, ray_dir, ray_dir_bone_space);
        // // VectorCopy(ray_dir_bone_space, ray_dir_bone_space_nor);
        // // VectorNormalizeFast(ray_dir_bone_space_nor);
        Matrix3x4_VectorRotate(world_to_bone_transform, ray_ofs, ray_ofs_bone_space);
        
        float cur_bone_tmin; // Fraction of the ray ofs from start to hit
        bool bbox_hit = ray_aabb_intersection(ray_start_bone_space, ray_ofs_bone_space, cur_bone_tmin);

        // Keep track of which bone hitbox has the smallest t value (hit closest to the ray start position)
        if(bbox_hit) {
            if(closest_hit_bone_idx == -1 || cur_bone_tmin < closest_hit_bone_ray_frac) {
                closest_hit_bone_ray_frac = cur_bone_tmin;
                closest_hit_bone_idx = i;
            }
        }
    }

    if(closest_hit_bone_idx != -1) {
        *trace_bone_idx = closest_hit_bone_idx;
        *trace_bone_hitbox_tag = bone_hitbox_tag[closest_hit_bone_idx];
        *trace_fraction = closest_hit_bone_ray_frac;
        // Con_Printf("Hit bone idx: %d, tag: %d Hit world dist: %.2f, original ray length: %.2f, frac: %.2f\n", 
        //     closest_hit_bone_idx, 
        //     bone_hitbox_tag[closest_hit_bone_idx], 
        //     closest_hit_bone_ray_frac * VectorLength(ray_ofs),
        //     VectorLength(ray_ofs),
        //     closest_hit_bone_ray_frac
        // );

        // char *bone_name = get_member_from_offset(bone_names[closest_hit_bone_idx], sv_skeleton->model);
        // Con_Printf("%d (\"%s\"), ", closest_hit_bone_idx, bone_name);
        // Con_Printf("\n");
    }
    // ------------------------------------------------------------------------
    // Con_Printf("sv_intersect_skeletal_model: skelidx %d reached end of control.\n", sv_skel_idx);

    // Return true to skip normal ent traceline collision detection
    return qtrue;
}



void draw_ent_aabb(entity_t *ent) {
    static float unit_box[72] = {
        -0.5f, -0.5f, -0.5f,        0.5f, -0.5f, -0.5f, // left bottom front    --> right bottom front
        -0.5f,  0.5f, -0.5f,        0.5f,  0.5f, -0.5f, // left top front       --> right top front
        -0.5f, -0.5f,  0.5f,        0.5f, -0.5f,  0.5f, // left bottom back     --> right bottom back
        -0.5f,  0.5f,  0.5f,        0.5f,  0.5f,  0.5f, // left top back        --> right top back
        // --
        -0.5f, -0.5f, -0.5f,        -0.5f,  0.5f, -0.5f, // left bottom front    --> left top front
        -0.5f, -0.5f,  0.5f,        -0.5f,  0.5f,  0.5f, // left bottom back     --> left top back
         0.5f, -0.5f, -0.5f,         0.5f,  0.5f, -0.5f, // right bottom front   --> right top front
         0.5f, -0.5f,  0.5f,         0.5f,  0.5f,  0.5f, // right bottom back    --> right top back
        // --
        -0.5f, -0.5f, -0.5f,        -0.5f, -0.5f,  0.5f, // left bottom front   --> left bottom back
        -0.5f,  0.5f, -0.5f,        -0.5f,  0.5f,  0.5f, // left top front      --> left top back
         0.5f, -0.5f, -0.5f,         0.5f, -0.5f,  0.5f, // right bottom front  --> right bottom back
         0.5f,  0.5f, -0.5f,         0.5f,  0.5f,  0.5f  // right top front     --> right top back
    };


    sceGumPushMatrix();
    sceGumLoadIdentity();
    // ------------------------------------------------------------------------
    // Modified logic from `R_BlendedRotateForEntity` without the rotation
    // (AABBs are not rotated)
    // ------------------------------------------------------------------------
    float timepassed;
    float blend;
    vec3_t d;
    int i;
    // Inerpolate position:
    timepassed = realtime - ent->translate_start_time;
    if (ent->translate_start_time == 0 || timepassed > 1) {
        ent->translate_start_time = realtime;
        VectorCopy(ent->origin, ent->origin1);
        VectorCopy(ent->origin, ent->origin2);
    }
    if (!VectorCompare(ent->origin, ent->origin2)) {
        ent->translate_start_time = realtime;
        VectorCopy(ent->origin2, ent->origin1);
        VectorCopy(ent->origin,  ent->origin2);
        blend = 0;
    }
    else {
        blend = timepassed / 0.1;
        if (cl.paused || blend > 1) {
            blend = 0;
        }
    }
    VectorSubtract (ent->origin2, ent->origin1, d);

    // Translate.
    const ScePspFVector3 translation = {
    	ent->origin[0] + (blend * d[0]),
    	ent->origin[1] + (blend * d[1]),
    	ent->origin[2] + (blend * d[2])
    };
    // Con_Printf("AABB Ent Translation: (%.2f, %.2f, %.2f)\n", translation.x, translation.y, translation.z);
    sceGumTranslate(&translation);

	// Scale.
	if (ent->scale != ENTSCALE_DEFAULT) {
		float scalefactor = ENTSCALE_DECODE(ent->scale);
		const ScePspFVector3 scale_vec = { scalefactor, scalefactor, scalefactor };
		sceGumScale(&scale_vec);


        // Con_Printf("AABB Ent Scale: (%.2f, %.2f, %.2f)\n", scale_vec.x, scale_vec.y, scale_vec.Z);
	}
	sceGumUpdateMatrix();
    // ------------------------------------------------------------------------

    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_TEXTURE_2D);

    vec3_t aabb_mins;
    vec3_t aabb_maxs;
    // FIXME - entity_t doesn't have mins / maxs. Doesn't look like client has them. Boo.
    // VectorCopy(ent->v.mins, aabb_mins);
    // VectorCopy(ent->v.maxs, aabb_maxs);
    VectorSet(aabb_mins, -16, -16, -36);
    VectorSet(aabb_maxs,  16,  16,  36);

    // Find transform that scales / translates unit AABB to ent's AABB
    vec3_t aabb_ofs;
    vec3_t aabb_scale;
    // Calculate the scale and ofs needed for a unit-bbox to have mins=`skel_model_mins` and maxs=`skel_model_maxs`
    VectorSubtract(aabb_maxs, aabb_mins, aabb_scale);
    VectorAdd(aabb_maxs, aabb_mins, aabb_ofs);
    VectorScale(aabb_ofs, 0.5, aabb_ofs);

    // Apply AABB scale / ofs:
    mat3x4_t aabb_transform;
    Matrix3x4_scale_translate(aabb_transform, aabb_scale, aabb_ofs);

    // -------------------------------
    // TODO salad - Print out aabb_transform
    // -------------------------------
    // Con_Printf("Ent world transform:\n");
    // Con_Printf("\t[%.1f, %.1f, %.1f, %.1f]\n", aabb_transform[0][0], aabb_transform[0][1], aabb_transform[0][2], aabb_transform[0][3]);
    // Con_Printf("\t[%.1f, %.1f, %.1f, %.1f]\n", aabb_transform[1][0], aabb_transform[1][1], aabb_transform[1][2], aabb_transform[1][3]);
    // Con_Printf("\t[%.1f, %.1f, %.1f, %.1f]\n", aabb_transform[2][0], aabb_transform[2][1], aabb_transform[2][2], aabb_transform[2][3]);
    // -------------------------------

    ScePspFMatrix4 aabb_transform_mat;
    Matrix3x4_to_ScePspFMatrix4(&aabb_transform_mat, &aabb_transform);
    sceGumMultMatrix(&aabb_transform_mat);
    sceGuColor(GU_COLOR(0.0,1.0,0.0,1.0)); // RGBA green
    sceGumDrawArray(GU_LINES,GU_VERTEX_32BITF|GU_TRANSFORM_3D, 24, nullptr, unit_box);
    sceGumPopMatrix();
    // TODO - Am I missing something?

    sceGuEnable(GU_DEPTH_TEST);
    sceGuEnable(GU_TEXTURE_2D);
}





//
// Draws bone hitbox bounding boxes and overall skeleton bounding box
//  draw_skel_bbox -- If `true`, draws overall skeleton bounding box around bones
//  draw_bone_bboxes -- If `true`, draws individual bone bboxes
//
void draw_skeleton_bboxes(entity_t *ent, skeletal_skeleton_t *skel, bool draw_skel_bbox, bool draw_bone_bboxes) {
    if(skel == nullptr) {
        return;
    }

    // Draw bones
    // Con_Printf("------------------------------\n");
    // char **bone_names = get_member_from_offset(skel_model->bone_name, skel_model);
    mat3x4_t *bone_transforms = skel->bone_transforms;

    // ------------------------------------------------------------------------
    // Draw bone bboxes
    // ------------------------------------------------------------------------
    static float unit_box[72] = {
        -0.5f, -0.5f, -0.5f,        0.5f, -0.5f, -0.5f, // left bottom front    --> right bottom front
        -0.5f,  0.5f, -0.5f,        0.5f,  0.5f, -0.5f, // left top front       --> right top front
        -0.5f, -0.5f,  0.5f,        0.5f, -0.5f,  0.5f, // left bottom back     --> right bottom back
        -0.5f,  0.5f,  0.5f,        0.5f,  0.5f,  0.5f, // left top back        --> right top back
        // --
        -0.5f, -0.5f, -0.5f,        -0.5f,  0.5f, -0.5f, // left bottom front    --> left top front
        -0.5f, -0.5f,  0.5f,        -0.5f,  0.5f,  0.5f, // left bottom back     --> left top back
         0.5f, -0.5f, -0.5f,         0.5f,  0.5f, -0.5f, // right bottom front   --> right top front
         0.5f, -0.5f,  0.5f,         0.5f,  0.5f,  0.5f, // right bottom back    --> right top back
        // --
        -0.5f, -0.5f, -0.5f,        -0.5f, -0.5f,  0.5f, // left bottom front   --> left bottom back
        -0.5f,  0.5f, -0.5f,        -0.5f,  0.5f,  0.5f, // left top front      --> left top back
         0.5f, -0.5f, -0.5f,         0.5f, -0.5f,  0.5f, // right bottom front  --> right bottom back
         0.5f,  0.5f, -0.5f,         0.5f,  0.5f,  0.5f  // right top front     --> right top back
    };

    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_TEXTURE_2D);


    mat3x4_t model_to_world_transform;
    R_BlendedRotateForEntity_2(ent, model_to_world_transform);

    // float ent_scale = 1.0f;
    // if(ent->scale != ENTSCALE_DEFAULT && ent->scale != 0) {
    //     ent_scale = ENTSCALE_DECODE(ent->scale);
    // }
    // Matrix3x4_CreateFromEntity(model_to_world_transform, ent->angles, ent->origin, ent_scale);


    // --------------------------------------------------
    // Create the player view ray
    // --------------------------------------------------
    vec3_t v_forward, v_right, v_up;
    AngleVectors (cl.viewangles, v_forward, v_right, v_up);
    edict_t *player = EDICT_NUM(cl.viewentity);
    vec3_t ray_start, ray_dir;
    VectorAdd(player->v.origin, player->v.view_ofs, ray_start);
    VectorCopy(v_forward, ray_dir);
    // VectorNormalizeFast(ray_dir);
    


    // --------------------------------------------------
    // Draw the player view ray
    // --------------------------------------------------
    // float ray_verts[6] = {
    //     // Start
    //     ray_start[0], 
    //     ray_start[1], 
    //     ray_start[2],
    //     // End
    //     ray_start[0] + 100.0f * ray_dir[0], 
    //     ray_start[1] + 100.0f * ray_dir[1], 
    //     ray_start[2] + 100.0f * ray_dir[2]
    // };
    // sceGumPushMatrix();
    // sceGumLoadIdentity();
    // sceGuColor(0x0000ff); // red
    // sceGumDrawArray( GU_LINES, GU_VERTEX_32BITF, 2, nullptr, ray_verts);
    // sceGumPopMatrix();
    // --------------------------------------------------


    bool *bone_hitbox_enabled = get_member_from_offset(skel->model->bone_hitbox_enabled, skel->model);
    vec3_t *bone_hitbox_ofs = get_member_from_offset(skel->model->bone_hitbox_ofs, skel->model);
    vec3_t *bone_hitbox_scale = get_member_from_offset(skel->model->bone_hitbox_scale, skel->model);
    int *bone_hitbox_tag = get_member_from_offset(skel->model->bone_hitbox_tag, skel->model);


    // ----------------------------------------------------
    // Draw skeleton bounds that contains all bones
    // ----------------------------------------------------
    if(draw_skel_bbox) {
        // Con_Printf("About to draw bbox around entire skeleton\n");
        vec3_t skel_model_mins;
        vec3_t skel_model_maxs;
        // Con_Printf("Getting skel bounds for model: %d on anim: %d\n", skel->modelindex, skel->anim_modelindex);
        cl_get_skel_model_bounds( skel->modelindex, skel->anim_modelindex, skel_model_mins, skel_model_maxs);

        mat3x4_t bone_hitbox_transform;
        // Apply skel model hitbox scale / ofs
        vec3_t skel_hitbox_ofs;
        vec3_t skel_hitbox_scale;
        // Calculate the scale and ofs needed for a unit-bbox to have mins=`skel_model_mins` and maxs=`skel_model_maxs`
        VectorSubtract(skel_model_maxs, skel_model_mins, skel_hitbox_scale);
        VectorAdd(skel_model_mins, skel_model_maxs, skel_hitbox_ofs);
        VectorScale(skel_hitbox_ofs, 0.5, skel_hitbox_ofs);

        mat3x4_t skel_hitbox_transform;
        // Apply AABB scale / ofs:
        Matrix3x4_scale_translate(skel_hitbox_transform, skel_hitbox_scale, skel_hitbox_ofs);

        // --------------------------------------------
        // Calculate player view ray-hitbox collision
        // --------------------------------------------
        mat3x4_t skel_hitbox_to_world_transform;
        Matrix3x4_ConcatTransforms(skel_hitbox_to_world_transform, model_to_world_transform, skel_hitbox_transform);
        mat3x4_t world_to_skel_hitbox_transform;
        Matrix3x4_Invert_Affine(world_to_skel_hitbox_transform, skel_hitbox_to_world_transform);
        // Con_Printf("About to draw bbox around entire skeleton 2\n");
        vec3_t ray_start_skel_space;
        vec3_t ray_dir_skel_space;
        Matrix3x4_VectorTransform(world_to_skel_hitbox_transform, ray_start, ray_start_skel_space);
        // Matrix3x4_VectorTransform(world_to_bone_transform, ray_dir, ray_dir_bone_space);
        Matrix3x4_VectorRotate(world_to_skel_hitbox_transform, ray_dir, ray_dir_skel_space);
        // VectorNormalizeFast(ray_dir_bone_space);
        float bbox_hit_tmin;
        // Con_Printf("About to draw bbox around entire skeleton 3\n");
        bool bbox_hit = ray_aabb_intersection(ray_start_skel_space, ray_dir_skel_space, bbox_hit_tmin);
        // --------------------------------------------

        ScePspFMatrix4 skel_mat;
        Matrix3x4_to_ScePspFMatrix4(&skel_mat, &skel_hitbox_transform);

        // --
        sceGumPushMatrix();
        sceGumMultMatrix(&skel_mat);
        sceGumUpdateMatrix();
        if(bbox_hit) {
            sceGuColor(GU_COLOR(1.0,0.0,0.0,1.0)); // RGBA red
        }
        else {
            sceGuColor(GU_COLOR(0.0,1.0,1.0,1.0)); // RGBA cyan
        }
        sceGumDrawArray(GU_LINES,GU_VERTEX_32BITF|GU_TRANSFORM_3D, 24, nullptr, unit_box);
        sceGumPopMatrix();

        // --------------------------------------------------------------------
        // Convert model-space AABB to world-space AABB
        // --------------------------------------------------------------------
        // mat3x4_t model_to_world_transform;
        vec3_t skel_world_mins;
        vec3_t skel_world_maxs;
        // Calculate a new world-space AABB given the ent's rotation / translation
        transform_AABB(skel_model_mins, skel_model_maxs, model_to_world_transform, skel_world_mins, skel_world_maxs);


        // Apply skel model hitbox scale / ofs
        vec3_t skel_world_hitbox_ofs;
        vec3_t skel_world_hitbox_scale;
        // Calculate the scale and ofs needed for a unit-bbox to have mins=`skel_world_mins` and maxs=`skel_world_maxs`
        VectorSubtract(skel_world_maxs, skel_world_mins, skel_world_hitbox_scale);
        VectorAdd(skel_world_maxs, skel_world_mins, skel_world_hitbox_ofs);
        VectorScale(skel_world_hitbox_ofs, 0.5, skel_world_hitbox_ofs);
        // Apply AABB scale / ofs:
        mat3x4_t skel_world_hitbox_transform;
        Matrix3x4_scale_translate(skel_world_hitbox_transform, skel_world_hitbox_scale, skel_world_hitbox_ofs);

        ScePspFMatrix4 skel_world_hitbox_mat;
        Matrix3x4_to_ScePspFMatrix4(&skel_world_hitbox_mat, &skel_world_hitbox_transform);
        sceGumPushMatrix();
        sceGumLoadMatrix(&skel_world_hitbox_mat);
        sceGuColor(GU_COLOR(1.0,0.0,1.0,1.0)); // RGBA pink
        sceGumDrawArray(GU_LINES,GU_VERTEX_32BITF|GU_TRANSFORM_3D, 24, nullptr, unit_box);
        sceGumPopMatrix();
        // --------------------------------------------------------------------

    }
    // ----------------------------------------------------


    if(draw_bone_bboxes) {
        for(int i = 0; i < skel->model->n_bones; i++) {
            if(!bone_hitbox_enabled[i]) {
                continue;
            }
            mat3x4_t bone_hitbox_transform;
            mat3x4_t bone_transform;
            // Apply bone AABB scale / ofs:
            Matrix3x4_scale_translate(bone_hitbox_transform, bone_hitbox_scale[i], bone_hitbox_ofs[i]);
            // Apply bone's current pose transform:
            Matrix3x4_ConcatTransforms(bone_transform, bone_transforms[i], bone_hitbox_transform);

            mat3x4_t bone_to_world_transform;
            Matrix3x4_ConcatTransforms(bone_to_world_transform, model_to_world_transform, bone_transform);
            mat3x4_t world_to_bone_transform;
            Matrix3x4_Invert_Affine(world_to_bone_transform, bone_to_world_transform);

            vec3_t ray_start_bone_space;
            vec3_t ray_dir_bone_space;
            Matrix3x4_VectorTransform(world_to_bone_transform, ray_start, ray_start_bone_space);
            // Matrix3x4_VectorTransform(world_to_bone_transform, ray_dir, ray_dir_bone_space);
            Matrix3x4_VectorRotate(world_to_bone_transform, ray_dir, ray_dir_bone_space);
            // VectorNormalizeFast(ray_dir_bone_space);
            float bbox_hit_tmin;
            bool bbox_hit = ray_aabb_intersection(ray_start_bone_space, ray_dir_bone_space, bbox_hit_tmin);

            // vec3_t ray_start_world_space;
            // vec3_t ray_dir_world_space;
            // Matrix3x4_VectorTransform(bone_to_world_transform, ray_start_bone_space, ray_start_world_space);
            // Matrix3x4_VectorRotate(bone_to_world_transform, ray_dir_bone_space, ray_dir_world_space);
            // VectorNormalizeFast(ray_dir_world_space);

            // Con_Printf("Bone %d ray: (%.2f, %.2f, %.2f) -> (%.2f, %.2f, %.2f)\n", i, 
            //     ray_start[0], ray_start[1], ray_start[2], 
            //     ray_dir[0], ray_dir[1], ray_dir[2]
            // );
            // Con_Printf("Bone %d ray bone-space: (%.2f, %.2f, %.2f) -> (%.2f, %.2f, %.2f)\n", i, 
            //     ray_start_bone_space[0], ray_start_bone_space[1], ray_start_bone_space[2], 
            //     ray_dir_bone_space[0], ray_dir_bone_space[1], ray_dir_bone_space[2]
            // );
            // Con_Printf("Bone %d ray world-space: (%.2f, %.2f, %.2f) -> (%.2f, %.2f, %.2f)\n", i, 
            //     ray_start_world_space[0], ray_start_world_space[1], ray_start_world_space[2], 
            //     ray_dir_world_space[0], ray_dir_world_space[1], ray_dir_world_space[2]
            // );
            // Con_Printf("=======================\n");



            // // ---------------------------------------------------
            // float ray_bone_space_verts[6] = {
            //     // Start
            //     ray_start_bone_space[0],
            //     ray_start_bone_space[1],
            //     ray_start_bone_space[2],
            //     // End
            //     ray_start_bone_space[0] + 100.0f * ray_dir_bone_space[0],
            //     ray_start_bone_space[1] + 100.0f * ray_dir_bone_space[1],
            //     ray_start_bone_space[2] + 100.0f * ray_dir_bone_space[2]
            // };
            // float ray_world_space_verts[6] = {
            //     // Start
            //     ray_start_world_space[0],
            //     ray_start_world_space[1],
            //     ray_start_world_space[2],
            //     // End
            //     ray_start_world_space[0] + 100.0f * ray_dir_world_space[0],
            //     ray_start_world_space[1] + 100.0f * ray_dir_world_space[1],
            //     ray_start_world_space[2] + 100.0f * ray_dir_world_space[2]
            // };
            // sceGumPushMatrix();
            // sceGumLoadIdentity();
            // if(bbox_hit) {
            //     sceGuColor(0x00ff00); // green
            // }
            // else {
            //     sceGuColor(0x0000ff); // red
            // }
            // // sceGumDrawArray(GU_LINES,GU_VERTEX_32BITF|GU_TRANSFORM_3D, 2, nullptr, ray_bone_space_verts);
            // sceGumDrawArray(GU_LINES,GU_VERTEX_32BITF|GU_TRANSFORM_3D, 2, nullptr, ray_world_space_verts);
            // sceGumPopMatrix();
            // // ---------------------------------------------------


            // // ---------------------------------------------------
            // // Draw manually transformed bone bboxes to check bone->model space
            // // ---------------------------------------------------
            // sceGumPushMatrix();
            // sceGumLoadIdentity();
            // if(bbox_hit) {
            //     sceGuColor(0x0000ff); // red
            // }
            // else {
            //     sceGuColor(0xffff00); // cyan
            // }
            // // TODO 
        
            // float bone_bbox[72];
            // // Loop through each vec3:
            // for(int i = 0; i < 72; i += 3) {
            //     Matrix3x4_VectorTransform(bone_to_world_transform, &(unit_box[i]), &(bone_bbox[i]));
            // }
            // sceGumDrawArray(GU_LINES,GU_VERTEX_32BITF|GU_TRANSFORM_3D, 24, nullptr, bone_bbox);
            // sceGumPopMatrix();
            // // ---------------------------------------------------


            ScePspFMatrix4 bone_mat;
            Matrix3x4_to_ScePspFMatrix4(&bone_mat, &bone_transform);
            sceGumPushMatrix();
            sceGumMultMatrix(&bone_mat);
            sceGumUpdateMatrix();

            if(bbox_hit) {
                sceGuColor(GU_COLOR(1.0,0.0,0.0,1.0)); // RGBA red
            }
            else {
                sceGuColor(GU_COLOR(0.0,1.0,1.0,1.0)); // RGBA cyan
                // Skip drawing:
                // sceGumPopMatrix();
                // continue;
            }
            sceGumDrawArray(GU_LINES,GU_VERTEX_32BITF|GU_TRANSFORM_3D, 24, nullptr, unit_box);
            sceGumPopMatrix();
        }
    }


    sceGuEnable(GU_DEPTH_TEST);
    sceGuEnable(GU_TEXTURE_2D);
    // ------------------------------------------------------------------------
}

void draw_skeleton_rest_pose_bone_axes(skeletal_model_t *skel_model) {
    mat3x4_t *bone_transforms = get_member_from_offset(skel_model->bone_rest_transforms, skel_model);


    // mat3x4_t ident_mat;
    // Matrix3x4_LoadIdentity(ident_mat);



    // -----------------------------------------
    // Draw bone axes
    // -----------------------------------------
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_TEXTURE_2D);
    static float line_verts_x[6] = {0,0,0,     1,0,0}; // Verts for x-axis
    static float line_verts_y[6] = {0,0,0,     0,1,0}; // Verts for y-axis
    static float line_verts_z[6] = {0,0,0,     0,0,1}; // Verts for z-axis

    for(int i = 0; i < skel_model->n_bones; i++) {
        ScePspFMatrix4 bone_mat;
        Matrix3x4_to_ScePspFMatrix4(&bone_mat, &(bone_transforms[i]));
        sceGumPushMatrix();
        sceGumMultMatrix(&bone_mat);
        sceGumUpdateMatrix();

        sceGuColor(GU_COLOR(1.0,0.0,0.0,1.0)); // RGBA red
        sceGumDrawArray(GU_LINES,GU_VERTEX_32BITF|GU_TRANSFORM_3D, 2, nullptr, line_verts_x);
        sceGuColor(GU_COLOR(0.0,1.0,0.0,1.0)); // RGBA green
        sceGumDrawArray(GU_LINES,GU_VERTEX_32BITF|GU_TRANSFORM_3D, 2, nullptr, line_verts_y);
        sceGuColor(GU_COLOR(0.0,0.0,1.0,1.0)); // RGBA blue
        sceGumDrawArray(GU_LINES,GU_VERTEX_32BITF|GU_TRANSFORM_3D, 2, nullptr, line_verts_z);
        sceGumPopMatrix();
    }
    sceGuEnable(GU_DEPTH_TEST);
    sceGuEnable(GU_TEXTURE_2D);
    // -----------------------------------------
}

void draw_skeleton_bone_axes(entity_t *ent, skeletal_skeleton_t *skel) {
    if(skel == nullptr) {
        return;
    }

    // Draw bones
    // char **bone_names = get_member_from_offset(skel_model->bone_name, skel_model);
    mat3x4_t *bone_transforms = skel->bone_transforms;
    skeletal_model_t *skel_model = skel->model;


    // -------------–
    // IQMFIXME - Remove these
    // -------------–
    // Test debug -- Draw bones at their rest positions...
    // mat3x4_t *bone_rest_transforms = get_member_from_offset(skel_model->bone_rest_transforms, skel_model);
    // bone_transforms = bone_rest_transforms;
    // -------------–
    // mat3x4_t ident_mat;
    // Matrix3x4_LoadIdentity(ident_mat);
    // -------------–


    // Bone transform takes us from bone-space to model-space
    // bone_rest_transforms does the same?


    // -----------------------------------------
    // Draw bone axes
    // -----------------------------------------
    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_TEXTURE_2D);
    static float line_verts_x[6] = {0,0,0,     1,0,0}; // Verts for x-axis
    static float line_verts_y[6] = {0,0,0,     0,1,0}; // Verts for y-axis
    static float line_verts_z[6] = {0,0,0,     0,0,1}; // Verts for z-axis

    for(int i = 0; i < skel_model->n_bones; i++) {
        ScePspFMatrix4 bone_mat;
        Matrix3x4_to_ScePspFMatrix4(&bone_mat, &(bone_transforms[i]));
        sceGumPushMatrix();
        sceGumMultMatrix(&bone_mat);
        sceGumUpdateMatrix();


        sceGuColor(GU_COLOR(1.0,0.0,0.0,1.0)); // RGBA red
        sceGumDrawArray(GU_LINES,GU_VERTEX_32BITF|GU_TRANSFORM_3D, 2, nullptr, line_verts_x);
        sceGuColor(GU_COLOR(0.0,1.0,0.0,1.0)); // RGBA green
        sceGumDrawArray(GU_LINES,GU_VERTEX_32BITF|GU_TRANSFORM_3D, 2, nullptr, line_verts_y);
        sceGuColor(GU_COLOR(0.0,0.0,1.0,1.0)); // RGBA blue
        sceGumDrawArray(GU_LINES,GU_VERTEX_32BITF|GU_TRANSFORM_3D, 2, nullptr, line_verts_z);
        sceGumPopMatrix();
    }
    sceGuEnable(GU_DEPTH_TEST);
    sceGuEnable(GU_TEXTURE_2D);
    // -----------------------------------------
}


//
// Given an ent's skeleton state vars, updated the cl_skeletons entry for that ent.
// Returns a pointer to the ent's skeleton
// If the skeleton is missing or misconfigured, returns nullptr, causing model to be drawn at rest pose
//
skeletal_skeleton_t *cl_update_skel_for_ent(entity_t *ent) {

    // Con_Printf("cl_update_skel_for_ent 1\n");
    int cl_skel_idx = ent->skeletonindex;
    if(cl_skel_idx < 0 || cl_skel_idx >= MAX_SKELETONS) {
        // Con_Printf("cl_update_skel_for_ent 1.1\n");
        return nullptr;
    }
    // Con_Printf("cl_update_skel_for_ent 2\n");

    skeletal_skeleton_t *cl_skel = &(cl_skeletons[cl_skel_idx]);
    skeletal_model_t *ent_skel_model = static_cast<skeletal_model_t*>(Mod_Extradata(ent->model));
    // Con_Printf("cl_update_skel_for_ent 3\n");
    skeletal_model_t *skeleton_skel_model = cl_get_skel_model_for_modelidx(ent->skeleton_modelindex);
    // Con_Printf("cl_update_skel_for_ent 4\n");
    if(skeleton_skel_model == nullptr) {
        Con_Printf("Skeleton skel model is null (modelindex: %d). Returning cl_skel=nullptr (n_bones: %d, time: %.2f)\n", ent->skeleton_modelindex, ent_skel_model->n_bones, sv.time);
        return nullptr;
    }
    // Con_Printf("cl_update_skel_for_ent 5\n");
    skeletal_model_t *skeleton_anim_skel_model = cl_get_skel_model_for_modelidx(ent->skeleton_anim_modelindex);
    // Con_Printf("cl_update_skel_for_ent 6\n");
    if(skeleton_anim_skel_model == nullptr) {
        Con_Printf("Skeleton anim skel model is null (modelindex: %d). Returning cl_skel=nullptr (n_bones: %d, time: %.2f)\n", ent->skeleton_anim_modelindex, ent_skel_model->n_bones, sv.time);
        return nullptr;
    }
    // Con_Printf("cl_update_skel_for_ent 7\n");

    int skel_anim_framegroup_idx = ent->skeleton_anim_framegroup;
    float skel_anim_starttime = ent->skeleton_anim_start_time;
    float skel_anim_speed = ent->skeleton_anim_speed;

    // Con_Printf("cl_update_skel_for_ent 8\n");

    // If we are using a new skeletal model, reallocate memory for the skeleton
    if(cl_skel->model != skeleton_skel_model) {
        Con_Printf("Ent skel model mismatch, reinitializing (n_bones: %d, time: %.2f)\n", ent_skel_model->n_bones, sv.time);
        // It's safe to call this if the skeleton is already freed
        free_skeleton(cl_skel);
        // Allocate memory to fit the new skeleton's bones
        init_skeleton(cl_skel, ent->skeleton_modelindex, skeleton_skel_model);
    }

    // Con_Printf("cl_update_skel_for_ent 9\n");


    // Check if any of the skeleton state vars have changed:
    bool do_rebuild_skel = false;
    if(ent->skeleton_anim_modelindex != cl_skel->anim_modelindex) {
        do_rebuild_skel = true;
    }
    else if(skel_anim_framegroup_idx != cl_skel->anim_framegroup) {
        do_rebuild_skel = true;
    }
    else if(skel_anim_starttime != cl_skel->anim_starttime) {
        do_rebuild_skel = true;
    }
    else if(skel_anim_speed != cl_skel->anim_speed) {
        do_rebuild_skel = true;
    }
    else if(sv.time != cl_skel->last_build_time) {
        do_rebuild_skel = true;
    }

    // Con_Printf("cl_update_skel_for_ent 10\n");

    if(do_rebuild_skel) {
        // Con_Printf("cl_update_skel_for_ent 11\n");
        // Con_Printf("Skeleton anim skel model rebuilt (n_bones: %d, time: %.2f)\n", ent_skel_model->n_bones, sv.time);
        build_skeleton_for_start_time(cl_skel, skeleton_anim_skel_model, skel_anim_framegroup_idx, skel_anim_starttime, skel_anim_speed);
        // Con_Printf("cl_update_skel_for_ent 12\n");
        // Cache state vars for next time
        cl_skel->anim_modelindex = ent->skeleton_anim_modelindex;
        cl_skel->anim_framegroup = skel_anim_framegroup_idx;
        cl_skel->anim_starttime = skel_anim_starttime;
        cl_skel->anim_speed = skel_anim_speed;
        cl_skel->last_build_time = sv.time;
    }
    else {
        // Con_Printf("Skeleton anim skel model NOT rebuilt (n_bones: %d, time: %.2f)\n", ent_skel_model->n_bones, sv.time);
    }
    return cl_skel;
}

// TODO - Move these elsewhere
#define ZOMBIE_LIMB_STATE_HEAD      1
#define ZOMBIE_LIMB_STATE_ARM_L     2
#define ZOMBIE_LIMB_STATE_ARM_R     4
#define ZOMBIE_LIMB_STATE_LEG_L     8
#define ZOMBIE_LIMB_STATE_LEG_R     16
#define ZOMBIE_LIMB_STATE_FULL      (ZOMBIE_LIMB_STATE_HEAD | ZOMBIE_LIMB_STATE_ARM_L | ZOMBIE_LIMB_STATE_ARM_R | ZOMBIE_LIMB_STATE_LEG_L | ZOMBIE_LIMB_STATE_LEG_R)




// https://gist.github.com/ciembor/1494530#file-gistfile1-c-L86
float hue2rgb(float p, float q, float t) {
    if(t < 0) {
        t += 1;
    }
    if(t > 1) {
        t -= 1;
    }
    if(t < 1./6) {
        return p + (q-p) * 6 * t;
    }
    if(t < 1./2) {
        return q;
    }
    if(t < 2./3) {
        return p + (q-p) * (2./3 - t) * 6;
    }
    return p;
}



void bind_material(skeletal_material_t *material) {
    // Missing material = fullbright pink color
    if(material == nullptr) {
        sceGuColor(GU_COLOR(1.0,0.0,1.0,1.0)); // RGBA
        sceGuDisable(GU_TEXTURE_2D);
        return;
    }

    GL_Bind(material->texture_idx);
    if(material->fullbright) {
        sceGuColor(GU_COLOR(1.0,1.0,1.0,1.0)); // RGBA
    }
    else {
        sceGuColor(GU_COLOR(lightcolor[0], lightcolor[1], lightcolor[2], 1.0f));
    }
    sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
    // OR use ...
	// sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);

    if(material->fog == false) {
        // Fog_DisableGFog();
        // sceGuDisable(GU_CULL_FACE); // Backface culling
        // sceGuDisable(GU_CLIP_PLANES);
        sceGuDisable(GU_FOG);
        // FIXME - Somehow PSP is still culling things past "far" fog value... happens with map geom as well

        // --------------------------------------------------------------------
        // TODO - Make this part of disabling fog in `main`
        // --------------------------------------------------------------------
        // float farplanedist = (r_refdef.fog_start == 0 || r_refdef.fog_end < 0) ? 4096 : (r_refdef.fog_end + 16);
        float farplanedist = 4096;
        vrect_t* renderrect = &r_refdef.vrect;
        float screenaspect = (float)renderrect->width/renderrect->height;
        float fovx = screenaspect;
        float fovy = r_refdef.fov_y;
        sceGumMatrixMode(GU_PROJECTION);
        sceGumLoadIdentity();
        sceGumPerspective(fovy, fovx, 4, farplanedist);
        sceGumUpdateMatrix();
        sceGumMatrixMode(GU_MODEL);
        // --------------------------------------------------------------------
    }


    if(material->transparent) {
        sceGuEnable(GU_BLEND);
        sceGuEnable(GU_ALPHA_TEST);
        sceGuAlphaFunc(GU_GREATER, 0, 0xff);
    }

    if(material->shade_smooth) {
        sceGuShadeModel(GU_SMOOTH);
    }
    else {
        sceGuShadeModel(GU_FLAT);
    }

    if(material->add_color) {
        sceGuColor(GU_COLOR(material->add_color_red, material->add_color_green, material->add_color_blue, material->add_color_alpha));
        // sceGuColor(0xffffffff);
        // // ----------------------------------------------------------------
        // // Test func to sweep through the rainbow:
        // // ----------------------------------------------------------------
        // vec3_t hsl; // H in [0,1], S in [0,1], L in [0,1]
        // // hsl[0] = (sv.time * 40) % 360; // Sweep full rainbow every 9 seconds
        // hsl[0] = fmodf(sv.time * 20, 360) / 360.0f; // Sweep full rainbow every 18 seconds
        // hsl[1] = 1.0f; // Full saturation
        // hsl[2] = 0.5f; // Half lightness

        // vec3_t rgb;

        // // HSL -> RGB conversion
        // // https://gist.github.com/ciembor/1494530#file-gistfile1-c-L86
        
        // if(hsl[1] == 0) {
        //     rgb[0] = rgb[1] = rgb[2] = hsl[2];
        // }
        // else {
        //     float h = hsl[0];
        //     float s = hsl[1];
        //     float l = hsl[2];
        //     float q = l < 0.5 ? l * (1 + s) : l + s - l * s;
        //     float p = 2 * l - q;

        //     rgb[0] = hue2rgb(p,q,h + 1./3);
        //     rgb[1] = hue2rgb(p,q,h);
        //     rgb[2] = hue2rgb(p,q,h - 1./3);
        // }
        // // Con_Printf("Binding color HSL: (%.2f, %.2f, %.2f), RGB: (%.2f, %.2f, %.2f)\n", hsl[0] * 360.0f, hsl[1], hsl[2], rgb[0], rgb[1], rgb[2]);
        // sceGuColor(GU_COLOR(rgb[0], rgb[1], rgb[2], 1.0f));
        // // ----------------------------------------------------------------
        sceGuTexFunc(GU_TFX_ADD, GU_TCC_RGBA);
    }

    // ------------------------------------------------------------------------
    // Chrome / skybox reflective material
    // ------------------------------------------------------------------------
    // bool chrome = true; // fixme
    // // if(chrome && skybox_name[0]) {
    // if(chrome) {
    //     sceGuColor(0xff000000); // AA BB GG RR
    //     // sceGuColor(0xff222222); // AA BB GG RR
    //     // GL_Bind(skyimage[0]); // Bind right-side skybox texture (some ground and some sky)
    //     GL_Bind(material->texture_idx); // Bind built-in material
    //     sceGuTexFunc(GU_TFX_ADD, GU_TCC_RGB);
    //     // sceGuTexFilter(GU_LINEAR,GU_LINEAR);
    //     // float envmap_angle = -2.0f * frame * (GU_PI / 180.0f);
    //     float envmap_angle = -2.0f * (GU_PI / 180.0f);
    //     float envmap_cos = cosf(envmap_angle);
    //     float envmap_sin = sinf(envmap_angle);
    //     ScePspFVector3 envmap_matrix_columns[2] = {
    //         {  envmap_cos, envmap_sin, 0.0f,},
    //         { -envmap_sin, envmap_cos, 0.0f }
    //     };
    //     sceGuLight( 1, GU_DIRECTIONAL, GU_DIFFUSE, &envmap_matrix_columns[0]); // Stash envmap matrix column 1 as light 1 data
    //     sceGuLight( 2, GU_DIRECTIONAL, GU_DIFFUSE, &envmap_matrix_columns[1]); // Stash envmap matrix column 2 as light 2 data
    //     sceGuTexMapMode(
    //         GU_ENVIRONMENT_MAP,
    //         1,  // Use light 1's data as envmap matrix column 1
    //         2   // Use light 2's data as envmap matrix column 2
    //     );
    // }
    // ------------------------------------------------------------------------
}

void cleanup_material(skeletal_material_t *material) {
    if(material == nullptr) {
        sceGuEnable(GU_TEXTURE_2D);
        return;
    }
    sceGuColor(0xffffffff);
    sceGuShadeModel(GU_FLAT);


    if(material->fog == false) {
        // Fog_EnableGFog();
        sceGuEnable(GU_FOG);
        // --------------------------------------------------------------------
        // Re-enable the fog perspective matrix to default dquake value
        // TODO - Make this part of enabling fog in `main`
        // --------------------------------------------------------------------
        float farplanedist = (r_refdef.fog_start == 0 || r_refdef.fog_end < 0) ? 4096 : (r_refdef.fog_end + 16); 
        vrect_t* renderrect = &r_refdef.vrect;
        float screenaspect = (float)renderrect->width/renderrect->height;
        float fovx = screenaspect;
        float fovy = r_refdef.fov_y;
        sceGumMatrixMode(GU_PROJECTION);
        sceGumLoadIdentity();
        sceGumPerspective(fovy, fovx, 4, farplanedist);
        sceGumUpdateMatrix();
        sceGumMatrixMode(GU_MODEL);
        // --------------------------------------------------------------------
    }


    // sceGuEnable(GU_CLIP_PLANES);
    // sceGuEnable(GU_CULL_FACE); // Backface culling

    // -------------------------
    // If envmap:
    // sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
    // sceGuTexMapMode(GU_TEXTURE_COORDS,1,2);
    // -------------------------
    // If add_color...
    sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
}

void R_DrawIQMModel(entity_t *ent) {
    // return; // IQMFIXME
    // Con_Printf("Draw IQM start\n");
    // Con_Printf("IQM model draw for ent with skelindex: %d\n", ent->skeletonindex);
    // --
    // Con_Printf("IQM model draw for ent with:\n");
    // Con_Printf("\tskeletonindex: %d\n", ent->skeletonindex);
    // Con_Printf("\tskeleton_modelindex: %d\n", ent->skeleton_modelindex);
    // Con_Printf("\tskeleton_anim_modelindex: %d\n", ent->skeleton_anim_modelindex);
    // Con_Printf("\tskeleton_anim_framegroup: %d\n", ent->skeleton_anim_framegroup);
    // Con_Printf("\tskeleton_anim_start_time: %.2f\n", ent->skeleton_anim_start_time);
    // Con_Printf("\tskeleton_anim_speed: %.2f\n", ent->skeleton_anim_speed);


    // FIXME - hardcoded references to other models...
    // model_t *zombie_model_with_anims = Mod_ForName("models/ai/zombie_with_anims.iqm", qtrue);
    // model_t *zombie_mesh = Mod_ForName("models/ai/zombie_mesh.iqm", qtrue);
    // model_t *zombie_anim_walk1 = Mod_ForName("models/ai/anim1_v2.iqm", qtrue);
    // model_t *zombie_anim_walk2 = Mod_ForName("models/ai/anim2_v2.iqm", qtrue);
    // model_t *zombie_mesh = Mod_ForName("models/ai/iqm/nazi_zombie.iqm", qtrue);
    // model_t *zombie_anim_walk1 = Mod_ForName("models/ai/iqm/zombie_anim_walk1.iqm", qtrue);
    // model_t *zombie_anim_walk2 = Mod_ForName("models/ai/iqm/zombie_anim_walk2.iqm", qtrue);


    // skeletal_model_t *skel_model = (skeletal_model_t*) Mod_Extradata(zombie_mesh);
    // skeletal_model_t *anim_model = (skeletal_model_t*) Mod_Extradata(zombie_anim_walk1);
    // skeletal_model_t *anim_model = (skeletal_model_t*) Mod_Extradata(zombie_anim_walk1);


    skeletal_model_t *skel_model = static_cast<skeletal_model_t*>(Mod_Extradata(ent->model));
    // Con_Printf("R_DrawIQMModel - n_bones: %d, time: %.2f\n", skel_model->n_bones, realtime);

    // TODO - If skeleton index is being used with new skel model index, clear and refill...
    // TODO - Rebuild skeleton

    // --------------–--------------–--------------–--------------–------------
    // Build the skeleton
    // --------------–--------------–--------------–--------------–------------
    skeletal_skeleton_t *cl_skel = cl_update_skel_for_ent(ent);



    // ===================================================
    // // skeletal_model_t *skel_model = (skeletal_model_t*) Mod_Extradata(ent->model);
    // // FIXME - Hardcoded skeleton index...
    // int cl_skel_idx = 0;

    // if(cl_n_skeletons == 0){
    //     // Con_Printf("Client has zero skeletons, building one!\n");
    //     cl_n_skeletons++;
    //     init_skeleton(&(cl_skeletons[cl_skel_idx]), skel_model);
    // }

    // int framegroup_idx = 0;
    // float frametime = sv.time;
    // build_skeleton(&cl_skeletons[cl_skel_idx], anim_model, framegroup_idx, frametime);
    // // build_skeleton(&cl_skeletons[cl_skel_idx], anim_model, framegroup_idx, 0.0f);
    // --------------–--------------–--------------–--------------–------------



    // =========================
    // Temp hack to animate vmodel without a skeleton in QC?
    // =========================


    // ------------------------------------------------------------------------
    // Perform frustum culling based on bone hitbox locations
    // FIXME - a skeletal model has no bones, should be based on rest-pose vert mins / maxs
    // ------------------------------------------------------------------------
    vec3_t ent_model_mins;
    vec3_t ent_model_maxs;
    if(cl_skel != nullptr) {
        cl_get_skel_model_bounds( cl_skel->modelindex, cl_skel->anim_modelindex, ent_model_mins, ent_model_maxs);
        // Con_Printf("Ent model fresh bounds 1: (bones: %d) (mins: [%.1f, %.1f, %.1f], maxs: [%.1f, %.1f, %.1f])\n", skel_model->n_bones, ent_model_mins[0], ent_model_mins[1], ent_model_mins[2], ent_model_maxs[0], ent_model_maxs[1], ent_model_maxs[2]);
    }
    else {
        cl_get_skel_model_bounds( ent->modelindex, -1, ent_model_mins, ent_model_maxs);
        // Con_Printf("Ent model fresh bounds 2: (bones: %d) (mins: [%.1f, %.1f, %.1f], maxs: [%.1f, %.1f, %.1f])\n", skel_model->n_bones, ent_model_mins[0], ent_model_mins[1], ent_model_mins[2], ent_model_maxs[0], ent_model_maxs[1], ent_model_maxs[2]);
        // Con_Printf("Ent model fresh bounds 2: modelindex: %d\n", ent->modelindex);
    }


    mat3x4_t model_to_world_transform;
    R_BlendedRotateForEntity_2(ent, model_to_world_transform);

    vec3_t ent_world_mins;
    vec3_t ent_world_maxs;
    // Calculate a new world-space AABB given the ent's rotation / translation
    transform_AABB(ent_model_mins, ent_model_maxs, model_to_world_transform, ent_world_mins, ent_world_maxs);



    // Con_Printf("R_DrawIQMModel - R_CullBox test n_bones: %d, time: %.2f\n", skel_model->n_bones, realtime);
    // Con_Printf("Ent world bounds: (mins: [%.1f, %.1f, %.1f], maxs: [%.1f, %.1f, %.1f])\n", ent_world_mins[0], ent_world_mins[1], ent_world_mins[2], ent_world_maxs[0], ent_world_maxs[1], ent_world_maxs[2]);
    // Con_Printf("Ent scale: %d (default: %d, decoded: %f)\n", ent->scale, ENTSCALE_DEFAULT,ENTSCALE_DECODE(ent->scale));

    // FIXME - Somehow vmodel ent has 0 scale? How does aliasmdl handle this?

    // --
    // Con_Printf("Ent model-to-world:\n");
    // Con_Printf("\t[%.1f, %.1f, %.1f, %.1f]\n", model_to_world_transform[0][0], model_to_world_transform[0][1], model_to_world_transform[0][2], model_to_world_transform[0][3]);
    // Con_Printf("\t[%.1f, %.1f, %.1f, %.1f]\n", model_to_world_transform[1][0], model_to_world_transform[1][1], model_to_world_transform[1][2], model_to_world_transform[1][3]);
    // Con_Printf("\t[%.1f, %.1f, %.1f, %.1f]\n", model_to_world_transform[2][0], model_to_world_transform[2][1], model_to_world_transform[2][2], model_to_world_transform[2][3]);
    // --
	if (R_CullBox(ent_world_mins, ent_world_maxs) == 2) {
        // Con_Printf("R_DrawIQMModel - R_CullBox fail n_bones: %d, time: %.2f\n", skel_model->n_bones, realtime);
		return;
    }



    // if(skel_model->n_bones == 41) {
    //    Con_Printf("R_DrawIQMModel - For model with 41 bones: ent->skeletonindex: %d\n", ent->skeletonindex);
    //    Con_Printf("R_DrawIQMModel - For model with 41 bones: ent->skleton_modelindex: %d\n", ent->skeleton_modelindex);
    //    Con_Printf("R_DrawIQMModel - For model with 41 bones: ent->skleton_anim_modelindex: %d\n", ent->skeleton_anim_modelindex);
    //    Con_Printf("R_DrawIQMModel - For model with 41 bones: ent->skeleton_anim_framegroup: %d\n", ent->skeleton_anim_framegroup);
    //    Con_Printf("R_DrawIQMModel - For model with 41 bones: ent->skeleton_anim_start_time: %f\n", ent->skeleton_anim_start_time);
    //    Con_Printf("R_DrawIQMModel - For model with 41 bones: ent->skeleton_anim_speed: %f\n", ent->skeleton_anim_speed);
    // }
    // Con_Printf("R_DrawIQMModel - R_CullBox success n_bones: %d, time: %.2f\n", skel_model->n_bones, realtime);



    // Con_Printf("-----------------------------------------\n");
    // Con_Printf("Ent model n_bones: %d\n", skel_model->n_bones);
    // Con_Printf("Ent model rest bounds: (mins: [%.1f, %.1f, %.1f], maxs: [%.1f, %.1f, %.1f])\n", skel_model->rest_pose_bone_hitbox_mins[0], skel_model->rest_pose_bone_hitbox_mins[1], skel_model->rest_pose_bone_hitbox_mins[2], skel_model->rest_pose_bone_hitbox_maxs[0], skel_model->rest_pose_bone_hitbox_maxs[1], skel_model->rest_pose_bone_hitbox_maxs[2]);
    // Con_Printf("Ent model anim bounds: (mins: [%.1f, %.1f, %.1f], maxs: [%.1f, %.1f, %.1f])\n", skel_model->anim_bone_hitbox_mins[0], skel_model->anim_bone_hitbox_mins[1], skel_model->anim_bone_hitbox_mins[2], skel_model->anim_bone_hitbox_maxs[0], skel_model->anim_bone_hitbox_maxs[1], skel_model->anim_bone_hitbox_maxs[2]);
    // // --
    // Con_Printf("Ent model bounds: (mins: [%.1f, %.1f, %.1f], maxs: [%.1f, %.1f, %.1f])\n", ent_model_mins[0], ent_model_mins[1], ent_model_mins[2], ent_model_maxs[0], ent_model_maxs[1], ent_model_maxs[2]);
    // Con_Printf("Ent model-to-world:\n");
    // Con_Printf("\t[%.1f, %.1f, %.1f, %.1f]\n", model_to_world_transform[0][0], model_to_world_transform[0][1], model_to_world_transform[0][2], model_to_world_transform[0][3]);
    // Con_Printf("\t[%.1f, %.1f, %.1f, %.1f]\n", model_to_world_transform[1][0], model_to_world_transform[1][1], model_to_world_transform[1][2], model_to_world_transform[1][3]);
    // Con_Printf("\t[%.1f, %.1f, %.1f, %.1f]\n", model_to_world_transform[2][0], model_to_world_transform[2][1], model_to_world_transform[2][2], model_to_world_transform[2][3]);
    // Con_Printf("Ent world bounds: (mins: [%.1f, %.1f, %.1f], maxs: [%.1f, %.1f, %.1f])\n", ent_world_mins[0], ent_world_mins[1], ent_world_mins[2], ent_world_maxs[0], ent_world_maxs[1], ent_world_maxs[2]);
    // Con_Printf("-----------------------------------------\n");




    // ------------------------------------------------------------------------


    sceGumPushMatrix();
    ScePspFMatrix4 model_to_world_mat;
    Matrix3x4_to_ScePspFMatrix4(&model_to_world_mat, &model_to_world_transform);
    sceGumMultMatrix(&model_to_world_mat);
    sceGumUpdateMatrix();
    // R_BlendedRotateForEntity(ent, 0, ent->scale);



    // sceGuDisable(GU_TEXTURE_2D);
    // sceGuColor(0x000000);
    // FIXME - Update this to draw meshes and submeshes
    // for(int i = 0; i < skel_model->n_meshes; i++) {
    //     skeletal_mesh_t *mesh = &((skeletal_mesh_t*) ((uint8_t*)skel_model + (int)skel_model->meshes))[i];
    //     // FIXME
    //     vertex_t *mesh_verts = (vertex_t*) ((uint8_t*) skel_model + (int) mesh->verts);
    //     uint16_t *mesh_tri_verts = (uint16_t*) ((uint8_t*) skel_model + (int) mesh->tri_verts);
    //     sceGumDrawArray(GU_TRIANGLES,GU_INDEX_16BIT|GU_TEXTURE_32BITF|GU_NORMAL_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, mesh->n_tris * 3, mesh_tri_verts, mesh_verts);
    // }
    


    skeletal_mesh_t *meshes = get_member_from_offset(skel_model->meshes, skel_model);

    // TODO - Need to somehow detect when this is being called on a zombie ent, to know when to check limbs state
    // Con_Printf("Ent limb state: %d\n", ent->limbs_state);

    // -------------------------------------------------------------------------
    // Hack to set the geomset values for zombie limbs
    // TODO - Move this elsewhere
    // -------------------------------------------------------------------------
    // All zombie limb meshes have geomid 0, setting it to 1 hides the mesh
    ent->iqm_geomsets[1] = (ent->limbs_state & ZOMBIE_LIMB_STATE_HEAD) ? 0 : 1; // Head
    ent->iqm_geomsets[2] = (ent->limbs_state & ZOMBIE_LIMB_STATE_HEAD) ? 0 : 1; // Eyes
    ent->iqm_geomsets[3] = (ent->limbs_state & ZOMBIE_LIMB_STATE_ARM_L) ? 0 : 1; // Left Arm
    ent->iqm_geomsets[4] = (ent->limbs_state & ZOMBIE_LIMB_STATE_LEG_L) ? 0 : 1; // Left Leg
    ent->iqm_geomsets[5] = (ent->limbs_state & ZOMBIE_LIMB_STATE_LEG_R) ? 0 : 1; // Right Leg
    ent->iqm_geomsets[6] = (ent->limbs_state & ZOMBIE_LIMB_STATE_ARM_R) ? 0 : 1; // Right Arm
    // -------------------------------------------------------------------------

    // // -------------------------------------------------------------------------
    // // 
    // // -------------------------------------------------------------------------
    // if(ent->eye_color != default) { 
    //     // set global
    //     // set colormods
    // }

    // R_DrawIQMModel();

    // if(ent->eye_color != default) { 
    //     // unset global
    // }
    // // -------------------------------------------------------------------------



    // ------------------------------------------------------------------------
    // Entity lighting
    // TODO - Disable this code when all models use fullbright materials?
    // ------------------------------------------------------------------------
    R_LightPoint(ent->origin); // Sets `lightcolor` global
    vec3_t dist;
	for (int lnum=0 ; lnum < MAX_DLIGHTS ; lnum++) {
		if (cl_dlights[lnum].die >= cl.time) {
			VectorSubtract(ent->origin, cl_dlights[lnum].origin, dist);
			float add = cl_dlights[lnum].radius - Length(dist);
			if (add > 0) {
				lightcolor[0] += add * cl_dlights[lnum].color[0];
				lightcolor[1] += add * cl_dlights[lnum].color[1];
				lightcolor[2] += add * cl_dlights[lnum].color[2];
			}
		}
	}
	if(r_model_brightness.value) {
		lightcolor[0] += 32;
		lightcolor[1] += 32;
		lightcolor[2] += 32;
	}
	for(int g = 0; g < 3; g++) {
        lightcolor[g] = lightcolor[g] > 125 ? 125 : lightcolor[g] < 8 ? 8 : lightcolor[g];
        // lightcolor[g] = std::min(std::max(8, (float) lightcolor[g]), 125);
    }
    VectorScale(lightcolor, 1.0f / 200.0f, lightcolor);
    // ------------------------------------------------------------------------


    for(int i = 0; i < skel_model->n_meshes; i++) {
        // Con_Printf("Drawing mesh %d\n", i);
        skeletal_mesh_t *mesh = &meshes[i];

        // // Check geomset / geomid values
        // if(mesh->geomset >= 0 && mesh->geomset < IQM_MAX_GEOMSETS) {
        //     if(mesh->geomid != ent->iqm_geomsets[mesh->geomset]) {
        //         continue;
        //     }
        // }


        int material_idx = mesh->material_idx;
        int *material_n_skins = get_member_from_offset(skel_model->material_n_skins, skel_model);
        
        skeletal_material_t *material = nullptr;
        if(material_idx >= 0 && skel_model->n_materials > 0) {
            skeletal_material_t **materials = get_member_from_offset(skel_model->materials, skel_model);
            skeletal_material_t *material_skins = get_member_from_offset(materials[material_idx], skel_model);
            int skin_idx = ent->skinnum;
            // If this material doesn't have requested skin number, get modulus
            // skin_idx to always get a valid skin
            skin_idx = skin_idx % material_n_skins[material_idx];
            material = &(material_skins[skin_idx]);
        }
        bind_material(material);



        // TODO - Register these material overrides somewhere?
        // "material.eyeglow_tex.fullbright" = true;
        // "material.eyeglow_tex.add_color" = [1,0,0,0];


        // // ------------------------------
        // // TODO - Figure out how to get rid of it here
        // // ------------------------------
        // if(is_zombie_ent && !strcmp(material_name, "eyeglow_tex")) {
        //     // Override eyeglow color
        //     sceGuColor(GU_COLOR(ent->eyeglow[0], ent->eyeglow[1], ent->eyeglow[2], ent->eyeglow[3]));
        // }
        // // ------------------------------

        // Con_Printf("\tn_submeshes: %d\n", mesh->n_submeshes);
        skeletal_mesh_t *submeshes = get_member_from_offset(mesh->submeshes, skel_model);
        for(int j = 0; j < mesh->n_submeshes; j++) {
            // Con_Printf("Drawing mesh %d submesh %d\n", i, j);
            skeletal_mesh_t *submesh = &submeshes[j];
            skel_vertex_i8_t *vert8s = get_member_from_offset(submesh->vert8s, skel_model);
            skel_vertex_i16_t *vert16s = get_member_from_offset(submesh->vert16s, skel_model);



            bind_submesh_bones(submesh, cl_skel);

            // if(something) {
            // ----------------------------------------------------------------
            // Draw 16-bit vertices
            // ----------------------------------------------------------------
#ifdef IQM_LOAD_NORMALS
            int draw_flags = GU_WEIGHTS(8)|GU_WEIGHT_8BIT|GU_TEXTURE_16BIT|GU_NORMAL_16BIT|GU_VERTEX_16BIT|GU_TRANSFORM_3D;
#else
            int draw_flags = GU_WEIGHTS(8)|GU_WEIGHT_8BIT|GU_TEXTURE_16BIT|GU_VERTEX_16BIT|GU_TRANSFORM_3D;
#endif // IQM_LOAD_NORMALS
            sceGumDrawArray(GU_TRIANGLES, draw_flags, submesh->n_tris * 3, nullptr, vert16s );
            // ----------------------------------------------------------------


            // TODO - Make this a branch based on some variable
//             // ----------------------------------------------------------------
//             // Draw 8-bit vertices
//             // ----------------------------------------------------------------
// #ifdef IQM_LOAD_NORMALS
//             int draw_flags = GU_WEIGHTS(8)|GU_WEIGHT_8BIT|GU_TEXTURE_8BIT|GU_NORMAL_8BIT|GU_VERTEX_8BIT|GU_TRANSFORM_3D; 
// #else
//             int draw_flags = GU_WEIGHTS(8)|GU_WEIGHT_8BIT|GU_TEXTURE_8BIT|GU_VERTEX_8BIT|GU_TRANSFORM_3D;
// #endif // IQM_LOAD_NORMALS
//             sceGumDrawArray( GU_TRIANGLES, draw_flags, submesh->n_tris * 3, nullptr, vert8s );
//             // ----------------------------------------------------------------

        }
        cleanup_material(material);
    }


    // TODO - Add cvar or something else to enable drawing:
    if(cl_skel != nullptr) {
        // draw_skeleton_bboxes(ent, cl_skel, true, true);
    }

    if(cl_skel != nullptr) {
        // draw_skeleton_bone_axes(ent, cl_skel);
    }
    else {
        // draw_skeleton_rest_pose_bone_axes(skel_model); // - enable this
    }

    // TODO - Hide behind r_showbboxes cvar:
    // FIXME - Looks like ent bbox AABB is not networked to client.
    // if(something) {
    //    draw_ent_aabb(ent);
    // }


    // for(int i = 0; i < skel_model->n_bones; i++) {
    //     char *bone_name = (char*)((uint8_t*) skel_model + (int) bone_names[i]);
    //     // Con_Printf("Drawing bone: %i, \"%s\"\n", i, bone_name);
    // }
    sceGumPopMatrix();
    // Con_Printf("\tDraw IQM end\n");
}




// ============================================================================
// IQM model x IQM animation bounding box size registration system
//
// Any IQM model can be paired with any IQM animation. Thus, the axis-aligned
// bounding box (AABB) of a model depends on the animation that its playing.
// 
// This system computes and caches the AABBs for pairs of IQM models and IQM
// animations.
//
// ============================================================================


// List of jagged arrays containing indices into `bounds_min` / `bounds_max`
int16_t *skel_model_bounds_idxs[MAX_MODELS] = {nullptr};
int16_t skel_model_bounds_count[MAX_MODELS] = {0};

vec3_t *bounds_mins = nullptr;
vec3_t *bounds_maxs = nullptr;
uint16_t bounds_count = 0;
uint16_t bounds_capacity = 0;


 
void calc_skel_model_bounds_for_rest_pose(skeletal_model_t *skel_model, vec3_t model_mins, vec3_t model_maxs) {
#ifdef IQMDEBUG_CALC_MODEL_BOUNDS
    Con_Printf("calc_skel_model_bounds_for_rest_pose started\n");
#endif // IQMDEBUG_CALC_MODEL_BOUNDS

    if(skel_model == nullptr) {
#ifdef IQMDEBUG_CALC_MODEL_BOUNDS
        Con_Printf("calc_skel_model_bounds_for_rest_pose finished 1\n");
#endif // IQMDEBUG_CALC_MODEL_BOUNDS
        return;
    }
    // In case we don't have any bones, load default mins/maxs:
    VectorSet(model_mins, -1,-1,-1);
    VectorSet(model_maxs, 1,1,1);
    if(skel_model->n_bones <= 0) {
#ifdef IQMDEBUG_CALC_MODEL_BOUNDS
        Con_Printf("calc_skel_model_bounds_for_rest_pose finished 1\n");
#endif // IQMDEBUG_CALC_MODEL_BOUNDS
        return;
    }


    // ------------------------------------------------------------------------
    // 
    // ------------------------------------------------------------------------
    mat3x4_t *bone_rest_transforms = get_member_from_offset(skel_model->bone_rest_transforms, skel_model);

    // NOTE - Though skeletons keep their own copy of bone hitbox ofs and scales
    // NOTE   This function disregards that and only uses the bone hitboxes as
    // NOTE   defined in the IQM model (and its JSON counterpart)
    // NOTE - Thus, if you programmatically increase the hitbox for a bone at 
    // NOTE   runtime, that change won't be reflected in this bounds calculation,
    // NOTE   and thus the bone hitbox may be larger than the overall bounds 
    // NOTE   calculated here against the default bone hitbox sizes. This is a 
    // NOTE   tricky edge case to watch out for. If you know you'll need that, 
    // NOTE   maybe calculate the bounds here using the skeleton's current 
    // NOTE   hitbox definition instead, but you'll need to come up with some 
    // NOTE   other non-model-based bounds caching system. - blubs 
    vec3_t *bone_hitbox_ofs = get_member_from_offset(skel_model->bone_hitbox_ofs, skel_model);
    vec3_t *bone_hitbox_scale = get_member_from_offset(skel_model->bone_hitbox_scale, skel_model);

    const int unit_bbox_n_corners = 8;
    static vec3_t unit_bbox_corners[8] = {
        // {-1,-1,-1}, // Left,  Back,  Bottom
        // { 1,-1,-1}, // Right, Back,  Bottom
        // { 1, 1,-1}, // Right, Front, Bottom
        // {-1, 1,-1}, // Left,  Front, Bottom
        // {-1,-1, 1}, // Left,  Back,  Top
        // { 1,-1, 1}, // Right, Back,  Top
        // { 1, 1, 1}, // Right, Front, Top
        // {-1, 1, 1}, // Left,  Front, Top
        {-0.5,-0.5,-0.5}, // Left,  Back,  Bottom
        { 0.5,-0.5,-0.5}, // Right, Back,  Bottom
        { 0.5, 0.5,-0.5}, // Right, Front, Bottom
        {-0.5, 0.5,-0.5}, // Left,  Front, Bottom
        {-0.5,-0.5, 0.5}, // Left,  Back,  Top
        { 0.5,-0.5, 0.5}, // Right, Back,  Top
        { 0.5, 0.5, 0.5}, // Right, Front, Top
        {-0.5, 0.5, 0.5}, // Left,  Front, Top
    };

    // Set to `true` after first pass
    bool bounds_initialized = true;

#ifdef IQMDEBUG_CALC_MODEL_BOUNDS
    Con_Printf("calc_skel_model_bounds_for_rest_pose 3\n");
#endif // IQMDEBUG_CALC_MODEL_BOUNDS

    // Calculate the current pose transform for all bones
    for(uint32_t bone_idx = 0; bone_idx < skel_model->n_bones; bone_idx++) {
        // Bone local-space -> model-space for rest pose is pre-calculated
        
        // Apply bone AABB scale / ofs:
        mat3x4_t bone_hitbox_transform;
        Matrix3x4_scale_translate(bone_hitbox_transform, bone_hitbox_scale[bone_idx], bone_hitbox_ofs[bone_idx]);
        // Apply bone's rest pose transform:
        mat3x4_t bone_transform;
        Matrix3x4_ConcatTransforms(bone_transform, bone_rest_transforms[bone_idx], bone_hitbox_transform);

#ifdef IQMDEBUG_CALC_MODEL_BOUNDS
        Con_Printf("Checking rest pose bone mins / maxs for bone %d\n", bone_idx);
        Con_Printf("\tBone ofs: [%f, %f, %f]\n", bone_hitbox_ofs[bone_idx][0], bone_hitbox_ofs[bone_idx][1], bone_hitbox_ofs[bone_idx][2]);
        Con_Printf("\tBone scale: [%f, %f, %f]\n", bone_hitbox_scale[bone_idx][0], bone_hitbox_scale[bone_idx][1], bone_hitbox_scale[bone_idx][2]);
        Con_Printf("\tBone hitbox transform:\n");
        Con_Printf("\t\t[%f, %f, %f, %f]\n", bone_hitbox_transform[0][0], bone_hitbox_transform[0][1], bone_hitbox_transform[0][2], bone_hitbox_transform[0][3]);
        Con_Printf("\t\t[%f, %f, %f, %f]\n", bone_hitbox_transform[1][0], bone_hitbox_transform[1][1], bone_hitbox_transform[1][2], bone_hitbox_transform[1][3]);
        Con_Printf("\t\t[%f, %f, %f, %f]\n", bone_hitbox_transform[2][0], bone_hitbox_transform[2][1], bone_hitbox_transform[2][2], bone_hitbox_transform[2][3]);
        Con_Printf("\t\t[%f, %f, %f, %f]\n", 0.0f, 0.0f, 0.0f, 1.0f);
        Con_Printf("\tBone rest transform:\n");
        Con_Printf("\t\t[%f, %f, %f, %f]\n", bone_rest_transforms[bone_idx][0][0], bone_rest_transforms[bone_idx][0][1], bone_rest_transforms[bone_idx][0][2], bone_rest_transforms[bone_idx][0][3]);
        Con_Printf("\t\t[%f, %f, %f, %f]\n", bone_rest_transforms[bone_idx][1][0], bone_rest_transforms[bone_idx][1][1], bone_rest_transforms[bone_idx][1][2], bone_rest_transforms[bone_idx][1][3]);
        Con_Printf("\t\t[%f, %f, %f, %f]\n", bone_rest_transforms[bone_idx][2][0], bone_rest_transforms[bone_idx][2][1], bone_rest_transforms[bone_idx][2][2], bone_rest_transforms[bone_idx][2][3]);
        Con_Printf("\t\t[%f, %f, %f, %f]\n", 0.0f, 0.0f, 0.0f, 1.0f);
        Con_Printf("\tBone transform:\n");
        Con_Printf("\t\t[%f, %f, %f, %f]\n", bone_transform[0][0], bone_transform[0][1], bone_transform[0][2], bone_transform[0][3]);
        Con_Printf("\t\t[%f, %f, %f, %f]\n", bone_transform[1][0], bone_transform[1][1], bone_transform[1][2], bone_transform[1][3]);
        Con_Printf("\t\t[%f, %f, %f, %f]\n", bone_transform[2][0], bone_transform[2][1], bone_transform[2][2], bone_transform[2][3]);
        Con_Printf("\t\t[%f, %f, %f, %f]\n", 0.0f, 0.0f, 0.0f, 1.0f);
#endif // IQMDEBUG_CALC_MODEL_BOUNDS


        // Project the 8 hitbox corners into model-space, update the overall model mins / maxs
        for(int i = 0; i < unit_bbox_n_corners; i++) {
            vec3_t model_space_corner;
            Matrix3x4_VectorTransform( bone_transform, unit_bbox_corners[i], model_space_corner);

            // On first pass, set the mins / maxs
            if(!bounds_initialized) {
                VectorCopy(model_space_corner, model_mins);
                VectorCopy(model_space_corner, model_maxs);
                bounds_initialized = true;
            }
            else {
                VectorMin(model_space_corner, model_mins, model_mins);
                VectorMax(model_space_corner, model_maxs, model_maxs);
            }
        }
    }

#ifdef IQMDEBUG_CALC_MODEL_BOUNDS
    Con_Printf("calc_skel_model_bounds_for_rest_pose finished 4\n");
    Con_Printf("Final bounds calculated: (mins: [%.2f, %.2f, %.2f], maxs: [%.2f, %.2f, %.2f])\n",
        model_mins[0], model_mins[1], model_mins[2],
        model_maxs[0], model_maxs[1], model_maxs[2]
    );
#endif // IQMDEBUG_CALC_MODEL_BOUNDS
}



void calc_skel_model_bounds_for_anim(skeletal_model_t *skel_model, skeletal_model_t *anim_model, vec3_t model_mins, vec3_t model_maxs) {
#ifdef IQMDEBUG_CALC_MODEL_BOUNDS
    Con_Printf("calc_skel_model_bounds_for_anim started\n");
#endif // IQMDEBUG_CALC_MODEL_BOUNDS

    if(skel_model == nullptr || anim_model == nullptr) {
#ifdef IQMDEBUG_CALC_MODEL_BOUNDS
        Con_Printf("calc_skel_model_bounds_for_anim finished 1\n");
#endif // IQMDEBUG_CALC_MODEL_BOUNDS
        return;
    }
    if(skel_model->n_bones <= 0) {
#ifdef IQMDEBUG_CALC_MODEL_BOUNDS
        Con_Printf("calc_skel_model_bounds_for_anim finished 2\n");
#endif // IQMDEBUG_CALC_MODEL_BOUNDS
        return;
    }

    // If no animation bones or frames, return skel model's rest pose hitbox bounds
    VectorCopy(skel_model->rest_pose_bone_hitbox_mins, model_mins);
    VectorCopy(skel_model->rest_pose_bone_hitbox_maxs, model_maxs);

    if(anim_model->n_bones <= 0 || anim_model->n_framegroups <= 0) {
#ifdef IQMDEBUG_CALC_MODEL_BOUNDS
        Con_Printf("calc_skel_model_bounds_for_anim finished 3\n");
#endif // IQMDEBUG_CALC_MODEL_BOUNDS
        return;
    }


    mat3x4_t *bone_rest_transforms = get_member_from_offset(skel_model->bone_rest_transforms, skel_model);
    int16_t *bone_parent_idx = get_member_from_offset(skel_model->bone_parent_idx, skel_model);
    vec3_t *anim_model_frames_bone_pos = get_member_from_offset(anim_model->frames_bone_pos, anim_model);
    quat_t *anim_model_frames_bone_rot = get_member_from_offset(anim_model->frames_bone_rot, anim_model);
    vec3_t *anim_model_frames_bone_scale = get_member_from_offset(anim_model->frames_bone_scale, anim_model);
    uint32_t *framegroup_start_frame = get_member_from_offset(anim_model->framegroup_start_frame, anim_model);
    uint32_t *framegroup_n_frames = get_member_from_offset(anim_model->framegroup_n_frames, anim_model);


    // NOTE - Though skeletons keep their own copy of bone hitbox ofs and scales
    // NOTE   This function disregards that and only uses the bone hitboxes as
    // NOTE   defined in the IQM model (and its JSON counterpart)
    // NOTE - Thus, if you programmatically increase the hitbox for a bone at 
    // NOTE   runtime, that change won't be reflected in this bounds calculation,
    // NOTE   and thus the bone hitbox may be larger than the overall bounds 
    // NOTE   calculated here against the default bone hitbox sizes. This is a 
    // NOTE   tricky edge case to watch out for. If you know you'll need that, 
    // NOTE   maybe calculate the bounds here using the skeleton's currnet 
    // NOTE   hitbox definition instead, but you'll need to come up with some 
    // NOTE   other non-model-based bounds caching system. - blubs 
    vec3_t *bone_hitbox_ofs = get_member_from_offset(skel_model->bone_hitbox_ofs, skel_model);
    vec3_t *bone_hitbox_scale = get_member_from_offset(skel_model->bone_hitbox_scale, skel_model);


    // Find the animation data bone index to pull for each skeleton bone index
    // Match them by bone name
    int32_t anim_bone_idxs[IQM_MAX_BONES];
    map_anim_bones_to_skel_bones(skel_model, anim_model, anim_bone_idxs);

    // Con_Printf("calc_skel_model_bounds_for_anim skel_model -> skel_anim bone map:\n");
    // Con_Printf("{");
    // for(int i = 0; i < skel_model->n_bones; i++) {
    //     Con_Printf("%d: %d, ", i, anim_bone_idxs[i]);
    // }
    // Con_Printf("}\n");


    // Each matrix takes us from bone-space to animated model-space
    mat3x4_t bone_transforms[IQM_MAX_BONES];

    const int unit_bbox_n_corners = 8;
    static vec3_t unit_bbox_corners[8] = {
        // {-1,-1,-1}, // Left,  Back,  Bottom
        // { 1,-1,-1}, // Right, Back,  Bottom
        // { 1, 1,-1}, // Right, Front, Bottom
        // {-1, 1,-1}, // Left,  Front, Bottom
        // {-1,-1, 1}, // Left,  Back,  Top
        // { 1,-1, 1}, // Right, Back,  Top
        // { 1, 1, 1}, // Right, Front, Top
        // {-1, 1, 1}, // Left,  Front, Top
        {-0.5,-0.5,-0.5}, // Left,  Back,  Bottom
        { 0.5,-0.5,-0.5}, // Right, Back,  Bottom
        { 0.5, 0.5,-0.5}, // Right, Front, Bottom
        {-0.5, 0.5,-0.5}, // Left,  Front, Bottom
        {-0.5,-0.5, 0.5}, // Left,  Back,  Top
        { 0.5,-0.5, 0.5}, // Right, Back,  Top
        { 0.5, 0.5, 0.5}, // Right, Front, Top
        {-0.5, 0.5, 0.5}, // Left,  Front, Top
    };
#ifdef IQMDEBUG_CALC_MODEL_BOUNDS
    Con_Printf("calc_skel_model_bounds_for_anim 4\n");
#endif // IQMDEBUG_CALC_MODEL_BOUNDS

    // Set to `true` after first pass
    bool bounds_initialized = true;


    for(uint16_t framegroup_idx = 0; framegroup_idx < anim_model->n_framegroups; framegroup_idx++) {
        uint32_t start_frame = framegroup_start_frame[framegroup_idx];
        uint32_t end_frame = start_frame + framegroup_n_frames[framegroup_idx];
        for(uint32_t frame_idx = start_frame; frame_idx < end_frame; frame_idx++) {

            // Calculate the current pose transform for all bones
            for(uint32_t bone_idx = 0; bone_idx < skel_model->n_bones; bone_idx++) {
                uint32_t anim_bone_idx = anim_bone_idxs[bone_idx];

                // If no animation data for this bone, set to rest pose and skip
                if(anim_bone_idx == -1) {
                    // ... set its transform to rest pose and skip it
                    Matrix3x4_Copy(bone_transforms[bone_idx], bone_rest_transforms[bone_idx]);
                }
                else {
                    vec3_t *frame_pos = &(anim_model_frames_bone_pos[anim_model->n_bones * frame_idx + anim_bone_idx]);
                    quat_t *frame_rot = &(anim_model_frames_bone_rot[anim_model->n_bones * frame_idx + anim_bone_idx]);
                    vec3_t *frame_scale = &(anim_model_frames_bone_scale[anim_model->n_bones * frame_idx + anim_bone_idx]);

                    // Current pose bone-space transform (relative to parent)
                    Matrix3x4_scale_rotate_translate(bone_transforms[bone_idx], *frame_scale, *frame_rot, *frame_pos);

                    // If we have a parent, concat parent transform to get model-space transform
                    int parent_bone_idx = bone_parent_idx[bone_idx];
                    if(parent_bone_idx >= 0) {
                        mat3x4_t temp; 
                        Matrix3x4_ConcatTransforms(temp, bone_transforms[parent_bone_idx], bone_transforms[bone_idx]);
                        Matrix3x4_Copy(bone_transforms[bone_idx], temp);
                    }
                    // If we don't have a parent, the bone-space transform _is_ the model-space transform
                }
                // bone_transform[bone_idx] now contains the bone-space -> model-space transform for this bone




                // Apply bone AABB scale / ofs:
                mat3x4_t bone_hitbox_transform;
                Matrix3x4_scale_translate(bone_hitbox_transform, bone_hitbox_scale[bone_idx], bone_hitbox_ofs[bone_idx]);
                // Apply bone's current pose transform:
                mat3x4_t bone_transform;
                Matrix3x4_ConcatTransforms(bone_transform, bone_transforms[bone_idx], bone_hitbox_transform);


#ifdef IQMDEBUG_CALC_MODEL_BOUNDS
                bool do_print = false;
                // bool do_print = true;
                // if(skel_model->n_bones < 25 && skel_model->n_meshes > 0 && start_frame == 0 && end_frame < 30) {
                //     do_print = true;
                // }

                if(do_print) {
                    Con_Printf("\tUpdating model mins/maxs for framegroup %d frame %d bone %d\n", framegroup_idx, frame_idx, bone_idx);
                    Con_Printf("\t\tGetting bone anim data for anim bone %d / %d for frame %d\n", anim_bone_idx, anim_model->n_bones, frame_idx);

                    Con_Printf("\t\tBone Transform:\n");
                    Con_Printf("\t\t\t[%.2f, %.2f, %.2f, %.2f]\n", bone_transforms[bone_idx][0][0], bone_transforms[bone_idx][0][1], bone_transforms[bone_idx][0][2], bone_transforms[bone_idx][0][3]);
                    Con_Printf("\t\t\t[%.2f, %.2f, %.2f, %.2f]\n", bone_transforms[bone_idx][1][0], bone_transforms[bone_idx][1][1], bone_transforms[bone_idx][1][2], bone_transforms[bone_idx][1][3]);
                    Con_Printf("\t\t\t[%.2f, %.2f, %.2f, %.2f]\n", bone_transforms[bone_idx][2][0], bone_transforms[bone_idx][2][1], bone_transforms[bone_idx][2][2], bone_transforms[bone_idx][2][3]);
                    Con_Printf("\t\t\t[%.2f, %.2f, %.2f, %.2f]\n", 0.0f, 0.0f, 0.0f, 1.0f);

                    Con_Printf("\t\tBone Hitbox Transform:\n");
                    Con_Printf("\t\t\t[%.2f, %.2f, %.2f, %.2f]\n", bone_hitbox_transform[0][0], bone_hitbox_transform[0][1], bone_hitbox_transform[0][2], bone_hitbox_transform[0][3]);
                    Con_Printf("\t\t\t[%.2f, %.2f, %.2f, %.2f]\n", bone_hitbox_transform[1][0], bone_hitbox_transform[1][1], bone_hitbox_transform[1][2], bone_hitbox_transform[1][3]);
                    Con_Printf("\t\t\t[%.2f, %.2f, %.2f, %.2f]\n", bone_hitbox_transform[2][0], bone_hitbox_transform[2][1], bone_hitbox_transform[2][2], bone_hitbox_transform[2][3]);
                    Con_Printf("\t\t\t[%.2f, %.2f, %.2f, %.2f]\n", 0.0f, 0.0f, 0.0f, 1.0f);

                    Con_Printf("\t\tBone + Bone Hitbox Transform:\n");
                    Con_Printf("\t\t\t[%.2f, %.2f, %.2f, %.2f]\n", bone_transform[0][0], bone_transform[0][1], bone_transform[0][2], bone_transform[0][3]);
                    Con_Printf("\t\t\t[%.2f, %.2f, %.2f, %.2f]\n", bone_transform[1][0], bone_transform[1][1], bone_transform[1][2], bone_transform[1][3]);
                    Con_Printf("\t\t\t[%.2f, %.2f, %.2f, %.2f]\n", bone_transform[2][0], bone_transform[2][1], bone_transform[2][2], bone_transform[2][3]);
                    Con_Printf("\t\t\t[%.2f, %.2f, %.2f, %.2f]\n", 0.0f, 0.0f, 0.0f, 1.0f);
                }
#endif // IQMDEBUG_CALC_MODEL_BOUNDS


                // Project the 8 hitbox corners into model-space, update the overall model mins / maxs
                for(int i = 0; i < unit_bbox_n_corners; i++) {
                    vec3_t model_space_corner;
                    Matrix3x4_VectorTransform( bone_transform, unit_bbox_corners[i], model_space_corner);

                    // On first pass, set the mins / maxs
                    if(!bounds_initialized) {
                        VectorCopy(model_space_corner, model_mins);
                        VectorCopy(model_space_corner, model_maxs);
                        bounds_initialized = true;
                    }
                    else {
                        VectorMax(model_space_corner, model_maxs, model_maxs);
                        VectorMin(model_space_corner, model_mins, model_mins);
                    }

#ifdef IQMDEBUG_CALC_MODEL_BOUNDS
                    if(do_print) {
                        Con_Printf("\t\tBBOX corner %d: [%.2f, %.2f, %.2f]\n", i, model_space_corner[0], model_space_corner[1], model_space_corner[2]);
                        Con_Printf("\t\tCurrent model bounds (mins: [%.2f, %.2f, %.2f], maxs: [%.2f, %.2f, %.2f])\n", 
                            model_mins[0], model_mins[1], model_mins[2],
                            model_maxs[0], model_maxs[1], model_maxs[2]
                        );
                    }
#endif // IQMDEBUG_CALC_MODEL_BOUNDS
                }
            }
        }
    }

#ifdef IQMDEBUG_CALC_MODEL_BOUNDS
    Con_Printf("calc_skel_model_bounds_for_anim finished 4\n");
    Con_Printf("Final bounds calculated: (mins: [%.2f, %.2f, %.2f], maxs: [%.2f, %.2f, %.2f])\n",
        model_mins[0], model_mins[1], model_mins[2],
        model_maxs[0], model_maxs[1], model_maxs[2]
    );
#endif // IQMDEBUG_CALC_MODEL_BOUNDS
}


// 
// Calculates the model hitbox overall mins/maxs for ...
//      ... model rest pose if anim_model == nullptr
//      ... across all animation frames specified by anim_model otherwise
// 
// Returns the hitbox mins / maxs as `model_mins`, `model_maxs1
// 
void calc_skel_model_bounds(skeletal_model_t *skel_model, skeletal_model_t *anim_model, vec3_t model_mins, vec3_t model_maxs) {
    if(anim_model == nullptr) {
        Con_Printf("Calculating model bounds for rest pose\n");
        calc_skel_model_bounds_for_rest_pose(skel_model, model_mins, model_maxs);
    }
    else {
        Con_Printf("Calculating model bounds for anim model\n");
        calc_skel_model_bounds_for_anim(skel_model, anim_model, model_mins, model_maxs);
    }
}



void cl_get_skel_model_bounds(int model_idx, int animmodel_idx, vec3_t mins, vec3_t maxs) {
    // Con_Printf("cl_get_skel_model_bounds -- Model: %d, Anim: %d\n", model_idx, animmodel_idx);
    skeletal_model_t *skel_model = cl_get_skel_model_for_modelidx(model_idx);
    if(skel_model == nullptr) {
        // Con_Printf("cl_get_skel_model_bounds -- model is null!\n");
        return;
    }
    if(model_idx == animmodel_idx) {
        // Con_Printf("cl_get_skel_model_bounds -- model is anim!\n");
        VectorCopy(skel_model->anim_bone_hitbox_mins, mins);
        VectorCopy(skel_model->anim_bone_hitbox_maxs, maxs);
        return;
    }
    skeletal_model_t *skel_anim = cl_get_skel_model_for_modelidx(animmodel_idx);
    if(skel_anim == nullptr) {
        // Con_Printf("cl_get_skel_model_bounds -- anim is null!\n");
        VectorCopy(skel_model->rest_pose_bone_hitbox_mins, mins);
        VectorCopy(skel_model->rest_pose_bone_hitbox_maxs, maxs);
        return;
    }
    _get_skel_model_bounds(skel_model, skel_anim, model_idx, animmodel_idx, mins, maxs);
}


void sv_get_skel_model_bounds(int model_idx, int animmodel_idx, vec3_t mins, vec3_t maxs) {
    skeletal_model_t *skel_model = sv_get_skel_model_for_modelidx(model_idx);
    if(skel_model == nullptr) {
        return;
    }
    if(model_idx == animmodel_idx) {
        VectorCopy(skel_model->anim_bone_hitbox_mins, mins);
        VectorCopy(skel_model->anim_bone_hitbox_maxs, maxs);
        return;
    }
    skeletal_model_t *skel_anim = sv_get_skel_model_for_modelidx(animmodel_idx);
    if(skel_anim == nullptr) {
        VectorCopy(skel_model->rest_pose_bone_hitbox_mins, mins);
        VectorCopy(skel_model->rest_pose_bone_hitbox_maxs, maxs);
        return;
    }
    _get_skel_model_bounds(skel_model, skel_anim, model_idx, animmodel_idx, mins, maxs);
}


//
// Given a skeletal model / skeletal animation and their model indices, calculate or return the precomputed bounds for all skeleton bones when playing that animation
// NOTE: Should only be called from `cl_get_skel_model_bounds` or `sv_get_skel_model_bounds`
//
void _get_skel_model_bounds(skeletal_model_t *skel_model, skeletal_model_t *skel_anim, int model_idx, int animmodel_idx, vec3_t mins, vec3_t maxs) {
    // Con_Printf("get_skel_model_bounds start\n");
    // Con_Printf("get_skel_model_bounds for skel model %d with anim %d\n", model_idx, animmodel_idx);
    if(skel_model == nullptr) {
        return;
    }
    if(skel_anim == nullptr) {
        return;
    }

    // If this animation doesn't fit in the current array, reallocate to make it big enough
    if(animmodel_idx >= skel_model_bounds_count[model_idx]) {
        int old_count = (skel_model_bounds_idxs[model_idx] == nullptr) ? 0 : skel_model_bounds_count[model_idx];
        int new_count = animmodel_idx + 1; // Allocate a new array that has index `animmodel_idx`
        int16_t *new_bounds_idxs = (int16_t*) malloc(sizeof(int16_t) * new_count);

        // Copy over all old values
        if(old_count > 0) {
            memcpy( new_bounds_idxs, skel_model_bounds_idxs[model_idx], sizeof(int16_t) * old_count);
            free(skel_model_bounds_idxs[model_idx]);
        }
        skel_model_bounds_idxs[model_idx] = new_bounds_idxs;
        skel_model_bounds_count[model_idx] = new_count;

        // Initialize all new entries in the array
        for(int i = old_count; i < new_count; i++) {
            skel_model_bounds_idxs[model_idx][i] = -1;
        }
    }


    if(skel_model_bounds_idxs[model_idx][animmodel_idx] == -1) {
        // Determine the next `bounds` slot to store the new entry at:
        int bounds_idx;
        if(bounds_count + 1 >= bounds_capacity) {
            // Start with 16 slots, double it every time after
            bounds_capacity = (bounds_capacity == 0) ? 16 : bounds_capacity * 2;
            Con_Printf("Allocating space for %d new bounds\n", bounds_capacity);
            vec3_t *new_bounds_mins = (vec3_t*) malloc(sizeof(vec3_t) * bounds_capacity); 
            vec3_t *new_bounds_maxs = (vec3_t*) malloc(sizeof(vec3_t) * bounds_capacity);
            // Con_Printf("Copying old bounds? (bounds_count: %d)\n", bounds_count);
            if(bounds_count > 0) {
                memcpy(new_bounds_mins, bounds_mins, sizeof(vec3_t) * bounds_count);
                memcpy(new_bounds_maxs, bounds_maxs, sizeof(vec3_t) * bounds_count);
                free(bounds_mins);
                free(bounds_maxs);
            }
            bounds_mins = new_bounds_mins;
            bounds_maxs = new_bounds_maxs;
        }
        // Insert new bounds at next index:
        bounds_idx = bounds_count;
        bounds_count += 1;

        Con_Printf("Inserting bounds for model-anim pair at index: %d (count: %d, capacity: %d)\n", bounds_idx, bounds_count, bounds_capacity);

        // Calculate the mins / maxs bounds for this ( model, animmodel ) pair
        skel_model_bounds_idxs[model_idx][animmodel_idx] = bounds_idx;
        calc_skel_model_bounds(skel_model, skel_anim, bounds_mins[bounds_idx], bounds_maxs[bounds_idx]);
    }

    VectorCopy(bounds_mins[skel_model_bounds_idxs[model_idx][animmodel_idx]], mins);
    VectorCopy(bounds_maxs[skel_model_bounds_idxs[model_idx][animmodel_idx]], maxs);

    // Con_Printf("Retrieved bounds for model %d, anim %d at idx %d (mins: [%.2f, %.2f, %.2f], maxs: [%.2f, %.2f, %.2f])\n",
    //     model_idx,
    //     animmodel_idx,
    //     skel_model_bounds_idxs[model_idx][animmodel_idx],
    //     mins[0], mins[1], mins[2],
    //     maxs[0], maxs[1], maxs[2]
    // );
}


// 
// Retrieve the skeleton mins / maxs for a server skeleton at index.
// The mins / maxs define an AABB in the skeleton's local-space 
// (i.e. rotated to match the skeleton's angles)
//
// Returns True if the skeleton is valid and mins / maxs were assigned
// Returns False otherwise
//
qboolean sv_get_skeleton_bounds(int skel_idx, vec3_t mins, vec3_t maxs) {
    if(skel_idx < 0 || skel_idx >= MAX_SKELETONS) {
        return qfalse;
    }

    if(sv_skeletons[skel_idx].in_use == false) {
        return qfalse;
    }
    int skel_modelindex = sv_skeletons[skel_idx].modelindex;
    int anim_modelindex = sv_skeletons[skel_idx].anim_modelindex;
    sv_get_skel_model_bounds( skel_modelindex, anim_modelindex, mins, maxs);
    return qtrue;
}


// ----------------------------------------------------------------------------

// Idea 5:
// Given MAX_MODELS=300
// int16_t *iqm_model_bounds_idx[MAX_MODELS]; // i'th index contains model i's list of indices
// int16_t *iqm_model_n_bounds[MAX_MODELS]; // i'th index contains model i's list of indices
// Each array is allocated up to the highest animmodel index it has seen
// If len(iqm_model_bounds_idx[i]) < animmodel index, we know it hasn't been ran against it
// Otherwise, model_bounds_idx[i][animmodel_index] is the index of the bounds...
//
// I like this, it has a very constrained memory consumption, and will likely never hit the full 900k size
// Only if all 300 models are IQM models, and only if animmodel with index 300 is played on all 300 models, a very degenerate case
// ... lookup is efficient though. An index, followed by a comparison, followed by two more indexes.


// Idea 6:
// Just keep a list, there won't be that many anyways. Look through the list of models.
// I think I like idea 5 better than this... it at least doesn't intermingle animations across models.



// // 300 * 300 * 2 = 180,000 = 180k bytes... this is... wasteful...
// uint16_t skel_model_anim_bounds_idx[MAX_MODELS][MAX_MODELS];
// // TODO - Make each value in this array point to a list of mins / maxs rather than making each field hold both vec3_ts...
// // TODO - Figure out a hash table setup for this.
// // TODO - Make a list of maps between anim models and skel models.
// vec3_t model_anim_bounds_mins;
// vec3_t model_anim_bounds_maxs;
// // MAX_MODELS

// ============================================================================


//
// Sets the matrix `camera_anim_view_matrix_out` to the transform representing 
// the current client-side pose for the skeletal model and skeleton used by 
// `view_ent`.
// 
//
//  to the ent `view_ent`'s current posed skeletal model.
//
// Returns 1 if `camera_anim_view_matrix_out` was set
// Returns 0 otherwise
//
int cl_get_camera_bone_view_transform(entity_t *view_ent, mat3x4_t camera_anim_view_matrix_out) {
    // Con_Printf("cl_get_camera_bone_view_transform 1\n");
    // If no model or not a skeletal model, don't update
    if(!view_ent->model || view_ent->model->type != mod_iqm) {
        // Con_Printf("cl_get_camera_bone_view_transform 1.1\n");
        return 0;
    } 
    // Con_Printf("cl_get_camera_bone_view_transform 2\n");

    skeletal_model_t *skel_model = static_cast<skeletal_model_t*>(Mod_Extradata(view_ent->model));
    // Con_Printf("cl_get_camera_bone_view_transform 3\n");

    // If not a skeletal model, don't update
    if(skel_model == nullptr) {
        // Con_Printf("cl_get_camera_bone_view_transform 3.1\n");
        return 0;
    }
    // Con_Printf("cl_get_camera_bone_view_transform 4\n");

    // If no camera bone, don't update
    if(skel_model->fps_cam_bone_idx < 0) {
        // Con_Printf("cl_get_camera_bone_view_transform 4.1\n");
        return 0;
    }
    // Con_Printf("cl_get_camera_bone_view_transform 5\n");

    skeletal_skeleton_t *cl_skel = cl_update_skel_for_ent(view_ent); // FIXME - Crash here
    // Con_Printf("cl_get_camera_bone_view_transform 6\n");
    

    // If no client-side skeleton, don't update
    if(cl_skel == nullptr) {
        // Con_Printf("cl_get_camera_bone_view_transform 6.1\n");
        return 0;
    }
    // Con_Printf("cl_get_camera_bone_view_transform 7\n");

    // mat3x4_t camera_anim_pose_transform;
    // mat3x4_t camera_anim_rest_to_pose_transform;
    // // mat3x4_t camera_anim_transform;
    // Matrix3x4_rotate_translate(camera_anim_pose_transform, cl_skel->fps_cam_bone_rot, cl_skel->fps_cam_bone_pos);
    // mat3x4_t *inv_bone_rest_transforms = get_member_from_offset(skel_model->inv_bone_rest_transforms, skel_model);
    // Matrix3x4_ConcatTransforms( camera_anim_rest_to_pose_transform, camera_anim_pose_transform, inv_bone_rest_transforms[skel_model->fps_cam_bone_idx]);
    // // Invert the camera matrix to get the view matrix
    // Matrix3x4_Invert_Affine(camera_anim_view_matrix_out, camera_anim_rest_to_pose_transform);


    // Con_Printf("---------------------------------------------------------\n");
    // mat3x4_t *inv_bone_rest_transforms = get_member_from_offset(skel_model->inv_bone_rest_transforms, skel_model);

    // mat3x4_t *print_mat;
    // Con_Printf("\t\tBone Transform:\n");
    // print_mat = &(cl_skel->bone_transforms[skel_model->fps_cam_bone_idx]);
    // Con_Printf("\t\t\t[%.2f, %.2f, %.2f, %.2f]\n", *print_mat[0][0], *print_mat[0][1], *print_mat[0][2], *print_mat[0][3]);
    // Con_Printf("\t\t\t[%.2f, %.2f, %.2f, %.2f]\n", *print_mat[1][0], *print_mat[1][1], *print_mat[1][2], *print_mat[1][3]);
    // Con_Printf("\t\t\t[%.2f, %.2f, %.2f, %.2f]\n", *print_mat[2][0], *print_mat[2][1], *print_mat[2][2], *print_mat[2][3]);
    // Con_Printf("\t\t\t[%.2f, %.2f, %.2f, %.2f]\n", 0.0f, 0.0f, 0.0f, 1.0f);

    // Con_Printf("\t\tBone inverse-rest-transform (rest-pose to parent local-space):\n");
    // print_mat = &(inv_bone_rest_transforms[skel_model->fps_cam_bone_idx]);
    // Con_Printf("\t\t\t[%.2f, %.2f, %.2f, %.2f]\n", *print_mat[0][0], *print_mat[0][1], *print_mat[0][2], *print_mat[0][3]);
    // Con_Printf("\t\t\t[%.2f, %.2f, %.2f, %.2f]\n", *print_mat[1][0], *print_mat[1][1], *print_mat[1][2], *print_mat[1][3]);
    // Con_Printf("\t\t\t[%.2f, %.2f, %.2f, %.2f]\n", *print_mat[2][0], *print_mat[2][1], *print_mat[2][2], *print_mat[2][3]);
    // Con_Printf("\t\t\t[%.2f, %.2f, %.2f, %.2f]\n", 0.0f, 0.0f, 0.0f, 1.0f);
    
    // Con_Printf("\t\tBone rest-to-pose (rest-pose mdoel-space to posed model-space):\n");
    // print_mat = &(cl_skel->bone_rest_to_pose_transforms[skel_model->fps_cam_bone_idx]);
    // Con_Printf("\t\t\t[%.2f, %.2f, %.2f, %.2f]\n", *print_mat[0][0], *print_mat[0][1], *print_mat[0][2], *print_mat[0][3]);
    // Con_Printf("\t\t\t[%.2f, %.2f, %.2f, %.2f]\n", *print_mat[1][0], *print_mat[1][1], *print_mat[1][2], *print_mat[1][3]);
    // Con_Printf("\t\t\t[%.2f, %.2f, %.2f, %.2f]\n", *print_mat[2][0], *print_mat[2][1], *print_mat[2][2], *print_mat[2][3]);
    // Con_Printf("\t\t\t[%.2f, %.2f, %.2f, %.2f]\n", 0.0f, 0.0f, 0.0f, 1.0f);

    // // TODO - Print the rest-to-local bone pose
    // // TODO - Print the 
    // Con_Printf("---------------------------------------------------------\n");
    
    // cl_skel->bone_rest_to_pose_transforms[fps_cam_bone_idx];
    // mat3x4_t *camera_anim_transform = &(cl_skel->bone_transforms[skel_model->fps_cam_bone_idx]);

    // Con_Printf("cl_get_camera_bone_view_transform 8\n");

    mat3x4_t *camera_anim_transform = &(cl_skel->bone_rest_to_pose_transforms[skel_model->fps_cam_bone_idx]);
    // mat3x4_t *camera_transform = &(cl_skel->bone_rest_to_pose_normal_transforms[fps_cam_bone_idx]);
    // Con_Printf("cl_get_camera_bone_view_transform 9\n");
    Matrix3x4_Invert_Affine(camera_anim_view_matrix_out, *camera_anim_transform);
    // Con_Printf("cl_get_camera_bone_view_transform 10\n");
    // Matrix3x4_Copy(camera_anim_view_matrix_out, camera_anim_transform);
    return 1;
}
		
