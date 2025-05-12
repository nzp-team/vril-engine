#include "quakedef.h"


// Engine-defined global holding name for model currently being loaded
extern char loadname[];
extern char pr_string_temp[PR_MAX_TEMPSTRING];



// ----------------------------------------------------------------------------
// IQM file loading utils
// ----------------------------------------------------------------------------
//
// Parses an IQM Vertex array from an IQM data file and converts all values to floats
//
void iqm_parse_float_array(const void *iqm_data, const iqm_vert_array_t *vert_array, float *out, size_t n_elements, size_t element_len, float *default_value) {
    if(vert_array == NULL) {
        return;
    }
    iqm_dtype dtype = (iqm_dtype) vert_array->format;
    size_t iqm_values_to_read = (size_t) vert_array->size;
    if(vert_array->ofs == 0) {
        iqm_values_to_read = 0;
        dtype = IQM_DTYPE_FLOAT;
    }
    const uint8_t *iqm_array_data = (const uint8_t*) iqm_data + vert_array->ofs;

    // Special cases:
    if(dtype == IQM_DTYPE_FLOAT && element_len == iqm_values_to_read) {
        memcpy(out, (const float*) iqm_array_data, sizeof(float) * element_len * n_elements);
        return;
    }
    if(dtype == IQM_DTYPE_HALF) {
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
                case IQM_DTYPE_BYTE:
                    out[i * element_len + j] = ((const int8_t*)iqm_array_data)[i * iqm_values_to_read + j] * (1.0f/127);
                    break;
                case IQM_DTYPE_UBYTE:
                    out[i * element_len + j] = ((const uint8_t*)iqm_array_data)[i * iqm_values_to_read + j] * (1.0f/255);
                    break;
                case IQM_DTYPE_SHORT:
                    out[i * element_len + j] = ((const int16_t*)iqm_array_data)[i * iqm_values_to_read + j] * (1.0f/32767);
                    break;
                case IQM_DTYPE_USHORT:
                    out[i * element_len + j] = ((const uint16_t*)iqm_array_data)[i * iqm_values_to_read + j] * (1.0f/65535);
                    break;
                case IQM_DTYPE_INT:
                    out[i * element_len + j] = ((const int32_t*)iqm_array_data)[i * iqm_values_to_read + j]  / ((float)0x7fffffff);
                    break;
                case IQM_DTYPE_UINT:
                    out[i * element_len + j] = ((const uint32_t*)iqm_array_data)[i * iqm_values_to_read + j] / ((float)0xffffffffu);
                    break;
                case IQM_DTYPE_FLOAT:
                    out[i * element_len + j] = ((const float*)iqm_array_data)[i * iqm_values_to_read + j];
                    break;
                case IQM_DTYPE_DOUBLE:
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
    if(vert_array == NULL) {
        return;
    }
    iqm_dtype dtype = (iqm_dtype) vert_array->format;
    size_t iqm_values_to_read = (size_t) vert_array->size;
    if(vert_array->ofs == 0) {
        iqm_values_to_read = 0;
        dtype = IQM_DTYPE_UBYTE;
    }
    const uint8_t *iqm_array_data = (const uint8_t*) iqm_data + vert_array->ofs;


    // // Special cases:
    // if(dtype == IQM_DTYPE_FLOAT && element_len == iqm_values_to_read) {
    //     memcpy(out, (const float*) iqm_array_data, sizeof(float) * element_len * n_elements);
    //     return;
    // }
    if(dtype == IQM_DTYPE_HALF) {
        iqm_values_to_read = 0;
    }

    // For all other dtype cases, parse each value from IQM:
    for(size_t i = 0; i < n_elements; i++) {
        // Read the first `iqm_values_to_read` values for vector `i`
        for(size_t j = 0; j < element_len && j < iqm_values_to_read; j++) {

            uint8_t in_val;
            switch(dtype) {
                case IQM_DTYPE_FLOAT:    // Skip, these values don't make sense
                case IQM_DTYPE_DOUBLE:   // Skip, these values don't make sense
                default:
                    in_val = 0;
                    iqm_values_to_read = 0;
                    break;
                case IQM_DTYPE_BYTE:     // Interpret as signed
                case IQM_DTYPE_UBYTE:
                    in_val = ((const uint8_t*)iqm_array_data)[i * iqm_values_to_read + j];
                    break;
                case IQM_DTYPE_SHORT:    // Interpret as signed
                case IQM_DTYPE_USHORT:
                    in_val = (uint8_t) ((const uint16_t*)iqm_array_data)[i * iqm_values_to_read + j];
                    break;
                case IQM_DTYPE_INT:      // Interpret as signed
                case IQM_DTYPE_UINT:
                    in_val = (uint8_t) ((const uint32_t*)iqm_array_data)[i * iqm_values_to_read + j];
                    break;
            }

            if(in_val > max_value) {
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


const void *iqm_find_extension(const uint8_t *iqm_data, size_t iqm_data_size, const char *extension_name, size_t *extension_size) {
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
    return NULL;
}
// ----------------------------------------------------------------------------




// 
// Util function that frees a pointer and sets it to NULL
// 
void free_pointer_and_clear(void **ptr) {
    if(*ptr != NULL) {
        free(*ptr);
        *ptr = NULL;
    }
}



#define MAX_SKELETONS 256
skeletal_skeleton_t sv_skeletons[MAX_SKELETONS]; // Server-side skeleton objects
skeletal_skeleton_t cl_skeletons[MAX_SKELETONS]; // Client-side skeleton objects
int sv_n_skeletons;
int cl_n_skeletons;




void free_skeleton(skeletal_skeleton_t *skeleton) {
    skeleton->model = NULL;
    skeleton->anim_model = NULL;
    free_pointer_and_clear( (void**) &skeleton->bone_transforms);
    free_pointer_and_clear( (void**) &skeleton->bone_rest_to_pose_transforms);

#ifdef IQM_LOAD_NORMALS
    free_pointer_and_clear( (void**) &skeleton->bone_rest_to_pose_normal_transforms);
#endif // IQM_LOAD_NORMALS

    free_pointer_and_clear( (void**) &skeleton->anim_bone_idx);
    free_pointer_and_clear( (void**) &skeleton->bone_hitbox_enabled);
    free_pointer_and_clear( (void**) &skeleton->bone_hitbox_ofs);
    free_pointer_and_clear( (void**) &skeleton->bone_hitbox_scale);
    free_pointer_and_clear( (void**) &skeleton->bone_hitbox_tag);
}


//
// Returns the skeletal model with index `model_idx`
// Returns NULL otherwise if invalid or if not a skeletal model.
//
skeletal_model_t *cl_get_skel_model_for_modelidx(int model_idx) {
    if(model_idx < 0 || model_idx >= MAX_MODELS) {
        Con_DPrintf("Model idx out of bounds: %d, return -1\n", model_idx);
        return NULL;
    }

    // Safety check to make sure model at index is valid
    model_t *model = cl.model_precache[model_idx];
    if(model == NULL) {
        Con_DPrintf("Model NULL at index: %d, return -1\n", model_idx);
        return NULL;
    }
    if(model->type != mod_iqm) {
        Con_DPrintf("Model type is not IQM: model: %d != %d, type: %d != %d, return -1\n", model_idx, model->type, mod_iqm);
        return NULL;
    }
    
    skeletal_model_t *skel_model = (skeletal_model_t*)Mod_Extradata(model);
    return skel_model;
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
        char **anim_model_bone_names = UNPACK_MEMBER(anim_model->bone_name, anim_model);
        char **skel_model_bone_names = UNPACK_MEMBER(skel_model->bone_name, skel_model);

        for(uint32_t i = 0; i < skel_model->n_bones; i++) {
            // Default to -1, indicating that this bone's corresponding animation bone has not yet been found
            skel_bone_to_anim_bone_mapping[i] = -1;
            char *skel_model_bone_name = UNPACK_MEMBER(skel_model_bone_names[i], skel_model);
            for(uint32_t j = 0; j < anim_model->n_bones; j++) {
                char *anim_model_bone_name = UNPACK_MEMBER(anim_model_bone_names[j], anim_model);
                if(!strcmp( skel_model_bone_name, anim_model_bone_name)) {
                    skel_bone_to_anim_bone_mapping[i] = j;
                    break;
                }
            }
        }
    }
}




void build_skeleton(skeletal_skeleton_t *skeleton, skeletal_model_t *anim_model, int framegroup_idx, float frametime) {
    if(skeleton == NULL || anim_model == NULL) {
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

    uint32_t *anim_model_framegroup_start_frame = UNPACK_MEMBER(anim_model->framegroup_start_frame, anim_model);
    uint32_t *anim_model_framegroup_n_frames = UNPACK_MEMBER(anim_model->framegroup_n_frames, anim_model);
    float *anim_model_framegroup_fps = UNPACK_MEMBER(anim_model->framegroup_fps, anim_model);
    bool *anim_model_framegroup_loop = UNPACK_MEMBER(anim_model->framegroup_loop, anim_model);


    // Con_Printf("Build_skeleton for model with %d bones\n", anim_model->n_bones);
    // Con_Printf("anim n framegroups: %d\n", anim_model->n_framegroups);
    // Con_Printf("framegroup_idx: %d\n", framegroup_idx);
    // Con_Printf("framegroup_fps: %f\n", anim_model_framegroup_fps[framegroup_idx]);
    // Con_Printf("framegroup_loop: %d\n", anim_model_framegroup_loop[framegroup_idx]);
    // Con_Printf("framegroup_n_frames: %d\n", anim_model_framegroup_n_frames[framegroup_idx]);
    // Con_Printf("framegroup_start_frame: %d\n", anim_model_framegroup_start_frame[framegroup_idx]);



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
        frame1_idx = fmin(fmax(0, frame1_idx), (int) anim_model_framegroup_n_frames[framegroup_idx] - 1);
        frame2_idx = fmin(fmax(0, frame2_idx), (int) anim_model_framegroup_n_frames[framegroup_idx] - 1);
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
        lerpfrac = fmin(fmax(0.0f, lerpfrac), 1.0f);
    }
    frame1_idx = anim_model_framegroup_start_frame[framegroup_idx] + frame1_idx;
    frame2_idx = anim_model_framegroup_start_frame[framegroup_idx] + frame2_idx;
    // TODO - If lerpfrac is 0.0 or 1.0, don't interpolate, just copy the bone poses

    // Con_Printf("build_skel: framegroup %d frames lerp(%d,%d,%.2f) for t=%.2f\n", framegroup_idx, frame1_idx, frame2_idx, lerpfrac, frametime);
    int16_t *bone_parent_idx = UNPACK_MEMBER(skeleton->model->bone_parent_idx, skeleton->model);
    mat3x4_t *bone_rest_transforms = UNPACK_MEMBER(skeleton->model->bone_rest_transforms, skeleton->model);
    mat3x4_t *inv_bone_rest_transforms = UNPACK_MEMBER(skeleton->model->inv_bone_rest_transforms, skeleton->model);

    // If skeletal model used to define the skeleton doesn't have bone rest transforms defined, stop
    if(bone_rest_transforms == NULL || inv_bone_rest_transforms == NULL) {
        return;
    }

    vec3_t *anim_model_frames_bone_pos = UNPACK_MEMBER(anim_model->frames_bone_pos, anim_model);
    quat_t *anim_model_frames_bone_rot = UNPACK_MEMBER(anim_model->frames_bone_rot, anim_model);
    vec3_t *anim_model_frames_bone_scale = UNPACK_MEMBER(anim_model->frames_bone_scale, anim_model);


    // Con_Printf("-----------------------\n");
    // Con_Printf("framegroup_fps: %f\n", anim_model_framegroup_fps[framegroup_idx]);
    // Con_Printf("framegroup_loop: %d\n", anim_model_framegroup_loop[framegroup_idx]);
    // Con_Printf("framegroup_n_frames: %d\n", anim_model_framegroup_n_frames[framegroup_idx]);
    // Con_Printf("framegroup_start_frame: %d\n", anim_model_framegroup_start_frame[framegroup_idx]);


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
        // if(skeleton->model->n_bones > 20) {
        // // if(i == 0 && skeleton->model->n_bones == 20) {

        //     Con_Printf("Interpolating transform for bone %d / %d at frames: %d to %d with frac: %f\n", i, skeleton->model->n_bones, frame1_idx, frame2_idx, lerpfrac);
        //     Con_Printf("frametime: %f\n", frametime);
        //     Con_Printf("framegroup_idx: %d\n", framegroup_idx);
        //     Con_Printf("framegroup_fps: %f\n", anim_model_framegroup_fps[framegroup_idx]);
        //     Con_Printf("raw frame1_idx: %d\n", (int) floor(frametime * anim_model_framegroup_fps[framegroup_idx]));
        //     Con_Printf("framegroup_loop: %d\n", anim_model_framegroup_loop[framegroup_idx]);
        //     Con_Printf("framegroup_n_frames: %d\n", anim_model_framegroup_n_frames[framegroup_idx]);
        //     Con_Printf("framegroup_start_frame: %d\n", anim_model_framegroup_start_frame[framegroup_idx]);


        //     Con_Printf("\tPos: (%.2f,%.2f,%.2f) = lerp(a=(%.2f,%.2f,%.2f),b=(%.2f,%.2f,%.2f),t=%.2f)\n",
        //         bone_local_pos[0], bone_local_pos[1], bone_local_pos[2], 
        //         (*frame1_pos)[0], (*frame1_pos)[1], (*frame1_pos)[2], 
        //         (*frame2_pos)[0], (*frame2_pos)[1], (*frame2_pos)[2], 
        //         lerpfrac
        //     );
        //     Con_Printf("\tRot: (%.2f,%.2f,%.2f,%.2f) = lerp(a=(%.2f,%.2f,%.2f,%.2f),b=(%.2f,%.2f,%.2f,%.2f),t=%.2f)\n",
        //         bone_local_rot[0], bone_local_rot[1], bone_local_rot[2], bone_local_rot[3], 
        //         (*frame1_rot)[0], (*frame1_rot)[1], (*frame1_rot)[2], (*frame1_rot)[3], 
        //         (*frame2_rot)[0], (*frame2_rot)[1], (*frame2_rot)[2], (*frame2_rot)[3], 
        //         lerpfrac
        //     );
        //     Con_Printf("\tScale: (%.2f,%.2f,%.2f) = lerp(a=(%.2f,%.2f,%.2f),b=(%.2f,%.2f,%.2f),t=%.2f)\n",
        //         bone_local_scale[0], bone_local_scale[1], bone_local_scale[2], 
        //         (*frame1_scale)[0], (*frame1_scale)[1], (*frame1_scale)[2], 
        //         (*frame2_scale)[0], (*frame2_scale)[1], (*frame2_scale)[2], 
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

        // Con_Printf("\tBuild skel bone posed transform components %d / %d:\n", i, skeleton->model->n_bones);
        // Con_Printf("\t\tPos: [%f, %f, %f]\n", bone_local_pos[0], bone_local_pos[1], bone_local_pos[2]);
        // Con_Printf("\t\tRot: [%f, %f, %f, %f]\n", bone_local_rot[0], bone_local_rot[1], bone_local_rot[2], bone_local_rot[3]);
        // Con_Printf("\t\tScale: [%f, %f, %f]\n", bone_local_scale[0], bone_local_scale[1], bone_local_scale[2]);
        // Con_Printf("\tBuild skel bone posed transform %d:\n", i);
        // Con_Printf("\t\t[%f, %f, %f, %f]\n", skeleton->bone_transforms[i][0][0], skeleton->bone_transforms[i][0][1], skeleton->bone_transforms[i][0][2], skeleton->bone_transforms[i][0][3]);
        // Con_Printf("\t\t[%f, %f, %f, %f]\n", skeleton->bone_transforms[i][1][0], skeleton->bone_transforms[i][1][1], skeleton->bone_transforms[i][1][2], skeleton->bone_transforms[i][1][3]);
        // Con_Printf("\t\t[%f, %f, %f, %f]\n", skeleton->bone_transforms[i][2][0], skeleton->bone_transforms[i][2][1], skeleton->bone_transforms[i][2][2], skeleton->bone_transforms[i][2][3]);
        // Con_Printf("\t\t[%f, %f, %f, %f]\n", 0.0f, 0.0f, 0.0f, 1.0f);
        // }

        // If we have a parent, concat parent transform to get model-space transform
        int parent_bone_idx = bone_parent_idx[i];
        if(parent_bone_idx >= 0) {
            mat3x4_t temp; 
            Matrix3x4_ConcatTransforms(temp, skeleton->bone_transforms[parent_bone_idx], skeleton->bone_transforms[i]);
            Matrix3x4_Copy(skeleton->bone_transforms[i], temp);
        }
        // If we don't have a parent, the bone-space transform _is_ the model-space transform

        // if(i == 0 && skeleton->model->n_bones == 20) {
        // if(skeleton->model->n_bones > 20) {
        //     Con_Printf("\tBuild skel bone posed transform %d with parent concat:\n", i);
        //     Con_Printf("\t\t[%f, %f, %f, %f]\n", skeleton->bone_transforms[i][0][0], skeleton->bone_transforms[i][0][1], skeleton->bone_transforms[i][0][2], skeleton->bone_transforms[i][0][3]);
        //     Con_Printf("\t\t[%f, %f, %f, %f]\n", skeleton->bone_transforms[i][1][0], skeleton->bone_transforms[i][1][1], skeleton->bone_transforms[i][1][2], skeleton->bone_transforms[i][1][3]);
        //     Con_Printf("\t\t[%f, %f, %f, %f]\n", skeleton->bone_transforms[i][2][0], skeleton->bone_transforms[i][2][1], skeleton->bone_transforms[i][2][2], skeleton->bone_transforms[i][2][3]);
        //     Con_Printf("\t\t[%f, %f, %f, %f]\n", 0.0f, 0.0f, 0.0f, 1.0f);
        // }
    }

    // Now that all bone transforms have been computed for the current pose, multiply in the inverse rest pose transforms
    // These transforms will take the vertices from model-space to each bone's local-space:
    for(uint32_t i = 0; i < skeleton->model->n_bones; i++) {
        Matrix3x4_ConcatTransforms( skeleton->bone_rest_to_pose_transforms[i], skeleton->bone_transforms[i], inv_bone_rest_transforms[i]);


#ifdef IQM_LOAD_NORMALS
        // Invert-transpose the upper-left 3x3 matrix to get the transform that is applied to vertex normals
        // FIXME - Do we even need this? We make no user of this matrix on dquake
        Matrix3x3_Invert_Transposed_Matrix3x4(skeleton->bone_rest_to_pose_normal_transforms[i], skeleton->bone_rest_to_pose_transforms[i]);
#endif // IQM_LOAD_NORMALS


        // // if(i == 0 && skeleton->model->n_bones == 20) {
        // if(skeleton->model->n_bones > 20) {
        //     Con_Printf("\tBuild skel final bone transform %d:\n", i);
        //     Con_Printf("\t\t[%f, %f, %f, %f]\n", skeleton->bone_rest_to_pose_transforms[i][0][0], skeleton->bone_rest_to_pose_transforms[i][0][1], skeleton->bone_rest_to_pose_transforms[i][0][2], skeleton->bone_rest_to_pose_transforms[0][3]);
        //     Con_Printf("\t\t[%f, %f, %f, %f]\n", skeleton->bone_rest_to_pose_transforms[i][1][0], skeleton->bone_rest_to_pose_transforms[i][1][1], skeleton->bone_rest_to_pose_transforms[i][1][2], skeleton->bone_rest_to_pose_transforms[1][3]);
        //     Con_Printf("\t\t[%f, %f, %f, %f]\n", skeleton->bone_rest_to_pose_transforms[i][2][0], skeleton->bone_rest_to_pose_transforms[i][2][1], skeleton->bone_rest_to_pose_transforms[i][2][2], skeleton->bone_rest_to_pose_transforms[2][3]);
        //     Con_Printf("\t\t[%f, %f, %f, %f]\n", 0.0f, 0.0f, 0.0f, 1.0f);
        // }
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


void set_skeleton_bone_states_from_model(skeletal_skeleton_t *skeleton, skeletal_model_t *skel_model) {
    bool *model_bone_hitbox_enabled = UNPACK_MEMBER(skel_model->bone_hitbox_enabled, skel_model);
    vec3_t *model_bone_hitbox_ofs = UNPACK_MEMBER(skel_model->bone_hitbox_ofs, skel_model);
    vec3_t *model_bone_hitbox_scale = UNPACK_MEMBER(skel_model->bone_hitbox_scale, skel_model);
    int *model_bone_hitbox_tag = UNPACK_MEMBER(skel_model->bone_hitbox_tag, skel_model);

    if(model_bone_hitbox_enabled == NULL) {
        return;
    }
    if(model_bone_hitbox_ofs == NULL) {
        return;
    }
    if(model_bone_hitbox_scale == NULL) {
        return;
    }
    if(model_bone_hitbox_tag == NULL) {
        return;
    }

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
    if(skel_model == NULL || skel_model_idx < 0) {
        return;
    }
    if(skel_model->n_meshes <= 0) {
        // IQM Models with no meshes do not have valid skeleton rest pose data
        // and therefore cannot be used to construct a skeleton
        return;
    }

    skeleton->modelindex = skel_model_idx;
    skeleton->model = skel_model;
    skeleton->anim_model = NULL;
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



//
// Given an ent's skeleton state vars, updated the cl_skeletons entry for that ent.
// Returns a pointer to the ent's skeleton
// If the skeleton is missing or misconfigured, returns NULL, causing model to be drawn at rest pose
//
skeletal_skeleton_t *cl_update_skel_for_ent(entity_t *ent) {
    // Con_Printf("cl_update_skel_for_ent 1\n");
    int cl_skel_idx = ent->skeletonindex;
    if(cl_skel_idx < 0 || cl_skel_idx >= MAX_SKELETONS) {
        // Con_Printf("cl_update_skel_for_ent 1.1\n");
        return NULL;
    }
    // Con_Printf("cl_update_skel_for_ent 2\n");

    skeletal_skeleton_t *cl_skel = &(cl_skeletons[cl_skel_idx]);
    skeletal_model_t *ent_skel_model = (skeletal_model_t*)Mod_Extradata(ent->model);
    // Con_Printf("cl_update_skel_for_ent 3\n");
    skeletal_model_t *skeleton_skel_model = cl_get_skel_model_for_modelidx(ent->skeleton_modelindex);
    // Con_Printf("cl_update_skel_for_ent 4\n");
    if(skeleton_skel_model == NULL) {
        Con_Printf("Skeleton skel model is null (modelindex: %d). Returning cl_skel=NULL (n_bones: %d, time: %.2f)\n", ent->skeleton_modelindex, ent_skel_model->n_bones, sv.time);
        return NULL;
    }
    // Con_Printf("cl_update_skel_for_ent 5\n");
    skeletal_model_t *skeleton_anim_skel_model = cl_get_skel_model_for_modelidx(ent->skeleton_anim_modelindex);
    // Con_Printf("cl_update_skel_for_ent 6\n");
    if(skeleton_anim_skel_model == NULL) {
        Con_Printf("Skeleton anim skel model is null (modelindex: %d). Returning cl_skel=NULL (n_bones: %d, time: %.2f)\n", ent->skeleton_anim_modelindex, ent_skel_model->n_bones, sv.time);
        return NULL;
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



// 
// Given a JSON object representing a material, parses the JSON to fill in 
// the material values
//  
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"



void load_material_json_info(skeletal_material_t *material, cJSON *material_data, const char *json_file_name, const char *material_name) {
    // Defaults
    material->texture_idx = 0; // FIXME - Default to missing texture?
    material->transparent = false;
    material->fullbright = false;
    material->fog = true;
    material->shade_smooth = true;
    material->add_color = false;
    material->add_color_red = 0.0f;
    material->add_color_green = 0.0f;
    material->add_color_blue = 0.0f;
    material->add_color_alpha = 1.0f;

    if(material_data == NULL) {
        return;
    }
    if(!cJSON_IsObject(material_data)) {
        return;
    }

    cJSON *texture_name = cJSON_GetObjectItemCaseSensitive(material_data, "texture");
    if(cJSON_IsString(texture_name)) {
        char *texture_file = texture_name->valuestring;
        material->texture_idx = load_material_texture_image(texture_file);
    }

    cJSON *is_transparent = cJSON_GetObjectItemCaseSensitive(material_data, "transparent");
    if(cJSON_IsBool(is_transparent)) {
        material->transparent = cJSON_IsTrue(is_transparent);
    }

    cJSON *is_fullbright = cJSON_GetObjectItemCaseSensitive(material_data, "fullbright");
    if(cJSON_IsBool(is_fullbright)) {
        material->fullbright = cJSON_IsTrue(is_fullbright);
    }

    cJSON *uses_fog = cJSON_GetObjectItemCaseSensitive(material_data, "fog");
    if(cJSON_IsBool(uses_fog)) {
        material->fog = cJSON_IsTrue(uses_fog);
    }

    cJSON *shade_smooth = cJSON_GetObjectItemCaseSensitive(material_data, "shade_smooth");
    if(cJSON_IsBool(shade_smooth)) {
        material->shade_smooth = cJSON_IsTrue(shade_smooth);
    }

    cJSON *add_color = cJSON_GetObjectItemCaseSensitive(material_data, "add_color");
    if(cJSON_IsArray(add_color)) {
        int add_color_array_len = cJSON_GetArraySize(add_color);
        bool numbers_valid = true;
        for(int i = 0; i < add_color_array_len; i++) {
            cJSON *number = cJSON_GetArrayItem(add_color, i);
            if(!cJSON_IsNumber(number)) {
                Con_Printf("Warning - IQM JSON file \"%s\" material \"%s\" has \"add_color\" array index %d entry is non-numeric\n", json_file_name, material_name, i);
                numbers_valid = false;
                break;
            }
        }
        if(numbers_valid) {
            if(add_color_array_len == 3) {
                material->add_color = true;
                material->add_color_red   = (float) cJSON_GetNumberValue(cJSON_GetArrayItem( add_color, 0));
                material->add_color_green = (float) cJSON_GetNumberValue(cJSON_GetArrayItem( add_color, 1));
                material->add_color_blue  = (float) cJSON_GetNumberValue(cJSON_GetArrayItem( add_color, 2));
            }
            else if(add_color_array_len == 4) {
                material->add_color = true;
                material->add_color_red   = (float) cJSON_GetNumberValue(cJSON_GetArrayItem( add_color, 0));
                material->add_color_green = (float) cJSON_GetNumberValue(cJSON_GetArrayItem( add_color, 1));
                material->add_color_blue  = (float) cJSON_GetNumberValue(cJSON_GetArrayItem( add_color, 2));
                material->add_color_alpha = (float) cJSON_GetNumberValue(cJSON_GetArrayItem( add_color, 3));
            }
            else {
                Con_Printf("Warning - IQM JSON file \"%s\" material \"%s\" \"add_color\" array has length %d, but should be 3 (for RGB) or 4 (for RGBA)\n", json_file_name, material_name, cJSON_GetArraySize(add_color));
            }
        }
    }
}


//
// Given a skel_model, loads it accompanying optional JSON file
// NOTE: Assumes skel_model is unpacked
//
void load_iqm_file_json_info(skeletal_model_t *skel_model, const char *iqm_file_path) {
    char iqm_info_json_file[256];
    sprintf(iqm_info_json_file, "%s.json", iqm_file_path);
    Con_Printf("Loading IQM model info JSON file: \"%s\"\n", iqm_info_json_file);

    const char *iqm_info_json_str = (const char*) COM_LoadFile(iqm_info_json_file, 0);
    if(iqm_info_json_str == NULL) {
        return;
    }
    Con_Printf("Loaded JSON file contents.\n");

    // char *iqm_info_clean_json_str = malloc(strlen(iqm_info_json_str) + 1);
    // Con_Printf("Cleaned JSON file contents.\n");
    // if(iqm_info_clean_json_str == NULL) {
    //     return;
    // }
    cJSON_Minify(iqm_info_json_str);

    cJSON *root = cJSON_Parse(iqm_info_json_str);
    if(root == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if(error_ptr != NULL) {
            Con_Printf("JSON error: \"%s\"\n", error_ptr);
        }
        return;
    }
    Con_Printf("JSON parse success!\n");


    // Parse bone_hitbox tag
    char **bone_names = skel_model->bone_name;
    if(skel_model->bone_hitbox_tag != NULL) {
        cJSON *bone_hitbox_tag = cJSON_GetObjectItemCaseSensitive(root, "bone_hitbox_tag");
        if(cJSON_IsObject(bone_hitbox_tag)) {
            for(int i = 0; i < skel_model->n_bones; i++) {
                cJSON *bone_data = cJSON_GetObjectItemCaseSensitive(bone_hitbox_tag, bone_names[i]);
                if(bone_data) {
                    if(cJSON_IsNumber(bone_data)) {
                        skel_model->bone_hitbox_tag[i] = (int) cJSON_GetNumberValue(bone_data);
                    }
                    else {
                        Con_Printf("Warning - IQM JSON file \"%s\" has non-integer \"bone_hitbox_tag\" value for bone \"%s\"\n", iqm_info_json_file, bone_names[i]);
                    }
                }
            }
        }
    }

    // Parse bone_hitbox_enabled
    if(skel_model->bone_hitbox_enabled != NULL) {
        cJSON *bone_hitbox_enabled = cJSON_GetObjectItemCaseSensitive(root, "bone_hitbox_enabled");
        if(cJSON_IsObject(bone_hitbox_enabled)) {
            for(int i = 0; i < skel_model->n_bones; i++) {
                cJSON *bone_data = cJSON_GetObjectItemCaseSensitive(bone_hitbox_enabled, bone_names[i]);
                if(bone_data) {
                    if(cJSON_IsBool(bone_data)) {
                        skel_model->bone_hitbox_enabled[i] = cJSON_IsTrue(bone_data);
                    }
                    else {
                        Con_Printf("Warning - IQM JSON file \"%s\" has non-boolean \"bone_hitbox_enabled\" value for bone \"%s\"\n", iqm_info_json_file, bone_names[i]);
                    }
                }
            }
        }
    }

    // Parse bone_hitbox_ofs
    if(skel_model->bone_hitbox_ofs != NULL) {
        cJSON *bone_hitbox_ofs = cJSON_GetObjectItemCaseSensitive(root, "bone_hitbox_ofs");
        if(cJSON_IsObject(bone_hitbox_ofs)) {
            for(int i = 0; i < skel_model->n_bones; i++) {
                cJSON *bone_data = cJSON_GetObjectItemCaseSensitive(bone_hitbox_ofs, bone_names[i]);
                if(bone_data) {
                    if(!cJSON_IsArray(bone_data)) {
                        Con_Printf("Warning - IQM JSON file \"%s\" has non-array \"bone_hitbox_ofs\" value for bone \"%s\"\n", iqm_info_json_file, bone_names[i]);
                        continue;
                    }
                    else if(cJSON_GetArraySize(bone_data) != 3) {
                        Con_Printf("Warning - IQM JSON file \"%s\" has \"bone_hitbox_ofs\" array of size != 3 for bone \"%s\"\n", iqm_info_json_file, bone_names[i]);
                        continue;
                    }
                    bool numbers_valid = true;
                    for(int j = 0; j < 3; j++) {
                        cJSON *number = cJSON_GetArrayItem(bone_data, j);
                        if(!cJSON_IsNumber(number)) {
                            Con_Printf("Warning - IQM JSON file \"%s\" has \"bone_hitbox_ofs\" array index %d entry is non-numeric for bone \"%s\"\n", iqm_info_json_file, j, bone_names[i]);
                            numbers_valid = false;
                            break;
                        }
                    }
                    if(!numbers_valid) {
                        continue;
                    }
                    for(int j = 0; j < 3; j++) {
                        cJSON *number = cJSON_GetArrayItem(bone_data, j);
                        skel_model->bone_hitbox_ofs[i][j] = (float) cJSON_GetNumberValue(number);
                    }
                }
            }
        }
    }

    // Parse bone_hitbox_scale
    if(skel_model->bone_hitbox_scale != NULL) {
        cJSON *bone_hitbox_scale = cJSON_GetObjectItemCaseSensitive(root, "bone_hitbox_scale");
        if(cJSON_IsObject(bone_hitbox_scale)) {
            for(int i = 0; i < skel_model->n_bones; i++) {
                cJSON *bone_data = cJSON_GetObjectItemCaseSensitive(bone_hitbox_scale, bone_names[i]);
                if(bone_data) {
                    if(!cJSON_IsArray(bone_data)) {
                        Con_Printf("Warning - IQM JSON file \"%s\" has non-array \"bone_hitbox_scale\" value for bone \"%s\"\n", iqm_info_json_file, bone_names[i]);
                        continue;
                    }
                    else if(cJSON_GetArraySize(bone_data) != 3) {
                        Con_Printf("Warning - IQM JSON file \"%s\" has \"bone_hitbox_scale\" array of size != 3 for bone \"%s\"\n", iqm_info_json_file, bone_names[i]);
                        continue;
                    }
                    bool numbers_valid = true;
                    for(int j = 0; j < 3; j++) {
                        cJSON *number = cJSON_GetArrayItem(bone_data, j);
                        if(!cJSON_IsNumber(number)) {
                            Con_Printf("Warning - IQM JSON file \"%s\" has \"bone_hitbox_scale\" array index %d entry is non-numeric for bone \"%s\"\n", iqm_info_json_file, j, bone_names[i]);
                            numbers_valid = false;
                            break;
                        }
                    }
                    if(!numbers_valid) {
                        continue;
                    }
                    for(int j = 0; j < 3; j++) {
                        cJSON *number = cJSON_GetArrayItem(bone_data, j);
                        skel_model->bone_hitbox_scale[i][j] = (float) cJSON_GetNumberValue(number);
                    }
                }
            }
        }
    }


    // Parse materials
    cJSON *materials = cJSON_GetObjectItemCaseSensitive(root, "materials");
    if(cJSON_IsObject(materials)) {
        // "To get the size of an object... use cJSON_GetArraySize... because internally objects are stored as arrays." 
        // - https://github.com/DaveGamble/cJSON/blob/master/README.md
        int n_materials = cJSON_GetArraySize(materials);
        Con_Printf("Found %d materials\n", n_materials);
        skel_model->n_materials = n_materials;
        skel_model->material_names = (char**) malloc(sizeof(char*) * n_materials);
        skel_model->material_n_skins = (int*) malloc(sizeof(int) * n_materials);
        skel_model->materials = (skeletal_material_t**) malloc(sizeof(skeletal_material_t*) * n_materials);

        for(int material_idx = 0; material_idx < n_materials; material_idx++) {
            // Treating JSON object "materials" as an array, for same reason as above
            cJSON *material_data = cJSON_GetArrayItem(materials, material_idx);
            const char *material_name = material_data->string;
            skel_model->material_names[material_idx] = strdup(material_name);
            Con_Printf("Material iter key: \"%s\"\n", material_name);
            // If array of skins, parse each skin material
            if(cJSON_IsArray(material_data)) {
                int n_skins = cJSON_GetArraySize(material_data);
                skel_model->material_n_skins[material_idx] = n_skins;
                skel_model->materials[material_idx] = (skeletal_material_t*) malloc(sizeof(skeletal_material_t) * n_skins);
                for(int skin_idx = 0; skin_idx < n_skins; skin_idx++) {
                    cJSON *skin_material_data = cJSON_GetArrayItem(material_data, skin_idx);
                    load_material_json_info(&(skel_model->materials[material_idx][skin_idx]), skin_material_data, iqm_info_json_file, material_name);
                }
            }
            // If Object, parse single material
            else if(cJSON_IsObject(material_data)) {
                int n_skins = 1;
                skel_model->material_n_skins[material_idx] = n_skins;
                skel_model->materials[material_idx] = (skeletal_material_t*) malloc(sizeof(skeletal_material_t) * n_skins);
                load_material_json_info(&(skel_model->materials[material_idx][0]), material_data, iqm_info_json_file, material_name);
            }
            // Otherwise, load single error material for this material name
            else {
                int n_skins = 1;
                skel_model->material_n_skins[material_idx] = n_skins;
                skel_model->materials[material_idx] = (skeletal_material_t*) malloc(sizeof(skeletal_material_t) * n_skins);
                load_material_json_info(&(skel_model->materials[material_idx][0]), NULL, iqm_info_json_file, material_name);

            }
        }
    }

    // Load per-framegroup per-frame movement distance and calculate cumulative sum
    if(skel_model->n_framegroups > 0) {
        // NOTE - skel_model->frames_move_dist has already been heap-alloced to skel_model->n_frames and filled with 0s

        cJSON *framegroup_move_dists_json_obj = cJSON_GetObjectItemCaseSensitive(root, "framegroup_movement_distances");
        if(cJSON_IsObject(framegroup_move_dists_json_obj)) {

            // Loop through model's animation framegroups
            for(int framegroup_idx = 0; framegroup_idx < skel_model->n_framegroups; framegroup_idx++) {

                // If movement dist info specified for this framegroup's name in the JSON, load it
                const char *framegroup_name = skel_model->framegroup_name[framegroup_idx];
                cJSON *anim_framegroup_move_dists_json_obj = cJSON_GetObjectItemCaseSensitive(framegroup_move_dists_json_obj, framegroup_name);
                if(cJSON_IsArray(anim_framegroup_move_dists_json_obj)) {

                    int n_frames = cJSON_GetArraySize(anim_framegroup_move_dists_json_obj);

                    if(n_frames != skel_model->framegroup_n_frames[framegroup_idx]) {
                        Con_Printf("Warning - IQM JSON file \"%s\" field \"framegroup_movement_distances\" for anim \"%s\" specifies %d frames, but this anim in the IQM file has %d frames.\n", iqm_info_json_file, framegroup_name, n_frames, skel_model->framegroup_n_frames[framegroup_idx]);
                        // Just warn and continue parsing
                    }

                    // Only read data up to frames that the anim actually has
                    if(n_frames > skel_model->framegroup_n_frames[framegroup_idx]) {
                        n_frames = skel_model->framegroup_n_frames[framegroup_idx];
                    }

                    for(int framegroup_frame_idx = 0; framegroup_frame_idx < n_frames; framegroup_frame_idx++) {
                        cJSON *framegroup_frame_move_dist = cJSON_GetArrayItem(anim_framegroup_move_dists_json_obj, framegroup_frame_idx);
                        if(!cJSON_IsNumber(framegroup_frame_move_dist)) {
                            Con_Printf("Warning - IQM JSON file \"%s\" field \"framegroup_movement_distances\" for anim \"%s\" entry is non-numeric for framegroup frame \"%d\"\n", iqm_info_json_file, framegroup_name, framegroup_frame_idx);
                            continue;
                        }

                        // Get global frame index (in interleaved frames array for all framegroups)
                        int frame_idx = skel_model->framegroup_start_frame[framegroup_idx] + framegroup_frame_idx;
                        // Store raw value at frame for now, we'll aggregate later
                        skel_model->frames_move_dist[frame_idx] = (float) cJSON_GetNumberValue(framegroup_frame_move_dist);
                    }
                }

                // Calculate inclusive cumulative sum for this framegroup's frames
                int start_frame_idx = skel_model->framegroup_start_frame[framegroup_idx];
                int end_frame_idx = start_frame_idx + skel_model->framegroup_n_frames[framegroup_idx];
                for(int frame_idx = start_frame_idx + 1; frame_idx < end_frame_idx; frame_idx++) {
                    skel_model->frames_move_dist[frame_idx] += skel_model->frames_move_dist[frame_idx - 1];
                    Con_Printf("Walkdist framegroup %d frame %d cumsumdist: %f\n", framegroup_idx, frame_idx, skel_model->frames_move_dist[frame_idx]);
                }
            }
        }
    }

    Con_Printf("Deleting cJSON object...\n");
    cJSON_Delete(root);
    Con_Printf("Done Deleting cJSON object...\n");
}

// 
// Calculate collision of ray with unit-size AABB (Centered at origin, size: 1.0)
// 
// https://tavianator.com/cgit/dimension.git/tree/libdimension/bvh/bvh.c#n196
//
bool ray_aabb_intersection(vec3_t ray_pos, vec3_t ray_dir, float *tmin) {
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
    *tmin = fmin(tx1, tx2);
    tmax = fmax(tx1, tx2);

    float ty1 = (box_mins[1] - ray_pos[1]) * inv_ray_dir[1];
    float ty2 = (box_maxs[1] - ray_pos[1]) * inv_ray_dir[1];
    *tmin = fmax(*tmin, fmin(ty1, ty2));
    tmax = fmin(tmax, fmax(ty1, ty2));

    float tz1 = (box_mins[2] - ray_pos[2]) * inv_ray_dir[2];
    float tz2 = (box_maxs[2] - ray_pos[2]) * inv_ray_dir[2];
    *tmin = fmax(*tmin, fmin(tz1, tz2));
    tmax = fmin(tmax, fmax(tz1, tz2));

    // return tmax >= fmax(0.0f, tmin);
    return tmax >= fmax(0.0f, *tmin);
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

    skeletal_model_t *skel_model = (skeletal_model_t*)Mod_Extradata(view_ent->model);
    // Con_Printf("cl_get_camera_bone_view_transform 3\n");

    // If not a skeletal model, don't update
    if(skel_model == NULL) {
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
    if(cl_skel == NULL) {
        // Con_Printf("cl_get_camera_bone_view_transform 6.1\n");
        return 0;
    }
    // Con_Printf("cl_get_camera_bone_view_transform 7\n");

    // mat3x4_t camera_anim_pose_transform;
    // mat3x4_t camera_anim_rest_to_pose_transform;
    // // mat3x4_t camera_anim_transform;
    // Matrix3x4_rotate_translate(camera_anim_pose_transform, cl_skel->fps_cam_bone_rot, cl_skel->fps_cam_bone_pos);
    // mat3x4_t *inv_bone_rest_transforms = UNPACK_MEMBER(skel_model->inv_bone_rest_transforms, skel_model);
    // Matrix3x4_ConcatTransforms( camera_anim_rest_to_pose_transform, camera_anim_pose_transform, inv_bone_rest_transforms[skel_model->fps_cam_bone_idx]);
    // // Invert the camera matrix to get the view matrix
    // Matrix3x4_Invert_Affine(camera_anim_view_matrix_out, camera_anim_rest_to_pose_transform);


    // Con_Printf("---------------------------------------------------------\n");
    // mat3x4_t *inv_bone_rest_transforms = UNPACK_MEMBER(skel_model->inv_bone_rest_transforms, skel_model);

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



//
// Returns the skeletal model with index `model_idx`
// Returns NULL if invalid index or if not a skeletal model.
//
skeletal_model_t *sv_get_skel_model_for_modelidx(int model_idx) {
    if(model_idx < 0 || model_idx >= MAX_MODELS) {
        Con_DPrintf("Model idx out of bounds: %d, return -1\n", model_idx);
        return NULL;
    }

    // Safety check to make sure model at index is valid
    // char *model_name = sv.model_precache[model_idx];
    // model_t *model = Mod_ForName(model_name, false);

    // No safety check
    model_t *model = sv.models[model_idx];

    if(model->type != mod_iqm) {
        Con_DPrintf("Model type is not IQM: model: %d, type: %d != %d, return -1\n", model_idx, model->type, mod_iqm);
        return NULL;
    }
    skeletal_model_t *skel_model = (skeletal_model_t*)Mod_Extradata(model);
    return skel_model;
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
int16_t *skel_model_bounds_idxs[MAX_MODELS] = {NULL};
int16_t skel_model_bounds_count[MAX_MODELS] = {0};

vec3_t *bounds_mins = NULL;
vec3_t *bounds_maxs = NULL;
uint16_t bounds_count = 0;
uint16_t bounds_capacity = 0;


 
void calc_skel_model_bounds_for_rest_pose(skeletal_model_t *skel_model, vec3_t model_mins, vec3_t model_maxs) {


#ifdef IQMDEBUG_CALC_MODEL_BOUNDS
    Con_Printf("calc_skel_model_bounds_for_rest_pose started\n");
#endif // IQMDEBUG_CALC_MODEL_BOUNDS

    if(skel_model == NULL) {
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
    mat3x4_t *bone_rest_transforms = UNPACK_MEMBER(skel_model->bone_rest_transforms, skel_model);
    if(bone_rest_transforms == NULL) {
        return;
    }

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
    vec3_t *bone_hitbox_ofs = UNPACK_MEMBER(skel_model->bone_hitbox_ofs, skel_model);
    vec3_t *bone_hitbox_scale = UNPACK_MEMBER(skel_model->bone_hitbox_scale, skel_model);
    if(bone_hitbox_ofs == NULL || bone_hitbox_scale == NULL) {
        return;
    }

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
        char **bone_names = UNPACK_MEMBER(skel_model->bone_name, skel_model);
        char *bone_name = UNPACK_MEMBER(bone_names[bone_idx], skel_model);

        Con_Printf("Checking rest pose bone mins / maxs for bone %d \"%s\"\n", bone_idx, bone_name);
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

    if(skel_model == NULL || anim_model == NULL) {
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


    mat3x4_t *bone_rest_transforms = UNPACK_MEMBER(skel_model->bone_rest_transforms, skel_model);
    int16_t *bone_parent_idx = UNPACK_MEMBER(skel_model->bone_parent_idx, skel_model);
    vec3_t *anim_model_frames_bone_pos = UNPACK_MEMBER(anim_model->frames_bone_pos, anim_model);
    quat_t *anim_model_frames_bone_rot = UNPACK_MEMBER(anim_model->frames_bone_rot, anim_model);
    vec3_t *anim_model_frames_bone_scale = UNPACK_MEMBER(anim_model->frames_bone_scale, anim_model);
    uint32_t *framegroup_start_frame = UNPACK_MEMBER(anim_model->framegroup_start_frame, anim_model);
    uint32_t *framegroup_n_frames = UNPACK_MEMBER(anim_model->framegroup_n_frames, anim_model);

    if(bone_rest_transforms == NULL) {
        return;
    }


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
    vec3_t *bone_hitbox_ofs = UNPACK_MEMBER(skel_model->bone_hitbox_ofs, skel_model);
    vec3_t *bone_hitbox_scale = UNPACK_MEMBER(skel_model->bone_hitbox_scale, skel_model);
    if(bone_hitbox_ofs == NULL || bone_hitbox_scale == NULL) {
        return;
    }


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
//      ... model rest pose if anim_model == NULL
//      ... across all animation frames specified by anim_model otherwise
// 
// Returns the hitbox mins / maxs as `model_mins`, `model_maxs1
// 
void calc_skel_model_bounds(skeletal_model_t *skel_model, skeletal_model_t *anim_model, vec3_t model_mins, vec3_t model_maxs) {
    if(anim_model == NULL) {
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
    if(skel_model == NULL) {
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
    if(skel_anim == NULL) {
        // Con_Printf("cl_get_skel_model_bounds -- anim is null!\n");
        VectorCopy(skel_model->rest_pose_bone_hitbox_mins, mins);
        VectorCopy(skel_model->rest_pose_bone_hitbox_maxs, maxs);
        return;
    }
    _get_skel_model_bounds(skel_model, skel_anim, model_idx, animmodel_idx, mins, maxs);
}


void sv_get_skel_model_bounds(int model_idx, int animmodel_idx, vec3_t mins, vec3_t maxs) {
    skeletal_model_t *skel_model = sv_get_skel_model_for_modelidx(model_idx);
    if(skel_model == NULL) {
        return;
    }
    if(model_idx == animmodel_idx) {
        VectorCopy(skel_model->anim_bone_hitbox_mins, mins);
        VectorCopy(skel_model->anim_bone_hitbox_maxs, maxs);
        return;
    }
    skeletal_model_t *skel_anim = sv_get_skel_model_for_modelidx(animmodel_idx);
    if(skel_anim == NULL) {
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
    if(skel_model == NULL) {
        return;
    }
    if(skel_anim == NULL) {
        return;
    }

    // If this animation doesn't fit in the current array, reallocate to make it big enough
    if(animmodel_idx >= skel_model_bounds_count[model_idx]) {
        int old_count = (skel_model_bounds_idxs[model_idx] == NULL) ? 0 : skel_model_bounds_count[model_idx];
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
    // vec3_t bbox_mins = {-16, -16, -36};
    // vec3_t bbox_maxs = { 16,  16,  36};

    // ... 2x HLBSP player hull (Option 2)
    vec3_t bbox_mins = {-32, -32, -72};
    vec3_t bbox_maxs = { 32,  32,  72};

    VectorCopy( bbox_mins, mins);
    VectorCopy( bbox_maxs, maxs);
}


void cl_get_skel_model_bounds(int model_idx, int animmodel_idx, vec3_t mins, vec3_t maxs) {
    // Dynamic bbox calculations are disabled, return static size for all skeletal models
    _get_skel_model_bounds(mins, maxs);
}


void sv_get_skel_model_bounds(int model_idx, int animmodel_idx, vec3_t mins, vec3_t maxs) {
    // Dynamic bbox calculations are disabled, return static size for all skeletal models
    _get_skel_model_bounds(mins, maxs);
}

#endif // IQM_BBOX_PER_MODEL_PER_ANIM
// ============================================================================

		
//
// Completely deallocates a skeletal_model_t struct, pointers and all.
// NOTE - This should only be used on unpacked (heap-allocated) skeletal models 
// NOTE   whose pointers are absolute (not-relative to model-start)
//
void free_unpacked_skeletal_model(skeletal_model_t *skel_model) {
    // Free fields in reverse order to avoid losing references

    // -- FTE Anim Events --
    if(skel_model->framegroup_n_events != NULL) {
        for(int i = 0; i < skel_model->n_framegroups; i++) {
            for(int j = 0; j < skel_model->framegroup_n_events[i]; j++) {
                free_pointer_and_clear( (void**) &skel_model->framegroup_event_data_str[i][j]);
            }
            free_pointer_and_clear( (void**) &skel_model->framegroup_event_data_str[i]);
            free_pointer_and_clear( (void**) &skel_model->framegroup_event_code[i]);
            free_pointer_and_clear( (void**) &skel_model->framegroup_event_time[i]);
        }
    }
    free_pointer_and_clear( (void**) &skel_model->framegroup_event_data_str);
    free_pointer_and_clear( (void**) &skel_model->framegroup_event_code);
    free_pointer_and_clear( (void**) &skel_model->framegroup_event_time);
    free_pointer_and_clear( (void**) &skel_model->framegroup_n_events);

    // -- materials --
    for(int i = 0; i < skel_model->n_materials; i++) {
        free_pointer_and_clear( (void**) &skel_model->materials[i]);
        free_pointer_and_clear( (void**) &skel_model->material_names[i]);
    }
    free_pointer_and_clear( (void**) &skel_model->materials);
    free_pointer_and_clear( (void**) &skel_model->material_n_skins);
    free_pointer_and_clear( (void**) &skel_model->material_names);

    // -- framegroups --
    for(int i = 0; i < skel_model->n_framegroups; i++) {
        free_pointer_and_clear( (void**) &skel_model->framegroup_name[i]);
    }
    free_pointer_and_clear( (void**) &skel_model->framegroup_loop);
    free_pointer_and_clear( (void**) &skel_model->framegroup_fps);
    free_pointer_and_clear( (void**) &skel_model->framegroup_n_frames);
    free_pointer_and_clear( (void**) &skel_model->framegroup_start_frame);
    free_pointer_and_clear( (void**) &skel_model->framegroup_name);

    // -- frames --
    free_pointer_and_clear( (void**) &skel_model->frames_bone_scale);
    free_pointer_and_clear( (void**) &skel_model->frames_bone_rot);
    free_pointer_and_clear( (void**) &skel_model->frames_bone_pos);
    free_pointer_and_clear( (void**) &skel_model->frames_move_dist);

    // -- cached bone rest transforms --
    free_pointer_and_clear( (void**) &skel_model->inv_bone_rest_transforms);
    free_pointer_and_clear( (void**) &skel_model->bone_rest_transforms);

    // -- bone hitbox info --
    free_pointer_and_clear( (void**) &skel_model->bone_hitbox_tag);
    free_pointer_and_clear( (void**) &skel_model->bone_hitbox_scale);
    free_pointer_and_clear( (void**) &skel_model->bone_hitbox_ofs);
    free_pointer_and_clear( (void**) &skel_model->bone_hitbox_enabled);

    // -- bones --
    for(int i = 0; i < skel_model->n_bones; i++) {
        free_pointer_and_clear( (void**) &skel_model->bone_name[i]);
    }
    free_pointer_and_clear( (void**) &skel_model->bone_name);
    free_pointer_and_clear( (void**) &skel_model->bone_parent_idx);
    free_pointer_and_clear( (void**) &skel_model->bone_rest_pos);
    free_pointer_and_clear( (void**) &skel_model->bone_rest_rot);
    free_pointer_and_clear( (void**) &skel_model->bone_rest_scale);

    free_unpacked_skeletal_model_meshes(skel_model);
    free(skel_model);
}





//
// Given iqm file header, returns struct containing vertex arrays, converted to expected types
//
// CAUTION: This allocates heap memory. Wherever this is invoked, make sure you call
// `free_iqm_model_vertex_arrays` to free the allocated heap memory
//
const iqm_model_vertex_arrays_t load_iqm_model_vertex_arrays(const iqm_header_t *iqm_header) {

    // Initialize to null values
    iqm_model_vertex_arrays_t iqm_vertex_arrays;
    iqm_vertex_arrays.verts_uv = NULL;
    iqm_vertex_arrays.verts_pos = NULL;
    iqm_vertex_arrays.verts_bone_weights = NULL;
    iqm_vertex_arrays.verts_bone_idxs = NULL;
#ifdef IQM_LOAD_NORMALS
    iqm_vertex_arrays.verts_nor = NULL;
#endif // IQM_LOAD_NORMALS
    // iqm_vertex_arrays.verts_tan = NULL;
    // iqm_vertex_arrays.verts_color = NULL;

    if(memcmp(iqm_header->magic, IQM_MAGIC, sizeof(iqm_header->magic))) {
        return iqm_vertex_arrays; 
    }
    if(iqm_header->version != IQM_VERSION_2) {
        return iqm_vertex_arrays;
    }

    // We won't know what the `iqm_data` buffer length is, but this is a useful check
    // if(iqm_header->filesize != file_len) {
    //     return iqm_model_null;
    // }
    size_t file_len = iqm_header->filesize;


    // ------------------------------------------------------------------------
    // Get points to the raw vertex data arrays in the IQM data
    // ------------------------------------------------------------------------
    // Pointers to vert arrays as stored in IQM data
    iqm_vert_array_t *raw_verts_pos = NULL;
    iqm_vert_array_t *raw_verts_uv = NULL;
    iqm_vert_array_t *raw_verts_nor = NULL;
    iqm_vert_array_t *raw_verts_tan = NULL;
    iqm_vert_array_t *raw_verts_color = NULL;
    iqm_vert_array_t *raw_verts_bone_idxs = NULL;
    iqm_vert_array_t *raw_verts_bone_weights = NULL;

#ifdef IQMDEBUG_LOADIQM_LOADMESH
    Con_Printf("load_iqm_model_vertex_arrays -- Parsing vert attribute arrays\n");
#endif // IQMDEBUG_LOADIQM_LOADMESH

    const iqm_vert_array_t *vert_arrays = (const iqm_vert_array_t*)((uint8_t*) iqm_header + iqm_header->ofs_vert_arrays);
    for(unsigned int i = 0; i < iqm_header->n_vert_arrays; i++) {
        if((iqm_vert_array_type) vert_arrays[i].type == IQM_VERT_POS) {
            raw_verts_pos = &vert_arrays[i];
        }
        else if((iqm_vert_array_type) vert_arrays[i].type == IQM_VERT_UV) {
            raw_verts_uv = &vert_arrays[i];
        }
        else if((iqm_vert_array_type) vert_arrays[i].type == IQM_VERT_NOR) {
            raw_verts_nor = &vert_arrays[i];
        }
        else if((iqm_vert_array_type) vert_arrays[i].type == IQM_VERT_TAN) {
            // Only use tangents if float and if each tangent is a 4D vector:
            if((iqm_dtype) vert_arrays[i].format == IQM_DTYPE_FLOAT && vert_arrays[i].size == 4) {
                raw_verts_tan = &vert_arrays[i];
            }
            else {
                Con_Printf("Warning: IQM vertex tangent array (idx: %d, type: %d, fmt: %d, size: %d) is not 4D float array.\n", i, vert_arrays[i].type, vert_arrays[i].format, vert_arrays[i].size);
            }
        }
        else if((iqm_vert_array_type) vert_arrays[i].type == IQM_VERT_COLOR) {
            raw_verts_color = &vert_arrays[i];
        }
        else if((iqm_vert_array_type) vert_arrays[i].type == IQM_VERT_BONE_IDXS) {
            raw_verts_bone_idxs = &vert_arrays[i];
        }
        else if((iqm_vert_array_type) vert_arrays[i].type == IQM_VERT_BONE_WEIGHTS) {
            raw_verts_bone_weights = &vert_arrays[i];
        }
        else {
            Con_Printf("Warning: Unrecognized IQM vertex array type (idx: %d, type: %d, fmt: %d, size: %d)\n", i, vert_arrays[i].type, vert_arrays[i].format, vert_arrays[i].size);
        }
    }
    // ------------------------------------------------------------------------

    // ------------------------------------------------------------------------
    // Check for minimum set of required fields for IQM models that have meshes:
    // If this model has at least one mesh, require vertex data for the following fields:
    //  - Vertex positions
    //  - Vertex UV coords
    //  - Vertex normals (If loading normals)
    //  - Vertex bone weights
    //  - Vertex bone indices
    // ------------------------------------------------------------------------
    if(iqm_header->n_meshes > 0) {
        if(raw_verts_pos == NULL) {
            Con_Printf("Error: IQM file model has meshes but no vertex positions data array.\n");
            return iqm_vertex_arrays;
        }
        if(raw_verts_uv == NULL) {
            Con_Printf("Error: IQM file model has meshes but no vertex UV data array.\n");
            return iqm_vertex_arrays;
        }
        // TODO - We could allow for support of IQM models without 
        // TODO   normals / bone weights / bone indices, but at present all of 
        // TODO   the IQM code assumes we have them. 
#ifdef IQM_LOAD_NORMALS
        if(raw_verts_nor == NULL) {
            Con_Printf("Error: IQM file model has meshes but no vertex normal data array.\n");
            return iqm_vertex_arrays;
        }
#endif // IQM_LOAD_NORMALS

        if(raw_verts_bone_weights == NULL) {
            Con_Printf("Error: IQM file model has meshes but no vertex bone weights data array.\n");
            return iqm_vertex_arrays;
        }
        if(raw_verts_bone_idxs == NULL) {
            Con_Printf("Error: IQM file model has meshes but no vertex bone indices data array.\n");
            return iqm_vertex_arrays;
        }
    }
    else {
        // If no meshes, return empty struct
        return iqm_vertex_arrays;
    }
    // ------------------------------------------------------------------------

    // ------------------------------------------------------------------------
    // Convert vertex data arrays to expected types
    // ------------------------------------------------------------------------
    iqm_vertex_arrays.verts_pos             = (vec3_t*)   malloc(sizeof(vec3_t) * iqm_header->n_verts);
    iqm_vertex_arrays.verts_uv              = (vec2_t*)   malloc(sizeof(vec2_t) * iqm_header->n_verts);
    iqm_vertex_arrays.verts_bone_weights    = (float*)    malloc(sizeof(float) * IQM_VERT_BONES * iqm_header->n_verts);
    iqm_vertex_arrays.verts_bone_idxs       = (uint8_t*)  malloc(sizeof(uint8_t) * IQM_VERT_BONES * iqm_header->n_verts);
#ifdef IQM_LOAD_NORMALS
    iqm_vertex_arrays.verts_nor             = (vec3_t*)   malloc(sizeof(vec3_t) * iqm_header->n_verts);
#endif // IQM_LOAD_NORMALS
    // iqm_vertex_arrays.verts_tan             = (vec4_t*)   malloc(sizeof(vec4_t) * iqm_header->n_verts);;
    // iqm_vertex_arrays.verts_color           = (vec4_t*)   malloc(sizeof(vec4_t) * iqm_header->n_verts);;

    vec3_t default_vert = {0,0,0};
    vec2_t default_uv = {0,0};
    float default_bone_weights[] = {0.0f, 0.0f, 0.0f, 0.0f};
    uint8_t max_bone_idx = (uint8_t) fmin((int) iqm_header->n_joints, (int) IQM_MAX_BONES) - 1;
#ifdef IQM_LOAD_NORMALS
    vec3_t default_nor = {0,0,1.0f};
#endif // IQM_LOAD_NORMALS
    // vec4_t default_tan = {0.0f, 1.0f, 0.0f, 0.0f};
    // vec4_t default_color = {1.0f, 1.0f, 1.0f, 1.0f};
    

#ifdef IQMDEBUG_LOADIQM_LOADMESH
    Con_Printf("load_iqm_model_vertex_arrays -- Parsing vertex attribute arrays\n");
    Con_Printf("\t\tVerts pos: %d\n", raw_verts_pos);
    Con_Printf("\t\tVerts uv: %d\n", raw_verts_uv);
    Con_Printf("\t\tVerts nor: %d\n", raw_verts_nor);
    Con_Printf("\t\tVerts bone weights: %d\n", raw_verts_bone_weights);
    Con_Printf("\t\tVerts bone idxs: %d\n", raw_verts_bone_idxs);
#endif // IQMDEBUG_LOADIQM_LOADMESH


    iqm_parse_float_array(iqm_header, raw_verts_pos, (float*) iqm_vertex_arrays.verts_pos, iqm_header->n_verts, 3, (float*) &default_vert);
    iqm_parse_float_array(iqm_header, raw_verts_uv,  (float*) iqm_vertex_arrays.verts_uv,  iqm_header->n_verts, 2, (float*) &default_uv);
    iqm_parse_float_array(iqm_header, raw_verts_bone_weights, iqm_vertex_arrays.verts_bone_weights,  iqm_header->n_verts, IQM_VERT_BONES, (float*) &default_bone_weights);
    iqm_parse_uint8_array(iqm_header, raw_verts_bone_idxs,    iqm_vertex_arrays.verts_bone_idxs,     iqm_header->n_verts, IQM_VERT_BONES, max_bone_idx);
#ifdef IQM_LOAD_NORMALS
    iqm_parse_float_array(iqm_header, raw_verts_nor, (float*) iqm_vertex_arrays.verts_nor, iqm_header->n_verts, 3, (float*) &default_nor);
#endif // IQM_LOAD_NORMALS
    // iqm_parse_float_array(iqm_header, raw_verts_tan, (float*) iqm_vertex_arrays.verts_tan, iqm_header->n_verts, 4, (float*) &default_tan);
    // iqm_parse_float_array(iqm_header, raw_verts_color,  (float*) iqm_vertex_arrays.verts_color,  iqm_header->n_verts, 4, (float*) &default_color);
    // ------------------------------------------------------------------------

#ifdef IQMDEBUG_LOADIQM_LOADMESH
    Con_Printf("load_iqm_model_vertex_arrays -- Done parsing %d verts\n", iqm_header->n_verts);
#endif // IQMDEBUG_LOADIQM_LOADMESH

    return iqm_vertex_arrays;
}





// 
// Deallocates any internal heap memory for struct `iqm_model_vertex_arrays_t`
//
void free_iqm_model_vertex_arrays(iqm_model_vertex_arrays_t *iqm_vertex_arrays) {

#ifdef IQMDEBUG_LOADIQM_LOADMESH
    Con_Printf("free_iqm_model_vertex_arrays -- Start\n");
#endif // IQMDEBUG_LOADIQM_LOADMESH

    free_pointer_and_clear(&iqm_vertex_arrays->verts_pos);
    free_pointer_and_clear(&iqm_vertex_arrays->verts_uv);
    free_pointer_and_clear(&iqm_vertex_arrays->verts_bone_weights);
    free_pointer_and_clear(&iqm_vertex_arrays->verts_bone_idxs);
#ifdef IQM_LOAD_NORMALS
    free_pointer_and_clear(&iqm_vertex_arrays->verts_nor);
#endif // IQM_LOAD_NORMALS
    // free_pointer_and_clear(iqm_vertex_arrays->verts_tan);
    // free_pointer_and_clear(iqm_vertex_arrays->verts_color);

#ifdef IQMDEBUG_LOADIQM_LOADMESH
    Con_Printf("free_iqm_model_vertex_arrays -- Done\n");
#endif // IQMDEBUG_LOADIQM_LOADMESH

}






//
// Given pointer to raw IQM model bytes buffer, loads the IQM model and 
// builds a heap-allocated `skeletal_model_t` skeletal model
// 
skeletal_model_t *load_iqm_file(void *iqm_data) {
    const iqm_header_t *iqm_header = (const iqm_header_t*) iqm_data;

    const iqm_model_vertex_arrays_t iqm_vertex_arrays = load_iqm_model_vertex_arrays(iqm_header);

    // TODO - Perform checks here to see if we found the right vert arrays
    // If at least one mesh, must have 
    // If no meshes, this is an animation only file - TODO - Check if we have at least one anim?


    skeletal_model_t *skel_model = (skeletal_model_t*) malloc(sizeof(skeletal_model_t));

    // Initialize values. These will be calculated later on
    VectorSet(skel_model->mins, -1,-1,-1);
    VectorSet(skel_model->maxs, 1,1,1);

    // ------------------------------------------------------------------------
    // Platform-specific mesh data loading of IQM data
    // ------------------------------------------------------------------------
    skel_model->n_meshes = iqm_header->n_meshes;
    skel_model->meshes = load_iqm_meshes(iqm_data, &iqm_vertex_arrays);
    // ------------------------------------------------------------------------

    free_iqm_model_vertex_arrays(&iqm_vertex_arrays);




    // ------------------------------------------------------------------------
    // Platform-agnostic loading of IQM data
    // ------------------------------------------------------------------------


    // --------------------------------------------------
    // Parse bones and their rest transforms
    // --------------------------------------------------
#ifdef IQMDEBUG_LOADIQM_BONEINFO
    Con_Printf("Parsing joints...\n");
#endif // IQMDEBUG_LOADIQM_BONEINFO
    skel_model->n_bones = iqm_header->n_joints ? iqm_header->n_joints : iqm_header->n_poses;
    // Skeleton bone names and parent indices are always included
    skel_model->bone_name = (char**) malloc(sizeof(char*) * skel_model->n_bones);
    skel_model->bone_parent_idx = (int16_t*) malloc(sizeof(int16_t) * skel_model->n_bones);

    const iqm_joint_quaternion_t *iqm_joints = (const iqm_joint_quaternion_t*) ((uint8_t*) iqm_data + iqm_header->ofs_joints);
    for(uint32_t i = 0; i < iqm_header->n_joints; i++) {
        const char *joint_name = (const char*) (((uint8_t*) iqm_data + iqm_header->ofs_text) + iqm_joints[i].name);
        skel_model->bone_name[i] = (char*) malloc(sizeof(char) * (strlen(joint_name) + 1));
        strcpy(skel_model->bone_name[i], joint_name);
        skel_model->bone_parent_idx[i] = iqm_joints[i].parent_joint_idx;
    }


    // Bone rest transforms are _only_ included if there is at least one mesh
    skel_model->bone_rest_pos = NULL;
    skel_model->bone_rest_rot = NULL;
    skel_model->bone_rest_scale = NULL;
    if(skel_model->n_meshes > 0 ) {
        skel_model->bone_rest_pos = (vec3_t*) malloc(sizeof(vec3_t) * skel_model->n_bones);
        skel_model->bone_rest_rot = (quat_t*) malloc(sizeof(quat_t) * skel_model->n_bones);
        skel_model->bone_rest_scale = (vec3_t*) malloc(sizeof(vec3_t) * skel_model->n_bones);
        for(uint32_t i = 0; i < iqm_header->n_joints; i++) {
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
        }
    }

    // Bone hitboxes are _only_ defined if the model has at least one mesh
    // --------------------------------------------------
    // Set default hitbox values for all bones
    // (These values may be overridden by JSON file)
    // --------------------------------------------------
    skel_model->bone_hitbox_enabled = NULL;
    skel_model->bone_hitbox_ofs = NULL;
    skel_model->bone_hitbox_scale = NULL;
    skel_model->bone_hitbox_tag = NULL;
    if(skel_model->n_meshes > 0 ) {
        skel_model->bone_hitbox_enabled = (bool*) malloc(sizeof(bool) * skel_model->n_bones);
        skel_model->bone_hitbox_ofs = (vec3_t*) malloc(sizeof(vec3_t) * skel_model->n_bones);
        skel_model->bone_hitbox_scale = (vec3_t*) malloc(sizeof(vec3_t) * skel_model->n_bones);
        skel_model->bone_hitbox_tag = (int*) malloc(sizeof(int) * skel_model->n_bones);
        for(uint32_t i = 0; i < skel_model->n_bones; i++) {
            skel_model->bone_hitbox_enabled[i] = false;
            skel_model->bone_hitbox_ofs[i][0] = 0.0f;
            skel_model->bone_hitbox_ofs[i][1] = 0.0f;
            skel_model->bone_hitbox_ofs[i][2] = 0.0f;
            skel_model->bone_hitbox_scale[i][0] = 1.0f;
            skel_model->bone_hitbox_scale[i][1] = 1.0f;
            skel_model->bone_hitbox_scale[i][2] = 1.0f;
            skel_model->bone_hitbox_tag[i] = 0;
        }
    }
    // --------------------------------------------------


#ifdef IQMDEBUG_LOADIQM_BONEINFO
    Con_Printf("\tParsed %d bones.\n", skel_model->n_bones);
#endif // IQMDEBUG_LOADIQM_BONEINFO



#ifdef IQM_BBOX_PER_MODEL_PER_ANIM
    // --------------------------------------------------
    // Set default skeleton hitbox values
    // These will be calculated properly by`calc_skel_model_bounds_for_rest_pose` 
    // and `calc_skel_model_bounds_for_anim`
    // --------------------------------------------------
    VectorSet(skel_model->rest_pose_bone_hitbox_mins, -1, -1, -1);
    VectorSet(skel_model->rest_pose_bone_hitbox_maxs,  1,  1,  1);
    VectorSet(skel_model->anim_bone_hitbox_mins, -1, -1, -1);
    VectorSet(skel_model->anim_bone_hitbox_maxs,  1,  1,  1);
    // --------------------------------------------------
#endif // IQM_BBOX_PER_MODEL_PER_ANIM





#ifdef IQMDEBUG_LOADIQM_BONEINFO
    if(skel_model->bone_parent_idx != NULL) {
        for(uint32_t i = 0; i < skel_model->n_bones; i++) {
            Con_Printf("Parsed bone: %i, \"%s\" (parent bone: %d)\n", i, skel_model->bone_name[i], skel_model->bone_parent_idx[i]);
            if(skel_model->bone_rest_pos != NULL) {
                Con_Printf("\tPos: (%.2f, %.2f, %.2f)\n", skel_model->bone_rest_pos[i][0], skel_model->bone_rest_pos[i][1], skel_model->bone_rest_pos[i][2]);
            }
            if(skel_model->bone_rest_rot != NULL) {
                Con_Printf("\tRot: (%.2f, %.2f, %.2f, %.2f)\n", skel_model->bone_rest_rot[i][0], skel_model->bone_rest_rot[i][1], skel_model->bone_rest_rot[i][2], skel_model->bone_rest_rot[i][3]);
            }
            if(skel_model->bone_rest_scale != NULL) {
                Con_Printf("\tScale: (%.2f, %.2f, %.2f)\n", skel_model->bone_rest_scale[i][0], skel_model->bone_rest_scale[i][1], skel_model->bone_rest_scale[i][2]);
            }
            if(skel_model->bone_hitbox_enabled != NULL) {
                Con_Printf("\tDefault hitbox enabled: %d\n", skel_model->bone_hitbox_enabled[i]);
            }
            if(skel_model->bone_hitbox_ofs != NULL) {
                Con_Printf("\tDefault hitbox ofs: (%.2f, %.2f, %.2f)\n", skel_model->bone_hitbox_ofs[i][0], skel_model->bone_hitbox_ofs[i][1], skel_model->bone_hitbox_ofs[i][2]);
            }
            if(skel_model->bone_hitbox_scale != NULL) {
                Con_Printf("\tDefault hitbox scale: (%.2f, %.2f, %.2f)\n", skel_model->bone_hitbox_scale[i][0], skel_model->bone_hitbox_scale[i][1], skel_model->bone_hitbox_scale[i][2]);
            }
            if(skel_model->bone_hitbox_tag != NULL) {
                Con_Printf("\tDefault hitbox tag: %d\n", skel_model->bone_hitbox_tag[i]);
            }
        }
    }
#endif // IQMDEBUG_LOADIQM_BONEINFO


    for(uint32_t i = 0; i < skel_model->n_bones; i++) {
        // i-th bone's parent index must be less than i
        if((int) i <= skel_model->bone_parent_idx[i]) {
            Con_Printf("Error: IQM file bones are sorted incorrectly. Bone %d is located before its parent bone %d.\n", i, skel_model->bone_parent_idx[i]);
            free_unpacked_skeletal_model(skel_model);
            return NULL;
        }
    }


    // Check if we have a bone named `fps_cam`
    skel_model->fps_cam_bone_idx = -1;
    for(uint32_t i = 0; i < skel_model->n_bones; i++) {
        if(!strcmp(skel_model->bone_name[i], "fps_cam")) {
            skel_model->fps_cam_bone_idx = i;
            break;
        }
    }
    // --------------------------------------------------




    // --------------------------------------------------
    // When model has defined rest transforms...
    // Calculate static bone transforms that will be reused often
    // --------------------------------------------------
    skel_model->bone_rest_transforms = NULL;
    skel_model->inv_bone_rest_transforms = NULL;
    if(skel_model->bone_rest_pos != NULL && skel_model->bone_rest_rot != NULL && skel_model->bone_rest_scale != NULL) {
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
        // NOTE - `Matrix3x4_Invert_Simple` assumes uniform scaling
        // NOTE - If we want non-uniform scaling, use `Matrix3x4_Invert_Affine`
        for(uint32_t i = 0; i < skel_model->n_bones; i++) {
            Matrix3x4_Invert_Simple( skel_model->inv_bone_rest_transforms[i], skel_model->bone_rest_transforms[i]);
        }
    }
    // --------------------------------------------------


    // --------------------------------------------------
    // Parse all frames (poses)
    // --------------------------------------------------
    skel_model->n_frames = iqm_header->n_frames;
#ifdef IQMDEBUG_LOADIQM_ANIMINFO
    Con_Printf("\tBones: %d\n",skel_model->n_bones);
    Con_Printf("\tFrames: %d\n",iqm_header->n_frames);
    Con_Printf("\tPoses: %d\n",iqm_header->n_poses);
#endif // IQMDEBUG_LOADIQM_ANIMINFO
    skel_model->frames_move_dist = (float *) malloc(sizeof(float) * skel_model->n_frames);
    skel_model->frames_bone_pos = (vec3_t *) malloc(sizeof(vec3_t) * skel_model->n_bones * skel_model->n_frames);
    skel_model->frames_bone_rot = (quat_t *) malloc(sizeof(quat_t) * skel_model->n_bones * skel_model->n_frames);
    skel_model->frames_bone_scale = (vec3_t *) malloc(sizeof(vec3_t) * skel_model->n_bones * skel_model->n_frames);



    const uint16_t *frames_data = (const uint16_t*)((uint8_t*) iqm_data + iqm_header->ofs_frames);
    int frames_data_ofs = 0;
    const iqm_pose_quaternion_t *iqm_poses = (const iqm_pose_quaternion_t*) ((uint8_t*) iqm_data + iqm_header->ofs_poses);

    // Iterate over actual frames in IQM file:
    for(uint32_t i = 0; i < iqm_header->n_frames; i++) {

        // Initialize frame movement distance to 0 for all frames
        skel_model->frames_move_dist[i] = 0.0f;

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
            skel_model->framegroup_loop[i] = iqm_framegroups[i].flags & (uint32_t) IQM_ANIM_FLAG_LOOP;
            // FOR DEBUG ONLY: Force-loop all animations:
            skel_model->framegroup_loop[i] = true;
        }
    }
#ifdef IQMDEBUG_LOADIQM_ANIMINFO
    Con_Printf("\tParsed %d framegroups.\n", skel_model->n_framegroups);
#endif // IQMDEBUG_LOADIQM_ANIMINFO
    // --------------------------------------------------


    // --------------------------------------------------
    // Initialize material default values
    // --------------------------------------------------
    skel_model->n_materials = 0;
    skel_model->material_names = NULL;
    skel_model->material_n_skins = NULL;
    skel_model->materials = NULL;
    // --------------------------------------------------


    // --------------------------------------------------
    // Parse IQM per-frame mins/maxs to compute overall mins/maxs
    // --------------------------------------------------
    const iqm_bounds_t *iqm_frame_bounds = (const iqm_bounds_t *)((uint8_t*) iqm_data + iqm_header->ofs_bounds);
    if(iqm_header->ofs_bounds != 0) {
        // Compute overall model mins / maxes by finding the most extreme points for all frames:
        for(uint16_t i = 0; i < iqm_header->n_frames; i++) {
            for(int j = 0; j < 3; j++) {
                skel_model->mins[0] = fmin(skel_model->mins[0], iqm_frame_bounds[i].mins[0]);
                skel_model->maxs[0] = fmax(skel_model->maxs[0], iqm_frame_bounds[i].maxs[0]);
            }
        }
    }
    // --------------------------------------------------


    size_t file_len = iqm_header->filesize;

    // --------------------------------------------------
    // Parse FTE_EVENT IQM extension
    // --------------------------------------------------
    Con_Printf("About to parse IQM FTE events\n");
    size_t iqm_fte_ext_event_size;
    const iqm_ext_fte_event_t *iqm_fte_ext_events = (const iqm_ext_fte_event_t*) iqm_find_extension((uint8_t*)iqm_data, file_len, "FTE_EVENT", &iqm_fte_ext_event_size);
    uint16_t iqm_fte_ext_event_n_events = iqm_fte_ext_event_size / sizeof(iqm_ext_fte_event_t);
    Con_Printf("FTE_EVENTS parsed size %d\n", iqm_fte_ext_event_size);
    Con_Printf("num FTE_EVENTS %d\n", iqm_fte_ext_event_n_events);
    Con_Printf("FTE event struct %d\n", sizeof(iqm_ext_fte_event_t));

    skel_model->framegroup_n_events = NULL;
    skel_model->framegroup_event_time = NULL;
    skel_model->framegroup_event_code = NULL;
    skel_model->framegroup_event_data_str = NULL;

    if(iqm_fte_ext_events != NULL) {
        // Count the number of events for each framegroup:
        skel_model->framegroup_n_events = (uint16_t*) malloc(sizeof(uint16_t) * skel_model->n_framegroups);
        for(uint16_t i = 0; i < skel_model->n_framegroups; i++) {
            skel_model->framegroup_n_events[i] = 0;
        }
        for(uint16_t i = 0; i < iqm_fte_ext_event_n_events; i++) {
            int framegroup_idx = iqm_fte_ext_events[i].anim;
            uint32_t event_code = iqm_fte_ext_events[i].event_code;
            skel_model->framegroup_n_events[framegroup_idx]++;
        }

        // Allocate memory for all framegroup arrays
        skel_model->framegroup_event_time = (float**) malloc(sizeof(float*) * skel_model->n_framegroups);
        skel_model->framegroup_event_code = (uint32_t**) malloc(sizeof(uint32_t*) * skel_model->n_framegroups);
        skel_model->framegroup_event_data_str = (char***) malloc(sizeof(char**) * skel_model->n_framegroups);

        // Allocate enough memory in each framegroup array to hold events in a list:
        for(uint16_t i = 0; i < skel_model->n_framegroups; i++) {
            if(skel_model->framegroup_n_events[i] == 0) {
                skel_model->framegroup_event_time[i] = NULL;
                skel_model->framegroup_event_code[i] = NULL;
                skel_model->framegroup_event_data_str[i] = NULL;
            }
            else {
                skel_model->framegroup_event_time[i] = (float*) malloc(sizeof(float) * skel_model->framegroup_n_events[i]);
                skel_model->framegroup_event_code[i] = (uint32_t*) malloc(sizeof(uint32_t) * skel_model->framegroup_n_events[i]);
                skel_model->framegroup_event_data_str[i] = (char**) malloc(sizeof(char*) * skel_model->framegroup_n_events[i]);
                // Set each char array pointer to NULL, we'll use this to identify empty indices
                for(int j = 0; j < skel_model->framegroup_n_events[i]; j++) {
                    skel_model->framegroup_event_data_str[i][j] = NULL;
                }
            }
        }

        for(uint16_t i = 0; i < iqm_fte_ext_event_n_events; i++) {
            const char *iqm_event_data_str = (const char*) ((uint8_t*) iqm_data + iqm_header->ofs_text + iqm_fte_ext_events[i].event_data_str);

            int event_framegroup_idx = iqm_fte_ext_events[i].anim;
            float event_time = iqm_fte_ext_events[i].timestamp;
            uint32_t event_code = iqm_fte_ext_events[i].event_code;

            // Allocate data for this string and copy the char array
            char *event_data_str = (char*) malloc(sizeof(char) * (strlen(iqm_event_data_str) + 1));
            strcpy(event_data_str, iqm_event_data_str);

            // If invalid framegroup idx, skip the event
            if(event_framegroup_idx < 0 || event_framegroup_idx >= skel_model->n_framegroups) {
                Con_Printf("WARNING: Unable to load IQM event. Framegroup idx %d invalid for a model with %d framegroups.\n", event_framegroup_idx, skel_model->n_framegroups);
                continue;
            }

            // Find the correct index to insert the event into in this framegroup's event list
            int event_idx = 0;
            for(; event_idx < skel_model->framegroup_n_events[event_framegroup_idx]; event_idx++) {
                // If this index is empty, insert here
                if(skel_model->framegroup_event_data_str[event_framegroup_idx][event_idx] == NULL) {
                    break;
                }
                // If the event at this index has an event time > the current event, insert here
                else if(skel_model->framegroup_event_time[event_framegroup_idx][event_idx] > event_time) {
                    break;
                }
            }
            // Con_Printf("Event %d inserting at framegroup %d index %d\n", i, event_framegroup_idx, event_idx);

            // If event_idx exceeds the length of the array, warn and skip (This should never happen)
            if(event_idx >= skel_model->framegroup_n_events[event_framegroup_idx]) {
                Con_Printf("WARNING: Unable to load IQM event for time %f for framegroup %d.\n", i, event_framegroup_idx);
                continue;
            }

            // Shift up the events in the array.
            // Starting from the end of the array, move up all events after the insertion `event_idx`
            for(int j = skel_model->framegroup_n_events[event_framegroup_idx] - 2; j >= event_idx; j--) {
                if(skel_model->framegroup_event_data_str[event_framegroup_idx][j] == NULL) {
                    continue;
                }
                // Con_Printf("\tframegroup %d moving event at idx %d to idx %d\n", event_framegroup_idx, j, j+1);
                skel_model->framegroup_event_time[event_framegroup_idx][j+1] = skel_model->framegroup_event_time[event_framegroup_idx][j];
                skel_model->framegroup_event_data_str[event_framegroup_idx][j+1] = skel_model->framegroup_event_data_str[event_framegroup_idx][j];
                skel_model->framegroup_event_code[event_framegroup_idx][j+1] = skel_model->framegroup_event_code[event_framegroup_idx][j];
            }

            // Insert the new event at the chosen index:
            skel_model->framegroup_event_time[event_framegroup_idx][event_idx] = event_time;
            skel_model->framegroup_event_data_str[event_framegroup_idx][event_idx] = event_data_str;
            skel_model->framegroup_event_code[event_framegroup_idx][event_idx] = event_code;
        }
    }
    // --------------------------------------------------


    // // --------------------------------------------------
    // // Parse FTE_SKIN IQM extension
    // // --------------------------------------------------
    // size_t iqm_fte_ext_skin_size;
    // const uint32_t *iqm_fte_ext_skin_n_skins = (const uint32_t*) iqm_find_extension(iqm_data, file_len, "FTE_SKINS", &iqm_fte_ext_skin_size);

    // // const iqm_ext_fte_skin_t *iqm_fte_ext_skins
    // if(iqm_fte_ext_skin_n_skins != NULL) {

    //     if(iqm_fte_ext_skin_size >= (sizeof(uint32_t) * 2 * iqm_header->n_meshes)) {
    //         const uint32_t n_skin_skinframes = iqm_fte_ext_skin_n_skins[0];
    //         const uint32_t n_skin_meshskins = iqm_fte_ext_skin_n_skins[1];
    //         size_t expected_skin_size = sizeof(iqm_ext_fte_skin_t) + sizeof(uint32_t) * iqm_header->n_meshes + sizeof(iqm_ext_fte_skin_t) * n_skin_skinframes + sizeof(iqm_ext_fte_skin_meshskin_t) * n_skin_meshskins;
    //         if(iqm_fte_ext_skin_size == expected_skin_size) {
    //             const iqm_ext_fte_skin_skinframe_t *skinframes = (const iqm_ext_fte_skin_skinframe_t *) (iqm_fte_ext_skin_n_skins + 2 + iqm_header->n_meshes);
    //             const iqm_ext_fte_skin_meshskin_t *meshskins = (const iqm_ext_fte_skin_meshskin_t *) (skinframes + n_skin_skinframes);

    //             // TODO - Print these? Not sure if we'll even want them... or if we should support them?
    //         }
    //     }
    // }
    // // --------------------------------------------------
    // ------------------------------------------------------------------------




    // ------------------------------------------------------------------------
    // IQM File requirements error checking:
    // ------------------------------------------------------------------------
    //
    // Currently we allow for three types of IQM model files:
    //  1. IQM model with: meshes, skeleton, animation framegroups
    //  2. IQM model with: meshes, skeleton
    //  3. IQM model with: skeleton, animation framegroups
    // 
    //  IQM models with only meshes, only skeletons, or only framegroups 
    //  are currently not supported.
    // 
    bool has_meshes = (skel_model->n_meshes > 0);
    bool has_skeleton = (skel_model->n_bones > 0);
    bool has_animations = (skel_model->n_frames > 0) && (skel_model->n_framegroups > 0);
    if(!has_skeleton) {
        Con_Printf("Error: IQM model file does not have skeleton data.\n");
        free_unpacked_skeletal_model(skel_model);
        return NULL;
    }
    if(!has_meshes && !has_animations) {
        Con_Printf("Error: IQM model file has neither mesh data nor animation data. At least one of these is required.\n");
        free_unpacked_skeletal_model(skel_model);
        return NULL;
    }
    // ------------------------------------------------------------------------

    return skel_model;
}




// 
// Returns the amount of memory needed to store a string, including null terminator
// Additionally, pads an extra byte to get 2-byte aligned values 
// (Since relocatable model tightly packs strings together, we must keep things 2-byte aligned)
// 
size_t safe_strsize(char *str) {
    if(str == NULL) {
        return 0;
    }

    size_t n_bytes = sizeof(char) * (strlen(str) + 1);
    // Make 4-byte aligned
    n_bytes = pad_n_bytes(n_bytes);
    return n_bytes;
}

// 
// Returns some number >= n_bytes that is divisible by 4
//
// Some platforms have limitations w.r.t. requiring read addresses to be 2-byte, 4-byte aligned
// Because we're packing structs into contiguous memory, each packed struct should be padded
// such that the next struct in the contiguous memory will always have an address that's 4-byte aligned
// 
size_t pad_n_bytes(size_t n_bytes) {
    n_bytes += (PACKING_BYTE_PADDING - (n_bytes % PACKING_BYTE_PADDING)) % PACKING_BYTE_PADDING;
    return n_bytes;
}





// 
// Sums up the data pointed to by all pointers and returns the total total number 
// of bytes required to store an unpacked skeletal_model_t as a packed contigous
// block of memory
// 
size_t count_unpacked_skeletal_model_n_bytes(skeletal_model_t *skel_model) {
    size_t skel_model_n_bytes = 0;
    skel_model_n_bytes += sizeof(skeletal_model_t);

    skel_model_n_bytes += count_unpacked_skeletal_model_meshes_n_bytes(skel_model);

    // -- bones --
    skel_model_n_bytes += pad_n_bytes(sizeof(char*) * skel_model->n_bones);
    skel_model_n_bytes += pad_n_bytes(sizeof(int16_t) * skel_model->n_bones);
    skel_model_n_bytes += pad_n_bytes(sizeof(vec3_t) * skel_model->n_bones);
    skel_model_n_bytes += pad_n_bytes(sizeof(quat_t) * skel_model->n_bones);
    skel_model_n_bytes += pad_n_bytes(sizeof(vec3_t) * skel_model->n_bones);
    for(int i = 0; i < skel_model->n_bones; i++) {
        skel_model_n_bytes += safe_strsize(skel_model->bone_name[i]);
    }
    // -- bone hitbox info --
    skel_model_n_bytes += pad_n_bytes(sizeof(bool) * skel_model->n_bones);
    skel_model_n_bytes += pad_n_bytes(sizeof(vec3_t) * skel_model->n_bones);
    skel_model_n_bytes += pad_n_bytes(sizeof(vec3_t) * skel_model->n_bones);
    skel_model_n_bytes += pad_n_bytes(sizeof(int) * skel_model->n_bones);
    // -- cached bone rest transforms --
    skel_model_n_bytes += pad_n_bytes(sizeof(mat3x4_t) * skel_model->n_bones);
    skel_model_n_bytes += pad_n_bytes(sizeof(mat3x4_t) * skel_model->n_bones);
    // -- animation frames -- 
    skel_model_n_bytes += pad_n_bytes(sizeof(vec3_t) * skel_model->n_bones * skel_model->n_frames);
    skel_model_n_bytes += pad_n_bytes(sizeof(quat_t) * skel_model->n_bones * skel_model->n_frames);
    skel_model_n_bytes += pad_n_bytes(sizeof(vec3_t) * skel_model->n_bones * skel_model->n_frames);
    skel_model_n_bytes += pad_n_bytes(sizeof(float) * skel_model->n_frames);
    // -- animation framegroups --
    skel_model_n_bytes += pad_n_bytes(sizeof(char*) * skel_model->n_framegroups);
    skel_model_n_bytes += pad_n_bytes(sizeof(uint32_t) * skel_model->n_framegroups);
    skel_model_n_bytes += pad_n_bytes(sizeof(uint32_t) * skel_model->n_framegroups);
    skel_model_n_bytes += pad_n_bytes(sizeof(float) * skel_model->n_framegroups);
    skel_model_n_bytes += pad_n_bytes(sizeof(bool) * skel_model->n_framegroups);
    for(int i = 0; i < skel_model->n_framegroups; i++) {
        skel_model_n_bytes += safe_strsize(skel_model->framegroup_name[i]);
    }
    // -- materials --
    skel_model_n_bytes += pad_n_bytes(sizeof(char*) * skel_model->n_materials);
    skel_model_n_bytes += pad_n_bytes(sizeof(int) * skel_model->n_materials);
    skel_model_n_bytes += pad_n_bytes(sizeof(skeletal_material_t*) * skel_model->n_materials);
    for(int i = 0; i < skel_model->n_materials; i++) {
        skel_model_n_bytes += safe_strsize(skel_model->material_names[i]);
        skel_model_n_bytes += pad_n_bytes(sizeof(skeletal_material_t) * skel_model->material_n_skins[i]);
    }
    // -- FTE Anim Events --
    if(skel_model->framegroup_n_events != NULL) {
        skel_model_n_bytes += pad_n_bytes(sizeof(uint16_t) * skel_model->n_framegroups);
        skel_model_n_bytes += pad_n_bytes(sizeof(float*) * skel_model->n_framegroups);
        skel_model_n_bytes += pad_n_bytes(sizeof(char**) * skel_model->n_framegroups);
        skel_model_n_bytes += pad_n_bytes(sizeof(uint32_t*) * skel_model->n_framegroups);
        for(int i = 0; i < skel_model->n_framegroups; i++) {
            skel_model_n_bytes += pad_n_bytes(sizeof(float) * skel_model->framegroup_n_events[i]);
            skel_model_n_bytes += pad_n_bytes(sizeof(char*) * skel_model->framegroup_n_events[i]);
            skel_model_n_bytes += pad_n_bytes(sizeof(uint32_t) * skel_model->framegroup_n_events[i]);
            for(int j = 0; j < skel_model->framegroup_n_events[i]; j++) {
                skel_model_n_bytes += safe_strsize(skel_model->framegroup_event_data_str[i][j]);
            }
        }
    }
    return skel_model_n_bytes;
}







//
// Given a packed or unpacked `skel_model`, debug prints all of its data
//
void debug_print_skeletal_model(skeletal_model_t *skel_model, int is_packed) {
    // ------------------------------------------------------------------------
    // Debug - printing the skel_model contents
    // ------------------------------------------------------------------------
    Con_Printf("------------------------------------------------------------\n");
    Con_Printf("Debug printing %s skeletal model \"%s\"\n", (is_packed ? "packed" : "unpacked") , loadname);
    Con_Printf("------------------------------------------------------------\n");
    Con_Printf("skel_model->mins = [%f, %f, %f]\n", skel_model->mins[0], skel_model->mins[1], skel_model->mins[2]);
    Con_Printf("skel_model->maxs = [%f, %f, %f]\n", skel_model->maxs[0], skel_model->maxs[1], skel_model->maxs[2]);
    Con_Printf("skel_model->n_meshes = %d\n", skel_model->n_meshes);

    // Print platform-specific mesh info
    debug_print_skeletal_model_meshes(skel_model, is_packed);

    Con_Printf("skel_model->n_bones = %d\n", skel_model->n_bones);

    char **bone_names         = OPT_UNPACK_MEMBER(skel_model->bone_name,           skel_model, is_packed);
    int16_t *bone_parent_idx  = OPT_UNPACK_MEMBER(skel_model->bone_parent_idx,     skel_model, is_packed);
    vec3_t *bone_rest_pos     = OPT_UNPACK_MEMBER(skel_model->bone_rest_pos,       skel_model, is_packed);
    quat_t *bone_rest_rot     = OPT_UNPACK_MEMBER(skel_model->bone_rest_rot,       skel_model, is_packed);
    vec3_t *bone_rest_scale   = OPT_UNPACK_MEMBER(skel_model->bone_rest_scale,     skel_model, is_packed);
    bool *bone_hitbox_enabled = OPT_UNPACK_MEMBER(skel_model->bone_hitbox_enabled, skel_model, is_packed);
    vec3_t *bone_hitbox_ofs   = OPT_UNPACK_MEMBER(skel_model->bone_hitbox_ofs,     skel_model, is_packed);
    vec3_t *bone_hitbox_scale = OPT_UNPACK_MEMBER(skel_model->bone_hitbox_scale,   skel_model, is_packed);
    int *bone_hitbox_tag      = OPT_UNPACK_MEMBER(skel_model->bone_hitbox_tag,     skel_model, is_packed);

    mat3x4_t *bone_rest_transforms     = OPT_UNPACK_MEMBER(skel_model->bone_rest_transforms,      skel_model, is_packed);
    mat3x4_t *inv_bone_rest_transforms = OPT_UNPACK_MEMBER(skel_model->inv_bone_rest_transforms,  skel_model, is_packed);


    for(uint32_t bone_idx = 0; bone_idx < skel_model->n_bones; bone_idx++) {
        Con_Printf("---------------------\n");
        // -------------
        const char *bone_name = OPT_UNPACK_MEMBER(bone_names[bone_idx], skel_model, is_packed);
        Con_Printf("skel_model->bone_name[%d] = \"%s\"\n",                bone_idx, bone_name);
        Con_Printf("skel_model->bone_parent_idx[%d] = %d\n",              bone_idx, bone_parent_idx[bone_idx]);
        if(bone_rest_pos != NULL) {
            Con_Printf("skel_model->bone_rest_pos[%d] = [%f, %f, %f]\n",      bone_idx, bone_rest_pos[bone_idx][0], bone_rest_pos[bone_idx][1], bone_rest_pos[bone_idx][2]);
        }
        if(bone_rest_rot != NULL) {
            Con_Printf("skel_model->bone_rest_rot[%d] = [%f, %f, %f, %f]\n",  bone_idx, bone_rest_rot[bone_idx][0], bone_rest_rot[bone_idx][1], bone_rest_rot[bone_idx][2], bone_rest_rot[bone_idx][3]);
        }
        if(bone_rest_scale != NULL) {
            Con_Printf("skel_model->bone_rest_scale[%d] = [%f, %f, %f]\n",    bone_idx, bone_rest_scale[bone_idx][0], bone_rest_scale[bone_idx][1], bone_rest_scale[bone_idx][2]);
        }
        if(bone_hitbox_enabled != NULL) {
            Con_Printf("skel_model->bone_hitbox_enabled[%d] = %d\n",          bone_idx, bone_hitbox_enabled[bone_idx]);
        }
        if(bone_hitbox_ofs != NULL) {
            Con_Printf("skel_model->bone_hitbox_ofs[%d] = [%f, %f, %f]\n",    bone_idx, bone_hitbox_ofs[bone_idx][0], bone_hitbox_ofs[bone_idx][1], bone_hitbox_ofs[bone_idx][2]);
        }
        if(bone_hitbox_scale != NULL) {
            Con_Printf("skel_model->bone_hitbox_scale[%d] = [%f, %f, %f]\n",  bone_idx, bone_hitbox_scale[bone_idx][0], bone_hitbox_scale[bone_idx][1], bone_hitbox_scale[bone_idx][2]);
        }
        if(bone_hitbox_tag != NULL) {
            Con_Printf("skel_model->bone_hitbox_tag[%d] = %d\n",              bone_idx, bone_hitbox_tag[bone_idx]);
        }

        mat3x4_t *print_mat;

        if(bone_rest_transforms != NULL) {
            Con_Printf("skel_model->bone_rest_transforms[%d] = \n", bone_idx);
            print_mat = &(bone_rest_transforms[bone_idx]);
            Con_Printf("\t[%.2f, %.2f, %.2f, %.2f]\n", (*print_mat)[0][0], (*print_mat)[0][1], (*print_mat)[0][2], (*print_mat)[0][3]);
            Con_Printf("\t[%.2f, %.2f, %.2f, %.2f]\n", (*print_mat)[1][0], (*print_mat)[1][1], (*print_mat)[1][2], (*print_mat)[1][3]);
            Con_Printf("\t[%.2f, %.2f, %.2f, %.2f]\n", (*print_mat)[2][0], (*print_mat)[2][1], (*print_mat)[2][2], (*print_mat)[2][3]);
            Con_Printf("\t[%.2f, %.2f, %.2f, %.2f]\n", 0.0f, 0.0f, 0.0f, 1.0f); // for 3x4 matrices, last row is implied to be [0,0,0,1]
        }

        if(inv_bone_rest_transforms != NULL) {
            Con_Printf("skel_model->inv_bone_rest_transforms[%d] = \n", bone_idx);
            print_mat = &(inv_bone_rest_transforms[bone_idx]);
            Con_Printf("\t[%.2f, %.2f, %.2f, %.2f]\n", (*print_mat)[0][0], (*print_mat)[0][1], (*print_mat)[0][2], (*print_mat)[0][3]);
            Con_Printf("\t[%.2f, %.2f, %.2f, %.2f]\n", (*print_mat)[1][0], (*print_mat)[1][1], (*print_mat)[1][2], (*print_mat)[1][3]);
            Con_Printf("\t[%.2f, %.2f, %.2f, %.2f]\n", (*print_mat)[2][0], (*print_mat)[2][1], (*print_mat)[2][2], (*print_mat)[2][3]);
            Con_Printf("\t[%.2f, %.2f, %.2f, %.2f]\n", 0.0f, 0.0f, 0.0f, 1.0f); // for 3x4 matrices, last row is implied to be [0,0,0,1]
        }

        Con_Printf("---------------------\n");
    }
    Con_Printf("skel_model->fps_cam_bone_idx = %d\n", skel_model->fps_cam_bone_idx);


#ifdef IQM_BBOX_PER_MODEL_PER_ANIM
        Con_Printf("skel_model->rest_pose_bone_hitbox_mins = [%f, %f, %f]\n",  skel_model->rest_pose_bone_hitbox_mins[0], skel_model->rest_pose_bone_hitbox_mins[1], skel_model->rest_pose_bone_hitbox_mins[2]);
        Con_Printf("skel_model->rest_pose_bone_hitbox_maxs = [%f, %f, %f]\n", skel_model->rest_pose_bone_hitbox_maxs[0], skel_model->rest_pose_bone_hitbox_maxs[1], skel_model->rest_pose_bone_hitbox_maxs[2]);
        Con_Printf("skel_model->anim_bone_hitbox_mins = [%f, %f, %f]\n",      skel_model->anim_bone_hitbox_mins[0], skel_model->anim_bone_hitbox_mins[1], skel_model->anim_bone_hitbox_mins[2]);
        Con_Printf("skel_model->anim_bone_hitbox_maxs = [%f, %f, %f]\n",      skel_model->anim_bone_hitbox_maxs[0], skel_model->anim_bone_hitbox_maxs[1], skel_model->anim_bone_hitbox_maxs[2]);
#endif // IQM_BBOX_PER_MODEL_PER_ANIM






    Con_Printf("skel_model->n_frames = %d\n", skel_model->n_frames);

    vec3_t *frames_bone_pos     = OPT_UNPACK_MEMBER(skel_model->frames_bone_pos,    skel_model, is_packed);
    quat_t *frames_bone_rot     = OPT_UNPACK_MEMBER(skel_model->frames_bone_rot,    skel_model, is_packed);
    vec3_t *frames_bone_scale   = OPT_UNPACK_MEMBER(skel_model->frames_bone_scale,  skel_model, is_packed);
    float  *frames_move_dist    = OPT_UNPACK_MEMBER(skel_model->frames_move_dist,   skel_model, is_packed);

    for(int frame_idx = 0; frame_idx < skel_model->n_frames; frame_idx++) {
        Con_Printf("---------------------\n");
        for(uint32_t bone_idx = 0; bone_idx < skel_model->n_bones; bone_idx++) {
            int entry_idx = frame_idx * skel_model->n_bones + bone_idx;
            Con_Printf("skel_model->frames_bone_pos[%d (frame: %d, bone: %d)] = [%f, %f, %f]\n",    entry_idx, frame_idx, bone_idx, frames_bone_pos[entry_idx][0], frames_bone_pos[entry_idx][1], frames_bone_pos[entry_idx][2]);
            Con_Printf("skel_model->frames_bone_rot[%d (frame: %d, bone: %d)] = [%f, %f, %f, %f]\n",    entry_idx, frame_idx, bone_idx, frames_bone_rot[entry_idx][0], frames_bone_rot[entry_idx][1], frames_bone_rot[entry_idx][2], frames_bone_rot[entry_idx][3]);
            Con_Printf("skel_model->frames_bone_scale[%d (frame: %d, bone: %d)] = [%f, %f, %f]\n",  entry_idx, frame_idx, bone_idx, frames_bone_scale[entry_idx][0], frames_bone_scale[entry_idx][1], frames_bone_scale[entry_idx][2]);
        }
        Con_Printf("---------------------\n");
        Con_Printf("skel_model->frames_move_dist[%d (frame: %d)] = %f\n",   frame_idx, frame_idx, frames_move_dist[frame_idx]);

        // Only print the first frame for all bones (printing all frames is too verbose)
        break;
    }

    Con_Printf("skel_model->n_framegroups = %d\n", skel_model->n_framegroups);
    char     **framegroup_names      = OPT_UNPACK_MEMBER(skel_model->framegroup_name,        skel_model, is_packed);
    uint32_t *framegroup_start_frame = OPT_UNPACK_MEMBER(skel_model->framegroup_start_frame, skel_model, is_packed);
    uint32_t *framegroup_n_frames    = OPT_UNPACK_MEMBER(skel_model->framegroup_n_frames,    skel_model, is_packed);
    float    *framegroup_fps         = OPT_UNPACK_MEMBER(skel_model->framegroup_fps,         skel_model, is_packed);
    bool     *framegroup_loop        = OPT_UNPACK_MEMBER(skel_model->framegroup_loop,        skel_model, is_packed);

    for(int framegroup_idx = 0; framegroup_idx < skel_model->n_framegroups; framegroup_idx++) {
        char *framegroup_name = OPT_UNPACK_MEMBER(framegroup_names[framegroup_idx], skel_model, is_packed);
        Con_Printf("---------------------\n");
        Con_Printf("skel_model->framegroup_names[%d] = \"%s\"\n",    framegroup_idx, framegroup_name);
        Con_Printf("skel_model->framegroup_start_frame[%d] = %d\n",  framegroup_idx, framegroup_start_frame[framegroup_idx]);
        Con_Printf("skel_model->framegroup_n_frames[%d] = %d\n",     framegroup_idx, framegroup_n_frames[framegroup_idx]);
        Con_Printf("skel_model->framegroup_fps[%d] = %f\n",          framegroup_idx, framegroup_fps[framegroup_idx]);
        Con_Printf("skel_model->framegroup_loop[%d] = %d\n",         framegroup_idx, framegroup_loop[framegroup_idx]);
        Con_Printf("---------------------\n");

    }

    Con_Printf("skel_model->n_materials = %d\n", skel_model->n_materials);
    char     **material_names       = OPT_UNPACK_MEMBER(skel_model->material_names,   skel_model, is_packed);
    int      *material_n_skins      = OPT_UNPACK_MEMBER(skel_model->material_n_skins, skel_model, is_packed);
    skeletal_material_t **materials = OPT_UNPACK_MEMBER(skel_model->materials,        skel_model, is_packed);

    for(int material_idx = 0; material_idx < skel_model->n_materials; material_idx++){
        char     *material_name       = OPT_UNPACK_MEMBER(material_names[material_idx],   skel_model, is_packed);
        Con_Printf("---------------------\n");
        Con_Printf("skel_model->material_names[%d] = \"%s\"\n",   material_idx, material_name);
        Con_Printf("skel_model->material_n_skins[%d] = %d\n", material_idx, material_n_skins[material_idx]);

        for(int skin_idx = 0; skin_idx < material_n_skins[material_idx]; skin_idx++) {
            skeletal_material_t *material_skins = OPT_UNPACK_MEMBER(materials[material_idx], skel_model, is_packed);
            Con_Printf("\t----------\n");
            Con_Printf("\tskel_model->materials[%d][%d].texture_idx = %d\n",     material_idx, skin_idx, material_skins[skin_idx].texture_idx);
            Con_Printf("\tskel_model->materials[%d][%d].transparent = %d\n",     material_idx, skin_idx, material_skins[skin_idx].transparent);
            Con_Printf("\tskel_model->materials[%d][%d].fullbright = %d\n",      material_idx, skin_idx, material_skins[skin_idx].fullbright);
            Con_Printf("\tskel_model->materials[%d][%d].fog = %d\n",             material_idx, skin_idx, material_skins[skin_idx].fog);
            Con_Printf("\tskel_model->materials[%d][%d].shade_smooth = %d\n",    material_idx, skin_idx, material_skins[skin_idx].shade_smooth);
            Con_Printf("\tskel_model->materials[%d][%d].add_color = %d\n",       material_idx, skin_idx, material_skins[skin_idx].add_color);
            Con_Printf("\tskel_model->materials[%d][%d].add_color_red = %f\n",   material_idx, skin_idx, material_skins[skin_idx].add_color_red);
            Con_Printf("\tskel_model->materials[%d][%d].add_color_green = %f\n", material_idx, skin_idx, material_skins[skin_idx].add_color_green);
            Con_Printf("\tskel_model->materials[%d][%d].add_color_blue = %f\n",  material_idx, skin_idx, material_skins[skin_idx].add_color_blue);
            Con_Printf("\tskel_model->materials[%d][%d].add_color_alpha = %f\n", material_idx, skin_idx, material_skins[skin_idx].add_color_alpha);
            Con_Printf("\t----------\n");
        }
        Con_Printf("---------------------\n");
    }



    Con_Printf("skel_model->framegroup_n_events = %d\n", skel_model->framegroup_n_events);

    if(skel_model->framegroup_n_events != NULL) {
        uint16_t  *framegroup_n_events       = OPT_UNPACK_MEMBER(skel_model->framegroup_n_events,       skel_model, is_packed);
        float    **framegroup_event_time     = OPT_UNPACK_MEMBER(skel_model->framegroup_event_time,     skel_model, is_packed);
        uint32_t **framegroup_event_code     = OPT_UNPACK_MEMBER(skel_model->framegroup_event_code,     skel_model, is_packed);
        char    ***framegroup_event_data_str = OPT_UNPACK_MEMBER(skel_model->framegroup_event_data_str, skel_model, is_packed);

        for(int framegroup_idx = 0; framegroup_idx < skel_model->n_framegroups; framegroup_idx++) {

            Con_Printf("\tskel_model->framegroup_n_events[%d] = %d\n", framegroup_idx, framegroup_n_events[framegroup_idx]);
            uint32_t *framegroup_event_codes        = OPT_UNPACK_MEMBER(framegroup_event_code[framegroup_idx],     skel_model, is_packed);
            float    *framegroup_event_times        = OPT_UNPACK_MEMBER(framegroup_event_time[framegroup_idx],     skel_model, is_packed);
            char    **framegroup_event_data_strs    = OPT_UNPACK_MEMBER(framegroup_event_data_str[framegroup_idx], skel_model, is_packed);

            Con_Printf("\t----------\n");
            for(int event_idx = 0; event_idx < framegroup_n_events[framegroup_idx]; event_idx++) {
                char *event_data_str = OPT_UNPACK_MEMBER(framegroup_event_data_strs[event_idx], skel_model, is_packed);
                Con_Printf("\tskel_model->framegroup_event_codes[%d][%d] = {code: %d, data: \"%s\", time: %.2f}\n", 
                    framegroup_idx, event_idx, 
                    framegroup_event_codes[event_idx], event_data_str, framegroup_event_times[event_idx]
                );
            }
            Con_Printf("\t----------\n");
        }
    }
}



// 
// Util function for flattening a region of memory
// Given a region of memory `buffer` that can fit `n_bytes`
// 1. Copies `n_bytes` bytes from `src_ptr` to `*buffer_head`
// 2. Increments `*buffer_head` pointer to point to the next free region of memory
// 3. Sets `*dest_ptr` to be the memory offset of the data relative to `buffer_start`
//
// Arguments:
//  - *dest_ptr : Target pointer to copy data into and to assign as offset
//  - src_ptr : Source pointer to copy data from
//  - n_bytes : Number of bytes to copy for data
//  - buffer_start : Pointer to start of full buffer
//  - *buffer_head : Current write position of buffer
// 
// Call via `PACK_MEMBER` macro for implicit type conversions
//
void _pack_member(void **dest_ptr, void *src_ptr, size_t n_bytes, uint8_t *buffer_start, uint8_t **buffer_head) {
    // If src is NULL pointer, set dest to NULL pointer and don't do anything else
    if(src_ptr == NULL || n_bytes == 0) {
        *dest_ptr = NULL;
        return;
    }
    // Pad to `PACKING_BYTE_PADDING` bytes
    n_bytes = pad_n_bytes(n_bytes);
    memcpy(*buffer_head, src_ptr, n_bytes);
    *dest_ptr = *buffer_head;
    *buffer_head += n_bytes;

    // TODO - Add buffer_size checking to avoid overflows?

    // Make the pointer `dest_ptr` contain the offset relative to the start of the buffer
    *dest_ptr = (void*) ((uint8_t*) *dest_ptr - (uint8_t*) buffer_start);
}

// 
// Given an offset `packed_member_ptr` that contains an offset relative to `buffer_start`,
// retrieves the global pointer (i.e. buffer_start + offset,  where offset = packed_member_ptr)
//
// Call via `UNPACK_MEMBER` macro to implicitly cast return type to same type as `packed_member_ptr`
//
void *_unpack_member(void *packed_member_ptr, void *buffer_start) {
    if(packed_member_ptr == NULL) {
        return NULL;
    }
    return ((uint8_t*) buffer_start + (int) packed_member_ptr);
}










//
// Copies and packs data from `unpacked_skel_model_in` into `packed_skel_model_out`
// such that the entire model memory is packed. (i.e. all data is laid out 
// contiguously in memory, and pointers contain offsets relative to the 
// start of the memory block)
//
// NOTE - Assumes `packed_skel_model_out` is large enough to fully fit the skel_model's data
//
void pack_skeletal_model(skeletal_model_t *unpacked_skel_model_in, skeletal_model_t *packed_skel_model_out) {
    // ------------–------------–------------–------------–------------–-------
    // Memcpy each piece of the skeletal model into the corresponding section
    // ------------–------------–------------–------------–------------–-------
    // Con_Printf("make_skeletal_model_relocatable 1.1\n");
    uint8_t *buffer = (uint8_t*) packed_skel_model_out;
    uint8_t *buffer_head = buffer;
    uint8_t **buffer_head_ptr = &buffer_head;


    PACK_MEMBER(&packed_skel_model_out, unpacked_skel_model_in, sizeof(skeletal_model_t), buffer, buffer_head_ptr);

    // `packed_skel_model_out` is now the offset to the memory block relative to the 
    // buffer start, which is 0 because it's the first struct in the buffer
    // Revert the pointer `packed_skel_model_out` so we can reference its members
    packed_skel_model_out = (skeletal_model_t*) buffer;
    pack_skeletal_model_meshes(unpacked_skel_model_in, packed_skel_model_out, buffer_head_ptr);


    // -- bones --
    PACK_MEMBER(&packed_skel_model_out->bone_name,          unpacked_skel_model_in->bone_name,          sizeof(char*)   * unpacked_skel_model_in->n_bones, buffer, buffer_head_ptr);
    PACK_MEMBER(&packed_skel_model_out->bone_parent_idx,    unpacked_skel_model_in->bone_parent_idx,    sizeof(int16_t) * unpacked_skel_model_in->n_bones, buffer, buffer_head_ptr);
    PACK_MEMBER(&packed_skel_model_out->bone_rest_pos,      unpacked_skel_model_in->bone_rest_pos,      sizeof(vec3_t)  * unpacked_skel_model_in->n_bones, buffer, buffer_head_ptr);
    PACK_MEMBER(&packed_skel_model_out->bone_rest_rot,      unpacked_skel_model_in->bone_rest_rot,      sizeof(vec4_t)  * unpacked_skel_model_in->n_bones, buffer, buffer_head_ptr);
    PACK_MEMBER(&packed_skel_model_out->bone_rest_scale,    unpacked_skel_model_in->bone_rest_scale,    sizeof(vec3_t)  * unpacked_skel_model_in->n_bones, buffer, buffer_head_ptr);

    char **packed_bone_name = UNPACK_MEMBER(packed_skel_model_out->bone_name, packed_skel_model_out);
    char **unpacked_bone_name = unpacked_skel_model_in->bone_name;
    for(int bone_idx = 0; bone_idx < unpacked_skel_model_in->n_bones; bone_idx++) {
        PACK_MEMBER(&packed_bone_name[bone_idx], unpacked_bone_name[bone_idx], safe_strsize(unpacked_bone_name[bone_idx]), buffer, buffer_head_ptr);
    }


    // -- bone hitbox info --
    PACK_MEMBER(&packed_skel_model_out->bone_hitbox_enabled, unpacked_skel_model_in->bone_hitbox_enabled, sizeof(bool)     * unpacked_skel_model_in->n_bones, buffer, buffer_head_ptr);
    PACK_MEMBER(&packed_skel_model_out->bone_hitbox_ofs,     unpacked_skel_model_in->bone_hitbox_ofs,     sizeof(vec3_t)   * unpacked_skel_model_in->n_bones, buffer, buffer_head_ptr);
    PACK_MEMBER(&packed_skel_model_out->bone_hitbox_scale,   unpacked_skel_model_in->bone_hitbox_scale,   sizeof(vec3_t)   * unpacked_skel_model_in->n_bones, buffer, buffer_head_ptr);
    PACK_MEMBER(&packed_skel_model_out->bone_hitbox_tag,     unpacked_skel_model_in->bone_hitbox_tag,     sizeof(int)      * unpacked_skel_model_in->n_bones, buffer, buffer_head_ptr);


    // -- cached bone rest transforms --
    PACK_MEMBER(&packed_skel_model_out->bone_rest_transforms,     unpacked_skel_model_in->bone_rest_transforms,     sizeof(mat3x4_t) * unpacked_skel_model_in->n_bones, buffer, buffer_head_ptr);
    PACK_MEMBER(&packed_skel_model_out->inv_bone_rest_transforms, unpacked_skel_model_in->inv_bone_rest_transforms, sizeof(mat3x4_t) * unpacked_skel_model_in->n_bones, buffer, buffer_head_ptr);


    // -- frames --
    PACK_MEMBER(&packed_skel_model_out->frames_bone_pos,   unpacked_skel_model_in->frames_bone_pos,   sizeof(vec3_t) * unpacked_skel_model_in->n_bones * unpacked_skel_model_in->n_frames, buffer, buffer_head_ptr);
    PACK_MEMBER(&packed_skel_model_out->frames_bone_rot,   unpacked_skel_model_in->frames_bone_rot,   sizeof(quat_t) * unpacked_skel_model_in->n_bones * unpacked_skel_model_in->n_frames, buffer, buffer_head_ptr);
    PACK_MEMBER(&packed_skel_model_out->frames_bone_scale, unpacked_skel_model_in->frames_bone_scale, sizeof(vec3_t) * unpacked_skel_model_in->n_bones * unpacked_skel_model_in->n_frames, buffer, buffer_head_ptr);
    PACK_MEMBER(&packed_skel_model_out->frames_move_dist,  unpacked_skel_model_in->frames_move_dist,  sizeof(float)  * unpacked_skel_model_in->n_frames, buffer, buffer_head_ptr);


    // -- framegroups --
    PACK_MEMBER(&packed_skel_model_out->framegroup_name,         unpacked_skel_model_in->framegroup_name,          sizeof(char*)    * unpacked_skel_model_in->n_framegroups, buffer, buffer_head_ptr);
    PACK_MEMBER(&packed_skel_model_out->framegroup_start_frame,  unpacked_skel_model_in->framegroup_start_frame,   sizeof(uint32_t) * unpacked_skel_model_in->n_framegroups, buffer, buffer_head_ptr);
    PACK_MEMBER(&packed_skel_model_out->framegroup_n_frames,     unpacked_skel_model_in->framegroup_n_frames,      sizeof(uint32_t) * unpacked_skel_model_in->n_framegroups, buffer, buffer_head_ptr);
    PACK_MEMBER(&packed_skel_model_out->framegroup_fps,          unpacked_skel_model_in->framegroup_fps,           sizeof(float)    * unpacked_skel_model_in->n_framegroups, buffer, buffer_head_ptr);
    PACK_MEMBER(&packed_skel_model_out->framegroup_loop,         unpacked_skel_model_in->framegroup_loop,          sizeof(bool)     * unpacked_skel_model_in->n_framegroups, buffer, buffer_head_ptr);

    char **packed_framegroup_name = UNPACK_MEMBER(packed_skel_model_out->framegroup_name, packed_skel_model_out);
    char **unpacked_framegroup_name = unpacked_skel_model_in->framegroup_name;
    for(int framegroup_idx = 0; framegroup_idx < unpacked_skel_model_in->n_framegroups; framegroup_idx++) {
        PACK_MEMBER(&packed_framegroup_name[framegroup_idx], unpacked_framegroup_name[framegroup_idx], safe_strsize(unpacked_framegroup_name[framegroup_idx]), buffer, buffer_head_ptr);
    }


    // -- materials --
    PACK_MEMBER(&packed_skel_model_out->material_names,    unpacked_skel_model_in->material_names,    sizeof(char*)                * unpacked_skel_model_in->n_materials, buffer, buffer_head_ptr);
    PACK_MEMBER(&packed_skel_model_out->material_n_skins,  unpacked_skel_model_in->material_n_skins,  sizeof(int)                  * unpacked_skel_model_in->n_materials, buffer, buffer_head_ptr);
    PACK_MEMBER(&packed_skel_model_out->materials,         unpacked_skel_model_in->materials,         sizeof(skeletal_material_t*) * unpacked_skel_model_in->n_materials, buffer, buffer_head_ptr);

    char **packed_material_names = UNPACK_MEMBER(packed_skel_model_out->material_names, packed_skel_model_out);
    char **unpacked_material_names = unpacked_skel_model_in->material_names;
    
    skeletal_material_t **packed_materials = UNPACK_MEMBER(packed_skel_model_out->materials, packed_skel_model_out);
    skeletal_material_t **unpacked_materials = unpacked_skel_model_in->materials;

    for(int material_idx = 0; material_idx < unpacked_skel_model_in->n_materials; material_idx++) {
        PACK_MEMBER(&packed_material_names[material_idx],  unpacked_material_names[material_idx], safe_strsize(unpacked_material_names[material_idx]), buffer, buffer_head_ptr);
        PACK_MEMBER(&packed_materials[material_idx],       unpacked_materials[material_idx],      sizeof(skeletal_material_t) * unpacked_skel_model_in->material_n_skins[material_idx], buffer, buffer_head_ptr);
    }


    // -- FTE Anim Events --
    PACK_MEMBER(&packed_skel_model_out->framegroup_n_events,       unpacked_skel_model_in->framegroup_n_events,       sizeof(uint16_t)  * unpacked_skel_model_in->n_framegroups, buffer, buffer_head_ptr);
    PACK_MEMBER(&packed_skel_model_out->framegroup_event_time,     unpacked_skel_model_in->framegroup_event_time,     sizeof(float*)    * unpacked_skel_model_in->n_framegroups, buffer, buffer_head_ptr);
    PACK_MEMBER(&packed_skel_model_out->framegroup_event_code,     unpacked_skel_model_in->framegroup_event_code,     sizeof(uint32_t*) * unpacked_skel_model_in->n_framegroups, buffer, buffer_head_ptr);
    PACK_MEMBER(&packed_skel_model_out->framegroup_event_data_str, unpacked_skel_model_in->framegroup_event_data_str, sizeof(char**)    * unpacked_skel_model_in->n_framegroups, buffer, buffer_head_ptr);



    if(unpacked_skel_model_in->framegroup_n_events != NULL) {
        float **packed_framegroup_event_time = UNPACK_MEMBER(packed_skel_model_out->framegroup_event_time, packed_skel_model_out);
        float **unpacked_framegroup_event_time = unpacked_skel_model_in->framegroup_event_time;

        uint32_t **packed_framegroup_event_code = UNPACK_MEMBER(packed_skel_model_out->framegroup_event_code, packed_skel_model_out);
        uint32_t **unpacked_framegroup_event_code = unpacked_skel_model_in->framegroup_event_code;

        char ***packed_framegroup_event_data_str = UNPACK_MEMBER(packed_skel_model_out->framegroup_event_data_str, packed_skel_model_out);
        char ***unpacked_framegroup_event_data_str = unpacked_skel_model_in->framegroup_event_data_str;

        for(int framegroup_idx = 0; framegroup_idx < unpacked_skel_model_in->n_framegroups; framegroup_idx++) {
            int framegroup_n_events = unpacked_skel_model_in->framegroup_n_events[framegroup_idx];
            PACK_MEMBER(&packed_framegroup_event_time[framegroup_idx],     unpacked_framegroup_event_time[framegroup_idx],     sizeof(float)    * framegroup_n_events, buffer, buffer_head_ptr);
            PACK_MEMBER(&packed_framegroup_event_code[framegroup_idx],     unpacked_framegroup_event_code[framegroup_idx],     sizeof(uint32_t) * framegroup_n_events, buffer, buffer_head_ptr);
            PACK_MEMBER(&packed_framegroup_event_data_str[framegroup_idx], unpacked_framegroup_event_data_str[framegroup_idx], sizeof(char*)    * framegroup_n_events, buffer, buffer_head_ptr);

            char **packed_framegroup_frame_event_data_str =   UNPACK_MEMBER(packed_framegroup_event_data_str[framegroup_idx], packed_skel_model_out);
            char **unpacked_framegroup_frame_event_data_str =   unpacked_framegroup_event_data_str[framegroup_idx];

            for(int event_idx = 0; event_idx < unpacked_skel_model_in->framegroup_n_events[framegroup_idx]; event_idx++) {
                PACK_MEMBER(&packed_framegroup_frame_event_data_str[event_idx],  unpacked_framegroup_frame_event_data_str[event_idx], safe_strsize(unpacked_framegroup_frame_event_data_str[event_idx]), buffer, buffer_head_ptr);
            }
        }
    }
    // ------------–------------–------------–------------–------------–-------
}




/*
=================
Mod_LoadIQMModel
=================
*/
void Mod_LoadIQMModel (model_t *model, void *buffer) {



    Con_Printf("Loading IQM model: \"%s\"\n", model->name);
    // // Get a heap-allocated unpacked skeletal model (whose pointers are global addresses)
    skeletal_model_t *unpacked_skel_model = load_iqm_file(buffer);
    load_iqm_file_json_info(unpacked_skel_model, model->name);
    Con_Printf("Assign material indices to mesh...\n");
    set_unpacked_skeletal_model_mesh_material_indices(unpacked_skel_model);
    Con_Printf("Done assigning material indices to mesh...\n");


#ifdef IQMDEBUG_LOADIQM_DEBUGSUMMARY
    debug_print_skeletal_model(unpacked_skel_model, false);
#endif // IQMDEBUG_LOADIQM_DEBUGSUMMARY


#ifdef IQMDEBUG_LOADIQM_PACKING
    Con_Printf("About to calculate skeletal model size...\n");
#endif // IQMDEBUG_LOADIQM_PACKING
    size_t skel_model_n_bytes = count_unpacked_skeletal_model_n_bytes(unpacked_skel_model);
#ifdef IQMDEBUG_LOADIQM_PACKING
    Con_Printf("Done calculating skeletal model size\n");
    Con_Printf("Skeletal model size (before padding): %d\n", skel_model_n_bytes);
#endif // IQMDEBUG_LOADIQM_PACKING


    // Pad to four bytes:
    skel_model_n_bytes = pad_n_bytes(skel_model_n_bytes);

#ifdef IQMDEBUG_LOADIQM_PACKING
    Con_Printf("Skeletal model size (after padding): %d\n", skel_model_n_bytes);
#endif // IQMDEBUG_LOADIQM_PACKING

    // FIXME - Make sure every memory location is offset by two bytes? Might require modifying flattenmemory method

#ifdef IQMDEBUG_LOADIQM_PACKING
    Con_Printf("Allocating memory for skeletal model\n");
#endif // IQMDEBUG_LOADIQM_PACKING

    skeletal_model_t *packed_skel_model = (skeletal_model_t*) Hunk_AllocName(skel_model_n_bytes, loadname);

#ifdef IQMDEBUG_LOADIQM_PACKING
    Con_Printf("Done allocating memory for skeletal model\n");
#endif // IQMDEBUG_LOADIQM_PACKING

#ifdef IQMDEBUG_LOADIQM_PACKING
    Con_Printf("Packing skeletal model\n");
#endif // IQMDEBUG_LOADIQM_PACKING


    pack_skeletal_model(unpacked_skel_model, packed_skel_model);

#ifdef IQMDEBUG_LOADIQM_DEBUGSUMMARY
    debug_print_skeletal_model(packed_skel_model, true);
#endif // IQMDEBUG_LOADIQM_DEBUGSUMMARY

#ifdef IQMDEBUG_LOADIQM_PACKING
    Con_Printf("Done packing skeletal model\n");
#endif // IQMDEBUG_LOADIQM_PACKING

#ifdef IQMDEBUG_LOADIQM_PACKING
    Con_Printf("Freeing temp unpacked skeletal model...");
#endif // IQMDEBUG_LOADIQM_PACKING

    free_unpacked_skeletal_model(unpacked_skel_model);
    unpacked_skel_model = NULL;

#ifdef IQMDEBUG_LOADIQM_PACKING
    Con_Printf("DONE\n");
#endif // IQMDEBUG_LOADIQM_PACKING


#ifdef IQM_BBOX_PER_MODEL_PER_ANIM
    // These functions are also used at runtime, and so operate on packed skeletal models
    Con_Printf("About to calculate rest pose mins / maxs\n");
    calc_skel_model_bounds_for_rest_pose(packed_skel_model, packed_skel_model->rest_pose_bone_hitbox_mins, packed_skel_model->rest_pose_bone_hitbox_maxs);
    Con_Printf("Done calculating rest pose mins / maxs:\n");
    Con_Printf("\tmins: [%f, %f, %f]\n", packed_skel_model->rest_pose_bone_hitbox_mins[0], packed_skel_model->rest_pose_bone_hitbox_mins[1], packed_skel_model->rest_pose_bone_hitbox_mins[2]);
    Con_Printf("\tmaxs: [%f, %f, %f]\n", packed_skel_model->rest_pose_bone_hitbox_maxs[0], packed_skel_model->rest_pose_bone_hitbox_maxs[1], packed_skel_model->rest_pose_bone_hitbox_maxs[2]);
    Con_Printf("About to calculate anim pose mins / maxs\n");
    calc_skel_model_bounds_for_anim(packed_skel_model, packed_skel_model, packed_skel_model->anim_bone_hitbox_mins, packed_skel_model->anim_bone_hitbox_maxs);
    Con_Printf("Done calculating anim pose mins / maxs:\n");
    Con_Printf("\tmins: [%f, %f, %f]\n", packed_skel_model->anim_bone_hitbox_mins[0], packed_skel_model->anim_bone_hitbox_mins[1], packed_skel_model->anim_bone_hitbox_mins[2]);
    Con_Printf("\tmaxs: [%f, %f, %f]\n", packed_skel_model->anim_bone_hitbox_maxs[0], packed_skel_model->anim_bone_hitbox_maxs[1], packed_skel_model->anim_bone_hitbox_maxs[2]);
#endif // IQM_BBOX_PER_MODEL_PER_ANIM


    model->type = mod_iqm;

#ifdef IQMDEBUG_LOADIQM_PACKING
    Con_Printf("Copying final skeletal model struct... ");
#endif // IQMDEBUG_LOADIQM_PACKING

    if(!model->cache.data) {
        Cache_Alloc(&model->cache, skel_model_n_bytes, loadname);
    }
    if(!model->cache.data) {
        return;
    }

    memcpy_vfpu(model->cache.data, (void*) packed_skel_model, skel_model_n_bytes);

#ifdef IQMDEBUG_LOADIQM_PACKING
    Con_Printf("DONE\n");
#endif // IQMDEBUG_LOADIQM_PACKING
}







void sv_skel_clear_all() {
    for(int i = 0; i < MAX_SKELETONS; i++) {
        sv_skeletons[i].in_use = false;
        sv_skeletons[i].modelindex = 0;
        sv_skeletons[i].model = NULL;
        sv_skeletons[i].anim_modelindex = 0;
        sv_skeletons[i].anim_framegroup = 0;
        sv_skeletons[i].anim_starttime = 0.0f;
        sv_skeletons[i].anim_speed = 0.0f;
        sv_skeletons[i].last_build_time = 0.0f;
        sv_skeletons[i].anim_model = NULL;

        // Free malloc-ed memory
        free_pointer_and_clear( (void**) &(sv_skeletons[i].bone_hitbox_enabled));
        free_pointer_and_clear( (void**) &(sv_skeletons[i].bone_hitbox_ofs));
        free_pointer_and_clear( (void**) &(sv_skeletons[i].bone_hitbox_scale));
        free_pointer_and_clear( (void**) &(sv_skeletons[i].bone_hitbox_tag));
        free_pointer_and_clear( (void**) &(sv_skeletons[i].bone_transforms));
        free_pointer_and_clear( (void**) &(sv_skeletons[i].bone_rest_to_pose_transforms));
        free_pointer_and_clear( (void**) &(sv_skeletons[i].anim_bone_idx));
#ifdef IQM_LOAD_NORMALS
        free_pointer_and_clear( (void**) &(sv_skeletons[i].bone_rest_to_pose_normal_transforms));
#endif // IQM_LOAD_NORMALS

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
bool sv_get_skeleton_bounds(int skel_idx, vec3_t mins, vec3_t maxs) {
    if(skel_idx < 0 || skel_idx >= MAX_SKELETONS) {
        return false;
    }

    if(sv_skeletons[skel_idx].in_use == false) {
        return false;
    }
    int skel_modelindex = sv_skeletons[skel_idx].modelindex;
    int anim_modelindex = sv_skeletons[skel_idx].anim_modelindex;
    sv_get_skel_model_bounds( skel_modelindex, anim_modelindex, mins, maxs);
    return true;
}






char *sv_skel_get_bonename(int skel_idx, int bone_idx) {
    if(skel_idx < 0 || skel_idx >= MAX_SKELETONS) {
        return NULL;
    }

    if(sv_skeletons[skel_idx].in_use == false) {
        return NULL;
    }

    if(bone_idx < 0 || bone_idx >= sv_skeletons[skel_idx].model->n_bones) {
        return NULL;
    }
    
    skeletal_skeleton_t *sv_skeleton = &(sv_skeletons[skel_idx]);
    char **bone_names = UNPACK_MEMBER(sv_skeleton->model->bone_name, sv_skeleton->model);
    char *bone_name = UNPACK_MEMBER(bone_names[bone_idx], sv_skeleton->model);
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
    int16_t *bone_parent_idxs = UNPACK_MEMBER(sv_skeleton->model->bone_parent_idx, sv_skeleton->model);
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
    char **bone_names = UNPACK_MEMBER(sv_skeleton->model->bone_name, sv_skeleton->model);

    for(int i = 0; i < sv_skeleton->model->n_bones; i++) {
        char *cur_bone_name = UNPACK_MEMBER(bone_names[i], sv_skeleton->model);
        if(!strcmp( cur_bone_name, bone_name)) {
            bone_idx = i;
            break;
        }
    }
    return bone_idx;
}







//
// Returns the model index for a given model name, if it exists
// If it doesn't exist, or isn't precached, index 0 is returned.
// NOTE: Index 0 technically corresponds to the world bsp model. But for the 
// NOTE  context of this function, it indicates an invalid value, for compat
// NOTE  with FTE's convention
//
void PF_getmodelindex(void) {
    Con_Printf("PF_getmodelindex start\n");
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
    Con_Printf("PF_getmodelindex end\n");
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
    if(anim_skel_model == NULL) {
        return;
    }

    if(anim_framegroup_idx < 0 || anim_framegroup_idx >= anim_skel_model->n_framegroups) {
        return;
    }

    uint32_t *framegroup_start_frame = UNPACK_MEMBER(anim_skel_model->framegroup_start_frame, anim_skel_model);
    uint32_t *framegroup_n_frames = UNPACK_MEMBER(anim_skel_model->framegroup_n_frames, anim_skel_model);
    float *framegroup_fps = UNPACK_MEMBER(anim_skel_model->framegroup_fps, anim_skel_model);
    bool *framegroup_loop = UNPACK_MEMBER(anim_skel_model->framegroup_loop, anim_skel_model);

    // Movement distance for all frames (cumsum-ed within each framegroup)
    float *frames_move_dist = UNPACK_MEMBER(anim_skel_model->frames_move_dist, anim_skel_model);
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
    Con_Printf("PF_skeleton_create start\n");
    int skel_idx = -1;
    int skel_model_idx = G_FLOAT(OFS_PARM0);

    // Con_Printf("PF_skeleton_create: For modelindex: %d\n", skel_model_idx);
    skeletal_model_t *skel_model = sv_get_skel_model_for_modelidx(skel_model_idx);
    if(skel_model == NULL) {
        G_FLOAT(OFS_RETURN) = skel_idx;
        Con_Printf("PF_skeleton_create: Invalid IQM model\n");
        return;
    }
    if(skel_model->n_meshes <= 0) {
        // IQM Models with no meshes do not have valid skeleton rest pose data
        G_FLOAT(OFS_RETURN) = skel_idx;
        Con_Printf("PF_skeleton_create: Specified IQM model has no skeleton\n");
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
    Con_Printf("PF_skeleton_create (skel_idx: %d)\n", skel_idx);
    G_FLOAT(OFS_RETURN) = skel_idx;
    Con_Printf("PF_skeleton_create end\n");
}

//
// Clears the skeleton state buffer at that index
//
void PF_skeleton_destroy(void) {
    Con_Printf("PF_skeleton_destroy start\n");
    int skel_idx = G_FLOAT(OFS_PARM0);
    if(skel_idx < 0 || skel_idx >= MAX_SKELETONS) {
        return;
    }
    if(sv_skeletons[skel_idx].in_use) {
        sv_skeletons[skel_idx].in_use = false;
        free_skeleton(&(sv_skeletons[skel_idx]));
    }
    Con_Printf("PF_skeleton_destroy (skel_idx: %d)\n", skel_idx);
    Con_Printf("PF_skeleton_destroy end\n");
}


//
// Sets the skeleton state at some index and builds required transform matrices.
//
void PF_skeleton_build(void) {
    Con_Printf("PF_skeleton_build start\n");
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
    if(anim_skel_model == NULL) {
        Con_Printf("PF_skeleton_build: Invalid skeletal animation model\n");
        return;
    }

    // Con_Printf("PF_skeleton_build: skel_idx: %d, skel model: %d, skel anim: %d\n", skel_idx, sv_skeleton->modelindex, sv_skeleton->anim_modelindex);

    // TODO - Add skeleton-owning entity as an arg to this function
    // TODO - Update the entity's skeleton state variables so they are networked to clients

    build_skeleton_for_start_time(sv_skeleton, anim_skel_model, anim_framegroup_idx, anim_starttime, anim_speed);
    sv_skeleton->anim_modelindex = anim_modelindex;
    Con_Printf("PF_skeleton_build end\n");
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
//      true: if the entity has a valid skeleton that we checked against, and traceline code should skip normal entity AABB collision detection
//      false: if the entity did not have a valid skeleton, and traceline code should resume normal entity AABB collision detection
//
bool sv_intersect_skeletal_model_ent(edict_t *ent, vec3_t ray_start, vec3_t ray_end, float *trace_fraction, int *trace_bone_idx, int *trace_bone_hitbox_tag) {
    if(ent == NULL) {
        // No ent
        // Revert to normal ent traceline collision detection
        // Con_Printf("sv_intersect_skeletal_model: ent invalid.\n");
        return false;
    }
    int sv_skel_idx = (int) ent->v.skeletonindex;
    // Con_Printf("sv_intersect_skeletal_model for ent (skelidx: %d) (t=%.2f)\n", sv_skel_idx, sv.time);
    // Con_Printf("sv_intersect_skeletal_model for ent: \"%s\" (skelidx: %d) (t=%.2f)\n", pr_strings + ent->v.classname, sv_skel_idx, sv.time);
    if(sv_skel_idx < 0 || sv_skel_idx >= MAX_SKELETONS) {
        // No skeleton
        // Revert to normal ent traceline collision detection
        Con_Printf("sv_intersect_skeletal_model: skelidx %d invalid.\n", sv_skel_idx);
        return false;
    }

    skeletal_skeleton_t *sv_skeleton = &(sv_skeletons[sv_skel_idx]);
    if(!sv_skeleton->in_use) {
        Con_Printf("sv_intersect_skeletal_model: skelidx %d not in use.\n", sv_skel_idx);
        // Skeleton exists, but has not been created properly
        // Revert to normal ent traceline collision detection
        return false;
    }

    if(sv_skeleton->model == NULL || sv_skeleton->anim_model == NULL) {
        Con_Printf("sv_intersect_skeletal_model: skelidx %d has no model or anim_model.\n", sv_skel_idx);
        // Skeleton exists and was created, but has not been built
        // Revert to normal ent traceline collision detection
        return false;
    }
    if( sv_skeleton->bone_transforms == NULL || sv_skeleton->bone_hitbox_enabled == NULL || sv_skeleton->bone_hitbox_ofs == NULL || 
        sv_skeleton->bone_hitbox_scale == NULL || sv_skeleton->bone_hitbox_tag == NULL) {
        Con_Printf("sv_intersect_skeletal_model: skelidx %d is missing bone data.\n", sv_skel_idx);
        // Skeleton exists and was created, but was not built correctly
        // Revert to normal ent traceline collision detection
        return false;
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
        return true;
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
    bool skel_hitbox_hit = ray_aabb_intersection(ray_start_skel_hitbox_space, ray_ofs_skel_hitbox_space, &skel_hitbox_trace_fraction);

    // If skeleton hitbox wasn't hit, or its trace_fraction is larger than current, stop
    if(!skel_hitbox_hit || skel_hitbox_trace_fraction > *trace_fraction) {
        // Con_Printf("sv_intersect_skeletal_model: worldAABB: 1  modelAABB: 0\n");
        // Con_Printf("sv_intersect_skeletal_model: skelidx %d traceline missed model-space AABB.\n", sv_skel_idx);
        // Con_Printf("\ttracelint hit: %d\n", skel_hitbox_hit);
        // Con_Printf("\thitbox trace_fraction: %f, current: %f\n", skel_hitbox_trace_fraction, *trace_fraction);

        // Indicate that normal traceline checking code should be skipped for this ent
        return true;
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
    // char **bone_names = UNPACK_MEMBER(sv_skeleton->model->bone_name, sv_skeleton->model);
    // bool *bone_hitbox_enabled = UNPACK_MEMBER(sv_skeleton->model->bone_hitbox_enabled, sv_skeleton->model);
    // vec3_t *bone_hitbox_ofs = UNPACK_MEMBER(sv_skeleton->model->bone_hitbox_ofs, sv_skeleton->model);
    // vec3_t *bone_hitbox_scale = UNPACK_MEMBER(sv_skeleton->model->bone_hitbox_scale, sv_skeleton->model);
    // int *bone_hitbox_tag = UNPACK_MEMBER(sv_skeleton->model->bone_hitbox_tag, sv_skeleton->model);


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
        bool bbox_hit = ray_aabb_intersection(ray_start_bone_space, ray_ofs_bone_space, &cur_bone_tmin);

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

        // char *bone_name = UNPACK_MEMBER(bone_names[closest_hit_bone_idx], sv_skeleton->model);
        // Con_Printf("%d (\"%s\"), ", closest_hit_bone_idx, bone_name);
        // Con_Printf("\n");
    }
    // ------------------------------------------------------------------------
    Con_Printf("sv_intersect_skeletal_model: skelidx %d reached end of control.\n", sv_skel_idx);

    // Return true to skip normal ent traceline collision detection
    return true;
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
    if(bone_name == NULL || bone_name_len < 0) {
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
    if(sv_skeletons[skel_idx].bone_hitbox_enabled == NULL) {
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
        if(sv_skeletons[skel_idx].bone_hitbox_tag == NULL) {
            return;
        }
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
    if(sv_skeletons[skel_idx].bone_hitbox_ofs == NULL) {
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
        if(sv_skeletons[skel_idx].bone_hitbox_tag == NULL) {
            return;
        }
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
    if(sv_skeletons[skel_idx].bone_hitbox_scale == NULL) {
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
        if(sv_skeletons[skel_idx].bone_hitbox_tag == NULL) {
            return;
        }
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
    if(sv_skeletons[skel_idx].bone_hitbox_tag == NULL) {
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
    if(anim_skel_model == NULL) {
        Con_Printf("PF_skeleton_build: Invalid skeletal animation model\n");
        G_FLOAT(OFS_RETURN) = 0.0f;
        return;
    }

    if(anim_framegroup_idx < 0 || anim_framegroup_idx >= anim_skel_model->n_framegroups) {
        G_FLOAT(OFS_RETURN) = 0.0f;
        return;
    }

    uint32_t *framegroup_n_frames = UNPACK_MEMBER(anim_skel_model->framegroup_n_frames, anim_skel_model);
    float *framegroup_fps = UNPACK_MEMBER(anim_skel_model->framegroup_fps, anim_skel_model);


    float anim_duration = framegroup_n_frames[anim_framegroup_idx] / framegroup_fps[anim_framegroup_idx];
    G_FLOAT(OFS_RETURN) = anim_duration;
    // Con_Printf("PF_getframeduration end\n");
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
    if(event_data == NULL || event_data_len < 0) {
        event_data_len = 0;
    }
    strncpy(pr_string_temp, event_data, event_data_len);
    // Null-terminate the string
    pr_string_temp[event_data_len] = 0;
    // Con_Printf("\ttempstring content: \"%s\"\n", pr_string_temp);
    G_INT(OFS_PARM2) = pr_string_temp - pr_strings;
    // ------------------------------------


    // TODO - Set context entity so "self" / time is correct
    // pr_global_struct->time = sv.time;
	// pr_global_struct->self = EDICT_TO_PROG(sv_player);
    PR_ExecuteProgram(callback_func);
    // Con_Printf("\tafter -- tempstring content: \"%s\"\n", pr_string_temp);
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
    if(anim_skel_model == NULL) {
        return;
    }

    if(anim_framegroup_idx < 0 || anim_framegroup_idx >= anim_skel_model->n_framegroups) {
        return;
    }

    // Get animation events data
    uint16_t *framegroup_n_events = UNPACK_MEMBER(anim_skel_model->framegroup_n_events, anim_skel_model);
    if(framegroup_n_events == NULL) {
        return;
    }
    uint16_t anim_n_events = framegroup_n_events[anim_framegroup_idx];
    if(anim_n_events <= 0) {
        return;
    }
    float **framegroup_event_time = UNPACK_MEMBER(anim_skel_model->framegroup_event_time, anim_skel_model);
    uint32_t **framegroup_event_code = UNPACK_MEMBER(anim_skel_model->framegroup_event_code, anim_skel_model);
    char ***framegroup_event_data_str = UNPACK_MEMBER(anim_skel_model->framegroup_event_data_str, anim_skel_model);
    // --
    float *anim_event_times = UNPACK_MEMBER(framegroup_event_time[anim_framegroup_idx], anim_skel_model);
    uint32_t *anim_event_codes = UNPACK_MEMBER(framegroup_event_code[anim_framegroup_idx], anim_skel_model);
    char **anim_event_data_strs = UNPACK_MEMBER(framegroup_event_data_str[anim_framegroup_idx], anim_skel_model);

    // Get animation playback data
    uint32_t *framegroup_n_frames = UNPACK_MEMBER(anim_skel_model->framegroup_n_frames, anim_skel_model);
    float *framegroup_fps = UNPACK_MEMBER(anim_skel_model->framegroup_fps, anim_skel_model);
    bool *framegroup_loop = UNPACK_MEMBER(anim_skel_model->framegroup_loop, anim_skel_model);
    float anim_duration = framegroup_n_frames[anim_framegroup_idx] / framegroup_fps[anim_framegroup_idx]; // Animation duration in seconds
    bool anim_looped = framegroup_loop[anim_framegroup_idx];

    if(anim_looped == false) {
        // Iterate through this framegroup's events, dispatching the callback for any events in the search window
        for(int event_idx = 0; event_idx < anim_n_events; event_idx++) {
            float event_time = anim_event_times[event_idx];
            if(event_time >= t_start && event_time <= t_stop) {
                uint32_t event_code = anim_event_codes[event_idx];
                char *event_data_str = UNPACK_MEMBER(anim_event_data_strs[event_idx], anim_skel_model);
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
                char *event_data_str = UNPACK_MEMBER(anim_event_data_strs[event_idx], anim_skel_model);
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
