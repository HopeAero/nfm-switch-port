// Host-buildable test for native/core/plane.c.
//
// rot/xs/ys/spy/deltafntyp are checked against web/Plane.test.js's own
// literals verbatim (that file's header says they came from
// js/tools/PlaneMath.java, i.e. the real Java -- this test does not
// re-derive them, just checks the C port agrees with the already-verified
// JS/Java values). plane_d/plane_s have no JS test coverage of their own
// (web/Plane.test.js only covers the pure-math helpers), so those were
// checked by running the real web/Plane.js headless under Node (see the
// commit that added this file for the oracle scripts) and are reproduced
// here as literals, same fallback as trackers_test.c/medium_test.c.
//
// The oracle comparison for plane_d's "chip_active" scenario caught a real
// transcription bug on the first attempt: the JS computes cox[1]/cox[2]
// first, then coy[1]/coy[2], then coz[1]/coz[2] (grouped by AXIS), not
// interleaved per-index the way a straightforward loop naturally reads --
// see plane.c's comment at that loop for the fix. Left as a note here
// because it's exactly the kind of mistake this test exists to catch.
#include <stdio.h>
#include <string.h>
#include "../core/java_compat.h"
#include "../core/medium.h"
#include "../core/trackers.h"
#include "../core/plane.h"
#include "../core/gfx.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
  if (!(cond)) { fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); failures++; } \
} while (0)

static Plane make_plane(Medium *m, Trackers *t, const int32_t *ox, const int32_t *oz,
                         const int32_t *oy, int32_t n, int32_t glass) {
  Plane p;
  int32_t oc[3] = {0, 0, 0};
  plane_init(&p, m, t, ox, oz, oy, n, oc, glass, 0, 0, 0, 0, 0, 7, 0, false, 0, false);
  return p;
}

static void test_rot_matches_java(void) {
  Medium m; medium_init(&m);
  m.cx = 400; m.cy = 225; m.cz = 100; m.focus_point = 500;
  Trackers t; trackers_init(&t);
  int32_t ox[4] = {100, -250, 3000, 17}, oz[4] = {40, 900, -1200, 6}, oy[4] = {0, 0, 0, 0};
  Plane p = make_plane(&m, &t, ox, oz, oy, 4, 3);

  struct { int32_t ang; int32_t ex[4]; int32_t ez[4]; } cases[] = {
    {37,  {45, -751, 3108, 0},    {82, 558, 837, 4}},
    {90,  {-50, -910, 1190, -16}, {70, -280, 2970, -13}},
    {-45, {116, 476, 1289, 33},   {-41, 814, -2968, -7}},
    {405, {31, -824, 2958, -3},   {86, 446, 1259, 3}},
    {180, {-80, 270, -2980, 3},   {-80, -940, 1160, -46}},
    {1,   {98, -266, 3020, 16},   {41, 895, -1147, 6}},
    {359, {101, -233, 2978, 17},  {38, 904, -1252, 5}},
  };
  for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
    int32_t x[4] = {100, -250, 3000, 17}, z[4] = {40, 900, -1200, 6};
    plane_rot(&p, x, z, 10, -20, cases[i].ang, 4);
    char label[64];
    snprintf(label, sizeof(label), "rot x at angle %d", cases[i].ang);
    CHECK(memcmp(x, cases[i].ex, sizeof(x)) == 0, label);
    snprintf(label, sizeof(label), "rot z at angle %d", cases[i].ang);
    CHECK(memcmp(z, cases[i].ez, sizeof(z)) == 0, label);
  }

  int32_t x[2] = {1000, -500}, z[2] = {750, 250};
  for (int i = 0; i < 1000; i++) plane_rot(&p, x, z, 0, 0, 7, 2);
  CHECK(x[0] == -585 && x[1] == 0, "rot 1000-iteration drift x");
  CHECK(z[0] == -173 && z[1] == 0, "rot 1000-iteration drift z");

  plane_free(&p);
  medium_free(&m);
}

static void test_xs_ys_spy(void) {
  Medium m; medium_init(&m);
  m.cx = 400; m.cy = 225; m.cz = 100; m.focus_point = 500;
  Trackers t; trackers_init(&t);
  int32_t ox[4] = {100, -250, 3000, 17}, oz[4] = {40, 900, -1200, 6}, oy[4] = {0, 0, 0, 0};
  Plane p = make_plane(&m, &t, ox, oz, oy, 4, 3);

  int32_t cases[5][4] = {
    {0, 100, -1600, -900}, {400, 5000, 400, 243}, {-3000, 12000, 258, 90},
    {800, 50, 2400, 3100}, {123, 100000, 398, 224},
  };
  for (int i = 0; i < 5; i++) {
    char label[32];
    snprintf(label, sizeof(label), "xs case %d", i);
    CHECK(plane_xs(&p, cases[i][0], cases[i][1]) == cases[i][2], label);
    snprintf(label, sizeof(label), "ys case %d", i);
    CHECK(plane_ys(&p, cases[i][0], cases[i][1]) == cases[i][3], label);
  }

  CHECK(plane_spy(&p, 0, 0) == 400, "spy(0,0)");
  CHECK(plane_spy(&p, 9000, 12000) == 14763, "spy(9000,12000)");
  CHECK(plane_spy(&p, -9000, -12000) == 15243, "spy(-9000,-12000)");

  plane_free(&p);
  medium_free(&m);
}

static void test_deltafntyp(void) {
  Medium m; medium_init(&m);
  Trackers t; trackers_init(&t);
  int32_t ox[4] = {0, 1400, 700, -700}, oz[4] = {-1100, 0, 0, -1100}, oy[4] = {0, 0, 0, 0};
  Plane p = make_plane(&m, &t, ox, oz, oy, 4, 3);
  CHECK(p.deltaf == 880203.2f, "deltafntyp accumulator");
  plane_free(&p);
  medium_free(&m);
}

static void check_vertex(Graphics2D *g, int32_t i, float wantX, float wantY, const char *label) {
  float x, y;
  gfx_vertex_at(g, i, &x, &y);
  char msg[64];
  snprintf(msg, sizeof(msg), "%s (got %g,%g want %g,%g)", label, (double)x, (double)y, (double)wantX, (double)wantY);
  CHECK(x == wantX && y == wantY, msg);
}

// Oracle: convex quad, embos=0 (undamaged), glass=0 (snap-based colour),
// captured from the real web/Plane.js headless. First tri's vertices/colour
// checked exhaustively; count and final state confirm the rest.
static void test_d_undamaged_quad(void) {
  nfm_set_seed(1001);
  Medium m; medium_init(&m);
  m.snap[0] = 5; m.snap[1] = -3; m.snap[2] = 2;
  m.cx = 400; m.cy = 225; m.cz = 100; m.focus_point = 500; m.trk = 0; m.adv = 900;
  Trackers t; trackers_init(&t);
  int32_t ox[4] = {-50, 50, 50, -50}, oz[4] = {-50, -50, 50, 50}, oy[4] = {0, 0, 0, 0};
  int32_t oc[3] = {180, 170, 160};
  Plane p; plane_init(&p, &m, &t, ox, oz, oy, 4, oc, 0, -1, 0, 0, 0, 0, 7, 0, false, 0, false);

  Graphics2D g; gfx_init(&g, 800, 450); gfx_begin(&g);
  plane_d(&p, &g, 0, -300, 2000, 0, 0, 0, 0, 0, false, -1);

  CHECK(g.count == 30, "undamaged quad vertex count");
  check_vertex(&g, 0, 284, 90, "undamaged quad v0");
  check_vertex(&g, 1, 310, 90, "undamaged quad v1");
  check_vertex(&g, 2, 314, 96, "undamaged quad v2");
  int32_t r, gg, b, a;
  gfx_color_at(&g, 0, &r, &gg, &b, &a);
  CHECK(r == 113 && gg == 98 && b == 98, "undamaged quad fill colour");
  CHECK(p.av == 2106, "undamaged quad av");
  CHECK(p.deltaf == 0.6666666269302368f, "undamaged quad deltaf");

  gfx_free(&g); plane_free(&p); medium_free(&m);
}

// Oracle: embos=5 (mid-damage jitter path), glass=3.
static void test_d_damaged_quad(void) {
  nfm_set_seed(1002);
  Medium m; medium_init(&m);
  m.cx = 400; m.cy = 225; m.cz = 100; m.focus_point = 500; m.trk = 0; m.adv = 900;
  Trackers t; trackers_init(&t);
  int32_t ox[4] = {-50, 50, 50, -50}, oz[4] = {-50, -50, 50, 50}, oy[4] = {0, 0, 0, 0};
  int32_t oc[3] = {200, 50, 50};
  Plane p; plane_init(&p, &m, &t, ox, oz, oy, 4, oc, 3, -1, 0, 0, 0, 0, 7, 0, false, 0, false);

  Graphics2D g; gfx_init(&g, 800, 450); gfx_begin(&g);
  plane_d(&p, &g, 10, -300, 2000, 5, 8, 0, 0, 0, false, -1);

  CHECK(g.count == 30, "damaged quad vertex count");
  check_vertex(&g, 0, 288, 88, "damaged quad v0");
  check_vertex(&g, 1, 313, 92, "damaged quad v1");
  check_vertex(&g, 2, 315, 98, "damaged quad v2");
  int32_t r, gg, b, a;
  gfx_color_at(&g, 0, &r, &gg, &b, &a);
  CHECK(r == 120 && gg == 24 && b == 24, "damaged quad fill colour");
  CHECK(p.av == 2104, "damaged quad av");
  CHECK(p.projf == 0.6066953539848328f, "damaged quad projf");

  gfx_free(&g); plane_free(&p); medium_free(&m);
}

// Oracle: embos forced to 12 on entry -> chip=1/ctmag=2.0/bfase=-7, so this
// call draws the damage-shard debris triangle. This is the scenario that
// caught the cox/coy/coz axis-grouping bug (see this file's header).
// Checked exhaustively (all 33 vertices) since it's the scenario that
// actually found a bug -- worth the extra literals here specifically.
static void test_d_chip_debris(void) {
  nfm_set_seed(1003);
  Medium m; medium_init(&m);
  m.cx = 400; m.cy = 225; m.cz = 100; m.focus_point = 500; m.trk = 0; m.adv = 900;
  Trackers t; trackers_init(&t);
  int32_t ox[4] = {-40, 40, 40, -40}, oz[4] = {-40, -40, 40, 40}, oy[4] = {0, 0, 0, 0};
  int32_t oc[3] = {210, 60, 60};
  Plane p; plane_init(&p, &m, &t, ox, oz, oy, 4, oc, 3, -1, 0, 0, 0, 0, 7, 0, false, 0, false);
  p.embos = 12;

  Graphics2D g; gfx_init(&g, 800, 450); gfx_begin(&g);
  plane_d(&p, &g, 20, -200, 1800, 3, -5, 0, 0, 0, false, -1);

  CHECK(g.count == 33, "chip debris vertex count");
  static const float want_x[6] = {285, 281, 284, 280, 304, 307};
  static const float want_y[6] = {110, 112, 110, 104, 103, 108};
  for (int i = 0; i < 6; i++) {
    char label[32];
    snprintf(label, sizeof(label), "chip debris v%d", i);
    check_vertex(&g, i, want_x[i], want_y[i], label);
  }
  int32_t r, gg, b, a;
  gfx_color_at(&g, 0, &r, &gg, &b, &a);
  CHECK(r == 128 && gg == 30 && b == 30, "chip debris colour 1");
  gfx_color_at(&g, 3, &r, &gg, &b, &a);
  CHECK(r == 77 && gg == 18 && b == 18, "chip debris colour 2");
  CHECK(p.av == 1888 && p.chip == 3 && p.embos == 13 && p.bfase == -7, "chip debris state");
  CHECK(p.dx == 12 && p.dy == -12 && p.dz == 12, "chip debris dx/dy/dz");
  CHECK(p.vx == 12 && p.vy == -5 && p.vz == 12, "chip debris vx/vy/vz");

  gfx_free(&g); plane_free(&p); medium_free(&m);
}

// Oracle: plane_d's wheel-roll/steer rotation gate (JS: `if (this.wz !==
// 0)` / `if (this.wx !== 0)`), with nonzero wxz/wzy passed in (n6=15,
// n7=-20) so the gate's condition actually matters. This is the scenario
// that caught a real translation bug: the port checked `n7 != 0`/`n6 !=
// 0` (whether the car-wide angle was nonzero) instead of `p->wz != 0`/
// `p->wx != 0` (whether THIS plane belongs to a wheel), so every plane
// in the car -- body panels included -- got rotated around a wheel
// pivot any time the car had any speed or steering at all. Every OTHER
// plane_d scenario in this file passes wxz=wzy=0 (a parked car), so none
// of them exercised this gate either way -- this is deliberately the
// first one that does. Two sub-cases share one oracle-verified shape:
// wx=wy=wz=0 (body plane -- MUST be unaffected by wxz/wzy, byte-for-byte
// identical to test_d_undamaged_quad's own vertices despite the nonzero
// angles) and wx=100/wz=200 (a wheel plane -- MUST rotate).
static void test_d_wheel_rotation_gate(void) {
  int32_t ox[4] = {-50, 50, 50, -50}, oz[4] = {-50, -50, 50, 50}, oy[4] = {0, 0, 0, 0};
  int32_t oc_body[3] = {180, 170, 160};

  nfm_set_seed(2001);
  Medium m1; medium_init(&m1);
  m1.cx = 400; m1.cy = 225; m1.cz = 100; m1.focus_point = 500; m1.trk = 0; m1.adv = 900;
  Trackers t1; trackers_init(&t1);
  Plane pbody;
  plane_init(&pbody, &m1, &t1, ox, oz, oy, 4, oc_body, 0, -1, 0, 0, 0, 0, 7, 0, false, 0, false);
  Graphics2D gbody; gfx_init(&gbody, 800, 450); gfx_begin(&gbody);
  plane_d(&pbody, &gbody, 0, -300, 2000, 0, 0, 0, 15, -20, false, -1);
  CHECK(gbody.count == 30, "wheel gate body count");
  static const float body_x[3] = {284, 310, 314};
  static const float body_y[3] = {90, 90, 96};
  for (int i = 0; i < 3; i++) {
    char label[48];
    snprintf(label, sizeof(label), "wheel gate body v%d (unaffected)", i);
    check_vertex(&gbody, i, body_x[i], body_y[i], label);
  }
  gfx_free(&gbody); plane_free(&pbody); medium_free(&m1);

  nfm_set_seed(2001);
  Medium m2; medium_init(&m2);
  m2.cx = 400; m2.cy = 225; m2.cz = 100; m2.focus_point = 500; m2.trk = 0; m2.adv = 900;
  Trackers t2; trackers_init(&t2);
  int32_t oc_wheel[3] = {180, 170, 160};
  Plane pwheel;
  plane_init(&pwheel, &m2, &t2, ox, oz, oy, 4, oc_wheel, 0, -1, 0, 100, 0, 200, 7, 0, false, 0, false);
  Graphics2D gwheel; gfx_init(&gwheel, 800, 450); gfx_begin(&gwheel);
  plane_d(&pwheel, &gwheel, 0, -300, 2000, 0, 0, 0, 15, -20, false, -1);
  CHECK(gwheel.count == 30, "wheel gate wheel count");
  static const float wheel_x[3] = {300, 326, 323};
  static const float wheel_y[3] = {67, 69, 84};
  for (int i = 0; i < 3; i++) {
    char label[48];
    snprintf(label, sizeof(label), "wheel gate wheel v%d (rotated)", i);
    check_vertex(&gwheel, i, wheel_x[i], wheel_y[i], label);
  }
  gfx_free(&gwheel); plane_free(&pwheel); medium_free(&m2);
}

// Oracle: plane_s's shadow projection, including the Trackers colour-lookup
// branch (n7==0, a dam==167 boundary tracker covering the object).
static void test_s_shadow(void) {
  nfm_set_seed(2001);
  Medium m; medium_init(&m);
  m.x = 0; m.y = -300; m.z = 0; m.xz = 0; m.zy = 0;
  m.ground = 250 - m.y;
  Trackers t; trackers_init(&t);
  t.nt = 1;
  t.x[0] = 0; t.y[0] = 0; t.z[0] = 2000; t.dam[0] = 167;
  t.zy[0] = 0; t.xy[0] = 0; t.rady[0] = 0; t.radx[0] = 5000; t.radz[0] = 5000;
  t.c[0][0] = 100; t.c[0][1] = 90; t.c[0][2] = 80; t.decor[0] = false;
  trackers_devidetrackers(&t, 0, 9000, 0, 9000);

  int32_t ox[4] = {-40, 40, 40, -40}, oz[4] = {-40, -40, 40, 40}, oy[4] = {0, 0, 0, 0};
  int32_t oc[3] = {180, 170, 160};
  Plane p; plane_init(&p, &m, &t, ox, oz, oy, 4, oc, 0, -1, 0, 0, 0, 0, 7, 0, false, 0, false);

  Graphics2D g; gfx_init(&g, 800, 450); gfx_begin(&g);
  plane_s(&p, &g, 0, 0, 2000, 0, 0, 0, 0);

  CHECK(g.count == 6, "shadow vertex count");
  static const float want_x[6] = {310, 326, 329, 310, 329, 313};
  static const float want_y[6] = {241, 241, 240, 241, 240, 240};
  for (int i = 0; i < 6; i++) {
    char label[32];
    snprintf(label, sizeof(label), "shadow v%d", i);
    check_vertex(&g, i, want_x[i], want_y[i], label);
  }
  int32_t r, gg, b, a;
  gfx_color_at(&g, 0, &r, &gg, &b, &a);
  CHECK(r == 66 && gg == 60 && b == 53, "shadow colour");

  gfx_free(&g); plane_free(&p); medium_free(&m); trackers_free_sect(&t);
}

int main(void) {
  test_rot_matches_java();
  test_xs_ys_spy();
  test_deltafntyp();
  test_d_undamaged_quad();
  test_d_damaged_quad();
  test_d_chip_debris();
  test_d_wheel_rotation_gate();
  test_s_shadow();
  if (failures == 0) {
    printf("all tests passed\n");
    return 0;
  }
  fprintf(stderr, "%d failure(s)\n", failures);
  return 1;
}
