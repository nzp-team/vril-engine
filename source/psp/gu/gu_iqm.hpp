
// Doesn't compile with C... hmm...
#include <cstdint>

// Compiles with C, but then C++ features all fall apart below:
// #include <stdint.h>


#ifndef VIDEO_HARDWARE_IQM_H
#define VIDEO_HARDWARE_IQM_H


// 
// If defined, enables runtime calculation of skeletal model bbox when playing an arbitrary skeletal anim
// This bbox is used as an early-out check for raytrace skeleton bone collision detection.
// If enabled, the early-out bbox will accurately reflect the min / max positions of all bone bboxes across the animation
// If disabled, the early-out bbox will be static (currently 2x HLBSP player hull). 
// Any bones outside of this static bbox will not be deetected by raytrace collision detection.
// 
// TODO - Profile hit detection with and without this
// TODO - Profile time spent calculating the AABB for a model
// #define IQM_BBOX_PER_MODEL_PER_ANIM


// #define IQM_LOAD_NORMALS    // Uncomment this to load vertex normals


typedef matrix3x4 mat3x4_t;
typedef vec4_t quat_t;
typedef matrix3x3 mat3x3_t;



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
    int8_t bone_weights[8];
    int8_t u,v;

#ifdef IQM_LOAD_NORMALS
    int8_t nor_x, nor_y, nor_z;
#endif // IQM_LOAD_NORMALS

    int8_t x,y,z;
} skel_vertex_i8_t;






typedef struct skeletal_mesh_s {
    uint32_t n_verts;               // Number of vertices in this mesh
    uint32_t n_tris;                // Number of triangles in this mesh
    uint32_t geomset;
    uint32_t geomid;

    char *material_name;
    int material_idx;

    // ------------------------------------------------------------------------
    // Pointers to use to stash temporary data during model loading
    // ------------------------------------------------------------------------
    vec3_t *vert_rest_positions;    // Vert xyz coordinates
    vec2_t *vert_uvs;               // Vert UV coordinates

#ifdef IQM_LOAD_NORMALS
    vec3_t *vert_rest_normals;      // Vert normals
#endif // IQM_LOAD_NORMALS

    float  *vert_bone_weights;      // 4 bone weights per vertex
    uint8_t *vert_bone_idxs;        // 4 bone indices per vertex
    float *vert_skinning_weights;   // 8 weights per vertex (Used for mesh -> submesh splitting)
    uint16_t *tri_verts;            // (3 * n_tris) indices to verts that define triangles
    // ------------------------------------------------------------------------

    // ------------------------------------------------------------------------
    // Pointers to data structures holding the hardware-skinning vertex data
    // NOTE - These are only assigned at the submesh level
    // ------------------------------------------------------------------------
    // skel_vertex_f32_t *verts   = nullptr;    // float32 Vertex struct
    skel_vertex_i16_t *vert16s = nullptr;       // int16 Vertex struct
    skel_vertex_i8_t  *vert8s  = nullptr;       // int8 Vertex struct
    uint8_t n_skinning_bones = 0;               // Number of hardware skinning bones (<= 8)
    uint8_t skinning_bone_idxs[8];              // Model bone indices to use for hardware skinning
    vec3_t verts_ofs;                           // Offset for vertex quantization grid
    vec3_t verts_scale;                         // Scale for vertex quantization grid
    // ------------------------------------------------------------------------

    uint8_t n_submeshes = 0;
    struct skeletal_mesh_s *submeshes = nullptr;
} skeletal_mesh_t;


void Mod_LoadIQMModel (model_t *model, void *buffer);
void R_DrawIQMModel(entity_t *ent);


#endif // VIDEO_HARDWARE_IQM_H