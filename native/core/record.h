// ports web/Record.js AND the plain decompiled Record.java (web/Record.js
// itself never got a full replay port either -- see the top of record.c
// for why this file goes to the Java directly for rec/cotchinow/play/
// playh/regy/regx/regz/chipx/chipz instead).
//
// FULL PORT (as of the replay feature). Record.java is 923 lines: per-car
// position/orientation history (`rec`/`play`/`playh`), damage-dent/spark
// ring buffers the SAME `rec()` call also maintains, a "photo finish"
// snapshot (`cotchinow`) that freezes 300 ticks of history the instant a
// race ends, and REPLAY-TIME reapplications of damage (`regy`/`regx`/
// `regz`/`chipx`/`chipz` -- separate from Mad.java's own live versions,
// see their own doc comments in record.c for the confirmed differences).
#ifndef NFM_RECORD_H
#define NFM_RECORD_H

#include <stdbool.h>
#include <stdint.h>
#include "cont_o.h"
#include "nfm_limits.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Mad;

typedef struct Record {
  int32_t caught;
  bool hcaught;
  bool prepit;
  ContO ocar[NFM_MAX_CARS];      // Record.java:103 -- fed by GameSparker's own stuck-car
                       // respawn (:1261), not this port's scope (see mad.h's
                       // own note on why `newcar` never becomes true here);
                       // written for fidelity, never consumed.
  int32_t cntf;
  ContO car[6][NFM_MAX_CARS];     // Record.java:105 -- 50-tick-interval keyframes, same
                       // "recorded but not consumed" scope note as ocar above.
  int32_t squash[6][NFM_MAX_CARS];

  int32_t fix[NFM_MAX_CARS], dest[NFM_MAX_CARS];

  // 300-tick position/orientation ring, oldest at index 0, newest at 299
  // (rec() shifts everything down by one and appends -- see record_rec's
  // own doc comment for why this port keeps that O(n) shift instead of a
  // circular index, same "translate the algorithm, not just the result"
  // reasoning PORT_SPEC.md gives for ContO's own painter's-algorithm sort).
  int32_t x[300][NFM_MAX_CARS], y[300][NFM_MAX_CARS], z[300][NFM_MAX_CARS];
  int32_t xy[300][NFM_MAX_CARS], zy[300][NFM_MAX_CARS], xz[300][NFM_MAX_CARS];
  int32_t wxz[300][NFM_MAX_CARS], wzy[300][NFM_MAX_CARS];

  // Spark-particle ring: [car][stg-slot 0-19][ring-slot 0-29]. A fresh
  // spark is stamped with a countdown starting at 300 (matches ContO's
  // own `stg[]` semantics -- see cont_o_pdust); play()/playh() re-arm
  // `contO->stg[k]` when the ring's countdown equals the tick being
  // played back.
  int32_t ns[NFM_MAX_CARS][20];
  int32_t sspark[NFM_MAX_CARS][20][30];
  int32_t sx[NFM_MAX_CARS][20][30], sy[NFM_MAX_CARS][20][30], sz[NFM_MAX_CARS][20][30];
  float smag[NFM_MAX_CARS][20][30];
  int32_t scx[NFM_MAX_CARS][20][30], scz[NFM_MAX_CARS][20][30];

  // Skid/scrape spark ring: [car][ring-slot 0-199], same countdown idiom.
  int32_t nr[NFM_MAX_CARS];
  int32_t rspark[NFM_MAX_CARS][200];
  int32_t sprk[NFM_MAX_CARS][200];
  int32_t srx[NFM_MAX_CARS][200], sry[NFM_MAX_CARS][200], srz[NFM_MAX_CARS][200];
  float rcx[NFM_MAX_CARS][200], rcy[NFM_MAX_CARS][200], rcz[NFM_MAX_CARS][200];

  // Damage-dent ring: [car][corner/quadrant 0-3][ring-slot 0-6]. Already
  // ported (record_recy/recx/recz below write into these) -- see their
  // own doc comments for the recx/recz `nry`-not-`nrx`/`nrz` indexing
  // quirk, preserved verbatim.
  int32_t nry[NFM_MAX_CARS][4], ry[NFM_MAX_CARS][4][7], magy[NFM_MAX_CARS][4][7];
  bool mtouch[NFM_MAX_CARS][7];
  int32_t nrx[NFM_MAX_CARS][4], rx[NFM_MAX_CARS][4][7], magx[NFM_MAX_CARS][4][7];
  int32_t nrz[NFM_MAX_CARS][4], rz[NFM_MAX_CARS][4][7], magz[NFM_MAX_CARS][4][7];

  int32_t checkpoint[300];
  bool lastcheck[300];

  int32_t wasted;
  int32_t whenwasted;
  int32_t powered;
  int32_t closefinish; // written by check_points_checkstat on a photo finish

  // The "photo finish" snapshot -- cotchinow() deep-copies everything
  // above into these h-prefixed twins the instant `caught` reaches 300
  // (a full 300-tick buffer exists). The replay state (game.c's
  // STATE_REPLAY) reads ONLY these h* fields via record_playh(), never
  // the live ones above, so the replay stays a frozen moment even as
  // racing keeps mutating the live ring after the snapshot is taken (the
  // real Java relies on the same distinction -- `caught` never resets
  // mid-race, so cotchinow's `>= 300` gate can fire at most once per
  // race, exactly matching a race having exactly one "best moment").
  ContO starcar[NFM_MAX_CARS];
  int32_t hsquash[NFM_MAX_CARS], hfix[NFM_MAX_CARS], hdest[NFM_MAX_CARS];
  int32_t hx[300][NFM_MAX_CARS], hy[300][NFM_MAX_CARS], hz[300][NFM_MAX_CARS];
  int32_t hxy[300][NFM_MAX_CARS], hzy[300][NFM_MAX_CARS], hxz[300][NFM_MAX_CARS];
  int32_t hwxz[300][NFM_MAX_CARS], hwzy[300][NFM_MAX_CARS];
  int32_t hsspark[NFM_MAX_CARS][20][30];
  int32_t hsx[NFM_MAX_CARS][20][30], hsy[NFM_MAX_CARS][20][30], hsz[NFM_MAX_CARS][20][30];
  float hsmag[NFM_MAX_CARS][20][30];
  int32_t hscx[NFM_MAX_CARS][20][30], hscz[NFM_MAX_CARS][20][30];
  int32_t hrspark[NFM_MAX_CARS][200];
  int32_t hsprk[NFM_MAX_CARS][200];
  int32_t hsrx[NFM_MAX_CARS][200], hsry[NFM_MAX_CARS][200], hsrz[NFM_MAX_CARS][200];
  float hrcx[NFM_MAX_CARS][200], hrcy[NFM_MAX_CARS][200], hrcz[NFM_MAX_CARS][200];
  int32_t hry[NFM_MAX_CARS][4][7], hmagy[NFM_MAX_CARS][4][7];
  int32_t hrx[NFM_MAX_CARS][4][7], hmagx[NFM_MAX_CARS][4][7];
  int32_t hrz[NFM_MAX_CARS][4][7], hmagz[NFM_MAX_CARS][4][7];
  bool hmtouch[NFM_MAX_CARS][7];
  int32_t hcheckpoint[300];
  bool hlastcheck[300];

  int32_t cntdest[NFM_MAX_CARS];
  int32_t lastfr;
  int32_t ncars;   // cars the last record_reset set up (cars[0..ncars))
} Record;

// Zeroes *r, then applies Record.java's constructor's own non-zero
// defaults (Record.java:99-190): `cntf=50`, `hfix`/`hdest` = -1 (NOT the
// same as `fix`/`dest`, which stay 0 here and only become -1 via
// record_reset() -- a real distinction the Java itself makes, preserved).
void record_init(Record *r);
// Frees the ContOs a valid-or-zeroed Record owns -- see record.c.
void record_free(Record *r);

// Ports Record.java's reset(final ContO[] array) (:192-236) -- called
// once per race start (same moment mad_reseto() runs for each car).
// `cars` must point at the 8 (or fewer; unused slots may be garbage,
// matching Java's own fixed 8-car arrays) currently-placed player
// ContOs. Re-arms `fix`/`dest`/`cntdest` to their -1/-1/0 race-start
// values, seeds the 6-keyframe/300-tick/spark/skid/damage rings to -1
// (an "empty slot" sentinel these countdown-style rings share -- never
// equal to any real countdown value once ticking starts), and captures
// `starcar`/`car[j]` snapshots of the just-placed cars. `prepit` (true
// only before the very first reset ever) gates whether `starcar` gets
// captured here at all -- matches the Java's own guard.
void record_reset(Record *r, ContO *cars[], int32_t ncars);

void record_recy(Record *r, int32_t n, double n2, bool b, int32_t n3);
void record_recx(Record *r, int32_t n, double n2, int32_t n3);
void record_recz(Record *r, int32_t n, double n2, int32_t n3);

// Ports Record.java's rec(contO, n, squash, lastcolido, cntdest, im)
// (:305-418) -- the per-tick, per-car recorder. Called once per car per
// physics tick from the SAME loop iteration GameSparker.java calls it
// from (:945, right after the drive() loop and before checkstat --
// game.c's own tick loop mirrors that exact ordering). `n` is the car
// index (0-7), `squash`/`lastcolido`/`cntdest` are that car's own Mad
// fields (`mad->squash`/`mad->lastcolido`/`mad->cntdest`), `im` is
// always 0 in this single-player-only port (xtGraphics.im). Shifts the
// 300-tick position ring down by one and appends `contO`'s current
// pose, decrements every ring's countdown by one, stamps a fresh spark/
// skid ring slot when `contO->stg[l]`/`contO->sprk_` is active this
// tick, and calls record_cotchinow() the instant a car's `dest` timer
// crosses 230 (matching the exact "just got wasted" trigger the Java
// uses to freeze the replay snapshot).
void record_rec(Record *r, ContO *contO, int32_t n, int32_t squash, int32_t lastcolido,
                 int32_t cntdest, int32_t im);

/** Ports `cotchinow(wasted)` (Record.java:238-303) -- gated on `caught >=
 * 300` (a full 300-tick ring exists), deep-copies every live ring above
 * into its h-prefixed twin and sets `hcaught`. A no-op once `hcaught` is
 * already true for this race (matches the Java: `caught` only ever grows,
 * so the `>= 300` gate stays open, but nothing re-triggers it after the
 * first true call this race in practice since record_rec only calls this
 * again on a LATER dest-crossing, which would just recopy the same
 * (still-current) live ring -- kept exactly as loose as the Java rather
 * than adding a guard it doesn't have). */
void record_cotchinow(Record *r, int32_t wasted);

/** Ports play(contO, mad, n, n2) (Record.java:420-492) -- reconstructs car
 * `n`'s pose/spark/dent state at LIVE ring index `n2` (0-299) into
 * `contO`, reapplying any dent recorded at exactly this tick via
 * record_regy/regx/regz (below). NOT used by the replay state (which
 * reads the frozen h* copy via record_playh instead) -- kept for
 * parity with the Java's own still-present method, not currently called
 * from game.c (no in-race "instant replay while still driving" UI exists
 * in this port, matching the original's own multiplayer-only use of this
 * particular method per GameSparker.java's own call sites). */
void record_play(Record *r, ContO *contO, struct Mad *mad, int32_t n, int32_t n2);

/** Ports playh(contO, mad, n, lastfr, n2) (Record.java:494-577) -- the
 * REPLAY's own reader: reconstructs car `n`'s pose/spark/dent state at
 * FROZEN ring index `lastfr` (0-299) from the h* snapshot into `contO`.
 * `n2` is `xtGraphics.im` (always 0); only used to decide whether to
 * also restore `contO->m->checkpoint`/`lastcheck` (only for the locally-
 * played car). `r->lastfr` (the PREVIOUS call's `lastfr`) distinguishes
 * "just landed on this exact tick" (apply the dent via record_regy/regx/
 * regz) from "already showing this tick, called again mid-frame" (apply
 * via record_chipx/chipz instead, matching the Java's own `this.lastfr
 * != lastfr` branches exactly) -- updates `r->lastfr = lastfr` at the end
 * either way. */
void record_playh(Record *r, ContO *contO, struct Mad *mad, int32_t n, int32_t lastfr, int32_t n2);

// Replay-time damage reapplication (Record.java:610-917) -- separate
// from Mad.java's own regy/regx/regz/chipx/chipz (mad.c's mad_regy/
// mad_regx/mad_regz): no re-recording into the ring buffers (this IS the
// ring buffer being read back), no multiplayer/camera-shake side effects,
// and -- confirmed by direct comparison, not assumed -- a GENUINELY
// DIFFERENT bruise-recolor clamp ladder than mad_recolor_plane's live
// version (0.2/0.5/0.1/0.05/0.8/0.075/0.05 here vs mad.c's own
// 0.25/0.7/0.15/0.6/0.075/0.5/0.05) -- a real divergence in the original
// game between live and replayed damage color, not a transcription slip,
// preserved via record_recolor_plane() in record.c rather than sharing
// mad_recolor_plane. `n` is the corner/quadrant index (0-3, into
// `contO->keyx`/`keyz`), `mad` supplies `cd`/`cn`/`im` (CarDefine
// constants + car index) and `mad->m` (the shared Medium, for
// medium_random/sin/cos -- Record.java keeps its own `this.m` reference
// to the SAME singleton, so reading it via `mad->m` here is not a
// divergence, just one fewer redundant pointer to keep in sync).
void record_regy(Record *r, int32_t n, float a, bool b, ContO *contO, struct Mad *mad);
void record_regx(Record *r, int32_t n, float a, ContO *contO, struct Mad *mad);
void record_regz(Record *r, int32_t n, float a, ContO *contO, struct Mad *mad);
void record_chipx(int32_t n, float a, ContO *contO, struct Mad *mad);
void record_chipz(int32_t n, float a, ContO *contO, struct Mad *mad);

#ifdef __cplusplus
}
#endif

#endif
