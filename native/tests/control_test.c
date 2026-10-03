// Host-buildable test for native/core/control.c (falseo/reset/py/pys/
// preform).
//
// No web/Control.test.js coverage of reset() exists that fits this
// scenario, so expected values were captured by running the real
// web/Control.js under Node: a synthetic CheckPoints (6 checkpoints, 3 fix
// points) run through every stage number reset() special-cases (1 as a
// plain default, 16-26), each with n in {0, 13, 14} (the two "not this
// player" branches reset() checks). All 33 (stage, n) combinations'
// hold/revstart/statusque/fpnt[]/left/arrace matched exactly on the first
// attempt.
//
// preform() itself is checked against Control.test.js's own "preform main
// work method matches Java probe state" fixture (see
// test_preform_matches_java_probe() below) -- the one preform() scenario
// with a real Java-probe oracle. It's a stage-1 scenario that only lightly
// exercises SECTION 1 and none of the per-stage scripted branches (those
// need their own scenario coverage separately), so treat it as a useful
// smoke test establishing the translation is fundamentally sound, not
// exhaustive coverage of preform()'s ~1950 lines.
#include <stdio.h>
#include <string.h>
#include "../core/java_compat.h"
#include "../core/medium.h"
#include "../core/check_points.h"
#include "../core/control.h"
#include "../core/mad.h"
#include "../core/cont_o.h"
#include "../core/car_define.h"
#include "../core/trackers.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
  if (!(cond)) { fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); failures++; } \
} while (0)

static const int32_t kCpData[6][2] = {{100,200},{500,-300},{900,700},{-200,150},{300,900},{1200,-400}};
static const int32_t kFnData[3][2] = {{80,190},{490,-280},{890,690}};

typedef struct {
  int32_t stage, n, hold, revstart, statusque, fpnt[5];
} Expect;

static const Expect kExpect[33] = {
  {1,0,0,0,0,{8,9,10,0,0}},     {1,13,0,0,0,{8,9,10,0,0}},    {1,14,0,0,0,{8,9,10,0,0}},
  {16,0,50,0,0,{8,9,10,0,0}},   {16,13,50,0,0,{8,9,10,0,0}},  {16,14,50,0,0,{8,9,10,0,0}},
  {17,0,10,0,0,{8,9,10,0,0}},   {17,13,10,0,0,{8,9,10,0,0}},  {17,14,10,0,0,{8,9,10,0,0}},
  {18,0,50,0,0,{8,9,10,0,0}},   {18,13,50,0,0,{8,9,10,0,0}},  {18,14,50,0,0,{8,9,10,0,0}},
  {19,0,0,0,0,{14,36,0,0,0}},   {19,13,0,0,0,{14,36,0,0,0}},  {19,14,0,0,0,{14,36,0,0,0}},
  {20,0,30,0,0,{8,9,10,0,0}},   {20,13,30,0,0,{8,9,10,0,0}},  {20,14,30,0,0,{8,9,10,0,0}},
  {21,0,35,25,0,{8,9,10,0,0}},  {21,13,5,0,0,{8,9,10,0,0}},   {21,14,35,25,0,{8,9,10,0,0}},
  {22,0,25,13,0,{8,9,10,0,0}},  {22,13,5,0,0,{8,9,10,0,0}},   {22,14,25,13,0,{8,9,10,0,0}},
  {24,0,30,1,0,{8,9,10,0,0}},   {24,13,30,1,0,{8,9,10,0,0}},  {24,14,30,0,0,{8,9,10,0,0}},
  {25,0,40,0,0,{8,9,10,0,0}},   {25,13,40,0,0,{8,9,10,0,0}},  {25,14,40,0,0,{8,9,10,0,0}},
  {26,0,20,0,0,{0,0,0,39,0}},   {26,13,20,0,0,{0,0,0,39,0}},  {26,14,20,0,0,{0,0,0,39,0}},
};

// Differential check of control_preform() against web/Control.test.js's
// "preform main work method matches Java probe state" fixture -- the only
// preform() scenario with a verified Java-probe oracle
// (js/tools/ControlProbe.java). That JS test stubs `m.random()` to always
// return exactly 0.5; medium_random()'s own interpolating/rlog replay path
// (see medium_test.c's test_random_recording_and_replay) gives the same
// constant-stub behavior here for free: with rn==1, `rlog[(rp++) % 1]` is
// always index 0, so writing a single 0.5 into rlog[0] reproduces the JS
// stub exactly, call for call, without needing a mockable Medium.
//
// mad/contO/checkPoints/trackers are zeroed rather than run through their
// own init functions, mirroring the JS fixture's bare object literals
// (only a handful of fields set, everything else left at JS's implicit
// "undefined") -- every field this scenario reads but neither side sets
// reduces to 0/0.0f producing NaN in comparisons on both sides (see
// mad.hitmag / cd.maxmag[0] below), matching JS's undefined/0 NaN exactly.
static void test_preform_matches_java_probe(void) {
  Medium m;
  medium_init(&m);
  m.interpolating = true;
  m.rlog[0] = 0.5f;
  m.rn = 1;
  m.rp = 0;

  Control c;
  control_init(&c, &m);

  CarDefine cd;
  memset(&cd, 0, sizeof(cd)); // maxmag[*]/swits[*] all 0, matching intArray(16)

  Mad mad;
  memset(&mad, 0, sizeof(mad));
  mad.dest = false;
  mad.mtouch = true;
  mad.im = 1;
  mad.power = 90.0f;
  mad.point = 0;
  mad.cd = &cd;

  ContO contO;
  memset(&contO, 0, sizeof(contO));
  contO.x = 500;
  contO.z = 500;
  contO.xz = 45;

  CheckPoints cp;
  memset(&cp, 0, sizeof(cp));
  cp.stage = 1;
  int32_t posArr[7] = {0, 1, 2, 3, 4, 5, 6};
  int32_t opxzArr[7] = {0, 1000, 2000, 3000, 4000, 5000, 6000};
  for (int i = 0; i < 7; i++) {
    cp.pos[i] = posArr[i];
    cp.clear[i] = 0;
    cp.opx[i] = opxzArr[i];
    cp.opz[i] = opxzArr[i];
  }
  cp.typ[0] = 1; // typ[1..9] already 0 from the memset
  cp.fn = 2;
  cp.n = 10;
  cp.nsp = 4;
  cp.fx[0] = 1000; cp.fx[1] = 2000;
  cp.fz[0] = 3000; cp.fz[1] = 4000;
  int32_t xArr[10] = {100, 200, 300, 400, 500, 600, 700, 800, 900, 1000};
  int32_t zArr[10] = {150, 250, 350, 450, 550, 650, 750, 850, 950, 1050};
  for (int i = 0; i < 10; i++) { cp.x[i] = xArr[i]; cp.z[i] = zArr[i]; }

  Trackers trackers;
  trackers_init(&trackers);

  c.stcnt = 10;
  c.statusque = 5;

  control_preform(&c, &mad, &contO, &cp, &trackers);

  // Expected values from Control.test.js, verified against the real Java
  // probe (js/tools/ControlProbe.java) with m.random() stubbed to 0.5:
  // left=false, right=true, up=true, down=false, handb=false, acuracy=0,
  // upwait=20, skiplev=0.9 (fr), saftey=3, mustland=0.0, pan=0.
  CHECK(c.up == true, "preform probe: up");
  CHECK(c.down == false, "preform probe: down");
  CHECK(c.left == false, "preform probe: left");
  CHECK(c.right == true, "preform probe: right");
  CHECK(c.handb == false, "preform probe: handb");
  CHECK(c.acuracy == 0, "preform probe: acuracy");
  CHECK(c.upwait == 20, "preform probe: upwait");
  CHECK(c.skiplev == 0.9f, "preform probe: skiplev");
  CHECK(c.saftey == 3, "preform probe: saftey");
  CHECK(c.mustland == 0.0f, "preform probe: mustland");
  CHECK(c.pan == 0, "preform probe: pan");

  medium_free(&m);
}

// Regression test for the Control.java:1794 `forget`-flag fix: a special
// fix point must LATCH forget=true (so recovery steering continues across
// ticks) and a normal one must CLEAR it -- the bug (`!checkPoints->special`)
// inverted both. Reuses test_preform_matches_java_probe()'s exact known-
// good mad/contO/checkPoints/trackers scaffolding (same stage/typ/clear
// setup already proven to run preform() end-to-end without hitting an
// unrelated infinite loop or OOB access) and only overrides the handful of
// fields that steer execution into the fix-point retarget block: c.trfix=2
// and mad.missedcp=0 open `(missedcp==0||forget||trfix==4) && trfix!=0`;
// c.fpnt[]'s default-zeroed contents (control_init never sets it) index
// checkPoints.x[0]/z[0], and with cp.fn==1 the block's own nearest-fix-point
// search picks n26=0 on its first (and only) iteration regardless of
// distance (py3==-10 sentinel), landing squarely on checkPoints.special[0].
static void run_forget_flag_scenario(bool special_flag, bool *out_forget) {
  Medium m;
  medium_init(&m);
  m.interpolating = true;
  m.rlog[0] = 0.5f;
  m.rn = 1;
  m.rp = 0;

  Control c;
  control_init(&c, &m);
  c.trfix = 2;

  CarDefine cd;
  memset(&cd, 0, sizeof(cd));

  Mad mad;
  memset(&mad, 0, sizeof(mad));
  mad.dest = false;
  mad.mtouch = true;
  mad.im = 1;
  mad.power = 90.0f;
  mad.point = 0;
  mad.missedcp = 0;
  mad.cd = &cd;

  ContO contO;
  memset(&contO, 0, sizeof(contO));
  // Far from checkPoints.x[0]/z[0] (100,150) -- the block right after the
  // one under test re-clears forget whenever the car is within 2000
  // (squared-distance units) of its fix point, which would mask the very
  // assignment this scenario is checking. This distance stays outside that
  // radius while remaining small enough to avoid int32 overflow in the
  // surrounding bearing/atan arithmetic (see the opx/opz table above).
  contO.x = 100000;
  contO.z = 100000;
  contO.xz = 45;

  CheckPoints cp;
  memset(&cp, 0, sizeof(cp));
  cp.stage = 1; // not 27, not 10/19/26, not (18 && pcleared==73) -- forget's
                // pre-set-to-true special case must stay out of the way so
                // this test isolates the trfix==2 branch's own assignment.
  int32_t posArr[7] = {0, 1, 2, 3, 4, 5, 6};
  int32_t opxzArr[7] = {0, 1000, 2000, 3000, 4000, 5000, 6000};
  for (int i = 0; i < 7; i++) {
    cp.pos[i] = posArr[i];
    cp.clear[i] = 0;
    cp.opx[i] = opxzArr[i];
    cp.opz[i] = opxzArr[i];
  }
  cp.typ[0] = 1;
  cp.fn = 1; // single fix point -- n26's search always lands on it (n27==0)
  cp.n = 10;
  cp.nsp = 4;
  cp.special[0] = special_flag;
  int32_t xArr[10] = {100, 200, 300, 400, 500, 600, 700, 800, 900, 1000};
  int32_t zArr[10] = {150, 250, 350, 450, 550, 650, 750, 850, 950, 1050};
  for (int i = 0; i < 10; i++) { cp.x[i] = xArr[i]; cp.z[i] = zArr[i]; }
  // c.fpnt[] stays at control_init's default zero -- fpnt[0]==0 indexes
  // checkPoints.x[0]/z[0] above, which is exactly what cp.special[0] gates.

  Trackers trackers;
  trackers_init(&trackers);

  // Deliberately leave stcnt/statusque at control_init's 0/0 (stcnt >
  // statusque is false) so SECTION 1 does not run: it unconditionally
  // resets c->trfix from mad->hitmag/cd->maxmag[] (control.c:632-633)
  // before re-deriving it, which would clobber the trfix=2 this scenario
  // sets directly to reach the block under test. SECTION 2 (where that
  // block lives) is gated only by mad->mtouch, not by this same stcnt
  // check, so it still runs.

  control_preform(&c, &mad, &contO, &cp, &trackers);

  *out_forget = c.forget;
  medium_free(&m);
}

int main(void) {
  for (int32_t i = 0; i < 33; i++) {
    const Expect *e = &kExpect[i];
    nfm_set_seed(9001);
    Medium m;
    medium_init(&m);
    CheckPoints cp;
    check_points_init(&cp);
    cp.stage = e->stage;
    cp.nsp = 12;
    cp.n = 6;
    cp.fn = 3;
    for (int j = 0; j < 6; j++) { cp.x[j] = kCpData[j][0]; cp.z[j] = kCpData[j][1]; }
    for (int j = 0; j < 3; j++) { cp.fx[j] = kFnData[j][0]; cp.fz[j] = kFnData[j][1]; }

    Control c;
    control_init(&c, &m);
    control_reset(&c, &cp, e->n);

    char label[64];
    snprintf(label, sizeof(label), "stage%d n=%d hold/revstart/statusque", e->stage, e->n);
    CHECK(c.hold == e->hold && c.revstart == e->revstart && c.statusque == e->statusque, label);
    snprintf(label, sizeof(label), "stage%d n=%d fpnt", e->stage, e->n);
    CHECK(c.fpnt[0] == e->fpnt[0] && c.fpnt[1] == e->fpnt[1] && c.fpnt[2] == e->fpnt[2] &&
              c.fpnt[3] == e->fpnt[3] && c.fpnt[4] == e->fpnt[4],
          label);
    snprintf(label, sizeof(label), "stage%d n=%d flags cleared", e->stage, e->n);
    CHECK(!c.left && !c.right && !c.up && !c.down && !c.handb && c.lookback == 0 && !c.arrace &&
              !c.mutem && !c.mutes,
          label);

    medium_free(&m);
  }

  {
    Medium m;
    medium_init(&m);
    Control c;
    control_init(&c, &m);
    c.left = true;
    c.radar = true;
    c.chatup = 5;
    c.multion = 2;
    c.mutem = true;
    control_falseo(&c, 1);
    CHECK(!c.left && c.radar && c.chatup == 5 && c.multion == 2 && c.mutem, "falseo(1) skips radar/chatup/multion/mute");
    control_falseo(&c, 0);
    CHECK(!c.radar && c.chatup == 0 && c.multion == 0 && !c.mutem, "falseo(0) clears everything");
    medium_free(&m);
  }

  CHECK(control_py(10, 3, 20, 7) == (10 - 3) * (10 - 3) + (20 - 7) * (20 - 7), "py");

  test_preform_matches_java_probe();

  {
    bool forget_special = false, forget_normal = false;
    run_forget_flag_scenario(true, &forget_special);
    CHECK(forget_special == true, "Control.java:1794 forget latches true on a special fix point");
    run_forget_flag_scenario(false, &forget_normal);
    CHECK(forget_normal == false, "Control.java:1794 forget clears on a normal fix point");
  }

  if (failures == 0) {
    printf("all tests passed\n");
    return 0;
  }
  fprintf(stderr, "%d failure(s)\n", failures);
  return 1;
}
