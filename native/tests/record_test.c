// Host-buildable test for native/core/record.c -- see record.h for scope
// (recx/recy/recz only, not the full 857-line file).
//
// Expected values captured by running the real web/Record.js under Node:
// two recy() calls (fresh dent + touch flag), one recx(), one recz(), all
// on car slot 3. recx/recz deliberately index via nry (not nrx/nrz)
// despite incrementing their own counters -- matches the JS's own
// preserved-Java-bug comment; the oracle scenario is built specifically
// so nry[3][1] (what recx reads) and nry[3][0] (what recz reads) are
// still 0 at that point, distinct from nry[3][2] (already bumped to 2 by
// the two recy() calls), so a regression that "fixes" the quirk into
// reading nrx/nrz instead would be caught (slot 0 vs whatever nrx/nrz
// happened to be).
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../core/java_compat.h"
#include "../core/medium.h"
#include "../core/trackers.h"
#include "../core/cont_o.h"
#include "../core/car_define.h"
#include "../core/record.h"
#include "../core/xt_graphics.h"
#include "../core/check_points.h"
#include "../core/control.h"
#include "../core/mad.h"
#include "../core/vfs.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
  if (!(cond)) { fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); failures++; } \
} while (0)

// Real 8-car test rig -- same "load Simple_Car.rad, cont_o_init_copy it"
// pattern mad_test.c uses, but forcing shadow=true on the base model
// before copying (Simple_Car.rad itself has no `shadow` command, but
// every actual placed race car does end up shadow=true in the real game
// -- see cont_o_init_copy's own doc comment for why THAT'S what allocates
// stg/sx/sy/sz/scx/scz/osmag/rx/ry/rz, which record_rec/play/playh all
// dereference).
typedef struct RecordTestRig {
  Medium m;
  Trackers t;
  CarDefine cd;
  CheckPoints cp;
  XtGraphicsStub xt;
  ContO base;
  ContO cars[8];
  Mad mad[8];
  char *text;
} RecordTestRig;

static bool rig_init(RecordTestRig *rig) {
  memset(rig, 0, sizeof(*rig));
  nfm_set_seed(9001);
  medium_init(&rig->m);
  trackers_init(&rig->t);
  car_define_init(&rig->cd);
  check_points_init(&rig->cp);
  xt_graphics_stub_init(&rig->xt);

  vfs_set_fpath("../../../");
  rig->text = vfs_read_text("mycars/Simple_Car.rad");
  if (!rig->text) return false;
  cont_o_init_buf(&rig->base, rig->text, &rig->m, &rig->t);
  rig->base.shadow = true; // see this file's own top comment
  for (int32_t i = 0; i < 8; i++) {
    cont_o_init_copy(&rig->cars[i], &rig->base, i * 1000, 0, i * 500, 0);
    mad_init(&rig->mad[i], &rig->cd, &rig->m, NULL /*rpd set per-test*/, &rig->xt, i);
    rig->mad[i].cn = 0;
  }
  return true;
}

static void rig_free(RecordTestRig *rig) {
  for (int32_t i = 0; i < 8; i++) cont_o_free(&rig->cars[i]);
  cont_o_free(&rig->base);
  trackers_free_sect(&rig->t);
  medium_free(&rig->m);
  free(rig->text);
}

static void test_reset(void) {
  RecordTestRig rig;
  if (!rig_init(&rig)) { CHECK(false, "reset: rig_init (Simple_Car.rad)"); return; }
  Record r;
  record_init(&r);
  CHECK(r.cntf == 50, "init: cntf==50");
  CHECK(r.hfix[3] == -1 && r.hdest[5] == -1, "init: hfix/hdest default to -1");
  CHECK(r.fix[3] == 0 && r.dest[5] == 0, "init: fix/dest default to 0 (NOT -1 until reset)");

  ContO *carPtrs[8];
  for (int32_t i = 0; i < 8; i++) carPtrs[i] = &rig.cars[i];
  record_reset(&r, carPtrs);
  for (int32_t i = 0; i < 8; i++) {
    CHECK(r.fix[i] == -1, "reset: fix[i]==-1");
    CHECK(r.dest[i] == -1, "reset: dest[i]==-1");
    CHECK(r.cntdest[i] == 0, "reset: cntdest[i]==0");
    CHECK(r.rspark[i][0] == -1 && r.rspark[i][199] == -1, "reset: rspark ring seeded -1");
    CHECK(r.sspark[i][0][0] == -1 && r.sspark[i][19][29] == -1, "reset: sspark ring seeded -1");
    CHECK(r.ry[i][0][0] == -1 && r.rx[i][3][6] == -1 && r.rz[i][2][3] == -1, "reset: damage rings seeded -1");
  }
  // prepit starts false (record_init zeroes it), so reset() should NOT
  // have captured starcar this first call -- Record.java:200's own guard.
  CHECK(r.starcar[0].npl == 0, "reset: prepit==false skips starcar capture on a fresh Record");

  rig_free(&rig);
}

// Drives record_rec() for car 0 (== im) across enough ticks to fill the
// 300-tick ring with a recognisable ramp (tick k writes x=1000+k), then
// checks the ring holds exactly that ramp with the newest tick at index
// 299 and the oldest still-live value at index 0 -- confirms the O(n)
// downward-shift translation (Record.java:337-346) didn't get an
// off-by-one or a direction flip.
static void test_rec_position_ring(void) {
  RecordTestRig rig;
  if (!rig_init(&rig)) { CHECK(false, "rec_ring: rig_init"); return; }
  Record r;
  record_init(&r);
  ContO *carPtrs[8];
  for (int32_t i = 0; i < 8; i++) carPtrs[i] = &rig.cars[i];
  record_reset(&r, carPtrs);

  for (int32_t k = 0; k < 310; k++) {
    rig.cars[0].x = 1000 + k;
    rig.cars[0].xz = k % 360;
    record_rec(&r, &rig.cars[0], 0, 0, 0, 0, 0);
  }
  // After 310 ticks the ring (300 deep) holds ticks 10..309: oldest (index
  // 0) is tick 10 -> x=1010, newest (index 299) is tick 309 -> x=1309.
  CHECK(r.x[299][0] == 1309, "rec: newest tick lands at ring index 299");
  CHECK(r.x[0][0] == 1010, "rec: oldest surviving tick sits at ring index 0");
  CHECK(r.x[150][0] == 1160, "rec: mid-ring tick is contiguous with the ramp");

  rig_free(&rig);
}

// checkpoint/lastcheck only update when n==im (Record.java:355-362) --
// verified by recording car 1 (!= im 0) and confirming the checkpoint
// ring stays at its reset default while car 0's own rec() calls DO
// advance it.
static void test_rec_checkpoint_only_for_im(void) {
  RecordTestRig rig;
  if (!rig_init(&rig)) { CHECK(false, "rec_checkpoint: rig_init"); return; }
  Record r;
  record_init(&r);
  ContO *carPtrs[8];
  for (int32_t i = 0; i < 8; i++) carPtrs[i] = &rig.cars[i];
  record_reset(&r, carPtrs);

  rig.m.checkpoint = 7;
  rig.m.lastcheck = true;
  record_rec(&r, &rig.cars[1], 1, 0, 0, 0, 0); // n=1, im=0 -- should NOT touch checkpoint ring
  CHECK(r.checkpoint[299] == 0, "rec: non-im car's tick doesn't touch the checkpoint ring");

  record_rec(&r, &rig.cars[0], 0, 0, 0, 0, 0); // n=0==im -- SHOULD touch it
  CHECK(r.checkpoint[299] == 7 && r.lastcheck[299] == true, "rec: im car's tick records checkpoint/lastcheck");

  rig_free(&rig);
}

// Drives car 0 (== im) to caught==300 purely via repeated record_rec()
// calls (Record.java:306-308's own `if (n==n5) ++caught`), matching how
// the real game reaches it -- not by poking `r.caught` directly -- then
// confirms cotchinow's deep copy actually landed: a value recorded only
// on the LAST tick before freezing must be visible in the h* twin.
static void test_caught_and_cotchinow_deep_copy(void) {
  RecordTestRig rig;
  if (!rig_init(&rig)) { CHECK(false, "cotchinow: rig_init"); return; }
  Record r;
  record_init(&r);
  ContO *carPtrs[8];
  for (int32_t i = 0; i < 8; i++) carPtrs[i] = &rig.cars[i];
  record_reset(&r, carPtrs);

  for (int32_t k = 0; k < 300; k++) {
    rig.cars[0].x = 5000 + k;
    record_rec(&r, &rig.cars[0], 0, 0, 0, 0, 0);
  }
  CHECK(r.caught == 300, "cotchinow: caught reaches exactly 300 after 300 im-car ticks");
  CHECK(!r.hcaught, "cotchinow: not yet frozen -- record_rec's own dest==230 trigger never fired here");

  // Call it directly now that caught>=300, matching what dest crossing
  // 230 would have done (Record.java:238's own >=300 gate, exercised
  // here without needing to also simulate a real 230-tick destruction
  // countdown just to reach the same gate).
  record_cotchinow(&r, 0);
  CHECK(r.hcaught, "cotchinow: hcaught set once caught>=300");
  CHECK(r.hx[299][0] == r.x[299][0], "cotchinow: hx deep-copies the live ring's newest tick");
  CHECK(r.hx[0][0] == r.x[0][0], "cotchinow: hx deep-copies the live ring's oldest tick");
  CHECK(r.hx[299][0] == 5299, "cotchinow: the frozen value is the real recorded one, not a stale default");

  rig_free(&rig);
}

// record_playh() must reconstruct the exact pose cotchinow() froze,
// independent of whatever the LIVE ring does afterward -- proves the
// replay reads the snapshot, not the live (still-mutating) ring.
static void test_playh_reads_frozen_snapshot(void) {
  RecordTestRig rig;
  if (!rig_init(&rig)) { CHECK(false, "playh: rig_init"); return; }
  Record r;
  record_init(&r);
  ContO *carPtrs[8];
  for (int32_t i = 0; i < 8; i++) carPtrs[i] = &rig.cars[i];
  record_reset(&r, carPtrs);

  for (int32_t k = 0; k < 300; k++) {
    rig.cars[0].x = 9000 + k;
    rig.cars[0].xz = 42;
    record_rec(&r, &rig.cars[0], 0, 0, 0, 0, 0);
  }
  record_cotchinow(&r, 0);
  CHECK(r.hcaught, "playh: snapshot frozen before readback");

  // Mutate the LIVE ring further -- playh must not see this.
  for (int32_t k = 0; k < 5; k++) {
    rig.cars[0].x = -1;
    record_rec(&r, &rig.cars[0], 0, 0, 0, 0, 0);
  }

  ContO replayCar;
  cont_o_init_copy(&replayCar, &rig.base, 0, 0, 0, 0);
  record_playh(&r, &replayCar, &rig.mad[0], 0, 299, 0);
  CHECK(replayCar.x == 9299, "playh: reconstructs the frozen tick 299 pose, unaffected by later live recs");
  CHECK(replayCar.xz == 42, "playh: reconstructs xz too");

  record_playh(&r, &replayCar, &rig.mad[0], 0, 0, 0);
  CHECK(replayCar.x == 9000, "playh: reconstructs the frozen tick 0 (oldest) pose");

  cont_o_free(&replayCar);
  rig_free(&rig);
}

int main(void) {
  Record r;
  record_init(&r);
  record_recy(&r, 2, 45.7, true, 3);
  record_recy(&r, 2, 12.2, false, 3);
  record_recx(&r, 1, 88.9, 3);
  record_recz(&r, 0, 5.5, 3);

  CHECK(r.ry[3][2][0] == 300 && r.ry[3][2][1] == 300 && r.ry[3][2][2] == 0, "ry[3][2]");
  CHECK(r.magy[3][2][0] == 45 && r.magy[3][2][1] == 12, "magy[3][2]");
  CHECK(r.mtouch[3][0] == true && r.mtouch[3][1] == false, "mtouch[3]");
  CHECK(r.nry[3][2] == 2, "nry[3][2]");
  CHECK(r.rx[3][1][0] == 300, "rx[3][1][0] (indexed via nry[3][1]==0)");
  CHECK(r.magx[3][1][0] == 88, "magx[3][1][0]");
  CHECK(r.nrx[3][1] == 1, "nrx[3][1]");
  CHECK(r.rz[3][0][0] == 300, "rz[3][0][0] (indexed via nry[3][0]==0)");
  CHECK(r.nrz[3][0] == 1, "nrz[3][0]");

  test_reset();
  test_rec_position_ring();
  test_rec_checkpoint_only_for_im();
  test_caught_and_cotchinow_deep_copy();
  test_playh_reads_frozen_snapshot();

  if (failures == 0) {
    printf("all tests passed\n");
    return 0;
  }
  fprintf(stderr, "%d failure(s)\n", failures);
  return 1;
}
