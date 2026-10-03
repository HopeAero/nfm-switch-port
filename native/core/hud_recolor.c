// See hud_recolor.h for scope, including the one deliberate departure
// from web/images.js's own loadsnap() (the cornerOpaque fallback).
#include "hud_recolor.h"

// Math.trunc(v) then clamp to [0,255], matching web/images.js's own
// clamp255. images.js does plain JS double arithmetic throughout (no
// fr()-style float wrapping -- it's not part of the original decompiled
// Java transpile pipeline the fr()/trunc() convention exists for), so
// this stays in `double` end to end rather than rounding through
// `float` anywhere.
static uint8_t clamp255(double v) {
  int32_t n = (int32_t)v; // truncates toward zero, matching Math.trunc
  if (n > 255) return 255;
  if (n < 0) return 0;
  return (uint8_t)n;
}

void hud_recolor(uint8_t *rgba, int32_t width, int32_t height, const int32_t snap[3]) {
  int32_t pixel_count = width * height;
  // Reference background: bottom-right pixel -- see this file's header
  // comment on why this reads the real RGB unconditionally rather than
  // porting the JS's `cornerOpaque ? real : 192` fallback.
  int32_t last_o = (pixel_count - 1) * 4;
  double ref_r = (double)rgba[last_o + 0];

  for (int32_t i = 0; i < pixel_count; i++) {
    int32_t o = i * 4;
    uint8_t r = rgba[o], g = rgba[o + 1], b = rgba[o + 2];
    if (rgba[o + 3] == 0) continue; // already transparent; leave it be

    if (r != g && g != b) {
      // Coloured: tint and force opaque.
      rgba[o + 0] = clamp255((double)r + (double)r * ((double)snap[0] / 100.0));
      rgba[o + 1] = clamp255((double)g + (double)g * ((double)snap[1] / 100.0));
      rgba[o + 2] = clamp255((double)b + (double)b * ((double)snap[2] / 100.0));
      rgba[o + 3] = 255;
    } else {
      // Grey: black, with coverage from the distance below the reference.
      uint8_t a = clamp255(((ref_r - (double)r) / ref_r) * 255.0);
      rgba[o + 0] = 0;
      rgba[o + 1] = 0;
      rgba[o + 2] = 0;
      rgba[o + 3] = a;
    }
  }
}
