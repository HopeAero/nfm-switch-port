// Host-buildable test for native/core/check_points.c -- see check_points.h
// for scope.
//
// No web/CheckPoints.test.js exists; expected values captured by running
// the real web/CheckPoints.js under Node (seed 9001): the constructor's
// random `stage` default, a calprox scenario, and two checkstat()
// scenarios (see checkstat_scenarios() below).
#include <stdio.h>
#include <string.h>
#include "../core/java_compat.h"
#include "../core/check_points.h"
#include "../core/mad.h"
#include "../core/cont_o.h"
#include "../core/record.h"
#include "../core/car_define.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
  if (!(cond)) { fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); failures++; } \
} while (0)

// checkstat() only reads/writes a small, specific set of Mad/ContO fields
// (see check_points.c's own body) -- these helpers build minimal fixtures
// satisfying just those, the same minimal-fixture approach the Node
// oracle script used (plain objects, not full Mad/ContO instances).
static Mad make_car(CarDefine *cd, int32_t hitmag, int32_t cn, int32_t pcleared,
                     int32_t clear, int32_t mxz, bool dest) {
  Mad mad;
  memset(&mad, 0, sizeof(mad));
  mad.cd = cd;
  mad.hitmag = hitmag;
  mad.cn = cn;
  mad.pcleared = pcleared;
  mad.clear = clear;
  mad.mxz = mxz;
  mad.dest = dest;
  mad.shakedam = 5;
  return mad;
}
static ContO make_cont_o(int32_t x, int32_t z, int32_t dist) {
  ContO co;
  memset(&co, 0, sizeof(co));
  co.x = x;
  co.z = z;
  co.dist = dist;
  return co;
}

// Two-car race-position scenario: car 0 (local player, n2=0) has cleared
// 3 checkpoints, car 1 only 2 -- exercises the clear[j] !== clear[k]
// ranking branch (no tie-break distance check needed since they differ).
// Oracle: real web/CheckPoints.js's checkstat(), same fixture shape.
static void checkstat_ranking_scenario(void) {
  CheckPoints cp;
  check_points_init(&cp);
  cp.n = 4;
  cp.typ[0] = 1; cp.typ[1] = 1; cp.typ[2] = 1; cp.typ[3] = 1;
  cp.x[0] = 0; cp.x[1] = 1000; cp.x[2] = 2000; cp.x[3] = 3000;
  cp.z[0] = 0; cp.z[1] = 0; cp.z[2] = 0; cp.z[3] = 0;
  cp.nlaps = 2;
  cp.nsp = 4;
  cp.stage = 5;

  CarDefine cd;
  car_define_init(&cd);
  cd.maxmag[0] = 8000;
  cd.maxmag[3] = 8000;

  Mad mads[2] = {
      make_car(&cd, 4000, 0, 2, 3, 45, false),
      make_car(&cd, 1000, 3, 1, 2, 90, false),
  };
  ContO contOs[2] = {
      make_cont_o(2950, 0, 500),
      make_cont_o(1950, 0, 400),
  };
  Mad *mad_ptrs[2] = {&mads[0], &mads[1]};
  ContO *co_ptrs[2] = {&contOs[0], &contOs[1]};
  static Record record;
  record_init(&record);

  check_points_checkstat(&cp, mad_ptrs, co_ptrs, &record, 2, 0, 0);

  bool ok = cp.pcleared == 2 && cp.pos[0] == 0 && cp.pos[1] == 1 &&
            cp.magperc[0] == 0.5f && cp.magperc[1] == 0.125f &&
            cp.onscreen[0] == 500 && cp.onscreen[1] == 400 &&
            cp.opx[0] == 2950 && cp.opx[1] == 1950 &&
            cp.omxz[0] == 45 && cp.omxz[1] == 90 &&
            cp.clear[0] == 3 && cp.clear[1] == 2 &&
            mads[0].outshakedam == 5 && mads[0].shakedam == 0 &&
            mads[1].outshakedam == 5 && mads[1].shakedam == 0 &&
            cp.wasted == 0 && cp.catchfin == 0;
  CHECK(ok, "checkstat ranking (car0 ahead of car1 by clear count)");
}

// Photo-finish scenario: car 0 (n2=0) just cleared enough checkpoints to
// finish (clear === nlaps*nsp) while in first place, with car 1 close
// behind (py distance < 14000, clear difference exactly 1) -- exercises
// the `stage > 2` catch-up detection branch, which sets catchfin=30 then
// immediately decrements it once in the same call (the "wasted" tail
// runs unconditionally after the haltall-gated block).
static void checkstat_catchfin_scenario(void) {
  CheckPoints cp;
  check_points_init(&cp);
  cp.n = 4;
  cp.typ[0] = 1; cp.typ[1] = 1; cp.typ[2] = 1; cp.typ[3] = 1;
  cp.x[0] = 0; cp.x[1] = 1000; cp.x[2] = 2000; cp.x[3] = 3000;
  cp.z[0] = 0; cp.z[1] = 0; cp.z[2] = 0; cp.z[3] = 0;
  cp.nlaps = 1;
  cp.nsp = 1;
  cp.stage = 5;

  CarDefine cd;
  car_define_init(&cd);
  cd.maxmag[0] = 8000;
  cd.maxmag[3] = 8000;

  Mad mads[2] = {
      make_car(&cd, 0, 0, 0, 1, 0, false),
      make_car(&cd, 0, 3, 3, 0, 0, false),
  };
  ContO contOs[2] = {
      make_cont_o(100, 0, 500),
      make_cont_o(50, 0, 400),
  };
  Mad *mad_ptrs[2] = {&mads[0], &mads[1]};
  ContO *co_ptrs[2] = {&contOs[0], &contOs[1]};
  static Record record;
  record_init(&record);

  check_points_checkstat(&cp, mad_ptrs, co_ptrs, &record, 2, 0, 0);

  bool ok = cp.pos[0] == 0 && cp.pos[1] == 1 && cp.catchfin == 29 && cp.postwo == 1;
  CHECK(ok, "checkstat catchfin (local player finishes with a close second place)");
}

int main(void) {
  nfm_set_seed(9001);
  CheckPoints cp;
  check_points_init(&cp);
  CHECK(cp.stage == 15, "stage default");
  CHECK(strcmp(cp.name, "hogan rewish") == 0, "name default");
  CHECK(cp.trackvol == 200, "trackvol default");
  bool pos_ok = true;
  for (int32_t i = 0; i < 8; i++) if (cp.pos[i] != 7) pos_ok = false;
  CHECK(pos_ok, "pos default all 7");

  cp.n = 4;
  cp.x[0] = 100; cp.z[0] = 200;
  cp.x[1] = -500; cp.z[1] = 300;
  cp.x[2] = 700; cp.z[2] = -900;
  cp.x[3] = 50; cp.z[3] = 50;
  check_points_calprox(&cp);
  CHECK(cp.prox == 13.333333f, "calprox");

  CHECK(check_points_py(10, 3, 20, 7) == (10 - 3) * (10 - 3) + (20 - 7) * (20 - 7), "py");

  checkstat_ranking_scenario();
  checkstat_catchfin_scenario();

  if (failures == 0) {
    printf("all tests passed\n");
    return 0;
  }
  fprintf(stderr, "%d failure(s)\n", failures);
  return 1;
}
