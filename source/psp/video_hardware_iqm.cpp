extern"C" {
    #include "../quakedef.h"
}

#include "video_hardware_iqm.h"


//
// Parses an IQM Vertex array and converts all values to floats
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
skeletal_model_t *load_iqm_file(void *iqm_data) {
    const iqm_header_t *iqm_header = (const iqm_header_t*) iqm_data;

    if(memcmp(iqm_header->magic, IQM_MAGIC, sizeof(iqm_header->magic))) {
        return nullptr; 
    }
    if(iqm_header->version != IQM_VERSION_2) {
        return nullptr;
    }

    // We won't know what the `iqm_data` buffer length is, but this is a useful check
    // if(iqm_header->filesize != file_len) {
    //     return nullptr;
    // }
    size_t file_len = iqm_header->filesize;

    const iqm_vert_array_t *iqm_verts_pos = nullptr;
    const iqm_vert_array_t *iqm_verts_uv = nullptr;
    const iqm_vert_array_t *iqm_verts_nor = nullptr;
    const iqm_vert_array_t *iqm_verts_tan = nullptr;
    const iqm_vert_array_t *iqm_verts_color = nullptr;
    const iqm_vert_array_t *iqm_verts_bone_idxs = nullptr;
    const iqm_vert_array_t *iqm_verts_bone_weights = nullptr;


    Con_Printf("\tParsing vert attribute arrays\n");

    const iqm_vert_array_t *vert_arrays = (const iqm_vert_array_t*)((uint8_t*) iqm_data + iqm_header->ofs_vert_arrays);
    for(unsigned int i = 0; i < iqm_header->n_vert_arrays; i++) {
        if((iqm_vert_array_type) vert_arrays[i].type == iqm_vert_array_type::IQM_VERT_POS) {
            iqm_verts_pos = &vert_arrays[i];
        }
        else if((iqm_vert_array_type) vert_arrays[i].type == iqm_vert_array_type::IQM_VERT_UV) {
            iqm_verts_uv = &vert_arrays[i];
        }
        else if((iqm_vert_array_type) vert_arrays[i].type == iqm_vert_array_type::IQM_VERT_NOR) {
            iqm_verts_nor = &vert_arrays[i];
        }
        else if((iqm_vert_array_type) vert_arrays[i].type == iqm_vert_array_type::IQM_VERT_TAN) {
            // Only use tangents if float and if each tangent is a 4D vector:
            if((iqm_dtype) vert_arrays[i].format == iqm_dtype::IQM_DTYPE_FLOAT && vert_arrays[i].size == 4) {
                iqm_verts_tan = &vert_arrays[i];
            }
            else {
                Con_Printf("Warning: IQM vertex normals array (idx: %d, type: %d, fmt: %d, size: %d) is not 4D float array.\n", i, vert_arrays[i].type, vert_arrays[i].format, vert_arrays[i].size);
            }
        }
        else if((iqm_vert_array_type) vert_arrays[i].type == iqm_vert_array_type::IQM_VERT_COLOR) {
            iqm_verts_color = &vert_arrays[i];
        }
        else if((iqm_vert_array_type) vert_arrays[i].type == iqm_vert_array_type::IQM_VERT_BONE_IDXS) {
            iqm_verts_bone_idxs = &vert_arrays[i];
        }
        else if((iqm_vert_array_type) vert_arrays[i].type == iqm_vert_array_type::IQM_VERT_BONE_WEIGHTS) {
            iqm_verts_bone_weights = &vert_arrays[i];
        }
        else {
            Con_Printf("Warning: Unrecognized IQM vertex array type (idx: %d, type: %d, fmt: %d, size: %d)\n", i, vert_arrays[i].type, vert_arrays[i].format, vert_arrays[i].size);
        }
    }

    Con_Printf("\tCreating skeletal model object\n");
    skeletal_model_t *skel_model = (skeletal_model_t*) malloc(sizeof(skeletal_model_t));
    skel_model->n_meshes = iqm_header->n_meshes;
    skel_model->meshes = (skeletal_mesh_t*) malloc(sizeof(skeletal_mesh_t) * skel_model->n_meshes);

    // FIXME - Should we even store these? Should we automatically convert them down?
    vec2_t *verts_uv = (vec2_t*) malloc(sizeof(vec2_t) * iqm_header->n_verts);
    vec3_t *verts_pos = (vec3_t*) malloc(sizeof(vec3_t) * iqm_header->n_verts);
    vec3_t *verts_nor = (vec3_t*) malloc(sizeof(vec3_t) * iqm_header->n_verts);


    // ------------------------------------------------------------------------
    // Convert verts_pos / verts_uv datatypes to floats 
    // ------------------------------------------------------------------------
    vec3_t default_vert = {0,0,0};
    vec2_t default_uv = {0,0};
    vec3_t default_nor = {0,0,1.0f};

    Con_Printf("\tParsing vertex attribute arrays\n");
    Con_Printf("Verts pos: %d\n", iqm_verts_pos);
    Con_Printf("Verts uv: %d\n", iqm_verts_uv);
    Con_Printf("Verts nor: %d\n", iqm_verts_nor);
    Con_Printf("Verts bone weights: %d\n", iqm_verts_bone_weights);
    Con_Printf("Verts bone idxs: %d\n", iqm_verts_bone_idxs);
    iqm_parse_float_array(iqm_data, iqm_verts_pos, (float*) verts_pos, iqm_header->n_verts, 3, (float*) &default_vert);
    iqm_parse_float_array(iqm_data, iqm_verts_uv,  (float*) verts_uv,  iqm_header->n_verts, 2, (float*) &default_uv);
    iqm_parse_float_array(iqm_data, iqm_verts_nor, (float*) verts_nor, iqm_header->n_verts, 3, (float*) &default_nor);

    float *verts_bone_weights = (float*)   malloc(sizeof(float) * 4 * iqm_header->n_verts);
    uint8_t *verts_bone_idxs  = (uint8_t*) malloc(sizeof(uint8_t) * 4 * iqm_header->n_verts);
    float default_bone_weights[] = {0.0f, 0.0f, 0.0f, 0.0f};
    iqm_parse_float_array(iqm_data, iqm_verts_bone_weights, verts_bone_weights,  iqm_header->n_verts, 4, (float*) &default_bone_weights);
    iqm_parse_uint8_array(iqm_data, iqm_verts_bone_idxs,    verts_bone_idxs,     iqm_header->n_verts, 4, (uint8_t) std::min( (int) iqm_header->n_joints, (int) IQM_MAX_BONES));

    // TODO - Parse / set other vertex attributes we care about:
    // - vertex normals <-  iqm_verts_nor
    // - vertex tangents <- iqm_verts_tan
    // - vertex colors <-   iqm_verts_color
    const iqm_mesh_t *iqm_meshes = (const iqm_mesh_t*)((uint8_t*) iqm_data + iqm_header->ofs_meshes);

    for(uint32_t i = 0; i < iqm_header->n_meshes; i++) {
        const char *material_name = (const char*) (((uint8_t*) iqm_data + iqm_header->ofs_text) + iqm_meshes[i].material);
        // Con_Printf("Mesh[%d]: \"%s\"\n", i, material_name);

        uint32_t first_vert = iqm_meshes[i].first_vert_idx;
        uint32_t first_tri = iqm_meshes[i].first_tri;
        // skel_model->meshes[i].first_vert = first_vert;
        uint32_t n_verts = iqm_meshes[i].n_verts;
        skel_model->meshes[i].n_verts = n_verts;

        skel_model->meshes[i].verts = (vertex_t*) malloc(sizeof(vertex_t) * n_verts);
        for(uint32_t j = 0; j < skel_model->meshes[i].n_verts; j++) {
            // TODO - Write other constant vertex attributes
        }

        skel_model->meshes[i].n_tris = iqm_meshes[i].n_tris;
        skel_model->meshes[i].tri_verts = (uint16_t*) malloc(sizeof(uint16_t) * 3 * skel_model->meshes[i].n_tris);

        for(uint32_t j = 0; j < skel_model->meshes[i].n_tris; j++) {
            uint16_t vert_a = ((iqm_tri_t*)((uint8_t*) iqm_data + iqm_header->ofs_tris))[first_tri + j].vert_idxs[0] - first_vert;
            uint16_t vert_b = ((iqm_tri_t*)((uint8_t*) iqm_data + iqm_header->ofs_tris))[first_tri + j].vert_idxs[1] - first_vert;
            uint16_t vert_c = ((iqm_tri_t*)((uint8_t*) iqm_data + iqm_header->ofs_tris))[first_tri + j].vert_idxs[2] - first_vert;
            skel_model->meshes[i].tri_verts[j*3 + 0] = vert_a;
            skel_model->meshes[i].tri_verts[j*3 + 1] = vert_b;
            skel_model->meshes[i].tri_verts[j*3 + 2] = vert_c;
        }
    }

    free(verts_pos);
    free(verts_uv);
    free(verts_nor);
    free(verts_bone_weights);
    free(verts_bone_idxs);

    return skel_model;
}

/*
=================
Mod_LoadIQMModel
=================
*/
extern char loadname[];
void Mod_LoadIQMModel (model_t *model, void *buffer) {

    skeletal_model_t *skel_model = load_iqm_file(buffer);
    Con_Printf("About to cache skeletal model struct... \n");

    uint32_t skel_model_n_bytes = 0;
    // ------------–------------–------------–------------–------------–-------
    // Calculate the total size of the skeletal model:
    // ------------–------------–------------–------------–------------–-------
    Con_Printf("Calculating total skeletal model size... ");
    skel_model_n_bytes += sizeof(skeletal_model_t);
    skel_model_n_bytes += sizeof(skeletal_mesh_t) * skel_model->n_meshes;
    skel_model_n_bytes += sizeof(skeletal_mesh_t) * skel_model->n_meshes;
    for(int i = 0; i < skel_model->n_meshes; i++) {
        skel_model_n_bytes += sizeof(vertex_t) * skel_model->meshes[i].n_verts;
        skel_model_n_bytes += sizeof(uint16_t) * skel_model->meshes[i].n_tris * 3;
    }
    Con_Printf("DONE\n");
    // ------------–------------–------------–------------–------------–-------

    // ------------–------------–------------–------------–------------–-------
    // Allocate enough memory for fit the entire skeletal model
    // ------------–------------–------------–------------–------------–-------
    Con_Printf("Allocating contiguous memory for skeletal model... ");
    // skeletal_model_t *contiguous_skel_model = (skeletal_model_t*) malloc(skel_model_n_bytes); // TODO - Pad to 2-byte alignment?
    skeletal_model_t *contiguous_skel_model = (skeletal_model_t*) Hunk_AllocName(skel_model_n_bytes, loadname); // TODO - Pad to 2-byte alignment?
    Con_Printf("DONE\n");
    // ------------–------------–------------–------------–------------–-------

    // ------------–------------–------------–------------–------------–-------
    // Memcpy each piece of the skeletal model into the corresponding section
    // ------------–------------–------------–------------–------------–-------
    Con_Printf("Memcopying skeletal_model_t struct... ");
    uint8_t *ptr = (uint8_t*) contiguous_skel_model;
    memcpy(contiguous_skel_model, skel_model, sizeof(skeletal_model_t));
    Con_Printf("DONE\n");
    ptr += sizeof(skeletal_model_t);
    Con_Printf("Memcopying skeletal_mesh_t array... ");
    contiguous_skel_model->meshes = (skeletal_mesh_t*) ptr;
    memcpy(contiguous_skel_model->meshes, skel_model->meshes, sizeof(skeletal_mesh_t) * skel_model->n_meshes);
    Con_Printf("DONE\n");
    ptr += sizeof(skeletal_mesh_t) * skel_model->n_meshes;

    for(int i = 0; i < skel_model->n_meshes; i++) {
        Con_Printf("Memcopying vertex_t array for mesh %d ... ", i);
        contiguous_skel_model->meshes[i].verts = (vertex_t*) ptr;
        memcpy(contiguous_skel_model->meshes[i].verts, skel_model->meshes[i].verts, sizeof(vertex_t) * contiguous_skel_model->meshes[i].n_verts);
        Con_Printf("DONE\n");
        ptr += sizeof(vertex_t) * contiguous_skel_model->meshes[i].n_verts;
        
        Con_Printf("Memcopying tri_verts uint16_t array for mesh %d ... ", i);
        contiguous_skel_model->meshes[i].tri_verts = (uint16_t*) ptr;
        memcpy(contiguous_skel_model->meshes[i].tri_verts, skel_model->meshes[i].tri_verts, sizeof(uint16_t) * contiguous_skel_model->meshes[i].n_tris * 3);
        Con_Printf("DONE\n");
        ptr += sizeof(uint16_t) * contiguous_skel_model->meshes[i].n_tris * 3;
    }
    // ------------–------------–------------–------------–------------–-------

    // ------------–------------–------------–------------–------------–-------
    // Clean up all pointers to be relative to model start location in memory
    // ------------–------------–------------–------------–------------–-------
    for(int i = 0; i < skel_model->n_meshes; i++) {
        Con_Printf("Shifting vertex_t array pointer for mesh %d ... ", i);
        contiguous_skel_model->meshes[i].verts = (vertex_t*) ((uint8_t*)(contiguous_skel_model->meshes[i].verts) - (uint8_t*)contiguous_skel_model);
        Con_Printf("DONE\n");
        Con_Printf("Shifting tri_verts uint16_t array pointer for mesh %d ... ", i);
        contiguous_skel_model->meshes[i].tri_verts = (uint16_t*) ((uint8_t*)(contiguous_skel_model->meshes[i].tri_verts) - (uint8_t*)contiguous_skel_model);
        Con_Printf("DONE\n");
    }
    Con_Printf("Shifting skeletal_mesh_t array pointer... ");
    contiguous_skel_model->meshes = (skeletal_mesh_t*) ((uint8_t*)(contiguous_skel_model->meshes) - (uint8_t*)contiguous_skel_model);
    Con_Printf("DONE\n");
    // ------------–------------–------------–------------–------------–-------

    model->type = mod_iqm;
    Con_Printf("Copying final skeletal model struct... ");
    if (!model->cache.data)
        Cache_Alloc(&model->cache, skel_model_n_bytes, loadname);
    if (!model->cache.data)
        return;

    memcpy_vfpu(model->cache.data, (void*) contiguous_skel_model, skel_model_n_bytes);
    Con_Printf("DONE\n");

    Con_Printf("About to free temp skeletal model struct...");
    // Completely clean up the temporary skeletal model:
    for(int i = 0; i < skel_model->n_meshes; i++) {
        free(skel_model->meshes[i].tri_verts);
        free(skel_model->meshes[i].verts);
    }
    free(skel_model->meshes);
    free(skel_model);
    Con_Printf("DONE\n");
}



void R_DrawIQMModel(entity_t *ent) {
    // TODO
    Con_Printf("IQM model draw!\n");
}

