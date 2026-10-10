// Host-buildable test for native/core/mad.c: mad_reseto plus an initial
// oracle-verified mad_drive() regression scenario (see drive_scenario()
// below and mad.c's own verification-status comment for exactly how much
// of drive() this covers vs. still needs scenario testing).
//
// Oracle: the real web/Mad.js's reseto(), run against a real parsed car
// (mycars/Simple_Car.rad) and CheckPoints, across all nfix values 0-5 and
// both im===xt.im/im!==xt.im branches (12 scenarios). All fields matched
// exactly on the first attempt, including `forca`'s double/case-1/case-2
// mixed fr() classification (four fr(sqrt(...)) terms summed unwrapped,
// then THAT sum's /10000.0 wrapped in one fr(), times a separately
// fr()-wrapped bounce term).
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

static void scenario(int32_t nfix, bool imEqualsXtIm, int32_t expectFixes) {
  nfm_set_seed(9001);
  Medium m;
  medium_init(&m);
  Trackers t;
  trackers_init(&t);
  CarDefine cd;
  car_define_init(&cd);
  Record rpd;
  record_init(&rpd);
  CheckPoints cp;
  check_points_init(&cp);
  cp.pcs = 7;
  cp.nfix = nfix;
  XtGraphicsStub xt;
  xt_graphics_stub_init(&xt);
  xt.im = 2;

  vfs_set_fpath("../../../");
  char *text = vfs_read_text("mycars/Simple_Car.rad");
  CHECK(text != NULL, "mycars/Simple_Car.rad read (mad_test)");
  if (!text) {
    medium_free(&m);
    return;
  }
  m.loadnew = true;
  ContO contO;
  cont_o_init_buf(&contO, text, &m, &t);
  m.loadnew = false;
  free(text);

  int32_t im = imEqualsXtIm ? xt.im : 5;
  Mad mad;
  mad_init(&mad, &cd, &m, &rpd, &xt, im);
  mad_reseto(&mad, 3, &contO, &cp);

  char label[64];
  snprintf(label, sizeof(label), "nfix=%d imEq=%d cn/mxz/pcleared/power", nfix, imEqualsXtIm);
  CHECK(mad.cn == 3 && mad.mxz == 0 && mad.pcleared == 7 && mad.power == 98.0f, label);
  snprintf(label, sizeof(label), "nfix=%d imEq=%d forca", nfix, imEqualsXtIm);
  CHECK(mad.forca == 0.037368882f, label);
  snprintf(label, sizeof(label), "nfix=%d imEq=%d fixes", nfix, imEqualsXtIm);
  CHECK(mad.fixes == expectFixes, label);
  snprintf(label, sizeof(label), "nfix=%d imEq=%d checkpoint/dested", nfix, imEqualsXtIm);
  CHECK(m.checkpoint == -1 && !m.lastcheck && cp.dested[im] == 0, label);

  cont_o_free(&contO);
  medium_free(&m);
}

// Oracle-verified drive() regression scenario: a fresh car (Simple_Car.rad)
// dropped in free space above an empty (trackless, single far-off
// checkpoint) world, coasting vs. holding up-throttle, for 10 ticks each.
// Exercises: the !mtouch/airborne position-update path, the mtouch
// transition (direct array3>245 ground-touch branch, no track geometry
// involved), the wheel-touch skid/speed-clamp physics, and forward-speed
// integration into contO.z once grounded. Does NOT exercise steering,
// braking-curve edge cases, wall/track-plane collision, capsize, loop
// tricks, or checkpoint-clearing -- see mad.c's own verification-status
// comment for what's still unverified. Regenerate via
// /tmp/*/scratchpad/mad_drive_oracle.mjs (web/Mad.js run under Node) if
// this needs re-deriving.
// Oracle-verified GROUNDED-HANDBRAKE scenario. The pre-existing handbrake
// coverage in this file only exercised the AIRBORNE case (the loop-trick
// entry at ~line 613); holding the handbrake while on the ground at speed
// -- lines 794/831/854-856/993/1083 of mad.c -- had none, despite being
// the exact path Control's attack mode drives through: turntyp==2 sets
// `handb = true` with the throttle still held, so a bot attacking another
// car performs a handbrake slide into it rather than a straight ram.
//
// The car spins hard and sustains speed under this input (xz 45 -> -484
// over 80 ticks while speed holds near 100), so any drift in the slide
// maths would show up immediately here.
//
// Oracle: web/Mad.js driven through the identical rig (same formula7.rad,
// same synthetic CarDefine fills, same placement, same tracker bounds and
// checkpoints) under Node -- regenerate with scratchpad/handb_oracle.mjs,
// same convention as this file's other scenarios. Every value below was
// produced by that run and matched by this code exactly, floats included.
static void handb_grounded_scenario(void) {
  nfm_set_seed(9001);
  Medium m; medium_init(&m);
  Trackers t; trackers_init(&t);
  trackers_devidetrackers(&t, -10000, 40000, -10000, 40000);
  CarDefine cd; car_define_init(&cd);
  for (int i = 0; i < CAR_DEFINE_NUM_CARS; i++) {
    cd.bounce[i]=1.2f; cd.flipy[i]=100; cd.airs[i]=1.0f; cd.airc[i]=1;
    cd.swits[i][0]=50; cd.swits[i][1]=100; cd.swits[i][2]=150;
    cd.acelf[i][0]=10; cd.acelf[i][1]=10; cd.acelf[i][2]=10;
    cd.handb[i]=5; cd.turn[i]=4; cd.simag[i]=1.0f; cd.grip[i]=20.0f;
    cd.powerloss[i]=100000; cd.maxmag[i]=1000; cd.msquash[i]=100;
    cd.clrad[i]=500; cd.dammult[i]=1.0f; cd.moment[i]=1.0f;
    cd.push[i]=100; cd.revpush[i]=100; cd.revlift[i]=10; cd.lift[i]=10;
    cd.comprad[i]=300.0f;
  }
  Record rpd; record_init(&rpd);
  CheckPoints cp; check_points_init(&cp);
  cp.pcs=0; cp.nfix=0; cp.n=4;
  cp.typ[0]=cp.typ[1]=cp.typ[2]=cp.typ[3]=1;
  cp.x[0]=0; cp.x[1]=5000; cp.x[2]=5000; cp.x[3]=0;
  cp.y[0]=cp.y[1]=cp.y[2]=cp.y[3]=0;
  cp.z[0]=0; cp.z[1]=0; cp.z[2]=5000; cp.z[3]=5000;
  cp.nsp=4; cp.nlaps=1; cp.stage=1; cp.fn=0;

  vfs_set_fpath("../../../");
  VfsZip zip;
  // A missing asset must FAIL, not silently skip: an early `return` here
  // would leave the scenario vacuously green in any environment whose
  // working directory makes vfs_set_fpath's relative path miss.
  CHECK(vfs_read_zip("data/models.zip", &zip), "handb: data/models.zip readable");
  if (!zip.count) { medium_free(&m); trackers_free_sect(&t); return; }
  char *text = NULL;
  for (int32_t i = 0; i < zip.count; i++) {
    if (strcmp(zip.entries[i].name, "formula7.rad") == 0) { text = vfs_entry_text(&zip.entries[i]); break; }
  }
  CHECK(text != NULL, "handb: formula7.rad present in models.zip");
  if (!text) { vfs_free_zip(&zip); medium_free(&m); trackers_free_sect(&t); return; }
  m.loadnew = true;
  ContO base; cont_o_init_buf(&base, text, &m, &t);
  m.loadnew = false; free(text);
  base.shadow = true;
  ContO contO; cont_o_init_copy(&contO, &base, 1000, 200, -5000, 0);
  contO.keyx[0]=-100; contO.keyx[1]=100; contO.keyx[2]=100; contO.keyx[3]=-100;
  contO.keyz[0]=200;  contO.keyz[1]=200; contO.keyz[2]=-200; contO.keyz[3]=-200;
  contO.grat=0; contO.x=1000; contO.y=200; contO.z=-5000;
  contO.xz=45; contO.zy=0; contO.xy=0;

  XtGraphicsStub xt; xt_graphics_stub_init(&xt); xt.im = 0;
  Control control; control_init(&control, &m); control_falseo(&control, 0);
  Mad mad; mad_init(&mad, &cd, &m, &rpd, &xt, 0);
  mad_reseto(&mad, 0, &contO, &cp);

  // Phase 1 -- 40 ticks of plain acceleration, to reach a speed where the
  // handbrake actually breaks traction rather than just slowing the car.
  control.up = true; control.right = false; control.handb = false;
  for (int32_t k = 1; k <= 40; k++) mad_drive(&mad, &control, &contO, &t, &cp);
  CHECK(mad.speed == 111.653061f && contO.xz == 45, "handb: spin-up matches oracle");

  // Phase 2 -- the attack manoeuvre itself: handbrake + steer, throttle held.
  struct { int32_t tick, x, z, xz; float speed; } expected[] = {
    {  45, -1735, -2242,   35, 102.158432f },
    {  50, -2031, -1823,    6, 104.603172f },
    {  60, -2546,  -938,  -64, 102.203865f },
    {  80, -3278,   563, -204, 104.646362f },
    { 100, -4066,  1695, -344,  99.635475f },
    { 120, -4796,  2935, -484, 102.203865f },
  };
  int32_t ei = 0;
  control.up = true; control.right = true; control.handb = true;
  for (int32_t k = 41; k <= 120; k++) {
    mad_drive(&mad, &control, &contO, &t, &cp);
    if (ei < (int32_t)(sizeof(expected)/sizeof(expected[0])) && expected[ei].tick == k) {
      char lbl[96];
      snprintf(lbl, sizeof(lbl), "handb grounded tick %d matches Mad.js oracle", k);
      CHECK(contO.x == expected[ei].x && contO.z == expected[ei].z &&
            contO.xz == expected[ei].xz && mad.speed == expected[ei].speed, lbl);
      ei++;
    }
  }

  cont_o_free(&contO);
  cont_o_free(&base);
  vfs_free_zip(&zip);
  medium_free(&m);
  trackers_free_sect(&t);
}

static void drive_scenario(int32_t nticks, bool upPressed, const char *label) {
  nfm_set_seed(9001);
  Medium m;
  medium_init(&m);
  Trackers t;
  trackers_init(&t);
  trackers_devidetrackers(&t, -10000, 20000, -10000, 20000);
  CarDefine cd;
  car_define_init(&cd);
  Record rpd;
  record_init(&rpd);
  CheckPoints cp;
  check_points_init(&cp);
  cp.pcs = 0;
  cp.n = 1;
  cp.typ[0] = 1;
  cp.x[0] = 0;
  cp.y[0] = 0;
  cp.z[0] = 100000;
  cp.fn = 0;
  cp.nsp = 1;
  cp.nlaps = 1;

  vfs_set_fpath("../../../");
  char *text = vfs_read_text("mycars/Simple_Car.rad");
  CHECK(text != NULL, "mycars/Simple_Car.rad read (mad_test drive)");
  if (!text) {
    medium_free(&m);
    trackers_free_sect(&t);
    return;
  }
  m.loadnew = true;
  ContO baseModel;
  cont_o_init_buf(&baseModel, text, &m, &t);
  m.loadnew = false;
  free(text);
  // Real gameplay never drives a raw #initBuf object directly -- only an
  // #initCopy clone gets the shadow/dust particle arrays (sx/sy/sz/
  // osmag/scx/scz) that #initBuf itself never allocates even with
  // shadow=true (see cont_o.h's #initCopy doc comment). Driving the raw
  // base model instead would only "work" by accident for inputs that
  // never trigger a dust() call with nonzero lateral speed -- this
  // scenario now matches how GameSparker.js's own tick actually drives a
  // car (a fresh #initCopy clone each "newcar" reset).
  baseModel.shadow = true;
  ContO contO;
  cont_o_init_copy(&contO, &baseModel, 0, 0, 0, 0);

  XtGraphicsStub xt;
  xt_graphics_stub_init(&xt);
  xt.im = 2;

  Control control;
  control_init(&control, &m);
  control_falseo(&control, 0);
  control.up = upPressed;

  Mad mad;
  mad_init(&mad, &cd, &m, &rpd, &xt, 2);
  mad_reseto(&mad, 3, &contO, &cp);

  for (int32_t i = 0; i < nticks; i++) {
    mad_drive(&mad, &control, &contO, &t, &cp);
    if (i == 0) {
      char lbl[64];
      snprintf(lbl, sizeof(lbl), "%s tick0", label);
      bool ok = contO.y == 7 && contO.z == 0 && mad.scy[0] == 7.0f && !mad.mtouch && !mad.wtouch;
      if (upPressed) ok = ok && mad.speed == 11.0f && contO.wzy == -1;
      else ok = ok && mad.speed == 0.0f && contO.wzy == 0;
      CHECK(ok, lbl);
    }
    if (i == 5) {
      char lbl[64];
      snprintf(lbl, sizeof(lbl), "%s tick5", label);
      bool ok = contO.y == 147 && contO.z == 0 && mad.scy[0] == 42.0f && !mad.mtouch && !mad.wtouch;
      if (upPressed) ok = ok && mad.speed == 66.0f && contO.wzy == -30;
      else ok = ok && mad.speed == 0.0f && contO.wzy == 0;
      CHECK(ok, lbl);
    }
    if (i == 6) {
      char lbl[64];
      snprintf(lbl, sizeof(lbl), "%s tick6 (ground touch)", label);
      bool ok = contO.y == 182 && mad.scy[0] == -7.349998474121094f && mad.mtouch && mad.wtouch && !mad.capsized;
      if (upPressed) ok = ok && mad.speed == 77.0f && contO.wzy == -11;
      else ok = ok && mad.speed == 0.0f && contO.wzy == 0;
      CHECK(ok, lbl);
    }
    if (i == 9) {
      char lbl[64];
      snprintf(lbl, sizeof(lbl), "%s tick9 (settled)", label);
      bool ok = contO.y == 182 && mad.scy[0] == 0.0f && mad.mtouch && mad.wtouch;
      if (upPressed) ok = ok && contO.z == 43 && mad.speed == 37.5f && mad.scz[0] == 38.0f && contO.wzy == -30;
      else ok = ok && contO.z == 0 && mad.speed == 0.0f && mad.scz[0] == 0.0f && contO.wzy == 0;
      CHECK(ok, lbl);
    }
  }

  cont_o_free(&contO);
  cont_o_free(&baseModel);
  medium_free(&m);
  trackers_free_sect(&t);
}

// Second oracle-verified drive() regression scenario: same car, this time
// dropped onto a single synthetic flat-ground Trackers plane (y=200,
// wide enough to cover the whole run) with up-throttle and (in the
// second variant) left-steering held -- exercises the ACTUAL
// trackers.sect collision loop's flat-ground branch (distinct from the
// free-fall scenario above, which only ever hits the direct
// array3>245 suspension-travel-limit shortcut, never real track
// geometry), plus steering (contO.wxz/xz/tilt) and the skid/dust path
// once lateral speed is nonzero.
static void drive_track_scenario(int32_t nticks, bool leftPressed, const char *label) {
  nfm_set_seed(9001);
  Medium m;
  medium_init(&m);
  Trackers t;
  trackers_init(&t);
  t.x[0] = 0; t.y[0] = 200; t.z[0] = 0;
  t.xy[0] = 0; t.zy[0] = 0;
  t.radx[0] = 5000; t.rady[0] = 300; t.radz[0] = 5000;
  t.skd[0] = 0; t.dam[0] = 1;
  t.notwall[0] = true; t.decor[0] = false;
  t.nt = 1;
  trackers_devidetrackers(&t, -10000, 20000, -10000, 20000);

  CarDefine cd;
  car_define_init(&cd);
  Record rpd;
  record_init(&rpd);
  CheckPoints cp;
  check_points_init(&cp);
  cp.pcs = 0;
  cp.n = 1;
  cp.typ[0] = 1;
  cp.x[0] = 0;
  cp.y[0] = 0;
  cp.z[0] = 1000000;
  cp.fn = 0;
  cp.nsp = 1;
  cp.nlaps = 1;

  vfs_set_fpath("../../../");
  char *text = vfs_read_text("mycars/Simple_Car.rad");
  CHECK(text != NULL, "mycars/Simple_Car.rad read (mad_test track)");
  if (!text) {
    medium_free(&m);
    trackers_free_sect(&t);
    return;
  }
  m.loadnew = true;
  ContO baseModel;
  cont_o_init_buf(&baseModel, text, &m, &t);
  m.loadnew = false;
  free(text);
  baseModel.shadow = true;
  ContO contO;
  cont_o_init_copy(&contO, &baseModel, 0, 0, 0, 0);

  XtGraphicsStub xt;
  xt_graphics_stub_init(&xt);
  xt.im = 2;

  Control control;
  control_init(&control, &m);
  control_falseo(&control, 0);
  control.up = true;
  control.left = leftPressed;

  Mad mad;
  mad_init(&mad, &cd, &m, &rpd, &xt, 2);
  mad_reseto(&mad, 3, &contO, &cp);

  for (int32_t i = 0; i < nticks; i++) {
    mad_drive(&mad, &control, &contO, &t, &cp);
  }

  char lbl[64];
  snprintf(lbl, sizeof(lbl), "%s final", label);
  bool ok;
  if (!leftPressed) {
    // ground_up15, tick 14 (0-indexed): oracle-derived exact state.
    ok = contO.x == 0 && contO.y == 132 && contO.z == 467 && contO.xz == 0 && contO.xy == 0 && contO.zy == 0 &&
         contO.wxz == 0 && contO.wzy == -25 && mad.speed == 96.0f && mad.mtouch && mad.wtouch && !mad.capsized &&
         mad.skid == 0 && mad.pxy == 0 && mad.pzy == 0 && mad.tilt == 0.0f && mad.mxz == 0 &&
         mad.scz[0] == 96.0f && mad.scx[0] == 0.0f && mad.scy[0] == 0.0f;
  } else {
    // ground_upleft20, tick 19 (0-indexed): oracle-derived exact state.
    ok = contO.x == -740 && contO.y == 129 && contO.z == 437 && contO.xz == 98 && contO.xy == 3 && contO.zy == 0 &&
         contO.wxz == 36 && contO.wzy == -5 && mad.speed == 111.74698638916016f && mad.mtouch && mad.wtouch &&
         !mad.capsized && mad.skid == 1 && mad.pxy == 0 && mad.pzy == 0 && mad.tilt == 3.0500004291534424f &&
         mad.mxz == 97 && mad.scx[0] == -111.0f && mad.scy[0] == 0.0f && mad.scz[0] == -15.0f;
  }
  CHECK(ok, lbl);

  cont_o_free(&contO);
  cont_o_free(&baseModel);
  medium_free(&m);
  trackers_free_sect(&t);
}

// Checkpoint-clearing regression scenario: same flat-ground rig as
// drive_track_scenario(), but this time with a REAL, reachable checkpoint
// (typ=1, a "forward"/z-facing gate) placed a short drive ahead, instead
// of every other scenario's placeholder checkpoint 100000+ units away
// (unreachable by design, just there so drive()'s focus-search loop has
// something to iterate without spinning on "zero checkpoints"). Exercises
// the actual clear-detection branch (the `abs(contO->z - checkPoints->z)
// < threshold && ... && mad->clear == n111 + nlaps*nsp - 1` test at
// mad.c's checkpoint loop) and everything downstream of a real clear:
// `mad->clear`/`mad->pcleared`/`mad->focus` getting set, and (since this
// track has only ONE checkpoint, so clearing it always also completes a
// lap -- `n111` maxes out at 1 the same tick) `mad->nlaps` incrementing
// and `m->checkpoint`/`m->lastcheck` updating too.
//
// Oracle: the real web/Mad.js run against the same rig with a single
// typ=1 checkpoint at (x=0, y=182, z=400) -- y picked with a wide margin
// inside the `abs(contO.y - checkPoints.y + 350) < 450` test, not an
// attempt to match this rig's actual ~132 settled ride height (the
// margin covers that gap comfortably). First clear happens at tick 12
// (car at z=291, still short of the checkpoint's own z=400 -- the
// threshold is `60 + abs(scz-sum)/4`, wide enough to trigger noticeably
// before/after the checkpoint's exact position, not a bug). NOTE: the
// oracle shows this specific single-checkpoint setup keeps RE-clearing
// (and re-lapping) on tick 13 and 14 too, while the car is still inside
// the threshold window and `mad->clear == n111 + nlaps*nsp - 1` keeps
// re-satisfying as nlaps climbs in lockstep -- a genuine, JS-confirmed
// emergent quirk of a single-checkpoint "lap", not a defect in this
// scenario or the port; pinning down tick 12 (the FIRST clear) is enough
// to prove the clear-detection branch itself works.
static void drive_checkpoint_scenario(void) {
  nfm_set_seed(9001);
  Medium m;
  medium_init(&m);
  Trackers t;
  trackers_init(&t);
  t.x[0] = 0; t.y[0] = 200; t.z[0] = 0;
  t.xy[0] = 0; t.zy[0] = 0;
  t.radx[0] = 5000; t.rady[0] = 300; t.radz[0] = 5000;
  t.skd[0] = 0; t.dam[0] = 1;
  t.notwall[0] = true; t.decor[0] = false;
  t.nt = 1;
  trackers_devidetrackers(&t, -10000, 20000, -10000, 20000);

  CarDefine cd;
  car_define_init(&cd);
  Record rpd;
  record_init(&rpd);
  CheckPoints cp;
  check_points_init(&cp);
  cp.pcs = 0;
  cp.n = 1;
  cp.typ[0] = 1;
  cp.x[0] = 0;
  cp.y[0] = 182;
  cp.z[0] = 400;
  cp.fn = 0;
  cp.nsp = 1;
  cp.nlaps = 1;

  vfs_set_fpath("../../../");
  char *text = vfs_read_text("mycars/Simple_Car.rad");
  CHECK(text != NULL, "mycars/Simple_Car.rad read (mad_test checkpoint)");
  if (!text) {
    medium_free(&m);
    trackers_free_sect(&t);
    return;
  }
  m.loadnew = true;
  ContO baseModel;
  cont_o_init_buf(&baseModel, text, &m, &t);
  m.loadnew = false;
  free(text);
  baseModel.shadow = true;
  ContO contO;
  cont_o_init_copy(&contO, &baseModel, 0, 0, 0, 0);

  XtGraphicsStub xt;
  xt_graphics_stub_init(&xt);
  xt.im = 2;

  Control control;
  control_init(&control, &m);
  control_falseo(&control, 0);
  control.up = true;

  Mad mad;
  mad_init(&mad, &cd, &m, &rpd, &xt, 2);
  mad_reseto(&mad, 3, &contO, &cp);

  for (int32_t i = 0; i <= 12; i++) {
    mad_drive(&mad, &control, &contO, &t, &cp);
  }
  bool ok = contO.x == 0 && contO.y == 132 && contO.z == 291 && contO.xz == 0 && contO.xy == 0 &&
            contO.zy == 0 && contO.wxz == 0 && contO.wzy == -29 && mad.speed == 85.0f &&
            mad.mtouch && mad.wtouch && !mad.capsized && mad.skid == 0 &&
            mad.scz[0] == 85.0f && mad.scx[0] == 0.0f && mad.scy[0] == 0.0f &&
            mad.clear == 1 && mad.nlaps == 1 && mad.focus == -1 && mad.pcleared == 0 &&
            m.checkpoint == 0 && m.lastcheck;
  CHECK(ok, "checkpoint tick12 (first real checkpoint clear + lap complete)");

  cont_o_free(&contO);
  cont_o_free(&baseModel);
  medium_free(&m);
  trackers_free_sect(&t);
}

// Sloped-surface regression scenario: same free-fall-then-land rig as
// drive_track_scenario(), but the single Trackers plane's `zy` is 30
// instead of 0/90/-90 -- a genuine RAMP, not flat ground or a vertical
// wall. Exercises the `trackers->zy[n53] != 0 && != 90 && != -90` branch
// (mad.c ~1365-1397) that no other scenario in this file reaches: the
// `mad_rot`-based rotation of a wheel corner's (y,z) into the ramp's own
// local frame, the resulting per-corner `scy` split between front
// wheels (corners 0/1, first to touch the ramp face) and back wheels
// (corners 2/3, still airborne a moment longer -- the ramp's 30-degree
// tilt means front and back corners cross the `n93v > z[n53]-30`
// contact threshold on different ticks), and `gtouch` staying false
// while `wtouch`/`mtouch` go true (the sloped branch never sets `gtouch`
// -- only the flat-ground `xy===0 && zy===0` branch does).
//
// Oracle: the real web/Mad.js run against the same rig (car cn=3,
// up-held from a small drop, same as drive_track_scenario) with the
// ramp in place. By tick 5 the car has climbed onto the ramp and all
// four wheels have made contact (mtouch/wtouch both true), with the
// front/back scy split from momentarily-uneven contact still visible;
// by tick 14 the ramp flattens out (trackers.zy=30's local frame puts
// the car back to contO.zy=0) and the car settles at the same y=182
// flat-ground ride height every other flat scenario in this file
// reaches, confirming the ramp hands off to level ground correctly
// rather than leaving some residual tilt behind.
static void drive_slope_scenario(void) {
  nfm_set_seed(9001);
  Medium m;
  medium_init(&m);
  Trackers t;
  trackers_init(&t);
  t.x[0] = 0; t.y[0] = 200; t.z[0] = 0;
  t.xy[0] = 0; t.zy[0] = 30;
  t.radx[0] = 5000; t.rady[0] = 300; t.radz[0] = 5000;
  t.skd[0] = 0; t.dam[0] = 1;
  t.notwall[0] = true; t.decor[0] = false;
  t.nt = 1;
  trackers_devidetrackers(&t, -10000, 20000, -10000, 20000);

  CarDefine cd;
  car_define_init(&cd);
  Record rpd;
  record_init(&rpd);
  CheckPoints cp;
  check_points_init(&cp);
  cp.pcs = 0;
  cp.n = 1;
  cp.typ[0] = 1;
  cp.x[0] = 0;
  cp.y[0] = 0;
  cp.z[0] = 1000000;
  cp.fn = 0;
  cp.nsp = 1;
  cp.nlaps = 1;

  vfs_set_fpath("../../../");
  char *text = vfs_read_text("mycars/Simple_Car.rad");
  CHECK(text != NULL, "mycars/Simple_Car.rad read (mad_test slope)");
  if (!text) {
    medium_free(&m);
    trackers_free_sect(&t);
    return;
  }
  m.loadnew = true;
  ContO baseModel;
  cont_o_init_buf(&baseModel, text, &m, &t);
  m.loadnew = false;
  free(text);
  baseModel.shadow = true;
  ContO contO;
  cont_o_init_copy(&contO, &baseModel, 0, 0, 0, 0);

  XtGraphicsStub xt;
  xt_graphics_stub_init(&xt);
  xt.im = 2;

  Control control;
  control_init(&control, &m);
  control_falseo(&control, 0);
  control.up = true;

  Mad mad;
  mad_init(&mad, &cd, &m, &rpd, &xt, 2);
  mad_reseto(&mad, 3, &contO, &cp);

  for (int32_t i = 0; i <= 5; i++) {
    mad_drive(&mad, &control, &contO, &t, &cp);
  }
  bool ok = contO.x == 0 && contO.y == 132 && contO.z == 27 && contO.xz == 0 && contO.xy == 0 &&
            contO.zy == -16 && contO.wxz == 0 && contO.wzy == -30 && mad.speed == 66.0f &&
            mad.mtouch && mad.wtouch && !mad.gtouch && !mad.capsized && mad.skid == 2 &&
            mad.pxy == 0 && mad.pzy == -18 && mad.tilt == 0.0f && mad.mxz == 0 &&
            mad.scz[0] == 0.0f && mad.scx[0] == 0.0f &&
            mad.scy[0] == 42.0f && mad.scy[1] == 42.0f &&
            mad.scy[2] == 23.975191116333008f && mad.scy[3] == 23.975191116333008f;
  CHECK(ok, "slope tick5 (ramp contact, front/back wheels split)");

  cont_o_free(&contO);
  cont_o_free(&baseModel);
  medium_free(&m);
  trackers_free_sect(&t);
}

// Multiple-trackers-in-one-sector regression scenario: same rig as
// drive_track_scenario()'s ground_up15 case (single flat ground plane,
// up-held, 15 ticks), but with a SECOND tracker added at the exact same
// position/size/shape as the first (index 1, a byte-for-byte duplicate
// of index 0). Both land in every sector cell the car's path touches
// (see Trackers.js's own devidetrackers() -- sect[][] lists tracker
// INDICES in ascending order, so tracker 0 is always checked before
// tracker 1 for a given cell). Exercises the `array7[wheel]` "claimed"
// gate (mad.c ~1176-1228) genuinely: unlike drive_wall_scenario's
// ground+wall pair (which occupy the same sector too, but satisfy
// DIFFERENT branches -- flat-ground vs. the zy===-90 wall check -- so
// that scenario never actually tests two trackers competing for the
// SAME branch), this scenario's two trackers are both eligible for the
// exact same flat-ground branch on the exact same tick for the exact
// same wheel corner. If the gate were broken, the duplicate would fire
// its own dust()/regy() a second time per wheel per tick -- an extra
// medium_random() draw per tick would desync the sim PRNG stream from
// the very first ground-touch tick onward (the same bug class the
// cont_o_dust draw-phase fix caught earlier in this file's history).
//
// Oracle: the real web/Mad.js run with `t.nt = 2`, tracker 1 an exact
// copy of tracker 0. Result matches drive_track_scenario's own
// ground_up15 (single-tracker) final assertion values EXACTLY, byte for
// byte -- proof the duplicate never contributes anything, not even an
// extra PRNG draw.
static void drive_multitracker_scenario(void) {
  nfm_set_seed(9001);
  Medium m;
  medium_init(&m);
  Trackers t;
  trackers_init(&t);
  t.x[0] = 0; t.y[0] = 200; t.z[0] = 0;
  t.xy[0] = 0; t.zy[0] = 0;
  t.radx[0] = 5000; t.rady[0] = 300; t.radz[0] = 5000;
  t.skd[0] = 0; t.dam[0] = 1;
  t.notwall[0] = true; t.decor[0] = false;
  // Tracker 1: byte-for-byte duplicate of tracker 0.
  t.x[1] = 0; t.y[1] = 200; t.z[1] = 0;
  t.xy[1] = 0; t.zy[1] = 0;
  t.radx[1] = 5000; t.rady[1] = 300; t.radz[1] = 5000;
  t.skd[1] = 0; t.dam[1] = 1;
  t.notwall[1] = true; t.decor[1] = false;
  t.nt = 2;
  trackers_devidetrackers(&t, -10000, 20000, -10000, 20000);

  CarDefine cd;
  car_define_init(&cd);
  Record rpd;
  record_init(&rpd);
  CheckPoints cp;
  check_points_init(&cp);
  cp.pcs = 0;
  cp.n = 1;
  cp.typ[0] = 1;
  cp.x[0] = 0;
  cp.y[0] = 0;
  cp.z[0] = 1000000;
  cp.fn = 0;
  cp.nsp = 1;
  cp.nlaps = 1;

  vfs_set_fpath("../../../");
  char *text = vfs_read_text("mycars/Simple_Car.rad");
  CHECK(text != NULL, "mycars/Simple_Car.rad read (mad_test multitracker)");
  if (!text) {
    medium_free(&m);
    trackers_free_sect(&t);
    return;
  }
  m.loadnew = true;
  ContO baseModel;
  cont_o_init_buf(&baseModel, text, &m, &t);
  m.loadnew = false;
  free(text);
  baseModel.shadow = true;
  ContO contO;
  cont_o_init_copy(&contO, &baseModel, 0, 0, 0, 0);

  XtGraphicsStub xt;
  xt_graphics_stub_init(&xt);
  xt.im = 2;

  Control control;
  control_init(&control, &m);
  control_falseo(&control, 0);
  control.up = true;

  Mad mad;
  mad_init(&mad, &cd, &m, &rpd, &xt, 2);
  mad_reseto(&mad, 3, &contO, &cp);

  for (int32_t i = 0; i < 15; i++) {
    mad_drive(&mad, &control, &contO, &t, &cp);
  }
  // Same exact expected values as drive_track_scenario's ground_up15
  // (single-tracker) case -- see that scenario's own comment.
  bool ok = contO.x == 0 && contO.y == 132 && contO.z == 467 && contO.xz == 0 && contO.xy == 0 && contO.zy == 0 &&
            contO.wxz == 0 && contO.wzy == -25 && mad.speed == 96.0f && mad.mtouch && mad.wtouch && !mad.capsized &&
            mad.skid == 0 && mad.pxy == 0 && mad.pzy == 0 && mad.tilt == 0.0f && mad.mxz == 0 &&
            mad.scz[0] == 96.0f && mad.scx[0] == 0.0f && mad.scy[0] == 0.0f;
  CHECK(ok, "multitracker tick14 (duplicate tracker matches single-tracker baseline exactly)");

  cont_o_free(&contO);
  cont_o_free(&baseModel);
  medium_free(&m);
  trackers_free_sect(&t);
}

// Capsize/loop-trick regression scenario: a fresh car in free space (no
// trackers, same free-fall rig as drive_scenario()), holding the
// handbrake WHILE AIRBORNE from tick 0 -- the loop-trick entry condition
// (mad.c ~681-699): `control->handb && !mad->wtouch && mad->loop === 0`
// sets `loop=1` the same tick, which immediately falls through to the
// `loop===1` block averaging `scy[0..3]` and advancing to `loop=2`. From
// there, holding `down` every tick keeps growing `mad->dcomp`, which
// feeds into `mad->pzy` (mad.c ~743): a genuine mid-air backflip. No
// other scenario in this file ever sets `mad->capsized`, exercises
// `loop` past 0, or touches `pzy`/`dcomp` at all. Exercises: the
// loop-trick ucomp/dcomp/lcomp/rcomp accumulation, `pzy` advancing past
// 90 degrees, and the capsize DETECTION itself (mad.c ~641-665, the
// `zyinv`/`n3` wrap-and-compare against `mad->pzy` from the END of the
// PREVIOUS tick, evaluated at the TOP of drive() before anything else
// runs) actually flipping `mad->capsized` to true.
//
// Oracle: the real web/Mad.js run against the same rig (handb+down held
// from tick 0). `capsized` flips true for the first time exactly at
// tick 8 (pzy=110, just past the 90-degree wrap threshold) -- pinned
// here. (The same run continues on to a full land-upside-down-then-
// right-back-up cycle by tick 56 -- not asserted, single-tick coverage
// of the capsize transition itself is the goal here.)
static void drive_capsize_scenario(void) {
  nfm_set_seed(9001);
  Medium m;
  medium_init(&m);
  Trackers t;
  trackers_init(&t);
  t.nt = 0;
  trackers_devidetrackers(&t, -10000, 20000, -10000, 20000);

  CarDefine cd;
  car_define_init(&cd);
  Record rpd;
  record_init(&rpd);
  CheckPoints cp;
  check_points_init(&cp);
  cp.pcs = 0;
  cp.n = 1;
  cp.typ[0] = 1;
  cp.x[0] = 0;
  cp.y[0] = 0;
  cp.z[0] = 1000000;
  cp.fn = 0;
  cp.nsp = 1;
  cp.nlaps = 1;

  vfs_set_fpath("../../../");
  char *text = vfs_read_text("mycars/Simple_Car.rad");
  CHECK(text != NULL, "mycars/Simple_Car.rad read (mad_test capsize)");
  if (!text) {
    medium_free(&m);
    trackers_free_sect(&t);
    return;
  }
  m.loadnew = true;
  ContO baseModel;
  cont_o_init_buf(&baseModel, text, &m, &t);
  m.loadnew = false;
  free(text);
  baseModel.shadow = true;
  ContO contO;
  cont_o_init_copy(&contO, &baseModel, 0, 0, 0, 0);

  XtGraphicsStub xt;
  xt_graphics_stub_init(&xt);
  xt.im = 2;

  Control control;
  control_init(&control, &m);
  control_falseo(&control, 0);
  control.handb = true;
  control.down = true;

  Mad mad;
  mad_init(&mad, &cd, &m, &rpd, &xt, 2);
  mad_reseto(&mad, 3, &contO, &cp);

  for (int32_t i = 0; i <= 8; i++) {
    mad_drive(&mad, &control, &contO, &t, &cp);
  }
  bool ok = contO.x == 0 && contO.y == -65 && contO.z == -8 && contO.xz == 0 && contO.xy == 0 &&
            contO.zy == 100 && contO.wxz == 0 && contO.wzy == 0 && mad.speed == 0.0f &&
            !mad.mtouch && !mad.wtouch && mad.capsized && mad.skid == 2 && mad.loop == 2 &&
            mad.pxy == 0 && mad.pzy == 110 && mad.dcomp == 14.5f && mad.ucomp == 0.0f &&
            mad.lcomp == 0.0f && mad.rcomp == 0.0f && mad.tilt == 0.0f && mad.mxz == 0 &&
            mad.scz[0] == 0.0f && mad.scx[0] == 0.0f && mad.scy[0] == 63.0f;
  CHECK(ok, "capsize tick8 (mid-air backflip past 90 degrees flips mad->capsized)");

  cont_o_free(&contO);
  cont_o_free(&baseModel);
  medium_free(&m);
  trackers_free_sect(&t);
}

// Damage/repair regression scenario: exercises the repair-completion
// branch in mad.c (~1754-1758, `contO->fcnt === 7 || 8`) that was
// PERMANENTLY DEAD CODE until this session -- `cont_o_step_fix()` (the
// port of ContO.js's own `stepFix()`, a real SIMULATION function despite
// driving a visual repair-pit animation, see cont_o.h) had never been
// ported at all, so `contO->fcnt` could never advance past its initial
// 0 and the repair branch could never fire. Ported alongside this
// scenario and wired into native/platform/linux/main.c's tick loop
// (called once per tick per car, AFTER mad_drive(), matching
// GameSparker.js's own simulate() ordering).
//
// Rig: the flat-ground settle rig from drive_track_scenario() (so the
// car comes to rest at a known, STABLE (x,y,z) and just sits there with
// no throttle held), with a real fix zone (`checkPoints->fn=1`) placed
// exactly at that settled position, and `checkPoints->nfix=1` (->
// `mad->fixes=4` after reseto, see mad_reseto()). The car is hand-
// damaged right after reseto (squash/nbsq/hitmag/cntdest/dest all set
// nonzero/true, bypassing an actual collision sequence -- same approach
// as colide_scenario()'s hand-set speeds). Once settled and within the
// fix zone's proximity every tick, `contO->fix` goes true and
// `cont_o_step_fix()` climbs `fcnt` 1 per tick; mad_drive() reads
// `fcnt===7` on tick 7 and applies the repair in that same tick's call
// (clearing squash/nbsq/hitmag/cntdest/dest, setting `newcar`,
// decrementing `mad->fixes`, and setting `contO->fcnt=9` directly --
// which is why the repair fires only ONCE per cycle, not twice at both
// fcnt===7 and fcnt===8 as the condition's phrasing alone might suggest;
// `cont_o_step_fix()` then sees fcnt=9>7 immediately after and resets
// fcnt to 0 / fix to false the same tick).
//
// Oracle: the real web/Mad.js + web/ContO.js's stepFix(), run against
// the same rig. tick 6: still damaged, `fcnt` has just reached 7 (the
// LAST tick before repair fires). tick 7: fully repaired (squash=nbsq=
// hitmag=cntdest=0, dest=false, newcar=true, fixes 4->3, fix false,
// fcnt reset to 0).
static void drive_repair_scenario(void) {
  nfm_set_seed(9001);
  Medium m;
  medium_init(&m);
  Trackers t;
  trackers_init(&t);
  t.x[0] = 0; t.y[0] = 200; t.z[0] = 0;
  t.xy[0] = 0; t.zy[0] = 0;
  t.radx[0] = 5000; t.rady[0] = 300; t.radz[0] = 5000;
  t.skd[0] = 0; t.dam[0] = 1;
  t.notwall[0] = true; t.decor[0] = false;
  t.nt = 1;
  trackers_devidetrackers(&t, -10000, 20000, -10000, 20000);

  CarDefine cd;
  car_define_init(&cd);
  Record rpd;
  record_init(&rpd);
  CheckPoints cp;
  check_points_init(&cp);
  cp.pcs = 0;
  cp.n = 1;
  cp.typ[0] = 1;
  cp.x[0] = 0;
  cp.y[0] = 0;
  cp.z[0] = 1000000;
  cp.fn = 1;
  cp.fx[0] = 0;
  cp.fy[0] = 132;
  cp.fz[0] = 0;
  cp.roted[0] = false;
  cp.nsp = 1;
  cp.nlaps = 1;
  cp.nfix = 1;

  vfs_set_fpath("../../../");
  char *text = vfs_read_text("mycars/Simple_Car.rad");
  CHECK(text != NULL, "mycars/Simple_Car.rad read (mad_test repair)");
  if (!text) {
    medium_free(&m);
    trackers_free_sect(&t);
    return;
  }
  m.loadnew = true;
  ContO baseModel;
  cont_o_init_buf(&baseModel, text, &m, &t);
  m.loadnew = false;
  free(text);
  baseModel.shadow = true;
  ContO contO;
  cont_o_init_copy(&contO, &baseModel, 0, 0, 0, 0);

  XtGraphicsStub xt;
  xt_graphics_stub_init(&xt);
  xt.im = 2;

  Control control;
  control_init(&control, &m);
  control_falseo(&control, 0);

  Mad mad;
  mad_init(&mad, &cd, &m, &rpd, &xt, 2);
  mad_reseto(&mad, 3, &contO, &cp);
  CHECK(mad.fixes == 4, "repair pre: fixes==4 (nfix=1 -> fixes=4)");

  mad.squash = 10;
  mad.nbsq = 2;
  mad.hitmag = 50000;
  mad.cntdest = 3;
  mad.dest = true;

  for (int32_t i = 0; i <= 6; i++) {
    mad_drive(&mad, &control, &contO, &t, &cp);
    cont_o_step_fix(&contO);
  }
  bool ok6 = contO.x == 0 && contO.y == 132 && contO.z == 0 && contO.fix && contO.fcnt == 7 &&
             mad.squash == 10 && mad.nbsq == 2 && mad.hitmag == 50000 && mad.cntdest == 3 &&
             mad.dest && !mad.newcar && mad.fixes == 4 && rpd.fix[2] == 300;
  CHECK(ok6, "repair tick6 (fcnt just reached 7, still damaged)");

  mad_drive(&mad, &control, &contO, &t, &cp);
  cont_o_step_fix(&contO);
  bool ok7 = contO.x == 0 && contO.y == 132 && contO.z == 0 && !contO.fix && contO.fcnt == 0 &&
             mad.squash == 0 && mad.nbsq == 0 && mad.hitmag == 0 && mad.cntdest == 0 &&
             !mad.dest && mad.newcar && mad.fixes == 3;
  CHECK(ok7, "repair tick7 (repair applied: damage cleared, fixes decremented)");

  cont_o_free(&contO);
  cont_o_free(&baseModel);
  medium_free(&m);
  trackers_free_sect(&t);
}

// Car-vs-car collision regression scenario: no prior scenario in this
// file ever calls mad_colide() at all (drive() only exercises a single
// car against static Trackers geometry), so this is the first oracle
// verification of the whole function. Two #initCopy clones of the same
// base model, 150 units apart along z (well inside the maxR-based
// contact threshold -- see mad_colide's own `threshold` comment), with
// mad1 given nonzero forward speed/scz (so `myForce = power*speed*moment`
// is nonzero and clearly beats mad2's, making mad1 the DOMINATE car) and
// mad2 left at rest. Exercises: the caught/dominate bookkeeping, the
// corner-pair (j,k) rpy-distance-gated push loop (dominate-only, so only
// mad2 -- the non-dominant car -- gets pushed), and regx/regy/regz's
// return-value accumulation into contO2. Not exercised here: the
// cont_o_sprk spark-trigger coin-flip branches (mad2 starts with zero
// scx, so the `fabsf(scx*moment) > fabsf(...)` gate never opens on the
// X-axis half of the push, only Z) or xt.multion's dcrashes accounting
// (multion left at its 0 default).
//
// Oracle: the real web/Mad.js's colide(contO1, mad2, contO2) called
// directly (no drive() ticks -- mad1.speed/power/scz set by hand right
// after reseto, matching how this scenario constructs it) against the
// same two-ContO rig. mad1 (dominant) ends unchanged (mad2's scz/scx
// both start at zero, so the "push back" terms pushing on mad1 are all
// zero); mad2.scz becomes 300 at every corner (started 0, pushed by
// mad1's scz=200 corners) and mad2.scy becomes -40 at corners 0/1 but
// -80 at corners 2/3 -- confirmed NOT a mistake: corners 2/3 get hit by
// TWO of mad1's four corners within the rpy threshold (double
// accumulation), corners 0/1 only by one, a real consequence of the
// corner geometry at this particular offset, not a bug.
static void colide_scenario(void) {
  nfm_set_seed(9001);
  Medium m;
  medium_init(&m);
  Trackers t;
  trackers_init(&t);
  trackers_devidetrackers(&t, -10000, 20000, -10000, 20000);

  CarDefine cd;
  car_define_init(&cd);
  Record rpd;
  record_init(&rpd);
  CheckPoints cp;
  check_points_init(&cp);
  cp.pcs = 0;
  cp.n = 1;
  cp.typ[0] = 1;
  cp.x[0] = 0;
  cp.y[0] = 0;
  cp.z[0] = 1000000;
  cp.fn = 0;
  cp.nsp = 1;
  cp.nlaps = 1;

  vfs_set_fpath("../../../");
  char *text = vfs_read_text("mycars/Simple_Car.rad");
  CHECK(text != NULL, "mycars/Simple_Car.rad read (mad_test colide)");
  if (!text) {
    medium_free(&m);
    trackers_free_sect(&t);
    return;
  }
  m.loadnew = true;
  ContO baseModel;
  cont_o_init_buf(&baseModel, text, &m, &t);
  m.loadnew = false;
  free(text);
  baseModel.shadow = true;

  ContO contO1, contO2;
  cont_o_init_copy(&contO1, &baseModel, 0, 0, 0, 0);
  cont_o_init_copy(&contO2, &baseModel, 0, 0, 150, 0);

  XtGraphicsStub xt;
  xt_graphics_stub_init(&xt);
  xt.im = 0;

  Mad mad1, mad2;
  mad_init(&mad1, &cd, &m, &rpd, &xt, 0);
  mad_reseto(&mad1, 3, &contO1, &cp);
  mad_init(&mad2, &cd, &m, &rpd, &xt, 1);
  mad_reseto(&mad2, 3, &contO2, &cp);

  mad1.speed = 200.0f;
  mad1.power = 100.0f;
  mad1.scz[0] = 200.0f; mad1.scz[1] = 200.0f; mad1.scz[2] = 200.0f; mad1.scz[3] = 200.0f;
  mad1.scx[0] = 0.0f; mad1.scx[1] = 0.0f; mad1.scx[2] = 0.0f; mad1.scx[3] = 0.0f;

  mad2.speed = 0.0f;
  mad2.scz[0] = 0.0f; mad2.scz[1] = 0.0f; mad2.scz[2] = 0.0f; mad2.scz[3] = 0.0f;
  mad2.scx[0] = 0.0f; mad2.scx[1] = 0.0f; mad2.scx[2] = 0.0f; mad2.scx[3] = 0.0f;

  mad_colide(&mad1, &contO1, &mad2, &contO2);

  bool ok = mad1.dominate[1] && mad1.caught[1] &&
            mad1.scz[0] == 200.0f && mad1.scz[1] == 200.0f && mad1.scz[2] == 200.0f && mad1.scz[3] == 200.0f &&
            mad1.scx[0] == 0.0f && mad1.scx[1] == 0.0f && mad1.scx[2] == 0.0f && mad1.scx[3] == 0.0f &&
            mad2.scz[0] == 300.0f && mad2.scz[1] == 300.0f && mad2.scz[2] == 300.0f && mad2.scz[3] == 300.0f &&
            mad2.scx[0] == 0.0f && mad2.scx[1] == 0.0f && mad2.scx[2] == 0.0f && mad2.scx[3] == 0.0f &&
            mad2.scy[0] == -40.0f && mad2.scy[1] == -40.0f && mad2.scy[2] == -80.0f && mad2.scy[3] == -80.0f &&
            mad1.lastcolido == 0 && mad2.lastcolido == 70 && !mad2.colidim &&
            contO1.x == 0 && contO1.y == 0 && contO1.z == 0 &&
            contO2.x == 0 && contO2.y == 0 && contO2.z == 150;
  CHECK(ok, "colide dominant-push (mad1 dominates, mad2 gets pushed)");

  // The wheel speeds a hard hit leaves behind: drive() clamps each to within
  // 200 of the mean (142 -> {342, -58, 106, 106}) and moves the car by the
  // CLAMPED mean, 124 -- not the raw 142.5, which carried a hit car on into
  // the one that hit it.
  mad1.scz[0] = 486.0f; mad1.scz[1] = -128.0f; mad1.scz[2] = 106.0f; mad1.scz[3] = 106.0f;
  Control idle;
  control_init(&idle, &m);
  control_falseo(&idle, 0);
  mad_drive(&mad1, &idle, &contO1, &t, &cp);
  CHECK(contO1.z == 124, "drive moves a hit car by its clamped wheel speeds");

  cont_o_free(&contO1);
  cont_o_free(&contO2);
  cont_o_free(&baseModel);
  medium_free(&m);
  trackers_free_sect(&t);
}

// Gear-cap regression scenarios: same flat-ground rig as
// drive_track_scenario() (car cn=3, single wide ground plane), but run
// long enough that the throttle/brake gear-curve loop's TOP branch
// actually fires -- `n9 === 3` (forward top-gear speed cap: every one of
// the 3 `swits[cn]` thresholds cleared, so drive() clamps `speed` to the
// closed-form cap `fr(swits[cn][2]/2 + power*swits[cn][2]/196)` instead
// of adding another `acelf[cn][n9]` increment) and `n8 === 2` (reverse
// cap, same shape against `swits[cn][1]`, only 2 thresholds). No other
// scenario in this file runs long enough for the accumulated speed to
// clear all of a gear's thresholds -- drive_scenario/drive_track_scenario
// stay under 20 ticks, well inside the lower gears.
//
// Oracle: the real web/Mad.js run against the same rig (up-held /
// down-held from rest) for up to 400 ticks, computing `n9`/`n8` per-tick
// from `mad.speed`/`mad.power` exactly as drive()'s own loop does, to
// find the first tick each cap condition holds. Forward cap first holds
// at tick 63 (speed 283.0057373046875); by tick ~100 `power` has decayed
// enough that the car drops back to n9===2 (a real mechanic -- power
// decays over sustained full throttle, see mad.c's power-decay comment
// in its stunt-score section -- not a bug), so tick 63 is the one exact
// instant to pin down. Reverse cap first holds at tick 32 (speed
// -193.14842224121094) and was still holding at that same tick when
// checked (reverse doesn't share forward's decay pressure over so short
// a run).
static void drive_gear_cap_scenario(void) {
  nfm_set_seed(9001);
  Medium m;
  medium_init(&m);
  Trackers t;
  trackers_init(&t);
  t.x[0] = 0; t.y[0] = 200; t.z[0] = 0;
  t.xy[0] = 0; t.zy[0] = 0;
  t.radx[0] = 5000; t.rady[0] = 300; t.radz[0] = 5000;
  t.skd[0] = 0; t.dam[0] = 1;
  t.notwall[0] = true; t.decor[0] = false;
  t.nt = 1;
  trackers_devidetrackers(&t, -10000, 20000, -10000, 20000);

  CarDefine cd;
  car_define_init(&cd);
  Record rpd;
  record_init(&rpd);
  CheckPoints cp;
  check_points_init(&cp);
  cp.pcs = 0;
  cp.n = 1;
  cp.typ[0] = 1;
  cp.x[0] = 0;
  cp.y[0] = 0;
  cp.z[0] = 1000000;
  cp.fn = 0;
  cp.nsp = 1;
  cp.nlaps = 1;

  vfs_set_fpath("../../../");
  char *text = vfs_read_text("mycars/Simple_Car.rad");
  CHECK(text != NULL, "mycars/Simple_Car.rad read (mad_test gear_cap)");
  if (!text) {
    medium_free(&m);
    trackers_free_sect(&t);
    return;
  }
  m.loadnew = true;
  ContO baseModel;
  cont_o_init_buf(&baseModel, text, &m, &t);
  m.loadnew = false;
  free(text);
  baseModel.shadow = true;
  ContO contO;
  cont_o_init_copy(&contO, &baseModel, 0, 0, 0, 0);

  XtGraphicsStub xt;
  xt_graphics_stub_init(&xt);
  xt.im = 2;

  Control control;
  control_init(&control, &m);
  control_falseo(&control, 0);
  control.up = true;

  Mad mad;
  mad_init(&mad, &cd, &m, &rpd, &xt, 2);
  mad_reseto(&mad, 3, &contO, &cp);

  for (int32_t i = 0; i <= 63; i++) {
    mad_drive(&mad, &control, &contO, &t, &cp);
  }
  bool ok = contO.x == 0 && contO.y == 182 && contO.z == 10355 && contO.xz == 0 && contO.xy == 1 &&
            contO.zy == 0 && contO.wxz == 0 && contO.wzy == -21 && mad.speed == 283.0057373046875f &&
            mad.mtouch && mad.wtouch && !mad.capsized && mad.skid == 0 &&
            mad.scz[0] == 283.0f && mad.scx[0] == 0.0f && mad.scy[0] == 0.0f;
  CHECK(ok, "gear_cap forward tick63 (n9===3 top-gear speed cap)");

  cont_o_free(&contO);
  cont_o_free(&baseModel);
  medium_free(&m);
  trackers_free_sect(&t);
}

static void drive_reverse_cap_scenario(void) {
  nfm_set_seed(9001);
  Medium m;
  medium_init(&m);
  Trackers t;
  trackers_init(&t);
  t.x[0] = 0; t.y[0] = 200; t.z[0] = 0;
  t.xy[0] = 0; t.zy[0] = 0;
  t.radx[0] = 5000; t.rady[0] = 300; t.radz[0] = 5000;
  t.skd[0] = 0; t.dam[0] = 1;
  t.notwall[0] = true; t.decor[0] = false;
  t.nt = 1;
  trackers_devidetrackers(&t, -10000, 20000, -10000, 20000);

  CarDefine cd;
  car_define_init(&cd);
  Record rpd;
  record_init(&rpd);
  CheckPoints cp;
  check_points_init(&cp);
  cp.pcs = 0;
  cp.n = 1;
  cp.typ[0] = 1;
  cp.x[0] = 0;
  cp.y[0] = 0;
  cp.z[0] = 1000000;
  cp.fn = 0;
  cp.nsp = 1;
  cp.nlaps = 1;

  vfs_set_fpath("../../../");
  char *text = vfs_read_text("mycars/Simple_Car.rad");
  CHECK(text != NULL, "mycars/Simple_Car.rad read (mad_test reverse_cap)");
  if (!text) {
    medium_free(&m);
    trackers_free_sect(&t);
    return;
  }
  m.loadnew = true;
  ContO baseModel;
  cont_o_init_buf(&baseModel, text, &m, &t);
  m.loadnew = false;
  free(text);
  baseModel.shadow = true;
  ContO contO;
  cont_o_init_copy(&contO, &baseModel, 0, 0, 0, 0);

  XtGraphicsStub xt;
  xt_graphics_stub_init(&xt);
  xt.im = 2;

  Control control;
  control_init(&control, &m);
  control_falseo(&control, 0);
  control.down = true;

  Mad mad;
  mad_init(&mad, &cd, &m, &rpd, &xt, 2);
  mad_reseto(&mad, 3, &contO, &cp);

  for (int32_t i = 0; i <= 32; i++) {
    mad_drive(&mad, &control, &contO, &t, &cp);
  }
  bool ok = contO.x == 0 && contO.y == 132 && contO.z == -3025 && contO.xz == 0 && contO.xy == 0 &&
            contO.zy == 0 && contO.wxz == 0 && contO.wzy == 10 && mad.speed == -193.14842224121094f &&
            mad.mtouch && mad.wtouch && !mad.capsized && mad.skid == 0 &&
            mad.scz[0] == -193.0f && mad.scx[0] == 0.0f && mad.scy[0] == 0.0f;
  CHECK(ok, "reverse_cap tick32 (n8===2 reverse speed cap)");

  cont_o_free(&contO);
  cont_o_free(&baseModel);
  medium_free(&m);
  trackers_free_sect(&t);
}

// Third oracle-verified drive() regression scenario: same car accelerating
// on the flat-ground plane from drive_track_scenario(), but this time
// with a second Trackers plane -- a wall (zy=-90, radz=287, matching the
// JS's own "wall thickness===287 means always solid" magic-number shape)
// placed ahead of it. Ground's radz stops short of the wall's own face so
// the two bounding boxes don't overlap; see the JS oracle script's own
// comment on why that matters (first tracker to match a given wheel in a
// tick claims it -- array7[wheel] -- and blocks every other tracker's
// branches for that wheel that same tick, a real game mechanic, not
// something to avoid via ordering). Exercises the crank/regz wall-bounce
// path (trackers.zy===-90) that no other scenario here reaches.
static void drive_wall_scenario(int32_t nticks, const char *label) {
  nfm_set_seed(9001);
  Medium m;
  medium_init(&m);
  Trackers t;
  trackers_init(&t);
  t.x[0] = 0; t.y[0] = 200; t.z[0] = 0;
  t.xy[0] = 0; t.zy[0] = 0;
  t.radx[0] = 5000; t.rady[0] = 300; t.radz[0] = 400;
  t.skd[0] = 0; t.dam[0] = 1;
  t.notwall[0] = true; t.decor[0] = false;
  t.x[1] = 0; t.y[1] = 200; t.z[1] = 700;
  t.xy[1] = 0; t.zy[1] = -90;
  t.radx[1] = 5000; t.rady[1] = 300; t.radz[1] = 287;
  t.skd[1] = 0; t.dam[1] = 1;
  t.notwall[1] = false; t.decor[1] = false;
  t.nt = 2;
  trackers_devidetrackers(&t, -10000, 20000, -10000, 20000);

  CarDefine cd;
  car_define_init(&cd);
  Record rpd;
  record_init(&rpd);
  CheckPoints cp;
  check_points_init(&cp);
  cp.pcs = 0;
  cp.n = 1;
  cp.typ[0] = 1;
  cp.x[0] = 0;
  cp.y[0] = 0;
  cp.z[0] = 1000000;
  cp.fn = 0;
  cp.nsp = 1;
  cp.nlaps = 1;

  vfs_set_fpath("../../../");
  char *text = vfs_read_text("mycars/Simple_Car.rad");
  CHECK(text != NULL, "mycars/Simple_Car.rad read (mad_test wall)");
  if (!text) {
    medium_free(&m);
    trackers_free_sect(&t);
    return;
  }
  m.loadnew = true;
  ContO baseModel;
  cont_o_init_buf(&baseModel, text, &m, &t);
  m.loadnew = false;
  free(text);
  baseModel.shadow = true;
  ContO contO;
  cont_o_init_copy(&contO, &baseModel, 0, 0, 0, 0);

  XtGraphicsStub xt;
  xt_graphics_stub_init(&xt);
  xt.im = 2;

  Control control;
  control_init(&control, &m);
  control_falseo(&control, 0);
  control.up = true;

  Mad mad;
  mad_init(&mad, &cd, &m, &rpd, &xt, 2);
  mad_reseto(&mad, 3, &contO, &cp);

  for (int32_t i = 0; i < nticks; i++) {
    mad_drive(&mad, &control, &contO, &t, &cp);
    if (i == 13) {
      char lbl[64];
      snprintf(lbl, sizeof(lbl), "%s tick13 (wall hit)", label);
      bool ok = contO.x == 0 && contO.y == 135 && contO.z == 769 && contO.xz == 0 && contO.xy == 0 && contO.zy == 0 &&
                contO.wzy == -12 && mad.speed == 90.5f && !mad.mtouch && mad.wtouch && !mad.capsized &&
                mad.skid == 2 && mad.crank[0][0] == 1 && mad.crank[0][1] == 1 && mad.crank[0][2] == 0 && mad.crank[0][3] == 0 &&
                mad.scz[0] == 191.10000610351562f && mad.scz[2] == 91.0f && mad.hitmag == 0;
      CHECK(ok, lbl);
    }
  }

  char lbl2[64];
  snprintf(lbl2, sizeof(lbl2), "%s tick18 (settled)", label);
  bool ok2 = contO.x == 0 && contO.y == 182 && contO.z == 1970 && contO.xz == 0 && contO.xy == 0 && contO.zy == 0 &&
             contO.wzy == -27 && mad.speed == 181.4250030517578f && mad.mtouch && mad.wtouch && !mad.capsized &&
             mad.skid == 0 && mad.scz[0] == 174.60000610351562f && mad.scz[2] == 188.25f && mad.hitmag == 0;
  CHECK(ok2, lbl2);

  // Previously (through the commit that added this scenario) tick 19
  // onward diverged from web/Mad.js by exactly one this.m.random() draw
  // per tick from this point on -- root-caused to a real bug, not a
  // narrow test artifact: cont_o_dust (ContO's `#dust`) was missing the
  // public `dust()` wrapper's `setDrawPhase(true)` guard, so its one
  // random() call drew from the SIM stream instead of the DRAW stream.
  // Harmless while dust never fired (every earlier scenario), but once
  // driving over uneven ground makes dust() fire nearly every tick (see
  // cont_o_pdust's doc comment), it silently ate one sim-stream draw per
  // occurrence and desynced everything downstream on that stream from
  // then on. Fixed in cont_o_dust (native/core/cont_o.c) by wrapping its
  // body in nfm_set_draw_phase(true)/(false), matching the JS exactly --
  // this scenario now matches web/Mad.js exactly through tick 29,
  // including the cosmetic landing-wobble jitter (contO.xy/zy) that used
  // to be the first field to visibly drift.
  for (int32_t i = nticks; i < 30; i++) {
    mad_drive(&mad, &control, &contO, &t, &cp);
  }
  char lbl3[64];
  snprintf(lbl3, sizeof(lbl3), "%s tick29 (post-fix draw-count check)", label);
  bool ok3 = contO.x == 0 && contO.y == 182 && contO.z == 4198 && contO.xz == 0 && contO.xy == -1 && contO.zy == 1 &&
             contO.wzy == -7 && mad.speed == 221.92500305175781f && mad.mtouch && mad.wtouch && !mad.capsized &&
             mad.skid == 0 && mad.scz[0] == 222.0f && mad.scz[2] == 222.0f && mad.hitmag == 0;
  CHECK(ok3, lbl3);

  cont_o_free(&contO);
  cont_o_free(&baseModel);
  medium_free(&m);
  trackers_free_sect(&t);
}

// High Rider (18000 health) hammered in one spot. NFM 2 dented its points
// out of clrad and stopped taking damage at 2163; Extended's dent alone at
// 3513. With the guard (mad.c dent_keep) it is wrecked.
static void dent_scenario(void) {
  nfm_set_seed(77);
  Medium m; medium_init(&m);
  Trackers t; trackers_init(&t);
  CarDefine cd; car_define_init(&cd);
  for (int i = 0; i < CAR_DEFINE_NUM_CARS; i++) {
    cd.maxmag[i] = 18000; cd.clrad[i] = 3000; cd.dammult[i] = 1.0f; cd.msquash[i] = 10;
  }
  static Record rpd; record_init(&rpd);
  vfs_set_fpath("../../../");
  VfsZip zip;
  CHECK(vfs_read_zip("data/models.zip", &zip), "dent: data/models.zip readable");
  if (!zip.count) { medium_free(&m); return; }
  char *text = NULL;
  for (int32_t i = 0; i < zip.count; i++)
    if (strcmp(zip.entries[i].name, "formula7.rad") == 0) text = vfs_entry_text(&zip.entries[i]);
  CHECK(text != NULL, "dent: formula7.rad present");
  if (!text) { vfs_free_zip(&zip); medium_free(&m); return; }
  m.loadnew = true;
  ContO base; cont_o_init_buf(&base, text, &m, &t);
  m.loadnew = false; free(text);
  ContO contO; cont_o_init_copy(&contO, &base, 0, 0, 0, 0);
  XtGraphicsStub xt; xt_graphics_stub_init(&xt); xt.im = 0;
  Mad mad; mad_init(&mad, &cd, &m, &rpd, &xt, 1);
  mad.cn = 8;   // High Rider
  for (int32_t k = 0; k < 2000 && mad.hitmag < 18000; k++) mad_regx(&mad, 0, 2000.0f, &contO);
  printf("dent: %d damage (18000 wrecks it)\n", mad.hitmag);
  CHECK(mad.hitmag >= 18000, "dent: a dented car keeps taking damage until it is wrecked");
  cont_o_free(&contO);
  cont_o_free(&base);
  vfs_free_zip(&zip);
  medium_free(&m);
}

int main(void) {
  dent_scenario();
  handb_grounded_scenario();
  int32_t expectFixes[6] = {-1, 4, 3, 2, 1, -1};
  for (int32_t nfix = 0; nfix <= 5; nfix++) {
    scenario(nfix, true, expectFixes[nfix]);
    scenario(nfix, false, expectFixes[nfix]);
  }

  drive_scenario(10, false, "coast10");
  drive_scenario(10, true, "up10");
  drive_track_scenario(15, false, "ground_up15");
  drive_track_scenario(20, true, "ground_upleft20");
  drive_gear_cap_scenario();
  drive_reverse_cap_scenario();
  drive_checkpoint_scenario();
  drive_slope_scenario();
  drive_multitracker_scenario();
  drive_capsize_scenario();
  drive_repair_scenario();
  colide_scenario();
  drive_wall_scenario(19, "wall_hit19");

  if (failures == 0) {
    printf("all tests passed\n");
    return 0;
  }
  fprintf(stderr, "%d failure(s)\n", failures);
  return 1;
}
