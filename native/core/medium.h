// ports web/Medium.js
//
// PARTIAL PORT. Medium.js is 1582 lines: the world's fog/sky/ground state,
// its own PRNG, camera modes (follow/around/watch/transaround), and the
// procedural generators for ground polys, clouds, mountains and stars, plus
// their draw methods. This header declares the FULL field layout (every
// field the JS constructor initialises, in the same order) so later work
// can extend this struct without reshaping it. Ported so far: medium_init,
// medium_sin/cos (fractional-index lerp), medium_xs/ys (perspective
// projection), medium_rot, medium_random (the game's own correlated PRNG),
// medium_follow (the default chase camera), medium_groundpolys, and
// medium_d (the world backdrop draw -- minus stars/mountains/clouds, see
// medium_d's own doc comment for why that's safe, not an approximation),
// medium_aroundtrack (the dive-and-orbit camera driving the stage-select
// 3D preview -- see its own doc comment below), medium_around (the
// pre-race starting-grid flyby camera), and medium_transaround (the
// replay's camera crossfade between two cars -- see their own doc
// comments below).
//
// NOT ported yet: getaround/getfollow (both netplay-spectator cameras)
// (the other camera modes), newpolys/newclouds/newmountains/newstars
// (procedural generation, needs JavaRandom + Trackers), drawclouds/
// drawmountains/drawstars (need the generators above first), setsky/
// setcloads/setgrnd/setexture/setpolys/setfade/fadfrom/adjstfade (stage-load
// setters), addsp/setsnap (trivial, just not needed by anything ported
// yet). Their target fields exist in the struct (as NULL/zero pointers for
// the not-yet-allocated generation arrays) so porting them later is
// additive, not a restructure.
#ifndef NFM_MEDIUM_H
#define NFM_MEDIUM_H

// Forward-declared at file scope (not inside a parameter list -- that would
// give it prototype scope instead, a distinct incomplete type each time,
// which fails to unify with gfx.h's actual `struct Graphics2D` definition
// later in the same translation unit). Callers that need to actually touch
// a Graphics2D must include gfx.h themselves; this header only needs the
// pointer type.
struct Graphics2D;

// Same forward-declaration reasoning as Graphics2D above -- cont_o.h
// itself includes this header (ContO embeds a Medium*), so this header
// can't include cont_o.h back. Callers that touch a ContO must include
// cont_o.h themselves.
struct ContO;

#include <stdint.h>
#include <stdbool.h>
#include "check_points.h"
#include "java_compat.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Medium {
  int32_t focus_point;
  int32_t ground;
  int32_t skyline;
  int32_t fade[16];
  // The stage's own fog bands (the defaults, or its fadefrom(), which
  // the original never resets between stages) and the Draw Distance
  // percent fade[] is them scaled by.
  int32_t fade_base[16];
  int32_t fade_pct;
  int32_t cldd[5];
  int32_t clds[3];
  int32_t osky[3];
  int32_t csky[3];
  int32_t ogrnd[3];
  int32_t cgrnd[3];
  int32_t texture[4];
  int32_t cpol[3];
  int32_t crgrnd[3];
  int32_t cfade[3];
  int32_t snap[3];
  int32_t fogd;
  int32_t mgen;
  bool loadnew;
  bool lightson;
  bool darksky;
  int32_t lightn;
  int32_t lilo;
  bool lton;
  int32_t noelec;
  int32_t trk;
  bool crs;
  int32_t cx, cy, cz;
  int32_t xz, zy;
  // Fractional part added to xz/zy by the draw code only: non-zero just
  // while a smooth-frames pass draws a camera blended between two ticks
  // (the angles are whole degrees in the game; a whole-degree step on a
  // turn is ~13px of yaw). Zero leaves every draw bit-identical.
  float fxz, fzy;
  // The same for the object being drawn (cont_o_d sets them from its ContO's
  // fxz/fxy/fzy around its planes): smooth frames' blended car angles.
  float ofxz, ofxy, ofzy;
  int32_t x, y, z;
  int32_t iw, ih;
  int32_t w, h;
  int32_t nsp;
  int32_t spx[7], spz[7], sprad[7];
  bool td;
  int32_t bcxz;
  double bcxzwatch; // Extended's far camera (medium_watch_far)
  bool bt;
  int32_t vxz;
  int32_t adv;
  bool vert;
  float tcos[360];
  float tsin[360];
  int32_t lastmaf;
  int32_t checkpoint;
  bool lastcheck;
  float elecr;
  bool cpflik;
  // Not in the Java -- see web/Medium.js's own comment on this field.
  bool interpolating;
  // The tick draw's random sequence, so an interpolated pass can replay it.
  // Grows (doubles) like the JS's, starting at 8192 entries.
  float *rlog;
  int32_t rlog_cap;
  int32_t rn;
  int32_t rp;
  bool recording;
  bool nochekflk;
  // SIM bank (medium_random consumes this outside the draw phase) and DRAW
  // bank (inside it) -- see the split rationale in web/Medium.js/java.js.
  int32_t cntrn;
  bool diup[3];
  int32_t rand[3];
  int32_t trn;
  int32_t dcntrn;
  bool ddiup[3];
  int32_t drand[3];
  int32_t dtrn;
  int32_t hit;
  int32_t ptr;
  int32_t ptcnt;
  int32_t nrnd;
  int32_t trx, trz;   // JS: plain assignment/+= only, no fr()/trunc() -- int
  int32_t atrx, atrz; // JS: assigned from ldiv() (integer division) -- int
  int32_t fallen;
  float fo;
  float gofo;
  int32_t fvect;

  // --- procedural generation state: NOT allocated by medium_init yet ---
  // (newpolys/newclouds/newmountains/newstars not ported). All NULL/0 until
  // then; medium_free() is already written to tear them down once they are.
  int32_t **ogpx, **ogpz; // [cells][8]
  float **pvr;            // [cells][8]
  int32_t *cgpx, *cgpz;   // [cells]
  int32_t *pmx;           // [cells]
  float *pcv;             // [cells]
  int32_t sgpx, sgpz;
  int32_t nrw, ncl, noc;
  int32_t *clx, *clz, *cmx;     // [noc]
  int32_t ***clax, ***clay, ***claz; // [noc][3][12]
  int32_t ****clc;               // [noc][2][6][3]
  int32_t nmt;
  int32_t *mrd, *nmv;    // [nmt]
  int32_t **mtx, **mty, **mtz; // [nmt][nmv[i]*2], ragged
  int32_t ***mtc;        // [nmt][nmv[i]][3], ragged
  int32_t nst;
  int32_t *stx, *stz;    // [nst]
  int32_t ***stc;        // [nst][2][3]
  bool *bst;             // [nst]
  int32_t *twn;          // [nst]
  int32_t resdown;
  int32_t rescnt;
} Medium;

// Zeroes *m, then runs the JS constructor's field initialisers in order.
// mgen/gofo are seeded from the shared java_compat PRNG (nfm_random()) --
// call nfm_set_seed() first for a reproducible instance, same as the JS
// needing java.js's setSeed() called before `new Medium()`.
void medium_init(Medium *m);

// Frees every procedural-generation pointer above (safe on a freshly
// medium_init'd or already-freed instance, since those start NULL). Does
// NOT free `m` itself.
void medium_free(Medium *m);

// Perspective projection. All-int in Java, where the product wraps past
// 2^31 -- only for a point both far away and far off-centre, which the race
// never draws (its fog culls first) but the stage select's high dive does:
// pieces of the big stages (8, 9) projected to garbage and vanished. In 64
// bits the result is the Java's wherever the Java did not wrap, and right
// where it did (and signed overflow is undefined in C anyway).
static inline int32_t medium_xs(Medium *m, int32_t n, int32_t cz) {
  if (cz < m->cz) cz = m->cz;
  return (int32_t)((int64_t)(cz - m->focus_point) * (m->cx - n) / cz + n);
}
static inline int32_t medium_ys(Medium *m, int32_t n, int32_t n2) {
  if (n2 < m->cz) n2 = m->cz;
  return (int32_t)((int64_t)(n2 - m->focus_point) * (m->cy - n) / n2 + n);
}

// Table lookup with fractional-index lerp -- see the JS's comment on why
// the lerp exists (interpolated-frame heading smoothing). Pass a whole
// number for the exact bit-identical simulation path.
static inline float medium_cos(Medium *m, float i) {
  while (i >= 360.0f) i -= 360.0f;
  while (i < 0.0f) i += 360.0f;
  int32_t i0 = (int32_t)i;
  if ((float)i0 == i) return m->tcos[i0];
  double a = m->tcos[i0];
  double b = m->tcos[i0 + 1 == 360 ? 0 : i0 + 1];
  return (float)(a + (b - a) * ((double)i - (double)i0));
}
static inline float medium_sin(Medium *m, float i) {
  while (i >= 360.0f) i -= 360.0f;
  while (i < 0.0f) i += 360.0f;
  int32_t i0 = (int32_t)i;
  if ((float)i0 == i) return m->tsin[i0];
  double a = m->tsin[i0];
  double b = m->tsin[i0 + 1 == 360 ? 0 : i0 + 1];
  return (float)(a + (b - a) * ((double)i - (double)i0));
}

// Rotate a point set about (n, n2) by n3 degrees, in place.
static inline void medium_rot(Medium *m, int32_t *array, int32_t *array2, int32_t n, int32_t n2, float n3, int32_t n4) {
  if (n3 != 0) {
    float cos = medium_cos(m, (float)n3);
    float sin = medium_sin(m, (float)n3);
    for (int32_t i = 0; i < n4; i++) {
      int32_t n5 = array[i];
      int32_t n6 = array2[i];
      array[i] = n + jtrunc(((float)(n5 - n) * cos) - ((float)(n6 - n2) * sin));
      array2[i] = n2 + jtrunc(((float)(n5 - n) * sin) + ((float)(n6 - n2) * cos));
    }
  }
}

// The game's own correlated PRNG (distinct from java_compat's nfm_random,
// which this calls internally as its underlying source). See web/Medium.js
// for why successive calls are deliberately correlated (walks three digits
// up/down) and why sim/draw are separate banks.
float medium_random(Medium *m);

// Chase camera -- the one the race uses by default (view == 0). Takes the
// followed car's position as three ints rather than a ContO* : ContO.js
// isn't ported yet (M2), and this is the only thing follow() reads from it,
// so this stays reusable once ContO exists instead of blocking on it.
void medium_follow(Medium *m, int32_t car_x, int32_t car_y, int32_t car_z, int32_t n, int32_t n2);

// Dive-and-orbit camera for the stage-select 3D preview -- Medium.java:
// 304-380's aroundtrack(). Two phases, switched on m->hit (armed to 45000
// by the caller when a stage is (re)loaded, matching GameSparker.java:
// 2735-2749's `xtGraphics.fase == 2` block):
//   - m->hit > 5000: a scripted dive from high altitude down to cruise
//     height while orbiting the track's bounding-box center (m->trx/trz)
//     at a fixed 17000 radius, angle m->vxz advancing 3 deg/frame.
//   - m->hit <= 5000 (settled): pans the look-at point checkpoint-to-
//     checkpoint every 7 frames (m->ptr/ptcnt, m->nrnd counts full
//     sweeps), with a slowly breathing depth-of-fog focus (m->fo/gofo).
// `cp` must be the same CheckPoints the previewed stage was loaded into
// (its x[]/z[]/n are read directly) -- reads checkPoints.x[0]/z[0] once
// per dive (m->hit==45000/20000) and checkPoints.x[ptr]/z[ptr] every
// settled frame, so `cp->n` must be >= 1 before calling this.
// NOTE: two decompiled lines at Medium.java:373-374 (an unused local
// distance computation and an empty `if` body) are omitted here, matching
// web/Medium.js's own aroundtrack() -- dead decompiler artifacts with no
// observable effect (the computed value is never read, the if-body is
// empty), not a behavioural quirk to preserve.
void medium_aroundtrack(Medium *m, CheckPoints *cp);

// Pre-race starting-grid flyby camera -- Medium.java:382-431's around().
// Orbits `co` (the ContO to circle) at a radius that swings between 500
// and 1400+`adv` world units, `adv` itself oscillating between -500 and
// 900 in the ordinary (`b == false`) mode -- used elsewhere for the
// player-selectable "around" race view (m->view == 1 in
// GameSparker.java's fase==0 tick, not currently wired into this port,
// no in-race view switching exists yet). `b == true` is the FAST variant
// this port DOES need: GameSparker.java:958-1017's fase==0 tick uses
// medium.around(car, true) unconditionally for every tick from
// xtGraphics.starcnt==130 (race start) down to starcnt==38 -- a ~1.5s
// cinematic orbit around the player's own car sitting on the starting
// grid, shown BEFORE the 3-2-1-GO countdown text and the normal chase
// camera take over. `b` narrows the orbit radius floor to 1300+ and
// speeds up both the dolly-in (`adv -= 14` vs `+=2/-=2`) and the orbit
// angle (`vxz += 4` vs `+= 2`).
void medium_around(Medium *m, struct ContO *co, bool b);

// Replay-only camera crossfade -- Medium.java:582-620's transaround().
// Same orbit shape as medium_around()'s ordinary (b==false) mode, but
// orbiting a point LERPED between two cars (`from`/`to`) instead of one
// fixed ContO: `t` is 0..20, the lerp weight toward `to` (0 = fully on
// `from`, 20 = fully on `to`). Used by the post-race replay's THREE
// camera-choreography variants (GameSparker.java:1499-1599 -- crash,
// close-finish, and normal-win reels each interleave medium_around() on
// one car, medium_transaround() sweeping to the other, medium_around()
// on that other car, and so on) to swing the "around" orbit from one car
// to another over 20 ticks rather than cutting instantly.
void medium_transaround(Medium *m, struct ContO *from, struct ContO *to, int32_t t);

/** Medium.java:274-302 -- the "watch" camera, view 2 of the three the V
 * key cycles through in-race (0 = follow/chase, 1 = around/orbit, 2 =
 * this). A fixed tripod that plants itself once, then only re-aims at the
 * car, re-planting when the car passes 6000 units away. `n` is the car's
 * heading (Java passes `mad.mxz`), used only when choosing a new spot. */
/** Extended's far camera (its Medium.watch, :1971-1994): 12000 behind and
 * 3750 above the car, turning with `angle` (the car's cxz / 15). */
void medium_watch_far(Medium *m, const struct ContO *co, double angle, int32_t boost);

void medium_watch(Medium *m, struct ContO *co, int32_t n);

// Procedural ground-poly scatter -- draws into cells populated by newpolys()
// (not ported yet, M1 remainder). Safe to call before that exists: nrw/ncl
// are 0 until then, which makes every loop here empty -- verified, not
// assumed, see native/tests/medium_test.c.
void medium_groundpolys(Medium *m, struct Graphics2D *g);

// Draws the world backdrop: ground/sky/fog gradient bands, then (if
// resdown != 2) 19 more atmosphere bands, then ground polys. Does NOT call
// drawstars/drawmountains/drawclouds (not ported -- their data stays at
// zero count until newclouds/newmountains/newstars exist, so omitting the
// calls is behaviourally identical to calling them on an empty scene, not
// an approximation -- see native/tests/medium_test.c and TASKS_NATIVE.md).
void medium_d(Medium *m, struct Graphics2D *g);

/** Queues a shadow-blend blob (ground-poly darkening under an object's
 * shadow) at world (x,z) with radius n3. Silently drops the call once 7
 * are queued in a frame, matching the JS. */
void medium_addsp(Medium *m, int32_t x, int32_t z, int32_t n3);

// --- Stage-load setters (a lean subset -- see TASKS_NATIVE.md) ---
//
// Ported: setsnap/setsky/setgrnd/setfade, the four a stage file's
// `snap(`/`sky(`/`ground(`/`fog(` commands need to make medium_d's already-
// ported gradient bands actually reflect a STAGE's colours instead of
// medium_init's hardcoded defaults. NOT ported: setcloads/setexture/
// setpolys/fadfrom/adjstfade (`clouds(`/`texture(`/`polys(`/`fadefrom(`/
// dynamic fade adjustment) -- refinements to the same colours medium_d
// already draws with reasonable defaults from setgrnd, not a gap in
// whether the stage's geometry or base colours appear at all.

/** `(int)(v + v * (snap/100f))`, clamped to [0,255] -- the shape every
 * colour setter below repeats (web/Medium.js's own `snapped()` helper). */
int32_t medium_snapped(int32_t v, int32_t snap);

void medium_setsnap(Medium *m, int32_t n, int32_t n2, int32_t n3);
void medium_setsky(Medium *m, int32_t n, int32_t n2, int32_t n3);
void medium_setgrnd(Medium *m, int32_t n, int32_t n2, int32_t n3);
void medium_setexture(Medium *m, int32_t n, int32_t n2, int32_t n3, int32_t n4);
void medium_setpolys(Medium *m, int32_t n, int32_t n2, int32_t n3);
void medium_setfade(Medium *m, int32_t n, int32_t n2, int32_t n3);
/** `clouds(r,g,b,mix,height)` and `fadefrom(d)` lines. */
void medium_setcloads(Medium *m, int32_t n, int32_t n2, int32_t n3, int32_t n4, int32_t n5);
void medium_fadfrom(Medium *m, int32_t n);

/** The stage backdrop's procedural parts, generated after the stage file
 * is read (bounds from its maxl/maxr/maxt/maxb): the ground patches around
 * the track (seeded from the stage, kept off the road by `t`), the clouds
 * (from the race's random stream), the mountains (seeded by `mountains(`)
 * and, on a `lightson` stage, the stars. */
struct Trackers;
void medium_newpolys(Medium *m, int32_t n, int32_t n2, int32_t n3, int32_t n4, const struct Trackers *t, int32_t n5);
void medium_newclouds(Medium *m, int32_t n, int32_t n2, int32_t n3, int32_t n4);
void medium_newmountains(Medium *m, int32_t n, int32_t n2, int32_t n3, int32_t n4);
void medium_newstars(Medium *m);
void medium_drawclouds(Medium *m, struct Graphics2D *g);
void medium_drawmountains(Medium *m, struct Graphics2D *g);
void medium_drawstars(Medium *m, struct Graphics2D *g);

/** Settings > Graphics > Draw Distance: the fog bands (and with them where
 * objects stop being drawn) at `percent` of the original's distances --
 * 100 is the original. Drawing only: nothing in the simulation reads fade. */
void medium_draw_distance(Medium *m, int32_t percent);

#ifdef __cplusplus
}
#endif

#endif
