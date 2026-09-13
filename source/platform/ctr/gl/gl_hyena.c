// gl_hyena.c -- Nintendo 3DS OpenGL Hyena backend
#include "../../../nzportable_def.h"

static int hyena_vertex_mode;
static vec3_t hyena_translation;
static vec3_t hyena_scale;
static float *hyena_positions;
static float *hyena_texcoords;
static short *hyena_alias_positions;
static int hyena_vertex_capacity;

static void
Hyena_ReserveVertices(int count)
{
    float *positions;
    float *texcoords;
    short *alias_positions;

    if (count <= hyena_vertex_capacity)
        return;
    positions = realloc(hyena_positions,
      count * 3 * sizeof(*hyena_positions));
    if (!positions)
        Sys_Error("Hyena_ReserveVertices: out of memory");
    hyena_positions = positions;
    texcoords = realloc(hyena_texcoords,
      count * 2 * sizeof(*hyena_texcoords));
    if (!texcoords)
        Sys_Error("Hyena_ReserveVertices: out of memory");
    hyena_texcoords = texcoords;
    alias_positions = realloc(hyena_alias_positions,
      count * 3 * sizeof(*hyena_alias_positions));
    if (!alias_positions)
        Sys_Error("Hyena_ReserveVertices: out of memory");
    hyena_alias_positions = alias_positions;
    hyena_vertex_capacity = count;
}

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
    GLenum mode = hyena_vertex_mode == HYE_TRIANGLES ? GL_TRIANGLES :
      (hyena_vertex_mode == HYE_TRIANGLE_STRIP ? GL_TRIANGLE_STRIP : GL_TRIANGLE_FAN);
    int textured = texture_precision != HYE_TEXTURE_NOTEXTURE;

    (void) vertex_precision;

    Hyena_ReserveVertices(count);
    for (i = 0; i < count; ++i) {
        hyena_positions[i * 3] = vertices[i].xyz.x * hyena_scale[0] + hyena_translation[0];
        hyena_positions[i * 3 + 1] = vertices[i].xyz.y * hyena_scale[1] + hyena_translation[1];
        hyena_positions[i * 3 + 2] = vertices[i].xyz.z * hyena_scale[2] + hyena_translation[2];
        hyena_texcoords[i * 2] = vertices[i].uv.u;
        hyena_texcoords[i * 2 + 1] = vertices[i].uv.v;
    }
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, hyena_positions);
    if (textured) {
        glEnableClientState(GL_TEXTURE_COORD_ARRAY);
        glTexCoordPointer(2, GL_FLOAT, 0, hyena_texcoords);
    }
    glDrawArrays(mode, 0, count);
    if (textured)
        glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
    free(vertices);
}

void Hyena_EndVertices(void){ }

void Hyena_DrawAliasBatch(const alias_batch_t *batch)
{
    int i;

    if (!batch->num_indices) return;
    Hyena_ReserveVertices(batch->num_vertices);
    for (i = 0; i < batch->num_vertices; ++i) {
        hyena_alias_positions[i * 3] = batch->vertices[i].xyz[0];
        hyena_alias_positions[i * 3 + 1] = batch->vertices[i].xyz[1];
        hyena_alias_positions[i * 3 + 2] = batch->vertices[i].xyz[2];
        hyena_texcoords[i * 2] = batch->vertices[i].uv[0];
        hyena_texcoords[i * 2 + 1] = batch->vertices[i].uv[1];
    }
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(3, GL_SHORT, 0, hyena_alias_positions);
    glTexCoordPointer(2, GL_FLOAT, 0, hyena_texcoords);
    glPushMatrix();
    glScalef(1.0f / 128.0f, 1.0f / 128.0f, 1.0f / 128.0f);
    glDrawElements(GL_TRIANGLES, batch->num_indices, GL_UNSIGNED_SHORT,
      batch->indices);
    glPopMatrix();
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
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
    Hyena_ReserveVertices(count);
    for (i = 0; i < count; ++i) {
        const float *vertex = source + i * stride;
        memcpy(&hyena_positions[i * 3], vertex, 3 * sizeof(float));
        memcpy(&hyena_texcoords[i * 2], vertex + texture_offset,
          2 * sizeof(float));
    }
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, hyena_positions);
    glTexCoordPointer(2, GL_FLOAT, 0, hyena_texcoords);
    glDrawArrays(GL_TRIANGLE_FAN, 0, count);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
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
    float color[4] = { red, green, blue, alpha };

    (void) is_world_geometry;
    glFogfv(GL_FOG_COLOR, color);
    glFogf(GL_FOG_START, start);
    glFogf(GL_FOG_END, end);
}

void Hyena_FogEnable(void){ glEnable(GL_FOG); }

void Hyena_FogDisable(void){ glDisable(GL_FOG); }

void Hyena_FogInit(void){ glFogi(GL_FOG_MODE, GL_LINEAR); }
