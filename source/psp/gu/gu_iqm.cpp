extern"C" {
    #include "../../quakedef.h"
    void PR_CheckEmptyString (char *s);

}


#include <limits>

#include <pspgu.h>
#include <pspgum.h>


#include "nlohmann/json.hpp"
using json = nlohmann::json;


#include "gu_iqm.hpp"


extern vec3_t lightcolor; // LordHavoc: .lit support
extern char	skybox_name[32];
extern void Fog_DisableGFog();
extern void Fog_EnableGFog();
// extern char *PF_VarString (int	first);
extern char pr_string_temp[PR_MAX_TEMPSTRING];



void free_skeletal_model(skeletal_model_t *skel_model);




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









//
// Splits a `skeletal_mesh_t` mesh into one or more submeshes that reference no more than 8 bones
// Writes triangle submesh index into `tri_submesh_idxs` 
// CAUTION: Assumes `tri_submesh_idxs` is of length `mesh->n_tris`
//
// Returns the number of submeshes that were created
// 
int skeletal_submesh_get_tri_submesh_idxs(skeletal_full_mesh_t *mesh, int8_t *tri_submesh_idxs) {
#ifdef IQMDEBUG_LOADIQM_MESHSPLITTING
    Con_Printf("skeletal_submesh_get_tri_submesh_idxs -- Start\n");
#endif // IQMDEBUG_LOADIQM_MESHSPLITTING

    // --------------------------------------------------------------------
    // Build the set of bones referenced by each triangle
    // --------------------------------------------------------------------
    // Make lists to hold bones idxs referenced by each triangle, initialized to 0s
    uint8_t tri_n_bones[mesh->n_tris] = {}; // Contains the number of bones that the i-th triangle references
    uint8_t tri_bones[TRI_VERTS * VERT_BONES * mesh->n_tris] = {}; // Contains the list of bones referenced by the i-th triangle


    for(uint32_t tri_idx = 0; tri_idx < mesh->n_tris; tri_idx++) {
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


#ifdef IQMDEBUG_LOADIQM_MESHSPLITTING
    Con_Printf("skeletal_submesh_get_tri_submesh_idxs -- Done gathering triangle-bone references\n");
#endif // IQMDEBUG_LOADIQM_MESHSPLITTING


    // --------------------------------------------------------------------
    // Assign each triangle in the mesh to a submesh idx
    // --------------------------------------------------------------------
    const int SET_DISCARD = -2; // Discarded triangles
    const int SET_UNASSIGNED = -1; // Denotes unassigned triangles
    for(uint32_t j = 0; j < mesh->n_tris; j++) {
        tri_submesh_idxs[j] = SET_UNASSIGNED;
    }

    int cur_submesh_idx = -1;
    int cur_submesh_n_bones = 0;
    uint8_t cur_submesh_bones[SUBMESH_BONES];


    while(true) {
        // Find the unassigned triangle that uses the most bones:
        int cur_tri = -1;
        for(uint32_t tri_idx = 0; tri_idx < mesh->n_tris; tri_idx++) {
            // If this triangle isn't `UNASSIGNED`, skip it
            if(tri_submesh_idxs[tri_idx] != SET_UNASSIGNED) {
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


#ifdef IQMDEBUG_LOADIQM_MESHSPLITTING
    Con_Printf("skeletal_submesh_get_tri_submesh_idxs -- Found unassigned triangle: %d\n", cur_tri);
#endif // IQMDEBUG_LOADIQM_MESHSPLITTING


        // If we didn't find any triangles, stop submesh algorithm. We're done.
        if(cur_tri == -1) {
            break;
        }

        // Verify the triangle doesn't have more than the max bones allowed:
        if(tri_n_bones[cur_tri] > SUBMESH_BONES) {
            Con_Printf("Warning: Mesh Triangle %d references %d bones, which is more than the maximum allowed for any mesh (%d). Skipping triangle...\n", cur_tri, tri_n_bones[cur_tri], SUBMESH_BONES);
            // Discard it
            tri_submesh_idxs[cur_tri] = SET_DISCARD;
            continue;
        }


        cur_submesh_idx += 1;
        cur_submesh_n_bones = 0;
        for(int submesh_bone_idx = 0; submesh_bone_idx < SUBMESH_BONES; submesh_bone_idx++) {
            cur_submesh_bones[submesh_bone_idx] = 0;
        }
        
#ifdef IQMDEBUG_LOADIQM_MESHSPLITTING
        Con_Printf("Creating submesh %d for mesh\n", cur_submesh_idx);
#endif // IQMDEBUG_LOADIQM_MESHSPLITTING


        // Add the triangle to the current submesh:
        tri_submesh_idxs[cur_tri] = cur_submesh_idx;

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
            if(tri_submesh_idxs[tri_idx] != SET_UNASSIGNED) {
                continue;
            }

            // If this triangle's bones is not a subset of the current submesh bone list, skip it
            if(!bone_list_is_subset(&tri_bones[tri_idx * TRI_VERTS * VERT_BONES], tri_n_bones[tri_idx], cur_submesh_bones, cur_submesh_n_bones)) {
                continue;
            }

            // Otherwise, it is a subset, add it to the current submesh
            tri_submesh_idxs[tri_idx] = cur_submesh_idx;
        }


#ifdef IQMDEBUG_LOADIQM_MESHSPLITTING
        {
            // Print how many triangles belong to the current submesh
            int cur_submesh_n_tris = 0;
            int n_assigned_tris = 0;
            for(uint32_t tri_idx = 0; tri_idx < mesh->n_tris; tri_idx++) {
                if(tri_submesh_idxs[tri_idx] != SET_UNASSIGNED) {
                    n_assigned_tris++;
                }
                if(tri_submesh_idxs[tri_idx] == cur_submesh_idx) {
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
                if(tri_submesh_idxs[tri_idx] != SET_UNASSIGNED) {
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
            tri_submesh_idxs[cur_tri] = cur_submesh_idx;
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

            // Add all unassigned triangles from the main mesh that reference bones in this submesh's bone list
            for(uint32_t tri_idx = 0; tri_idx < mesh->n_tris; tri_idx++) {
                // If this triangle isn't `UNASSIGNED`, skip it
                if(tri_submesh_idxs[tri_idx] != SET_UNASSIGNED) {
                    continue;
                }

                // If this triangle's bones is not a subset of the current submesh bone list, skip it
                if(!bone_list_is_subset(&tri_bones[tri_idx * TRI_VERTS * VERT_BONES], tri_n_bones[tri_idx], cur_submesh_bones, cur_submesh_n_bones)) {
                    continue;
                }

                // Otherwise, it is a subset, add it to the current submesh
                tri_submesh_idxs[tri_idx] = cur_submesh_idx;
            }

#ifdef IQMDEBUG_LOADIQM_MESHSPLITTING
            {
                // Print how many triangles belong to the current submesh
                int cur_submesh_n_tris = 0;
                int n_assigned_tris = 0;
                for(uint32_t tri_idx = 0; tri_idx < mesh->n_tris; tri_idx++) {
                    if(tri_submesh_idxs[tri_idx] != SET_UNASSIGNED) {
                        n_assigned_tris++;
                    }
                    if(tri_submesh_idxs[tri_idx] == cur_submesh_idx) {
                        cur_submesh_n_tris++;
                    }
                }
                Con_Printf("\tDone adding new tris for cur triangle\n");
                Con_Printf("\tcur submesh (%d) n_tris: %d/%d, total assigned: %d/%d\n", cur_submesh_idx, cur_submesh_n_tris, mesh->n_tris, n_assigned_tris, mesh->n_tris);
            }
#endif // IQMDEBUG_LOADIQM_MESHSPLITTING
        }
    }
    int n_submeshes = cur_submesh_idx + 1;


#ifdef IQMDEBUG_LOADIQM_MESHSPLITTING
    Con_Printf("skeletal_submesh_get_tri_submesh_idxs -- Done, split into %d submeshes\n", n_submeshes);
#endif // IQMDEBUG_LOADIQM_MESHSPLITTING

    return n_submeshes;
}




//
// Given list of triangle submesh indices `tri_submesh_idxs`, splits up mesh `mesh` 
// into `n_submeshes` submeshes
//
skeletal_full_mesh_t *skeletal_submesh_build_submeshes(skeletal_full_mesh_t *mesh, int8_t *tri_submesh_idxs, int n_submeshes) {
#ifdef IQMDEBUG_LOADIQM_MESHSPLITTING
        Con_Printf("skeletal_submesh_build_submeshes -- Start\n");
#endif // IQMDEBUG_LOADIQM_MESHSPLITTING

    skeletal_full_mesh_t *submeshes = (skeletal_full_mesh_t *) malloc(sizeof(skeletal_full_mesh_t) * n_submeshes);

    for(int submesh_idx = 0; submesh_idx < n_submeshes; submesh_idx++) {
#ifdef IQMDEBUG_LOADIQM_MESHSPLITTING
        Con_Printf("Reconstructing submesh %d/%d for mesh\n", submesh_idx+1, n_submeshes);
#endif // IQMDEBUG_LOADIQM_MESHSPLITTING

        // ----------------------------------------------------------------
        // Build this submesh's list of triangle indices, vertex list, and
        // triangle vertex indices
        // ----------------------------------------------------------------
        // Count the number of triangles that have been assigned to this submesh
        int submesh_tri_count = 0;
        for(uint32_t tri_idx = 0; tri_idx < mesh->n_tris; tri_idx++) {
            if(tri_submesh_idxs[tri_idx] == submesh_idx) {
                submesh_tri_count++;
            }
        }

        // Allocate enough memory to fit theoretical max amount of unique vertes this model can reference (given we know its triangle count)
        uint16_t submesh_mesh_vert_idxs[TRI_VERTS * submesh_tri_count]; // Indices into mesh list of vertices
        // Allocate enough memoery to fit 3 vertex indices per triangle
        uint16_t submesh_tri_verts[TRI_VERTS * submesh_tri_count];
        uint32_t submesh_n_tris = 0;
        uint32_t submesh_n_verts = 0;

        for(uint32_t mesh_tri_idx = 0; mesh_tri_idx < mesh->n_tris; mesh_tri_idx++) {
            // Skip triangles that don't belong to this submesh
            if(tri_submesh_idxs[mesh_tri_idx] != submesh_idx) {
                continue;
            }

            // Add the triangle to our submesh's list of triangles
            int submesh_tri_idx = submesh_n_tris;
            submesh_n_tris += 1;

            // Add each of the triangle's verts to the submesh list of verts
            // If that vertex is already in the submesh, use that index instead
            for(int tri_vert_idx = 0; tri_vert_idx < TRI_VERTS; tri_vert_idx++) {
                int mesh_vert_idx = mesh->tri_verts[(mesh_tri_idx * TRI_VERTS) + tri_vert_idx];

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


        // ----------------------------------------------------------------
        // Populate the submesh struct vertex arrays
        // ----------------------------------------------------------------
        submeshes[submesh_idx].n_verts = submesh_n_verts;
        submeshes[submesh_idx].vert_rest_positions = (vec3_t*) malloc(sizeof(vec3_t) * submesh_n_verts);
        submeshes[submesh_idx].vert_uvs = (vec2_t*) malloc(sizeof(vec2_t) * submesh_n_verts);

#ifdef IQM_LOAD_NORMALS
        submeshes[submesh_idx].vert_rest_normals = (vec3_t*) malloc(sizeof(vec3_t) * submesh_n_verts);
#endif // IQM_LOAD_NORMALS
    
        submeshes[submesh_idx].vert_bone_weights = (float*) malloc(sizeof(float) * VERT_BONES * submesh_n_verts);
        submeshes[submesh_idx].vert_bone_idxs = (uint8_t*) malloc(sizeof(uint8_t) * VERT_BONES * submesh_n_verts);
        submeshes[submesh_idx].geomset = mesh->geomset;
        submeshes[submesh_idx].geomid = mesh->geomid;
        submeshes[submesh_idx].material_name = nullptr; // TODO - Don't copy string content for submeshes, it's held by the containing mesh
        submeshes[submesh_idx].material_idx = mesh->material_idx;

        for(uint32_t vert_idx = 0; vert_idx < submesh_n_verts; vert_idx++) {
            uint16_t mesh_vert_idx = submesh_mesh_vert_idxs[vert_idx]; 
            submeshes[submesh_idx].vert_rest_positions[vert_idx][0] = mesh->vert_rest_positions[mesh_vert_idx][0];
            submeshes[submesh_idx].vert_rest_positions[vert_idx][1] = mesh->vert_rest_positions[mesh_vert_idx][1];
            submeshes[submesh_idx].vert_rest_positions[vert_idx][2] = mesh->vert_rest_positions[mesh_vert_idx][2];
            submeshes[submesh_idx].vert_uvs[vert_idx][0] = mesh->vert_uvs[mesh_vert_idx][0];
            submeshes[submesh_idx].vert_uvs[vert_idx][1] = mesh->vert_uvs[mesh_vert_idx][1];
    
#ifdef IQM_LOAD_NORMALS
            submeshes[submesh_idx].vert_rest_normals[vert_idx][0] = mesh->vert_rest_normals[mesh_vert_idx][0];
            submeshes[submesh_idx].vert_rest_normals[vert_idx][1] = mesh->vert_rest_normals[mesh_vert_idx][1];
            submeshes[submesh_idx].vert_rest_normals[vert_idx][2] = mesh->vert_rest_normals[mesh_vert_idx][2];
#endif // IQM_LOAD_NORMALS

            for(int k = 0; k < VERT_BONES; k++) {
                submeshes[submesh_idx].vert_bone_weights[(vert_idx * VERT_BONES) + k] = mesh->vert_bone_weights[(mesh_vert_idx * VERT_BONES) + k];
                submeshes[submesh_idx].vert_bone_idxs[(vert_idx * VERT_BONES) + k] = mesh->vert_bone_idxs[(mesh_vert_idx * VERT_BONES) + k];
            }
        }

        // ----------------------------------------------------------------
        // Populate the submesh triangles
        // ----------------------------------------------------------------
        submeshes[submesh_idx].n_tris = submesh_n_tris;

        submeshes[submesh_idx].tri_verts = (uint16_t*) malloc(sizeof(uint16_t) * TRI_VERTS * submesh_n_tris);
        for(uint32_t tri_idx = 0; tri_idx < submesh_n_tris; tri_idx++) {
            submeshes[submesh_idx].tri_verts[tri_idx * TRI_VERTS + 0] = submesh_tri_verts[tri_idx * TRI_VERTS + 0];
            submeshes[submesh_idx].tri_verts[tri_idx * TRI_VERTS + 1] = submesh_tri_verts[tri_idx * TRI_VERTS + 1];
            submeshes[submesh_idx].tri_verts[tri_idx * TRI_VERTS + 2] = submesh_tri_verts[tri_idx * TRI_VERTS + 2];
        }
        // ----------------------------------------------------------------

#ifdef IQMDEBUG_LOADIQM_MESHSPLITTING
        Con_Printf("skeletal_submesh_build_submeshes -- Done\n");
#endif // IQMDEBUG_LOADIQM_MESHSPLITTING
    }
    return submeshes;
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
void calc_quantization_grid_scale_ofs(vec3_t *vert_pos, int n_verts, vec3_t verts_ofs, vec3_t verts_scale) {
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
    for(int axis = 0; axis < 3; axis++) {
        float model_min = vert_pos[0][axis];
        float model_max = vert_pos[0][axis];
        for(uint32_t vert_idx = 1; vert_idx < n_verts; vert_idx++) {
            model_min = std::min(vert_pos[vert_idx][axis], model_min);
            model_max = std::max(vert_pos[vert_idx][axis], model_max);
        }
        verts_ofs[axis] = ((model_max + model_min) / 2.0f);
        verts_scale[axis] = (model_max - model_min) / 2.0f;
    }

#ifdef IQMDEBUG_LOADIQM_LOADMESH
    Con_Printf("Done calculating quantization ofs / scale for mesh\n");
#endif // IQMDEBUG_LOADIQM_LOADMESH
    // ----------------------------------------------------------------------
}







//
// Converts a float `x` in [`min_x`,`max_x`] to integer type
// Supports any of: uint8_t, uint16_t, int8_t, int16_t
//
template <typename T, int min_x_i, int max_x_i>
T float_to_int(float x) {
    constexpr float min_x = (float) min_x_i;
    constexpr float max_x = (float) max_x_i;
    // Ensure x is within [min_val, max_val]
    x = std::min( max_x, std::max( min_x, x ));
    // Rescale to [0,1]
    x = (x - min_x) / (max_x - min_x);
    // Interpolate between T's min and max value
    constexpr T min_val = std::numeric_limits<T>::min();
    constexpr T max_val = std::numeric_limits<T>::max();
    T val = (T) (min_val + x * (max_val - min_val));
    // Clamp to bounds
    return static_cast<T>(std::min( max_val, std::max( min_val, val)));
}


//
// Applies the inverse of an ofs and a scale
//
float apply_inv_ofs_scale(float x, float ofs, float scale) {
    return (x - ofs) / scale;
}


// 
// Given a mesh loaded by ???, writes vertex draw struct that:
//      - Use quantization
//      - Remove triangle indices to have a raw list of unindexed vertices
//
template <typename T_verts, typename T_pos, typename T_uv, typename T_nor, typename T_bone_weight>
T_verts *build_skinning_vert_structs(skeletal_full_mesh_t *mesh, float *vert_skinning_weights, vec3_t verts_ofs, vec3_t verts_scale) {
    int n_unindexed_verts = mesh->n_tris * TRI_VERTS;
    T_verts *verts = (T_verts*) malloc(sizeof(T_verts) * n_unindexed_verts);

    for(uint32_t tri_idx = 0; tri_idx < mesh->n_tris; tri_idx++) {
        for(int tri_vert_idx = 0; tri_vert_idx < TRI_VERTS; tri_vert_idx++) {
            int unindexed_vert_idx = tri_idx * 3 + tri_vert_idx;
            uint16_t indexed_vert_idx = mesh->tri_verts[tri_idx * 3 + tri_vert_idx];

            // Con_Printf("\twriting position...\n");
            verts[unindexed_vert_idx].x = float_to_int<T_pos,-1,1>(apply_inv_ofs_scale(mesh->vert_rest_positions[indexed_vert_idx][0], verts_ofs[0], verts_scale[0]));
            verts[unindexed_vert_idx].y = float_to_int<T_pos,-1,1>(apply_inv_ofs_scale(mesh->vert_rest_positions[indexed_vert_idx][1], verts_ofs[1], verts_scale[1]));
            verts[unindexed_vert_idx].z = float_to_int<T_pos,-1,1>(apply_inv_ofs_scale(mesh->vert_rest_positions[indexed_vert_idx][2], verts_ofs[2], verts_scale[2]));
            // Con_Printf("\twriting uv...\n");
            verts[unindexed_vert_idx].u = float_to_int<T_uv,-1,1>(mesh->vert_uvs[indexed_vert_idx][0]);
            verts[unindexed_vert_idx].v = float_to_int<T_uv,-1,1>(mesh->vert_uvs[indexed_vert_idx][1]);

#ifdef IQM_LOAD_NORMALS
            // Con_Printf("\twriting normal...\n");
            verts[unindexed_vert_idx].nor_x = float_to_int<T_nor,-1,1>(mesh->vert_rest_normals[indexed_vert_idx][0]);
            verts[unindexed_vert_idx].nor_y = float_to_int<T_nor,-1,1>(mesh->vert_rest_normals[indexed_vert_idx][1]);
            verts[unindexed_vert_idx].nor_z = float_to_int<T_nor,-1,1>(mesh->vert_rest_normals[indexed_vert_idx][2]);
#endif // IQM_LOAD_NORMALS

            // Con_Printf("\twriting bone weights...\n");
            for(int bone_idx = 0; bone_idx < SUBMESH_BONES; bone_idx++) {
                // FIXME - This ain't right 

                verts[unindexed_vert_idx].bone_weights[bone_idx] = float_to_int<T_bone_weight,-1,1>(vert_skinning_weights[indexed_vert_idx * SUBMESH_BONES + bone_idx]);
            }
        }
    }
    return verts;
}



// 
// Loads the a full mesh from an IQM file without alteration
// 
void load_full_iqm_mesh(skeletal_full_mesh_t *mesh, int mesh_idx, const void *iqm_data, const iqm_model_vertex_arrays_t *iqm_vertex_arrays) {
    const iqm_header_t *iqm_header = (const iqm_header_t*) iqm_data;
    if(mesh_idx < 0 || mesh_idx >= iqm_header->n_meshes) {
        return;
    }

    const iqm_mesh_t *iqm_meshes = (const iqm_mesh_t*)((uint8_t*) iqm_data + iqm_header->ofs_meshes);

    const char *material_name = (const char*) (((uint8_t*) iqm_data + iqm_header->ofs_text) + iqm_meshes[mesh_idx].material);
    Con_Printf("Mesh[%d]: \"%s\"\n", mesh_idx, material_name);
    mesh->material_name = (char*) malloc(sizeof(char) * (strlen(material_name) + 1));
    strcpy(mesh->material_name, material_name);
    // Indicate that material has not yet been found in list of materials, we'll find it later
    mesh->material_idx = -1;
    

    uint32_t first_vert = iqm_meshes[mesh_idx].first_vert_idx;
    uint32_t n_verts = iqm_meshes[mesh_idx].n_verts;
    uint32_t first_tri = iqm_meshes[mesh_idx].first_tri;
    uint32_t n_tris = iqm_meshes[mesh_idx].n_tris;


    mesh->n_verts = n_verts;
    mesh->vert_rest_positions = (vec3_t*) malloc(sizeof(vec3_t) * n_verts);
#ifdef IQM_LOAD_NORMALS
    mesh->vert_rest_normals = (vec3_t*) malloc(sizeof(vec3_t) * n_verts);
#endif // IQM_LOAD_NORMALS
    mesh->vert_uvs = (vec2_t*) malloc(sizeof(vec2_t) * n_verts);
    mesh->vert_bone_weights = (float*) malloc(sizeof(float)  * VERT_BONES * n_verts);  // 4 bone weights per vertex
    mesh->vert_bone_idxs = (uint8_t*) malloc(sizeof(uint8_t) * VERT_BONES * n_verts);  // 4 bone indices per vertex

    for(uint32_t vert_idx = 0; vert_idx < n_verts; vert_idx++) {
        // Write static fields:
        mesh->vert_rest_positions[vert_idx][0] = iqm_vertex_arrays->verts_pos[first_vert + vert_idx][0];
        mesh->vert_rest_positions[vert_idx][1] = iqm_vertex_arrays->verts_pos[first_vert + vert_idx][1];
        mesh->vert_rest_positions[vert_idx][2] = iqm_vertex_arrays->verts_pos[first_vert + vert_idx][2];
        mesh->vert_uvs[vert_idx][0] = iqm_vertex_arrays->verts_uv[first_vert + vert_idx][0];
        mesh->vert_uvs[vert_idx][1] = iqm_vertex_arrays->verts_uv[first_vert + vert_idx][1];
#ifdef IQM_LOAD_NORMALS
        mesh->vert_rest_normals[vert_idx][0] = iqm_vertex_arrays->verts_nor[first_vert + vert_idx][0];
        mesh->vert_rest_normals[vert_idx][1] = iqm_vertex_arrays->verts_nor[first_vert + vert_idx][1];
        mesh->vert_rest_normals[vert_idx][2] = iqm_vertex_arrays->verts_nor[first_vert + vert_idx][2];
#endif // IQM_LOAD_NORMALS

        for(int vert_bone_idx = 0; vert_bone_idx < 4; vert_bone_idx++) {
            mesh->vert_bone_weights[vert_idx * 4 + vert_bone_idx] = iqm_vertex_arrays->verts_bone_weights[(first_vert + vert_idx) * 4 + vert_bone_idx];
            mesh->vert_bone_idxs[vert_idx * 4 + vert_bone_idx] = iqm_vertex_arrays->verts_bone_idxs[(first_vert + vert_idx) * 4 + vert_bone_idx];
        }
    }

    mesh->n_tris = n_tris;
    mesh->tri_verts = (uint16_t*) malloc(sizeof(uint16_t) * 3 * mesh->n_tris);

    for(uint32_t tri_idx = 0; tri_idx < mesh->n_tris; tri_idx++) {
        uint16_t vert_a = ((iqm_tri_t*)((uint8_t*) iqm_data + iqm_header->ofs_tris))[first_tri + tri_idx].vert_idxs[0] - first_vert;
        uint16_t vert_b = ((iqm_tri_t*)((uint8_t*) iqm_data + iqm_header->ofs_tris))[first_tri + tri_idx].vert_idxs[1] - first_vert;
        uint16_t vert_c = ((iqm_tri_t*)((uint8_t*) iqm_data + iqm_header->ofs_tris))[first_tri + tri_idx].vert_idxs[2] - first_vert;
        mesh->tri_verts[tri_idx * 3 + 0] = vert_a;
        mesh->tri_verts[tri_idx * 3 + 1] = vert_b;
        mesh->tri_verts[tri_idx * 3 + 2] = vert_c;
    }

    // --------------------------------------------------
    // Parse FTE_MESH IQM extension
    // --------------------------------------------------
    size_t iqm_fte_ext_mesh_size;
    size_t file_len = iqm_header->filesize;
    const iqm_ext_fte_mesh_t *iqm_fte_ext_mesh = (const iqm_ext_fte_mesh_t *) iqm_find_extension((uint8_t*) iqm_data, file_len, "FTE_MESH", &iqm_fte_ext_mesh_size);
    if(iqm_fte_ext_mesh != nullptr) {
        mesh->geomset = iqm_fte_ext_mesh[mesh_idx].geomset;
        mesh->geomid = iqm_fte_ext_mesh[mesh_idx].geomid;
    }
    // --------------------------------------------------
}



void free_full_iqm_mesh(skeletal_full_mesh_t *mesh) {
    if(mesh == nullptr) {
        return;
    }
    free_pointer_and_clear( (void**) &mesh->material_name);
    free_pointer_and_clear( (void**) &mesh->vert_rest_positions);
#ifdef IQM_LOAD_NORMALS
    free_pointer_and_clear( (void**) &mesh->vert_rest_normals);
#endif // IQM_LOAD_NORMALS
    free_pointer_and_clear( (void**) &mesh->vert_uvs);
    free_pointer_and_clear( (void**) &mesh->vert_bone_weights);
    free_pointer_and_clear( (void**) &mesh->vert_bone_idxs);
    free_pointer_and_clear( (void**) &mesh->tri_verts);
}







//
// Given iqm file header, and parsed vertex data arrays, allocate and return an
// array of platform-specific `skeletal_mesh_t` structs
//
skeletal_mesh_t *load_iqm_meshes(const void *iqm_data, const iqm_model_vertex_arrays_t *iqm_vertex_arrays) {

#ifdef IQMDEBUG_LOADIQM_LOADMESH
    Con_Printf("load_iqm_meshes -- Start\n");
#endif // IQMDEBUG_LOADIQM_LOADMESH



    const iqm_header_t *iqm_header = (const iqm_header_t*) iqm_data;

    if(iqm_header->n_meshes <= 0) {
        return nullptr;
    }

    // TODO - Verify we have non-null pointers in `iqm_vertex_arrays`?


    skeletal_mesh_t *meshes = (skeletal_mesh_t*) malloc(sizeof(skeletal_mesh_t) * iqm_header->n_meshes);
    
    for(int mesh_idx = 0; mesh_idx < iqm_header->n_meshes; mesh_idx++) {

        skeletal_full_mesh_t full_mesh;
        load_full_iqm_mesh(&full_mesh, mesh_idx, iqm_data, iqm_vertex_arrays);


        // --------------------------------------------------------------------
        // Load values at the mesh level
        // --------------------------------------------------------------------
        meshes[mesh_idx].geomset = full_mesh.geomset;
        meshes[mesh_idx].geomid = full_mesh.geomid;
        // Transfer ownership of heap-alloced material name char array
        meshes[mesh_idx].material_name = full_mesh.material_name;
        // Clear full_mesh's pointer so it's not freed
        full_mesh.material_name = nullptr;
        // --------------------------------------------------------------------


        // --------------------------------------------------------------------
        // Load values at the submesh level
        // --------------------------------------------------------------------

#ifdef IQMDEBUG_LOADIQM_LOADMESH
        Con_Printf("load_iqm_meshes -- Splitting mesh into submeshes\n");
#endif // IQMDEBUG_LOADIQM_LOADMESH

        int8_t tri_submesh_idxs[full_mesh.n_tris];
        // Submeshing step 1. Get submesh index for each mesh triangle
        int n_submeshes = skeletal_submesh_get_tri_submesh_idxs(&full_mesh, tri_submesh_idxs);
        // Submeshing step 2. Split the mesh into submeshes
        skeletal_full_mesh_t *full_mesh_submeshes = skeletal_submesh_build_submeshes(&full_mesh, tri_submesh_idxs, n_submeshes);

#ifdef IQMDEBUG_LOADIQM_LOADMESH
        Con_Printf("load_iqm_meshes -- Done splitting mesh into submeshes\n");
#endif // IQMDEBUG_LOADIQM_LOADMESH



        meshes[mesh_idx].n_submeshes = n_submeshes;
        meshes[mesh_idx].submeshes = (skeletal_submesh_t*) malloc(sizeof(skeletal_submesh_t) * n_submeshes);



        for(int submesh_idx = 0; submesh_idx < n_submeshes; submesh_idx++) {
#ifdef IQMDEBUG_LOADIQM_LOADMESH
            Con_Printf("load_iqm_meshes -- Writing vert structs for mesh %d/%d submesh %d/%d\n", mesh_idx+1, iqm_header->n_meshes, submesh_idx+1, n_submeshes);
#endif // IQMDEBUG_LOADIQM_LOADMESH

            skeletal_full_mesh_t *full_submesh = &(full_mesh_submeshes[submesh_idx]);
            skeletal_submesh_t *submesh = &(meshes[mesh_idx].submeshes[submesh_idx]);

            // Submesh vertices are stored as lists of triangles (no triangle indexing array)
            submesh->n_verts = full_submesh->n_tris * 3;
            submesh->n_tris = full_submesh->n_tris;

            // Initialize to all 0s
            submesh->n_skinning_bones = 0;
            for(int bone_idx = 0; bone_idx < SUBMESH_BONES; bone_idx++) {
                submesh->skinning_bone_idxs[bone_idx] = 0;
            }

            
            // Temporary array to calculate new bone weights layout for verts, initialized to all 0s
            float vert_skinning_weights[SUBMESH_BONES * full_submesh->n_verts] = {};


            for(uint32_t vert_idx = 0; vert_idx < full_submesh->n_verts; vert_idx++) {
                // Add all bones used by this vertex to the submesh's list of bones
                for(int vert_bone_idx = 0; vert_bone_idx < VERT_BONES; vert_bone_idx++) {
                    uint8_t bone_idx = full_submesh->vert_bone_idxs[vert_idx * VERT_BONES + vert_bone_idx];
                    float bone_weight = full_submesh->vert_bone_weights[vert_idx * VERT_BONES + vert_bone_idx];

                    if(bone_weight > 0.0f) {
                        int vert_bone_submesh_idx = -1;
                        // Search the submesh's list of bones for this bone
                        for(int submesh_bone_idx = 0; submesh_bone_idx < submesh->n_skinning_bones; submesh_bone_idx++) {
                            if(bone_idx == submesh->skinning_bone_idxs[submesh_bone_idx]) {
                                vert_bone_submesh_idx = submesh_bone_idx;
                                break;
                            }
                        }

                        // If we didn't find the bone, add it to the submesh's list of bones
                        if(vert_bone_submesh_idx == -1) {
                            if(submesh->n_skinning_bones >= SUBMESH_BONES) {
                                Con_Printf("Warning: Vertex puts submesh over %d bone limit. Reference ignored (submesh->n_skinning_bones=%d, bone=%d, weight=%.2f)\n", 
                                    SUBMESH_BONES,
                                    submesh->n_skinning_bones, 
                                    bone_idx, 
                                    bone_weight
                                );
                                continue;
                            }
                            submesh->skinning_bone_idxs[submesh->n_skinning_bones] = bone_idx;
                            vert_bone_submesh_idx = submesh->n_skinning_bones;
                            submesh->n_skinning_bones += 1;
                        }

                        // Set the vertex's corresponding weight entry for that bone
                        vert_skinning_weights[vert_idx * SUBMESH_BONES + vert_bone_submesh_idx] = bone_weight;
                    }
                }
            }


            calc_quantization_grid_scale_ofs(full_submesh->vert_rest_positions, full_submesh->n_verts, submesh->verts_ofs, submesh->verts_scale);
            submesh->vert8s  = build_skinning_vert_structs< skel_vertex_i8_t,  int8_t,  int8_t,  int8_t, int8_t>(full_submesh, vert_skinning_weights, submesh->verts_ofs, submesh->verts_scale);
            submesh->vert16s = build_skinning_vert_structs<skel_vertex_i16_t, int16_t, int16_t, int16_t, int8_t>(full_submesh, vert_skinning_weights, submesh->verts_ofs, submesh->verts_scale);

#ifdef IQMDEBUG_LOADIQM_LOADMESH
            Con_Printf("load_iqm_meshes -- Done writing vert structs for mesh %d/%d submesh %d/%d\n", mesh_idx+1, iqm_header->n_meshes, submesh_idx+1, n_submeshes);
#endif // IQMDEBUG_LOADIQM_LOADMESH
        }


        for(int submesh_idx = 0; submesh_idx < n_submeshes; submesh_idx++) {
#ifdef IQMDEBUG_LOADIQM_LOADMESH
            Con_Printf("load_iqm_meshes -- Clearing temporary memory for mesh %d/%d submesh %d/%d\n", mesh_idx+1, iqm_header->n_meshes, submesh_idx+1, n_submeshes);
#endif // IQMDEBUG_LOADIQM_LOADMESH

            free_full_iqm_mesh(&(full_mesh_submeshes[submesh_idx]));

#ifdef IQMDEBUG_LOADIQM_LOADMESH
            Con_Printf("load_iqm_meshes -- Clearing temporary memory for mesh %d/%d submesh %d/%d\n", mesh_idx+1, iqm_header->n_meshes, submesh_idx+1, n_submeshes);
#endif // IQMDEBUG_LOADIQM_LOADMESH

        }
        
        free(full_mesh_submeshes);
        // --------------------------------------------------------------------



#ifdef IQMDEBUG_LOADIQM_LOADMESH
        Con_Printf("load_iqm_meshes -- Clearing temporary memory for mesh %d/%d\n", mesh_idx+1, iqm_header->n_meshes);
#endif // IQMDEBUG_LOADIQM_LOADMESH

        free_full_iqm_mesh(&full_mesh);

#ifdef IQMDEBUG_LOADIQM_LOADMESH
        Con_Printf("load_iqm_meshes -- Done clearing temporary memory for mesh %d/%d\n", mesh_idx+1, iqm_header->n_meshes);
#endif // IQMDEBUG_LOADIQM_LOADMESH
    }


#ifdef IQMDEBUG_LOADIQM_LOADMESH
        Con_Printf("load_iqm_meshes -- Done for %d meshes\n", iqm_header->n_meshes);
#endif // IQMDEBUG_LOADIQM_LOADMESH

    return meshes;
}


// 
// Returns the total number of bytes required to store a skeletal_model_t's
// list of meshes ONLY, including all data pointed to via internal pointers
// 
uint32_t count_unpacked_skeletal_model_meshes_n_bytes(skeletal_model_t *skel_model) {

    uint32_t skel_model_n_bytes = 0;

    skel_model_n_bytes += sizeof(skeletal_mesh_t) * skel_model->n_meshes;
    for(int mesh_idx = 0; mesh_idx < skel_model->n_meshes; mesh_idx++) {
        skeletal_mesh_t *mesh = &skel_model->meshes[mesh_idx];
        skel_model_n_bytes += safe_strsize(mesh->material_name);
        skel_model_n_bytes += sizeof(skeletal_submesh_t) * mesh->n_submeshes;


        for(int submesh_idx = 0; submesh_idx < mesh->n_submeshes; submesh_idx++) {
            skeletal_submesh_t *submesh = &mesh->submeshes[submesh_idx];

            if(submesh->vert8s != NULL) {
                skel_model_n_bytes += sizeof(skel_vertex_i8_t) * submesh->n_verts;
            }
            if(submesh->vert16s != NULL) {
                skel_model_n_bytes += sizeof(skel_vertex_i16_t) * submesh->n_verts;
            }
        }
    }

    return skel_model_n_bytes;
}


void free_unpacked_skeletal_meshes(skeletal_model_t *unpacked_skel_model) {
    // FIXME - Implement this
}



// 
// Returns the total number of bytes required to store a skeletal_model_t
// including all data pointed to via pointers 
// 
// uint32_t count_skel_model_n_bytes(skeletal_model_t *skel_model) {
//     uint32_t skel_model_n_bytes = 0;
//     skel_model_n_bytes += sizeof(skeletal_model_t);
//     skel_model_n_bytes += sizeof(skeletal_mesh_t) * skel_model->n_meshes;
//     for(int i = 0; i < skel_model->n_meshes; i++) {
//         skeletal_mesh_t *mesh = &skel_model->meshes[i];
//         skel_model_n_bytes += safe_strsize(mesh->material_name);
//         skel_model_n_bytes += sizeof(skeletal_mesh_t) * mesh->n_submeshes;
//         for(int j = 0; j < mesh->n_submeshes; j++) {
//             skeletal_mesh_t *submesh = &mesh->submeshes[j];
//             if(submesh->vert8s != nullptr) {
//                 skel_model_n_bytes += sizeof(skel_vertex_i8_t) * submesh->n_verts;
//             }
//             if(submesh->vert16s != nullptr) {
//                 skel_model_n_bytes += sizeof(skel_vertex_i16_t) * submesh->n_verts;
//             }
//         }
//     }
//     // -- bones --
//     skel_model_n_bytes += sizeof(char*) * skel_model->n_bones;
//     skel_model_n_bytes += sizeof(int16_t) * skel_model->n_bones;
//     skel_model_n_bytes += sizeof(vec3_t) * skel_model->n_bones;
//     skel_model_n_bytes += sizeof(quat_t) * skel_model->n_bones;
//     skel_model_n_bytes += sizeof(vec3_t) * skel_model->n_bones;
//     for(int i = 0; i < skel_model->n_bones; i++) {
//         skel_model_n_bytes += safe_strsize(skel_model->bone_name[i]);
//     }
//     // -- bone hitbox info --
//     skel_model_n_bytes += sizeof(bool) * skel_model->n_bones;
//     skel_model_n_bytes += sizeof(vec3_t) * skel_model->n_bones;
//     skel_model_n_bytes += sizeof(vec3_t) * skel_model->n_bones;
//     skel_model_n_bytes += sizeof(int) * skel_model->n_bones;
//     // -- cached bone rest transforms --
//     skel_model_n_bytes += sizeof(mat3x4_t) * skel_model->n_bones;
//     skel_model_n_bytes += sizeof(mat3x4_t) * skel_model->n_bones;
//     // -- animation frames -- 
//     skel_model_n_bytes += sizeof(vec3_t) * skel_model->n_bones * skel_model->n_frames;
//     skel_model_n_bytes += sizeof(quat_t) * skel_model->n_bones * skel_model->n_frames;
//     skel_model_n_bytes += sizeof(vec3_t) * skel_model->n_bones * skel_model->n_frames;
//     skel_model_n_bytes += sizeof(float) * skel_model->n_frames;
//     // -- animation framegroups --
//     skel_model_n_bytes += sizeof(char*) * skel_model->n_framegroups;
//     skel_model_n_bytes += sizeof(uint32_t) * skel_model->n_framegroups;
//     skel_model_n_bytes += sizeof(uint32_t) * skel_model->n_framegroups;
//     skel_model_n_bytes += sizeof(float) * skel_model->n_framegroups;
//     skel_model_n_bytes += sizeof(bool) * skel_model->n_framegroups;
//     for(int i = 0; i < skel_model->n_framegroups; i++) {
//         skel_model_n_bytes += safe_strsize(skel_model->framegroup_name[i]);
//     }
//     // -- materials --
//     skel_model_n_bytes += sizeof(char*) * skel_model->n_materials;
//     skel_model_n_bytes += sizeof(int) * skel_model->n_materials;
//     skel_model_n_bytes += sizeof(skeletal_material_t*) * skel_model->n_materials;
//     for(int i = 0; i < skel_model->n_materials; i++) {
//         skel_model_n_bytes += safe_strsize(skel_model->material_names[i]);
//         skel_model_n_bytes += sizeof(skeletal_material_t) * skel_model->material_n_skins[i];
//     }
//     // -- FTE Anim Events --
//     if(skel_model->framegroup_n_events != nullptr) {
//         skel_model_n_bytes += sizeof(uint16_t) * skel_model->n_framegroups;
//         skel_model_n_bytes += sizeof(float*) * skel_model->n_framegroups;
//         skel_model_n_bytes += sizeof(char**) * skel_model->n_framegroups;
//         skel_model_n_bytes += sizeof(uint32_t*) * skel_model->n_framegroups;
//         for(int i = 0; i < skel_model->n_framegroups; i++) {
//             skel_model_n_bytes += sizeof(float) * skel_model->framegroup_n_events[i];
//             skel_model_n_bytes += sizeof(char*) * skel_model->framegroup_n_events[i];
//             skel_model_n_bytes += sizeof(uint32_t) * skel_model->framegroup_n_events[i];
//             for(int j = 0; j < skel_model->framegroup_n_events[i]; j++) {
//                 skel_model_n_bytes += safe_strsize(skel_model->framegroup_event_data_str[i][j]);
//             }
//         }
//     }
//     return skel_model_n_bytes;
// }



void pack_skeletal_model_meshes(skeletal_model_t *unpacked_skel_model_in, skeletal_model_t *packed_skel_model_out, uint8_t **buffer_head_ptr) {

    // `packed_skel_model_out` also points to the start of the contiguous memory buffer, grab that reference
    uint8_t *buffer = (uint8_t*) packed_skel_model_out;

    PACK_MEMBER(&packed_skel_model_out->meshes, unpacked_skel_model_in->meshes, sizeof(skeletal_mesh_t) * unpacked_skel_model_in->n_meshes, buffer, buffer_head_ptr);

    for(int mesh_idx = 0; mesh_idx < unpacked_skel_model_in->n_meshes; mesh_idx++) {
        skeletal_mesh_t *packed_mesh = &UNPACK_MEMBER(packed_skel_model_out->meshes,packed_skel_model_out)[mesh_idx];
        skeletal_mesh_t *unpacked_mesh = &unpacked_skel_model_in->meshes[mesh_idx];

        PACK_MEMBER(&packed_mesh->material_name, unpacked_mesh->material_name, safe_strsize(unpacked_mesh->material_name), buffer, buffer_head_ptr);
        PACK_MEMBER(&packed_mesh->submeshes, unpacked_mesh->submeshes, sizeof(skeletal_submesh_t) * unpacked_mesh->n_submeshes, buffer, buffer_head_ptr);

        for(int submesh_idx = 0; submesh_idx < unpacked_mesh->n_submeshes; submesh_idx++) {
            skeletal_submesh_t *packed_submesh = &UNPACK_MEMBER(packed_mesh->submeshes,packed_skel_model_out)[submesh_idx];
            skeletal_submesh_t *unpacked_submesh = &unpacked_mesh->submeshes[submesh_idx];

            PACK_MEMBER(&packed_submesh->vert8s, unpacked_submesh->vert8s, sizeof(skel_vertex_i8_t) * unpacked_submesh->n_verts, buffer, buffer_head_ptr);
            PACK_MEMBER(&packed_submesh->vert16s, unpacked_submesh->vert16s, sizeof(skel_vertex_i16_t) * unpacked_submesh->n_verts, buffer, buffer_head_ptr);
        }
    }
}


//
// Given a packed or unpacked `skel_model`, debug prints its platform-specicif mesh data
//
void debug_print_skeletal_model_meshes(skeletal_model_t *skel_model, int is_packed) {

    skeletal_mesh_t *meshes = OPT_UNPACK_MEMBER(skel_model->meshes, skel_model, is_packed);

    for(int mesh_idx = 0; mesh_idx < skel_model->n_meshes; mesh_idx++) {
        Con_Printf("---------------------\n");
        char *material_name = OPT_UNPACK_MEMBER(meshes[mesh_idx].material_name, skel_model, is_packed);
        Con_Printf("skel_model->meshes[%d].geomset = %d\n",           mesh_idx, meshes[mesh_idx].geomset);
        Con_Printf("skel_model->meshes[%d].geomid = %d\n",            mesh_idx, meshes[mesh_idx].geomid);
        Con_Printf("skel_model->meshes[%d].material_name = \"%s\"\n", mesh_idx, material_name);
        Con_Printf("skel_model->meshes[%d].material_idx = %d\n",      mesh_idx, meshes[mesh_idx].material_idx);
        Con_Printf("skel_model->meshes[%d].n_submeshes = %d\n",       mesh_idx, meshes[mesh_idx].n_submeshes);
        Con_Printf("skel_model->meshes[%d].submeshes = %d\n",         mesh_idx, meshes[mesh_idx].submeshes);
        Con_Printf("---------------------\n");

        skeletal_submesh_t *submeshes = OPT_UNPACK_MEMBER(meshes[mesh_idx].submeshes, skel_model, is_packed);

        for(int submesh_idx = 0; submesh_idx < meshes[mesh_idx].n_submeshes; submesh_idx++) {
            Con_Printf("\t----------\n");
            Con_Printf("\tskel_model->meshes[%d].submeshes[%d].n_verts = %d\n",          mesh_idx, submesh_idx, submeshes[submesh_idx].n_verts);
            Con_Printf("\tskel_model->meshes[%d].submeshes[%d].n_tris = %d\n",           mesh_idx, submesh_idx, submeshes[submesh_idx].n_tris);
            Con_Printf("\tskel_model->meshes[%d].submeshes[%d].vert16s = %d\n",          mesh_idx, submesh_idx, submeshes[submesh_idx].vert16s);
            Con_Printf("\tskel_model->meshes[%d].submeshes[%d].vert8s = %d\n",           mesh_idx, submesh_idx, submeshes[submesh_idx].vert8s);
            Con_Printf("\tskel_model->meshes[%d].submeshes[%d].n_skinning_bones = %d\n", mesh_idx, submesh_idx, submeshes[submesh_idx].n_skinning_bones);
            Con_Printf("\tskel_model->meshes[%d].submeshes[%d].skinning_bone_idxs = [",  mesh_idx, submesh_idx);
            for(int submesh_bone_idx = 0; submesh_bone_idx < SUBMESH_BONES; submesh_bone_idx++) {
                Con_Printf("%d, ", submeshes[submesh_idx].skinning_bone_idxs[submesh_bone_idx]);
            }
            Con_Printf("]\n");

            // --
            Con_Printf("\tskel_model->meshes[%d].submeshes[%d].verts_ofs = [%f, %f, %f]\n", 
                mesh_idx, submesh_idx,
                submeshes[submesh_idx].verts_ofs[0], 
                submeshes[submesh_idx].verts_ofs[1], 
                submeshes[submesh_idx].verts_ofs[2]
            );
            // --
            Con_Printf("\tskel_model->meshes[%d].submeshes[%d].verts_scale = [%f, %f, %f]\n", 
                mesh_idx, submesh_idx,
                submeshes[submesh_idx].verts_scale[0], 
                submeshes[submesh_idx].verts_scale[1], 
                submeshes[submesh_idx].verts_scale[2]
            );
            // --
            Con_Printf("\t----------\n");
        }
        Con_Printf("---------------------\n");
    }
}



void bind_submesh_bones(skeletal_submesh_t *submesh, skeletal_skeleton_t *skeleton) {
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
    // char **bone_names = UNPACK_MEMBER(skel_model->bone_name, skel_model);
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


    bool *bone_hitbox_enabled = UNPACK_MEMBER(skel->model->bone_hitbox_enabled, skel->model);
    vec3_t *bone_hitbox_ofs = UNPACK_MEMBER(skel->model->bone_hitbox_ofs, skel->model);
    vec3_t *bone_hitbox_scale = UNPACK_MEMBER(skel->model->bone_hitbox_scale, skel->model);
    int *bone_hitbox_tag = UNPACK_MEMBER(skel->model->bone_hitbox_tag, skel->model);


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
        bool bbox_hit = ray_aabb_intersection(ray_start_skel_space, ray_dir_skel_space, &bbox_hit_tmin);
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
            bool bbox_hit = ray_aabb_intersection(ray_start_bone_space, ray_dir_bone_space, &bbox_hit_tmin);

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
    mat3x4_t *bone_transforms = UNPACK_MEMBER(skel_model->bone_rest_transforms, skel_model);

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
    // char **bone_names = UNPACK_MEMBER(skel_model->bone_name, skel_model);
    mat3x4_t *bone_transforms = skel->bone_transforms;
    skeletal_model_t *skel_model = skel->model;


    // -------------–
    // IQMFIXME - Remove these
    // -------------–
    // Test debug -- Draw bones at their rest positions...
    // mat3x4_t *bone_rest_transforms = UNPACK_MEMBER(skel_model->bone_rest_transforms, skel_model);
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
// Converts a HSV color's hue value to RGB
// https://gist.github.com/ciembor/1494530#file-gistfile1-c-L86
//
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


//
// Converts a HSL color to RGB
// https://gist.github.com/ciembor/1494530#file-gistfile1-c-L86
//
void hsl2rgb(vec3_t hsl_in, vec3_t rgb_out) {
    if(hsl_in[1] == 0) {
        VectorSet(rgb_out, hsl_in[2], hsl_in[2], hsl_in[2]);
    }
    else {
        float h = hsl_in[0];
        float s = hsl_in[1];
        float l = hsl_in[2];

        float q = l < 0.5 ? l * (1 + s) : l + s - l * s;
        float p = 2 * l - q;

        rgb_out[0] = hue2rgb(p,q,h + 1./3);
        rgb_out[1] = hue2rgb(p,q,h);
        rgb_out[2] = hue2rgb(p,q,h - 1./3);
    }
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
        // hsl2rgb(hsl, rgb);

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
    


    skeletal_mesh_t *meshes = UNPACK_MEMBER(skel_model->meshes, skel_model);

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
        int *material_n_skins = UNPACK_MEMBER(skel_model->material_n_skins, skel_model);
        
        skeletal_material_t *material = nullptr;
        if(material_idx >= 0 && skel_model->n_materials > 0) {
            skeletal_material_t **materials = UNPACK_MEMBER(skel_model->materials, skel_model);
            skeletal_material_t *material_skins = UNPACK_MEMBER(materials[material_idx], skel_model);
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
        skeletal_submesh_t *submeshes = UNPACK_MEMBER(mesh->submeshes, skel_model);
        for(int j = 0; j < mesh->n_submeshes; j++) {
            // Con_Printf("Drawing mesh %d submesh %d\n", i, j);
            skeletal_submesh_t *submesh = &submeshes[j];
            skel_vertex_i8_t *vert8s = UNPACK_MEMBER(submesh->vert8s, skel_model);
            skel_vertex_i16_t *vert16s = UNPACK_MEMBER(submesh->vert16s, skel_model);



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
