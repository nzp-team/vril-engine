// ----------------------------------------------------------------------------
// IQM file loading utils
// ----------------------------------------------------------------------------
//
// Parses an IQM Vertex array from an IQM data file and converts all values to floats
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
// ----------------------------------------------------------------------------














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




#ifdef IQM_BBOX_PER_MODEL_PER_ANIM

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

#else // IQM_BBOX_PER_MODEL_PER_ANIM

// 
// If dynamic bbox calculations are disabled, this is a stub
// (If we remove this function, we'll need to remove references to it from QC)
//
void PF_skel_register_anim(void) {};

#endif // IQM_BBOX_PER_MODEL_PER_ANIM



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


void PF_skel_get_boneparent(void) {
    Con_Printf("PF_skel_get_boneparent start\n");
    int skel_idx = G_FLOAT(OFS_PARM0);
    int bone_idx = G_FLOAT(OFS_PARM1);
    int16_t parent_bone_idx = sv_skel_get_boneparent(skel_idx, bone_idx);
    G_FLOAT(OFS_RETURN) = parent_bone_idx;
    Con_Printf("PF_skel_get_boneparent end\n");
};



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
#ifdef IQM_BBOX_PER_MODEL_PER_ANIM

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



#else // IQM_BBOX_PER_MODEL_PER_ANIM


inline void _get_skel_model_bounds(vec3_t mins, vec3_t maxs) {
    // If dynamic bbox calculations are enabled, treat skeletal model bbox max size as:
    // ... HLBSP player hull (Option 1)
    // vec3_t bbox_mins {-16, -16, -36};
    // vec3_t bbox_maxs { 16,  16,  36};

    // ... 2x HLBSP player hull (Option 2)
    vec3_t bbox_mins {-32, -32, -72};
    vec3_t bbox_maxs { 32,  32,  72};

    VectorCopy( bbox_mins, mins);
    VectorCopy( bbox_maxs, maxs);
}


void cl_get_skel_model_bounds(int model_idx, int animmodel_idx, vec3_t mins, vec3_t maxs) {
    // Dynamic bbox calculates are disabled, return static size for all skeletal models
    _get_skel_model_bounds(mins, maxs);
}


void sv_get_skel_model_bounds(int model_idx, int animmodel_idx, vec3_t mins, vec3_t maxs) {
    // Dynamic bbox calculates are disabled, return static size for all skeletal models
    _get_skel_model_bounds(mins, maxs);
}

#endif // IQM_BBOX_PER_MODEL_PER_ANIM
// ============================================================================

		
