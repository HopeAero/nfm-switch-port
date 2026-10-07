// Host-buildable test for native/core/game_sparker.c -- see game_sparker.h
// for the exact lean scope (real geometry + colours, no CheckPoints/
// XtGraphics/Control/Record bookkeeping, no procedural decor, no `pile`).
//
// No web/GameSparker.test.js exists, so -- same fallback as trackers_test.c
// and friends -- expected values were captured by running the REAL
// web/GameSparker.js's loadbase()+loadstage() under Node, with real
// CheckPoints/XtGraphics instances (their defaults are enough for
// loadstage() not to crash; nothing about their state is compared, since
// this port doesn't have them). kTestStage below is a synthetic stage
// file, not one from stages/, deliberately built to exercise every kept
// command (snap/sky/ground/fog/set/chk/fix/maxr/maxl/maxt/maxb) while
// avoiding `pile(` -- real stage files have pile() lines, which the real
// JS places via ContO's #initModel (a real, working constructor there)
// but this port's game_sparker_loadstage skips entirely, which would
// shift every object's array index after the first pile line and make a
// direct index-by-index oracle comparison meaningless.
//
// The JS's own `this.nob` starts at `xtGraphics.nplayers` (7 by default)
// -- the real game reserves the first N array slots for player cars,
// filled in elsewhere. This port's placed-objects array has no such
// reservation (the player's own car is drawn separately, outside this
// array -- see platform/linux/main.c), so game_sparker_loadstage's
// objects start at index 0; the oracle run skips the JS's first 7 (null)
// slots before comparing.
//
// Matched exactly on the first attempt: loadbase()'s 621172-byte total
// (the same tamper/truncation check the JS performs against the real
// data/models.zip), all 31 Trackers entries (6 from tracks(...) volumes
// on the placed objects + 4 boundary walls' own trackers), t.ncx/t.ncz,
// Medium's csky/cgrnd/cfade/crgrnd after snap/sky/ground/fog, and all 16
// placed objects' baseIndex/npl/x/y/z/xz/elec/roted -- including fix()'s
// y/z field-swap and elec/roted flags.
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
#include "../core/game_sparker.h"
#include "../core/check_points.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
  if (!(cond)) { fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); failures++; } \
} while (0)

static const char *kTestStage =
  "snap(-5,-5,20)\n"
  "sky(207,232,255)\n"
  "fog(198,219,224)\n"
  "ground(185,210,205)\n"
  "\n"
  "set(10,0,0,0)\n"
  "set(47,500,2000,90)\n"
  "set(26,-800,3500,180)\n"
  "chk(20,1200,4800,0)\n"
  "fix(30,-500,10600,-1400,0)\n"
  "fix(30,300,11600,-1400,90)\n"
  "nlaps(3)\n"
  "\n"
  "maxr(3,-4800,0)\n"
  "maxl(3,4800,0)\n"
  "maxt(2,-3200,0)\n"
  "maxb(2,3200,0)\n";

typedef struct {
  int32_t baseIndex, npl, x, y, z, xz;
  bool elec, roted;
} ObjExpect;

static const ObjExpect kObjExpect[16] = {
  {56, 55, 0, 250, 0, 0, false, false},
  {93, 74, 500, 250, 2000, 0, false, false},
  {72, 26, -800, 250, 3500, 0, false, false},
  {66, 54, 1200, 250, 4800, 0, false, false},
  {76, 82, -500, -1400, 10600, 0, true, false},
  {76, 82, 300, -1400, 11600, 0, true, true},
  {85, 60, -4800, 250, 0, 0, false, false},
  {85, 60, -4800, 250, 4800, 0, false, false},
  {85, 60, -4800, 250, 9600, 0, false, false},
  {85, 60, 4800, 250, 0, 0, false, false},
  {85, 60, 4800, 250, 4800, 0, false, false},
  {85, 60, 4800, 250, 9600, 0, false, false},
  {85, 60, 0, 250, -3200, 0, false, false},
  {85, 60, 4800, 250, -3200, 0, false, false},
  {85, 60, 0, 250, 3200, 0, false, false},
  {85, 60, 4800, 250, 3200, 0, false, false},
};

typedef struct {
  int32_t x, y, z, radx, radz, rady, xy, zy, dam, skd;
} TrackExpect;

static const TrackExpect kTrackExpect[31] = {
  {0, 250, 0, 700, 2800, 0, 0, 0, 1, 0},
  {770, 250, 0, 70, 2800, 0, 0, 0, 1, 0},
  {-770, 250, 0, 70, 2800, 0, 0, 0, 1, 0},
  {500, 250, 2000, 2800, 700, 0, 0, 0, 1, 0},
  {500, 250, 2770, 2800, 70, 0, 0, 0, 1, 0},
  {499, 250, 1230, 2800, 70, 0, 0, 0, 1, 0},
  {-800, -38, 3600, 500, 500, 300, 0, 30, 1, 0},
  {-800, 250, 3000, 700, 200, 350, 0, 90, 1, 0},
  {-1400, 250, 3300, 200, 400, 350, 90, 0, 1, 0},
  {-200, 250, 3300, 200, 400, 350, -90, 0, 1, 0},
  {1200, 250, 4800, 700, 1190, 0, 0, 0, 1, 1},
  {1200, 250, 6760, 700, 910, 0, 0, 0, 1, 0},
  {1200, 250, 2840, 700, 910, 0, 0, 0, 1, 2},
  {2040, 250, 4800, 280, 1190, 0, 0, 0, 1, 1},
  {2040, 250, 2840, 280, 910, 0, 0, 0, 1, 2},
  {360, 250, 4800, 280, 1190, 0, 0, 0, 1, 1},
  {360, 250, 2840, 280, 910, 0, 0, 0, 1, 2},
  {100, -1400, 10062, 200, 2218, 100, -90, 0, 3, 0},
  {-1100, -1400, 10062, 200, 2218, 100, 90, 0, 3, 0},
  {-500, -1983, 11440, 700, 840, 300, 0, 0, 1, 0},
  {-500, -1692, 8946, 700, 1654, 700, 0, -10, 1, 0},
  {-500, -1690, 12250, 700, 200, 300, 0, -90, 3, 0},
  {838, -1400, 12200, 2218, 200, 100, 0, -90, 3, 0},
  {838, -1400, 11000, 2218, 200, 100, 0, 90, 3, 0},
  {-540, -1983, 11600, 840, 700, 300, 0, 0, 1, 0},
  {1954, -1692, 11600, 1654, 700, 700, 10, 0, 1, 0},
  {-1350, -1690, 11600, 200, 700, 300, 90, 0, 3, 0},
  {-4300, -5000, 4800, 600, 7200, 7100, 90, 0, 1, 0},
  {4300, -5000, 4800, 600, 7200, 7100, -90, 0, 1, 0},
  {2400, -5000, -2700, 4800, 600, 7100, 0, 90, 1, 0},
  {2400, -5000, 2700, 4800, 600, 7100, 0, -90, 1, 0},
};

static void test_loadbase_and_loadstage(void) {
  nfm_set_seed(9001);
  Medium m;
  medium_init(&m);
  Trackers t;
  trackers_init(&t);

  vfs_set_fpath("../../../"); // native/tests/build/ -> repo root
  ContO *base_models = calloc(GAME_SPARKER_NUM_BASE_MODELS, sizeof(ContO));
  bool ok = game_sparker_loadbase(base_models, &m, &t, "data/models.zip");
  CHECK(ok, "loadbase: total uncompressed size matches 621172");

  ContO *objects = calloc(610, sizeof(ContO));
  int32_t count = 0;
  CheckPoints cp;
  check_points_init(&cp);
  bool sok = game_sparker_loadstage(objects, 610, &count, base_models, &m, &t, &cp, kTestStage,
                                     NULL, NULL);
  // One checkpoint: rejected like Java (stage -3), but parsed in full.
  CHECK(!sok, "loadstage: a one-checkpoint stage is rejected");
  CHECK(count == 16, "loadstage: object count");

  // Real checkpoint/fix-point/nlaps bookkeeping (see game_sparker.h's own
  // doc comment) -- checked against the same oracle run's CheckPoints
  // instance, not just the placed geometry above.
  CHECK(cp.n == 1 && cp.nsp == 1 && cp.pcs == 0, "checkpoints: n/nsp/pcs");
  CHECK(cp.x[0] == 1200 && cp.z[0] == 4800 && cp.y[0] == 250 && cp.typ[0] == 1,
        "checkpoints: chk(20,1200,4800,0) -> typ 1 (arg3==0)");
  CHECK(objects[3].checkpoint == 1, "checkpoints: chk object's own checkpoint field (nsp+1 at placement time)");
  CHECK(cp.fn == 2, "fix points: fn");
  CHECK(cp.fx[0] == -500 && cp.fz[0] == 10600 && cp.fy[0] == -1400 && !cp.roted[0] && !cp.special[0],
        "fix points: fix(30,-500,10600,-1400,0)");
  CHECK(cp.fx[1] == 300 && cp.fz[1] == 11600 && cp.fy[1] == -1400 && cp.roted[1] && !cp.special[1],
        "fix points: fix(30,300,11600,-1400,90) -> roted (arg4!=0)");
  CHECK(cp.nlaps == 3, "nlaps(3)");

  CHECK(m.ground == 250 && m.trk == 0 && m.resdown == 0, "medium ground/trk/resdown");
  CHECK(m.csky[0] == 196 && m.csky[1] == 220 && m.csky[2] == 255, "csky");
  CHECK(m.cgrnd[0] == 175 && m.cgrnd[1] == 199 && m.cgrnd[2] == 246, "cgrnd");
  CHECK(m.cfade[0] == 188 && m.cfade[1] == 208 && m.cfade[2] == 255, "cfade");
  CHECK(m.crgrnd[0] == 172 && m.crgrnd[1] == 195 && m.crgrnd[2] == 241, "crgrnd");

  CHECK(t.nt == 31, "trackers.nt");
  for (int32_t i = 0; i < 31 && i < t.nt; i++) {
    const TrackExpect *e = &kTrackExpect[i];
    char label[32];
    snprintf(label, sizeof(label), "t[%d]", i);
    CHECK(t.x[i] == e->x && t.y[i] == e->y && t.z[i] == e->z &&
              t.radx[i] == e->radx && t.radz[i] == e->radz && t.rady[i] == e->rady &&
              t.xy[i] == e->xy && t.zy[i] == e->zy && t.dam[i] == e->dam && t.skd[i] == e->skd,
          label);
  }
  CHECK(t.ncx == 0 && t.ncz == 0, "t.ncx/ncz");

  for (int32_t i = 0; i < 16 && i < count; i++) {
    ContO *c = &objects[i];
    const ObjExpect *e = &kObjExpect[i];
    char label[32];
    snprintf(label, sizeof(label), "obj[%d]", i);
    CHECK(c->baseIndex == e->baseIndex && c->npl == e->npl && c->x == e->x && c->y == e->y &&
              c->z == e->z && c->xz == e->xz && c->elec == e->elec && c->roted == e->roted,
          label);
  }

  for (int32_t i = 0; i < count; i++) cont_o_free(&objects[i]);
  free(objects);
  for (int32_t i = 0; i < GAME_SPARKER_NUM_BASE_MODELS; i++) {
    if (base_models[i].p) cont_o_free(&base_models[i]);
  }
  free(base_models);
  medium_free(&m);
  trackers_free_sect(&t);
}

// Regression for the stage-select 3D preview's camera-center out-params
// (GameSparker.java:2736-2737's `medium.trx = (getint2+getint)/2` /
// `medium.trz = (getint3+getint4)/2`). Deliberately asymmetric bounds so
// a mixed-up ge1..ge4 or a swapped x/z would fail this, unlike kTestStage
// above (whose maxr/maxl/maxt/maxb happen to be symmetric around 0).
static void test_loadstage_center(void) {
  static const char *kCenterStage =
      "sky(0,0,0)\n"
      "ground(0,0,0)\n"
      "maxr(1,1000,0)\n"
      "maxl(1,-2000,0)\n"
      "maxt(1,3000,0)\n"
      "maxb(1,-6000,0)\n";

  Medium m;
  medium_init(&m);
  Trackers t;
  trackers_init(&t);
  vfs_set_fpath("../../../");
  ContO *base_models = calloc(GAME_SPARKER_NUM_BASE_MODELS, sizeof(ContO));
  bool ok = game_sparker_loadbase(base_models, &m, &t, "data/models.zip");
  CHECK(ok, "loadstage_center: loadbase");

  ContO *objects = calloc(32, sizeof(ContO));
  int32_t count = 0;
  CheckPoints cp;
  check_points_init(&cp);
  int32_t cx = -999999, cz = -999999;
  bool sok = game_sparker_loadstage(objects, 32, &count, base_models, &m, &t, &cp, kCenterStage,
                                     &cx, &cz);
  CHECK(!sok, "loadstage_center: no checkpoints, rejected");
  // ge1=1000 (maxr), ge2=-2000 (maxl), ge3=3000 (maxt), ge4=-6000 (maxb).
  // center_x = (ge2+ge1)/2 = (-2000+1000)/2 = -500
  // center_z = (ge3+ge4)/2 = (3000+-6000)/2 = -1500
  CHECK(cx == -500, "loadstage_center: center_x");
  CHECK(cz == -1500, "loadstage_center: center_z");

  for (int32_t i = 0; i < count; i++) cont_o_free(&objects[i]);
  free(objects);
  for (int32_t i = 0; i < GAME_SPARKER_NUM_BASE_MODELS; i++) {
    if (base_models[i].p) cont_o_free(&base_models[i]);
  }
  free(base_models);
  medium_free(&m);
  trackers_free_sect(&t);
}

// GameSparker.java:2697-2707's stage == -3 checks: two checkpoints race, a
// model id outside the base models fails instead of reading past the array.
static void test_loadstage_rejects(void) {
  Medium m;
  medium_init(&m);
  Trackers t;
  trackers_init(&t);
  vfs_set_fpath("../../../");
  ContO *base_models = calloc(GAME_SPARKER_NUM_BASE_MODELS, sizeof(ContO));
  CHECK(game_sparker_loadbase(base_models, &m, &t, "data/models.zip"), "rejects: loadbase");
  static const char *kStages[3] = {
      "chk(20,0,0,0)\nchk(20,0,2000,0)\n",
      "chk(20,0,0,0)\nchk(20,0,2000,0)\nset(500,0,0,0)\n",
      "chk(20,0,0,0)\nchk(20,0,2000,0)\nset(-100,0,0,0)\n",
  };
  for (int32_t k = 0; k < 3; k++) {
    ContO *objects = calloc(32, sizeof(ContO));
    int32_t count = 0;
    CheckPoints cp;
    check_points_init(&cp);
    bool sok = game_sparker_loadstage(objects, 32, &count, base_models, &m, &t, &cp, kStages[k], NULL, NULL);
    CHECK(sok == (k == 0), k == 0 ? "rejects: two checkpoints load" : "rejects: bad model id fails");
    for (int32_t i = 0; i < count; i++) cont_o_free(&objects[i]);
    free(objects);
  }
  // Every stock stage still passes the checks.
  for (int32_t n = 1; n <= 32; n++) {
    char path[32], label[48];
    snprintf(path, sizeof(path), "stages/%d.txt", n);
    char *text = vfs_read_text(path);
    ContO *objects = calloc(610, sizeof(ContO));
    int32_t count = 0;
    CheckPoints cp;
    check_points_init(&cp);
    bool sok = text && game_sparker_loadstage(objects, 610, &count, base_models, &m, &t, &cp, text, NULL, NULL);
    snprintf(label, sizeof(label), "rejects: stock stage %d loads", n);
    CHECK(sok, label);
    for (int32_t i = 0; i < count; i++) cont_o_free(&objects[i]);
    free(objects);
    free(text);
  }
  for (int32_t i = 0; i < GAME_SPARKER_NUM_BASE_MODELS; i++) {
    if (base_models[i].p) cont_o_free(&base_models[i]);
  }
  free(base_models);
  medium_free(&m);
  trackers_free_sect(&t);
}

int main(void) {
  test_loadbase_and_loadstage();
  test_loadstage_center();
  test_loadstage_rejects();
  if (failures == 0) {
    printf("all tests passed\n");
    return 0;
  }
  fprintf(stderr, "%d failure(s)\n", failures);
  return 1;
}
