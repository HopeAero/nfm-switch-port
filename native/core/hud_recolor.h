// Ports web/images.js's loadsnap() -- the per-pixel recolour step every
// HUD/menu image asset goes through before display.
//
// Not part of core/xt_graphics.h's own scope (XtGraphics.js itself,
// 2578 lines, is out of scope -- see that header's own comment) --
// `loadsnap`/`pixelsOf`/`loadHudImages` actually live in the separate
// web/images.js, conceptually "XtGraphics's own image-loading step" per
// that file's own comments, but physically a standalone function this
// port can pull in on its own without needing the rest of XtGraphics.js.
//
// What it does, per web/images.js's own comment on loadsnap(): every
// pixel that isn't already transparent gets ONE of two treatments --
// COLOURED pixels (r!=g && g!=b) are tinted by `snap[]` (Medium's own
// per-stage palette shift, the same one that biases sky/track colours)
// and forced opaque; GREY pixels (r==g || g==b) become BLACK with an
// alpha derived from how far below a reference white they sit -- how
// the original fakes antialiased edges out of assets with no real
// alpha channel (the grey ramp around a glyph becomes coverage).
//
// One deliberate DEPARTURE from the JS here, not an oversight: the JS's
// own `cornerOpaque` special case (see its own long comment) works
// around a BROWSER CANVAS quirk -- `ctx.getImageData` zeroes the RGB
// under a GIF-transparent pixel, unlike Java's original PixelGrabber,
// which (per that same JS comment) "hands back the palette RGB even
// for the transparent index". This port's own gif_decode.c/png_decode.c
// already behave like PixelGrabber, not like canvas -- they never zero
// a pixel's RGB just because its alpha is 0 (see gif_decode.c's own
// `rgba[i*4+0..2] = c.r/g/b` regardless of the transparency check right
// next to it). So the reference pixel's real RGB is always readable
// here, and this reads it directly and unconditionally instead of
// porting the JS's `cornerOpaque ? real : 192` fallback -- MORE
// faithful to the original Java than the JS itself needed to be, not
// less, since there's no canvas-shaped hole to patch over. Verified
// against 4 real assets (gif_decode.c decodes each one's actual corner
// pixel as (192,192,192,alpha) regardless of alpha, matching what the
// JS's own comment says Java always saw) before choosing this over a
// literal transcription of the JS's workaround.
#ifndef NFM_HUD_RECOLOR_H
#define NFM_HUD_RECOLOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Recolours `rgba` (width*height*4 bytes, RGBA8888 -- the direct output
 * of gif_decode.c/png_decode.c/jpeg_decode.c) IN PLACE, matching
 * loadsnap(bitmap, snap)'s own `ctx.putImageData` overwrite of the same
 * buffer it read. `snap` is Medium.snap (3 percentages, e.g. -40..40,
 * may be all 0).
 */
void hud_recolor(uint8_t *rgba, int32_t width, int32_t height, const int32_t snap[3]);

// The port's dark-sky HUD (web/images.js's `?hud=auto`): where the Java
// drew boxes behind the HUD (Medium.darksky), the ink itself is moved along
// HSB brightness, keeping its hue, until it reads 4.5:1 against the sky.
#define HUD_MIN_CONTRAST 4.5

/** WCAG 2 contrast ratio of two RGB colours, 1 to 21. */
double hud_contrast(const int32_t a[3], const int32_t b[3]);

/** `c`, or the nearest brightness of its hue that reads against `bg`. */
void hud_readable(const int32_t c[3], const int32_t bg[3], int32_t out[3]);

/** hud_readable() on every visible pixel of a hud_recolor()'d sprite; alpha kept. */
void hud_adapt_ink(uint8_t *rgba, int32_t width, int32_t height, const int32_t bg[3]);

#ifdef __cplusplus
}
#endif

#endif
