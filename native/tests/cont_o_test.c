// Host-buildable test for native/core/cont_o.c (cont_o_init_buf only --
// see cont_o.h for what's not ported).
//
// No web/ContO.test.js exists, so -- same fallback as trackers_test.c and
// friends -- expected values were captured by running the real
// web/ContO.js under Node against the actual repo-root mycars/Simple
// Car.rad (with m.loadnew = true, matching how CarDefine.js's loadcar()
// really calls it) and are reproduced here as literals.
//
// This scenario is kept specifically because it caught a real bug: the
// first cut of cont_o_init_buf left the m.loadnew-gated post-processing
// pass (JS lines ~469-829) unported, on the theory that it only affected
// "some" glass/decal panels. Running this exact oracle showed every
// summary field already matched (npl, errd, maxR, grat, sprkat, wh, key*,
// fcol/scol/colok, tnt, disline/shadow/decor/grounded) but EVERY plane's
// fs field was 0 in the C output vs the JS's +-1/0 -- the pass turned out
// to set fs for the majority of real faces, not an edge case. The pass
// was then ported in full and this file checks fs (along with n/master/
// gr/glass/c/road/light) for all 102 planes so a regression here is
// caught immediately.
//
// test_d_ground_shadow below covers the OTHER half of cont_o.c: the
// runtime `d()` draw entry point (cont_o_d), plus its `xs`/`ys` helpers.
// Oracle: the real web/ContO.js's d() run against the same parsed car,
// positioned with Medium.follow() (the real chase-camera setup
// GameSparker uses) and instrumented (Plane.prototype.d/s monkey-patched)
// to record every draw call's index/color/mode, then compared against an
// identically-instrumented native/core/cont_o.c build -- all 204 calls
// (102 shadow + 102 face draws) matched exactly, in order, on the first
// attempt after one real bug was found and fixed: dsprk() was originally
// stubbed to abort unconditionally, but the JS calls it every frame
// co.shadow is true (not just when actually sparking) -- its OWN body is
// what's gated on co.sprk_/co.rtg[], both zero for an undamaged car, so
// the call itself is not unreachable, only its spawn/trail-draw bodies
// are. Also checked with m.crs = true (the alternate "always show full
// shadow" mode) -- same 204-call match. Only the summary counters
// (objCalls/objDrawn/dist/camera fields) are kept here as permanent
// literals; the full per-call trace lived in the oracle script, not this
// file, since plane_d/plane_s's own per-vertex fidelity is already
// exhaustively covered by plane_test.c.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../core/java_compat.h"
#include "../core/medium.h"
#include "../core/trackers.h"
#include "../core/plane.h"
#include "../core/wheels.h"
#include "../core/cont_o.h"
#include "../core/vfs.h"
#include "../core/gfx.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
  if (!(cond)) { fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); failures++; } \
} while (0)

typedef struct {
  int32_t n, master, gr, fs, glass;
  int32_t c0, c1, c2;
  bool road;
  int32_t light;
} PlaneExpect;

// Captured verbatim from the Node oracle run described above.
static const PlaneExpect kExpect[102] = {
  {4,0,0,1,0,78,94,238,false,0},   {4,0,0,-1,0,78,94,238,false,0},
  {4,0,0,1,0,78,94,238,false,0},   {3,0,0,-1,0,230,51,11,false,0},
  {4,0,0,1,0,230,51,11,false,0},   {4,0,0,-1,1,198,220,243,false,0},
  {4,0,0,-1,0,78,94,238,false,0},  {4,0,0,1,0,78,94,238,false,0},
  {4,0,0,-1,0,78,94,238,false,0},  {3,0,0,1,0,230,51,11,false,0},
  {4,0,0,-1,0,230,51,11,false,0},  {4,0,0,1,1,198,220,243,false,0},
  {4,0,0,1,0,255,255,255,false,1},{4,0,0,-1,0,255,255,255,false,1},
  {4,0,0,1,0,78,94,238,false,0},   {4,0,0,-1,0,78,94,238,false,0},
  {4,0,0,1,1,198,220,243,false,0},{4,0,0,-1,0,230,51,11,false,0},
  {4,0,0,1,0,230,51,11,false,0},   {4,0,0,1,0,255,0,0,false,2},
  {4,0,0,-1,0,255,0,0,false,2},    {4,0,0,1,0,78,94,238,false,0},
  {4,0,0,1,1,198,220,243,false,0},{4,0,0,1,0,70,70,70,false,0},
  {4,0,0,-1,0,70,70,70,false,0},   {4,0,0,1,0,70,70,70,false,0},
  {20,1,0,0,0,45,45,45,false,0},
  {3,2,-2,0,0,170,170,255,false,0},{3,2,-2,0,0,170,170,255,false,0},
  {3,2,-2,0,0,170,170,255,false,0},{3,2,-2,0,0,170,170,255,false,0},
  {3,2,-2,0,0,170,170,255,false,0},{3,2,-2,0,0,170,170,255,false,0},
  {4,0,0,-1,0,45,45,45,false,0},   {4,0,0,1,0,45,45,45,false,0},
  {4,0,0,1,0,45,45,45,false,0},    {4,0,0,-1,0,45,45,45,false,0},
  {4,0,0,1,0,45,45,45,false,0},    {4,0,0,1,0,45,45,45,false,0},
  {4,0,0,-1,0,45,45,45,false,0},   {4,0,0,1,0,45,45,45,false,0},
  {4,0,0,1,0,45,45,45,false,0},    {4,0,0,-1,0,45,45,45,false,0},
  {4,0,0,1,0,45,45,45,false,0},    {4,0,0,1,0,45,45,45,false,0},
  {20,1,0,0,0,45,45,45,false,0},
  {3,2,-2,0,0,170,170,255,false,0},{3,2,-2,0,0,170,170,255,false,0},
  {3,2,-2,0,0,170,170,255,false,0},{3,2,-2,0,0,170,170,255,false,0},
  {3,2,-2,0,0,170,170,255,false,0},{3,2,-2,0,0,170,170,255,false,0},
  {4,0,0,1,0,45,45,45,false,0},    {4,0,0,-1,0,45,45,45,false,0},
  {4,0,0,-1,0,45,45,45,false,0},   {4,0,0,1,0,45,45,45,false,0},
  {4,0,0,-1,0,45,45,45,false,0},   {4,0,0,-1,0,45,45,45,false,0},
  {4,0,0,1,0,45,45,45,false,0},    {4,0,0,-1,0,45,45,45,false,0},
  {4,0,0,-1,0,45,45,45,false,0},   {4,0,0,1,0,45,45,45,false,0},
  {4,0,0,-1,0,45,45,45,false,0},   {4,0,0,-1,0,45,45,45,false,0},
  {20,1,0,0,0,45,45,45,false,0},
  {3,2,-2,0,0,170,170,255,false,0},{3,2,-2,0,0,170,170,255,false,0},
  {3,2,-2,0,0,170,170,255,false,0},{3,2,-2,0,0,170,170,255,false,0},
  {3,2,-2,0,0,170,170,255,false,0},{3,2,-2,0,0,170,170,255,false,0},
  {4,0,0,-1,0,45,45,45,false,0},   {4,0,0,1,0,45,45,45,false,0},
  {4,0,0,1,0,45,45,45,false,0},    {4,0,0,-1,0,45,45,45,false,0},
  {4,0,0,1,0,45,45,45,false,0},    {4,0,0,1,0,45,45,45,false,0},
  {4,0,0,-1,0,45,45,45,false,0},   {4,0,0,1,0,45,45,45,false,0},
  {4,0,0,1,0,45,45,45,false,0},    {4,0,0,-1,0,45,45,45,false,0},
  {4,0,0,1,0,45,45,45,false,0},    {4,0,0,1,0,45,45,45,false,0},
  {20,1,0,0,0,45,45,45,false,0},
  {3,2,-2,0,0,170,170,255,false,0},{3,2,-2,0,0,170,170,255,false,0},
  {3,2,-2,0,0,170,170,255,false,0},{3,2,-2,0,0,170,170,255,false,0},
  {3,2,-2,0,0,170,170,255,false,0},{3,2,-2,0,0,170,170,255,false,0},
  {4,0,0,1,0,45,45,45,false,0},    {4,0,0,-1,0,45,45,45,false,0},
  {4,0,0,-1,0,45,45,45,false,0},   {4,0,0,1,0,45,45,45,false,0},
  {4,0,0,-1,0,45,45,45,false,0},   {4,0,0,-1,0,45,45,45,false,0},
  {4,0,0,1,0,45,45,45,false,0},    {4,0,0,-1,0,45,45,45,false,0},
  {4,0,0,-1,0,45,45,45,false,0},   {4,0,0,1,0,45,45,45,false,0},
  {4,0,0,-1,0,45,45,45,false,0},   {4,0,0,-1,0,45,45,45,false,0},
};

static void test_simple_car(void) {
  nfm_set_seed(9001);
  Medium m;
  medium_init(&m);
  Trackers t;
  trackers_init(&t);

  // native/tests/build/ -> repo root, same convention as vfs_test.c.
  vfs_set_fpath("../../../");
  char *text = vfs_read_text("mycars/Simple_Car.rad");
  CHECK(text != NULL, "mycars/Simple_Car.rad read");
  if (!text) {
    medium_free(&m);
    return;
  }

  m.loadnew = true;
  ContO co;
  cont_o_init_buf(&co, text, &m, &t);
  free(text);

  CHECK(co.npl == 102, "npl");
  CHECK(co.errd == false, "errd");
  CHECK(co.maxR == 130, "maxR");
  CHECK(co.grat == 68, "grat");
  CHECK(co.sprkat == 72, "sprkat");
  CHECK(co.wh == 30, "wh");
  CHECK(co.keyx[0] == -72 && co.keyx[1] == 72 && co.keyx[2] == -72 && co.keyx[3] == 72, "keyx");
  CHECK(co.keyz[0] == 87 && co.keyz[1] == 87 && co.keyz[2] == -79 && co.keyz[3] == -79, "keyz");
  CHECK(co.fcol[0] == 78 && co.fcol[1] == 94 && co.fcol[2] == 238, "fcol");
  CHECK(co.scol[0] == 230 && co.scol[1] == 51 && co.scol[2] == 11, "scol");
  CHECK(co.colok == 2, "colok");
  CHECK(co.tnt == 0, "tnt");
  CHECK(co.disline == 14, "disline");
  CHECK(co.shadow == true, "shadow");
  CHECK(co.decor == false, "decor");
  CHECK(co.grounded == 1.0f, "grounded");

  for (int32_t i = 0; i < 102 && i < co.npl; i++) {
    Plane *p = &co.p[i];
    const PlaneExpect *e = &kExpect[i];
    char label[64];
    snprintf(label, sizeof(label), "plane %d n/master/gr/glass/road/light", i);
    CHECK(p->n == e->n && p->master == e->master && p->gr == e->gr && p->glass == e->glass &&
              p->road == e->road && p->light == e->light,
          label);
    snprintf(label, sizeof(label), "plane %d fs", i);
    CHECK(p->fs == e->fs, label);
    snprintf(label, sizeof(label), "plane %d c", i);
    CHECK(p->c[0] == e->c0 && p->c[1] == e->c1 && p->c[2] == e->c2, label);
  }

  cont_o_free(&co);
  medium_free(&m);
}

static void test_d_ground_shadow(void) {
  nfm_set_seed(9001);
  Medium m;
  medium_init(&m);
  Trackers t;
  trackers_init(&t);

  vfs_set_fpath("../../../");
  char *text = vfs_read_text("mycars/Simple_Car.rad");
  CHECK(text != NULL, "mycars/Simple_Car.rad read (test_d_ground_shadow)");
  if (!text) {
    medium_free(&m);
    return;
  }

  m.loadnew = true;
  ContO co;
  cont_o_init_buf(&co, text, &m, &t);
  m.loadnew = false;
  free(text);
  co.shadow = true;
  co.noline = false;
  co.decor = false;
  co.tnt = 0;
  co.disp = 0;
  co.disline = 7;
  co.grounded = 1.0f;

  co.x = 5000;
  co.y = 0;
  co.z = 5000;
  co.xz = 30;
  co.xy = 0;
  co.zy = 0;
  // Real chase-camera setup (GameSparker calls Medium.follow the same way).
  // t.ncx == t.ncz == 0 (no stage loaded) so this exercises the ground-
  // shadow path (mode 1), not the track-sector-match path (mode 0).
  medium_follow(&m, co.x, co.y, co.z, co.xz, 0);

  Graphics2D g;
  gfx_init(&g, 800, 450);

  cont_o_d(&co, &g);

  CHECK(g.objCalls == 1, "objCalls");
  CHECK(g.objDrawn == 1, "objDrawn");
  CHECK(co.dist == 29, "dist");
  CHECK(m.xz == -30 && m.zy == 10 && m.x == 5000 && m.y == -475 && m.z == 4258, "camera after follow");

  gfx_free(&g);
  cont_o_free(&co);
  medium_free(&m);
}

// Exercises cont_o_lowshadow -- the OTHER branch cont_o_d's shadow code
// can take (n3 >= 2000, i.e. far enough from the camera that per-wheel
// shadow polygons aren't worth computing). Was stubbed to abort loudly
// until AI opponents (native/core/bots.c/control.c) made it genuinely
// reachable -- see cont_o.h's own note. No web/ContO.test.js coverage
// exists for this scenario (ContO.test.js predates the native port's AI
// work), so this checks structural invariants rather than an exact pixel
// oracle: the call must not abort, must still count as drawn, and must
// still compute a distance for next frame's painter's-algorithm sort --
// same level of assertion test_d_ground_shadow above already uses for
// cont_o_d's summary counters.
//
// trk=1/ih=25/iw=65 match the real values GameSparker.js sets entering a
// race (xtGraphics.java:2743-2747, ported verbatim in main.c's own
// STATE_RACING setup) -- trk=1 is load-bearing here: it's what lets an
// object past cont_o_d's own distance-fade cull far enough out to reach
// the n3>=2000 lowshadow branch at all (fade[7] defaults to 13500, well
// under the 20000 depth used below, so trk=0 would reject the object
// before shadow code runs at all).
static void test_d_lowshadow_far_camera(void) {
  nfm_set_seed(9001);
  Medium m;
  medium_init(&m);
  Trackers t;
  trackers_init(&t);

  vfs_set_fpath("../../../");
  char *text = vfs_read_text("mycars/Simple_Car.rad");
  CHECK(text != NULL, "mycars/Simple_Car.rad read (test_d_lowshadow_far_camera)");
  if (!text) {
    medium_free(&m);
    return;
  }

  m.loadnew = true;
  ContO co;
  cont_o_init_buf(&co, text, &m, &t);
  m.loadnew = false;
  free(text);
  co.shadow = true;
  co.noline = false;
  co.decor = false;
  co.tnt = 0;
  co.disp = 0;
  co.disline = 7;
  co.grounded = 1.0f;

  medium_follow(&m, 0, 0, 0, 0, 0);
  m.trk = 1;
  m.ih = 25;
  m.iw = 65;

  co.x = 0;
  co.y = 0;
  co.z = 20000; // straight ahead of the camera, well past the 2000 threshold
  co.xz = 0;
  co.xy = 0;
  co.zy = 0;

  Graphics2D g;
  gfx_init(&g, 800, 450);

  cont_o_d(&co, &g); // must not abort (see cont_o.c's cont_o_lowshadow)

  CHECK(g.objCalls == 1, "lowshadow: objCalls");
  CHECK(g.objDrawn == 1, "lowshadow: objDrawn (n3>=2000 still passes the outer visibility gate under trk=1)");
  CHECK(co.dist != 0, "lowshadow: dist computed for next frame's sort");

  gfx_free(&g);
  cont_o_free(&co);
  medium_free(&m);
}

typedef struct {
  int32_t n, colnum, master;
  float projf;
  int32_t ox[4], oz[4];
} CopyPlaneExpect;

static const CopyPlaneExpect kCopyExpect[55] = {
  {4,0,0,41123.156250000f,{1120,1400,2006,1726},{-1939,-2424,-2074,-1589}},
  {4,0,0,41316.015625000f,{513,793,1400,1120},{-2289,-2774,-2424,-1939}},
  {4,0,0,41137.492187500f,{1120,840,1446,1726},{-1939,-1454,-1104,-1589}},
  {4,0,0,41272.902343750f,{513,233,840,1120},{-2289,-1804,-1454,-1939}},
  {4,0,0,41123.156250000f,{560,840,1446,1166},{-969,-1454,-1104,-619}},
  {4,0,0,41151.535156250f,{-46,233,840,560},{-1319,-1804,-1454,-969}},
  {4,0,0,41137.492187500f,{-46,-326,280,560},{-1319,-834,-484,-969}},
  {4,0,0,41137.492187500f,{560,280,886,1166},{-969,-484,-134,-619}},
  {4,0,0,40982.378906250f,{-606,-326,280,0},{-350,-834,-484,0}},
  {4,0,0,40982.378906250f,{0,280,886,606},{0,-484,-134,350}},
  {4,0,0,40925.207031250f,{-606,-886,-280,0},{-350,134,484,0}},
  {4,0,0,40925.207031250f,{0,-280,326,606},{0,484,834,350}},
  {4,0,0,41123.156250000f,{-1166,-886,-280,-560},{619,134,484,969}},
  {4,0,0,41123.156250000f,{-560,-280,326,46},{969,484,834,1319}},
  {4,0,0,41137.492187500f,{-1166,-1446,-840,-560},{619,1104,1454,969}},
  {4,0,0,41272.902343750f,{-560,-840,-233,46},{969,1454,1804,1319}},
  {4,0,0,41123.156250000f,{-1726,-1446,-840,-1120},{1589,1104,1454,1939}},
  {4,0,0,41316.015625000f,{-1120,-840,-233,-513},{1939,1454,1804,2289}},
  {4,0,0,41137.492187500f,{-1726,-2006,-1400,-1120},{1589,2074,2424,1939}},
  {4,0,0,41272.902343750f,{-1120,-1400,-793,-513},{1939,2424,2774,2289}},
  {4,0,0,1625.028686523f,{1777,2127,2006,1656},{-1398,-2004,-2074,-1468}},
  {4,0,0,1625.028686523f,{1427,1777,1656,1306},{-792,-1398,-1468,-862}},
  {4,0,0,1624.135375977f,{1427,1077,956,1306},{-792,-186,-256,-862}},
  {4,0,0,1624.135375977f,{1077,727,606,956},{-186,420,350,-256}},
  {4,0,0,1624.135375977f,{-322,-672,-793,-443},{2238,2844,2774,2168}},
  {4,0,0,1618.819458008f,{27,-322,-443,-93},{1632,2238,2168,1562}},
  {4,0,0,1625.028686523f,{27,377,256,-93},{1632,1026,956,1562}},
  {4,0,0,1625.028686523f,{377,727,606,256},{1026,420,350,956}},
  {4,0,0,1625.028686523f,{1777,2127,2006,1656},{-1398,-2004,-2074,-1468}},
  {4,0,0,1625.028686523f,{1427,1777,1656,1306},{-792,-1398,-1468,-862}},
  {4,0,0,1624.135375977f,{1427,1077,956,1306},{-792,-186,-256,-862}},
  {4,0,0,1624.135375977f,{1077,727,606,956},{-186,420,350,-256}},
  {4,0,0,1624.135375977f,{322,672,793,443},{-2238,-2844,-2774,-2168}},
  {4,0,0,1618.819458008f,{-27,322,443,93},{-1632,-2238,-2168,-1562}},
  {4,0,0,1625.028686523f,{-27,-377,-256,93},{-1632,-1026,-956,-1562}},
  {4,0,0,1625.028686523f,{-377,-727,-606,-256},{-1026,-420,-350,-956}},
  {4,0,0,1625.028686523f,{-1777,-2127,-2006,-1656},{1398,2004,2074,1468}},
  {4,0,0,1625.028686523f,{-1427,-1777,-1656,-1306},{792,1398,1468,862}},
  {4,0,0,1624.135375977f,{-1427,-1077,-956,-1306},{792,186,256,862}},
  {4,0,0,1624.135375977f,{-1077,-727,-606,-956},{186,-420,-350,256}},
  {4,0,0,53.064239502f,{-157,122,157,-122},{232,-252,-232,252}},
  {4,0,0,53.588714600f,{-717,-437,-402,-682},{1202,717,737,1222}},
  {4,0,0,53.520763397f,{682,402,437,717},{-1222,-737,-717,-1202}},
  {4,0,0,53.520763397f,{-997,-1277,-1242,-962},{1687,2172,2192,1707}},
  {4,0,0,53.588714600f,{962,1242,1277,997},{-1707,-2192,-2172,-1687}},
  {4,0,0,0.027653916f,{-600,-560,-525,-565},{-300,-369,-349,-280}},
  {4,0,0,0.027378276f,{560,600,565,525},{369,300,280,349}},
  {4,0,0,0.027653916f,{-1160,-1120,-1085,-1125},{669,600,620,689}},
  {4,0,0,0.027378276f,{0,40,5,-34},{1339,1270,1250,1319}},
  {4,0,0,0.027378276f,{0,-40,-5,34},{-1339,-1270,-1250,-1319}},
  {4,0,0,0.027653916f,{1160,1120,1085,1125},{-669,-600,-620,-689}},
  {4,0,0,0.027653916f,{-1720,-1680,-1645,-1685},{1639,1570,1590,1659}},
  {4,0,0,0.027378276f,{-559,-519,-554,-594},{2309,2240,2220,2289}},
  {4,0,0,0.027378276f,{559,519,554,594},{-2309,-2240,-2220,-2289}},
  {4,0,0,0.027653916f,{1720,1680,1645,1685},{-1639,-1570,-1590,-1659}},
};

// Oracle: the real web/ContO.js's #initCopy, cloning `road.rad` (a real
// track piece straight out of data/models.zip, m.setsnap(10,-5,20)) at
// (x=1000,y=-50,z=2000,a=30). All 55 planes' n/colnum/master/projf/ox/oz
// matched exactly, as did every summary field and (after this clone) the
// shared Trackers' first 3 entries -- road.rad has 3 tracks(...) volumes.
// Exercised the case-3 (not case-1) fr()-classification for t.x/t.z that
// initially looked like it could reuse medium_rot (it can't -- see the
// commit message / cont_o.c's comment on why trunc(this.x + fr(...)) is a
// genuinely different computation from medium_rot's own n + trunc(fr(...))
// shape). Also checked with 3 more (x,y,z,a) orientations, including a=180
// (the radx/radz "abs==180 -> 0" special case) and a=90/a=-90 (the
// m.loadnew-gated grounded+=10000 branch) -- all matched exactly; only
// this one scenario's full per-plane data is kept as a permanent literal.
static void test_init_copy(void) {
  nfm_set_seed(9001);
  Medium m;
  medium_init(&m);
  Trackers t;
  trackers_init(&t);
  medium_setsnap(&m, 10, -5, 20);

  vfs_set_fpath("../../../");
  VfsZip zip;
  CHECK(vfs_read_zip("data/models.zip", &zip), "data/models.zip read (test_init_copy)");
  if (!zip.count) {
    medium_free(&m);
    return;
  }
  VfsZipEntry *road = NULL;
  for (int32_t i = 0; i < zip.count; i++) {
    if (strcmp(zip.entries[i].name, "road.rad") == 0) road = &zip.entries[i];
  }
  CHECK(road != NULL, "models.zip has road.rad");
  if (!road) {
    vfs_free_zip(&zip);
    medium_free(&m);
    return;
  }
  char *text = vfs_entry_text(road);

  m.loadnew = true;
  ContO base;
  cont_o_init_buf(&base, text, &m, &t);
  m.loadnew = false;
  free(text);
  vfs_free_zip(&zip);
  base.shadow = true;
  base.baseIndex = 42;
  CHECK(base.npl == 55 && base.tnt == 3 && !base.errd, "road.rad base model parsed");

  ContO inst;
  cont_o_init_copy(&inst, &base, 1000, -50, 2000, 30);

  CHECK(inst.npl == 55, "npl");
  CHECK(inst.maxR == 2923, "maxR");
  CHECK(inst.disp == 90, "disp");
  CHECK(inst.disline == 8, "disline");
  CHECK(inst.noline == true, "noline");
  CHECK(inst.shadow == true, "shadow");
  CHECK(inst.grounded == 300.0f, "grounded");
  CHECK(inst.decor == false, "decor");
  CHECK(inst.baseIndex == 42, "baseIndex");
  CHECK(inst.x == 1000 && inst.y == -50 && inst.z == 2000, "x/y/z");
  CHECK(inst.xz == 0 && inst.xy == 0 && inst.zy == 0, "xz/xy/zy");
  CHECK(inst.keyx[0] == 0 && inst.keyz[0] == 0, "keyx/keyz");

  for (int32_t i = 0; i < 55 && i < inst.npl; i++) {
    Plane *p = &inst.p[i];
    const CopyPlaneExpect *e = &kCopyExpect[i];
    char label[64];
    snprintf(label, sizeof(label), "clone plane %d n/colnum/master", i);
    CHECK(p->n == e->n && p->colnum == e->colnum && p->master == e->master, label);
    snprintf(label, sizeof(label), "clone plane %d projf", i);
    CHECK(p->projf == e->projf, label);
    snprintf(label, sizeof(label), "clone plane %d ox", i);
    CHECK(memcmp(p->ox, e->ox, sizeof(e->ox)) == 0, label);
    snprintf(label, sizeof(label), "clone plane %d oz", i);
    CHECK(memcmp(p->oz, e->oz, sizeof(e->oz)) == 0, label);
  }

  CHECK(t.nt == 3, "trackers.nt after one clone");
  CHECK(t.xy[0] == 0 && t.zy[0] == 0, "t[0] xy/zy");
  CHECK(t.c[0][0] == 161 && t.c[0][1] == 139 && t.c[0][2] == 176, "t[0] c");
  CHECK(t.x[0] == 1000 && t.z[0] == 2000 && t.y[0] == -50, "t[0] x/z/y");
  CHECK(t.dam[0] == 1 && t.notwall[0] == false && t.decor[0] == false, "t[0] dam/notwall/decor");
  CHECK(t.radx[0] == 2006 && t.radz[0] == 2774 && t.rady[0] == 0, "t[0] radx/radz/rady");
  CHECK(t.c[1][0] == 198 && t.c[1][1] == 171 && t.c[1][2] == 216, "t[1] c");
  CHECK(t.x[1] == 1666 && t.z[1] == 2385 && t.radx[1] == 1460 && t.radz[1] == 2459, "t[1] x/z/radx/radz");
  CHECK(t.x[2] == 333 && t.z[2] == 1615 && t.radx[2] == 1460 && t.radz[2] == 2459, "t[2] x/z/radx/radz");

  cont_o_free(&inst);
  cont_o_free(&base);
  medium_free(&m);
}

// Oracle: the real web/ContO.js's electrify(), run 3 times in a row (like
// 3 frames) on a road.rad clone with elec=true, once with roted=false and
// once with roted=true (each with its OWN fresh seed/Medium -- m.random()
// is one shared stateful stream, so reusing it across the two scenarios
// would make roted=true's randoms depend on how many roted=false already
// consumed, not on roted itself; caught this exact mistake in the oracle
// script before trusting its first, misleading diff -- the fix was giving
// each scenario its own setSeed()+Medium, matched below by giving each
// scenario its own process invocation when this was checked ad hoc).
// Every fillPolygon/drawPolygon call's colour and all 8 vertices, across
// all 3 frames, both scenarios, matched exactly (checked via a temporarily
// instrumented cont_o.c, not kept here), as did the elc[]/edl[]/edr[]/
// xy/zy state after each frame, which IS kept -- it's what the per-bolt
// geometry is computed from, so a regression there is what would actually
// matter and is cheap to keep exhaustively.
static void test_electrify(bool roted, int32_t expect_xy, int32_t expect_zy,
                            const int32_t expect_elc[4], const int32_t expect_edl[4],
                            const int32_t expect_edr[4]) {
  nfm_set_seed(9001);
  Medium m;
  medium_init(&m);
  Trackers t;
  trackers_init(&t);
  medium_setsnap(&m, 10, -5, 20);

  vfs_set_fpath("../../../");
  VfsZip zip;
  CHECK(vfs_read_zip("data/models.zip", &zip), "data/models.zip read (test_electrify)");
  if (!zip.count) {
    medium_free(&m);
    return;
  }
  VfsZipEntry *road = NULL;
  for (int32_t i = 0; i < zip.count; i++) {
    if (strcmp(zip.entries[i].name, "road.rad") == 0) road = &zip.entries[i];
  }
  char *text = vfs_entry_text(road);
  m.loadnew = true;
  ContO base;
  cont_o_init_buf(&base, text, &m, &t);
  m.loadnew = false;
  free(text);
  vfs_free_zip(&zip);
  base.shadow = true;

  ContO inst;
  cont_o_init_copy(&inst, &base, 100, -1400, 5000, 0);
  inst.elec = true;
  inst.roted = roted;
  inst.xz = 0;

  Graphics2D g;
  gfx_init(&g, 800, 450);
  gfx_begin(&g);
  for (int32_t frame = 0; frame < 3; frame++) cont_o_electrify(&inst, &g);

  char label[64];
  snprintf(label, sizeof(label), "electrify roted=%d elc", roted);
  CHECK(memcmp(inst.elc, expect_elc, sizeof(inst.elc)) == 0, label);
  snprintf(label, sizeof(label), "electrify roted=%d edl", roted);
  CHECK(memcmp(inst.edl, expect_edl, sizeof(inst.edl)) == 0, label);
  snprintf(label, sizeof(label), "electrify roted=%d edr", roted);
  CHECK(memcmp(inst.edr, expect_edr, sizeof(inst.edr)) == 0, label);
  snprintf(label, sizeof(label), "electrify roted=%d xy/zy", roted);
  CHECK(inst.xy == expect_xy && inst.zy == expect_zy, label);
  snprintf(label, sizeof(label), "electrify roted=%d drew something", roted);
  CHECK(g.count > 0, label);

  gfx_free(&g);
  cont_o_free(&inst);
  cont_o_free(&base);
  medium_free(&m);
}

// Regression test for the cont_o_fixit() crash: this used to be a stub
// that called abort() unconditionally (see cont_o.h's own doc comment on
// why it was originally believed unreachable, and why that turned out to
// be wrong -- any damaged car that reaches a real stage's `fix(` repair
// point sets co->fix, and cont_o_d() calls this every frame while it's
// true). Drives a real car (mycars/Simple_Car.rad, so npl > 0 and the
// Plane-retint half has something to act on) through the full fcnt
// 0->8 cycle and checks: (1) it doesn't crash -- the entire point of this
// test; (2) the Plane retint fires at the exact fcnt values Java's
// fixit() specifies (flx=1 at fcnt==1, flx=3 at fcnt==4); (3) something
// is actually drawn; (4) the cycle self-terminates (fix clears, fcnt
// resets to 0) after fcnt passes 7, matching ContO.java:1759-1765.
static void test_fixit(void) {
  nfm_set_seed(9001);
  Medium m;
  medium_init(&m);
  Trackers t;
  trackers_init(&t);
  medium_setsnap(&m, 10, -5, 20);

  vfs_set_fpath("../../../");
  char *text = vfs_read_text("mycars/Simple_Car.rad");
  CHECK(text != NULL, "mycars/Simple_Car.rad read (test_fixit)");
  if (!text) {
    medium_free(&m);
    return;
  }
  m.loadnew = true;
  ContO co;
  cont_o_init_buf(&co, text, &m, &t);
  m.loadnew = false;
  free(text);
  CHECK(co.npl > 0, "test_fixit: car has planes to retint");

  medium_follow(&m, 0, 0, 0, 0, 0);
  co.x = 0;
  co.y = 0;
  co.z = 5000;
  co.fix = true;
  co.fcnt = 0;

  Graphics2D g;
  gfx_init(&g, 800, 450);
  gfx_begin(&g);

  for (int32_t frame = 0; frame < 9; frame++) {
    cont_o_fixit(&co, &g); // must not abort -- see this test's own comment
    if (frame == 1) CHECK(co.p[0].flx == 1, "fixit: flx==1 after fcnt==1 frame");
    if (frame == 4) CHECK(co.p[0].flx == 3, "fixit: flx==3 after fcnt==4 frame");
  }
  CHECK(g.count > 0, "fixit: drew something across the cycle");
  CHECK(!co.fix, "fixit: co.fix clears once fcnt passes 7");
  CHECK(co.fcnt == 0, "fixit: co.fcnt resets to 0 once it passes 7");

  gfx_free(&g);
  cont_o_free(&co);
  medium_free(&m);
}

// Regression test for the cont_o_pdust() rewrite -- see cont_o.c's own
// comment on cont_o_pdust for the full list of divergences the old
// (translated-from-an-intermediate-JS-layer) version had versus the real
// ContO.java:1950-2105. This scenario is built to fail loudly against
// EITHER of the two most consequential bugs at once: the always-{0,0,0}
// tint-add array (fixed by computing it from m->snap[]) and the
// camera-relative-instead-of-absolute coordinate bug in the track-sector
// color lookup (fixed by comparing co->sx[n]/sz[n] directly against
// t->x[n2]/t->z[n2], both absolute) -- a deliberately large, nonzero
// camera position (m.x/m.z = 9999) means the OLD buggy comparison
// `abs(sx[n] - (t.x[n2] - m.x))` would land far outside t.radx[n2] and
// silently fall through to the "no match" branch, while the FIXED
// absolute comparison correctly finds the match. If either bug were still
// present, srgb[n]/sbln[n] below would come out wrong.
static void test_pdust(void) {
  nfm_set_seed(9001);
  Medium m;
  medium_init(&m);
  medium_setsnap(&m, 0, 0, 0); // tint-add array[] == {255,255,255} exactly
  m.x = 9999; // deliberately far from the dust particle -- see comment above
  m.z = 9999;
  m.crgrnd[0] = 1; m.crgrnd[1] = 2; m.crgrnd[2] = 3; // only reached if the lookup wrongly misses

  Trackers t;
  trackers_init(&t);
  t.sx = 0;
  t.sz = 0;
  t.ncx = 0;
  t.ncz = 0;
  t.x[0] = 500;
  t.z[0] = 500;
  t.radx[0] = 1000;
  t.radz[0] = 1000;
  t.xy[0] = 0;
  t.zy[0] = 0;
  t.skd[0] = 1; // expect sbln == 0.4f (ContO.java:1988-1990) -- the old
                // port used 0.25f here, a second independent bug this
                // scenario also catches.
  t.c[0][0] = 100; t.c[0][1] = 150; t.c[0][2] = 200;
  // Hand-built 1x1 sector grid pointing at tracker index 0 -- avoids
  // depending on trackers_devidetrackers()'s own geometry-binning logic
  // for a scenario that only needs one deterministic cell.
  t.sect = malloc(sizeof(int32_t **));
  t.sect[0] = malloc(sizeof(int32_t *));
  t.sect[0][0] = malloc(sizeof(int32_t));
  t.sect[0][0][0] = 0;
  t.sect_len = malloc(sizeof(int32_t *));
  t.sect_len[0] = malloc(sizeof(int32_t));
  t.sect_len[0][0] = 1;

  vfs_set_fpath("../../../");
  char *text = vfs_read_text("mycars/Simple_Car.rad");
  CHECK(text != NULL, "mycars/Simple_Car.rad read (test_pdust)");
  if (!text) {
    trackers_free_sect(&t);
    medium_free(&m);
    return;
  }
  m.loadnew = true;
  ContO base;
  cont_o_init_buf(&base, text, &m, &t);
  m.loadnew = false;
  free(text);
  base.shadow = true;

  ContO co;
  cont_o_init_copy(&co, &base, 600, 0, 600, 0);
  co.x = 600;
  co.z = 600;
  co.sx[0] = 600; // abs(600-500)=100 < radx[0]=1000 -- matches, using
  co.sz[0] = 600; // ABSOLUTE coordinates only, no m.x/m.z offset
  co.sy[0] = 0;
  co.stg[0] = 1;
  co.dist = 0; // guarantees sav[0] > dist below, so the block executes

  Graphics2D g;
  gfx_init(&g, 800, 450);
  gfx_begin(&g);
  int32_t count_before = g.count;

  cont_o_pdust(&co, 0, &g, true);

  CHECK(co.sbln[0] == 0.4f, "pdust: sbln matches skd==1 via the ABSOLUTE-coordinate sector match");
  CHECK(co.srgb[0][0] == (100 + 255) / 2, "pdust: srgb[0] blends real track colour + snap-derived tint (not always-0)");
  CHECK(co.srgb[0][1] == (150 + 255) / 2, "pdust: srgb[1] blends real track colour + snap-derived tint (not always-0)");
  CHECK(co.srgb[0][2] == (200 + 255) / 2, "pdust: srgb[2] blends real track colour + snap-derived tint (not always-0)");
  CHECK(g.count > count_before, "pdust: drew something");
  if (g.count > count_before) {
    uint8_t alpha = (uint8_t)(g.verts[count_before].rgba >> 24);
    // Expected alpha == sbln[0] - stg_at_draw_time(1)*(sbln[0]/8) = 0.4 -
    // 1*0.05 = 0.35 -> ~89/255. A wide-ish tolerance band (60..140, i.e.
    // roughly 0.24..0.55) still cleanly distinguishes this from the old
    // code's fully-opaque 255 (no AlphaComposite call at all).
    CHECK(alpha > 60 && alpha < 140, "pdust: drawn with genuine partial alpha, not fully opaque");
  }

  gfx_free(&g);
  cont_o_free(&co);
  cont_o_free(&base);
  trackers_free_sect(&t);
  medium_free(&m);
}

int main(void) {
  test_simple_car();
  test_d_ground_shadow();
  test_d_lowshadow_far_camera();
  test_init_copy();
  {
    const int32_t elc[4] = {4, 3, 4, 4}, edl[4] = {-304, 380, -152, 151}, edr[4] = {304, 0, 151, -304};
    test_electrify(false, 33, 0, elc, edl, edr);
  }
  {
    const int32_t elc[4] = {4, 3, 4, 4}, edl[4] = {-304, 380, -152, 151}, edr[4] = {304, 0, 151, -304};
    test_electrify(true, 0, 33, elc, edl, edr);
  }
  test_fixit();
  test_pdust();
  if (failures == 0) {
    printf("all tests passed\n");
    return 0;
  }
  fprintf(stderr, "%d failure(s)\n", failures);
  return 1;
}
