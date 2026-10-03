// Host-buildable test for native/core/java_compat.c and trig.c.
// Expected values are copied verbatim from web/java.test.js and
// web/trig.js's own test, which were themselves checked against the real
// JDK / V8 (see web/TRANSPILE_SPEC.md §6). This file does not re-derive
// them; it only checks the C port agrees with the already-verified JS.
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../core/java_compat.h"
#include "../core/trig.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
  if (!(cond)) { fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); failures++; } \
} while (0)

static void test_jtrunc(void) {
  CHECK(jtrunc(3.9f) == 3, "jtrunc(3.9)");
  CHECK(jtrunc(-3.9f) == -3, "jtrunc(-3.9)");
  CHECK(jtrunc(NAN) == 0, "jtrunc(NaN)");
  CHECK(jtrunc(1e18f) == 2147483647, "jtrunc(1e18) saturates high");
  CHECK(jtrunc(-1e18f) == -2147483648, "jtrunc(-1e18) saturates low");
}

static void test_jround(void) {
  CHECK(jround(2.5f) == 3, "jround(2.5)");
  CHECK(jround(-2.5f) == -2, "jround(-2.5)"); // Java: -2, not round-half-to-even
  CHECK(jround(-2.6f) == -3, "jround(-2.6)");
}

static void test_int32_wrap(void) {
  // -fwrapv is what makes this defined behaviour instead of UB.
  int32_t a = 2147483647;
  a = (int32_t)((int64_t)a + 1);
  CHECK(a == -2147483648, "int32 add wraps");
  int32_t b = (int32_t)((int64_t)2147483647 * 2);
  CHECK(b == -2, "int32 mul wraps");
}

static void test_java_random(void) {
  // Same seed and expected sequence as web/java.test.js.
  JavaRandom r;
  jrandom_init(&r, 167118381);
  CHECK(fabs(jrandom_next_double(&r) - 0.24311774345140946) < 1e-15, "nextDouble 1");
  CHECK(fabs(jrandom_next_double(&r) - 0.9222107107077597) < 1e-15, "nextDouble 2");
  CHECK(fabs(jrandom_next_double(&r) - 0.9294754657298414) < 1e-15, "nextDouble 3");
  CHECK(fabs(jrandom_next_double(&r) - 0.5536601129130871) < 1e-15, "nextDouble 4");
  CHECK(fabs(jrandom_next_double(&r) - 0.9612418302178641) < 1e-15, "nextDouble 5");
  CHECK(jrandom_next_int(&r) == 212580432, "nextInt 1");
  CHECK(jrandom_next_int(&r) == 1789041211, "nextInt 2");
  CHECK(jrandom_next_int(&r) == 640489841, "nextInt 3");
  CHECK(jrandom_next_int_bound(&r, 100) == 35, "nextInt(100) 1");
  CHECK(jrandom_next_int_bound(&r, 100) == 17, "nextInt(100) 2");
  CHECK(jrandom_next_int_bound(&r, 100) == 74, "nextInt(100) 3");
  CHECK(jrandom_next_float(&r) == (float)0.69909424, "nextFloat");
  CHECK(jrandom_next_boolean(&r) == true, "nextBoolean");

  JavaRandom r0;
  jrandom_init(&r0, 0);
  CHECK(fabs(jrandom_next_double(&r0) - 0.730967787376657) < 1e-15, "nextDouble seed 0");
}

static void test_rgb_hsb(void) {
  float out[3];
  rgb_to_hsb(177, 171, 160, out);
  CHECK(fabsf(out[0] - 0.10784314f) < 1e-6f, "RGBtoHSB h");
  CHECK(fabsf(out[1] - 0.09604520f) < 1e-6f, "RGBtoHSB s");
  CHECK(fabsf(out[2] - 0.69411767f) < 1e-6f, "RGBtoHSB b");

  rgb_to_hsb(0, 0, 0, out);
  CHECK(out[0] == 0 && out[1] == 0 && out[2] == 0, "RGBtoHSB black");

  rgb_to_hsb(255, 0, 0, out);
  CHECK(out[0] == 0 && out[1] == 1 && out[2] == 1, "RGBtoHSB red");

  int triples[4][3] = {{177,171,160},{12,200,43},{255,255,255},{1,2,3}};
  for (int i = 0; i < 4; i++) {
    int r = triples[i][0], g = triples[i][1], b = triples[i][2];
    rgb_to_hsb(r, g, b, out);
    int32_t rgb = hsb_to_rgb(out[0], out[1], out[2]);
    CHECK(((rgb >> 16) & 255) == r, "HSBtoRGB roundtrip r");
    CHECK(((rgb >> 8) & 255) == g, "HSBtoRGB roundtrip g");
    CHECK((rgb & 255) == b, "HSBtoRGB roundtrip b");
  }
}

static void test_trig_tables(void) {
  for (int i = 0; i < 360; i++) {
    double rad = i * 0.017453292519943295;
    float wantCos = (float)cos(rad);
    float wantSin = (float)sin(rad);
    CHECK(TCOS[i] == wantCos, "TCOS entry");
    CHECK(TSIN[i] == wantSin, "TSIN entry");
  }
}

int main(void) {
  test_jtrunc();
  test_jround();
  test_int32_wrap();
  test_java_random();
  test_rgb_hsb();
  test_trig_tables();
  if (failures == 0) {
    printf("all tests passed\n");
    return 0;
  }
  fprintf(stderr, "%d failure(s)\n", failures);
  return 1;
}
