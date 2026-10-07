// See hud_recolor.h for scope, including the one deliberate departure
// from web/images.js's own loadsnap() (the cornerOpaque fallback).
#include "hud_recolor.h"
#include "java_compat.h"
#include <math.h>
#include <stdbool.h>
#include <stddef.h>

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

// ---- contrast-aware HUD ink on dark skies (web/images.js readable/adaptInk) --
// WCAG 2 relative luminance and contrast ratio, 1:1 (invisible) to 21:1.
static double lin(int32_t c) {
  const double v = c / 255.0;
  return v <= 0.04045 ? v / 12.92 : pow((v + 0.055) / 1.055, 2.4);
}
static double luminance(const int32_t c[3]) {
  return 0.2126 * lin(c[0]) + 0.7152 * lin(c[1]) + 0.0722 * lin(c[2]);
}
double hud_contrast(const int32_t a[3], const int32_t b[3]) {
  const double x = luminance(a), y = luminance(b);
  return ((x > y ? x : y) + 0.05) / ((x > y ? y : x) + 0.05);
}

void hud_readable(const int32_t c[3], const int32_t bg[3], int32_t out[3]) {
  out[0] = c[0]; out[1] = c[1]; out[2] = c[2];
  if (hud_contrast(c, bg) >= HUD_MIN_CONTRAST) return;
  float hsb[3];
  rgb_to_hsb(c[0], c[1], c[2], hsb);
  // Lighter on a dark background, darker on a light one, keeping the hue.
  const int32_t white[3] = {255, 255, 255}, black[3] = {0, 0, 0};
  const bool up = hud_contrast(white, bg) >= hud_contrast(black, bg);
  for (int32_t k = 1; k <= 20; k++) {
    const float v = up ? hsb[2] + (1.0f - hsb[2]) * (float)k / 20.0f : hsb[2] * (1.0f - (float)k / 20.0f);
    const float s = up ? hsb[1] * (1.0f - 0.5f * (float)k / 20.0f) : hsb[1];
    const int32_t x = hsb_to_rgb(hsb[0], s, v);
    const int32_t rgb[3] = {(x >> 16) & 0xff, (x >> 8) & 0xff, x & 0xff};
    if (hud_contrast(rgb, bg) >= HUD_MIN_CONTRAST) {
      out[0] = rgb[0]; out[1] = rgb[1]; out[2] = rgb[2];
      return;
    }
  }
  const int32_t *end = up ? white : black;
  out[0] = end[0]; out[1] = end[1]; out[2] = end[2];
}

void hud_adapt_ink(uint8_t *rgba, int32_t width, int32_t height, const int32_t bg[3]) {
  // ponytail: one-entry memo -- the sprites are runs of a few flat colours.
  int32_t last[3] = {-1, -1, -1}, mapped[3] = {0, 0, 0};
  for (int32_t i = 0; i < width * height; i++) {
    uint8_t *px = rgba + (size_t)i * 4;
    if (px[3] == 0) continue;
    const int32_t c[3] = {px[0], px[1], px[2]};
    if (c[0] != last[0] || c[1] != last[1] || c[2] != last[2]) {
      hud_readable(c, bg, mapped);
      last[0] = c[0]; last[1] = c[1]; last[2] = c[2];
    }
    px[0] = (uint8_t)mapped[0]; px[1] = (uint8_t)mapped[1]; px[2] = (uint8_t)mapped[2];
  }
}
