// ..

// MARK: Global

void Fog_SetupFrame(bool is_world_geometry);
void Fog_Init(void);
void Fog_ParseWorldspawn(void);
void Fog_ParseServerMessage(void);
void Fog_EnableGFog(void);
void Fog_DisableGFog(void);
void Fog_SetColorForSkyS(void);
void Fog_SetColorForSkyE(void);

// MARK: Platform-specific

void Platform_Fog_Init(void);
void Platform_Fog_Disable(void);
void Platform_Fog_Enable(void);
void Platform_Fog_Set(bool is_world_geometry, float start, float end, float red, float green, float blue, float alpha);