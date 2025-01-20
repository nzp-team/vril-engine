#ifndef IQM_H

// ----------------------------------------------------------------------------
// IQM file loading utils
// ----------------------------------------------------------------------------
#define IQMHEADER "INTERQUAKEMODEL\0"
#define IQM_MAGIC "INTERQUAKEMODEL"
#define IQM_VERSION_1 1
#define IQM_VERSION_2 2
#define IQM_MAX_BONES 256

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


void iqm_parse_float_array(const void *iqm_data, const iqm_vert_array_t *vert_array, float *out, size_t n_elements, size_t element_len, float *default_value);
void iqm_parse_uint8_array(const void *iqm_data, const iqm_vert_array_t *vert_array, uint8_t *out, size_t n_elements, size_t element_len, uint8_t max_value);
static const void *iqm_find_extension(const uint8_t *iqm_data, size_t iqm_data_size, const char *extension_name, size_t *extension_size);
// ----------------------------------------------------------------------------
















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
    // skeletal_mesh_t *meshes;
    void *meshes; // void-pointer to platform-specific mesh structs


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


#ifdef IQM_BBOX_PER_MODEL_PER_ANIM
    // Overall AABB mins / maxs of all bone hitboxes in the model's default hitbox config
    // ... for the model rest pose
    vec3_t rest_pose_bone_hitbox_mins;
    vec3_t rest_pose_bone_hitbox_maxs;
    // ... across all animations and frames in the model
    vec3_t anim_bone_hitbox_mins;
    vec3_t anim_bone_hitbox_maxs;
#endif // IQM_BBOX_PER_MODEL_PER_ANIM


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


















void PF_getmodelindex(void);
void PF_getmovedistance(void);
void PF_skeleton_create(void);
void PF_skeleton_destroy(void);
void PF_skeleton_build(void);
qboolean sv_intersect_skeletal_model_ent(edict_t *ent, vec3_t ray_start, vec3_t ray_end, float *trace_fraction, int *trace_bone_idx, int *trace_bone_hitbox_tag);
void PF_skel_get_bonename(void);
void PF_skel_get_boneparent(void);
void PF_skel_find_bone(void);
void PF_skel_set_bone_hitbox_enabled(void);
void PF_skel_set_bone_hitbox_ofs(void);
void PF_skel_set_bone_hitbox_scale(void);
void PF_skel_set_bone_hitbox_tag(void);
void PF_skel_reset_bone_hitboxes(void);
void PF_getframeduration(void);
void PF_processmodelevents(void);
void PF_skel_register_anim(void);
void sv_skel_clear_all();
qboolean sv_get_skeleton_bounds(int skel_idx, vec3_t mins, vec3_t maxs);
int cl_get_camera_bone_view_transform(entity_t *view_ent, mat3x4_t camera_anim_view_matrix_out);

#endif // IQM_H