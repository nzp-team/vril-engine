// Function declarations that are referenced from C code
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


