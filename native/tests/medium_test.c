// Host-buildable test for native/core/medium.c (the partial port -- see
// native/core/medium.h for what's implemented).
//
// No web/Medium.test.js exists, so -- same as trackers_test.c -- the
// expected values were captured by running the real web/Medium.js under
// Node (see the commit that added this file for the scripts) and are
// reproduced here as literals. nfm_random()'s agreement with the JS
// random() it must match is checked separately, first, since medium_init's
// mgen/gofo and medium_random() both depend on it entirely.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "../core/java_compat.h"
#include "../core/medium.h"
#include "../core/gfx.h"
#include "../core/cont_o.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
  if (!(cond)) { fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); failures++; } \
} while (0)

#define CHECK_FLOAT_EQ(a, b, msg) CHECK(fabsf((a) - (b)) < 1e-6f, msg)

static void test_nfm_random_matches_js(void) {
  // web/java.js `random()`, seed 12345, first value only -- the fuller
  // sequence is exercised indirectly by every other test in this file that
  // depends on nfm_random(). This is the canary: if it fails, nothing else
  // here is trustworthy.
  nfm_set_seed(12345);
  CHECK(fabs(nfm_random() - 0.776938705239445) < 1e-9, "nfm_random seed 12345 [0]");
}

static void test_init_seeded_fields(void) {
  nfm_set_seed(12345);
  Medium m;
  medium_init(&m);
  CHECK(m.mgen == 77693, "mgen");
  CHECK_FLOAT_EQ(m.gofo, 0.8595314025878906f, "gofo");
  // Non-seeded defaults, spot-checked.
  CHECK(m.focus_point == 400, "focus_point");
  CHECK(m.cx == 400 && m.cy == 225 && m.cz == 50, "cx/cy/cz");
  CHECK(m.w == 800 && m.h == 450, "w/h");
  CHECK(m.hit == 45000, "hit");
  CHECK(m.checkpoint == -1, "checkpoint");
  medium_free(&m);
}

static void test_sin_cos(void) {
  Medium m;
  medium_init(&m);
  CHECK_FLOAT_EQ(medium_cos(&m, 0), 1.0f, "cos(0)");
  CHECK_FLOAT_EQ(medium_cos(&m, 90), 6.123234262925839e-17f, "cos(90)");
  CHECK_FLOAT_EQ(medium_cos(&m, 45), 0.7071067690849304f, "cos(45)");
  CHECK_FLOAT_EQ(medium_cos(&m, 359), 0.9998477101325989f, "cos(359)");
  CHECK_FLOAT_EQ(medium_cos(&m, -10), 0.9848077297210693f, "cos(-10)");
  CHECK_FLOAT_EQ(medium_cos(&m, 370), 0.9848077297210693f, "cos(370)");
  CHECK_FLOAT_EQ(medium_cos(&m, 45.5f), 0.7008825540542603f, "cos(45.5) frac");
  CHECK_FLOAT_EQ(medium_sin(&m, 45.5f), 0.7132232785224915f, "sin(45.5) frac");
  CHECK_FLOAT_EQ(medium_sin(&m, 90), 1.0f, "sin(90)");
  medium_free(&m);
}

static void test_xs_ys(void) {
  Medium m;
  medium_init(&m);
  m.cx = 400; m.cy = 225; m.cz = 50; m.focus_point = 400;
  CHECK(medium_xs(&m, 100, 200) == -200, "xs(100,200)");
  CHECK(medium_xs(&m, 100, 30) == -2000, "xs(100,30) cz clamp");
  CHECK(medium_ys(&m, 100, 200) == -25, "ys(100,200)");
  medium_free(&m);
}

static void test_rot(void) {
  Medium m;
  medium_init(&m);
  int32_t ax[3] = {100, 200, -50};
  int32_t az[3] = {300, -100, 75};
  medium_rot(&m, ax, az, 0, 0, 45, 3);
  int32_t want_ax[3] = {-141, 212, -88};
  int32_t want_az[3] = {282, 70, 17};
  for (int i = 0; i < 3; i++) {
    char label[32];
    snprintf(label, sizeof(label), "rot ax[%d]", i);
    CHECK(ax[i] == want_ax[i], label);
    snprintf(label, sizeof(label), "rot az[%d]", i);
    CHECK(az[i] == want_az[i], label);
  }
  medium_free(&m);
}

static void test_random_sim_phase(void) {
  nfm_set_seed(999);
  Medium m;
  medium_init(&m);
  float want[25] = {
    0.1f,0.3f,0.6f,0.4f,0.0f,0.9f,0.7f,0.7f,0.2f,0.0f,0.4f,0.5f,0.3f,
    0.1f,0.8f,0.6f,0.8f,0.1f,0.9f,0.5f,0.4f,0.9f,0.2f,0.1f,0.2f,
  };
  for (int i = 0; i < 25; i++) {
    char label[32];
    snprintf(label, sizeof(label), "sim random[%d]", i);
    CHECK_FLOAT_EQ(medium_random(&m), want[i], label);
  }
  CHECK(m.cntrn == 17, "cntrn after");
  CHECK(m.trn == 1, "trn after");
  CHECK(m.rand[0] == 0 && m.rand[1] == 2 && m.rand[2] == 4, "rand after");
  CHECK(!m.diup[0] && m.diup[1] && m.diup[2], "diup after");
  medium_free(&m);
}

static void test_random_draw_phase(void) {
  nfm_set_seed(999);
  Medium m;
  medium_init(&m);
  nfm_set_draw_phase(true);
  float want[25] = {
    0.3f,0.1f,0.0f,0.6f,0.4f,0.7f,0.9f,0.7f,0.4f,0.2f,0.0f,0.1f,0.5f,
    0.3f,0.8f,0.8f,0.6f,0.5f,0.1f,0.9f,0.2f,0.2f,0.1f,0.7f,0.5f,
  };
  for (int i = 0; i < 25; i++) {
    char label[32];
    snprintf(label, sizeof(label), "draw random[%d]", i);
    CHECK_FLOAT_EQ(medium_random(&m), want[i], label);
  }
  nfm_set_draw_phase(false);
  medium_free(&m);
}

static void test_random_recording_and_replay(void) {
  nfm_set_seed(555);
  Medium m;
  medium_init(&m);
  m.recording = true;
  float recorded[10];
  float want_recorded[10] = {0.1f,0.9f,0.4f,0.8f,0.2f,0.7f,0.5f,0.5f,0.0f,0.2f};
  for (int i = 0; i < 10; i++) recorded[i] = medium_random(&m);
  for (int i = 0; i < 10; i++) {
    char label[32];
    snprintf(label, sizeof(label), "recorded[%d]", i);
    CHECK_FLOAT_EQ(recorded[i], want_recorded[i], label);
  }
  CHECK(m.rn == 10, "rn after recording");

  m.interpolating = true;
  m.rp = 0;
  // 15 calls against a 10-entry log: exercises the modulo wraparound.
  float want_replayed[15] = {
    0.1f,0.9f,0.4f,0.8f,0.2f,0.7f,0.5f,0.5f,0.0f,0.2f,
    0.1f,0.9f,0.4f,0.8f,0.2f,
  };
  for (int i = 0; i < 15; i++) {
    char label[32];
    snprintf(label, sizeof(label), "replayed[%d]", i);
    CHECK_FLOAT_EQ(medium_random(&m), want_replayed[i], label);
  }
  medium_free(&m);
}

// web/Medium.js's follow() -- expected values from running the real JS
// (setSeed(4242), fresh Medium, bcxz set as noted, {x:1000,y:-200,z:500}).
static void test_follow(void) {
  nfm_set_seed(4242);
  Medium m; medium_init(&m);
  m.bcxz = 50;
  medium_follow(&m, 1000, -200, 500, 90, 0);
  CHECK(m.zy == 10 && m.bcxz == 36 && m.xz == -126, "follow n2=0 angles");
  CHECK(m.x == 1247 && m.y == -675 && m.z == 920, "follow n2=0 position");
  medium_free(&m);

  nfm_set_seed(4242);
  Medium m2; medium_init(&m2);
  m2.bcxz = 50;
  medium_follow(&m2, 1000, -200, 500, 90, 1);
  CHECK(m2.bcxz == 64 && m2.xz == -154, "follow n2=1 angles");
  CHECK(m2.x == 950 && m2.y == -675 && m2.z == 1169, "follow n2=1 position");
  medium_free(&m2);

  nfm_set_seed(4242);
  Medium m3; medium_init(&m3);
  m3.bcxz = -300; // out of [-180,180], exercises the clamp
  medium_follow(&m3, 1000, -200, 500, 90, -1);
  CHECK(m3.bcxz == -180 && m3.xz == 90, "follow n2=-1 clamp angles");
  CHECK(m3.x == -200 && m3.y == -675 && m3.z == 450, "follow n2=-1 clamp position");
  medium_free(&m3);
}

// web/Medium.js's groundpolys() -- a synthetic 2x2 populated cell grid (as
// newpolys() would build), expected values from running the real JS
// headless (setSeed(777), see the commit that added this test for the
// full setup script). Exhaustive: all 72 vertices, not a sample.
static void test_groundpolys(void) {
  nfm_set_seed(777);
  Medium m; medium_init(&m);
  m.nrw = 2; m.ncl = 2;
  m.sgpx = -1200; m.sgpz = -1200;
  int32_t cells = 4;
  m.cgpx = malloc(sizeof(int32_t) * (size_t)cells);
  m.cgpz = malloc(sizeof(int32_t) * (size_t)cells);
  m.pmx = malloc(sizeof(int32_t) * (size_t)cells);
  m.pcv = malloc(sizeof(float) * (size_t)cells);
  m.ogpx = malloc(sizeof(int32_t *) * (size_t)cells);
  m.ogpz = malloc(sizeof(int32_t *) * (size_t)cells);
  m.pvr = malloc(sizeof(float *) * (size_t)cells);
  static const int32_t base_ox[8] = {100, 70, 0, -70, -100, -70, 0, 70};
  static const int32_t base_oz[8] = {0, 70, 100, 70, 0, -70, -100, -70};
  static const float base_pvr[8] = {1.1f, 1.2f, 1.15f, 1.25f, 1.1f, 1.2f, 1.15f, 1.25f};
  for (int32_t i = 0; i < cells; i++) {
    m.cgpx[i] = i * 300 - 200;
    m.cgpz[i] = i * 150 - 100;
    m.pmx[i] = 400 + i * 20;
    m.pcv[i] = 0.9f + (float)i * 0.02f;
    m.ogpx[i] = malloc(sizeof(int32_t) * 8);
    m.ogpz[i] = malloc(sizeof(int32_t) * 8);
    m.pvr[i] = malloc(sizeof(float) * 8);
    for (int32_t k = 0; k < 8; k++) {
      m.ogpx[i][k] = base_ox[k] + i;
      m.ogpz[i][k] = base_oz[k] - i;
      m.pvr[i][k] = base_pvr[k];
    }
  }
  m.x = 50; m.y = -80; m.z = 30;
  m.cx = 400; m.cy = 225; m.cz = 50; m.focus_point = 400;
  m.xz = 15; m.zy = 5;
  m.ground = 250 - m.y;

  Graphics2D g;
  gfx_init(&g, 800, 450);
  gfx_begin(&g);
  medium_groundpolys(&m, &g);
  CHECK(g.count == 72, "groundpolys vertex count");
  static const int32_t want_x[72] = {
    462,374,243, 462,243,69, 462,69,-160, 462,-160,-554, 462,-554,32, 462,32,522,
    677,595,519, 677,519,462, 677,462,478, 677,478,567, 677,567,704, 677,704,755,
    441,356,241, 441,241,90, 441,90,-120, 441,-120,-281, 441,-281,77, 441,77,429,
    667,593,526, 667,526,483, 667,483,487, 667,487,569, 667,569,683, 667,683,715,
  };
  static const int32_t want_y[72] = {
    423,356,352, 423,352,387, 423,387,526, 423,526,954, 423,954,1065, 423,1065,637,
    298,281,279, 298,279,290, 298,290,315, 298,315,347, 298,347,352, 298,352,326,
    426,368,360, 426,360,399, 426,399,519, 426,519,781, 426,781,885, 426,885,588,
    298,284,283, 298,283,293, 298,293,312, 298,312,337, 298,337,343, 298,343,321,
  };
  for (int32_t i = 0; i < 72; i++) {
    float x, y;
    gfx_vertex_at(&g, i, &x, &y);
    char label[32];
    snprintf(label, sizeof(label), "groundpolys v%d", i);
    CHECK((int32_t)x == want_x[i] && (int32_t)y == want_y[i], label);
  }
  gfx_free(&g);
  medium_free(&m);
}

// web/Medium.js's d() -- the world backdrop draw. Four scenarios, expected
// values from running the real JS headless (see the commit that added this
// test for the oracle script). Checked exhaustively (full vertex diff, not
// sampled) when this was written; this test spot-checks vertex count, the
// first and last few vertices' position+colour, and the post-call PRNG
// state fields, which is enough to catch a regression without embedding
// ~1500 literals across four scenarios.
static void check_d_scenario(const char *label, uint32_t seed, void (*setup)(Medium *),
                              int32_t want_count,
                              int32_t want_first_x, int32_t want_first_y,
                              int32_t want_last_x, int32_t want_last_y,
                              float want_elecr, bool want_cpflik) {
  nfm_set_seed(seed);
  Medium m;
  medium_init(&m);
  if (setup) setup(&m);
  Graphics2D g;
  gfx_init(&g, 800, 450);
  gfx_begin(&g);
  medium_d(&m, &g);

  char msg[64];
  snprintf(msg, sizeof(msg), "%s: vertex count", label);
  CHECK(g.count == want_count, msg);
  if (g.count == want_count && g.count > 0) {
    float x, y;
    gfx_vertex_at(&g, 0, &x, &y);
    snprintf(msg, sizeof(msg), "%s: first vertex", label);
    CHECK((int32_t)x == want_first_x && (int32_t)y == want_first_y, msg);
    gfx_vertex_at(&g, g.count - 1, &x, &y);
    snprintf(msg, sizeof(msg), "%s: last vertex", label);
    CHECK((int32_t)x == want_last_x && (int32_t)y == want_last_y, msg);
  }
  snprintf(msg, sizeof(msg), "%s: elecr", label);
  CHECK_FLOAT_EQ(m.elecr, want_elecr, msg);
  snprintf(msg, sizeof(msg), "%s: cpflik", label);
  CHECK(m.cpflik == want_cpflik, msg);

  gfx_free(&g);
  medium_free(&m);
}

static void setup_zy_nonzero(Medium *m) { m->zy = -30; m->y = -400; m->xz = 200; }
static void setup_resdown2(Medium *m) { m->zy = 15; m->resdown = 2; }
static void setup_lightn(Medium *m) { m->lightn = 5; m->lton = true; m->lilo = 100; }

// Medium.java:304-380's dive-and-orbit stage-preview camera. Exact camera
// x/y/z pixel positions depend on medium_cos/sin, already checked bit-for-
// bit by test_sin_cos() above, so this focuses on the state-machine
// bookkeeping (hit/fallen/atrx/atrz/zy/vxz/ptr/ptcnt/nrnd/cpflik) that's
// this method's own logic, hand-traced against GameSparker.java:2735-2749's
// arming values for the first frame, then run to settling for the rest.
static void test_aroundtrack_dive_and_settle(void) {
  nfm_set_seed(9001);
  Medium m;
  medium_init(&m);
  CheckPoints cp;
  memset(&cp, 0, sizeof(cp));
  cp.n = 1;
  cp.x[0] = 11600;
  cp.z[0] = 23200;

  // Arm exactly like GameSparker.java:2735-2749's `xtGraphics.fase == 2`
  // block (the part medium_aroundtrack itself doesn't do).
  m.trx = 4000;
  m.trz = -2000;
  m.ptr = 0;
  m.ptcnt = -10;
  m.hit = 45000;
  m.fallen = 0;
  m.nrnd = 0;

  medium_aroundtrack(&m, &cp);
  CHECK(m.hit == 45000, "aroundtrack: hit unchanged on frame 1 (fallen was still 0)");
  CHECK(m.fallen == 7, "aroundtrack: fallen += 7 every dive frame");
  CHECK(m.atrx == (11600 - 4000) / 116, "aroundtrack: atrx armed from checkpoint 0 at hit==45000");
  CHECK(m.atrz == (23200 - -2000) / 116, "aroundtrack: atrz armed from checkpoint 0 at hit==45000");
  CHECK(m.trx == 4000 + (11600 - 4000) / 116, "aroundtrack: trx nudged by atrx");
  CHECK(m.trz == -2000 + (23200 - -2000) / 116, "aroundtrack: trz nudged by atrz");
  CHECK(m.zy == 67 && m.fo == 1.0f && m.focus_point == 400, "aroundtrack: hit==45000 arming");
  // medium_init seeds vxz=180 (Medium.java's own constructor default, see
  // medium.c:66) -- aroundtrack advances it 3deg/frame while diving.
  CHECK(m.vxz == 183 && m.xz == -273, "aroundtrack: orbit angle advances 3deg/frame while diving");
  CHECK(m.cpflik == true, "aroundtrack: cpflik toggles every frame");

  // NOTE: the hit==20000 special case (Medium.java:315-322, a second
  // fog/zy burst partway down the dive) never actually fires from this
  // arming -- verified by hand-tracing the exact hit/fallen integer
  // sequence starting at hit=45000/fallen=0: fallen caps at 500 well
  // before the cumulative descent could land exactly on 20000. This is
  // the SAME arithmetic the Java runs (GameSparker.java arms hit=45000/
  // fallen=0 identically), so the branch is equally dead there -- not a
  // porting bug, just an authentic vestigial case preserved as-is.
  int frames = 1;
  while (m.hit > 5000) {
    medium_aroundtrack(&m, &cp);
    frames++;
    CHECK(frames < 100000, "aroundtrack: dive settles in a bounded number of frames");
    if (frames >= 100000) break;
  }
  CHECK(m.hit == 5000, "aroundtrack: settles to exactly hit==5000");
  CHECK(m.fallen == 0, "aroundtrack: fallen resets to 0 on settling");

  bool cpflik_before = m.cpflik;
  int32_t nrnd_before = m.nrnd;
  for (int i = 0; i < 50; i++) {
    medium_aroundtrack(&m, &cp);
    CHECK(m.hit == 5000, "aroundtrack: hit stays pinned at 5000 once settled");
  }
  CHECK(m.nrnd > nrnd_before, "aroundtrack: ptr wraps a 1-checkpoint stage every 8 frames, advancing nrnd");
  CHECK(m.cpflik == cpflik_before, "aroundtrack: cpflik back to the same phase after an even frame count");

  medium_free(&m);
}

// Medium.java:382-431's pre-race starting-grid flyby camera. Like
// medium_aroundtrack, the x/y/z/zy camera pose depends on medium_cos/sin
// (already bit-checked by test_sin_cos) and sqrt/atan (libm, not this
// port's code) -- calls them directly here to build the expected values
// from the SAME formula the header doc comment describes, independently
// of medium_around's own source, rather than re-deriving the trig by
// hand. This still catches real integration bugs (wrong field, wrong
// sign, mixed-up co/m pointers) since it's authored fresh from the
// Java, not copied from medium.c.
static void test_around_fast_intro(void) {
  Medium m;
  medium_init(&m);
  // Arm exactly like GameSparker.java:958-963's `xtGraphics.starcnt ==
  // 130` one-time arming (the fast/"b=true" intro flyby's own setup).
  m.adv = 1900;
  m.zy = 40;
  m.vxz = 70;

  ContO co;
  memset(&co, 0, sizeof(co));
  co.x = 1000;
  co.y = -100;
  co.z = 2000;

  medium_around(&m, &co, true);

  CHECK(m.adv == 1900 - 14, "around(b=true): adv -= 14, above the 617 floor");
  int32_t n = 500 + m.adv;
  CHECK(n == 2386, "around(b=true): n = 500+adv (not clamped, already >= 1300/1000)");
  CHECK(m.y == co.y - (1900 - 14), "around: y = co.y - adv");
  int32_t radius_term = co.x - n - co.x; // Medium.java:412-413's own identity, = -n
  int32_t expect_x = co.x + jtrunc((float)radius_term * medium_cos(&m, 70.0f));
  int32_t expect_z = co.z + jtrunc((float)radius_term * medium_sin(&m, 70.0f));
  CHECK(m.x == expect_x, "around(b=true): x from radius_term*cos(vxz)");
  CHECK(m.z == expect_z, "around(b=true): z from radius_term*sin(vxz)");
  CHECK(m.vxz == 74, "around(b=true): vxz += 4");
  CHECK(m.xz == -74 + 90, "around: xz = -vxz + 90");
  int32_t y_clamped = m.y > 0 ? 0 : m.y;
  int32_t n2 = (co.y - y_clamped - m.cy < 0) ? -180 : 0;
  int32_t dz = co.z - m.z + m.cz;
  int32_t dx = co.x - m.x - m.cx;
  int32_t dist = jtrunc_d(sqrt((double)(dz * dz + dx * dx)));
  int32_t n3 = jtrunc_d(90.0 + (double)n2 -
                         atan((double)dist / (double)(co.y - y_clamped - m.cy)) / 0.017453292519943295);
  n3 -= 15; // b == true
  int32_t expect_zy = 40 + (n3 - 40) / 10;
  CHECK(m.zy == expect_zy, "around(b=true): zy += (n3-zy)/10, with the b==true -15 offset");

  // Run the "ordinary" (b=false) mode for a few hundred frames and check
  // adv actually oscillates inside [-500, 900] rather than drifting or
  // getting stuck -- the vert flip-flop is the whole point of this mode
  // (used by the player-selectable "around" race view, not currently
  // wired into this port, but the camera math itself should still behave
  // like a real orbit rather than a one-way ramp).
  // co2.y is deliberately very negative so `m.y = co2.y - adv` never goes
  // above Medium.java:409's own `if (this.y > 10) { this.vert = false; }`
  // early-reset threshold across the whole adv range being explored here
  // -- with the first test's co (co.y == -100) that check fires whenever
  // adv drops below -110, cutting the oscillation short before it ever
  // reaches anywhere near -500.
  Medium m2;
  medium_init(&m2);
  ContO co2;
  memset(&co2, 0, sizeof(co2));
  co2.y = -2000;
  int32_t min_adv = m2.adv, max_adv = m2.adv;
  for (int i = 0; i < 2000; i++) {
    medium_around(&m2, &co2, false);
    if (m2.adv < min_adv) min_adv = m2.adv;
    if (m2.adv > max_adv) max_adv = m2.adv;
  }
  // The +2/-500 and >900 checks run AFTER the +=2/-=2 step, so each half
  // cycle can overshoot the nominal bound by exactly one step (2) before
  // flipping direction -- [-502, 902], not [-500, 900].
  CHECK(min_adv >= -502 && max_adv <= 902, "around(b=false): adv stays within its ~[-500,900] oscillation band");
  CHECK(min_adv < 0 && max_adv > 500, "around(b=false): adv actually oscillates (not stuck at one end)");
}

// Medium.java:582-620's transaround() -- the replay's camera crossfade
// between two cars. Same hand-traced-plus-cross-checked-formula approach
// as test_around_fast_intro() above: exact bookkeeping (adv/n5/y/vxz/xz)
// by hand, x/z/zy by calling the same trig/sqrt/atan formula independently.
static void test_transaround_lerp_and_orbit(void) {
  Medium m;
  medium_init(&m);
  m.vxz = 10;

  ContO from, to;
  memset(&from, 0, sizeof(from));
  memset(&to, 0, sizeof(to));
  from.x = 0;   from.y = -2000; from.z = 0;
  to.x = 2000;  to.y = -2000;   to.z = 4000;

  // t=5 of 20 -- 25% of the way from `from` to `to`.
  medium_transaround(&m, &from, &to, 5);

  int32_t n2 = (from.x * (20 - 5) + to.x * 5) / 20;
  int32_t n3 = (from.y * (20 - 5) + to.y * 5) / 20;
  int32_t n4 = (from.z * (20 - 5) + to.z * 5) / 20;
  CHECK(n2 == 500 && n3 == -2000 && n4 == 1000, "transaround: lerp point sanity (test's own arithmetic)");

  CHECK(m.adv == 500 + 2, "transaround: adv += 2 (medium_init's own adv=500 default, vert=false)");
  int32_t n5 = 500 + m.adv;
  if (n5 < 1000) n5 = 1000;
  CHECK(m.y == n3 - m.adv, "transaround: y = lerped_y - adv");
  int32_t radius_term = n2 - n5 - n2;
  int32_t expect_x = n2 + jtrunc((float)radius_term * medium_cos(&m, 10.0f));
  int32_t expect_z = n4 + jtrunc((float)radius_term * medium_sin(&m, 10.0f));
  CHECK(m.x == expect_x, "transaround: x from radius_term*cos(vxz)");
  CHECK(m.z == expect_z, "transaround: z from radius_term*sin(vxz)");
  CHECK(m.vxz == 12, "transaround: vxz += 2 (the ordinary, non-b, rate)");
  CHECK(m.xz == -12 + 90, "transaround: xz = -vxz + 90");

  int32_t y_clamped = m.y > 0 ? 0 : m.y;
  int32_t n6 = (n3 - y_clamped - m.cy < 0) ? -180 : 0;
  int32_t dz = n4 - m.z + m.cz;
  int32_t dx = n2 - m.x - m.cx;
  int32_t dist = jtrunc_d(sqrt((double)(dz * dz + dx * dx)));
  int32_t n7 = jtrunc_d(90.0 + (double)n6 -
                         atan((double)dist / (double)(n3 - y_clamped - m.cy)) / 0.017453292519943295);
  int32_t expect_zy = 0 + (n7 - 0) / 10; // medium_init's own zy default is 0
  CHECK(m.zy == expect_zy, "transaround: zy += (n7-zy)/10");
}

static void test_d(void) {
  check_d_scenario("default", 111, NULL, 312, 0, 229, 800, 0, -1.5f, true);
  check_d_scenario("zy_nonzero", 222, setup_zy_nonzero, 216, 0, 368, 800, 0, 6.0f, true);
  check_d_scenario("resdown2", 333, setup_resdown2, 198, 0, 123, 800, 109, 0.0f, true);
  check_d_scenario("lightn", 444, setup_lightn, 312, 0, 229, 800, 0, 6.0f, true);
}

int main(void) {
  test_nfm_random_matches_js();
  test_init_seeded_fields();
  test_sin_cos();
  test_xs_ys();
  test_rot();
  test_random_sim_phase();
  test_random_draw_phase();
  test_random_recording_and_replay();
  test_follow();
  test_groundpolys();
  test_aroundtrack_dive_and_settle();
  test_around_fast_intro();
  test_transaround_lerp_and_orbit();
  test_d();
  if (failures == 0) {
    printf("all tests passed\n");
    return 0;
  }
  fprintf(stderr, "%d failure(s)\n", failures);
  return 1;
}
