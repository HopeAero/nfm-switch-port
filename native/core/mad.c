// ports web/Mad.js -- see mad.h for scope.
#include "mad.h"
#include "java_compat.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

void mad_init(Mad *mad, CarDefine *cd, Medium *m, Record *rpd, XtGraphicsStub *xt, int32_t im) {
  memset(mad, 0, sizeof(*mad));
  mad->drag = 0.5f;
  mad->pmlt = 1;
  mad->nmlt = 1;
  mad->focus = -1;
  mad->power = 75.0f;
  mad->fixes = -1;
  mad->cd = cd;
  mad->m = m;
  mad->rpd = rpd;
  mad->xt = xt;
  mad->im = im;
}

// M A S H E E N, NFM 2's car 13 (Extended's 36).
#define CAR_MASHEEN 13

void mad_reseto(Mad *mad, int32_t cn, ContO *contO, CheckPoints *checkPoints) {
  mad->cn = cn;
  mad->lastcolider = -1;
  // Extended (Madness.java:1154-1155): an empty bar; the next tick refills
  // speclast (an empty bar and no special left).
  mad->spatk = 0.0f;
  mad->speclast = 0.0f;
  mad->speclast2 = 120.0f;
  mad->specialact = mad->frozen = mad->strswap = mad->leech = mad->redstr = false;
  // Extended (Madness.java:1080-1087): M A S H E E N takes less damage and
  // reaches further in Classic Mode, in this car's copy of the tables.
  if (mad->xt->extended) {
    mad->cd->dammult[CAR_MASHEEN] = mad->xt->classicmode ? 0.225f : 0.3f;
    mad->cd->clrad[CAR_MASHEEN] = mad->xt->classicmode ? 30000 : 20000;
  }
  for (int32_t i = 0; i < NFM_MAX_CARS; i++) {
    mad->dominate[i] = false;
    mad->caught[i] = false;
  }
  mad->mxz = 0;
  mad->cxz = 0;
  mad->pzy = 0;
  mad->pxy = 0;
  mad->speed = 0.0f;
  for (int32_t j = 0; j < 4; j++) {
    mad->scy[j] = 0.0f;
    mad->scx[j] = 0.0f;
    mad->scz[j] = 0.0f;
  }

  // Java: ((float)sqrt(A) + (float)sqrt(B) + (float)sqrt(C) + (float)sqrt(D))
  // / 10000.0f * (float)(bounce - 0.3) -- float sums, and the 0.3 is a
  // double: (float)(bounce - 0.3) is not bounce - 0.3f.
  // Extended divides by 8000 (Madness.java:1104): a quarter more yaw from
  // uneven wheels after hits and bumps.
  double sqrtA = sqrt((double)(contO->keyz[0] * contO->keyz[0] + contO->keyx[0] * contO->keyx[0]));
  double sqrtB = sqrt((double)(contO->keyz[1] * contO->keyz[1] + contO->keyx[1] * contO->keyx[1]));
  double sqrtC = sqrt((double)(contO->keyz[2] * contO->keyz[2] + contO->keyx[2] * contO->keyx[2]));
  double sqrtD = sqrt((double)(contO->keyz[3] * contO->keyz[3] + contO->keyx[3] * contO->keyx[3]));
  float fSqrtA = (float)sqrtA, fSqrtB = (float)sqrtB, fSqrtC = (float)sqrtC, fSqrtD = (float)sqrtD;
  float middle = (((fSqrtA + fSqrtB) + fSqrtC) + fSqrtD) / (mad->xt->extended ? 8000.0f : 10000.0f);
  mad->forca = middle * (float)((double)mad->cd->bounce[mad->cn] - 0.3);

  mad->mtouch = false;
  mad->wtouch = false;
  mad->txz = 0;
  mad->fxz = 0;
  mad->pmlt = 1;
  mad->nmlt = 1;
  mad->dcnt = 0;
  mad->skid = 0;
  mad->pushed = false;
  mad->gtouch = false;
  mad->pl = false;
  mad->pr = false;
  mad->pd = false;
  mad->pu = false;
  mad->loop = 0;
  mad->ucomp = 0.0f;
  mad->dcomp = 0.0f;
  mad->lcomp = 0.0f;
  mad->rcomp = 0.0f;
  mad->lxz = 0;
  mad->travxy = 0;
  mad->travzy = 0;
  mad->travxz = 0;
  mad->rtab = false;
  mad->ftab = false;
  mad->btab = false;
  mad->powerup = 0.0f;
  mad->xtpower = 0;
  mad->trcnt = 0;
  mad->capcnt = 0;
  mad->tilt = 0.0f;
  for (int32_t k = 0; k < 4; k++) {
    for (int32_t l = 0; l < 4; l++) {
      mad->crank[k][l] = 0;
      mad->lcrank[k][l] = 0;
    }
  }
  mad->pan = 0;
  mad->pcleared = checkPoints->pcs;
  mad->clear = 0;
  mad->nlaps = 0;
  mad->focus = -1;
  mad->missedcp = 0;
  mad->nofocus = false;
  mad->power = 98.0f;
  mad->lastcolido = 0;
  checkPoints->dested[mad->im] = 0;
  mad->squash = 0;
  mad->nbsq = 0;
  mad->hitmag = 0;
  mad->cntdest = 0;
  mad->dest = false;
  mad->newcar = false;
  if (mad->im == mad->xt->im) {
    mad->m->checkpoint = -1;
    mad->m->lastcheck = false;
  }
  mad->rpdcatch = 0;
  mad->newedcar = 0;
  mad->fixes = -1;
  if (checkPoints->nfix == 1) mad->fixes = 4;
  if (checkPoints->nfix == 2) mad->fixes = 3;
  if (checkPoints->nfix == 3) mad->fixes = 2;
  if (checkPoints->nfix == 4) mad->fixes = 1;
}

// --- drive() and its helpers -----------------------------------------
//
// Translated line-by-line from web/Mad.js's `drive()` (~1630 lines) plus
// distruct/regy/regx/regz/colide/rot/rpy/py. Follows the same fr()/trunc()
// case classification as the rest of this port (see PORT_SPEC.md and the
// worked examples throughout cont_o.c) -- each fr() nesting was re-derived
// individually against the JS's own parenthesisation, not pattern-matched
// blindly. Variable names mirror the JS's own procyon-emitted locals
// (n, n2, n3, ...) exactly, block-scoped the same way, so this stays a
// mechanical transliteration a reader can diff against web/Mad.js
// line-by-line rather than a paraphrase.
//
// VERIFICATION STATUS: translated, compiles clean under ASan/UBSan, and
// FOURTEEN oracle scenarios are captured so far (mad_test.c), ALL
// matching web/Mad.js exactly, tick by tick, with no remaining known
// scenario-coverage gaps:
//   - coast10/up10: a fresh car falling from a spawn height under
//     gravity, coasting vs. holding up-throttle, against a trackless
//     single-far-off-checkpoint world. Exercises the !mtouch/airborne
//     position-update path, the mtouch transition via the direct
//     array3>245 suspension-travel-limit shortcut (NOT real track
//     geometry -- see below), and forward-speed integration once
//     "grounded" that way.
//   - ground_up15/ground_upleft20: the same car dropped onto one
//     synthetic flat-ground Trackers plane, up-throttle with and without
//     left-steering. Exercises the ACTUAL trackers.sect collision loop's
//     flat-ground branch (the n51 sector loop), steering (contO.wxz/xz/
//     tilt), and the skid/dust path once lateral speed is nonzero.
//   - wall_hit19: the same car accelerating into a second Trackers plane,
//     a wall (zy===-90), run 30 ticks. Exercises the crank/regz
//     wall-bounce branch (the zy===-90 arm of the n51 sector loop). This
//     scenario caught a real bug: ticks 19-29 diverged from web/Mad.js by
//     exactly one this.m.random() draw per tick once dust() started
//     firing every tick (sustained n22===1 landing-wobble). Root cause:
//     cont_o_dust was missing the public dust() wrapper's
//     setDrawPhase(true) guard, so its one random() call drew from the
//     SIM PRNG stream instead of the DRAW stream, silently eating a
//     sim-stream draw each time dust() fired and desyncing everything
//     downstream. Fixed in cont_o_dust (native/core/cont_o.c).
//   - gear_cap forward/reverse: the ground rig run long enough (63/32
//     ticks) to actually clear every swits[cn] threshold, exercising the
//     throttle/brake gear-curve loop's top-gear-cap closed-form branches
//     (n9===3 / n8===2) that no short scenario ever reaches.
//   - checkpoint: a single REAL reachable typ=1 checkpoint (every other
//     scenario's checkpoint sits 100000+ units away, purely so the
//     focus-search loop has something to iterate) that the car actually
//     clears, exercising mad->clear/pcleared/focus and (since this is a
//     single-checkpoint track, so clearing it always also completes a
//     lap) mad->nlaps/m->checkpoint/m->lastcheck too.
//   - slope: a single Trackers plane with zy=30 (a genuine ramp, not
//     flat ground or a +-90 wall), exercising the mad_rot-based
//     local-frame rotation branch and its front/back-wheel contact
//     split as the car climbs on.
//   - multitracker: a byte-for-byte duplicate ground tracker at the same
//     position as tracker 0, proving the array7[wheel] "first tracker in
//     sect[] order claims it" gate actually works (unlike the wall
//     scenario's ground+wall pair, which satisfy DIFFERENT branches
//     rather than genuinely competing for the same one) -- result
//     matches the single-tracker ground_up15 baseline byte for byte.
//   - capsize: holding the handbrake while airborne (entering the
//     loop-trick state machine), then holding down for a mid-air
//     backflip -- exercises loop/ucomp/dcomp/lcomp/rcomp and the capsize
//     DETECTION itself (the zyinv/n3 wrap-and-compare against the
//     previous tick's pzy, at the very top of this function).
//   - repair: a hand-damaged car parked in a real fix zone long enough
//     for contO->fcnt to climb to 7 and trigger the repair-completion
//     branch. This scenario surfaced a real, more serious gap than a
//     missing test: ContO.js's stepFix() (the repair-animation counter
//     stepper -- SIMULATION despite driving a visual effect, per its own
//     JS comment) had never been ported at all, so contO->fcnt could
//     never advance past 0 and this function's fcnt===7||8 branch was
//     permanently DEAD CODE, not merely untested. Ported as
//     cont_o_step_fix() (native/core/cont_o.c/.h), wired into
//     native/platform/linux/main.c's tick loop right after mad_drive(),
//     matching GameSparker.js's own simulate() ordering.
//   - colide (native/tests/mad_test.c's colide_scenario): two
//     #initCopy clones 150 units apart, one given nonzero forward
//     speed/scz so it dominates and pushes the other -- the first
//     exercise of mad_colide() at all. This scenario surfaced a second
//     real, previously-dormant bug: xt_graphics_stub_human()
//     (native/core/xt_graphics.c) implemented `!isbot[n]`, but the real
//     XtGraphics.human(i) is `humans ? humans.has(i) : i === this.im` --
//     with no humans Set wired up in this single-player-only stub,
//     "human" actually means "the locally-viewed car" (n === xt.im),
//     nothing to do with isbot. Every scenario before this one only ever
//     drove ONE car at im === xt.im, where both formulas happen to
//     agree, so it was invisible until a second car with a different im
//     exercised it. Fixed to `n == xt->im`.
// Every scenario family drives an #initCopy-cloned instance, not a raw
// #initBuf object -- matching how the real game always drives a car
// (GameSparker.js's own tick rebuilds it via #initCopy every "newcar"
// reset). This matters: #initBuf never allocates the shadow/dust
// particle arrays (sx/sy/sz/osmag/scx/scz) even with shadow=true (see
// ContO.js's own #initBuf vs #initCopy), so driving a raw #initBuf object
// crashes the instant a skid rolls a dust() call with nonzero lateral
// speed -- caught by exactly this mismatch while deriving the
// ground_upleft20 oracle.
// NOT exercised by any scenario here (out of scope for a human-driven
// single-player car, not a known gap in this function's own
// correctness): preform()-driven AI branches (Control.js, not ported)
// and xt.multion-gated netplay bookkeeping (multion stays 0 throughout
// this port's single-player scope). Regenerate/extend the oracle by
// running web/Mad.js's drive()/colide() under Node against the same
// scenario, the same way as every other oracle in this port -- the
// scripts used to derive these scenarios' expected values were scratch
// files, not checked in.

static int32_t contO_zy_norm(int32_t i) {
  while (i < 360) i += 360;
  while (i > 360) i -= 360;
  return i;
}

static int32_t contO_xy_norm(int32_t j) {
  while (j < 360) j += 360;
  while (j > 360) j -= 360;
  return j;
}

// Mad.js's OWN `rot()` -- operates on float[4] arrays (wheel-corner
// positions), NOT the same as cont_o_rot/medium_rot (which operate on
// int[] arrays and truncate per-point). Every step here is a single op
// between two already-float values, so this is case 1 throughout: native
// `float` arithmetic is exact, no double detour needed.
static void mad_rot(Mad *mad, float *array, float *array2, float n, float n2, int32_t n3, int32_t n4) {
  if (n3 != 0) {
    float c = medium_cos(mad->m, (float)n3);
    float s = medium_sin(mad->m, (float)n3);
    for (int32_t i = 0; i < n4; i++) {
      float n5 = array[i];
      float n6 = array2[i];
      array[i] = n + (((n5 - n) * c) - ((n6 - n2) * s));
      array2[i] = n2 + (((n5 - n) * s) + ((n6 - n2) * c));
    }
  }
}

// Mad.js's `rpy()`. Called with both int (contO.x/y/z) and float (rotated
// corner array) arguments at different call sites -- only consistent if
// the JS/Java method's own parameters are float (ints widen into them for
// free, matching the int-typed call sites), so parameters are `float`
// here. Every intermediate is a single op between already-float values --
// case 1 throughout -- so trunc() applies directly to the final float.
static int32_t mad_rpy(float n, float n2, float n3, float n4, float n5, float n6) {
  float a = (n - n2) * (n - n2);
  float b = (n3 - n4) * (n3 - n4);
  float c = (n5 - n6) * (n5 - n6);
  return jtrunc((a + b) + c);
}

// Mad.js's `py()` -- identical formula to trackers_py/check_points_py,
// delegates rather than reimplementing.
static int32_t mad_py(int32_t n, int32_t n2, int32_t n3, int32_t n4) {
  return trackers_py(n, n2, n3, n4);
}

// Shared by drive()'s four corner-tilt-angle computations (JS's own
// inline "a"/"a2"/"a3"/"a4" blocks, identical shape each time bar which
// corners/axis feed in). dz/dy/dx are the three coordinate deltas between
// two wheel corners (JS: array2 diff/array3 diff/array diff -- Z/Y/X
// respectively); their sum-of-squares is a case-1 native float chain
// (each fr() wraps one op), but the sqrt()/acos() that follow are NOT
// fr()-wrapped in the JS at all until the very end, so they stay double
// -- only the final `fr(acos(..)/CONST)` rounds once, matching the
// general case-1/case-2 rule rather than the native-float shortcut
// (acos's result isn't already a rounded float).
static int32_t mad_corner_angle(float dz, float dy, float dx, int32_t axisKeySum, int32_t sign) {
  float sumSq = ((dz * dz) + (dy * dy)) + (dx * dx);
  double a5 = sqrt((double)sumSq) / (double)axisKeySum;
  if (a5 >= 0.9998) return sign;
  double acosVal = acos(a5);
  float innerRounded = (float)(acosVal / 0.017453292519943295);
  return jtrunc(innerRounded * (float)sign);
}

// Shared by drive()'s n22==1/n22==2 landing-wobble kicks (JS repeats this
// exact expression shape, once for zy once for xy, each its own fresh
// this.m.random() draw -- callers must call this TWICE, not reuse one
// result, to preserve the PRNG draw count/order). fr(rand*speedFactor*
// speed/swits[2]) and fr(speedFactor2*speed/swits[2]) are each THREE/TWO
// chained ops under one fr() -- case 2, double then round once; the
// subtraction and final multiply by the (separately-rounded) bounce term
// are each single-op case 1.
// Java: (int)((random() * k1 * speed / swits - k2 * speed / swits) * (bounce - 0.3))
// -- float step by step, then the double (bounce - 0.3), truncated once.
static int32_t mad_wobble_delta(Mad *mad, float k1, float k2) {
  const float sw = (float)mad->cd->swits[mad->cn][2];
  const float d = ((medium_random(mad->m) * k1) * mad->speed) / sw - (k2 * mad->speed) / sw;
  return jtrunc_d((double)d * ((double)mad->cd->bounce[mad->cn] - 0.3));
}

void mad_distruct(Mad *mad, ContO *contO) {
  (void)mad; // JS's distruct(contO) doesn't touch `this` either; kept as a
             // Mad method for API consistency with regy/regx/regz/colide.
  for (int32_t i = 0; i < contO->npl; i++) {
    if (contO->p[i].wz == 0 || contO->p[i].gr == -17 || contO->p[i].gr == -16) {
      contO->p[i].embos = 1;
    }
  }
}

// Shared by regy/regx/regz: recolor a damaged plane's HSB from its bfase
// (bruise-fade counter), matching the JS's identical clamp chain +
// HSBtoRGB call repeated verbatim in all three (kept as one helper here
// rather than copy-pasted three times).
static void mad_recolor_plane(Plane *p) {
  if (p->bfase > 20 && p->hsb[1] > 0.25f) p->hsb[1] = 0.25f;
  if (p->bfase > 25 && p->hsb[2] > 0.7f) p->hsb[2] = 0.7f;
  if (p->bfase > 30 && p->hsb[1] > 0.15f) p->hsb[1] = 0.15f;
  if (p->bfase > 35 && p->hsb[2] > 0.6f) p->hsb[2] = 0.6f;
  if (p->bfase > 40) p->hsb[0] = 0.075f;
  if (p->bfase > 50 && p->hsb[2] > 0.5f) p->hsb[2] = 0.5f;
  if (p->bfase > 60) p->hsb[0] = 0.05f;
  int32_t rgb = hsb_to_rgb(p->hsb[0], p->hsb[1], p->hsb[2]);
  p->c[0] = (rgb >> 16) & 255;
  p->c[1] = (rgb >> 8) & 255;
  p->c[2] = rgb & 255;
}

int32_t mad_regy(Mad *mad, int32_t n, float a, ContO *contO) {
  CarDefine *cd = mad->cd;
  Medium *m = mad->m;
  int32_t n2 = 0;
  bool b = true;
  if (mad->xt->multion == 1 && mad->xt->im != mad->im) b = false;
  if (mad->xt->multion >= 2) b = false;
  if (mad->xt->lan && mad->xt->multion >= 1 && mad->xt->isbot[mad->im]) b = true;
  a = a * cd->dammult[mad->cn];
  if (a > 100.0f) {
    record_recy(mad->rpd, n, (double)a, mad->mtouch, mad->im);
    a = a - 100.0f;
    int32_t n3 = 0;
    int32_t n4 = 0;
    int32_t i = contO_zy_norm(contO->zy);
    int32_t j = contO_xy_norm(contO->xy);
    if (i < 210 && i > 150) n3 = -1;
    if (i > 330 || i < 30) n3 = 1;
    if (j < 210 && j > 150) n4 = -1;
    if (j > 330 || j < 30) n4 = 1;
    if (n4 * n3 == 0) {
      mad->shakedam = jtrunc((fabsf(a) + (float)mad->shakedam) / 2.0f);
    }
    if (mad->im == mad->xt->im || mad->colidim) {
      xt_graphics_stub_crash(mad->xt, a, n4 * n3);
    }
    if (n4 * n3 == 0 || mad->mtouch) {
      for (int32_t k = 0; k < contO->npl; k++) {
        float n5 = 0.0f;
        for (int32_t l = 0; l < contO->p[k].n; l++) {
          if (contO->p[k].wz == 0 && mad_py(contO->keyx[n], contO->p[k].ox[l], contO->keyz[n], contO->p[k].oz[l]) < cd->clrad[mad->cn]) {
            n5 = (a / 20.0f) * medium_random(m);
            contO->p[k].oz[l] = jtrunc((float)contO->p[k].oz[l] + n5 * medium_sin(m, (float)i));
            contO->p[k].ox[l] = jtrunc((float)contO->p[k].ox[l] - n5 * medium_sin(m, (float)j));
            if (b) {
              mad->hitmag = jtrunc((float)mad->hitmag + fabsf(n5));
              n2 = jtrunc((float)n2 + fabsf(n5));
            }
          }
        }
        if (n5 != 0.0f) {
          if (fabsf(n5) >= 1.0f) {
            contO->p[k].chip = 1;
            contO->p[k].ctmag = n5;
          }
          if (!contO->p[k].nocol && contO->p[k].glass != 1) {
            contO->p[k].bfase = jtrunc((float)contO->p[k].bfase + n5);
            mad_recolor_plane(&contO->p[k]);
          }
          if (contO->p[k].glass == 1) {
            contO->p[k].gr = jtrunc_d((double)contO->p[k].gr + fabs((double)n5 * 1.5));
          }
        }
      }
    }
    if (n4 * n3 == -1) {
      if (mad->nbsq > 0) {
        int32_t n8 = 0;
        int32_t n9 = 1;
        for (int32_t n10 = 0; n10 < contO->npl; n10++) {
          float n11 = 0.0f;
          for (int32_t n12 = 0; n12 < contO->p[n10].n; n12++) {
            if (contO->p[n10].wz == 0) {
              n11 = (a / 15.0f) * medium_random(m);
              if ((abs(contO->p[n10].oy[n12] - cd->flipy[mad->cn] - mad->squash) < cd->msquash[mad->cn] * 3 ||
                   contO->p[n10].oy[n12] < cd->flipy[mad->cn] + mad->squash) &&
                  mad->squash < cd->msquash[mad->cn]) {
                contO->p[n10].oy[n12] = jtrunc((float)contO->p[n10].oy[n12] + n11);
                n8 = jtrunc((float)n8 + n11);
                n9++;
                if (b) {
                  mad->hitmag = jtrunc((float)mad->hitmag + fabsf(n11));
                  n2 = jtrunc((float)n2 + fabsf(n11));
                }
              }
            }
          }
          if (contO->p[n10].glass == 1) {
            contO->p[n10].gr += 5;
          } else if (n11 != 0.0f) {
            contO->p[n10].bfase = jtrunc((float)contO->p[n10].bfase + n11);
          }
          if (fabsf(n11) >= 1.0f) {
            contO->p[n10].chip = 1;
            contO->p[n10].ctmag = n11;
          }
        }
        mad->squash += n8 / n9;
        mad->nbsq = 0;
      } else {
        mad->nbsq++;
      }
    }
  }
  return n2;
}

int32_t mad_regx(Mad *mad, int32_t n, float n2, ContO *contO) {
  CarDefine *cd = mad->cd;
  Medium *m = mad->m;
  int32_t n3 = 0;
  bool b = true;
  if (mad->xt->multion == 1 && mad->xt->im != mad->im) b = false;
  if (mad->xt->multion >= 2) b = false;
  if (mad->xt->lan && mad->xt->multion >= 1 && mad->xt->isbot[mad->im]) b = true;
  n2 = n2 * cd->dammult[mad->cn];
  if (fabsf(n2) > 100.0f) {
    record_recx(mad->rpd, n, (double)n2, mad->im);
    if (n2 > 100.0f) n2 = n2 - 100.0f;
    if (n2 < -100.0f) n2 = n2 + 100.0f;
    mad->shakedam = jtrunc((fabsf(n2) + (float)mad->shakedam) / 2.0f);
    if (mad->im == mad->xt->im || mad->colidim) xt_graphics_stub_crash(mad->xt, n2, 0);
    for (int32_t i = 0; i < contO->npl; i++) {
      float a = 0.0f;
      for (int32_t j = 0; j < contO->p[i].n; j++) {
        if (contO->p[i].wz == 0 && mad_py(contO->keyx[n], contO->p[i].ox[j], contO->keyz[n], contO->p[i].oz[j]) < cd->clrad[mad->cn]) {
          a = (n2 / 20.0f) * medium_random(m);
          contO->p[i].oz[j] = jtrunc((float)contO->p[i].oz[j] - (a * medium_sin(m, (float)contO->xz)) * medium_cos(m, (float)contO->zy));
          contO->p[i].ox[j] = jtrunc((float)contO->p[i].ox[j] + (a * medium_cos(m, (float)contO->xz)) * medium_cos(m, (float)contO->xy));
          if (b) {
            mad->hitmag = jtrunc((float)mad->hitmag + fabsf(a));
            n3 = jtrunc((float)n3 + fabsf(a));
          }
        }
      }
      if (a != 0.0f) {
        if (fabsf(a) >= 1.0f) {
          contO->p[i].chip = 1;
          contO->p[i].ctmag = a;
        }
        if (!contO->p[i].nocol && contO->p[i].glass != 1) {
          contO->p[i].bfase = jtrunc((float)contO->p[i].bfase + fabsf(a));
          mad_recolor_plane(&contO->p[i]);
        }
        if (contO->p[i].glass == 1) {
          contO->p[i].gr = jtrunc_d((double)contO->p[i].gr + fabs((double)a * 1.5));
        }
      }
    }
  }
  return n3;
}

int32_t mad_regz(Mad *mad, int32_t n, float n2, ContO *contO) {
  CarDefine *cd = mad->cd;
  Medium *m = mad->m;
  int32_t n3 = 0;
  bool b = true;
  if (mad->xt->multion == 1 && mad->xt->im != mad->im) b = false;
  if (mad->xt->multion >= 2) b = false;
  if (mad->xt->lan && mad->xt->multion >= 1 && mad->xt->isbot[mad->im]) b = true;
  n2 = n2 * cd->dammult[mad->cn];
  if (fabsf(n2) > 100.0f) {
    record_recz(mad->rpd, n, (double)n2, mad->im);
    if (n2 > 100.0f) n2 = n2 - 100.0f;
    if (n2 < -100.0f) n2 = n2 + 100.0f;
    mad->shakedam = jtrunc((fabsf(n2) + (float)mad->shakedam) / 2.0f);
    if (mad->im == mad->xt->im || mad->colidim) xt_graphics_stub_crash(mad->xt, n2, 0);
    for (int32_t i = 0; i < contO->npl; i++) {
      float a = 0.0f;
      for (int32_t j = 0; j < contO->p[i].n; j++) {
        if (contO->p[i].wz == 0 && mad_py(contO->keyx[n], contO->p[i].ox[j], contO->keyz[n], contO->p[i].oz[j]) < cd->clrad[mad->cn]) {
          a = (n2 / 20.0f) * medium_random(m);
          contO->p[i].oz[j] = jtrunc((float)contO->p[i].oz[j] + (a * medium_cos(m, (float)contO->xz)) * medium_cos(m, (float)contO->zy));
          contO->p[i].ox[j] = jtrunc((float)contO->p[i].ox[j] + (a * medium_sin(m, (float)contO->xz)) * medium_cos(m, (float)contO->xy));
          if (b) {
            mad->hitmag = jtrunc((float)mad->hitmag + fabsf(a));
            n3 = jtrunc((float)n3 + fabsf(a));
          }
        }
      }
      if (a != 0.0f) {
        if (fabsf(a) >= 1.0f) {
          contO->p[i].chip = 1;
          contO->p[i].ctmag = a;
        }
        if (!contO->p[i].nocol && contO->p[i].glass != 1) {
          contO->p[i].bfase = jtrunc((float)contO->p[i].bfase + fabsf(a));
          mad_recolor_plane(&contO->p[i]);
        }
        if (contO->p[i].glass == 1) {
          contO->p[i].gr = jtrunc_d((double)contO->p[i].gr + fabs((double)a * 1.5));
        }
      }
    }
  }
  return n3;
}

// JS: `colide(contO, mad, contO2)` -- the JS's own parameter is itself
// named `mad` (the OTHER car), shadowing the instance; renamed `mad2`/
// `contO2` here to avoid confusion with `this`/`mad`.
void mad_colide(Mad *mad, ContO *contO, Mad *mad2, ContO *contO2) {
  CarDefine *cd = mad->cd;
  Medium *m = mad->m;
  float array[4], array2[4], array3[4], array4[4], array5[4], array6[4];
  for (int32_t i = 0; i < 4; i++) {
    int32_t sx = contO->x + contO->keyx[i];
    array[i] = (float)sx;
    if (mad->capsized) {
      int32_t sy = contO->y + cd->flipy[mad->cn];
      array2[i] = (float)sy + (float)mad->squash;
    } else {
      int32_t sy = contO->y + contO->grat;
      array2[i] = (float)sy;
    }
    int32_t sz = contO->z + contO->keyz[i];
    array3[i] = (float)sz;

    int32_t sx2 = contO2->x + contO2->keyx[i];
    array4[i] = (float)sx2;
    if (mad->capsized) {
      int32_t sy2 = contO2->y + cd->flipy[mad2->cn];
      array5[i] = (float)sy2 + (float)mad2->squash;
    } else {
      int32_t sy2 = contO2->y + contO2->grat;
      array5[i] = (float)sy2;
    }
    int32_t sz2 = contO2->z + contO2->keyz[i];
    array6[i] = (float)sz2;
  }
  mad_rot(mad, array, array2, (float)contO->x, (float)contO->y, contO->xy, 4);
  mad_rot(mad, array2, array3, (float)contO->y, (float)contO->z, contO->zy, 4);
  mad_rot(mad, array, array3, (float)contO->x, (float)contO->z, contO->xz, 4);
  mad_rot(mad, array4, array5, (float)contO2->x, (float)contO2->y, contO2->xy, 4);
  mad_rot(mad, array5, array6, (float)contO2->y, (float)contO2->z, contO2->zy, 4);
  mad_rot(mad, array4, array6, (float)contO2->x, (float)contO2->z, contO2->xz, 4);

  // fr(fr(imul(maxR,maxR)+imul(maxR2,maxR2))*1.5) -- no i32() around the
  // addition (unlike the sum-of-squares spots elsewhere in this port that
  // DO have one), so the two Math.imul results widen to float BEFORE
  // adding, not after -- a single float+float op, case 1 exact.
  int32_t maxRSq = contO->maxR * contO->maxR;
  int32_t maxRSq2 = contO2->maxR * contO2->maxR;
  float sqSum = (float)maxRSq + (float)maxRSq2;
  float threshold = sqSum * 1.5f;
  if (mad_rpy((float)contO->x, (float)contO2->x, (float)contO->y, (float)contO2->y, (float)contO->z, (float)contO2->z) < threshold) {
    if (!mad->caught[mad2->im] && (mad->speed != 0.0f || mad2->speed != 0.0f)) {
      float myForce = (mad->power * mad->speed) * cd->moment[mad->cn];
      float otherForce = (mad2->power * mad2->speed) * cd->moment[mad2->cn];
      if (fabsf(myForce) != fabsf(otherForce)) {
        mad->dominate[mad2->im] = fabsf(myForce) > fabsf(otherForce);
      } else if (cd->moment[mad->cn] > cd->moment[mad2->cn]) {
        mad->dominate[mad2->im] = true;
      } else {
        mad->dominate[mad2->im] = false;
      }
      mad->caught[mad2->im] = true;
    }
  } else if (mad->caught[mad2->im]) {
    mad->caught[mad2->im] = false;
  }

  int32_t n = 0;
  int32_t n2 = 0;
  if (mad->dominate[mad2->im]) {
    float dZ = ((mad->scz[0] - mad2->scz[0]) + (mad->scz[1] - mad2->scz[1])) + (mad->scz[2] - mad2->scz[2]);
    dZ = dZ + (mad->scz[3] - mad2->scz[3]);
    float dZSq = dZ * dZ;
    float dX = ((mad->scx[0] - mad2->scx[0]) + (mad->scx[1] - mad2->scx[1])) + (mad->scx[2] - mad2->scx[2]);
    dX = dX + (mad->scx[3] - mad2->scx[3]);
    float dXSq = dX * dX;
    float sumSq = dZSq + dXSq;
    float div16 = sumSq / 16.0f;
    int32_t n3b = jtrunc(div16);
    int32_t n4 = 7000;
    float n5 = 1.0f;
    if (mad->xt->multion != 0) { n4 = 28000; n5 = 1.27f; }
    // Extended (Madness.java:883-898, 928-949): hits on M A S H E E N count
    // 1.27 times in Classic Mode, and the hitter's own recoil uses its
    // moment as the other car's tables have it, capped at 3 -- which only
    // bites once a special has raised a car's strength past that.
    const bool ext = mad->xt->extended;
    const float masheen_hit = (ext && mad->xt->classicmode && mad2->cn == CAR_MASHEEN) ? 1.27f : 1.0f;
    float recoil = ext ? mad2->cd->moment[mad->cn] : cd->moment[mad->cn];
    if (ext && recoil > 3.0f) recoil = 3.0f;
    // Extended (Madness.java:867, 918): Wow Caninaro, DR Monstaa and its own
    // car 15 take no knockback from their own hits while their special runs.
    // Extended numbers them 25, 38 and 15; NFM 2's cars are its 23-38.
    const int32_t ext_cn = mad->cn < 16 ? mad->cn + 23 : mad->cn - 16;
    const bool no_knockback = ext && mad->specialact && (ext_cn == 15 || ext_cn == 25 || ext_cn == 38);
    for (int32_t j = 0; j < 4; j++) {
      for (int32_t k = 0; k < 4; k++) {
        float compradSum = cd->comprad[mad2->cn] + cd->comprad[mad->cn];
        float rpyThreshold = (float)(n3b + n4) * compradSum;
        if (mad_rpy(array[j], array4[k], array2[j], array5[k], array3[j], array6[k]) < rpyThreshold) {
          if (fabsf(mad->scx[j] * cd->moment[mad->cn]) > fabsf(mad2->scx[k] * cd->moment[mad2->cn])) {
            float n6 = mad2->scx[k] * cd->revpush[mad->cn];
            if (n6 > 300.0f) n6 = 300.0f;
            if (n6 < -300.0f) n6 = -300.0f;
            if (no_knockback) n6 = 0.0f;
            float n7 = mad->scx[j] * cd->push[mad->cn];
            if (n7 > 300.0f) n7 = 300.0f;
            if (n7 < -300.0f) n7 = -300.0f;
            mad2->scx[k] = mad2->scx[k] + n7;
            if (xt_graphics_stub_human(mad->xt, mad->im)) mad2->colidim = true;
            int32_t n9 = n + mad_regx(mad2, k, ((n7 * cd->moment[mad->cn]) * n5) * masheen_hit, contO2);
            if (mad2->colidim) mad2->colidim = false;
            mad->scx[j] = mad->scx[j] - n6;
            n2 += mad_regx(mad, j, (-n6 * recoil) * n5, contO);
            mad->scy[j] = mad->scy[j] - (float)cd->revlift[mad->cn];
            if (mad->im == mad->xt->im) mad2->colidim = true;
            n = n9 + mad_regy(mad2, k, cd->revlift[mad->cn] * 7.0f, contO2);
            if (mad2->colidim) mad2->colidim = false;
            if (medium_random(m) > medium_random(m)) {
              cont_o_sprk(contO2, (array[j] + array4[k]) / 2.0f, (array2[j] + array5[k]) / 2.0f, (array3[j] + array6[k]) / 2.0f,
                          (mad2->scx[k] + mad->scx[j]) / 4.0f, (mad2->scy[k] + mad->scy[j]) / 4.0f, (mad2->scz[k] + mad->scz[j]) / 4.0f, 2);
            }
          }
          if (fabsf(mad->scz[j] * cd->moment[mad->cn]) > fabsf(mad2->scz[k] * cd->moment[mad2->cn])) {
            float n12 = mad2->scz[k] * cd->revpush[mad->cn];
            if (n12 > 300.0f) n12 = 300.0f;
            if (n12 < -300.0f) n12 = -300.0f;
            if (no_knockback) n12 = 0.0f;
            float n13 = mad->scz[j] * cd->push[mad->cn];
            if (n13 > 300.0f) n13 = 300.0f;
            if (n13 < -300.0f) n13 = -300.0f;
            mad2->scz[k] = mad2->scz[k] + n13;
            if (mad->im == mad->xt->im) mad2->colidim = true;
            int32_t n15 = n + mad_regz(mad2, k, ((n13 * cd->moment[mad->cn]) * n5) * masheen_hit, contO2);
            if (mad2->colidim) mad2->colidim = false;
            mad->scz[j] = mad->scz[j] - n12;
            n2 += mad_regz(mad, j, (-n12 * recoil) * n5, contO);
            mad->scy[j] = mad->scy[j] - (float)cd->revlift[mad->cn];
            if (mad->im == mad->xt->im) mad2->colidim = true;
            n = n15 + mad_regy(mad2, k, cd->revlift[mad->cn] * 7.0f, contO2);
            if (mad2->colidim) mad2->colidim = false;
            if (medium_random(m) > medium_random(m)) {
              cont_o_sprk(contO2, (array[j] + array4[k]) / 2.0f, (array2[j] + array5[k]) / 2.0f, (array3[j] + array6[k]) / 2.0f,
                          (mad2->scx[k] + mad->scx[j]) / 4.0f, (mad2->scy[k] + mad->scy[j]) / 4.0f, (mad2->scz[k] + mad->scz[j]) / 4.0f, 2);
            }
          }
          if (xt_graphics_stub_human(mad->xt, mad->im)) mad2->lastcolido = 70;
          mad2->lastcolider = mad->im;
          if (xt_graphics_stub_human(mad->xt, mad2->im)) mad->lastcolido = 70;
          mad2->scy[k] = mad2->scy[k] - (float)cd->lift[mad->cn];
        }
      }
    }
  }
  if (mad->xt->multion == 1) {
    if (mad2->im == mad->xt->im && n != 0) mad->xt->dcrashes[mad->im] += n;
    if (mad->im == mad->xt->im && n2 != 0) mad->xt->dcrashes[mad2->im] += n2;
  }
}

// Gear thresholds and the top-speed clamp. NFM 2 writes `swits / 2 + power *
// swits / 196.0f` with an int half (185/2 is 92); Extended's Madness copies
// swits into a float first (Madness.java:1677-1702), so the half is 92.5.
static float swit_speed(bool ext, float power, int32_t swits) {
  const float half = ext ? (float)swits / 2.0f : (float)(swits / 2);
  return half + power * (float)swits / 196.0f;
}

// `acelf / 2.0f + power * acelf / 196.0f`, all float (acelf is float).
static float acel_step(float power, float acelf) {
  return acelf / 2.0f + power * acelf / 196.0f;
}

// Extended's special bar, each tick (Madness.java:3074-3151, outside career):
// it creeps up from the last landed stunt, a full one (120) can be fired,
// and a running special drains speclast at 343000 / 2500000 a tick (about
// 875 ticks); when it is spent the bar empties.
static void mad_special_tick(Mad *mad, Control *control) {
  if (mad->im > 0 && mad->powerup <= 100.0f) mad->spatk += mad->powerup / 500.0f;
  else mad->spatk += mad->powerup / 3500.0f;
  if (control->spatk && mad->spatk < 120.0f) control->spatk = false;
  if (mad->spatk > 120.0f) mad->spatk = 120.0f;
  if (mad->spatk < 0.0f) mad->spatk = 0.0f;
  const float drain = 343000.0f / 2500000.0f;
  if ((control->spatk && mad->speclast <= 120.0f && mad->spatk == 120.0f) ||
      (!control->spatk && mad->speclast != 120.0f && mad->spatk == 120.0f)) {
    mad->speclast -= drain;
    mad->speclast2 -= drain;
  }
  if (mad->speclast2 < 0.0f) mad->speclast2 = 0.0f;
  if (mad->speclast > 120.0f) mad->speclast = 120.0f;
  if (mad->speclast < 0.0f) mad->speclast = 0.0f;
  if (mad->speclast == 0.0f && mad->spatk == 120.0f) {
    mad->spatk = 0.0f;
    mad->specialact = false;
  }
  if (mad->speclast == 0.0f && mad->spatk == 0.0f) {
    mad->speclast = 120.0f;
    mad->speclast2 = 120.0f;
  }
}

void mad_drive(Mad *mad, Control *control, ContO *contO, Trackers *trackers, CheckPoints *checkPoints) {
  CarDefine *cd = mad->cd;
  Medium *m = mad->m;
  // The car's pitch and roll as this tick starts, 0..360 (Extended,
  // Madness.java:1322-1325), for the in-air righting below.
  int32_t zyangle, xyangle;
  for (zyangle = abs(mad->pzy); zyangle > 360; zyangle -= 360) {}
  for (xyangle = abs(mad->pxy); xyangle > 360; xyangle -= 360) {}

  int32_t n = 1;
  int32_t n2 = 1;
  bool zyinv = false;
  bool b = false;
  bool b2 = false;
  mad->capsized = false;
  mad->just_fixed = false;
  int32_t i;
  for (i = abs(mad->pzy); i > 270; i -= 360) {}
  if (abs(i) > 90) zyinv = true;
  int32_t n3 = 0;
  int32_t j;
  for (j = abs(mad->pxy); j > 270; j -= 360) {}
  if (abs(j) > 90) { n3 = 1; n2 = -1; }
  int32_t grat = contO->grat;
  if (zyinv) {
    if (n3 != 0) { n3 = 0; b = true; }
    else { n3 = 1; mad->capsized = true; }
    n = -1;
  } else if (n3 != 0) {
    mad->capsized = true;
  }
  if (mad->capsized) {
    grat = cd->flipy[mad->cn] + mad->squash; // i32(): plain int32_t add wraps under -fwrapv
  }
  control->zyinv = zyinv;
  float n4 = 0.0f, n5 = 0.0f, n6 = 0.0f;
  if (mad->mtouch) mad->loop = 0;
  if (mad->wtouch) {
    if (mad->loop == 2 || mad->loop == -1) {
      mad->loop = -1;
      if (control->left) mad->pl = true;
      if (control->right) mad->pr = true;
      if (control->up) mad->pu = true;
      if (control->down) mad->pd = true;
    }
    mad->ucomp = 0.0f;
    mad->dcomp = 0.0f;
    mad->lcomp = 0.0f;
    mad->rcomp = 0.0f;
  }
  if (control->handb) {
    if (!mad->pushed) {
      if (!mad->wtouch) {
        if (mad->loop == 0) mad->loop = 1;
      } else if (mad->gtouch) {
        mad->pushed = true;
      }
    }
  } else {
    mad->pushed = false;
  }
  if (mad->loop == 1) {
    // fr(fr(fr(fr(A+B)+C)+D)/4.0) -- chained single-op fr()s (case 1) all
    // the way, including the final /4.0 (dividing an already-native float
    // sum by an exact literal).
    float n7 = ((mad->scy[0] + mad->scy[1]) + mad->scy[2] + mad->scy[3]) / 4.0f;
    for (int32_t k = 0; k < 4; k++) mad->scy[k] = n7;
    mad->loop = 2;
  }
  if (!mad->dest) {
    if (mad->loop == 2) {
      if (control->up) {
        if (mad->ucomp == 0.0f) {
          mad->ucomp = 10.0f + (mad->scy[0] + 50.0f) / 20.0f;
          if (mad->ucomp < 5.0f) mad->ucomp = 5.0f;
          if (mad->ucomp > 10.0f) mad->ucomp = 10.0f;
          mad->ucomp = mad->ucomp * cd->airs[mad->cn];
        }
        if (mad->ucomp < 20.0f) mad->ucomp = mad->ucomp + 0.5f * cd->airs[mad->cn];
        n4 = (-cd->airc[mad->cn] * medium_sin(m, (float)contO->xz)) * (float)n2;
        n5 = (cd->airc[mad->cn] * medium_cos(m, (float)contO->xz)) * (float)n2;
      } else if (mad->ucomp != 0.0f && mad->ucomp > -2.0f) {
        mad->ucomp = mad->ucomp - 0.5f * cd->airs[mad->cn];
      }
      if (control->down) {
        if (mad->dcomp == 0.0f) {
          mad->dcomp = 10.0f + (mad->scy[0] + 50.0f) / 20.0f;
          if (mad->dcomp < 5.0f) mad->dcomp = 5.0f;
          if (mad->dcomp > 10.0f) mad->dcomp = 10.0f;
          mad->dcomp = mad->dcomp * cd->airs[mad->cn];
        }
        if (mad->dcomp < 20.0f) mad->dcomp = mad->dcomp + 0.5f * cd->airs[mad->cn];
        n6 = -(float)cd->airc[mad->cn];
      } else if (mad->dcomp != 0.0f && mad->ucomp > -2.0f) {
        mad->dcomp = mad->dcomp - 0.5f * cd->airs[mad->cn];
      }
      if (control->left) {
        if (mad->lcomp == 0.0f) mad->lcomp = 5.0f;
        if (mad->lcomp < 20.0f) mad->lcomp = mad->lcomp + 2.0f * cd->airs[mad->cn];
        n4 = (-cd->airc[mad->cn] * medium_cos(m, (float)contO->xz)) * (float)n;
        n5 = (-cd->airc[mad->cn] * medium_sin(m, (float)contO->xz)) * (float)n;
      } else if (mad->lcomp > 0.0f) {
        mad->lcomp = mad->lcomp - 2.0f * cd->airs[mad->cn];
      }
      if (control->right) {
        if (mad->rcomp == 0.0f) mad->rcomp = 5.0f;
        if (mad->rcomp < 20.0f) mad->rcomp = mad->rcomp + 2.0f * cd->airs[mad->cn];
        n4 = (cd->airc[mad->cn] * medium_cos(m, (float)contO->xz)) * (float)n;
        n5 = (cd->airc[mad->cn] * medium_sin(m, (float)contO->xz)) * (float)n;
      } else if (mad->rcomp > 0.0f) {
        mad->rcomp = mad->rcomp - 2.0f * cd->airs[mad->cn];
      }
      mad->pzy = jtrunc((float)mad->pzy + (mad->dcomp - mad->ucomp) * medium_cos(m, (float)mad->pxy));
      if (zyinv) {
        contO->xz = jtrunc((float)contO->xz + (mad->dcomp - mad->ucomp) * medium_sin(m, (float)mad->pxy));
      } else {
        contO->xz = jtrunc((float)contO->xz - (mad->dcomp - mad->ucomp) * medium_sin(m, (float)mad->pxy));
      }
      mad->pxy = jtrunc((float)mad->pxy + (mad->rcomp - mad->lcomp));
    } else {
      float power = mad->power;
      if (power < 40.0f) power = 40.0f;
      // Extended (Madness.java:1587-1608, carried from the older NFM 2): the
      // player's power counts for 0.76 of itself until it is full, 98. The
      // AI's does not. (Career stat points scale it back up; not ported.)
      const bool ext = mad->xt->extended;
      if (ext && mad->im == 0 && mad->power != 98.0f) power = (float)((double)power * 0.76);
      if (control->down) {
        if (mad->speed > 0.0f) {
          // Java: `speed -= handb / 2` -- int division, 7/2 is 3; Extended's
          // float handb makes it 3.5.
          mad->speed = mad->speed - (ext ? (float)cd->handb[mad->cn] / 2.0f : (float)(cd->handb[mad->cn] / 2));
        } else {
          int32_t n8 = 0;
          for (int32_t l = 0; l < 2; l++) {
            if (mad->speed <= -swit_speed(ext, power, cd->swits[mad->cn][l])) n8++;
          }
          if (n8 != 2) {
            mad->speed = mad->speed - acel_step(power, cd->acelf[mad->cn][n8]);
          } else {
            mad->speed = -swit_speed(ext, power, cd->swits[mad->cn][1]);
          }
        }
      }
      if (control->up) {
        if (mad->speed < 0.0f) {
          mad->speed = mad->speed + (float)cd->handb[mad->cn];
        } else {
          int32_t n9 = 0;
          for (int32_t n10 = 0; n10 < 3; n10++) {
            if (mad->speed >= swit_speed(ext, power, cd->swits[mad->cn][n10])) n9++;
          }
          if (n9 != 3) {
            mad->speed = mad->speed + acel_step(power, cd->acelf[mad->cn][n9]);
          } else {
            mad->speed = swit_speed(ext, power, cd->swits[mad->cn][2]);
          }
        }
      }
      if (control->handb && fabsf(mad->speed) > (float)cd->handb[mad->cn]) {
        if (mad->speed < 0.0f) mad->speed = mad->speed + (float)cd->handb[mad->cn];
        else mad->speed = mad->speed - (float)cd->handb[mad->cn];
      }
      if (mad->loop == -1 && contO->y < 100) {
        if (control->left) {
          if (!mad->pl) {
            if (mad->lcomp == 0.0f) mad->lcomp = 5.0f * cd->airs[mad->cn];
            if (mad->lcomp < 20.0f) mad->lcomp = mad->lcomp + 2.0f * cd->airs[mad->cn];
          }
        } else {
          if (mad->lcomp > 0.0f) mad->lcomp = mad->lcomp - 2.0f * cd->airs[mad->cn];
          mad->pl = false;
        }
        if (control->right) {
          if (!mad->pr) {
            if (mad->rcomp == 0.0f) mad->rcomp = 5.0f * cd->airs[mad->cn];
            if (mad->rcomp < 20.0f) mad->rcomp = mad->rcomp + 2.0f * cd->airs[mad->cn];
          }
        } else {
          if (mad->rcomp > 0.0f) mad->rcomp = mad->rcomp - 2.0f * cd->airs[mad->cn];
          mad->pr = false;
        }
        if (control->up) {
          if (!mad->pu) {
            if (mad->ucomp == 0.0f) mad->ucomp = 5.0f * cd->airs[mad->cn];
            if (mad->ucomp < 20.0f) mad->ucomp = mad->ucomp + 2.0f * cd->airs[mad->cn];
          }
        } else {
          if (mad->ucomp > 0.0f) mad->ucomp = mad->ucomp - 2.0f * cd->airs[mad->cn];
          mad->pu = false;
        }
        if (control->down) {
          if (!mad->pd) {
            if (mad->dcomp == 0.0f) mad->dcomp = 5.0f * cd->airs[mad->cn];
            if (mad->dcomp < 20.0f) mad->dcomp = mad->dcomp + 2.0f * cd->airs[mad->cn];
          }
        } else {
          if (mad->dcomp > 0.0f) mad->dcomp = mad->dcomp - 2.0f * cd->airs[mad->cn];
          mad->pd = false;
        }
        mad->pzy = jtrunc((float)mad->pzy + (mad->dcomp - mad->ucomp) * medium_cos(m, (float)mad->pxy));
        if (zyinv) {
          contO->xz = jtrunc((float)contO->xz + (mad->dcomp - mad->ucomp) * medium_sin(m, (float)mad->pxy));
        } else {
          contO->xz = jtrunc((float)contO->xz - (mad->dcomp - mad->ucomp) * medium_sin(m, (float)mad->pxy));
        }
        mad->pxy = jtrunc((float)mad->pxy + (mad->rcomp - mad->lcomp));
      }
    }

    // Touch-trick (mobile drag gesture): JS-native code, not transpiled
    // from the original Java (no fr() anywhere in this block -- genuinely
    // double throughout, matching the JS's own trunc() calls with
    // jtrunc_d). Unreachable for a gamepad-only session (control->touchTrick
    // stays false unless something wires Vita touchscreen input), so only
    // the trivial else-branch below actually fires today; kept in full for
    // when that wiring happens.
    if (control->touchTrick && !mad->trickDisableUntilLift) {
      if (!mad->wasTouchTrick) {
        mad->basePzy = mad->pzy;
        mad->basePxy = mad->pxy;
        mad->baseXz = contO->xz;
        mad->baseTouchX = control->touchTrickX;
        mad->baseTouchY = control->touchTrickY;
        mad->lastDeltaX = 0.0;
        mad->lastDeltaY = 0.0;
        mad->wasTouchTrick = true;
      }
      double deltaY = (double)(control->touchTrickY - mad->baseTouchY) * 0.8;
      double deltaX = (double)(control->touchTrickX - mad->baseTouchX) * 0.8;
      if (fabs(deltaX) > fabs(deltaY)) deltaY = 0.0;
      else deltaX = 0.0;
      double frameDeltaY = deltaY - mad->lastDeltaY;
      double frameDeltaX = deltaX - mad->lastDeltaX;
      mad->lastDeltaY = deltaY;
      mad->lastDeltaX = deltaX;
      mad->pxy = jtrunc_d((double)mad->basePxy - deltaX);
      mad->pzy = jtrunc_d((double)mad->basePzy + deltaY * (double)medium_cos(m, (float)mad->pxy));
      if (zyinv) {
        contO->xz = jtrunc_d((double)mad->baseXz + deltaY * (double)medium_sin(m, (float)mad->pxy));
      } else {
        contO->xz = jtrunc_d((double)mad->baseXz - deltaY * (double)medium_sin(m, (float)mad->pxy));
      }
      mad->travxy = jtrunc_d((double)mad->travxy - frameDeltaX);
      mad->travzy = jtrunc_d((double)mad->travzy - frameDeltaY);
      mad->loop = 2;
      mad->lcomp = 0.0f;
      mad->rcomp = 0.0f;
      mad->ucomp = 0.0f;
      mad->dcomp = 0.0f;
    } else {
      mad->wasTouchTrick = false;
      if (!control->touchTrick) mad->trickDisableUntilLift = false;
    }
  } else {
    mad->wasTouchTrick = false;
    if (control->touchTrick) mad->trickDisableUntilLift = true;
    else mad->trickDisableUntilLift = false;
  }

  float n11 = (20.0f * mad->speed) / (154.0f * cd->simag[mad->cn]);
  if (n11 > 20.0f) n11 = 20.0f;
  contO->wzy = jtrunc((float)contO->wzy - n11);
  if (contO->wzy < -30) contO->wzy += 30;
  if (contO->wzy > 30) contO->wzy -= 30;
  // Extended keeps the wheels' angle as a double (Madness.java:1826-1861):
  // its own cars turn by 7.5, 4.5, 3.5 a tick. contO->wxz, which the
  // drawing reads, follows it truncated; anything else that sets
  // contO->wxz (a reset) is picked up.
  const bool ext_steer = mad->xt->extended;
  if (ext_steer && (int32_t)mad->wxzd != contO->wxz) mad->wxzd = contO->wxz;
  if (control->steer != 0.0f) {
    contO->wxz = jtrunc(-36.0f * control->steer);
    mad->wxzd = contO->wxz;
  } else if (ext_steer) {
    const double tp = cd->turn[mad->cn];
    double w = mad->wxzd;
    if (control->right) {
      w -= tp;
      if (w < -36.0) w = -36.0;
    }
    if (control->left) {
      w += tp;
      if (w > 36.0) w = 36.0;
    }
    if (w != 0.0 && !control->left && !control->right) {
      if (fabsf(mad->speed) < 10.0f) {
        if (fabs(w) == 1.0) w = 0.0;
        if (w > 0.0) --w;
        if (w < 0.0) ++w;
      } else {
        if (fabs(w) < tp * 2.0) w = 0.0;
        if (w > 0.0) w -= tp * 2.0;
        if (w < 0.0) w += tp * 2.0;
      }
    }
    mad->wxzd = w;
    contO->wxz = (int32_t)w;
  } else {
    if (control->right) {
      contO->wxz -= cd->turn[mad->cn];
      if (contO->wxz < -36) contO->wxz = -36;
    }
    if (control->left) {
      contO->wxz += cd->turn[mad->cn];
      if (contO->wxz > 36) contO->wxz = 36;
    }
    if (contO->wxz != 0 && !control->left && !control->right) {
      if (fabsf(mad->speed) < 10.0f) {
        if (abs(contO->wxz) == 1) contO->wxz = 0;
        if (contO->wxz > 0) --contO->wxz;
        if (contO->wxz < 0) ++contO->wxz;
      } else {
        if (abs(contO->wxz) < cd->turn[mad->cn] * 2) contO->wxz = 0;
        if (contO->wxz > 0) contO->wxz -= cd->turn[mad->cn] * 2;
        if (contO->wxz < 0) contO->wxz += cd->turn[mad->cn] * 2;
      }
    }
  }

  // trunc(3600.0/(speed*speed)) -- no fr() at all, genuinely double
  // (matches the JS's own trunc(), so jtrunc_d; also saturates correctly
  // when speed is 0.0 and this divides by zero, same as jtrunc/jtrunc_d
  // elsewhere in this port).
  int32_t n12 = jtrunc_d(3600.0 / ((double)mad->speed * (double)mad->speed));
  if (n12 < 5) n12 = 5;
  if (mad->speed < 0.0f) n12 = -n12;
  if (mad->wtouch) {
    if (!mad->capsized) {
      if (ext_steer) {
        // Extended: float divisions of the double angle (:1873-1877).
        const float w = (float)mad->wxzd, d = (float)n12;
        mad->fxz = control->handb ? (int32_t)(w / d) : (int32_t)(w / (d * 3.0f));
        contO->xz += (int32_t)(w / d);
      } else {
      if (!control->handb) {
        mad->fxz = contO->wxz / (n12 * 3);
      } else {
        mad->fxz = contO->wxz / n12;
      }
      contO->xz += contO->wxz / n12;
      }
    }
    mad->wtouch = false;
    mad->gtouch = false;
  } else {
    contO->xz += mad->fxz;
  }
  if (mad->speed > 30.0f || mad->speed < -100.0f) {
    while (abs(mad->mxz - mad->cxz) > 180) {
      if (mad->cxz > mad->mxz) {
        mad->cxz -= 360;
      } else {
        if (mad->cxz >= mad->mxz) continue;
        mad->cxz += 360;
      }
    }
    if (abs(mad->mxz - mad->cxz) < 30) {
      int32_t diff = mad->mxz - mad->cxz;
      float inner = (float)diff / 4.0f;
      mad->cxz = jtrunc((float)mad->cxz + inner);
    } else {
      if (mad->cxz > mad->mxz) mad->cxz -= 10;
      if (mad->cxz < mad->mxz) mad->cxz += 10;
    }
  }

  float array[4], array2[4], array3[4];
  for (int32_t n13 = 0; n13 < 4; n13++) {
    int32_t sumKX = contO->keyx[n13] + contO->x;
    array[n13] = (float)sumKX;
    int32_t sumGY = grat + contO->y;
    array3[n13] = (float)sumGY;
    int32_t sumZK = contO->z + contO->keyz[n13];
    array2[n13] = (float)sumZK;
    mad->scy[n13] = mad->scy[n13] + 7.0f;
  }
  mad_rot(mad, array, array3, (float)contO->x, (float)contO->y, mad->pxy, 4);
  mad_rot(mad, array3, array2, (float)contO->y, (float)contO->z, mad->pzy, 4);
  mad_rot(mad, array, array2, (float)contO->x, (float)contO->z, contO->xz, 4);

  bool b3 = false;
  // trunc(fr(fr(fr(A+B)+C)+D) / 4.0) -- the /4.0 is OUTSIDE the fr() chain
  // (unwrapped), but division by 4.0 is exact for any finite float
  // (power-of-two divisor), so native float division still matches the
  // JS's double division bit-for-bit; no double detour needed.
  float sumScx = ((mad->scx[0] + mad->scx[1]) + mad->scx[2]) + mad->scx[3];
  int32_t n15 = jtrunc(sumScx / 4.0f);
  float sumScz = ((mad->scz[0] + mad->scz[1]) + mad->scz[2]) + mad->scz[3];
  int32_t n16 = jtrunc(sumScz / 4.0f);
  for (int32_t n17 = 0; n17 < 4; n17++) {
    if (mad->scx[n17] - n15 > 200.0f) mad->scx[n17] = (float)(200 + n15);
    if (mad->scx[n17] - n15 < -200.0f) mad->scx[n17] = (float)(n15 - 200);
    if (mad->scz[n17] - n16 > 200.0f) mad->scz[n17] = (float)(200 + n16);
    if (mad->scz[n17] - n16 < -200.0f) mad->scz[n17] = (float)(n16 - 200);
  }
  // The move uses the wheel speeds AFTER the clamp above: Java re-adds
  // scx[0..3] here. Reusing the pre-clamp sums moved a car that had just
  // been hit by its unclamped wheel speeds -- after a head-on hit, straight
  // on into (and through) the other car.
  sumScx = ((mad->scx[0] + mad->scx[1]) + mad->scx[2]) + mad->scx[3];
  sumScz = ((mad->scz[0] + mad->scz[1]) + mad->scz[2]) + mad->scz[3];
  for (int32_t n18 = 0; n18 < 4; n18++) {
    array3[n18] = array3[n18] + mad->scy[n18];
    array[n18] = array[n18] + (sumScx / 4.0f);
    array2[n18] = array2[n18] + (sumScz / 4.0f);
  }
  int32_t ncx = (contO->x - trackers->sx) / 3000;
  if (ncx > trackers->ncx) ncx = trackers->ncx;
  if (ncx < 0) ncx = 0;
  int32_t ncz = (contO->z - trackers->sz) / 3000;
  if (ncz > trackers->ncz) ncz = trackers->ncz;
  if (ncz < 0) ncz = 0;
  int32_t n22 = 1;
  for (int32_t n23 = 0; n23 < trackers->sect_len[ncx][ncz]; n23++) {
    int32_t n24 = trackers->sect[ncx][ncz][n23];
    if (abs(trackers->zy[n24]) != 90 && abs(trackers->xy[n24]) != 90 &&
        abs(contO->x - trackers->x[n24]) < trackers->radx[n24] &&
        abs(contO->z - trackers->z[n24]) < trackers->radz[n24] &&
        (!trackers->decor[n24] || m->resdown != 2 || mad->xt->multion != 0)) {
      n22 = trackers->skd[n24];
    }
  }

  if (mad->mtouch) {
    // fr(grip - fr(fr(abs(txz-xz)*speed)/250.0)) -- inner fr(diff*speed)
    // case 1 (int*float), fr(A/250.0) case 1 (float/double-literal is
    // still a single native-float op), outer fr(grip-B) case 1.
    int32_t diffTxz = abs(mad->txz - contO->xz);
    float aTerm = (float)diffTxz * mad->speed;
    float bTerm = aTerm / 250.0f;
    float n25 = (float)cd->grip[mad->cn] - bTerm;
    if (control->handb) {
      float handbTerm = (float)abs(mad->txz - contO->xz) * 4.0f;
      n25 = n25 - handbTerm;
    }
    if (n25 < cd->grip[mad->cn]) {
      if (mad->skid != 2) mad->skid = 1;
      mad->speed = mad->speed - mad->speed / 100.0f;
    } else if (mad->skid == 1) {
      mad->skid = 2;
    }
    if (n22 == 1) n25 = n25 * 0.75f;
    if (n22 == 2) n25 = n25 * 0.55f;
    int32_t n26 = jtrunc(-mad->speed * (medium_sin(m, (float)contO->xz) * medium_cos(m, (float)mad->pzy)));
    int32_t n27 = jtrunc(mad->speed * (medium_cos(m, (float)contO->xz) * medium_cos(m, (float)mad->pzy)));
    int32_t n28 = jtrunc(-mad->speed * medium_sin(m, (float)mad->pzy));
    if (mad->capsized || mad->dest || checkPoints->haltall) {
      n26 = 0; n27 = 0; n28 = 0;
      n25 = (float)cd->grip[mad->cn] / 5.0f;
      if (mad->speed > 0.0f) mad->speed = mad->speed - 2.0f;
      else mad->speed = mad->speed + 2.0f;
    }
    if (fabsf(mad->speed) > mad->drag) {
      if (mad->speed > 0.0f) mad->speed = mad->speed - mad->drag;
      else mad->speed = mad->speed + mad->drag;
    } else {
      mad->speed = 0.0f;
    }
    if (mad->cn == 8 && n25 < 5.0f) n25 = 5.0f;
    if (n25 < 1.0f) n25 = 1.0f;
    float n29 = 0.0f;
    float n30 = 0.0f;
    for (int32_t n31 = 0; n31 < 4; n31++) {
      if (fabsf(mad->scx[n31] - n26) > n25) {
        if (mad->scx[n31] < n26) mad->scx[n31] = mad->scx[n31] + n25;
        else mad->scx[n31] = mad->scx[n31] - n25;
      } else {
        mad->scx[n31] = (float)n26;
      }
      if (fabsf(mad->scz[n31] - n27) > n25) {
        if (mad->scz[n31] < n27) mad->scz[n31] = mad->scz[n31] + n25;
        else mad->scz[n31] = mad->scz[n31] - n25;
      } else {
        mad->scz[n31] = (float)n27;
      }
      if (fabsf(mad->scy[n31] - n28) > n25) {
        if (mad->scy[n31] < n28) mad->scy[n31] = mad->scy[n31] + n25;
        else mad->scy[n31] = mad->scy[n31] - n25;
      } else {
        mad->scy[n31] = (float)n28;
      }
      if (n25 < cd->grip[mad->cn]) {
        if (mad->txz != contO->xz) mad->dcnt++;
        else if (mad->dcnt != 0) mad->dcnt = 0;
        // fr(40.0*n25) case 1, then /grip is a raw (unwrapped) division
        // between two already-float values -- native float division, no
        // detour needed.
        float dcntThreshold = (40.0f * n25) / cd->grip[mad->cn];
        if ((float)mad->dcnt > dcntThreshold || mad->capsized) {
          float n38 = 1.0f;
          if (n22 != 0) n38 = 1.2f;
          if (medium_random(m) > 0.65f) {
            cont_o_dust(contO, n31, array[n31], array3[n31], array2[n31], jtrunc(mad->scx[n31]), jtrunc(mad->scz[n31]),
                        n38 * cd->simag[mad->cn], jtrunc(mad->tilt), mad->capsized && mad->mtouch);
            if (mad->im == mad->xt->im && !mad->capsized) {
              // fr(sqrt(scx*scx+scz*scz)) -- sqrt isn't case-1 exact even
              // though its argument is, so double-then-round-once.
              float scxSq = mad->scx[n31] * mad->scx[n31];
              float sczSq = mad->scz[n31] * mad->scz[n31];
              float mag = (float)sqrt((double)(scxSq + sczSq));
              xt_graphics_stub_skid(mad->xt, n22, mag);
            }
          }
        } else {
          if (n22 == 1 && medium_random(m) > 0.8f) {
            cont_o_dust(contO, n31, array[n31], array3[n31], array2[n31], jtrunc(mad->scx[n31]), jtrunc(mad->scz[n31]),
                        1.1f * cd->simag[mad->cn], jtrunc(mad->tilt), mad->capsized && mad->mtouch);
          }
          if ((n22 == 2 || n22 == 3) && medium_random(m) > 0.6f) {
            cont_o_dust(contO, n31, array[n31], array3[n31], array2[n31], jtrunc(mad->scx[n31]), jtrunc(mad->scz[n31]),
                        1.15f * cd->simag[mad->cn], jtrunc(mad->tilt), mad->capsized && mad->mtouch);
          }
        }
      } else if (mad->dcnt != 0) {
        mad->dcnt -= 2;
        if (mad->dcnt < 0) mad->dcnt = 0;
      }
      if ((n22 == 3 || n22 == 4) && !mad->xt->extended) {
        // trunc(rand*4.0) -- no fr(), genuinely double.
        int32_t idx = jtrunc_d((double)medium_random(m) * 4.0);
        // Java: (float)(-100.0f * random() * (speed / swits) * (bounce - 0.3))
        const float base = (n22 == 3) ? -100.0f : -150.0f;
        const float a1 = (base * medium_random(m)) * (mad->speed / (float)cd->swits[mad->cn][2]);
        mad->scy[idx] = (float)((double)a1 * ((double)cd->bounce[mad->cn] - 0.3));
      } else if (n22 == 3 || n22 == 4) {
        // Bumpy road. Extended (Madness.java:2148-2167, 1354-1356) kicks a
        // wheel only while the tyres grip (NFM 2: every wheel, every tick),
        // with one random number for the wheel and the kick, and bounce
        // capped at 1.35. Java: (float)(-100.0f * r * (speed / swits) *
        // (bounciness - 0.3)).
        if (!(n25 < cd->grip[mad->cn])) {
          const float r = medium_random(m);
          const int32_t idx = (int32_t)(r * 4.0f);
          float bounciness = cd->bounce[mad->cn];
          if (bounciness > 1.35f) bounciness = 1.35f;
          const float base = (n22 == 3) ? -100.0f : -150.0f;
          const float a1 = (base * r) * (mad->speed / (float)cd->swits[mad->cn][2]);
          mad->scy[idx] = (float)((double)a1 * ((double)bounciness - 0.3));
        }
      }
      n29 = n29 + mad->scx[n31];
      n30 = n30 + mad->scz[n31];
    }
    mad->txz = contO->xz;
    int32_t n39 = (n29 > 0.0f) ? -1 : 1;
    // Java: (int)(Math.acos(n30 / Math.sqrt(n29 * n29 + n30 * n30)) /
    // 0.017453292519943295 * n39) -- the squares and their sum are float,
    // everything from the sqrt on is double, rounded nowhere before the
    // (int). Straight down -z this is 179.99999999999997 -> 179; rounding it
    // through float first made it 180.
    const float sumSq = n29 * n29 + n30 * n30;
    mad->mxz = jtrunc_d(acos((double)n30 / sqrt((double)sumSq)) / 0.017453292519943295 * (double)n39);
    if (mad->skid == 2) {
      if (!mad->capsized) {
        n29 = n29 / 4.0f;
        n30 = n30 / 4.0f;
        float sqrtMag = (float)sqrt((double)(n29 * n29 + n30 * n30));
        float cosTerm = medium_cos(m, (float)(mad->mxz - contO->xz));
        float speedMag = sqrtMag * cosTerm;
        mad->speed = b ? -speedMag : speedMag;
      }
      mad->skid = 0;
    }
    if (mad->capsized && n29 == 0.0f && n30 == 0.0f) n22 = 0;
    mad->mtouch = false;
    b3 = true;
  } else if (mad->skid != 2) {
    mad->skid = 2;
  }

  int32_t n40 = 0;
  bool array7[4], array8[4], array9[4];
  float n41 = 0.0f;
  for (int32_t n42 = 0; n42 < 4; n42++) {
    array9[n42] = false;
    array8[n42] = false;
    if (array3[n42] > 245.0f) {
      n40++;
      mad->wtouch = true;
      mad->gtouch = true;
      if (!b3 && mad->scy[n42] != 7.0f) {
        float n43 = mad->scy[n42] / 333.33f;
        if (n43 > 0.3f) n43 = 0.3f;
        float n44;
        if (n22 == 0) n44 = n43 + 1.1f;
        else n44 = n43 + 1.2f;
        cont_o_dust(contO, n42, array[n42], array3[n42], array2[n42], jtrunc(mad->scx[n42]), jtrunc(mad->scz[n42]),
                    n44 * cd->simag[mad->cn], 0, mad->capsized && mad->mtouch);
      }
      array3[n42] = 250.0f;
      array9[n42] = true;
      n41 = n41 + (array3[n42] - 250.0f);
      float n45 = (fabsf(medium_sin(m, (float)mad->pxy)) + fabsf(medium_sin(m, (float)mad->pzy))) / 3.0f;
      if (n45 > 0.4f) n45 = 0.4f;
      float n46 = n45 + cd->bounce[mad->cn];
      if (n46 < 1.1f) n46 = 1.1f;
      mad_regy(mad, n42, fabsf(mad->scy[n42] * n46), contO);
      if (mad->scy[n42] > 0.0f) mad->scy[n42] = mad->scy[n42] - fabsf(mad->scy[n42] * n46);
      if (mad->capsized) array8[n42] = true;
    }
    array7[n42] = false;
  }
  if (n40 != 0) {
    float n48 = n41 / (float)n40;
    for (int32_t n49 = 0; n49 < 4; n49++) {
      if (!array9[n49]) array3[n49] = array3[n49] - n48;
    }
  }

  int32_t n51 = 0;
  for (int32_t n52 = 0; n52 < trackers->sect_len[ncx][ncz]; n52++) {
    int32_t n53 = trackers->sect[ncx][ncz][n52];
    int32_t n54 = 0;
    int32_t n55 = 0;
    for (int32_t n56 = 0; n56 < 4; n56++) {
      if (array8[n56] && (trackers->skd[n53] == 0 || trackers->skd[n53] == 1) &&
          array[n56] > trackers->x[n53] - trackers->radx[n53] && array[n56] < trackers->x[n53] + trackers->radx[n53] &&
          array2[n56] > trackers->z[n53] - trackers->radz[n53] && array2[n56] < trackers->z[n53] + trackers->radz[n53]) {
        cont_o_sprk(contO, array[n56], array3[n56], array2[n56], mad->scx[n56], mad->scy[n56], mad->scz[n56], 1);
        if (mad->im == mad->xt->im) {
          xt_graphics_stub_gscrape(mad->xt, jtrunc(mad->scx[n56]), jtrunc(mad->scy[n56]), jtrunc(mad->scz[n56]));
        }
      }
      if (!array7[n56] &&
          array[n56] > trackers->x[n53] - trackers->radx[n53] && array[n56] < trackers->x[n53] + trackers->radx[n53] &&
          array2[n56] > trackers->z[n53] - trackers->radz[n53] && array2[n56] < trackers->z[n53] + trackers->radz[n53] &&
          array3[n56] > trackers->y[n53] - trackers->rady[n53] && array3[n56] < trackers->y[n53] + trackers->rady[n53] &&
          (!trackers->decor[n53] || m->resdown != 2 || mad->xt->multion != 0)) {
        if (trackers->xy[n53] == 0 && trackers->zy[n53] == 0 && trackers->y[n53] != 250 && array3[n56] > trackers->y[n53] - 5) {
          n55++;
          mad->wtouch = true;
          mad->gtouch = true;
          if (!b3 && mad->scy[n56] != 7.0f) {
            float n57 = mad->scy[n56] / 333.33f;
            if (n57 > 0.3f) n57 = 0.3f;
            float n58;
            if (n22 == 0) n58 = n57 + 1.1f;
            else n58 = n57 + 1.2f;
            cont_o_dust(contO, n56, array[n56], array3[n56], array2[n56], jtrunc(mad->scx[n56]), jtrunc(mad->scz[n56]),
                        n58 * cd->simag[mad->cn], 0, mad->capsized && mad->mtouch);
          }
          array3[n56] = (float)trackers->y[n53];
          if (mad->capsized && (trackers->skd[n53] == 0 || trackers->skd[n53] == 1)) {
            cont_o_sprk(contO, array[n56], array3[n56], array2[n56], mad->scx[n56], mad->scy[n56], mad->scz[n56], 1);
            if (mad->im == mad->xt->im) {
              xt_graphics_stub_gscrape(mad->xt, jtrunc(mad->scx[n56]), jtrunc(mad->scy[n56]), jtrunc(mad->scz[n56]));
            }
          }
          float n59 = (fabsf(medium_sin(m, (float)mad->pxy)) + fabsf(medium_sin(m, (float)mad->pzy))) / 3.0f;
          if (n59 > 0.4f) n59 = 0.4f;
          float n60 = n59 + cd->bounce[mad->cn];
          if (n60 < 1.1f) n60 = 1.1f;
          mad_regy(mad, n56, fabsf(mad->scy[n56] * n60), contO);
          if (mad->scy[n56] > 0.0f) mad->scy[n56] = mad->scy[n56] - fabsf(mad->scy[n56] * n60);
          array7[n56] = true;
        }
        if (trackers->zy[n53] == -90 && array2[n56] < trackers->z[n53] + trackers->radz[n53] &&
            (mad->scz[n56] < 0.0f || trackers->radz[n53] == 287)) {
          for (int32_t n62 = 0; n62 < 4; n62++) {
            if (n56 != n62 && array2[n62] >= trackers->z[n53] + trackers->radz[n53]) {
              array2[n62] = array2[n62] - (array2[n56] - (float)(trackers->z[n53] + trackers->radz[n53]));
            }
          }
          array2[n56] = (float)(trackers->z[n53] + trackers->radz[n53]);
          if (trackers->skd[n53] != 2) mad->crank[0][n56]++;
          if (trackers->skd[n53] == 5 && medium_random(m) > medium_random(m)) mad->crank[0][n56]++;
          if (mad->crank[0][n56] > 1) {
            cont_o_sprk(contO, array[n56], array3[n56], array2[n56], mad->scx[n56], mad->scy[n56], mad->scz[n56], 0);
            if (mad->im == mad->xt->im) xt_graphics_stub_scrape(mad->xt, jtrunc(mad->scx[n56]), jtrunc(mad->scy[n56]), jtrunc(mad->scz[n56]), mad->m);
          }
          float n66 = (fabsf(medium_cos(m, (float)mad->pxy)) + fabsf(medium_cos(m, (float)mad->pzy))) / 4.0f;
          if (n66 > 0.3f) n66 = 0.3f;
          if (b3) n66 = 0.0f;
          float n67 = n66 + (cd->bounce[mad->cn] - 0.2f);
          if (n67 < 1.1f) n67 = 1.1f;
          mad_regz(mad, n56, fabsf((mad->scz[n56] * n67) * (float)trackers->dam[n53]), contO);
          mad->scz[n56] = mad->scz[n56] + fabsf(mad->scz[n56] * n67);
          mad->skid = 2;
          b2 = true;
          array7[n56] = true;
          if (!trackers->notwall[n53]) control->wall = n53;
        }
        if (trackers->zy[n53] == 90 && array2[n56] > trackers->z[n53] - trackers->radz[n53] &&
            (mad->scz[n56] > 0.0f || trackers->radz[n53] == 287)) {
          for (int32_t n69 = 0; n69 < 4; n69++) {
            if (n56 != n69 && array2[n69] <= trackers->z[n53] - trackers->radz[n53]) {
              array2[n69] = array2[n69] - (array2[n56] - (float)(trackers->z[n53] - trackers->radz[n53]));
            }
          }
          array2[n56] = (float)(trackers->z[n53] - trackers->radz[n53]);
          if (trackers->skd[n53] != 2) mad->crank[1][n56]++;
          if (trackers->skd[n53] == 5 && medium_random(m) > medium_random(m)) mad->crank[1][n56]++;
          if (mad->crank[1][n56] > 1) {
            cont_o_sprk(contO, array[n56], array3[n56], array2[n56], mad->scx[n56], mad->scy[n56], mad->scz[n56], 0);
            if (mad->im == mad->xt->im) xt_graphics_stub_scrape(mad->xt, jtrunc(mad->scx[n56]), jtrunc(mad->scy[n56]), jtrunc(mad->scz[n56]), mad->m);
          }
          float n73 = (fabsf(medium_cos(m, (float)mad->pxy)) + fabsf(medium_cos(m, (float)mad->pzy))) / 4.0f;
          if (n73 > 0.3f) n73 = 0.3f;
          if (b3) n73 = 0.0f;
          float n74 = n73 + (cd->bounce[mad->cn] - 0.2f);
          if (n74 < 1.1f) n74 = 1.1f;
          mad_regz(mad, n56, -fabsf((mad->scz[n56] * n74) * (float)trackers->dam[n53]), contO);
          mad->scz[n56] = mad->scz[n56] - fabsf(mad->scz[n56] * n74);
          mad->skid = 2;
          b2 = true;
          array7[n56] = true;
          if (!trackers->notwall[n53]) control->wall = n53;
        }
        if (trackers->xy[n53] == -90 && array[n56] < trackers->x[n53] + trackers->radx[n53] &&
            (mad->scx[n56] < 0.0f || trackers->radx[n53] == 287)) {
          for (int32_t n76 = 0; n76 < 4; n76++) {
            if (n56 != n76 && array[n76] >= trackers->x[n53] + trackers->radx[n53]) {
              array[n76] = array[n76] - (array[n56] - (float)(trackers->x[n53] + trackers->radx[n53]));
            }
          }
          array[n56] = (float)(trackers->x[n53] + trackers->radx[n53]);
          if (trackers->skd[n53] != 2) mad->crank[2][n56]++;
          if (trackers->skd[n53] == 5 && medium_random(m) > medium_random(m)) mad->crank[2][n56]++;
          if (mad->crank[2][n56] > 1) {
            cont_o_sprk(contO, array[n56], array3[n56], array2[n56], mad->scx[n56], mad->scy[n56], mad->scz[n56], 0);
            if (mad->im == mad->xt->im) xt_graphics_stub_scrape(mad->xt, jtrunc(mad->scx[n56]), jtrunc(mad->scy[n56]), jtrunc(mad->scz[n56]), mad->m);
          }
          float n80 = (fabsf(medium_cos(m, (float)mad->pxy)) + fabsf(medium_cos(m, (float)mad->pzy))) / 4.0f;
          if (n80 > 0.3f) n80 = 0.3f;
          if (b3) n80 = 0.0f;
          float n81 = n80 + (cd->bounce[mad->cn] - 0.2f);
          if (n81 < 1.1f) n81 = 1.1f;
          mad_regx(mad, n56, fabsf((mad->scx[n56] * n81) * (float)trackers->dam[n53]), contO);
          mad->scx[n56] = mad->scx[n56] + fabsf(mad->scx[n56] * n81);
          mad->skid = 2;
          b2 = true;
          array7[n56] = true;
          if (!trackers->notwall[n53]) control->wall = n53;
        }
        if (trackers->xy[n53] == 90 && array[n56] > trackers->x[n53] - trackers->radx[n53] &&
            (mad->scx[n56] > 0.0f || trackers->radx[n53] == 287)) {
          for (int32_t n83 = 0; n83 < 4; n83++) {
            if (n56 != n83 && array[n83] <= trackers->x[n53] - trackers->radx[n53]) {
              array[n83] = array[n83] - (array[n56] - (float)(trackers->x[n53] - trackers->radx[n53]));
            }
          }
          array[n56] = (float)(trackers->x[n53] - trackers->radx[n53]);
          if (trackers->skd[n53] != 2) mad->crank[3][n56]++;
          if (trackers->skd[n53] == 5 && medium_random(m) > medium_random(m)) mad->crank[3][n56]++;
          if (mad->crank[3][n56] > 1) {
            cont_o_sprk(contO, array[n56], array3[n56], array2[n56], mad->scx[n56], mad->scy[n56], mad->scz[n56], 0);
            if (mad->im == mad->xt->im) xt_graphics_stub_scrape(mad->xt, jtrunc(mad->scx[n56]), jtrunc(mad->scy[n56]), jtrunc(mad->scz[n56]), mad->m);
          }
          float n87 = (fabsf(medium_cos(m, (float)mad->pxy)) + fabsf(medium_cos(m, (float)mad->pzy))) / 4.0f;
          if (n87 > 0.3f) n87 = 0.3f;
          if (b3) n87 = 0.0f;
          float n88 = n87 + (cd->bounce[mad->cn] - 0.2f);
          if (n88 < 1.1f) n88 = 1.1f;
          mad_regx(mad, n56, -fabsf((mad->scx[n56] * n88) * (float)trackers->dam[n53]), contO);
          mad->scx[n56] = mad->scx[n56] - fabsf(mad->scx[n56] * n88);
          mad->skid = 2;
          b2 = true;
          array7[n56] = true;
          if (!trackers->notwall[n53]) control->wall = n53;
        }
        if (trackers->zy[n53] != 0 && trackers->zy[n53] != 90 && trackers->zy[n53] != -90) {
          int32_t n90 = 90 + trackers->zy[n53];
          float n91 = 1.0f + (float)(50 - abs(trackers->zy[n53])) / 30.0f;
          if (n91 < 1.0f) n91 = 1.0f;
          float n92 = array3[n56], n93v = array2[n56];
          mad_rot(mad, &n92, &n93v, (float)trackers->y[n53], (float)trackers->z[n53], n90, 1);
          if (n93v > trackers->z[n53] && n93v < trackers->z[n53] + 200) {
            mad->scy[n56] = mad->scy[n56] - (n93v - (float)trackers->z[n53]) / n91;
            n93v = (float)trackers->z[n53];
          }
          if (n93v > trackers->z[n53] - 30) {
            if (trackers->skd[n53] == 2) n54++;
            else n51++;
            mad->wtouch = true;
            mad->gtouch = false;
            if (mad->capsized && (trackers->skd[n53] == 0 || trackers->skd[n53] == 1)) {
              cont_o_sprk(contO, array[n56], array3[n56], array2[n56], mad->scx[n56], mad->scy[n56], mad->scz[n56], 1);
              if (mad->im == mad->xt->im) {
                xt_graphics_stub_gscrape(mad->xt, jtrunc(mad->scx[n56]), jtrunc(mad->scy[n56]), jtrunc(mad->scz[n56]));
              }
            }
            if (!b3 && n22 != 0) {
              cont_o_dust(contO, n56, array[n56], array3[n56], array2[n56], jtrunc(mad->scx[n56]), jtrunc(mad->scz[n56]),
                          1.4f * cd->simag[mad->cn], 0, mad->capsized && mad->mtouch);
            }
          }
          float outY, outZ;
          outY = n92; outZ = n93v;
          mad_rot(mad, &outY, &outZ, (float)trackers->y[n53], (float)trackers->z[n53], -n90, 1);
          array3[n56] = outY;
          array2[n56] = outZ;
          array7[n56] = true;
        }
        if (trackers->xy[n53] != 0 && trackers->xy[n53] != 90 && trackers->xy[n53] != -90) {
          int32_t n95 = 90 + trackers->xy[n53];
          float n96 = 1.0f + (float)(50 - abs(trackers->xy[n53])) / 30.0f;
          if (n96 < 1.0f) n96 = 1.0f;
          float n97 = array3[n56], n98v = array[n56];
          mad_rot(mad, &n97, &n98v, (float)trackers->y[n53], (float)trackers->x[n53], n95, 1);
          if (n98v > trackers->x[n53] && n98v < trackers->x[n53] + 200) {
            mad->scy[n56] = mad->scy[n56] - (n98v - (float)trackers->x[n53]) / n96;
            n98v = (float)trackers->x[n53];
          }
          if (n98v > trackers->x[n53] - 30) {
            if (trackers->skd[n53] == 2) n54++;
            else n51++;
            mad->wtouch = true;
            mad->gtouch = false;
            if (mad->capsized && (trackers->skd[n53] == 0 || trackers->skd[n53] == 1)) {
              cont_o_sprk(contO, array[n56], array3[n56], array2[n56], mad->scx[n56], mad->scy[n56], mad->scz[n56], 1);
              if (mad->im == mad->xt->im) {
                xt_graphics_stub_gscrape(mad->xt, jtrunc(mad->scx[n56]), jtrunc(mad->scy[n56]), jtrunc(mad->scz[n56]));
              }
            }
            if (!b3 && n22 != 0) {
              cont_o_dust(contO, n56, array[n56], array3[n56], array2[n56], jtrunc(mad->scx[n56]), jtrunc(mad->scz[n56]),
                          1.4f * cd->simag[mad->cn], 0, mad->capsized && mad->mtouch);
            }
          }
          float outY2, outX2;
          outY2 = n97; outX2 = n98v;
          mad_rot(mad, &outY2, &outX2, (float)trackers->y[n53], (float)trackers->x[n53], -n95, 1);
          array3[n56] = outY2;
          array[n56] = outX2;
          array7[n56] = true;
        }
      }
    }
    if (n54 == 4) mad->mtouch = true;
    if (n55 == 4) n40 = 4;
  }
  if (n51 == 4) mad->mtouch = true;

  for (int32_t n100 = 0; n100 < 4; n100++) {
    for (int32_t n101 = 0; n101 < 4; n101++) {
      if (mad->crank[n100][n101] == mad->lcrank[n100][n101]) mad->crank[n100][n101] = 0;
      mad->lcrank[n100][n101] = mad->crank[n100][n101];
    }
  }

  int32_t a_ = 0, a2_ = 0, a3_ = 0, a4_ = 0;
  if (mad->scy[2] != mad->scy[0]) {
    int32_t sign = (mad->scy[2] < mad->scy[0]) ? -1 : 1;
    a_ = mad_corner_angle(array2[0] - array2[2], array3[0] - array3[2], array[0] - array[2],
                           abs(contO->keyz[0]) + abs(contO->keyz[2]), sign);
  }
  if (mad->scy[3] != mad->scy[1]) {
    int32_t sign = (mad->scy[3] < mad->scy[1]) ? -1 : 1;
    a2_ = mad_corner_angle(array2[1] - array2[3], array3[1] - array3[3], array[1] - array[3],
                            abs(contO->keyz[1]) + abs(contO->keyz[3]), sign);
  }
  if (mad->scy[1] != mad->scy[0]) {
    int32_t sign = (mad->scy[1] < mad->scy[0]) ? -1 : 1;
    a3_ = mad_corner_angle(array2[0] - array2[1], array3[0] - array3[1], array[0] - array[1],
                            abs(contO->keyx[0]) + abs(contO->keyx[1]), sign);
  }
  if (mad->scy[3] != mad->scy[2]) {
    int32_t sign = (mad->scy[3] < mad->scy[2]) ? -1 : 1;
    a4_ = mad_corner_angle(array2[2] - array2[3], array3[2] - array3[3], array[2] - array[3],
                            abs(contO->keyx[2]) + abs(contO->keyx[3]), sign);
  }
  if (b2) {
    int32_t absv;
    for (absv = abs(contO->xz + 45); absv > 180; absv -= 360) {}
    mad->pmlt = (abs(absv) > 90) ? 1 : -1;
    int32_t absv2;
    for (absv2 = abs(contO->xz - 45); absv2 > 180; absv2 -= 360) {}
    mad->nmlt = (abs(absv2) > 90) ? 1 : -1;
  }

  // fr(forca * fr(fr(fr(fr(fr(fr(fr(scz0*nmlt-scz1*pmlt)+scz2*pmlt)-scz3*nmlt)+scx0*pmlt)+scx1*nmlt)-scx2*nmlt)-scx3*pmlt))
  // -- each layer's fr() wraps TWO ops (an unwrapped multiply plus the
  // combine with the previous layer's rounded result), not one -- case 2
  // at every layer, so this must round once per layer in double, not stay
  // in native float throughout like most of this function.
  double forceRaw = (double)mad->scz[0] * mad->nmlt - (double)mad->scz[1] * mad->pmlt;
  float forceL = (float)forceRaw;
  forceRaw = (double)forceL + (double)mad->scz[2] * mad->pmlt; forceL = (float)forceRaw;
  forceRaw = (double)forceL - (double)mad->scz[3] * mad->nmlt; forceL = (float)forceRaw;
  forceRaw = (double)forceL + (double)mad->scx[0] * mad->pmlt; forceL = (float)forceRaw;
  forceRaw = (double)forceL + (double)mad->scx[1] * mad->nmlt; forceL = (float)forceRaw;
  forceRaw = (double)forceL - (double)mad->scx[2] * mad->nmlt; forceL = (float)forceRaw;
  forceRaw = (double)forceL - (double)mad->scx[3] * mad->pmlt; forceL = (float)forceRaw;
  float forceProduct = mad->forca * forceL;
  contO->xz = jtrunc((float)contO->xz + forceProduct);

  if (abs(a2_) > abs(a_)) a_ = a2_;
  if (abs(a4_) > abs(a3_)) a3_ = a4_;
  if (mad->xt->extended && !mad->mtouch) {
    // Extended (Madness.java:2664-2700): in the air, the pitch and roll
    // corrections turn the car toward whichever is nearer, upright or
    // upside down, instead of NFM 2's fixed sign -- kinder landings.
    const int32_t zero_zy = zyangle < 360 - zyangle ? zyangle : 360 - zyangle, flip_zy = abs(zyangle - 180);
    if ((zero_zy <= flip_zy && zyangle < 180) || (flip_zy < zero_zy && zyangle >= 180)) {
      if (mad->pzy > 0) mad->pzy -= abs(a_); else mad->pzy += abs(a_);
    }
    if ((zero_zy <= flip_zy && zyangle >= 180) || (flip_zy < zero_zy && zyangle < 180)) {
      if (mad->pzy > 0) mad->pzy += abs(a_); else mad->pzy -= abs(a_);
    }
    const int32_t zero_xy = xyangle < 360 - xyangle ? xyangle : 360 - xyangle, flip_xy = abs(xyangle - 180);
    if ((zero_xy <= flip_xy && xyangle < 180) || (flip_xy < zero_xy && xyangle >= 180)) {
      if (mad->pxy > 0) mad->pxy -= abs(a3_); else mad->pxy += abs(a3_);
    }
    if ((zero_xy <= flip_xy && xyangle >= 180) || (flip_xy < zero_xy && xyangle < 180)) {
      if (mad->pxy > 0) mad->pxy += abs(a3_); else mad->pxy -= abs(a3_);
    }
  } else {
    if (!zyinv) mad->pzy += a_; else mad->pzy -= a_;
    if (n3 == 0) mad->pxy += a3_; else mad->pxy -= a3_;
  }
  if (n40 == 4) {
    int32_t n106 = 0;
    while (mad->pzy < 360) { mad->pzy += 360; contO->zy += 360; }
    while (mad->pzy > 360) { mad->pzy -= 360; contO->zy -= 360; }
    if (mad->pzy < 190 && mad->pzy > 170) { mad->pzy = 180; contO->zy = 180; n106++; }
    if (mad->pzy > 350 || mad->pzy < 10) { mad->pzy = 0; contO->zy = 0; n106++; }
    while (mad->pxy < 360) { mad->pxy += 360; contO->xy += 360; }
    while (mad->pxy > 360) { mad->pxy -= 360; contO->xy -= 360; }
    if (mad->pxy < 190 && mad->pxy > 170) { mad->pxy = 180; contO->xy = 180; n106++; }
    if (mad->pxy > 350 || mad->pxy < 10) { mad->pxy = 0; contO->xy = 0; n106++; }
    if (n106 == 2) mad->mtouch = true;
  }
  if (!mad->mtouch && mad->wtouch) {
    if (mad->cntouch == 10) mad->mtouch = true;
    else mad->cntouch++;
  } else {
    mad->cntouch = 0;
  }

  // Y/X/Z position update -- every fr() here wraps exactly one op between
  // already-float (or int-widening-to-float) values, so this whole block
  // is case 1 throughout: native `float` arithmetic matches the JS's
  // double-then-round bit-for-bit.
  float sumY = ((array3[0] + array3[1]) + array3[2]) + array3[3];
  float avgY = sumY / 4.0f;
  float gCosZy = (float)grat * medium_cos(m, (float)mad->pzy);
  float gCosZyPxy = gCosZy * medium_cos(m, (float)mad->pxy);
  float subY = avgY - gCosZyPxy;
  float yFloat = subY + n6;
  contO->y = jtrunc(yFloat);

  int32_t n107 = zyinv ? -1 : 1;
  float cosXz = medium_cos(m, (float)contO->xz);
  float sinXz = medium_sin(m, (float)contO->xz);
  float tX = array[0] - ((float)contO->keyx[0] * cosXz);
  tX = tX + ((float)(n107 * contO->keyz[0]) * sinXz);
  tX = tX + array[1];
  tX = tX - ((float)contO->keyx[1] * cosXz);
  tX = tX + ((float)(n107 * contO->keyz[1]) * sinXz);
  tX = tX + array[2];
  tX = tX - ((float)contO->keyx[2] * cosXz);
  tX = tX + ((float)(n107 * contO->keyz[2]) * sinXz);
  tX = tX + array[3];
  tX = tX - ((float)contO->keyx[3] * cosXz);
  tX = tX + ((float)(n107 * contO->keyz[3]) * sinXz);
  float avgX = tX / 4.0f;
  float gSinPxy = (float)grat * medium_sin(m, (float)mad->pxy);
  float gSinPxyCosXz = gSinPxy * cosXz;
  float add1X = avgX + gSinPxyCosXz;
  float gSinPzy = (float)grat * medium_sin(m, (float)mad->pzy);
  float gSinPzySinXz = gSinPzy * sinXz;
  float sub2X = add1X - gSinPzySinXz;
  float xFloat = sub2X + n4;
  contO->x = jtrunc(xFloat);

  float tZ = array2[0] - ((float)(n107 * contO->keyz[0]) * cosXz);
  tZ = tZ - ((float)contO->keyx[0] * sinXz);
  tZ = tZ + array2[1];
  tZ = tZ - ((float)(n107 * contO->keyz[1]) * cosXz);
  tZ = tZ - ((float)contO->keyx[1] * sinXz);
  tZ = tZ + array2[2];
  tZ = tZ - ((float)(n107 * contO->keyz[2]) * cosXz);
  tZ = tZ - ((float)contO->keyx[2] * sinXz);
  tZ = tZ + array2[3];
  tZ = tZ - ((float)(n107 * contO->keyz[3]) * cosXz);
  tZ = tZ - ((float)contO->keyx[3] * sinXz);
  float avgZ = tZ / 4.0f;
  float gSinPxyZ = (float)grat * medium_sin(m, (float)mad->pxy);
  float gSinPxySinXz = gSinPxyZ * sinXz;
  float add1Z = avgZ + gSinPxySinXz;
  float gSinPzyZ = (float)grat * medium_sin(m, (float)mad->pzy);
  float gSinPzyCosXz = gSinPzyZ * cosXz;
  float sub2Z = add1Z - gSinPzyCosXz;
  float zFloat = sub2Z + n5;
  contO->z = jtrunc(zFloat);

  if (fabsf(mad->speed) > 10.0f || !mad->mtouch) {
    if (abs(mad->pxy - contO->xy) >= 4) {
      if (mad->pxy > contO->xy) contO->xy += 2 + (mad->pxy - contO->xy) / 2;
      else contO->xy -= 2 + (contO->xy - mad->pxy) / 2;
    } else {
      contO->xy = mad->pxy;
    }
    if (abs(mad->pzy - contO->zy) >= 4) {
      if (mad->pzy > contO->zy) contO->zy += 2 + (mad->pzy - contO->zy) / 2;
      else contO->zy -= 2 + (contO->zy - mad->pzy) / 2;
    } else {
      contO->zy = mad->pzy;
    }
  }

  if (mad->wtouch && !mad->capsized) {
    // The 0.4 and 0.3 below are doubles in Java (as is the 1.5).
    const float n108 = (float)((double)((mad->speed / (float)cd->swits[mad->cn][2]) * 14.0f) *
                               ((double)cd->bounce[mad->cn] - 0.4));
    if (control->left && mad->tilt < n108 && mad->tilt >= 0.0f) {
      mad->tilt = mad->tilt + 0.4f;
    } else if (control->right && mad->tilt > -n108 && mad->tilt <= 0.0f) {
      mad->tilt = mad->tilt - 0.4f;
    } else if (fabs((double)mad->tilt) > 3.0 * ((double)cd->bounce[mad->cn] - 0.4)) {
      if (mad->tilt > 0.0f) mad->tilt = (float)((double)mad->tilt - 3.0 * ((double)cd->bounce[mad->cn] - 0.3));
      else mad->tilt = (float)((double)mad->tilt + 3.0 * ((double)cd->bounce[mad->cn] - 0.3));
    } else {
      mad->tilt = 0.0f;
    }
    contO->xy = jtrunc((float)contO->xy + mad->tilt);
    if (mad->gtouch) contO->y = jtrunc_d((double)contO->y - (double)mad->tilt / 1.5);
  } else if (mad->tilt != 0.0f) {
    mad->tilt = 0.0f;
  }

  if (mad->wtouch && n22 == 2) {
    contO->zy += mad_wobble_delta(mad, 6.0f, 3.0f);
    contO->xy += mad_wobble_delta(mad, 6.0f, 3.0f);
  }
  if (mad->wtouch && n22 == 1) {
    contO->zy += mad_wobble_delta(mad, 4.0f, 2.0f);
    contO->xy += mad_wobble_delta(mad, 4.0f, 2.0f);
  }

  if (mad->hitmag >= cd->maxmag[mad->cn] && !mad->dest) {
    mad_distruct(mad, contO);
    if (mad->cntdest == 7) mad->dest = true;
    else mad->cntdest++;
    if (mad->cntdest == 1) mad->rpd->dest[mad->im] = 300;
  }
  if (contO->dist == 0) {
    for (int32_t n109 = 0; n109 < contO->npl; n109++) {
      if (contO->p[n109].chip != 0) contO->p[n109].chip = 0;
      if (contO->p[n109].embos != 0) contO->p[n109].embos = 13;
    }
  }

  int32_t focus = 0;
  int32_t n110 = 0;
  int32_t n111 = 0;
  int32_t n112 = mad->nofocus ? 1 : 7;
  for (int32_t n113 = 0; n113 < checkPoints->n; n113++) {
    if (checkPoints->typ[n113] > 0) {
      n111++;
      if (checkPoints->typ[n113] == 1) {
        if (mad->clear == n111 + mad->nlaps * checkPoints->nsp) n112 = 1;
        // 60.0 + fr(abs(fr(fr(fr(A+B)+C)+D))/4.0) -- the "60.0 +" is NOT
        // itself fr()-wrapped, so this stays a genuine double comparison
        // threshold (float widened into double, added to the literal).
        float sczSum = ((mad->scz[0] + mad->scz[1]) + mad->scz[2]) + mad->scz[3];
        float sczAbsOver4 = fabsf(sczSum) / 4.0f;
        double thresholdZ = 60.0 + (double)sczAbsOver4;
        if (abs(contO->z - checkPoints->z[n113]) < thresholdZ &&
            abs(contO->x - checkPoints->x[n113]) < 700 &&
            abs(contO->y - checkPoints->y[n113] + 350) < 450 &&
            mad->clear == n111 + mad->nlaps * checkPoints->nsp - 1) {
          mad->clear = n111 + mad->nlaps * checkPoints->nsp;
          mad->pcleared = n113;
          mad->focus = -1;
        }
      }
      if (checkPoints->typ[n113] == 2) {
        if (mad->clear == n111 + mad->nlaps * checkPoints->nsp) n112 = 1;
        float scxSum = ((mad->scx[0] + mad->scx[1]) + mad->scx[2]) + mad->scx[3];
        float scxAbsOver4 = fabsf(scxSum) / 4.0f;
        double thresholdX = 60.0 + (double)scxAbsOver4;
        if (abs(contO->x - checkPoints->x[n113]) < thresholdX &&
            abs(contO->z - checkPoints->z[n113]) < 700 &&
            abs(contO->y - checkPoints->y[n113] + 350) < 450 &&
            mad->clear == n111 + mad->nlaps * checkPoints->nsp - 1) {
          mad->clear = n111 + mad->nlaps * checkPoints->nsp;
          mad->pcleared = n113;
          mad->focus = -1;
        }
      }
    }
    int32_t pyVal = mad_py(contO->x / 100, checkPoints->x[n113] / 100, contO->z / 100, checkPoints->z[n113] / 100);
    int32_t weighted = pyVal * n112;
    if (weighted < n110 || n110 == 0) {
      focus = n113;
      n110 = weighted;
    }
  }
  if (mad->clear == n111 + mad->nlaps * checkPoints->nsp) {
    mad->nlaps++;
    if (mad->xt->multion == 1 && mad->im == mad->xt->im) {
      if (mad->xt->laptime < mad->xt->fastestlap || mad->xt->fastestlap == 0) mad->xt->fastestlap = mad->xt->laptime;
      mad->xt->laptime = 0;
    }
  }
  if (mad->im == mad->xt->im) {
    if (mad->xt->multion == 1 && mad->xt->starcnt == 0) mad->xt->laptime++;
    m->checkpoint = mad->clear;
    while (m->checkpoint >= checkPoints->nsp) m->checkpoint -= checkPoints->nsp;
    if (mad->clear == checkPoints->nlaps * checkPoints->nsp - 1) m->lastcheck = true;
    if (checkPoints->haltall) m->lastcheck = false;
  }
  if (mad->focus == -1) {
    if (xt_graphics_stub_human(mad->xt, mad->im)) focus += 2; else focus++;
    if (!mad->nofocus) {
      int32_t n114 = mad->pcleared + 1;
      if (n114 >= checkPoints->n) n114 = 0;
      while (checkPoints->typ[n114] <= 0) {
        if (++n114 >= checkPoints->n) n114 = 0;
      }
      if (focus > n114 && (mad->clear != mad->nlaps * checkPoints->nsp || focus < mad->pcleared)) {
        focus = n114;
        mad->focus = focus;
      }
    }
    if (focus >= checkPoints->n) focus -= checkPoints->n;
    if (checkPoints->typ[focus] == -3) focus = 0;
    if (xt_graphics_stub_human(mad->xt, mad->im)) {
      if (mad->missedcp != -1) mad->missedcp = -1;
    } else if (mad->missedcp != 0) {
      mad->missedcp = 0;
    }
  } else {
    focus = mad->focus;
    if (xt_graphics_stub_human(mad->xt, mad->im)) {
      // this.py()/Math.sqrt() are pure (no PRNG draw), so hoisting one
      // shared computation across the three comparisons below -- unlike
      // this.m.random(), which must be called once per JS call site -- is
      // behaviorally identical to recomputing it three times.
      int32_t dpy = mad_py(contO->x / 10, checkPoints->x[mad->focus] / 10, contO->z / 10, checkPoints->z[mad->focus] / 10);
      double dist = sqrt((double)dpy);
      if (mad->missedcp == 0 && mad->mtouch && dist > 800.0) mad->missedcp = 1;
      if (mad->missedcp == -2 && dist < 400.0) mad->missedcp = 0;
      if (mad->missedcp != 0 && mad->mtouch && dist < 250.0) mad->missedcp = 68;
    } else {
      mad->missedcp = 1;
    }
    if (mad->nofocus) { mad->focus = -1; mad->missedcp = 0; }
  }
  if (mad->nofocus) mad->nofocus = false;
  mad->point = focus;

  if (mad->fixes != 0) {
    if (m->noelec == 0) {
      for (int32_t n115 = 0; n115 < checkPoints->fn; n115++) {
        if (!checkPoints->roted[n115]) {
          if (abs(contO->z - checkPoints->fz[n115]) < 200 &&
              mad_py(contO->x / 100, checkPoints->fx[n115] / 100, contO->y / 100, checkPoints->fy[n115] / 100) < 30) {
            // this.xt.carfixed.play() -- audio, no-op in this M2 stub.
            contO->fix = true;
            mad->rpd->fix[mad->im] = 300;
          }
        } else if (abs(contO->x - checkPoints->fx[n115]) < 200 &&
                   mad_py(contO->z / 100, checkPoints->fz[n115] / 100, contO->y / 100, checkPoints->fy[n115] / 100) < 30) {
          contO->fix = true;
          mad->rpd->fix[mad->im] = 300;
        }
      }
    }
  } else {
    for (int32_t n116 = 0; n116 < checkPoints->fn; n116++) {
      if (mad_rpy((float)(contO->x / 100), (float)(checkPoints->fx[n116] / 100), (float)(contO->y / 100), (float)(checkPoints->fy[n116] / 100),
                  (float)(contO->z / 100), (float)(checkPoints->fz[n116] / 100)) < 760) {
        m->noelec = 2;
      }
    }
  }

  if (contO->fcnt == 7 || contO->fcnt == 8) {
    mad->squash = 0; mad->nbsq = 0; mad->hitmag = 0; mad->cntdest = 0; mad->dest = false; mad->newcar = true;
    mad->just_fixed = true; // see mad.h's own doc comment on this field
    contO->fcnt = 9;
    if (mad->fixes > 0) mad->fixes--;
  }
  if (mad->newedcar != 0) {
    mad->newedcar--;
    if (mad->newedcar == 10) mad->newcar = false;
  }

  if (!mad->mtouch) {
    if (mad->trcnt != 1) { mad->trcnt = 1; mad->lxz = contO->xz; }
    if (mad->loop == 2 || mad->loop == -1) {
      mad->travxy = jtrunc((float)mad->travxy + (mad->rcomp - mad->lcomp));
      if (abs(mad->travxy) > 135) mad->rtab = true;
      mad->travzy = jtrunc((float)mad->travzy + (mad->ucomp - mad->dcomp));
      if (mad->travzy > 135) mad->ftab = true;
      if (mad->travzy < -135) mad->btab = true;
    }
    if (mad->lxz != contO->xz) { mad->travxz += mad->lxz - contO->xz; mad->lxz = contO->xz; }
    if (mad->srfcnt < 10) {
      if (control->wall != -1) mad->surfer = true;
      mad->srfcnt++;
    }
  } else if (!mad->dest) {
    if (!mad->capsized) {
      if (mad->capcnt != 0) mad->capcnt = 0;
      if (mad->gtouch && mad->trcnt != 0) {
        if (mad->trcnt == 9) {
          mad->powerup = 0.0f;
          if (abs(mad->travxy) > 90) mad->powerup = mad->powerup + (float)abs(mad->travxy) / 24.0f;
          else if (mad->rtab) mad->powerup = mad->powerup + 30.0f;
          if (abs(mad->travzy) > 90) mad->powerup = mad->powerup + (float)abs(mad->travzy) / 18.0f;
          else {
            if (mad->ftab) mad->powerup = mad->powerup + 40.0f;
            if (mad->btab) mad->powerup = mad->powerup + 40.0f;
          }
          if (abs(mad->travxz) > 90) mad->powerup = mad->powerup + (float)abs(mad->travxz) / 18.0f;
          if (mad->surfer) mad->powerup = mad->powerup + (mad->xt->extended ? 15.0f : 30.0f);   // Extended halves it (Madness.java:2869)
          mad->power = mad->power + mad->powerup;
          // Extended (Madness.java:2900-2916): a landed stunt charges the
          // special bar too -- a third of it for the AI's ordinary stunts, a
          // fifth for the player's and for big ones -- unless one is running.
          if (mad->xt->extended && !control->spatk) {
            if (mad->im > 0 && mad->powerup <= 100.0f) mad->spatk += mad->powerup / 3.0f;
            else mad->spatk += mad->powerup / 5.0f;
          }
          if (mad->im == mad->xt->im && jtrunc(mad->powerup) > mad->rpd->powered && mad->rpd->wasted == 0 &&
              (mad->powerup > 60.0f || checkPoints->stage == 1 || checkPoints->stage == 2)) {
            mad->rpdcatch = 30;
            if (mad->rpd->hcaught) mad->rpd->powered = jtrunc(mad->powerup);
            if (mad->xt->multion == 1 && mad->powerup > (float)mad->xt->beststunt) mad->xt->beststunt = jtrunc(mad->powerup);
          }
          if (mad->power > 98.0f) {
            mad->power = 98.0f;
            if (mad->powerup > 150.0f) mad->xtpower = 200; else mad->xtpower = 100;
          }
        }
        if (mad->trcnt == 10) {
          mad->travxy = 0; mad->travzy = 0; mad->travxz = 0;
          mad->ftab = false; mad->rtab = false; mad->btab = false;
          mad->trcnt = 0; mad->srfcnt = 0; mad->surfer = false;
        } else {
          mad->trcnt++;
        }
      }
    } else {
      if (mad->trcnt != 0) {
        mad->travxy = 0; mad->travzy = 0; mad->travxz = 0;
        mad->ftab = false; mad->rtab = false; mad->btab = false;
        mad->trcnt = 0; mad->srfcnt = 0; mad->surfer = false;
      }
      if (mad->capcnt == 0) {
        int32_t n117 = 0;
        for (int32_t n118 = 0; n118 < 4; n118++) {
          if (fabsf(mad->scz[n118]) < 70.0f && fabsf(mad->scx[n118]) < 70.0f) n117++;
        }
        if (n117 == 4) mad->capcnt = 1;
      } else {
        mad->capcnt++;
        if (mad->capcnt == 30) {
          mad->speed = 0.0f;
          contO->y += cd->flipy[mad->cn];
          mad->pxy += 180;
          contO->xy += 180;
          mad->capcnt = 0;
        }
      }
    }
    if (mad->trcnt == 0 && mad->speed != 0.0f) {
      if (mad->xtpower == 0) {
        if (mad->power > 0.0f) {
          float p2 = mad->power * mad->power;
          float p3 = p2 * mad->power;
          mad->power = mad->power - p3 / (float)cd->powerloss[mad->cn];
        } else {
          mad->power = 0.0f;
        }
      } else {
        mad->xtpower--;
      }
    }
  }

  if (xt_graphics_stub_human(mad->xt, mad->im)) {
    if (control->wall != -1) control->wall = -1;
  } else if (mad->lastcolido != 0 && !mad->dest) {
    mad->lastcolido--;
  }
  if (mad->dest) {
    if (checkPoints->dested[mad->im] == 0) {
      if (mad->lastcolido == 0) checkPoints->dested[mad->im] = 1;
      else checkPoints->dested[mad->im] = 2;
    }
  } else if (checkPoints->dested[mad->im] != 0 && checkPoints->dested[mad->im] != 3) {
    checkPoints->dested[mad->im] = 0;
  }
  if (mad->im == mad->xt->im && mad->rpd->wasted == 0 && mad->rpdcatch != 0) {
    mad->rpdcatch--;
    if (mad->rpdcatch == 0) {
      record_cotchinow(mad->rpd, mad->im);
      if (mad->rpd->hcaught) {
        nfm_set_draw_phase(true);
        mad->rpd->whenwasted = jtrunc(185.0f + medium_random(m) * 20.0f);
        nfm_set_draw_phase(false);
      }
    }
  }
  if (mad->xt->extended) mad_special_tick(mad, control);

}
