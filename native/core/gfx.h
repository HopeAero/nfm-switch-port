// ports web/graphics.js's geometry-building half (the `Graphics2D` class),
// EXCLUDING the actual WebGL calls -- those have no C equivalent to port,
// they're platform glue (see native/core/gfx_gl.c and PORT_SPEC.md §5/§6).
// This file is pure CPU-side vertex-list building: fully platform-agnostic,
// fully host-testable, no GL context required.
//
// ============================================================================
// THE ORDERING CONSTRAINT -- read before changing anything in this file.
// Ported verbatim from web/graphics.js's own banner; it is equally binding
// here. There is NO DEPTH BUFFER. Occlusion is submission order ONLY.
// Colour is a per-vertex attribute, every primitive lands in ONE list in
// original call order, and the whole frame becomes one draw. Do NOT batch by
// material or primitive type, do NOT sort, do NOT split fills and outlines
// into separate passes -- any of those still looks plausible in a screenshot,
// which is what makes it dangerous. See AGENTS.md's banner too.
// ============================================================================
#ifndef NFM_GFX_H
#define NFM_GFX_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  float x, y;
  uint32_t rgba; // packed 0xAABBGGRR (same byte order as web/graphics.js's _pack)
} GfxVert;

// One entry is EITHER a run of `vert_count` flat-coloured triangles
// starting at `vert_start` in `verts[]` (`image_id == -1`) OR a single
// textured quad draw (`vert_count == 0`, `image_id >= 0`) -- never both.
// Interleaving these (rather than batching all flat triangles into one
// draw and all images into another) is what makes textured draws obey
// the ordering constraint every other primitive in this file already
// does -- see gfx.h's own banner comment: no depth buffer, occlusion is
// submission order only. `image_id` is an opaque handle gfx.c never
// interprets -- see gfx_draw_image's own doc comment for what actually
// produces one.
typedef struct {
  int32_t vert_start, vert_count;
  int32_t image_id;
  float x, y, w, h; // image draw position/size, game-space pixels -- only meaningful when image_id >= 0
  float alpha;      // g->a at the time of the call -- setComposite() also affects drawImage in the original
  float u0, v0, u1, v1; // source-rect UVs 0..1 (default 0,0,1,1 = full image, used by
                        // gfx_draw_image_sub -- see its own doc comment)
} GfxDrawCmd;

// Tagged (not just typedef'd) so other headers can forward-declare
// `struct Graphics2D *` without including this file -- see medium.h.
typedef struct Graphics2D {
  int32_t width, height;

  GfxVert *verts;
  int32_t count;
  int32_t capacity;

  // Interleaved flat-triangle-batch / textured-quad draw list -- see
  // GfxDrawCmd's own comment. `flushed` tracks how much of `verts[]`
  // has already been claimed by an earlier command, so gfx_draw_image
  // knows how many trailing vertices belong to the batch it needs to
  // flush before appending its own command.
  GfxDrawCmd *cmds;
  int32_t cmd_count, cmd_capacity;
  int32_t flushed;

  // Scene-shape counters -- ported because Plane.d()/s() and ContO.d()
  // increment them directly (`graphics2D.faceCalls++` etc. in the JS), not
  // because gfx.c itself uses them. Reset every gfx_begin().
  int32_t inputVerts;
  int32_t objCalls, objDrawn, faceCalls, projVerts;

  // Current colour/composite state, mirroring Graphics2D's mutable state.
  float r, g, b, a; // 0..1
  uint32_t rgba;    // same colour, pre-packed for the vertex buffer
  float lineWidth;
} Graphics2D;

void gfx_init(Graphics2D *g, int32_t width, int32_t height);
void gfx_free(Graphics2D *g);

// Start/end a frame. gfx_begin resets the vertex list and counters (matches
// web/graphics.js's begin(), overlay/text handling excluded -- not ported,
// see PORT_SPEC.md M3). gfx_end does NOT touch a GL context -- that's
// gfx_submit_gl() in gfx_gl.c; gfx_end just exists for symmetry/future use.
void gfx_begin(Graphics2D *g);

/** setColor(new Color(r,g,b)). Accepts 0-255 ints, as the game passes. */
void gfx_set_color(Graphics2D *g, int32_t r, int32_t gg, int32_t b);

/** setComposite(AlphaComposite.getInstance(rule, alpha)). */
void gfx_set_composite(Graphics2D *g, float alpha);

/**
 * fillPolygon(xs, ys, n). Convex polygons fan (fast path); concave ones use
 * an even-odd trapezoid fill so self-intersecting/keyhole outlines (the
 * checkpoint glyphs) rasterize correctly. See web/graphics.js's own long
 * comment on why ear-clipping was tried and rejected for this.
 */
void gfx_fill_polygon(Graphics2D *g, const int32_t *xs, const int32_t *ys, int32_t n);

/** drawPolygon(xs, ys, n): closed 1px outline, expanded to quads. */
void gfx_draw_polygon(Graphics2D *g, const int32_t *xs, const int32_t *ys, int32_t n);

void gfx_draw_line(Graphics2D *g, int32_t x0, int32_t y0, int32_t x1, int32_t y1);
void gfx_fill_rect(Graphics2D *g, int32_t x, int32_t y, int32_t w, int32_t h);
void gfx_draw_rect(Graphics2D *g, int32_t x, int32_t y, int32_t w, int32_t h);
void gfx_fill_oval(Graphics2D *g, int32_t x, int32_t y, int32_t w, int32_t h);
void gfx_clear_rect(Graphics2D *g, int32_t x, int32_t y, int32_t w, int32_t h);

/**
 * drawImage(image, x, y): draws a `w`x`h` image (top-left at x,y, game-
 * space pixels, no scaling -- every real call site draws at native
 * size) at the CURRENT alpha (see gfx_set_composite -- the countdown
 * "dude" face fades in via exactly this, `setComposite(0.3)` then
 * `drawImage` then `setComposite(1.0)`, matching web/graphics.js's own
 * `drawImage`).
 *
 * `image_id` is an opaque handle this function never interprets --
 * gfx.c stays GL-free and host-testable (see this file's own banner),
 * so it doesn't decode images or own a texture cache itself. On the
 * Linux/Vita platforms, `image_id` comes from uploading a
 * gif_decode.c/png_decode.c/jpeg_decode.c result via
 * gfx_gl_upload_texture() (see gfx_gl.h); `gfx_submit_gl` is what
 * actually binds and draws it, in the same submission-order sequence
 * this call recorded it in.
 */
void gfx_draw_image(Graphics2D *g, int32_t image_id, int32_t x, int32_t y, int32_t w, int32_t h);

/**
 * drawImage(image, dx, dy, dx+dw, dy+dh, sx, sy, sx+sw, sy+sh) --
 * java.awt.Graphics2D's own subimage draw overload, used by
 * xtGraphics.java's own menu code to draw one row of options.png at a
 * time (the 4 options are baked into ONE 211x105 sprite, and each
 * screen may show a different subset). `src_*` are in the image's own
 * pixel coordinates (not UV 0..1); `image_w`/`image_h` are the full
 * image dimensions this function needs to convert to UVs internally,
 * matching what gfx.c already tracks in `GfxDrawCmd.u0..v1`.
 */
void gfx_draw_image_sub(Graphics2D *g, int32_t image_id, int32_t dst_x, int32_t dst_y,
                         int32_t dst_w, int32_t dst_h,
                         int32_t src_x, int32_t src_y, int32_t src_w, int32_t src_h,
                         int32_t image_w, int32_t image_h);

/** setRenderingHint: a no-op, matching web/graphics.js's (antialiasing is
 * set at GL-context creation there; here it's whatever the platform's GL
 * context asked for). Kept so call sites (Plane.d()'s Madness.anti check)
 * port as a direct, harmless call rather than being special-cased away. */
void gfx_set_rendering_hint(Graphics2D *g);

// --- introspection, for tests -- mirrors web/graphics.js's vertexAt/colorAt.
void gfx_vertex_at(const Graphics2D *g, int32_t i, float *x, float *y);
void gfx_color_at(const Graphics2D *g, int32_t i, int32_t *r, int32_t *gg, int32_t *b, int32_t *a);

#ifdef __cplusplus
}
#endif

#endif
