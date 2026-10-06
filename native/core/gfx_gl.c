// Not a port of anything in web/ -- see PORT_SPEC.md §5/§6. web/graphics.js
// uploads one VBO and issues one gl.drawArrays per frame; this uses
// immediate mode instead (glBegin/glVertex per vertex) because it needs
// zero vertex-array-object boilerplate to work identically against both
// desktop GL (Linux) and vitaGL (Vita) through the same source file -- see
// gl_include.h, provided per-platform, resolved by each platform's
// CMakeLists include path. Slower than a real VBO, correct first, optimize
// later if profiling on real hardware says so.
#include "gfx_gl.h"
#include "gl_include.h"
#include <stddef.h>

// A client-side vertex array, NOT the glBegin/glVertex loop this used to
// be. That loop cost two calls per vertex -- a glColor4ub unpacking
// v->rgba by hand, then a glVertex2f -- so a race frame carrying 53,529
// vertices spent over a hundred thousand calls per frame before the GPU
// drew a single pixel. On a desktop that is invisible; on the Vita it was
// the whole frame budget, and it is why a 2000s applet that rendered
// polygons straight into an image buffer outran this port on hardware
// twenty years newer. The same frame is now seven calls.
//
// No conversion is needed to get here, which is what makes the change
// cheap: GfxVert is already exactly an interleaved array -- 12 bytes,
// two floats at offset 0 then a packed 0xAABBGGRR at offset 8, which in
// memory order is R,G,B,A, precisely what GL_UNSIGNED_BYTE colour
// pointers expect. The old code was unpacking a layout the GL could read
// directly.
static void draw_triangle_range(const Graphics2D *g, int32_t start, int32_t count) {
  if (count == 0) return;
  const GfxVert *base = &g->verts[start];
  glEnableClientState(GL_VERTEX_ARRAY);
  glEnableClientState(GL_COLOR_ARRAY);
  glVertexPointer(2, GL_FLOAT, sizeof(GfxVert), &base->x);
  glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(GfxVert), &base->rgba);
  glDrawArrays(GL_TRIANGLES, 0, count);
  // Both switched back off before returning: draw_image_cmd() below sets
  // its colour with glColor4f, and a colour array left enabled would
  // silently override that for every textured quad.
  glDisableClientState(GL_COLOR_ARRAY);
  glDisableClientState(GL_VERTEX_ARRAY);
}

static void draw_image_cmd(const GfxDrawCmd *cmd) {
  if (cmd->image_id < 0) return;
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, (GLuint)cmd->image_id);
  // Default GL1.1 texture env mode is GL_MODULATE, so (1,1,1,alpha) here
  // multiplies the texture's own RGB unchanged and scales its alpha by
  // the composite alpha at the time of the call (see gfx_draw_image's
  // own doc comment on setComposite() affecting drawImage) -- straight
  // alpha blending, same GL_SRC_ALPHA/GL_ONE_MINUS_SRC_ALPHA state the
  // platform init already set up for the flat-coloured triangles.
  glColor4f(1.0f, 1.0f, 1.0f, cmd->alpha);
  float x0 = cmd->x, y0 = cmd->y, x1 = cmd->x + cmd->w, y1 = cmd->y + cmd->h;
  float u0 = cmd->u0, v0 = cmd->v0, u1 = cmd->u1, v1 = cmd->v1;
  glBegin(GL_TRIANGLES);
  glTexCoord2f(u0, v0); glVertex2f(x0, y0);
  glTexCoord2f(u1, v0); glVertex2f(x1, y0);
  glTexCoord2f(u1, v1); glVertex2f(x1, y1);
  glTexCoord2f(u0, v0); glVertex2f(x0, y0);
  glTexCoord2f(u1, v1); glVertex2f(x1, y1);
  glTexCoord2f(u0, v1); glVertex2f(x0, y1);
  glEnd();
  glDisable(GL_TEXTURE_2D);
}

void gfx_submit_gl(const Graphics2D *g) {
  for (int32_t i = 0; i < g->cmd_count; i++) {
    const GfxDrawCmd *cmd = &g->cmds[i];
    if (cmd->image_id < 0) {
      draw_triangle_range(g, cmd->vert_start, cmd->vert_count);
    } else {
      draw_image_cmd(cmd);
    }
  }
  // Any vertices pushed after the last gfx_draw_image() call (or all of
  // them, if this frame never called it) aren't covered by a command
  // yet -- gfx_draw_image only flushes what came BEFORE it, see gfx.c.
  draw_triangle_range(g, g->flushed, g->count - g->flushed);
}

int32_t gfx_gl_upload_texture(const uint8_t *rgba, int32_t width, int32_t height) {
  GLuint tex = 0;
  glGenTextures(1, &tex);
  if (tex == 0) return -1;
  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
  glBindTexture(GL_TEXTURE_2D, 0);
  return (int32_t)tex;
}

void gfx_gl_update_texture(int32_t texture, const uint8_t *rgba, int32_t width, int32_t height) {
  glBindTexture(GL_TEXTURE_2D, (GLuint)texture);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
  glBindTexture(GL_TEXTURE_2D, 0);
}

void gfx_gl_update_texture_rows(int32_t texture, const uint8_t *rgba, int32_t width, int32_t y,
                                int32_t rows) {
  glBindTexture(GL_TEXTURE_2D, (GLuint)texture);
  glTexSubImage2D(GL_TEXTURE_2D, 0, 0, y, width, rows, GL_RGBA, GL_UNSIGNED_BYTE,
                  rgba + (size_t)y * (size_t)width * 4);
  glBindTexture(GL_TEXTURE_2D, 0);
}

bool gfx_gl_render_target_init(GfxGlRenderTarget *rt, int32_t width, int32_t height) {
  return gfx_gl_render_target_init_scaled(rt, width, height, width, height, false);
}

bool gfx_gl_render_target_init_scaled(GfxGlRenderTarget *rt, int32_t width, int32_t height,
                                      int32_t px_w, int32_t px_h, bool linear) {
  rt->fbo = rt->tex = 0;
  rt->width = width;
  rt->height = height;
  rt->px_w = px_w;
  rt->px_h = px_h;
  rt->linear = linear;

  GLuint tex = 0;
  glGenTextures(1, &tex);
  if (tex == 0) return false;
  glBindTexture(GL_TEXTURE_2D, tex);
  // NEAREST, same as gfx_gl_upload_texture -- this target is always
  // blitted back at exactly its own size (see gfx_gl_render_target_blit's
  // own doc comment), a 1:1 texel copy every time, so filtering never
  // actually runs; NEAREST just documents that intent rather than
  // implying a smoothing pass that isn't happening.
  // (Smooth / HD: LINEAR, so the stretch to the display blends; the 1:1
  // target-to-target blits of the trail land on texel centres either way.)
  const GLint filter = linear ? GL_LINEAR : GL_NEAREST;
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, px_w, px_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
  glBindTexture(GL_TEXTURE_2D, 0);

  GLuint fbo = 0;
  glGenFramebuffers(1, &fbo);
  if (fbo == 0) {
    glDeleteTextures(1, &tex);
    return false;
  }
  glBindFramebuffer(GL_FRAMEBUFFER, fbo);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
  GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    glDeleteFramebuffers(1, &fbo);
    glDeleteTextures(1, &tex);
    return false;
  }

  rt->fbo = (uint32_t)fbo;
  rt->tex = (uint32_t)tex;
  return true;
}

void gfx_gl_render_target_free(GfxGlRenderTarget *rt) {
  GLuint fbo = (GLuint)rt->fbo, tex = (GLuint)rt->tex;
  if (fbo) glDeleteFramebuffers(1, &fbo);
  if (tex) glDeleteTextures(1, &tex);
  rt->fbo = rt->tex = 0;
}

void gfx_gl_render_target_bind(const GfxGlRenderTarget *rt) {
  glBindFramebuffer(GL_FRAMEBUFFER, rt ? (GLuint)rt->fbo : 0);
}

void gfx_gl_render_target_blit(const GfxGlRenderTarget *rt, float offset_x, float offset_y, float alpha) {
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, (GLuint)rt->tex);
  // Same GL_MODULATE-driven alpha-only blend gfx_gl.c's own draw_image_cmd
  // already relies on (see its comment) -- (1,1,1,alpha) leaves the
  // texture's RGB unchanged and scales its alpha by `alpha`, straight
  // GL_SRC_ALPHA/GL_ONE_MINUS_SRC_ALPHA blending against whatever is
  // already in the destination framebuffer (deliberately not cleared by
  // the caller -- see this function's own doc comment in gfx_gl.h).
  glColor4f(1.0f, 1.0f, 1.0f, alpha);
  float x0 = offset_x, y0 = offset_y;
  float x1 = offset_x + (float)rt->width, y1 = offset_y + (float)rt->height;
  // V flipped vs. the "obvious" (0,0)-top/(1,1)-bottom mapping
  // gfx_gl.c's other textured draws use: those sample images uploaded
  // via glTexImage2D directly from already-decoded top-down RGBA (gif/
  // png/jpeg_decode.c), where texel row 0 IS the image's top row by
  // construction. This texture instead comes from RENDERING into it
  // through the same y-down glOrtho(0,width,height,0,-1,1) projection
  // every other draw in this port uses -- OpenGL's rasterizer always
  // writes framebuffer/texture row 0 at NDC y=-1 (the BOTTOM of that
  // projection, since top=0/bottom=height flips it), so this render
  // target's row 0 holds the SCENE's bottom edge, the opposite of a
  // normally-loaded image. Sampling it with the same V mapping gfx_
  // draw_image uses upends the whole frame -- confirmed visually (the
  // HUD and horizon both landed upside down) before swapping V here.
  glBegin(GL_TRIANGLES);
  glTexCoord2f(0, 1); glVertex2f(x0, y0);
  glTexCoord2f(1, 1); glVertex2f(x1, y0);
  glTexCoord2f(1, 0); glVertex2f(x1, y1);
  glTexCoord2f(0, 1); glVertex2f(x0, y0);
  glTexCoord2f(1, 0); glVertex2f(x1, y1);
  glTexCoord2f(0, 0); glVertex2f(x0, y1);
  glEnd();
  glDisable(GL_TEXTURE_2D);
}

void gfx_gl_render_target_blit_region(const GfxGlRenderTarget *rt, float src_x, float src_y,
                                      float src_w, float src_h, float offset_x, float offset_y,
                                      float alpha) {
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, (GLuint)rt->tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glColor4f(1.0f, 1.0f, 1.0f, alpha);
  float x0 = offset_x, y0 = offset_y;
  float x1 = offset_x + (float)rt->width, y1 = offset_y + (float)rt->height;
  // Same flipped V as gfx_gl_render_target_blit (texture row 0 is the
  // scene's BOTTOM edge), restricted to the source rectangle.
  float u0 = src_x / (float)rt->width, u1 = (src_x + src_w) / (float)rt->width;
  float vt = 1.0f - src_y / (float)rt->height;           // scene top
  float vb = 1.0f - (src_y + src_h) / (float)rt->height; // scene bottom
  glBegin(GL_TRIANGLES);
  glTexCoord2f(u0, vt); glVertex2f(x0, y0);
  glTexCoord2f(u1, vt); glVertex2f(x1, y0);
  glTexCoord2f(u1, vb); glVertex2f(x1, y1);
  glTexCoord2f(u0, vt); glVertex2f(x0, y0);
  glTexCoord2f(u1, vb); glVertex2f(x1, y1);
  glTexCoord2f(u0, vb); glVertex2f(x0, y1);
  glEnd();
  // Back to the target's own filter (NEAREST unless Smooth / HD made it LINEAR).
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, rt->linear ? GL_LINEAR : GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, rt->linear ? GL_LINEAR : GL_NEAREST);
  glDisable(GL_TEXTURE_2D);
}
