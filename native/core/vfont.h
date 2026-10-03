// A minimal stroke/vector font for HUD and menu text.
//
// NOT a port of any web/*.js file (same category as main.c -- see
// PORT_SPEC.md §5). web/graphics.js draws text via the browser's own
// Canvas 2D `drawString`/overlay text layer (see gfx.h's own banner
// comment: "overlay/text handling excluded -- not ported"), which has
// no native equivalent to translate -- there is nothing to port here,
// only something to build. `core/gfx.c` is a pure flat-colour vector
// rasterizer (fillPolygon/drawLine/fillRect/...) with no text or
// image/texture support on either platform, so the menu (M3) and HUD
// (M3) both need real text and both go through this module.
//
// Design: each glyph is one or more POLYLINES ("strokes") of points on
// a 5-wide x 7-tall grid (x: 0..4, y: 0..6), drawn through the EXISTING
// gfx_draw_line primitive -- not a new texture/bitmap pipeline, and not
// an SDL_ttf dependency, so this works identically on Vita's vitaGL
// backend (same vertex-list submission as everything else Graphics2D
// draws) without that backend needing its own separate story. Digits
// are drawn as a classic 7-segment layout (highly legible at HUD sizes,
// cheap to encode); letters are hand-authored blocky single/multi-stroke
// shapes in the same grid so every character advances by the same cell
// width and lines up in a monospace grid.
//
// Covers: space, '0'-'9', 'A'-'Z' (only -- callers should uppercase
// first, see vfont_draw_string's own doc comment), and a small
// punctuation set: '.' ',' ':' '-' '/' '%' '!' '\'' '+'. Anything else
// falls back to a blank cell (advances the cursor, draws nothing) --
// deliberately, rather than asserting/crashing, so a stray unsupported
// byte in a car/track name never takes down the menu.
#ifndef NFM_VFONT_H
#define NFM_VFONT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Graphics2D;

// Glyph cell size, before the caller's own `scale`: 5 wide, 7 tall, plus
// 1 unit of inter-character spacing (see vfont_draw_string).
#define VFONT_CELL_W 5
#define VFONT_CELL_H 7
#define VFONT_ADVANCE (VFONT_CELL_W + 1)

/**
 * Draws `s` starting with its top-left corner at (x, y) in the same
 * game-space pixel coordinates every other gfx_* call uses (y-down,
 * origin top-left). Each glyph cell is `scale` pixels per grid unit, so
 * one character occupies `VFONT_ADVANCE * scale` pixels horizontally and
 * `VFONT_CELL_H * scale` vertically. Uses `g`'s CURRENT color (set via
 * gfx_set_color before calling, same convention as every other gfx_*
 * draw call) and `line_width` for stroke thickness.
 *
 * Only 'A'-'Z' are defined -- lowercase letters are folded to uppercase
 * automatically (this is a HUD/menu font, not a general-purpose one).
 */
void vfont_draw_string(struct Graphics2D *g, const char *s, int32_t x, int32_t y,
                        int32_t scale, float line_width);

/** Pixel width `vfont_draw_string` would occupy for `s` at this scale --
 * for centering/right-aligning labels. Does NOT include a trailing
 * inter-character gap after the last glyph. */
int32_t vfont_text_width(const char *s, int32_t scale);

#ifdef __cplusplus
}
#endif

#endif
