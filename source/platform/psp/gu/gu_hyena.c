#include "../../../nzportable_def.h"
#include <pspgu.h>
#include <pspgum.h>

// MARK: Vril Graphics Wrapper

static int current_vertex_mode;

void
Hyena_SetTextureMode(int texture_mode)
{
    switch (texture_mode) {
        case HYE_MODULATE:
            sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGBA);
            break;
        case HYE_REPLACE:
            sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
            break;
        default:
            Sys_Error("Received unknown texture mode [%d]\n", texture_mode);
            break;
    }
}

void
Hyena_SetColor(float red, float green, float blue, float alpha)
{
    sceGuColor(GU_COLOR((int) (red * 255.0f), (int) (green * 255.0f),
      (int) (blue * 255.0f), (int) (alpha * 255.0f)));
}

static int
Hyena_ResolveCapability(int capability)
{
    int gu_capability = -1;

    switch (capability) {
        case HYE_BLEND:
            gu_capability = GU_BLEND;
            break;
        case HYE_CULL_FACE:
            gu_capability = GU_CULL_FACE;
            break;
        case HYE_TEXTURE_2D:
            gu_capability = GU_TEXTURE_2D;
            break;
        default:
            Sys_Error("Received unknown capability [%d]\n", capability);
            break;
    }

    return gu_capability;
}

void
Hyena_EnableCapability(int capability)
{
    int gu_capability = Hyena_ResolveCapability(capability);

    sceGuEnable(gu_capability);
}

void
Hyena_DisableCapability(int capability)
{
    int gu_capability = Hyena_ResolveCapability(capability);

    sceGuDisable(gu_capability);
}

void
Hyena_DepthMask(qboolean value)
{
    // GU_TRUE masks depth writes.
    sceGuDepthMask(value ? GU_FALSE : GU_TRUE);
}

static int
Hyena_ResolveVertexMode(int mode)
{
    int gu_mode = -1;

    switch (mode) {
        case HYE_TRIANGLE_FAN:
            gu_mode = GU_TRIANGLE_FAN;
            break;
        default:
            Sys_Error("Received mode capability [%d]\n", mode);
            break;
    }

    return gu_mode;
}

void
Hyena_BeginVertices(int mode)
{
    int gu_mode = Hyena_ResolveVertexMode(mode);

    current_vertex_mode = gu_mode;
    sceGumPushMatrix();
}

void
Hyena_Translate(float x, float y, float z)
{
    const ScePspFVector3 translation = { x, y, z };

    sceGumTranslate(&translation);
}

void
Hyena_Scale(float x, float y, float z)
{
    const ScePspFVector3 scale = { x, y, z };

    sceGumScale(&scale);
}

void
Hyena_RotateXYZ(float x, float y, float z)
{
    const ScePspFVector3 rotation = { x, y, z };

    sceGumRotateXYZ(&rotation);
}

void
Hyena_RotateZYX(float z, float y, float x)
{
    const ScePspFVector3 rotation = { x, y, z };

    sceGumRotateZYX(&rotation);
}

void
Hyena_FlushMatrices(void)
{
    sceGumUpdateMatrix();
}

vertex_t *
Hyena_AllocateMemoryForVertices(int num_vertices)
{
    return (vertex_t *) (sceGuGetMemory(sizeof(vertex_t) * num_vertices));
}

void
Hyena_2DTextureCoord(vertex_t * vertex, float u, float v)
{
    vertex->uv.u = u;
    vertex->uv.v = v;
}

void
Hyena_VertexXYZ(vertex_t * vertex, float x, float y, float z)
{
    vertex->xyz.x = x;
    vertex->xyz.y = y;
    vertex->xyz.z = z;
}

static int
Hyena_ResolveTexturePrecision(int texture_precision)
{
    int gu_texture_precision = -1;

    switch (texture_precision) {
        case HYE_TEXTURE_NOTEXTURE:
            gu_texture_precision = 0;
            break;
        case HYE_TEXTURE_32BITFLOAT:
            gu_texture_precision = GU_TEXTURE_32BITF;
            break;
        default:
            Sys_Error("Received texture precision mode [%d]\n", texture_precision);
            break;
    }

    return gu_texture_precision;
}

static int
Hyena_ResolveVertexPrecision(int vertex_precision)
{
    int gu_vertex_precision = -1;

    switch (vertex_precision) {
        case HYE_VERTEX_32BITFLOAT:
            gu_vertex_precision = GU_VERTEX_32BITF;
            break;
        default:
            Sys_Error("Received vertex precision mode [%d]\n", vertex_precision);
            break;
    }

    return gu_vertex_precision;
}

void
Hyena_DrawVertices(vertex_t * vertices, int num_vertices, int texture_precision, int vertex_precision)
{
    int gu_texture_precision = Hyena_ResolveTexturePrecision(texture_precision);
    int gu_vertex_precision  = Hyena_ResolveVertexPrecision(vertex_precision);
    int i;

    //
    // sceGuDrawArray gets sent a pointer to data as well as information
    // on the number of vertices to determine bytes to read. this is all
    // well and good until you deal with vertex coloring + no UV, because
    // suddenly instead of gu expecting [u, v, x, y, z] like we're sending
    // it, it is expecting [x, y, z].. So we need to allocate more memory
    // on the GPU to copy the vertex positions exclusively. bummer.
    //
    if (!gu_texture_precision) {
        vertex_xyz_t * vertices_xyz = (vertex_xyz_t *) (sceGuGetMemory(sizeof(vertex_xyz_t) * num_vertices));

        for (i = 0; i < num_vertices; i++) {
            vertices_xyz[i].x = vertices[i].xyz.x;
            vertices_xyz[i].y = vertices[i].xyz.y;
            vertices_xyz[i].z = vertices[i].xyz.z;
        }

        sceGuDrawArray(current_vertex_mode, gu_vertex_precision, num_vertices, 0, vertices_xyz);
    } else {
        sceGuDrawArray(current_vertex_mode, gu_texture_precision | gu_vertex_precision, num_vertices, 0, vertices);
    }
}

void
Hyena_EndVertices(void)
{
    current_vertex_mode = -1;
    sceGumPopMatrix();
}

static int
Hyena_ResolveShadeMode(int shade_mode)
{
    int gu_shade_mode = -1;

    switch (shade_mode) {
        case HYE_SMOOTH:
            gu_shade_mode = GU_SMOOTH;
            break;
        case HYE_FLAT:
            gu_shade_mode = GU_FLAT;
            break;
        default:
            Sys_Error("Received shade mode [%d]\n", shade_mode);
            break;
    }

    return gu_shade_mode;
}

void
Hyena_SetShadeMode(int shade_mode)
{
    int gu_shade_mode = Hyena_ResolveShadeMode(shade_mode);

    sceGuShadeModel(gu_shade_mode);
}

static int
Hyena_ResolveBlendFunction(int blend_function)
{
    int gu_blend_function = -1;

    switch (blend_function) {
        case HYE_ONE_MINUS_SRC_ALPHA:
            gu_blend_function = GU_ONE_MINUS_SRC_ALPHA;
            break;
        case HYE_ONE:
            gu_blend_function = GU_FIX;
            break;
        case HYE_ONE_MINUS_SRC_COLOR:
            gu_blend_function = GU_ONE_MINUS_SRC_COLOR;
            break;
        case HYE_SRC_ALPHA:
            gu_blend_function = GU_SRC_ALPHA;
            break;
        default:
            Sys_Error("Received blend mode [%d]\n", blend_function);
            break;
    }

    return gu_blend_function;
}

void
Hyena_SetBlendFunction(int source_blend, int dest_blend)
{
    int gu_source_blend = Hyena_ResolveBlendFunction(source_blend);
    int gu_dest_blend   = Hyena_ResolveBlendFunction(dest_blend);

    sceGuBlendFunc(GU_ADD, gu_source_blend, gu_dest_blend, 0, 0xFFFFFFFF);
}

void
Hyena_SetDepthRange(float near, float far)
{
    sceGuDepthRange((int) (65535.0f * near), (int) (65535.0f * far));
}

void
Hyena_SetDepthOffset(float offset)
{
    sceGuDepthOffset((int) (offset * 256.0f));
}

void
Hyena_BindTexture(int texture)
{
    GL_Bind(texture);
}

void
Hyena_FogSet(bool is_world_geometry, float start, float end,
  float red, float green, float blue, float alpha)
{
    unsigned int color = is_world_geometry ?
      GU_COLOR(0.5f, 0.5f, 0.5f, alpha) :
      GU_COLOR(red * 0.01f, green * 0.01f, blue * 0.01f, alpha);

    sceGuFog(start, end, color);
}

void Hyena_FogEnable(void){ sceGuEnable(GU_FOG); }

void Hyena_FogDisable(void){ sceGuDisable(GU_FOG); }

void Hyena_FogInit(void){ }
