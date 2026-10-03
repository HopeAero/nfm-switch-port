// A tiny baked bitmap font (0-9, /, space, colon, period, dash, percent),
// for the HUD numbers XtGraphics.js draws with the browser's native `12px
// Arial` via Canvas drawString (see JS lines 1393-1396, 2338-2339). Not a
// port of graphics.js -- see gfx.h's own banner about text/image being
// deliberately excluded from that module. This module fills the gap for
// the specific HUD digits/separator glyphs, with real Arial-style glyph
// shapes so the port matches the JS visually, not just semantically.
//
// The alternative -- our own vector `vfont` at scale=1 -- is 5x7 pixels
// per glyph with 1px stroke width, which is unreadably small next to the
// original's crisp 12px Arial and had `os números que representam a volta
// e wasted [ficando] minusculos` as the immediate visible symptom the
// user flagged. This module renders each glyph pixel as a 1x1
// gfx_fill_rect quad (see bitfont.c) -- no new texture pipeline or
// source-rect draw API needed on top of what gfx.c already has, works
// identically on Linux and Vita, and the font data ships baked into the
// binary as a `uint8_t` array (see bitfont.c's own header comment) so
// there's no runtime file-load dependency and no Vita asset packaging
// step for the font.
//
// Baked from LiberationSans-Bold (a metric-compatible open substitute for
// Arial-Bold, indistinguishable at this size), regenerable via
// scratchpad/bake_bitfont.py -- see that script's header for why 1:1
// against the JS's original `12px Arial`.
#ifndef NFM_BITFONT_H
#define NFM_BITFONT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Graphics2D;

typedef struct {
  int32_t ascii;
  int32_t advance;    // pixels the cursor moves after drawing this glyph
  int32_t bit_offset; // where this glyph's bits start in BITFONT_BITS
} BitfontGlyph;

extern const int32_t BITFONT_CELL_W;
extern const int32_t BITFONT_CELL_H;
extern const int32_t BITFONT_GLYPH_COUNT;
extern const BitfontGlyph BITFONT_GLYPHS[];
extern const uint8_t BITFONT_BITS[];
extern const int32_t BITFONT_BITS_LEN;

/**
 * Draws `s` starting with its top-left corner at (x, y) in the same game-
 * space pixel coordinates as every other gfx_* call. Uses `g`'s CURRENT
 * color (set via gfx_set_color before calling, same convention as
 * gfx_fill_rect/vfont_draw_string). Unsupported characters (anything not
 * in the small covered set -- see bitfont.h's own header comment) fall
 * back to `space`'s advance width, silently, matching vfont_draw_string's
 * "advance the cursor, draw nothing" fallback rather than
 * asserting/crashing.
 */
void bitfont_draw_string(struct Graphics2D *g, const char *s, int32_t x, int32_t y);

/** Total pixel width `bitfont_draw_string` would advance for `s` -- used
 * for right-aligning or centering. Does NOT include a trailing padding
 * after the last glyph. */
int32_t bitfont_text_width(const char *s);

#ifdef __cplusplus
}
#endif

#endif
