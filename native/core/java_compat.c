// ports web/java.js
//
// Requires -fwrapv (see vita/CMakeLists.txt and vita/tests/CMakeLists.txt):
// this file relies on signed int32 arithmetic wrapping like Java's, which is
// undefined behaviour in standard C without it.
#include "java_compat.h"
#include <math.h>

int32_t jround(float x) {
  return (int32_t)floorf(x + 0.5f);
}

// --- deterministic PRNG (xorshift32, sim/draw split) ------------------------

static uint32_t g_seed = 0x2545f491u;
static uint32_t g_draw_seed = 0x9e3779b9u;
static bool g_draw_phase = false;

void nfm_set_seed(uint32_t s) {
  g_seed = s ? s : 1u;
  g_draw_seed = (g_seed ^ 0x9e3779b9u);
  if (!g_draw_seed) g_draw_seed = 1u;
}

void nfm_set_draw_phase(bool on) { g_draw_phase = on; }
bool nfm_in_draw_phase(void) { return g_draw_phase; }

double nfm_random(void) {
  uint32_t x = g_draw_phase ? g_draw_seed : g_seed;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  if (g_draw_phase) g_draw_seed = x; else g_seed = x;
  return (double)x / 4294967296.0;
}

// --- java.util.Random --------------------------------------------------------

#define JR_MULT 0x5deece66dULL
#define JR_ADDEND 0xbULL
#define JR_MASK ((1ULL << 48) - 1ULL)

void jrandom_init(JavaRandom *r, int64_t seed) {
  r->seed = ((uint64_t)seed ^ JR_MULT) & JR_MASK;
}

int32_t jrandom_next(JavaRandom *r, int bits) {
  r->seed = (r->seed * JR_MULT + JR_ADDEND) & JR_MASK;
  // top `bits` bits of the 48-bit state, sign-extended from bit 31 the way
  // Java's implicit (int) narrowing does.
  uint32_t shifted = (uint32_t)(r->seed >> (48 - bits));
  return (int32_t)shifted;
}

int32_t jrandom_next_int(JavaRandom *r) {
  return jrandom_next(r, 32);
}

int32_t jrandom_next_int_bound(JavaRandom *r, int32_t bound) {
  if ((bound & -bound) == bound) { // power of two
    return (int32_t)(((int64_t)bound * (int64_t)jrandom_next(r, 31)) >> 31);
  }
  int32_t bits, val;
  do {
    bits = jrandom_next(r, 31);
    val = bits % bound;
  } while (bits - val + (bound - 1) < 0);
  return val;
}

double jrandom_next_double(JavaRandom *r) {
  int64_t hi = jrandom_next(r, 26);
  int64_t lo = jrandom_next(r, 27);
  return (double)((hi * 134217728LL) + lo) / 9007199254740992.0;
}

float jrandom_next_float(JavaRandom *r) {
  return (float)jrandom_next(r, 24) / (float)(1 << 24);
}

bool jrandom_next_boolean(JavaRandom *r) {
  return jrandom_next(r, 1) != 0;
}

// --- java.awt.Color -----------------------------------------------------------

void rgb_to_hsb(int r, int g, int b, float out[3]) {
  float hue, saturation, brightness;
  int cmax = r > g ? r : g;
  if (b > cmax) cmax = b;
  int cmin = r < g ? r : g;
  if (b < cmin) cmin = b;

  brightness = (float)cmax / 255.0f;
  saturation = cmax != 0 ? (float)(cmax - cmin) / (float)cmax : 0.0f;

  if (saturation == 0) {
    hue = 0;
  } else {
    float redc = (float)(cmax - r) / (float)(cmax - cmin);
    float greenc = (float)(cmax - g) / (float)(cmax - cmin);
    float bluec = (float)(cmax - b) / (float)(cmax - cmin);
    if (r == cmax) hue = bluec - greenc;
    else if (g == cmax) hue = 2.0f + redc - bluec;
    else hue = 4.0f + greenc - redc;
    hue = hue / 6.0f;
    if (hue < 0) hue = hue + 1.0f;
  }

  out[0] = hue;
  out[1] = saturation;
  out[2] = brightness;
}

int32_t hsb_to_rgb(float hue, float saturation, float brightness) {
  int r = 0, g = 0, b = 0;
  if (saturation == 0) {
    r = g = b = (int)(brightness * 255.0f + 0.5f);
  } else {
    float h = (hue - floorf(hue)) * 6.0f;
    float f = h - floorf(h);
    float p = brightness * (1.0f - saturation);
    float q = brightness * (1.0f - saturation * f);
    float t = brightness * (1.0f - saturation * (1.0f - f));
    switch ((int)h) {
      case 0: r = (int)(brightness * 255.0f + 0.5f); g = (int)(t * 255.0f + 0.5f); b = (int)(p * 255.0f + 0.5f); break;
      case 1: r = (int)(q * 255.0f + 0.5f); g = (int)(brightness * 255.0f + 0.5f); b = (int)(p * 255.0f + 0.5f); break;
      case 2: r = (int)(p * 255.0f + 0.5f); g = (int)(brightness * 255.0f + 0.5f); b = (int)(t * 255.0f + 0.5f); break;
      case 3: r = (int)(p * 255.0f + 0.5f); g = (int)(q * 255.0f + 0.5f); b = (int)(brightness * 255.0f + 0.5f); break;
      case 4: r = (int)(t * 255.0f + 0.5f); g = (int)(p * 255.0f + 0.5f); b = (int)(brightness * 255.0f + 0.5f); break;
      case 5: r = (int)(brightness * 255.0f + 0.5f); g = (int)(p * 255.0f + 0.5f); b = (int)(q * 255.0f + 0.5f); break;
    }
  }
  return (int32_t)((r << 16) | (g << 8) | b);
}
