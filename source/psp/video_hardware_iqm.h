
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


typedef struct skel_vertex_f32_s {
    float u, v;
    // uint32_t color; - NOTE - Removed
    float nor_x, nor_y, nor_z;
    float x,y,z;
} skel_vertex_f32_t;


typedef struct skel_vertex_i16_s {
    // Bone skinning weights, one per bone.
    int8_t bone_weights[8];
    int16_t u,v;
    int16_t nor_x, nor_y, nor_z;
    int16_t x,y,z;
} skel_vertex_i16_t;


typedef struct skel_vertex_i8_s {
    // Bone skinning weights, one per bone.
    int8_t bone_weights[8];
    int8_t u,v;
    int8_t nor_x, nor_y, nor_z;
    int8_t x,y,z;
} skel_vertex_i8_t;






typedef struct skeletal_mesh_s {
    uint32_t n_verts;               // Number of vertices in this mesh
    uint32_t n_tris;                // Number of triangles in this mesh
    // ------------------------------------------------------------------------
    // Pointers to use to stash temporary data during model loading
    // ------------------------------------------------------------------------
    vec3_t *vert_rest_positions;    // Vert xyz coordinates
    vec2_t *vert_uvs;               // Vert UV coordinates
    vec3_t *vert_rest_normals;      // Vert normals
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



typedef struct skeletal_model_s {
    // Model mins / maxs across all animation frames
    vec3_t mins;
    vec3_t maxs;


    // List of meshes
    uint32_t n_meshes;
    skeletal_mesh_t *meshes;


    // List of bones
    uint32_t n_bones;
    char **bone_name;
    int16_t *bone_parent_idx;

    vec3_t *bone_rest_pos;
    quat_t *bone_rest_rot;
    vec3_t *bone_rest_scale;


    // The per-bone transform that takes us from rest-pose model-space to bone local space
    // These are static, so compute them once
    mat3x4_t *bone_rest_transforms;
    mat3x4_t *inv_bone_rest_transforms;
    

    // Animation frames data
    uint16_t n_frames;
    // TODO - IQM has a parent index for each pose, do we need it? is it always the same as the parent's bone parent idx?
    vec3_t *frames_bone_pos;
    quat_t *frames_bone_rot;
    vec3_t *frames_bone_scale;


    // Animation framegroup data
    uint16_t n_framegroups;
    char **framegroup_name;
    uint32_t *framegroup_start_frame;
    uint32_t *framegroup_n_frames;
    float *framegroup_fps;
    bool *framegroup_loop;


} skeletal_model_t;



typedef struct skeletal_skeleton_s {
    const skeletal_model_t *model; // Animation / skeleton data is pulled from this model

    // ------------------------------------------------------------------------
    // Skeleton State Variables
    // ------------------------------------------------------------------------
    // The following state variables are set whenever a pose is applied to a 
    // skeleton object.
    // ------------------------------------------------------------------------
    const skeletal_model_t *anim_model; // Which skeletal model the animation data was read from
    mat3x4_t *bone_transforms; // Transforms bone's local-space to model-space
    mat3x4_t *bone_rest_to_pose_transforms; // Transforms bone's rest-pose model-space to posed model-space
    mat3x3_t *bone_rest_to_pose_normal_transforms; // 3x3 inverse-transpose of `bone_transforms` for vertex normals
    int32_t *anim_bone_idx; // anim_bone_idx[i] = j : skeleton bone i reads data from animation bone j. If j == -1, bone i is not animated.
    // ------------------------------------------------------------------------
} skeletal_skeleton_t;



#define MAX_SKELETONS 256
skeletal_skeleton_t sv_skeletons[MAX_SKELETONS]; // Server-side skeleton objects
skeletal_skeleton_t cl_skeletons[MAX_SKELETONS]; // Client-side skeleton objects
int sv_n_skeletons = 0;
int cl_n_skeletons = 0;




skeletal_model_t *load_iqm_file(const char*file_path);



void Mod_LoadIQMModel (model_t *model, void *buffer);
void R_DrawIQMModel(entity_t *ent);

#endif // VIDEO_HARDWARE_IQM_H