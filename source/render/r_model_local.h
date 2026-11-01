#ifndef R_MODEL_LOCAL_H
#define R_MODEL_LOCAL_H

#ifdef __cplusplus
extern "C" {
#endif

extern const qboolean mod_loading_progress, mod_external_sprites, mod_permanent_sprites;
extern const int mod_lighting_channels;
texture_t *Mod_CreateTexture(miptex_t *texture);
void Mod_LoadModelLighting(lump_t *lump);
void Mod_LoadAliasModel(model_t *model, void *data);
void Mod_ClearModelStorage(void);
void Mod_InitializeBrushStorage(void);
void Mod_PreparePlanes(void);
void Mod_PrepareNode(mnode_t *node, int plane);
void Mod_PrepareSurface(msurface_t *surface, int plane);
extern model_t *loadmodel;
extern char loadname[MAX_QPATH];
extern byte *mod_base;
extern cvar_t gl_subdivide_size, r_loadq3models;
void Mod_LoadLighting(lump_t *lump);
float RadiusFromBounds(vec3_t mins, vec3_t maxs);
byte *Mod_LumpData(lump_t *lump);
void Mod_ReadLump(lump_t *lump, void *destination);
void Mod_LoadBrushModel(model_t *model, void *data);

mspriteframe_t *Mod_LoadSpritePixels(const char *name, int width, int height, int bytes, const void *pixels);
void *Mod_LoadAllSkins(int count, daliasskintype_t *data);
void R_BuildAliasMesh(model_t *model, aliashdr_t *header);
void Mod_FloodFillSkin(byte *skin, int width, int height);
#ifdef __cplusplus
}
#endif
#endif
