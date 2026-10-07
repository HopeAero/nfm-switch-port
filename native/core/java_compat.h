// ports web/java.js
//
// Java numeric + library semantics, for C. See vita/PORT_SPEC.md §1 for why
// most of web/java.js needs no C equivalent at all: `int32_t` arithmetic
// under `-fwrapv` already wraps like Java, and `float` arithmetic already
// rounds to float32 at every step like Java's `float` does. What's left here
// is the part C does NOT give you for free: saturating truncation, Java's
// exact round-half-up, the JDK's seeded LCG, and Color.RGBtoHSB/HSBtoRGB.
#ifndef NFM_JAVA_COMPAT_H
#define NFM_JAVA_COMPAT_H

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

// Java `(int)` cast of a float/double: truncates toward zero, NaN -> 0,
// saturates at INT32_MIN/MAX rather than wrapping. Plain `(int32_t)x` in C
// is undefined behaviour outside int32 range, so use this instead of a raw
// cast for any value that can plausibly overflow (car coordinates do, see
// web/TRANSPILE_SPEC.md §2b).
// static inline: called per vertex from several translation units, and
// without LTO an out-of-line call there cost more than the body.
static inline int32_t jtrunc(float x) {
  if (isnan(x)) return 0;
  if (x >= 2147483647.0f) return 2147483647;
  if (x <= -2147483648.0f) return -2147483648;
  return (int32_t)x;
}

// Same, but for the common JS shape `trunc(fr(X) + intA - intB)`: fr(X) is
// a single float32-rounded value, but the JS then combines it with exact
// integers in double precision (JS numbers are always double) BEFORE
// truncating -- not a second float32 rounding. Use this whenever the value
// being truncated is genuinely double in the JS (no outer fr() wrapping the
// whole expression), to avoid a double-rounding mismatch against `jtrunc`.
static inline int32_t jtrunc_d(double x) {
  if (isnan(x)) return 0;
  if (x >= 2147483647.0) return 2147483647;
  if (x <= -2147483648.0) return -2147483648;
  return (int32_t)x;
}

/** Java int division, but with web/java.js's idiv where Java throws: x / 0
 * is 0 (Java: ArithmeticException; C: undefined, SIGFPE on x86, 0 on
 * AArch64). INT_MIN / -1 wraps to INT_MIN as Java does. For divisors that
 * come from stage or car data; a constant or clamped divisor needs plain `/`. */
static inline int32_t jdiv(int32_t a, int32_t b) {
  if (b == 0) return 0;
  if (b == -1) return (int32_t)(0u - (uint32_t)a);
  return a / b;
}

// Java `Math.round(float)` -> int: floor(x + 0.5), NOT round-half-to-even.
int32_t jround(float x);

// java.lang.Math.random() replacement: xorshift32, matches web/java.js's
// two-stream split (sim vs draw) for the same lockstep-netplay reason.
void nfm_set_seed(uint32_t s);
void nfm_set_draw_phase(bool on);
bool nfm_in_draw_phase(void);
double nfm_random(void); // [0, 1)

// java.util.Random — exact 48-bit JDK LCG. Ports web/java.js's JavaRandom.
typedef struct {
  uint64_t seed; // low 48 bits significant
} JavaRandom;

void jrandom_init(JavaRandom *r, int64_t seed);
int32_t jrandom_next(JavaRandom *r, int bits);
int32_t jrandom_next_int(JavaRandom *r);
int32_t jrandom_next_int_bound(JavaRandom *r, int32_t bound);
double jrandom_next_double(JavaRandom *r);
float jrandom_next_float(JavaRandom *r);
bool jrandom_next_boolean(JavaRandom *r);

// java.awt.Color.RGBtoHSB. Writes h,s,b into out[3].
void rgb_to_hsb(int r, int g, int b, float out[3]);

// java.awt.Color.HSBtoRGB. Returns packed 0xRRGGBB.
int32_t hsb_to_rgb(float hue, float saturation, float brightness);

#ifdef __cplusplus
}
#endif

#endif
