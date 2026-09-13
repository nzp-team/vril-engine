// gl_hyena.c -- Nintendo 3DS OpenGL Hyena backend
#include "../../../nzportable_def.h"

static int hyena_vertex_mode;
static vec3_t hyena_translation;
static vec3_t hyena_scale;
static float hyena_color[4] = { 1, 1, 1, 1 };
static short *hyena_alias_positions;
static float *hyena_alias_texcoords;
static float *hyena_alias_colors;
static int hyena_alias_capacity;
static float *hyena_surface_positions;
static float *hyena_surface_texcoords;
static float *hyena_surface_colors;
static unsigned short *hyena_surface_indices;
static int hyena_surface_capacity;

static void
Hyena_ReserveAliasVertices(int count)
{
    short *positions;
    float *texcoords;
    float *colors;

    if (count <= hyena_alias_capacity)
        return;
    positions = realloc(hyena_alias_positions,
      count * 3 * sizeof(*hyena_alias_positions));
    if (!positions)
        Sys_Error("Hyena_ReserveAliasVertices: out of memory");
    hyena_alias_positions = positions;
    texcoords = realloc(hyena_alias_texcoords,
      count * 2 * sizeof(*hyena_alias_texcoords));
    if (!texcoords)
        Sys_Error("Hyena_ReserveAliasVertices: out of memory");
    hyena_alias_texcoords = texcoords;
    colors = realloc(hyena_alias_colors,
      count * 4 * sizeof(*hyena_alias_colors));
    if (!colors)
        Sys_Error("Hyena_ReserveAliasVertices: out of memory");
    hyena_alias_colors = colors;
    hyena_alias_capacity = count;
}

static void
Hyena_ReserveSurfaceVertices(int count)
{
    float *positions;
    float *texcoords;
    float *colors;
    unsigned short *indices;

    if (count <= hyena_surface_capacity)
        return;
    positions = realloc(hyena_surface_positions,
      count * 3 * sizeof(*hyena_surface_positions));
    if (!positions)
        Sys_Error("Hyena_ReserveSurfaceVertices: out of memory");
    hyena_surface_positions = positions;
    texcoords = realloc(hyena_surface_texcoords,
      count * 2 * sizeof(*hyena_surface_texcoords));
    if (!texcoords)
        Sys_Error("Hyena_ReserveSurfaceVertices: out of memory");
    hyena_surface_texcoords = texcoords;
    colors = realloc(hyena_surface_colors,
      count * 4 * sizeof(*hyena_surface_colors));
    if (!colors)
        Sys_Error("Hyena_ReserveSurfaceVertices: out of memory");
    hyena_surface_colors = colors;
    indices = realloc(hyena_surface_indices,
      (count - 2) * 3 * sizeof(*hyena_surface_indices));
    if (!indices)
        Sys_Error("Hyena_ReserveSurfaceVertices: out of memory");
    hyena_surface_indices = indices;
    hyena_surface_capacity = count;
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
    hyena_color[0] = r;
    hyena_color[1] = g;
    hyena_color[2] = b;
    hyena_color[3] = a;
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

static void
Hyena_SubmitVertex(const vertex_t * vertex, int textured)
{
    if (textured)
        glTexCoord2f(vertex->uv.u, vertex->uv.v);
    glVertex3f(vertex->xyz.x * hyena_scale[0] + hyena_translation[0],
      vertex->xyz.y * hyena_scale[1] + hyena_translation[1],
      vertex->xyz.z * hyena_scale[2] + hyena_translation[2]);
}

void
Hyena_DrawVertices(vertex_t * vertices, int count, int texture_precision, int vertex_precision)
{
    int i;
    int textured = texture_precision != HYE_TEXTURE_NOTEXTURE;

    (void) vertex_precision;

    // Expand fans for backends without reliable fan support.
    if (hyena_vertex_mode == HYE_TRIANGLE_FAN && count >= 3) {
        glBegin(GL_TRIANGLES);
        for (i = 1; i + 1 < count; ++i) {
            Hyena_SubmitVertex(&vertices[0], textured);
            Hyena_SubmitVertex(&vertices[i], textured);
            Hyena_SubmitVertex(&vertices[i + 1], textured);
        }
        glEnd();
    }
    free(vertices);
}

void Hyena_EndVertices(void){ }

void
Hyena_DrawAliasBatch(const alias_batch_t *batch)
{
    int i;

    if (!batch->num_indices)
        return;
    Hyena_ReserveAliasVertices(batch->num_vertices);
    for (i = 0; i < batch->num_vertices; ++i) {
        hyena_alias_positions[i * 3] = batch->vertices[i].xyz[0];
        hyena_alias_positions[i * 3 + 1] = batch->vertices[i].xyz[1];
        hyena_alias_positions[i * 3 + 2] = batch->vertices[i].xyz[2];
        hyena_alias_texcoords[i * 2] = batch->vertices[i].uv[0];
        hyena_alias_texcoords[i * 2 + 1] = batch->vertices[i].uv[1];
        hyena_alias_colors[i * 4] = hyena_color[0];
        hyena_alias_colors[i * 4 + 1] = hyena_color[1];
        hyena_alias_colors[i * 4 + 2] = hyena_color[2];
        hyena_alias_colors[i * 4 + 3] = hyena_color[3];
    }
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(3, GL_SHORT, 0, hyena_alias_positions);
    glTexCoordPointer(2, GL_FLOAT, 0, hyena_alias_texcoords);
    glColorPointer(4, GL_FLOAT, 0, hyena_alias_colors);
    glPushMatrix();
    glScalef(1.0f / 128.0f, 1.0f / 128.0f, 1.0f / 128.0f);
    glDrawElements(GL_TRIANGLES, batch->num_indices, GL_UNSIGNED_SHORT,
      batch->indices);
    glPopMatrix();
    glDisableClientState(GL_COLOR_ARRAY);
    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
    glDisableClientState(GL_VERTEX_ARRAY);
}

void
Hyena_DrawSurfaceFan(const float *source, int count, int stride,
  int texture_offset, qboolean warp, double time)
{
    int i;
    int num_indices;

    if (count < 3)
        return;
    Hyena_ReserveSurfaceVertices(count);
    for (i = 0; i < count; ++i) {
        const float *vertex = source + i * stride;
        float x = vertex[0];
        float y = vertex[1];

        if (warp) {
            x += 8 * sinf(vertex[1] * 0.05f + (float)time) *
              sinf(vertex[2] * 0.05f + (float)time);
            y += 8 * sinf(vertex[0] * 0.05f + (float)time) *
              sinf(vertex[2] * 0.05f + (float)time);
        }
        hyena_surface_positions[i * 3] = x;
        hyena_surface_positions[i * 3 + 1] = y;
        hyena_surface_positions[i * 3 + 2] = vertex[2];
        hyena_surface_texcoords[i * 2] = vertex[texture_offset];
        hyena_surface_texcoords[i * 2 + 1] = vertex[texture_offset + 1];
        hyena_surface_colors[i * 4] = hyena_color[0];
        hyena_surface_colors[i * 4 + 1] = hyena_color[1];
        hyena_surface_colors[i * 4 + 2] = hyena_color[2];
        hyena_surface_colors[i * 4 + 3] = hyena_color[3];
    }
    num_indices = 3 * (count - 2);
    for (i = 0; i < count - 2; ++i) {
        hyena_surface_indices[i * 3] = 0;
        hyena_surface_indices[i * 3 + 1] = i + 1;
        hyena_surface_indices[i * 3 + 2] = i + 2;
    }
    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);
    glVertexPointer(3, GL_FLOAT, 0, hyena_surface_positions);
    glTexCoordPointer(2, GL_FLOAT, 0, hyena_surface_texcoords);
    glColorPointer(4, GL_FLOAT, 0, hyena_surface_colors);
    glDrawElements(GL_TRIANGLES, num_indices, GL_UNSIGNED_SHORT,
      hyena_surface_indices);
    glDisableClientState(GL_COLOR_ARRAY);
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
