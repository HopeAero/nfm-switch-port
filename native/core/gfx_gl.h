// The GL-submission half of the graphics port -- see gfx.h for why this is
// split from gfx.c. Deliberately GL-header-free itself: including this
// doesn't pull GL types into unrelated files, only gfx_gl.c does that.
#ifndef NFM_GFX_GL_H
#define NFM_GFX_GL_H

#include "gfx.h"

#ifdef __cplusplus
extern "C" {
#endif

// Issues the frame's vertex list as immediate-mode GL1.1 draw calls
// (glBegin(GL_TRIANGLES)/glColor4ub/glVertex2f/glEnd), preserving submission
// order exactly -- see gfx.h's ordering banner. Requires an active GL
// context with depth testing off and blending on (PORT_SPEC.md §5); the
// platform's init code sets that up once, not this function every frame.
// Interleaved gfx_draw_image() calls (see gfx.h) are issued as their own
// textured-quad draws in the same submission-order sequence, switching
// GL state (bind/unbind a texture) as needed between flat-coloured
// batches and image draws.
void gfx_submit_gl(const Graphics2D *g);

/**
 * Uploads a decoded RGBA8888 image (from gif_decode.c/png_decode.c/
 * jpeg_decode.c) as a new GL texture and returns an opaque handle to
 * pass to gfx_draw_image(). The handle IS the GL texture name (a small
 * positive integer GL itself hands out) cast to int32_t -- no separate
 * lookup table needed, gfx_submit_gl casts it straight back to GLuint
 * at bind time. Returns -1 on failure (out of texture units/memory, not
 * expected in practice for this port's handful of small HUD/menu
 * assets). Call once per unique image at load time, not per frame --
 * this does real GL work (glGenTextures/glTexImage2D), unlike
 * gfx_draw_image itself, which just records a draw command.
 */
int32_t gfx_gl_upload_texture(const uint8_t *rgba, int32_t width, int32_t height);

/**
 * Replaces an existing texture's pixels in place (glTexSubImage2D) --
 * `width`/`height` must match the texture's own size from the
 * gfx_gl_upload_texture() call that created it. For content that's
 * genuinely re-rendered every frame (the car-select smoke-warp
 * background, xtGraphics.java's drawSmokeCarsbg/flexpix -- see game.c's
 * own doc comment), so the port doesn't leak a fresh GL texture name
 * every single frame the way calling gfx_gl_upload_texture() again would.
 */
void gfx_gl_update_texture(int32_t texture, const uint8_t *rgba, int32_t width, int32_t height);

// An offscreen color target the whole frame can render into, then be
// blitted back from -- see gfx_gl_render_target_blit()'s own doc comment
// for what this exists to support. `fbo`/`tex` are GL object names (GLuint
// in gfx_gl.c) stored as plain ints so this header stays GL-type-free,
// same convention gfx_gl_upload_texture's own int32_t return already uses.
typedef struct {
  uint32_t fbo, tex;
  int32_t width, height;
} GfxGlRenderTarget;

/** Allocates a `width`x`height` RGBA8 texture and a framebuffer object
 * rendering into it. Returns false (rt left zeroed) if either GL object
 * fails to create or the framebuffer comes back incomplete -- callers
 * should fall back to drawing straight to the default framebuffer, same
 * fail-soft convention as a missing HUD asset elsewhere in this port. */
bool gfx_gl_render_target_init(GfxGlRenderTarget *rt, int32_t width, int32_t height);
void gfx_gl_render_target_free(GfxGlRenderTarget *rt);

/** Redirects subsequent drawing (gfx_submit_gl, raw glClear/glViewport)
 * into `rt` instead of the default framebuffer. Pass NULL to switch back
 * to the default framebuffer (GL object 0). */
void gfx_gl_render_target_bind(const GfxGlRenderTarget *rt);

/**
 * Draws `rt`'s texture as a `rt->width`x`rt->height` quad onto whatever
 * framebuffer is CURRENTLY bound (the default one, in every call site
 * this port has), offset by (`offset_x`,`offset_y`) game-space pixels,
 * at flat alpha `alpha` (0..1) -- deliberately does NOT clear the
 * destination first, so anything already there (the previous frame,
 * left un-cleared on purpose) shows through wherever this draw doesn't
 * fully cover it or alpha < 1.
 *
 * This is the whole mechanism behind GameSparker.java's own `paint()`
 * (decompilation/java-src/GameSparker.java:1798-1861): it blits its
 * offscreen-rendered frame (`this.offImage`) onto the visible Canvas at
 * `AlphaComposite` alpha `this.mvect/100` and a small random jitter
 * offset while `this.shaka` (screen-shake-from-damage) is counting down,
 * onto a Canvas it does NOT clear between paints -- producing the
 * "motion blur"/ghosting trail most visible while turning hard or right
 * after an impact. web/ never ports this (paint()'s whole
 * offImage-compositing dance isn't in scope there, same class of gap as
 * GameSparker.java's instant-replay camera -- see TASKS_NATIVE.md's Part
 * 17 entry for that one), so this is translated directly from the Java.
 * See platform/common/game.c's own call site for the mvect/shaka state
 * this reads. */
void gfx_gl_render_target_blit(const GfxGlRenderTarget *rt, float offset_x, float offset_y, float alpha);

#ifdef __cplusplus
}
#endif

#endif
