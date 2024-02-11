extern"C" {
    #include "../quakedef.h"
}

#include <pspgu.h>
#include <pspgum.h>

#include "video_hardware_iqm.h"


#define VERT_BONES 4
#define TRI_VERTS 3
#define SUBMESH_BONES 8



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
    Con_Printf("=========== Submesh started =============\n");

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

        cur_submesh_idx += 1;
        int cur_submesh_n_bones = 0;
        uint8_t *cur_submesh_bones = (uint8_t*) malloc(sizeof(uint8_t) * SUBMESH_BONES);
        Con_Printf("Creating submesh %d for mesh\n", cur_submesh_idx);

        // Verify the triangle doesn't have more than the max bones allowed:
        if(tri_n_bones[cur_tri] > SUBMESH_BONES) {
            Con_Printf("Warning: Mesh Triangle %d references %d bones, which is more than the maximum allowed for any mesh (%d). Skipping triangle...\n", cur_tri, tri_n_bones[cur_tri], SUBMESH_BONES);
            // Discard it
            tri_submesh_idx[cur_tri] = SET_DISCARD;
            continue;
        }

        // Add the triangle to the current submesh:
        tri_submesh_idx[cur_tri] = cur_submesh_idx;

        // Add the triangle's bones to the current submesh:
        for(int submesh_bone_idx = 0; submesh_bone_idx < tri_n_bones[cur_tri]; submesh_bone_idx++) {
            cur_submesh_bones[submesh_bone_idx] = tri_bones[(cur_tri * TRI_VERTS * VERT_BONES) + submesh_bone_idx];
            cur_submesh_n_bones += 1;
        }

        Con_Printf("\tstart submesh bones (%d): [", cur_submesh_n_bones);
        for(int submesh_bone_idx = 0; submesh_bone_idx < cur_submesh_n_bones; submesh_bone_idx++) {
            Con_Printf("%d, ", cur_submesh_bones[submesh_bone_idx]);
        }
        Con_Printf("]\n");

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
        Con_Printf("\tcur submesh (%d) n_tris: %d/%d, remaining unassigned: %d/%d\n", cur_submesh_idx, cur_submesh_n_tris, mesh->n_tris, n_assigned_tris, mesh->n_tris);


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
            }

            // If no triangle found, stop. We're done.
            if(cur_tri == -1) {
                Con_Printf("\tNo more unassigned triangles. Done with mesh.\n");
                break;
            }

            // If this triangle pushes us past the submesh-bone limit, we are done with this submesh. Move onto the next one.
            if(cur_submesh_n_bones + cur_tri_n_missing_bones > SUBMESH_BONES) {
                Con_Printf("\tReached max number of bones allowed. Done with submesh.\n");
                break;
            }

            Con_Printf("\tNext loop using triangle: %d, missing bones: %d\n", cur_tri, cur_tri_n_missing_bones);


            // Assign the triangle to the current submesh
            tri_submesh_idx[cur_tri] = cur_submesh_idx;

            // Add this triangle's bones to the current submesh list of bones
            cur_submesh_n_bones = bone_list_set_union( cur_submesh_bones, cur_submesh_n_bones, &tri_bones[cur_tri * TRI_VERTS * VERT_BONES], tri_n_bones[cur_tri]);


            Con_Printf("\tcur submesh bones (%d): [", cur_submesh_n_bones);
            for(int submesh_bone_idx = 0; submesh_bone_idx < cur_submesh_n_bones; submesh_bone_idx++) {
                Con_Printf("%d, ", cur_submesh_bones[submesh_bone_idx]);
            }
            Con_Printf("]\n");


            // Add all unassigned triangles from the main mesh that reference bones in this submesh's bone list
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

            // Print how many triangles belong to the current submesh
            cur_submesh_n_tris = 0;
            n_assigned_tris = 0;
            for(uint32_t tri_idx = 0; tri_idx < mesh->n_tris; tri_idx++) {
                if(tri_submesh_idx[tri_idx] != SET_UNASSIGNED) {
                    n_assigned_tris++;
                }
                if(tri_submesh_idx[tri_idx] == cur_submesh_idx) {
                    cur_submesh_n_tris++;
                }
            }
            Con_Printf("\tDone adding new tris for cur triangle");
            Con_Printf("\tcur submesh (%d) n_tris: %d/%d, total assigned: %d/%d\n", cur_submesh_idx, cur_submesh_n_tris, mesh->n_tris, n_assigned_tris, mesh->n_tris);
        }

        free(cur_submesh_bones);
    }

    int n_submeshes = cur_submesh_idx + 1;
    mesh->n_submeshes = n_submeshes;
    mesh->submeshes = (skeletal_mesh_t *) malloc(sizeof(skeletal_mesh_t) * n_submeshes);


    for(int submesh_idx = 0; submesh_idx < n_submeshes; submesh_idx++) {
        Con_Printf("Reconstructing submesh %d/%d for mesh\n", submesh_idx+1, n_submeshes);
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
        mesh->submeshes[submesh_idx].vert_rest_normals = (vec3_t*) malloc(sizeof(vec3_t) * submesh_n_verts);
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



        for(uint32_t vert_idx = 0; vert_idx < submesh_n_verts; vert_idx++) {
            uint16_t mesh_vert_idx = submesh_mesh_vert_idxs[vert_idx]; 
            mesh->submeshes[submesh_idx].vert_rest_positions[vert_idx][0] = mesh->vert_rest_positions[mesh_vert_idx][0];
            mesh->submeshes[submesh_idx].vert_rest_positions[vert_idx][1] = mesh->vert_rest_positions[mesh_vert_idx][1];
            mesh->submeshes[submesh_idx].vert_rest_positions[vert_idx][2] = mesh->vert_rest_positions[mesh_vert_idx][2];
            mesh->submeshes[submesh_idx].vert_uvs[vert_idx][0] = mesh->vert_uvs[mesh_vert_idx][0];
            mesh->submeshes[submesh_idx].vert_uvs[vert_idx][1] = mesh->vert_uvs[mesh_vert_idx][1];
            mesh->submeshes[submesh_idx].vert_rest_normals[vert_idx][0] = mesh->vert_rest_normals[mesh_vert_idx][0];
            mesh->submeshes[submesh_idx].vert_rest_normals[vert_idx][1] = mesh->vert_rest_normals[mesh_vert_idx][1];
            mesh->submeshes[submesh_idx].vert_rest_normals[vert_idx][2] = mesh->vert_rest_normals[mesh_vert_idx][2];

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
        free(mesh->submeshes[submesh_idx].vert_bone_idxs);
        free(mesh->submeshes[submesh_idx].vert_bone_weights);
        mesh->submeshes[submesh_idx].vert_bone_idxs = nullptr;
        mesh->submeshes[submesh_idx].vert_bone_weights = nullptr;

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
        Con_Printf("Mesh submesh %d bones (%d): [", submesh_idx, n_submesh_bones);
        for(int j = 0; j < SUBMESH_BONES; j++) {
            Con_Printf("%d, ", submesh_bones[j]);
        }
        Con_Printf("]\n");
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

        Con_Printf("About to free submesh data structures...\n");
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



const matrix3x3 matrix3x3_zero =
{
{ 0, 0, 0 },
{ 0, 0, 0 },
{ 0, 0, 0 },
};
const matrix3x3 matrix3x3_identity =
{
{ 1, 0, 0 },
{ 0, 1, 0 },
{ 0, 0, 1 },
};
#define Matrix3x3_Copy( out, in )		memcpy( out, in, sizeof( matrix3x3 ))
#define Matrix3x3_LoadZero( mat )		Matrix3x3_Copy( mat, matrix3x3_zero )
#define Matrix3x3_LoadIdentity( mat )		Matrix3x3_Copy( mat, matrix3x3_identity )



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
    Con_Printf("Calculating quantization ofs / scale for mesh...\n");

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
    Con_Printf("Done calculating quantization ofs / scale for mesh\n");
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
            // Con_Printf("\twriting normal...\n");
            mesh->vert8s[unindexed_vert_idx].nor_x = float_to_int8(mesh->vert_rest_normals[indexed_vert_idx][0]);
            mesh->vert8s[unindexed_vert_idx].nor_y = float_to_int8(mesh->vert_rest_normals[indexed_vert_idx][1]);
            mesh->vert8s[unindexed_vert_idx].nor_z = float_to_int8(mesh->vert_rest_normals[indexed_vert_idx][2]);
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
            // Con_Printf("\twriting normal...\n");
            mesh->vert16s[unindexed_vert_idx].nor_x = float_to_int16(mesh->vert_rest_normals[indexed_vert_idx][0]);
            mesh->vert16s[unindexed_vert_idx].nor_y = float_to_int16(mesh->vert_rest_normals[indexed_vert_idx][1]);
            mesh->vert16s[unindexed_vert_idx].nor_z = float_to_int16(mesh->vert_rest_normals[indexed_vert_idx][2]);
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


    Con_Printf("\tParsing vert attribute arrays\n");

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
                Con_Printf("Warning: IQM vertex normals array (idx: %d, type: %d, fmt: %d, size: %d) is not 4D float array.\n", i, vert_arrays[i].type, vert_arrays[i].format, vert_arrays[i].size);
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

    Con_Printf("\tCreating skeletal model object\n");
    skeletal_model_t *skel_model = (skeletal_model_t*) malloc(sizeof(skeletal_model_t));
    skel_model->n_meshes = iqm_header->n_meshes;
    skel_model->meshes = (skeletal_mesh_t*) malloc(sizeof(skeletal_mesh_t) * skel_model->n_meshes);

    // FIXME - Should we even store these? Should we automatically convert them down?
    vec2_t *verts_uv = (vec2_t*) malloc(sizeof(vec2_t) * iqm_header->n_verts);
    vec3_t *verts_pos = (vec3_t*) malloc(sizeof(vec3_t) * iqm_header->n_verts);
    vec3_t *verts_nor = (vec3_t*) malloc(sizeof(vec3_t) * iqm_header->n_verts);


    // ------------------------------------------------------------------------
    // Convert verts_pos / verts_uv datatypes to floats 
    // ------------------------------------------------------------------------
    vec3_t default_vert = {0,0,0};
    vec2_t default_uv = {0,0};
    vec3_t default_nor = {0,0,1.0f};

    Con_Printf("\tParsing vertex attribute arrays\n");
    Con_Printf("Verts pos: %d\n", iqm_verts_pos);
    Con_Printf("Verts uv: %d\n", iqm_verts_uv);
    Con_Printf("Verts nor: %d\n", iqm_verts_nor);
    Con_Printf("Verts bone weights: %d\n", iqm_verts_bone_weights);
    Con_Printf("Verts bone idxs: %d\n", iqm_verts_bone_idxs);
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
    const iqm_mesh_t *iqm_meshes = (const iqm_mesh_t*)((uint8_t*) iqm_data + iqm_header->ofs_meshes);

    for(uint32_t i = 0; i < iqm_header->n_meshes; i++) {
        skeletal_mesh_t *mesh = &skel_model->meshes[i];
        const char *material_name = (const char*) (((uint8_t*) iqm_data + iqm_header->ofs_text) + iqm_meshes[i].material);
        // Con_Printf("Mesh[%d]: \"%s\"\n", i, material_name);

        uint32_t first_vert = iqm_meshes[i].first_vert_idx;
        uint32_t first_tri = iqm_meshes[i].first_tri;
        // skel_model->meshes[i].first_vert = first_vert;
        uint32_t n_verts = iqm_meshes[i].n_verts;
        mesh->n_verts = n_verts;
        mesh->vert_rest_positions = (vec3_t*) malloc(sizeof(vec3_t) * n_verts);
        mesh->vert_rest_normals = (vec3_t*) malloc(sizeof(vec3_t) * n_verts);
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
            mesh->vert_rest_normals[j][0] = verts_nor[first_vert + j][0];
            mesh->vert_rest_normals[j][1] = verts_nor[first_vert + j][1];
            mesh->vert_rest_normals[j][2] = verts_nor[first_vert + j][2];
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
        submesh_skeletal_mesh(mesh);
        Con_Printf("Done splitting mesh into submeshes\n");
        
        // --


        for(int submesh_idx = 0; submesh_idx < mesh->n_submeshes; submesh_idx++) {
            Con_Printf("Writing vert structs for mesh %d/%d submesh %d/%d\n", i+1, iqm_header->n_meshes, submesh_idx+1, mesh->n_submeshes);
            //  Cast float32 temp vertex data to vert8s / vert16s
            //  Remove triangle indices
            calc_quantization_grid_scale_ofs(&mesh->submeshes[submesh_idx]);
            // TODO - Avoid building both vert8 and vert16 structs?
            write_skinning_vert8_structs(&mesh->submeshes[submesh_idx]);
            write_skinning_vert16_structs(&mesh->submeshes[submesh_idx]);
            Con_Printf("Done writing vert structs for mesh %d/%d submesh %d/%d\n", i+1, iqm_header->n_meshes, submesh_idx+1, mesh->n_submeshes);
        }


        // Deallocate mesh's loading temporary structs (not used by drawing code)
        for(int submesh_idx = 0; submesh_idx < mesh->n_submeshes; submesh_idx++) {
            Con_Printf("Clearing temporary memory for mesh %d/%d submesh %d/%d\n", i+1, iqm_header->n_meshes, submesh_idx+1, mesh->n_submeshes);
            skeletal_mesh_t *submesh = &mesh->submeshes[submesh_idx];
            free_pointer_and_clear( submesh->vert_rest_positions);
            free_pointer_and_clear( submesh->vert_uvs);
            free_pointer_and_clear( submesh->vert_rest_normals);
            free_pointer_and_clear( submesh->vert_bone_weights); // NOTE - Unused by submeshes, but included for completeness
            free_pointer_and_clear( submesh->vert_bone_idxs);    // NOTE - Unused by submeshes, but included for completeness
            free_pointer_and_clear( submesh->vert_skinning_weights);
            free_pointer_and_clear( submesh->tri_verts);
            Con_Printf("Done clearing temporary memory for mesh %d/%d submesh %d/%d\n", i+1, iqm_header->n_meshes, submesh_idx+1, mesh->n_submeshes);
        }
        
        Con_Printf("Clearing temporary memory for mesh %d/%d\n", i+1, iqm_header->n_meshes);
        Con_Printf("\tClearing vert_rest_positions...\n", i+1, iqm_header->n_meshes);
        free_pointer_and_clear( mesh->vert_rest_positions);
        Con_Printf("\tClearing vert_rest_normals...\n", i+1, iqm_header->n_meshes);
        free_pointer_and_clear( mesh->vert_rest_normals);
        Con_Printf("\tClearing vert_uvs...\n", i+1, iqm_header->n_meshes);
        free_pointer_and_clear( mesh->vert_uvs);
        Con_Printf("\tClearing vert_bone_weights...\n", i+1, iqm_header->n_meshes);
        free_pointer_and_clear( mesh->vert_bone_weights);
        Con_Printf("\tClearing vert_bone_idxs...\n", i+1, iqm_header->n_meshes);
        free_pointer_and_clear( mesh->vert_bone_idxs);
        Con_Printf("\tClearing vert_skinning_weights...\n", i+1, iqm_header->n_meshes);
        free_pointer_and_clear( mesh->vert_skinning_weights);   // NOTE - Unused by meshes, but included for completeness
        Con_Printf("\tClearing tri_verts...\n", i+1, iqm_header->n_meshes);
        free_pointer_and_clear( mesh->tri_verts);
        Con_Printf("Done clearing temporary memory for mesh %d/%d\n", i+1, iqm_header->n_meshes);
    }

    // Deallocate iqm parsing buffers
    free(verts_pos);
    free(verts_uv);
    free(verts_nor);
    free(verts_bone_weights);
    free(verts_bone_idxs);


    // --------------------------------------------------
    // Parse bones and their rest transforms
    // --------------------------------------------------
    Con_Printf("Parsing joints...\n");
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
        Con_Printf("Parsed bone: %i, \"%s\" (parent bone: %d)\n", i, skel_model->bone_name[i], skel_model->bone_parent_idx[i]);
        Con_Printf("\tPos: (%.2f, %.2f, %.2f)\n", skel_model->bone_rest_pos[i][0], skel_model->bone_rest_pos[i][1], skel_model->bone_rest_pos[i][2]);
        Con_Printf("\tRot: (%.2f, %.2f, %.2f, %.2f)\n", skel_model->bone_rest_rot[i][0], skel_model->bone_rest_rot[i][1], skel_model->bone_rest_rot[i][2], skel_model->bone_rest_rot[i][3]);
        Con_Printf("\tScale: (%.2f, %.2f, %.2f)\n", skel_model->bone_rest_scale[i][0], skel_model->bone_rest_scale[i][1], skel_model->bone_rest_scale[i][2]);
    }
    Con_Printf("\tParsed %d bones.\n", skel_model->n_bones);

    for(uint32_t i = 0; i < skel_model->n_bones; i++) {
        // i-th bone's parent index must be less than i
        if((int) i <= skel_model->bone_parent_idx[i]) {
            Con_Printf("Error: IQM file bones are sorted incorrectly. Bone %d is located before its parent bone %d.\n", i, skel_model->bone_parent_idx[i]);
            // TODO - Deallocate all allocated memory
            return nullptr;
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
    for(uint32_t i = 0; i < skel_model->n_bones; i++) {
        Matrix3x4_Invert_Simple( skel_model->inv_bone_rest_transforms[i], skel_model->bone_rest_transforms[i]);
    }
    // --------------------------------------------------


    // --------------------------------------------------
    // Parse all frames (poses)
    // --------------------------------------------------
    skel_model->n_frames = iqm_header->n_frames;
    Con_Printf("\tBones: %d\n",skel_model->n_bones);
    Con_Printf("\tFrames: %d\n",iqm_header->n_frames);
    Con_Printf("\tPoses: %d\n",iqm_header->n_poses);
    skel_model->frames_bone_pos = (vec3_t *) malloc(sizeof(vec3_t) * skel_model->n_bones * skel_model->n_frames);
    skel_model->frames_bone_rot = (quat_t *) malloc(sizeof(quat_t) * skel_model->n_bones * skel_model->n_frames);
    skel_model->frames_bone_scale = (vec3_t *) malloc(sizeof(vec3_t) * skel_model->n_bones * skel_model->n_frames);

    const uint16_t *frames_data = (const uint16_t*)((uint8_t*) iqm_data + iqm_header->ofs_frames);
    int frames_data_ofs = 0;
    const iqm_pose_quaternion_t *iqm_poses = (const iqm_pose_quaternion_t*) ((uint8_t*) iqm_data + iqm_header->ofs_poses);

    // Iterate over actual frames in IQM file:
    for(uint32_t i = 0; i < iqm_header->n_frames; i++) {
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
    Con_Printf("\tParsed %d framegroups.\n", skel_model->n_framegroups);
    // --------------------------------------------------






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
        skel_model_n_bytes += sizeof(char) * (strlen(skel_model->bone_name[i]) + 1);
    }
    // -- cached bone rest transforms --
    skel_model_n_bytes += sizeof(mat3x4_t) * skel_model->n_bones;
    skel_model_n_bytes += sizeof(mat3x4_t) * skel_model->n_bones;
    // -- animation frames -- 
    skel_model_n_bytes += sizeof(vec3_t) * skel_model->n_bones * skel_model->n_frames;
    skel_model_n_bytes += sizeof(quat_t) * skel_model->n_bones * skel_model->n_frames;
    skel_model_n_bytes += sizeof(vec3_t) * skel_model->n_bones * skel_model->n_frames;
    // -- animation framegroups --
    skel_model_n_bytes += sizeof(char*) * skel_model->n_framegroups;
    skel_model_n_bytes += sizeof(uint32_t) * skel_model->n_framegroups;
    skel_model_n_bytes += sizeof(uint32_t) * skel_model->n_framegroups;
    skel_model_n_bytes += sizeof(float) * skel_model->n_framegroups;
    skel_model_n_bytes += sizeof(bool) * skel_model->n_framegroups;
    for(int i = 0; i < skel_model->n_framegroups; i++) {
        skel_model_n_bytes += sizeof(char) * (strlen(skel_model->framegroup_name[i]) + 1);
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
    uint8_t *ptr = (uint8_t*) relocatable_skel_model;
    flatten_member(relocatable_skel_model, skel_model, sizeof(skeletal_model_t), ptr);
    flatten_member(relocatable_skel_model->meshes, skel_model->meshes, sizeof(skeletal_mesh_t) * skel_model->n_meshes, ptr);

    for(int i = 0; i < skel_model->n_meshes; i++) {
        flatten_member(relocatable_skel_model->meshes[i].submeshes, skel_model->meshes[i].submeshes, sizeof(skeletal_mesh_t) * skel_model->meshes[i].n_submeshes, ptr);
        for(int j = 0; j < skel_model->meshes[i].n_submeshes; j++) {
            if(skel_model->meshes[i].submeshes[j].vert8s != nullptr) {
                flatten_member(relocatable_skel_model->meshes[i].submeshes[j].vert8s, skel_model->meshes[i].submeshes[j].vert8s, sizeof(skel_vertex_i8_t) * skel_model->meshes[i].submeshes[j].n_verts, ptr);
            }
            if(skel_model->meshes[i].submeshes[j].vert16s != nullptr) {
                flatten_member(relocatable_skel_model->meshes[i].submeshes[j].vert16s, skel_model->meshes[i].submeshes[j].vert16s, sizeof(skel_vertex_i16_t) * skel_model->meshes[i].submeshes[j].n_verts, ptr);
            }
        }
    }
    // -- bones --
    flatten_member(relocatable_skel_model->bone_name, skel_model->bone_name, sizeof(char*) * skel_model->n_bones, ptr);
    flatten_member(relocatable_skel_model->bone_parent_idx, skel_model->bone_parent_idx, sizeof(int16_t) * skel_model->n_bones, ptr);
    flatten_member(relocatable_skel_model->bone_rest_pos, skel_model->bone_rest_pos, sizeof(vec3_t) * skel_model->n_bones, ptr);
    flatten_member(relocatable_skel_model->bone_rest_rot, skel_model->bone_rest_rot, sizeof(vec4_t) * skel_model->n_bones, ptr);
    flatten_member(relocatable_skel_model->bone_rest_scale, skel_model->bone_rest_scale, sizeof(vec3_t) * skel_model->n_bones, ptr);
    for(int i = 0; i < skel_model->n_bones; i++) {
        flatten_member(relocatable_skel_model->bone_name[i], skel_model->bone_name[i], sizeof(char) * (strlen(skel_model->bone_name[i]) + 1), ptr);
    }
    // -- cached bone rest transforms --
    flatten_member(relocatable_skel_model->bone_rest_transforms, skel_model->bone_rest_transforms, sizeof(mat3x4_t) * skel_model->n_bones, ptr);
    flatten_member(relocatable_skel_model->inv_bone_rest_transforms, skel_model->inv_bone_rest_transforms, sizeof(mat3x4_t) * skel_model->n_bones, ptr);
    // -- frames --
    flatten_member(relocatable_skel_model->frames_bone_pos, skel_model->frames_bone_pos, sizeof(vec3_t) * skel_model->n_bones * skel_model->n_frames, ptr);
    flatten_member(relocatable_skel_model->frames_bone_rot, skel_model->frames_bone_rot, sizeof(quat_t) * skel_model->n_bones * skel_model->n_frames, ptr);
    flatten_member(relocatable_skel_model->frames_bone_scale, skel_model->frames_bone_scale, sizeof(vec3_t) * skel_model->n_bones * skel_model->n_frames, ptr);
    // -- framegroups --
    flatten_member(relocatable_skel_model->framegroup_name, skel_model->framegroup_name, sizeof(char*) * skel_model->n_framegroups, ptr);
    flatten_member(relocatable_skel_model->framegroup_start_frame, skel_model->framegroup_start_frame, sizeof(uint32_t) * skel_model->n_framegroups, ptr);
    flatten_member(relocatable_skel_model->framegroup_n_frames, skel_model->framegroup_n_frames, sizeof(uint32_t) * skel_model->n_framegroups, ptr);
    flatten_member(relocatable_skel_model->framegroup_fps, skel_model->framegroup_fps, sizeof(float) * skel_model->n_framegroups, ptr);
    flatten_member(relocatable_skel_model->framegroup_loop, skel_model->framegroup_loop, sizeof(bool) * skel_model->n_framegroups, ptr);
    for(int i = 0; i < skel_model->n_framegroups; i++) {
        flatten_member(relocatable_skel_model->framegroup_name[i], skel_model->framegroup_name[i], sizeof(char) * (strlen(skel_model->framegroup_name[i]) + 1), ptr);
    }
    // ------------–------------–------------–------------–------------–-------

    // ------------–------------–------------–------------–------------–-------
    // Clean up all pointers to be relative to model start location in memory
    // NOTE - Must deconstruct in reverse order to avoid undoing offsets
    // ------------–------------–------------–------------–------------–-------
    // -- framegroups --
    for(int i = 0; i < skel_model->n_framegroups; i++) {
        set_member_to_offset(relocatable_skel_model->framegroup_name[i], relocatable_skel_model);
    }
    set_member_to_offset(relocatable_skel_model->framegroup_loop, relocatable_skel_model);
    set_member_to_offset(relocatable_skel_model->framegroup_fps, relocatable_skel_model);
    set_member_to_offset(relocatable_skel_model->framegroup_n_frames, relocatable_skel_model);
    set_member_to_offset(relocatable_skel_model->framegroup_start_frame, relocatable_skel_model);
    set_member_to_offset(relocatable_skel_model->framegroup_name, relocatable_skel_model);
    // -- frames --
    set_member_to_offset(relocatable_skel_model->frames_bone_scale, relocatable_skel_model);
    set_member_to_offset(relocatable_skel_model->frames_bone_rot, relocatable_skel_model);
    set_member_to_offset(relocatable_skel_model->frames_bone_pos, relocatable_skel_model);
    // -- cached bone rest transforms --
    set_member_to_offset(relocatable_skel_model->inv_bone_rest_transforms, relocatable_skel_model);
    set_member_to_offset(relocatable_skel_model->bone_rest_transforms, relocatable_skel_model);
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
    }
    set_member_to_offset(relocatable_skel_model->meshes, relocatable_skel_model);
    // ------------–------------–------------–------------–------------–-------
}


//
// Completely deallocates a skeletal_model_t struct, pointers and all.
// NOTE - This should not be used on a relocatable skeletal_model_t object.
//
void free_skeletal_model(skeletal_model_t *skel_model) {
    // Free fields in reverse order to avoid losing references

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
    // -- cached bone rest transforms --
    free_pointer_and_clear(skel_model->inv_bone_rest_transforms);
    free_pointer_and_clear(skel_model->bone_rest_transforms);
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
            free_pointer_and_clear( skel_model->meshes[i].submeshes[j].vert_rest_normals);
            free_pointer_and_clear( skel_model->meshes[i].submeshes[j].vert_bone_weights);
            free_pointer_and_clear( skel_model->meshes[i].submeshes[j].vert_bone_idxs);
            free_pointer_and_clear( skel_model->meshes[i].submeshes[j].vert_skinning_weights);
            free_pointer_and_clear( skel_model->meshes[i].submeshes[j].tri_verts);
            free_pointer_and_clear( skel_model->meshes[i].submeshes[j].submeshes);

            // These submesh struct members are expected to be allocated:
            free_pointer_and_clear( skel_model->meshes[i].submeshes[j].vert8s);     // Only free if not nullptr
            free_pointer_and_clear( skel_model->meshes[i].submeshes[j].vert16s);    // Only free if not nullptr
        }
        
        // These mesh struct members are expected to be nullptr:
        free_pointer_and_clear( skel_model->meshes[i].vert_rest_positions);
        free_pointer_and_clear( skel_model->meshes[i].vert_uvs);
        free_pointer_and_clear( skel_model->meshes[i].vert_rest_normals);
        free_pointer_and_clear( skel_model->meshes[i].vert_bone_weights);
        free_pointer_and_clear( skel_model->meshes[i].vert_bone_idxs);
        free_pointer_and_clear( skel_model->meshes[i].vert_skinning_weights);
        free_pointer_and_clear( skel_model->meshes[i].tri_verts);
        free_pointer_and_clear( skel_model->meshes[i].vert16s);
        free_pointer_and_clear( skel_model->meshes[i].vert8s);

        // These mesh struct members are expected to be allocated:
        free_pointer_and_clear( skel_model->meshes[i].submeshes);
    }
    free(skel_model->meshes);
    free(skel_model);
}


/*
=================
Mod_LoadIQMModel
=================
*/
extern char loadname[];
void Mod_LoadIQMModel (model_t *model, void *buffer) {

    skeletal_model_t *skel_model = load_iqm_file(buffer);

    // ------------------------------------------------------------------------
    // Debug - printing the skel_model contents
    // ------------------------------------------------------------------------
    Con_Printf("------------------------------------------------------------\n");
    Con_Printf("Debug printing skeletal model\n");
    Con_Printf("------------------------------------------------------------\n");
    Con_Printf("skel_model->n_meshes = %d\n", skel_model->n_meshes);
    for(int i = 0; i < skel_model->n_meshes; i++) {
        Con_Printf("---------------------\n");
        Con_Printf("skel_model->meshes[%d].n_verts = %d\n", i, skel_model->meshes[i].n_verts);
        Con_Printf("skel_model->meshes[%d].n_tris = %d\n", i, skel_model->meshes[i].n_tris);
        Con_Printf("skel_model->meshes[%d].vert_rest_positions = %d\n", i, skel_model->meshes[i].vert_rest_positions);
        Con_Printf("skel_model->meshes[%d].vert_uvs = %d\n", i, skel_model->meshes[i].vert_uvs);
        Con_Printf("skel_model->meshes[%d].vert_rest_normals = %d\n", i, skel_model->meshes[i].vert_rest_normals);
        Con_Printf("skel_model->meshes[%d].vert_bone_weights = %d\n", i, skel_model->meshes[i].vert_bone_weights);
        Con_Printf("skel_model->meshes[%d].vert_bone_idxs = %d\n", i, skel_model->meshes[i].vert_bone_idxs);
        Con_Printf("skel_model->meshes[%d].vert_skinning_weights = %d\n", i, skel_model->meshes[i].vert_skinning_weights);
        Con_Printf("skel_model->meshes[%d].tri_verts = %d\n", i, skel_model->meshes[i].tri_verts);
        Con_Printf("skel_model->meshes[%d].vert16s = %d\n", i, skel_model->meshes[i].vert16s);
        Con_Printf("skel_model->meshes[%d].vert8s = %d\n", i, skel_model->meshes[i].vert8s);
        Con_Printf("skel_model->meshes[%d].n_skinning_bones = %d\n", i, skel_model->meshes[i].n_skinning_bones);
        // --
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
            Con_Printf("\tskel_model->meshes[%d].submeshes[%d].vert_rest_normals = %d\n", i, j, skel_model->meshes[i].submeshes[j].vert_rest_normals);
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
    Con_Printf("------------------------------------------------------------\n");
    // ------------------------------------------------------------------------


    // TODO - Need to update this function to work with latest version of skeletal_model_t
    Con_Printf("About to calculate skeletal model size...\n");
    uint32_t skel_model_n_bytes = count_skel_model_n_bytes(skel_model);
    Con_Printf("Done calculating skeletal model size\n");
    // skeletal_model_t *relocatable_skel_model = (skeletal_model_t*) malloc(skel_model_n_bytes); // TODO - Pad to 2-byte alignment?
    skeletal_model_t *relocatable_skel_model = (skeletal_model_t*) Hunk_AllocName(skel_model_n_bytes, loadname); // TODO - Pad to 2-byte alignment?
    // TODO - Need to update this function to work with latest version of skeletal_model_t
    make_skeletal_model_relocatable(relocatable_skel_model, skel_model);
    model->type = mod_iqm;
    Con_Printf("Copying final skeletal model struct... ");
    if (!model->cache.data)
        Cache_Alloc(&model->cache, skel_model_n_bytes, loadname);
    if (!model->cache.data)
        return;

    memcpy_vfpu(model->cache.data, (void*) relocatable_skel_model, skel_model_n_bytes);
    Con_Printf("DONE\n");

    Con_Printf("About to free temp skeletal model struct...");
    free_skeletal_model(skel_model);
    skel_model = nullptr;
    Con_Printf("DONE\n");
}



void build_skeleton(skeletal_skeleton_t *skeleton, skeletal_model_t *anim_model, int framegroup_idx, float frametime) {
    // ------------------------------------------------------------------------
    // Calculate skeleton bone -> animation bone mappings
    // 
    // Skeletons can pull animation data from any animation file, and each 
    // skeleton bone is matched to the animation bone by bone name
    //
    // ------------------------------------------------------------------------
    if(skeleton->model != anim_model) {
        // If we already have the mapping for this model, skip it
        if(skeleton->anim_model != anim_model) {
            skeleton->anim_model = anim_model;
            char **anim_model_bone_names = get_member_from_offset(anim_model->bone_name, anim_model);
            char **skel_model_bone_names = get_member_from_offset(skeleton->model->bone_name, skeleton->model);

            for(uint32_t i = 0; i < skeleton->model->n_bones; i++) {
                // Default to -1, indicating that this bone's corresponding animation bone has not yet been found
                skeleton->anim_bone_idx[i] = -1;
                char *skel_model_bone_name = get_member_from_offset(skel_model_bone_names[i], skeleton->model);
                for(uint32_t j = 0; j < anim_model->n_bones; j++) {
                    char *anim_model_bone_name = get_member_from_offset(anim_model_bone_names[j], anim_model);
                    if(!strcmp( skel_model_bone_name, anim_model_bone_name)) {
                        skeleton->anim_bone_idx[i] = j;
                        break;
                    }
                }
            }
        }
    }
    else {
        // If we already have the mapping for this model, skip it
        if(skeleton->anim_model != anim_model) {
            skeleton->anim_model = anim_model;
            for(int32_t i = 0; i < skeleton->model->n_bones; i++) {
                skeleton->anim_bone_idx[i] = i;
            }
        }
    }
    // ------------------------------------------------------------------------


    if(framegroup_idx < 0 || framegroup_idx >= anim_model->n_framegroups) {
        return;
    }

    // FIXME - Watch for offsets...
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

    Con_Printf("build_skel: framegroup %d frames lerp(%d,%d,%.2f) for t=%.2f\n", framegroup_idx, frame1_idx, frame2_idx, lerpfrac, frametime);
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

        // If we have a parent, concat parent transform to get model-space transform
        int parent_bone_idx = bone_parent_idx[i];
        if(parent_bone_idx >= 0) {
            mat3x4_t temp; 
            Matrix3x4_ConcatTransforms(temp, skeleton->bone_transforms[parent_bone_idx], skeleton->bone_transforms[i]);
            Matrix3x4_Copy(skeleton->bone_transforms[i], temp);
        }
        // If we don't have a parent, the bone-space transform _is_ the model-space transform
    }

    // Now that all bone transforms have been computed for the current pose, multiply in the inverse rest pose transforms
    // These transforms will take the vertices from model-space to each bone's local-space:
    for(uint32_t i = 0; i < anim_model->n_bones; i++) {
        Matrix3x4_ConcatTransforms( skeleton->bone_rest_to_pose_transforms[i], skeleton->bone_transforms[i], inv_bone_rest_transforms[i]);
        // Invert-transpose the upper-left 3x3 matrix to get the transform that is applied to vertex normals
        Matrix3x3_Invert_Transposed_Matrix3x4(skeleton->bone_rest_to_pose_normal_transforms[i], skeleton->bone_rest_to_pose_transforms[i]);
    }
}


void bind_submesh_bones(skeletal_mesh_t *submesh, skeletal_skeleton_t *skeleton) {
    // Transform matrix that undoes int16 / int8 quantization scale + ofs
    ScePspFMatrix4 undo_quantization;
    gumLoadIdentity(&undo_quantization);
    ScePspFVector3 undo_ofs = {submesh->verts_ofs[0],submesh->verts_ofs[1],submesh->verts_ofs[2]};
    ScePspFVector3 undo_scale = {submesh->verts_scale[0],submesh->verts_scale[1],submesh->verts_scale[2]};
    gumTranslate(&undo_quantization, &undo_ofs);
    gumScale(&undo_quantization, &undo_scale);
    
    for(int submesh_bone_idx = 0; submesh_bone_idx < submesh->n_skinning_bones; submesh_bone_idx++) {
        // Get the index into the skeleton list of bones:
        int bone_idx = submesh->skinning_bone_idxs[submesh_bone_idx];
        // mat3x4_t *bone_mat3x4 = &bone_rest_transforms[bone_idx];
        mat3x4_t *bone_mat3x4 = &(skeleton->bone_rest_to_pose_transforms[bone_idx]);

        // Translate the mat3x4_t bone transform matrix to ScePspFMatrix4
        ScePspFMatrix4 bone_mat;
        bone_mat.x.x = (*bone_mat3x4)[0][0];   bone_mat.y.x = (*bone_mat3x4)[0][1];   bone_mat.z.x = (*bone_mat3x4)[0][2];   bone_mat.w.x = (*bone_mat3x4)[0][3];
        bone_mat.x.y = (*bone_mat3x4)[1][0];   bone_mat.y.y = (*bone_mat3x4)[1][1];   bone_mat.z.y = (*bone_mat3x4)[1][2];   bone_mat.w.y = (*bone_mat3x4)[1][3];
        bone_mat.x.z = (*bone_mat3x4)[2][0];   bone_mat.y.z = (*bone_mat3x4)[2][1];   bone_mat.z.z = (*bone_mat3x4)[2][2];   bone_mat.w.z = (*bone_mat3x4)[2][3];
        bone_mat.x.w = 0.0f;                 bone_mat.y.w = 0.0f;                 bone_mat.z.w = 0.0f;                 bone_mat.w.w = 1.0f;

        // bone_mat = bone_mat * undo_quantization
        gumMultMatrix(&bone_mat, &bone_mat, &undo_quantization);
        sceGuBoneMatrix(submesh_bone_idx, &bone_mat);
    }
}


void R_DrawIQMModel(entity_t *ent) {
    // Con_Printf("IQM model draw!\n");

    // --------------–--------------–--------------–--------------–------------
    // Build the skeleton
    // --------------–--------------–--------------–--------------–------------
    skeletal_model_t *skel_model = (skeletal_model_t*) Mod_Extradata(ent->model);
    // FIXME - Hardcoded skeleton index...
    int cl_skel_idx = 0;

    if(cl_n_skeletons == 0){
        Con_Printf("Client has zero skeletons, building one!\n");
        cl_n_skeletons++;
        cl_skeletons[cl_skel_idx].model = skel_model;
        cl_skeletons[cl_skel_idx].anim_model = nullptr;
        cl_skeletons[cl_skel_idx].bone_transforms = (mat3x4_t*) malloc(sizeof(mat3x4_t) * skel_model->n_bones);
        cl_skeletons[cl_skel_idx].bone_rest_to_pose_transforms = (mat3x4_t*) malloc(sizeof(mat3x4_t) * skel_model->n_bones);
        cl_skeletons[cl_skel_idx].bone_rest_to_pose_normal_transforms = (mat3x3_t*) malloc(sizeof(mat3x3_t) * skel_model->n_bones);
        cl_skeletons[cl_skel_idx].anim_bone_idx = (int32_t*) malloc(sizeof(int32_t) * skel_model->n_bones);
    }

    int framegroup_idx = 0;
    float frametime = sv.time;
    build_skeleton(&cl_skeletons[cl_skel_idx], skel_model, framegroup_idx, frametime);
    // --------------–--------------–--------------–--------------–------------


    sceGumPushMatrix();
    R_BlendedRotateForEntity(ent, 0, ent->scale);

    sceGuDisable(GU_TEXTURE_2D);
    sceGuColor(0x000000);

    // FIXME - Update this to draw meshes and submeshes
    // for(int i = 0; i < skel_model->n_meshes; i++) {
    //     skeletal_mesh_t *mesh = &((skeletal_mesh_t*) ((uint8_t*)skel_model + (int)skel_model->meshes))[i];
    //     // FIXME
    //     vertex_t *mesh_verts = (vertex_t*) ((uint8_t*) skel_model + (int) mesh->verts);
    //     uint16_t *mesh_tri_verts = (uint16_t*) ((uint8_t*) skel_model + (int) mesh->tri_verts);
    //     sceGumDrawArray(GU_TRIANGLES,GU_INDEX_16BIT|GU_TEXTURE_32BITF|GU_NORMAL_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, mesh->n_tris * 3, mesh_tri_verts, mesh_verts);
    // }

    skeletal_mesh_t *meshes = get_member_from_offset(skel_model->meshes, skel_model);


    for(int i = 0; i < skel_model->n_meshes; i++) {
        // Con_Printf("Drawing mesh %d\n", i);
        skeletal_mesh_t *mesh = &meshes[i];
        // Con_Printf("\tn_submeshes: %d\n", mesh->n_submeshes);
        skeletal_mesh_t *submeshes = get_member_from_offset(mesh->submeshes, skel_model);
        for(int j = 0; j < mesh->n_submeshes; j++) {
            // Con_Printf("Drawing mesh %d submesh %d\n", i, j);
            skeletal_mesh_t *submesh = &submeshes[j];
            skel_vertex_i8_t *vert8s = get_member_from_offset(submesh->vert8s, skel_model);
            skel_vertex_i16_t *vert16s = get_member_from_offset(submesh->vert16s, skel_model);

            bind_submesh_bones(submesh, &cl_skeletons[cl_skel_idx]);

            sceGumDrawArray(
                GU_TRIANGLES,GU_WEIGHTS(8)|GU_WEIGHT_8BIT|GU_TEXTURE_16BIT|GU_NORMAL_16BIT|GU_VERTEX_16BIT|GU_TRANSFORM_3D, 
                submesh->n_tris * 3, 
                nullptr, 
                vert16s
            );
            // sceGumDrawArray(
            //     GU_TRIANGLES,GU_WEIGHTS(8)|GU_WEIGHT_8BIT|GU_TEXTURE_8BIT|GU_NORMAL_8BIT|GU_VERTEX_8BIT|GU_TRANSFORM_3D, 
            //     submesh->n_tris * 3, 
            //     nullptr, 
            //     vert8s
            // );
            // sceGumPopMatrix();
        }
    //     skeletal_mesh_t *mesh = &((skeletal_mesh_t*) ((uint8_t*)skel_model + (int)skel_model->meshes))[i];
    //     // FIXME
    //     vertex_t *mesh_verts = (vertex_t*) ((uint8_t*) skel_model + (int) mesh->verts);
    //     uint16_t *mesh_tri_verts = (uint16_t*) ((uint8_t*) skel_model + (int) mesh->tri_verts);
    //     sceGumDrawArray(GU_TRIANGLES,GU_INDEX_16BIT|GU_TEXTURE_32BITF|GU_NORMAL_32BITF|GU_VERTEX_32BITF|GU_TRANSFORM_3D, mesh->n_tris * 3, mesh_tri_verts, mesh_verts);
    }

    sceGuEnable(GU_TEXTURE_2D);


    // Draw bones
    // Con_Printf("------------------------------\n");
    // char **bone_names = get_member_from_offset(skel_model->bone_name, skel_model);
    mat3x4_t *bone_transforms = cl_skeletons[cl_skel_idx].bone_transforms;

    sceGuDisable(GU_DEPTH_TEST);
    sceGuDisable(GU_TEXTURE_2D);
    for(int i = 0; i < skel_model->n_bones; i++) {
        float line_verts_x[6] = {0,0,0,     1,0,0}; // Verts for x-axis
        float line_verts_y[6] = {0,0,0,     0,1,0}; // Verts for y-axis
        float line_verts_z[6] = {0,0,0,     0,0,1}; // Verts for z-axis

        ScePspFMatrix4 bone_mat;
        bone_mat.x.x = bone_transforms[i][0][0];   bone_mat.y.x = bone_transforms[i][0][1];   bone_mat.z.x = bone_transforms[i][0][2];   bone_mat.w.x = bone_transforms[i][0][3];
        bone_mat.x.y = bone_transforms[i][1][0];   bone_mat.y.y = bone_transforms[i][1][1];   bone_mat.z.y = bone_transforms[i][1][2];   bone_mat.w.y = bone_transforms[i][1][3];
        bone_mat.x.z = bone_transforms[i][2][0];   bone_mat.y.z = bone_transforms[i][2][1];   bone_mat.z.z = bone_transforms[i][2][2];   bone_mat.w.z = bone_transforms[i][2][3];
        bone_mat.x.w = 0.0f;                    bone_mat.y.w = 0.0f;                    bone_mat.z.w = 0.0f;                    bone_mat.w.w = 1.0f;
        sceGumPushMatrix();
        sceGumMultMatrix(&bone_mat);
        sceGuColor(0x0000ff); // red
        sceGumDrawArray(GU_LINES,GU_VERTEX_32BITF|GU_TRANSFORM_3D, 2, nullptr, line_verts_x);
        sceGuColor(0x00ff00); // green
        sceGumDrawArray(GU_LINES,GU_VERTEX_32BITF|GU_TRANSFORM_3D, 2, nullptr, line_verts_y);
        sceGuColor(0xff0000); // blue
        sceGumDrawArray(GU_LINES,GU_VERTEX_32BITF|GU_TRANSFORM_3D, 2, nullptr, line_verts_z);
        sceGumPopMatrix();
    }
    sceGuEnable(GU_DEPTH_TEST);
    sceGuEnable(GU_TEXTURE_2D);


    // for(int i = 0; i < skel_model->n_bones; i++) {
    //     char *bone_name = (char*)((uint8_t*) skel_model + (int) bone_names[i]);
    //     // Con_Printf("Drawing bone: %i, \"%s\"\n", i, bone_name);
    // }
    sceGumPopMatrix();
}

