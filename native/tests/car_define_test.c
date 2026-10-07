// Host-buildable test for native/core/car_define.c -- see car_define.h for
// scope (the 20 stat tables Mad.js reads, PLUS loadstat()/loadcar()).
//
// Expected values captured by running the real web/CarDefine.js (+
// web/ContO.js/Medium.js/Trackers.js) under Node against the actual
// mycars/Simple_Car.rad file -- see the loadstat/loadcar test below for
// the exact oracle script equivalent.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "../core/car_define.h"
#include "../core/cont_o.h"
#include "../core/medium.h"
#include "../core/trackers.h"
#include "../core/vfs.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
  if (!(cond)) { fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); failures++; } \
} while (0)
#define CHECK_F(actual, expected, msg) do { \
  if (fabsf((actual) - (expected)) > 0.0005f) { \
    fprintf(stderr, "FAIL: %s: got %g want %g (%s:%d)\n", msg, (double)(actual), (double)(expected), __FILE__, __LINE__); \
    failures++; \
  } \
} while (0)

static void test_init(void) {
  CarDefine cd;
  car_define_init(&cd);

  CHECK(cd.handb[13] == 12, "handb[13]");
  CHECK(cd.grip[10] == 22.5f, "grip[10]");
  CHECK(cd.acelf[5][0] == 12.0f && cd.acelf[5][1] == 6.0f && cd.acelf[5][2] == 3.0f, "acelf[5]");
  CHECK(cd.swits[15][0] == 70 && cd.swits[15][1] == 210 && cd.swits[15][2] == 290, "swits[15]");
  CHECK(cd.maxmag[13] == 45000, "maxmag[13]");
  CHECK(cd.dammult[8] == 0.58f, "dammult[8]");
  CHECK(cd.enginsignature[11] == 4, "enginsignature[11]");
  CHECK(cd.flipy[13] == -100, "flipy[13]");
  CHECK(cd.powerloss[13] == 16700000, "powerloss[13]");
  // Extended's retuned NFM 2 cars on top (Madness.java:404-435, its 23-38).
  CarDefine ext;
  car_define_init(&ext);
  car_define_extended(&ext);
  CHECK(ext.grip[10] == 22.4f && ext.maxmag[13] == 63000 && ext.dammult[8] == 0.6f, "extended: grip, maxmag, dammult");
  CHECK(ext.revpush[14] == 0.25f && ext.revpush[15] == 0.4f, "extended: revpush fractions");
  CHECK(ext.acelf[6][0] == 9.0f && ext.acelf[6][1] == 7.0f && ext.swits[0][1] == 180, "extended: Lead Oxide's gears, Tornado Shark");
  CHECK(ext.handb[13] == cd.handb[13] && ext.turn[5] == cd.turn[5], "extended: handling left as NFM 2's");
  // The car-select bars read these for the stock cars (CarDefine.java:108-110).
  CHECK(cd.dishandle[0] == 0.65f && cd.dishandle[14] == 1.0f, "dishandle[0], [14]");
  CHECK(cd.outdam[1] == 0.35f && cd.outdam[13] == 1.0f, "outdam[1], [13]");
  CHECK(cd.cclass[5] == 1 && cd.cclass[11] == 4, "cclass[5], [11]");
  // Unused custom-car slots (16-55) are all zero, including the padded
  // 56th acelf row (see car_define.h's doc comment on the JS's own
  // 55-vs-56-length quirk).
  CHECK(cd.handb[16] == 0 && cd.grip[55] == 0.0f && cd.acelf[55][0] == 0.0f, "unused slots zero");
}

// Ports web/ContO.js/Medium.js/Trackers.js/CarDefine.js's own real
// `new ContO(bytes, m, t); cd.loadcar('Simple Car', 16, text);` sequence
// (as CarDefine.js's loadcar() itself does -- see its lines 710-746) and
// checks the result against a Node run of that exact sequence against the
// real mycars/Simple_Car.rad file. This is the only test in the suite
// that exercises loadstat()/loadcar() against real, non-synthetic input.
static void test_loadcar_simple_car(void) {
  vfs_set_fpath("../../../");
  char *text = vfs_read_text("mycars/Simple_Car.rad");
  CHECK(text != NULL, "mycars/Simple_Car.rad read");
  if (!text) return;

  Medium m;
  medium_init(&m);
  Trackers t;
  trackers_init(&t);
  m.loadnew = true;
  ContO co;
  cont_o_init_buf(&co, text, &m, &t);
  m.loadnew = false;
  CHECK(!co.errd && co.npl > 60, "Simple_Car.rad parses");
  // Oracle (Node): maxR 130 roofat -50 wh 30, keyx [-72,72,-72,72], keyz [87,87,-79,-79]
  CHECK(co.maxR == 130, "co.maxR");
  CHECK(co.roofat == -50, "co.roofat");
  CHECK(co.wh == 30, "co.wh");
  CHECK(co.keyx[0] == -72 && co.keyx[1] == 72 && co.keyx[2] == -72 && co.keyx[3] == 72, "co.keyx");
  CHECK(co.keyz[0] == 87 && co.keyz[1] == 87 && co.keyz[2] == -79 && co.keyz[3] == -79, "co.keyz");

  CarDefine cd;
  car_define_init(&cd);
  bool ok = car_define_loadcar(&cd, text, &co, co.maxR, co.roofat, co.wh, 16);
  CHECK(ok, "car_define_loadcar succeeds for Simple_Car.rad");

  const int n = 16;
  // Oracle values below captured by running the real web/CarDefine.js
  // (loadcar -> loadstat) against mycars/Simple_Car.rad under Node.
  CHECK(cd.handb[n] == 12, "handb[16]");
  CHECK_F(cd.airs[n], 0.8269078731536865f, "airs[16]");
  CHECK(cd.airc[n] == 44, "airc[16]");
  CHECK(cd.turn[n] == 9, "turn[16]");
  CHECK_F(cd.grip[n], 21.8799991607666f, "grip[16]");
  CHECK_F(cd.bounce[n], 1.4000000953674316f, "bounce[16]");
  CHECK_F(cd.simag[n], 1.0671000480651855f, "simag[16]");
  CHECK_F(cd.moment[n], 1.1402174234390259f, "moment[16]");
  CHECK_F(cd.comprad[n], 0.41304346919059753f, "comprad[16]");
  CHECK(cd.push[n] == 2, "push[16]");
  CHECK(cd.revpush[n] == 2, "revpush[16]");
  CHECK(cd.lift[n] == 0, "lift[16]");
  CHECK(cd.revlift[n] == 23, "revlift[16]");
  CHECK(cd.powerloss[n] == 3590000, "powerloss[16]");
  CHECK(cd.flipy[n] == -50, "flipy[16]");
  CHECK(cd.msquash[n] == 8, "msquash[16]");
  CHECK(cd.clrad[n] == 12150, "clrad[16]");
  CHECK_F(cd.dammult[n], 0.6200000047683716f, "dammult[16]");
  CHECK(cd.maxmag[n] == 4588, "maxmag[16]");
  CHECK(cd.enginsignature[n] == 3, "enginsignature[16]");
  CHECK_F(cd.dishandle[n], 0.5f, "dishandle[16]");
  CHECK_F(cd.outdam[n], 0.5699999332427979f, "outdam[16]");
  CHECK(cd.cclass[n] == 2, "cclass[16]");
  CHECK_F(cd.acelf[n][0], 11.0f, "acelf[16][0]");
  CHECK_F(cd.acelf[n][1], 7.5f, "acelf[16][1]");
  CHECK_F(cd.acelf[n][2], 4.0f, "acelf[16][2]");
  CHECK(cd.swits[n][0] == 70 && cd.swits[n][1] == 209 && cd.swits[n][2] == 290, "swits[16]");

  cont_o_free(&co);
  medium_free(&m);
  free(text);
}

int main(void) {
  test_init();
  test_loadcar_simple_car();

  if (failures == 0) {
    printf("all tests passed\n");
    return 0;
  }
  fprintf(stderr, "%d failure(s)\n", failures);
  return 1;
}
