
// Doesn't compile with C... hmm...
#include <cstdint>

// Compiles with C, but then C++ features all fall apart below:
// #include <stdint.h>


#ifndef VIDEO_HARDWARE_IQM_H
#define VIDEO_HARDWARE_IQM_H

#define IQMHEADER "INTERQUAKEMODEL\0"
#define IQM_MAGIC "INTERQUAKEMODEL"
#define IQM_VERSION_1 1
#define IQM_VERSION_2 2
#define IQM_MAX_BONES 256




// #define IQM_LOAD_NORMALS    // Uncomment this to load vertex normals



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


typedef struct skeletal_material_s {
    // char *name;
    // int idx; // ???
    // char *texture_file_name;
    // char *texture_file_path;
    int texture_idx; // Texture image index in `gltextures`
    bool transparent;
    bool fullbright;
    bool fog;
    bool shade_smooth; // or flat
    // bool color; // For color tinting

    bool add_color; // If true, below color is added to texture
    // TODO - replace with vec3_t color; or vec4_t color?
    float add_color_red;
    float add_color_green;
    float add_color_blue;
    float add_color_alpha;

} skeletal_material_t;



typedef struct skeletal_model_s {
    // Model mins / maxs across all animation frames
    vec3_t mins;
    vec3_t maxs;

    // List of meshes
    uint32_t n_meshes;
    skeletal_mesh_t *meshes;


    // FIXME - I'd like to add these fields, but the model struct is immutable at runtime...
    // FIXME    ... unless we stash this in a global struct outside of the model? modelindex -> list of animation indices and the bbox for it
    // FIXME    ... or we stash them in the skeleton (not a good idea)
    // FIXME    
    // uint32_t *anim_model_bounds_modelindex; // [100, 101, 102]
    // vec3_t *anim_model_bounds_mins; // Same length as above
    // vec3_t *anim_model_bounds_maxs; // Same length as above


    // List of bones
    uint32_t n_bones;
    char **bone_name;
    int16_t *bone_parent_idx;

    vec3_t *bone_rest_pos;
    quat_t *bone_rest_rot;
    vec3_t *bone_rest_scale;

    // Cached values set at load-time
    int fps_cam_bone_idx; // -1 if no camera bone, otherwise it contains the index of the bone named `fps_cam`


    // Custom properties to support bone hitboxes
    bool *bone_hitbox_enabled;  // Whether or not to use the hitbox for each bone
    vec3_t *bone_hitbox_ofs;    // Ofs of the bounding box for each bone
    vec3_t *bone_hitbox_scale;  // Size of the bounding box for each bone
    int *bone_hitbox_tag;       // Tag value returned for ray-hitbox intersections with each bone

    // Overall AABB mins / maxs of all bone hitboxes in the model's default hitbox config
    // ... for the model rest pose
    vec3_t rest_pose_bone_hitbox_mins;
    vec3_t rest_pose_bone_hitbox_maxs;
    // ... across all animations and frames in the model
    vec3_t anim_bone_hitbox_mins;
    vec3_t anim_bone_hitbox_maxs;



    // The per-bone transform that takes us from rest-pose model-space to bone local space
    // These are static, so compute them once
    mat3x4_t *bone_rest_transforms;
    mat3x4_t *inv_bone_rest_transforms;
    

    // Animation frames data
    uint16_t n_frames;
    vec3_t *frames_bone_pos;
    quat_t *frames_bone_rot;
    vec3_t *frames_bone_scale;
    float *frames_move_dist;


    // Animation framegroup data
    uint16_t n_framegroups;
    char **framegroup_name;
    uint32_t *framegroup_start_frame;
    uint32_t *framegroup_n_frames;
    float *framegroup_fps;
    bool *framegroup_loop;


    int n_materials;
    char **material_names;
    int *material_n_skins;
    skeletal_material_t **materials;


    // Animation framegroup FTE events data
    uint16_t *framegroup_n_events;
    float **framegroup_event_time;
    uint32_t **framegroup_event_code;
    char ***framegroup_event_data_str;
} skeletal_model_t;



typedef struct skeletal_skeleton_s {
    bool in_use;
    int modelindex; // Animation / skeleton data is pulled from this model
    skeletal_model_t *model; // Animation / skeleton data is pulled from this model

    // ------------------------------------------------------------------------
    // Skeleton State Variables
    // ------------------------------------------------------------------------
    // The following state variables are set whenever a pose is applied to a 
    // skeleton object.
    // ------------------------------------------------------------------------
    int anim_modelindex;
    int anim_framegroup; // Framegroup index (0,1,2, ...)
    float anim_starttime; // Server time at which the animation was started
    float anim_speed; // Anim playback speed (1.0 for normal, 0.5 for half-speed, 2.0 for double speed, etc.)
    float last_build_time; // Time at which the skeleton was last built. Used to avoid rebuilding

    // Server-only: Custom properties to support bone hitboxes
    bool *bone_hitbox_enabled;  // Whether or not to use the hitbox for each bone
    vec3_t *bone_hitbox_ofs;    // Ofs of the bounding box for each bone
    vec3_t *bone_hitbox_scale;  // Size of the bounding box for each bone
    int *bone_hitbox_tag;              // Tag value returned for ray-hitbox intersections with each bone

    // ------------------------------------------------------------------------
    // The following state variables are derived from the above state variables
    // ------------------------------------------------------------------------
    skeletal_model_t *anim_model; // Which skeletal model the animation data was read from
    mat3x4_t *bone_transforms; // Transforms bone's local-space to model-space
    mat3x4_t *bone_rest_to_pose_transforms; // Transforms bone's rest-pose model-space to posed model-space

#ifdef IQM_LOAD_NORMALS
    mat3x3_t *bone_rest_to_pose_normal_transforms; // 3x3 inverse-transpose of `bone_transforms` for vertex normals
#endif // IQM_LOAD_NORMALS

    int32_t *anim_bone_idx; // anim_bone_idx[i] = j : skeleton bone i reads data from animation bone j. If j == -1, bone i is not animated.
    // ------------------------------------------------------------------------
} skeletal_skeleton_t;



#define MAX_SKELETONS 256
extern skeletal_skeleton_t sv_skeletons[MAX_SKELETONS]; // Server-side skeleton objects
extern skeletal_skeleton_t cl_skeletons[MAX_SKELETONS]; // Client-side skeleton objects
extern int sv_n_skeletons;
extern int cl_n_skeletons;



void Mod_LoadIQMModel (model_t *model, void *buffer);
void R_DrawIQMModel(entity_t *ent);
int cl_get_camera_bone_view_transform(entity_t *view_ent, mat3x4_t camera_anim_view_matrix_out);
void Matrix3x4_to_ScePspFMatrix4(ScePspFMatrix4 *out, mat3x4_t *in);




#endif // VIDEO_HARDWARE_IQM_H