#ifndef IQM_H
#define IQM_H



// Enable any of these to turn on debug prints
// #define IQMDEBUG_LOADIQM_LOADMESH
// #define IQMDEBUG_LOADIQM_MESHSPLITTING
// #define IQMDEBUG_LOADIQM_BONEINFO
#define IQMDEBUG_LOADIQM_ANIMINFO
#define IQMDEBUG_LOADIQM_DEBUGSUMMARY
#define IQMDEBUG_LOADIQM_PACKING
#define IQMDEBUG_SKEL_TRACELINE
// #define IQMDEBUG_CALC_MODEL_BOUNDS





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


// ----------------------------------------------------------------------------
// IQM file loading utils
// ----------------------------------------------------------------------------
#define IQMHEADER "INTERQUAKEMODEL\0"
#define IQM_MAGIC "INTERQUAKEMODEL"
#define IQM_VERSION_1 1
#define IQM_VERSION_2 2
#define IQM_MAX_BONES 256
#define IQM_VERT_BONES 4

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

typedef enum {
    IQM_VERT_POS            = 0,
    IQM_VERT_UV             = 1,
    IQM_VERT_NOR            = 2,
    IQM_VERT_TAN            = 3,
    IQM_VERT_BONE_IDXS      = 4,
    IQM_VERT_BONE_WEIGHTS   = 5,
    IQM_VERT_COLOR          = 6,
    IQM_VERT_CUSTOM         = 0X10
} iqm_vert_array_type;


typedef enum {
    IQM_DTYPE_BYTE          = 0,
    IQM_DTYPE_UBYTE         = 1,
    IQM_DTYPE_SHORT         = 2,
    IQM_DTYPE_USHORT        = 3,
    IQM_DTYPE_INT           = 4,
    IQM_DTYPE_UINT          = 5,
    IQM_DTYPE_HALF          = 6,
    IQM_DTYPE_FLOAT         = 7,
    IQM_DTYPE_DOUBLE        = 8,
} iqm_dtype;

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

typedef enum {
    IQM_ANIM_FLAG_LOOP = 1<<0
} iqm_anim_flag;

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



typedef struct iqm_model_vertex_arrays_s {
    vec2_t *verts_uv;
    vec3_t *verts_pos;
#ifdef IQM_LOAD_NORMALS
    vec3_t *verts_nor;
#endif // IQM_LOAD_NORMALS
    float *verts_bone_weights;
    uint8_t *verts_bone_idxs;
    // TODO - Add loading for these fields?
    // vec4_t *verts_tan;
    // vec4_t *verts_color;
} iqm_model_vertex_arrays_t;


size_t safe_strsize(char *str);
size_t pad_n_bytes(size_t n_bytes);
void iqm_parse_float_array(const void *iqm_data, const iqm_vert_array_t *vert_array, float *out, size_t n_elements, size_t element_len, float *default_value);
void iqm_parse_uint8_array(const void *iqm_data, const iqm_vert_array_t *vert_array, uint8_t *out, size_t n_elements, size_t element_len, uint8_t max_value);
const void *iqm_find_extension(const uint8_t *iqm_data, size_t iqm_data_size, const char *extension_name, size_t *extension_size);
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





// TODO - Remvoe this?
// Struct defined in platform-specific iqm
typedef struct skeletal_mesh_s skeletal_mesh_t;





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

    vec3_t *bone_rest_pos;      // NOTE - Is NULL for anim-only models (n_meshes=0)
    quat_t *bone_rest_rot;      // NOTE - Is NULL for anim-only models (n_meshes=0)
    vec3_t *bone_rest_scale;    // NOTE - Is NULL for anim-only models (n_meshes=0)

    // Cached values set at load-time
    int fps_cam_bone_idx; // -1 if no camera bone, otherwise it contains the index of the bone named `fps_cam`


    // Custom properties to support bone hitboxes
    bool *bone_hitbox_enabled;  // Whether or not to use the hitbox for each bone   // NOTE - Is NULL for anim-only models (n_meshes=0)
    vec3_t *bone_hitbox_ofs;    // Ofs of the bounding box for each bone            // NOTE - Is NULL for anim-only models (n_meshes=0)
    vec3_t *bone_hitbox_scale;  // Size of the bounding box for each bone           // NOTE - Is NULL for anim-only models (n_meshes=0)
    int *bone_hitbox_tag;       // Tag value used when traceline hits bone hitbox   // NOTE - Is NULL for anim-only models (n_meshes=0)


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
    mat3x4_t *bone_rest_transforms;     // NOTE - Is NULL for anim-only models (n_meshes=0)
    mat3x4_t *inv_bone_rest_transforms; // NOTE - Is NULL for anim-only models (n_meshes=0)
    

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


// ----------------------------------------------------------------------------
// Defined in platform-specific C++ IQM files, called from C
// ----------------------------------------------------------------------------
skeletal_mesh_t *load_iqm_meshes(const void *iqm_data, const iqm_model_vertex_arrays_t *iqm_vertex_arrays);
size_t count_unpacked_skeletal_model_meshes_n_bytes(skeletal_model_t *skel_model);
void pack_skeletal_model_meshes(skeletal_model_t *unpacked_skel_model_in, skeletal_model_t *packed_skel_model_out, uint8_t **buffer_head_ptr);
void free_unpacked_skeletal_model_meshes(skeletal_model_t *skel_model);
void debug_print_skeletal_model_meshes(skeletal_model_t *skel_model, int is_packed);
int load_material_texture_image(char *texture_file);
void set_unpacked_skeletal_model_mesh_material_indices(skeletal_model_t *skel_model);
// ----------------------------------------------------------------------------



#define PACKING_BYTE_PADDING 4


#define PACK_MEMBER(dest_ptr, src_ptr, n_bytes, buffer_start, buffer_head) (_pack_member( (void**) dest_ptr, (void*) src_ptr, n_bytes, buffer_start, buffer_head))
void _pack_member(void **dest_ptr, void *src_ptr, size_t n_bytes, uint8_t *buffer_start, uint8_t **buffer_head);
void *_unpack_member(void *packed_member_ptr, void *buffer_start);
#define UNPACK_MEMBER(packed_member_ptr, buffer_start) ((__typeof__(packed_member_ptr))_unpack_member(packed_member_ptr, buffer_start))
// Macro that takes an explicit "is_packed" argument, if not packed: returns input ptr, if packed: invokes PACK_MEMBER
#define OPT_UNPACK_MEMBER(packed_member_ptr, buffer_start, is_packed) \
    ((is_packed) ? (UNPACK_MEMBER(packed_member_ptr, buffer_start)) : (packed_member_ptr))


void free_pointer_and_clear(void **ptr);





bool ray_aabb_intersection(vec3_t ray_pos, vec3_t ray_dir, float *tmin);
skeletal_skeleton_t *cl_update_skel_for_ent(entity_t *ent);







void PF_getmodelindex(void);
void PF_getmovedistance(void);
void PF_skeleton_create(void);
void PF_skeleton_destroy(void);
void PF_skeleton_build(void);
bool sv_intersect_skeletal_model_ent(edict_t *ent, vec3_t ray_start, vec3_t ray_end, float *trace_fraction, int *trace_bone_idx, int *trace_bone_hitbox_tag);
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
bool sv_get_skeleton_bounds(int skel_idx, vec3_t mins, vec3_t maxs);
int cl_get_camera_bone_view_transform(entity_t *view_ent, mat3x4_t camera_anim_view_matrix_out);



void cl_get_skel_model_bounds(int model_idx, int animmodel_idx, vec3_t mins, vec3_t maxs);
void sv_get_skel_model_bounds(int model_idx, int animmodel_idx, vec3_t mins, vec3_t maxs);
void Mod_LoadIQMModel (model_t *model, void *buffer);


// TODO - Move these elsewhere
#define ZOMBIE_LIMB_STATE_HEAD      1
#define ZOMBIE_LIMB_STATE_ARM_L     2
#define ZOMBIE_LIMB_STATE_ARM_R     4
#define ZOMBIE_LIMB_STATE_LEG_L     8
#define ZOMBIE_LIMB_STATE_LEG_R     16
#define ZOMBIE_LIMB_STATE_FULL      (ZOMBIE_LIMB_STATE_HEAD | ZOMBIE_LIMB_STATE_ARM_L | ZOMBIE_LIMB_STATE_ARM_R | ZOMBIE_LIMB_STATE_LEG_L | ZOMBIE_LIMB_STATE_LEG_R)


#endif // IQM_H

