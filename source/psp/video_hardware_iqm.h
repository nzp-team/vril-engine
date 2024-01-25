
#include <cstdint>


#ifndef VIDEO_HARDWARE_IQM_H
#define VIDEO_HARDWARE_IQM_H

#define IQMHEADER "INTERQUAKEMODEL\0"
#define IQM_MAGIC "INTERQUAKEMODEL"
#define IQM_VERSION_1 1
#define IQM_VERSION_2 2
#define IQM_MAX_BONES 256

typedef matrix3x4 mat3x4_t;
typedef vec4_t quat_t;
typedef matrix3x3 mat3x3_t;


typedef struct iqm_header_s {
    char magic[16];
    uint32_t version;
    uint32_t filesize;
    uint32_t flags;
    uint32_t n_text, ofs_text;
    uint32_t n_meshes, ofs_meshes;
    uint32_t n_vert_arrays, n_verts, ofs_vert_arrays;
    uint32_t n_tris, ofs_tris, ofs_adjacency;
    uint32_t n_joints, ofs_joints;
    uint32_t n_poses, ofs_poses;
    uint32_t n_anims, ofs_anims;
    uint32_t n_frames, n_frame_channels, ofs_frames, ofs_bounds;
    uint32_t n_comments, ofs_comments;
    uint32_t n_extensions, ofs_extensions;
} iqm_header_t;

typedef struct iqm_mesh_s {
    uint32_t name;
    uint32_t material;
    uint32_t first_vert_idx, n_verts;
    uint32_t first_tri, n_tris;
} iqm_mesh_t;

enum class iqm_vert_array_type : uint32_t {
    IQM_VERT_POS            = 0,
    IQM_VERT_UV             = 1,
    IQM_VERT_NOR            = 2,
    IQM_VERT_TAN            = 3,
    IQM_VERT_BONE_IDXS      = 4,
    IQM_VERT_BONE_WEIGHTS   = 5,
    IQM_VERT_COLOR          = 6,
    IQM_VERT_CUSTOM         = 0X10
};

enum class iqm_dtype : uint32_t {
    IQM_DTYPE_BYTE          = 0,
    IQM_DTYPE_UBYTE         = 1,
    IQM_DTYPE_SHORT         = 2,
    IQM_DTYPE_USHORT        = 3,
    IQM_DTYPE_INT           = 4,
    IQM_DTYPE_UINT          = 5,
    IQM_DTYPE_HALF          = 6,
    IQM_DTYPE_FLOAT         = 7,
    IQM_DTYPE_DOUBLE        = 8,
};


typedef struct iqm_tri_s {
    uint32_t vert_idxs[3];
} iqm_tri_t;


typedef struct iqm_joint_euler_s {
    uint32_t name;
    int32_t parent_joint_idx;
    float translate[3], rotate[3], scale[3];
} iqm_joint_euler_t;


typedef struct iqm_joint_quaternion_s {
    uint32_t name;
    int32_t parent_joint_idx;
    float translate[3], rotate[4], scale[3];
} iqm_joint_quaternion_t;


typedef struct iqm_pose_euler_s {
    int32_t parent_idx; // Parent POSE idx? parent bone idx?
    uint32_t mask;
    float channel_ofs[9];
    float channel_scale[9];
} iqm_pose_euler_t;


typedef struct iqm_pose_quaternion_s {
    int32_t parent_idx; // Parent POSE idx? parent bone idx?
    uint32_t mask;
    float channel_ofs[10];
    float channel_scale[10];
} iqm_pose_quaternion_t;

typedef struct iqm_anim_s {
    uint32_t name;
    uint32_t first_frame, n_frames;
    float framerate;
    uint32_t flags;
} iqm_anim_t;


enum class iqm_anim_flag : uint32_t {
    IQM_ANIM_FLAG_LOOP = 1<<0
};


typedef struct iqm_vert_array_s {
    uint32_t type; // TODO - iqm_vert_array_type?
    uint32_t flags;
    uint32_t format;// TODO - iqm_dtype
    uint32_t size;
    uint32_t ofs;
} iqm_vert_array_t;


typedef struct iqm_bounds_s {
    float mins[3], maxs[3];
    float xyradius;
    float radius;
} iqm_bounds_t;


typedef struct iqm_extension_s {
    uint32_t name;
    uint32_t n_data, ofs_data;
    uint32_t ofs_extensions; // Linked list pointer to next extension. (As byte offset from start of IQM file)
} iqm_extension_t;


typedef struct iqm_ext_fte_mesh_s {
    uint32_t contents;      // default: CONTENTS_BODY (0x02000000)
    uint32_t surfaceflags;  // Propagates to trace_surfaceflags
    uint32_t surfaceid;     // Body reported to QC via trace_surface
    uint32_t geomset;
    uint32_t geomid;
    float min_dist;
    float max_dist;
} iqm_ext_fte_mesh_t;


typedef struct iqm_ext_fte_event_s {
    uint32_t anim;
    float timestamp;
    uint32_t event_code;
    uint32_t event_data_str;        // Stringtable
} iqm_ext_fte_event_t;


typedef struct iqm_ext_fte_skin_s {
    uint32_t n_skinframes;
    uint32_t n_meshskins;
} iqm_ext_fte_skin_t;


typedef struct iqm_ext_fte_skin_skinframe_s {
    uint32_t material_idx;
    uint32_t shadertext_idx;
} iqm_ext_fte_skin_skinframe_t;


typedef struct iqm_ext_fte_skin_meshskin_s {
    uint32_t first_frame;
    uint32_t n_frames;
    float interval;
} iqm_ext_fte_skin_meshskin_t;


typedef struct vertex_s {
    float u, v;
    // uint32_t color; - NOTE - Removed
    float nor_x, nor_y, nor_z;
    float x,y,z;
} vertex_t;

typedef struct skeletal_mesh_s {
    uint32_t n_verts; // Number of unique vertices in this mesh
    // vec3_t *vert_rest_positions; // Contains the rest position of all vertices
    vertex_t *verts = nullptr; // Vertex struct passed to GU for drawing
    // skinning_vertex_t *skinning_verts = nullptr; // Vertex struct used for hardware skinning (has 8 skinning weights)

    uint32_t n_tris; // Number of triangles in mesh
    uint16_t *tri_verts = nullptr; // Contains the vertex indices for each triangle

} skeletal_mesh_t;

typedef struct skeletal_model_s {
    // Model mins / maxs across all animation frames
    vec3_t mins;
    vec3_t maxs;


    // List of meshes
    uint32_t n_meshes;
    skeletal_mesh_t *meshes;


} skeletal_model_t;


skeletal_model_t *load_iqm_file(const char*file_path);



void Mod_LoadIQMModel (model_t *model, void *buffer);
void R_DrawIQMModel(entity_t *ent);

#endif // VIDEO_HARDWARE_IQM_H