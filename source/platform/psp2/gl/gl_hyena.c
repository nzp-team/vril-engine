// gl_hyena.c -- PlayStation Vita OpenGL Hyena backend
#include "../../../nzportable_def.h"

static int hyena_vertex_mode;
static vec3_t hyena_translation;
static vec3_t hyena_scale;

static GLenum
Hyena_ResolveCapability(int capability)
{
    switch (capability) {
        case HYE_BLEND: return GL_BLEND;

        case HYE_TEXTURE_2D: return GL_TEXTURE_2D;

        case HYE_CULL_FACE: return GL_CULL_FACE;

        default: Sys_Error("Hyena: unknown capability %d", capability);
    }
    return 0;
}

void
Hyena_SetTextureMode(int mode)
{
    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE,
      mode == HYE_MODULATE ? GL_MODULATE : GL_REPLACE);
}

void
Hyena_SetColor(float r, float g, float b, float a)
{
    glColor4f(r, g, b, a);
}

void Hyena_EnableCapability(int capability){ glEnable(Hyena_ResolveCapability(capability)); }

void Hyena_DisableCapability(int capability){ glDisable(Hyena_ResolveCapability(capability)); }

void
Hyena_DepthMask(qboolean value)
{
    glDepthMask(value ? GL_TRUE : GL_FALSE);
}

void
Hyena_BeginVertices(int mode)
{
    hyena_vertex_mode = mode;
    VectorClear(hyena_translation);
    hyena_scale[0] = hyena_scale[1] = hyena_scale[2] = 1.0f;
}

void Hyena_Translate(float x, float y, float z)
{ hyena_translation[0] = x; hyena_translation[1] = y; hyena_translation[2] = z; }

void Hyena_Scale(float x, float y, float z)
{ hyena_scale[0] = x; hyena_scale[1] = y; hyena_scale[2] = z; }

void
Hyena_RotateXYZ(float x, float y, float z)
{
    (void) x;
    (void) y;
    (void) z;
}

void
Hyena_RotateZYX(float z, float y, float x)
{
    (void) x;
    (void) y;
    (void) z;
}

void Hyena_FlushMatrices(void){ }

vertex_t *
Hyena_AllocateMemoryForVertices(int count)
{
    return (vertex_t *) malloc(sizeof(vertex_t) * count);
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

void
Hyena_DrawVertices(vertex_t * vertices, int count, int texture_precision, int vertex_precision)
{
    int i;
    GLenum mode = hyena_vertex_mode == HYE_TRIANGLES ? GL_TRIANGLES : (hyena_vertex_mode == HYE_TRIANGLE_STRIP ? GL_TRIANGLE_STRIP : GL_TRIANGLE_FAN);
    int textured = texture_precision != HYE_TEXTURE_NOTEXTURE;

    (void) vertex_precision;

    for (i = 0; i < count; ++i) { vertices[i].xyz.x = vertices[i].xyz.x * hyena_scale[0] + hyena_translation[0]; vertices[i].xyz.y = vertices[i].xyz.y * hyena_scale[1] + hyena_translation[1]; vertices[i].xyz.z = vertices[i].xyz.z * hyena_scale[2] + hyena_translation[2]; }
    glBindBuffer(GL_ARRAY_BUFFER, 0); glEnableClientState(GL_VERTEX_ARRAY); glVertexPointer(3, GL_FLOAT, sizeof(*vertices), &vertices[0].xyz);
    if (textured) { glEnableClientState(GL_TEXTURE_COORD_ARRAY); glTexCoordPointer(2, GL_FLOAT, sizeof(*vertices), &vertices[0].uv); }
    glDrawArrays(mode, 0, count); if (textured) glDisableClientState(GL_TEXTURE_COORD_ARRAY); glDisableClientState(GL_VERTEX_ARRAY);
    free(vertices);
}

void Hyena_EndVertices(void){ }

void Hyena_DrawAliasBatch(const alias_batch_t *batch)
{
    if (!batch->num_indices) return;
    glBindBuffer(GL_ARRAY_BUFFER, 0); glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glEnableClientState(GL_VERTEX_ARRAY); glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(3, GL_SHORT, sizeof(*batch->vertices), batch->vertices[0].xyz);
    glTexCoordPointer(2, GL_FLOAT, sizeof(*batch->vertices), batch->vertices[0].uv);
    glPushMatrix(); glScalef(1.0f / 128.0f, 1.0f / 128.0f, 1.0f / 128.0f);
    glDrawElements(GL_TRIANGLES, batch->num_indices, GL_UNSIGNED_SHORT, batch->indices);
    glPopMatrix();
    glDisableClientState(GL_TEXTURE_COORD_ARRAY); glDisableClientState(GL_VERTEX_ARRAY);
}

void Hyena_DrawSurfaceFan(const float *source, int count, int stride,
  int texture_offset, qboolean warp, double time)
{
    int i;
    if (warp) {
        vertex_t *vertices = Hyena_AllocateMemoryForVertices(count);
        for (i = 0; i < count; ++i) {
            const float *in = source + i * stride;
            Hyena_2DTextureCoord(&vertices[i], in[texture_offset], in[texture_offset + 1]);
            Hyena_VertexXYZ(&vertices[i], in[0] + 8*sinf(in[1]*0.05f+(float)time)*sinf(in[2]*0.05f+(float)time), in[1] + 8*sinf(in[0]*0.05f+(float)time)*sinf(in[2]*0.05f+(float)time), in[2]);
        }
        Hyena_BeginVertices(HYE_TRIANGLE_FAN);
        Hyena_DrawVertices(vertices, count, HYE_TEXTURE_32BITFLOAT, HYE_VERTEX_32BITFLOAT);
        Hyena_EndVertices(); return;
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glEnableClientState(GL_VERTEX_ARRAY); glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(3, GL_FLOAT, stride * sizeof(float), source);
    glTexCoordPointer(2, GL_FLOAT, stride * sizeof(float), source + texture_offset);
    glDrawArrays(GL_TRIANGLE_FAN, 0, count);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY); glDisableClientState(GL_VERTEX_ARRAY);
}


void
Hyena_SetShadeMode(int mode)
{
    glShadeModel(mode == HYE_FLAT ? GL_FLAT : GL_SMOOTH);
}

static GLenum
Hyena_ResolveBlend(int blend)
{
    switch (blend) {
        case HYE_SRC_ALPHA: return GL_SRC_ALPHA;

        case HYE_ONE_MINUS_SRC_ALPHA: return GL_ONE_MINUS_SRC_ALPHA;

        case HYE_ONE: return GL_ONE;

        case HYE_ONE_MINUS_SRC_COLOR: return GL_ONE_MINUS_SRC_COLOR;

        default: Sys_Error("Hyena: unknown blend function %d", blend);
    }
    return GL_ONE;
}

void
Hyena_SetBlendFunction(int source, int destination)
{
    glBlendFunc(Hyena_ResolveBlend(source), Hyena_ResolveBlend(destination));
}

void
Hyena_SetDepthRange(float near_value, float far_value)
{
    glDepthRange(near_value, far_value);
}

void
Hyena_SetDepthOffset(float offset)
{
    if (offset != 0.0f) {
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(offset, offset);
    } else glDisable(GL_POLYGON_OFFSET_FILL);
}

void Hyena_BindTexture(int texture){ GL_Bind(texture); }

void
Hyena_FogSet(bool is_world_geometry, float start, float end,
  float red, float green, float blue, float alpha)
{
    float color[4] = {
        is_world_geometry ? 0.5f : red * 0.01f,
        is_world_geometry ? 0.5f : green * 0.01f,
        is_world_geometry ? 0.5f : blue * 0.01f,
        alpha
    };

    glFogfv(GL_FOG_COLOR, color);
    glFogf(GL_FOG_START, start);
    glFogf(GL_FOG_END, end);
}

void Hyena_FogEnable(void){ glEnable(GL_FOG); }

void Hyena_FogDisable(void){ glDisable(GL_FOG); }

void Hyena_FogInit(void){ glFogi(GL_FOG_MODE, GL_LINEAR); }
