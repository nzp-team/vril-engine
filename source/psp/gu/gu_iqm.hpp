
// Doesn't compile with C... hmm...
#include <cstdint>

// Compiles with C, but then C++ features all fall apart below:
// #include <stdint.h>


#ifndef VIDEO_HARDWARE_IQM_H
#define VIDEO_HARDWARE_IQM_H



#define VERT_BONES 4
#define TRI_VERTS 3
#define SUBMESH_BONES 8




typedef struct skel_vertex_f32_s {
    float u, v;
    // uint32_t color; - NOTE - Removed

#ifdef IQM_LOAD_NORMALS
    float nor_x, nor_y, nor_z;
#endif // IQM_LOAD_NORMALS

    float x,y,z;
} skel_vertex_f32_t;


typedef struct skel_vertex_i16_s {
    // Bone skinning weights, one per bone.
    int8_t bone_weights[8];
    int16_t u,v;

#ifdef IQM_LOAD_NORMALS
    int16_t nor_x, nor_y, nor_z;
#endif // IQM_LOAD_NORMALS

    int16_t x,y,z;
} skel_vertex_i16_t;


typedef struct skel_vertex_i8_s {
    // Bone skinning weights, one per bone.
    int8_t bone_weights[SUBMESH_BONES];
    int8_t u,v;

#ifdef IQM_LOAD_NORMALS
    int8_t nor_x, nor_y, nor_z;
#endif // IQM_LOAD_NORMALS

    int8_t x,y,z;
} skel_vertex_i8_t;




// 
// Struct used to stash temporary mesh structure during loading
//
typedef struct skeletal_full_mesh_s {
    uint32_t n_verts;               // Number of vertices in this mesh
    uint32_t n_tris;                // Number of triangles in this mesh
    uint32_t geomset;
    uint32_t geomid;
    char *material_name;
    int material_idx;
    vec3_t *vert_rest_positions;    // Vert xyz coordinates
    vec2_t *vert_uvs;               // Vert UV coordinates

#ifdef IQM_LOAD_NORMALS
    vec3_t *vert_rest_normals;      // Vert normals
#endif // IQM_LOAD_NORMALS

    float  *vert_bone_weights;      // 4 bone weights per vertex
    uint8_t *vert_bone_idxs;        // 4 bone indices per vertex
    
    uint16_t *tri_verts;            // (3 * n_tris) indices to verts that define triangles
} skeletal_full_mesh_t;


typedef struct skeletal_submesh_s {
    uint32_t n_verts;               // Number of vertices in this mesh
    uint32_t n_tris;                // Number of triangles in this mesh

    // skel_vertex_f32_t *verts   = nullptr;    // float32 Vertex struct
    skel_vertex_i16_t *vert16s = nullptr;       // int16 Vertex struct
    skel_vertex_i8_t  *vert8s  = nullptr;       // int8 Vertex struct

    uint8_t n_skinning_bones = 0;               // Number of hardware skinning bones (<= 8)
    uint8_t skinning_bone_idxs[SUBMESH_BONES];  // Model bone indices to use for hardware skinning

    vec3_t verts_ofs;                           // Offset for vertex quantization grid
    vec3_t verts_scale;                         // Scale for vertex quantization grid
} skeletal_submesh_t;


typedef struct skeletal_mesh_s {
    uint32_t geomset;
    uint32_t geomid;
    char *material_name;
    int material_idx;
    uint8_t n_submeshes = 0;
    struct skeletal_submesh_s *submeshes = nullptr;
} skeletal_mesh_s;



// extern "C" {
//     skeletal_mesh_t *load_iqm_meshes(const void *iqm_data, const iqm_model_vertex_arrays_t *iqm_vertex_arrays);    
//     // TODO - Implement these
//     // pack_skel_meshes();
//     // count_skel_meshes_nbytes();
//     // free_unpacked_skel_meshes();
// };


void R_DrawIQMModel(entity_t *ent);
// void Mod_LoadIQMModel (model_t *model, void *buffer);


#endif // VIDEO_HARDWARE_IQM_H