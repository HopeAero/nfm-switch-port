// ports web/Mad.js
//
// One Mad instance per car: the actual physics simulation state and tick
// (`drive()`), plus car-vs-car collision (`colide()`) and destruction/
// damage-deformation helpers. The simulation core the whole port has been
// building toward -- see TASKS_NATIVE.md's M2 section for what still
// depends on it.
//
// FULL PORT of the field layout, `reseto`, `drive()`, `colide()`, and
// their distruct/regy/regx/regz/rot/rpy/py helpers -- but drive()/colide()
// are only PARTIALLY oracle-verified so far (five scenarios: free-fall
// onto flat trackless ground, free-fall onto a real flat-ground Trackers
// plane with/without steering, and driving into a wall). See mad.c's
// "VERIFICATION STATUS" comment above mad_drive() and TASKS_NATIVE.md for
// exactly what still needs scenario testing (braking-curve edge cases,
// sloped surfaces, capsize/loop tricks, checkpoint-clearing, damage,
// car-vs-car, and a narrow undiagnosed PRNG-draw-count gap found in the
// wall scenario past a certain tick) before this is trusted the way the
// rest of the port is.
#ifndef NFM_MAD_H
#define NFM_MAD_H

#include <stdbool.h>
#include <stdint.h>
#include "nfm_limits.h"
#include "medium.h"
#include "car_define.h"
#include "record.h"
#include "xt_graphics.h"
#include "control.h"
#include "cont_o.h"
#include "trackers.h"
#include "check_points.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Mad {
  int32_t cn;
  int32_t im;
  int32_t mxz, cxz;
  bool dominate[NFM_MAX_CARS], caught[NFM_MAX_CARS];
  int32_t pzy, pxy;
  float speed;
  float forca;
  float scy[4], scz[4], scx[4];
  float drag;
  bool mtouch, wtouch;
  int32_t cntouch;
  bool capsized;
  bool wasTouchTrick;
  int32_t basePzy, basePxy, baseXz;
  int32_t baseTouchX, baseTouchY;
  // Genuinely `double` (not fr()-wrapped anywhere, unlike everything else
  // in this struct) -- this touch-trick block is JS-native mobile-input
  // code, not transpiled from the original Java, and its own source never
  // truncates these two before storing them (only when it feeds pxy/pzy/
  // xz/travxy/travzy, which stay int32_t).
  double lastDeltaX, lastDeltaY;
  bool trickDisableUntilLift;
  int32_t txz, fxz;
  int32_t pmlt, nmlt;
  int32_t dcnt;
  int32_t skid;
  bool pushed;
  bool gtouch;
  bool pl, pr, pd, pu;
  int32_t loop;
  float ucomp, dcomp, lcomp, rcomp;
  int32_t lxz;
  int32_t travxy, travzy, travxz;
  int32_t trcnt, capcnt, srfcnt;
  bool rtab, ftab, btab;
  bool surfer;
  float powerup;
  int32_t xtpower;
  float tilt;
  int32_t crank[4][4], lcrank[4][4];
  int32_t squash;
  int32_t nbsq;
  int32_t hitmag;
  int32_t cntdest;
  bool dest;
  bool newcar;
  // Synthetic one-shot signal, NOT a literal Java field: true for exactly
  // the one tick `contO->fcnt == 7 || contO->fcnt == 8` fires (the same
  // condition Java's own inline code uses right where it calls
  // `array2[n].fcnt==7||8` and immediately plays carfixed.wav -- see
  // xtGraphics.java's engs/carfixed sound-trigger block). `newcar` itself
  // stays true only until the NEXT drawn frame's newcar-rebuild runs
  // (GameSparker.java:891-903, single-player -- see game.c's STATE_RACING
  // draw block), which is normally one tick but can span more than one
  // physics tick if several run before the next frame draws; either way
  // that's still too coarse for triggering a ONE-SHOT sound without a
  // risk of retriggering it every tick newcar stays true. (The
  // `newedcar`-gated 10-tick "shiny repaint" window mad.c's fcnt==7||8
  // block also maintains is multiplayer-only -- GameSparker.java fase==
  // 7001's own colorCar()/rebuild pair, out of scope, no netplay here.)
  // Reset false at the top of every mad_drive() call, so a caller must
  // read it right after that tick's call, before the next one clears it
  // -- same convention as reading any other post-tick Mad field.
  bool just_fixed;
  int32_t pan;
  int32_t pcleared;
  int32_t clear;
  int32_t nlaps;
  int32_t focus;
  float power;
  int32_t missedcp;
  int32_t lastcolido;
  int32_t point;
  bool nofocus;
  int32_t rpdcatch;
  int32_t newedcar;
  int32_t fixes;
  int32_t shakedam, outshakedam;
  bool colidim;

  // Extended's special (Madness.java): the bar (`spatk`, 0..120, filled by
  // stunts), what is left of a running special (`speclast`, 120 down to 0),
  // and the conditions specials.c puts on cars.
  float spatk, speclast, speclast2;
  bool specialact, frozen, strswap, leech, redstr;
  float strengthreduce;  // the strength a swap hands over
  double wxzd;           // Extended's double ContO.wxz (its cars turn 7.5 a tick); contO->wxz is its int

  CarDefine *cd;   // borrowed
  Medium *m;       // borrowed
  Record *rpd;     // borrowed
  XtGraphicsStub *xt; // borrowed (see xt_graphics.h -- M2 stub, not the real XtGraphics.js)
} Mad;

void mad_init(Mad *mad, CarDefine *cd, Medium *m, Record *rpd, XtGraphicsStub *xt, int32_t im);

/** Per-race reset (JS: `reseto(cn, contO, checkPoints)`). */
void mad_reseto(Mad *mad, int32_t cn, ContO *contO, CheckPoints *checkPoints);

/** One physics tick for this car (JS: `drive(control, contO, trackers,
 * checkPoints)`). The simulation core -- reads `control`'s input flags,
 * updates `contO`'s position/rotation and this Mad's speed/skid/damage/
 * checkpoint-progress state. See mad.c for the line-by-line translation. */
void mad_drive(Mad *mad, Control *control, ContO *contO, Trackers *trackers, CheckPoints *checkPoints);

/** Car-vs-car collision between this car (contO) and `mad2`/`contO2` (JS:
 * `colide(contO, mad, contO2)` -- note the JS's own parameter is itself
 * named `mad`, shadowing the instance; renamed `mad2` here to avoid
 * confusion with `this`). */
void mad_colide(Mad *mad, ContO *contO, Mad *mad2, ContO *contO2);

// --- Helpers `drive()`/`colide()` depend on (all ported, JS: distruct/
// regy/regx/regz/rot/rpy/py) ---

void mad_distruct(Mad *mad, ContO *contO);
int32_t mad_regy(Mad *mad, int32_t n, float a, ContO *contO);
int32_t mad_regx(Mad *mad, int32_t n, float n2, ContO *contO);
int32_t mad_regz(Mad *mad, int32_t n, float n2, ContO *contO);

#ifdef __cplusplus
}
#endif

#endif
