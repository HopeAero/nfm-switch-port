// ports web/ContO.js
//
// PARTIAL PORT. ContO.js is 2234 lines: a car (or track decoration)'s full
// state -- its Planes, wheels, damage/repair animation, spark/dust
// particles, collision -- plus THREE different constructors dispatched on
// argument shape (`#initBuf` parses a `.rad` file's bytes; `#initModel`
// procedurally generates decoration objects like trees from a seed;
// `#initCopy` clones an existing ContO, used for AI opponents). Only
// `#initBuf` is ported here -- it's the one CarDefine.js's loadcar() uses
// (`new ContO(bytes, this.m, this.t)`), and it's the piece actually needed
// to load a car's geometry.
//
// This header declares the FULL field layout the JS constructor sets (same
// order) so later work is additive. `cont_o_init_buf` implements the
// line-by-line `.rad` command parser (web/ContO.js lines 126-456: `p(`/
// `gr(`/`fs(`/`c(`/`glass`/`gshadow`/`light*`/`w(` [wheels]/`tracks`+
// `<track>` blocks/`disp(`/`disline(`/`shadow`/`stonecold`/`newstone`/
// `decorative`/`road`/`notroad`/`grounded(`/`div(`/`idiv(`/`iwid(`/
// `ScaleX/Y/Z(`/`gwgr(`/`1stColor(`/`2ndColor(`).
//
// The `this.m.loadnew`-gated post-processing pass (JS lines 457-829) IS
// ported: it computes each face's bounding box, classifies which axis
// it's "thin" along, and flips `fs` (the decal/glass flip-side flag) by
// comparing against neighbouring faces sharing an edge. Initially scoped
// as deferrable ("affects some glass/decal panels"), but real-file oracle
// testing against mycars/Simple_Car.rad showed it actually sets `fs` for
// the majority of real faces -- not an edge case -- so it was ported in
// full. See native/tests/cont_o_test.c for the verification.
//
// `d()` (the runtime draw entry point) IS also ported, along with the two
// helpers it depends on (`xs`/`ys` -- ContO's OWN perspective projection,
// not the same function as Medium's/Plane's despite the identical name and
// shape, see the doc comment on cont_o_d below). See cont_o_d's own doc
// comment for the exact scope: everything reachable for a freshly-parsed,
// undamaged car with no stage loaded is ported; the handful of branches
// that are only reachable once physics/damage/stage state exists
// (`lowshadow`, `electrify`, `fixit`, `pdust`, `dsprk`) were originally
// stubbed to abort loudly if ever actually hit, not silently
// approximated. `electrify`/`pdust`/`dsprk` turned out to be reachable
// almost immediately once mad_drive() was wired into a real game loop
// (native/platform/linux/main.c) -- driving over uneven ground alone
// triggers dust() most ticks, and any wall scrape triggers sprk() -- so
// all three are now ported in full. `lowshadow` turned out the same way
// once AI opponents (native/core/bots.c/control.c) gave the chase camera
// cars to actually be far from -- also now ported in full. `fixit` was
// the last one still stubbed, on the theory that it's only reachable
// during a repair-pit animation -- but mad.c sets co->fix for real
// whenever a damaged car nears a `fix(`-placed repair point (an ordinary
// stage feature, not an edge case), so the stub's abort() was a
// guaranteed crash reachable through normal play; also now ported in
// full (see cont_o.c's own comment on cont_o_fixit for the two effects
// it drives).
//
// NOT ported: `#initModel` (the procedural-decoration constructor --
// `#initCopy` IS ported, see below) and `py`/`getpy` (Mad.js has its own
// equivalents, used instead -- see mad.c).
#ifndef NFM_CONT_O_H
#define NFM_CONT_O_H

#include <stdint.h>
#include <stdbool.h>
#include "medium.h"
#include "trackers.h"
#include "plane.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Settings > Graphics: draw cars' and objects' shadows / dust and sparks.
 * Off still runs the code that draws them (it advances particle state, and
 * the shadow pass sits beside the depth-sort distance); only what it drew is
 * dropped (gfx_rewind), so nothing but the picture changes. Default true. */
extern bool cont_o_shadows;
extern bool cont_o_particles;

#define CONT_O_MAX_PLANES 286 // 210 body panels (guarded in the parser) + 4*19 wheel planes

typedef struct ContO {
  int32_t npl;
  int32_t x, y, z;
  int32_t xz, xy, zy;
  // Fractions of a degree on top of xz/xy/zy, drawing only: smooth frames
  // blend a car's angles between ticks (0 on every tick and in the sim).
  float fxz, fxy, fzy;
  int32_t wxz, wzy;
  int32_t dist;
  int32_t maxR;
  int32_t disp;
  int32_t disline;
  bool shadow;
  bool noline;
  bool decor;
  float grounded;
  int32_t grat;
  int32_t keyx[4], keyz[4];
  int32_t sprkat;

  // Track (collision volume) data, allocated when a `tracks` command is
  // seen. NULL until then. Length is whatever `tracks(N)` specified.
  int32_t *txy, *tzy;
  int32_t (*tc)[3];
  int32_t *tradx, *tradz, *trady;
  int32_t *tx, *ty, *tz;
  int32_t *skd, *dam;
  bool *notwall;
  int32_t tnt_cap; // allocated length of the arrays above
  int32_t tnt;     // count actually filled

  int32_t *stg; // int[20], allocated when shadow is true (by #initBuf OR #initCopy)
  int32_t *rtg; // int[100], allocated when shadow is true (by #initBuf OR #initCopy)

  // Allocated ONLY by #initCopy (cont_o_init_copy), when shadow is true --
  // #initBuf-parsed base models never touch these, matching the JS (see
  // cont_o_init_copy's doc comment). NULL until then.
  int32_t *sx, *sy, *sz, *scx, *scz; // int[20]
  float *osmag;                     // float[20]
  int32_t *sav;                     // int[20]
  float (*smag)[8];                 // float[20][8]
  int32_t (*srgb)[3];               // int[20][3]
  float *sbln;                      // float[20]
  int32_t ust;
  int32_t srx, sry, srz;
  float rcx, rcy, rcz;
  int32_t sprk_;
  bool *rbef;                       // bool[100]
  int32_t *rx, *ry, *rz;            // int[100] each
  float *vrx, *vry, *vrz;           // float[100] each
  bool elec;
  bool roted;
  int32_t edl[4], edr[4];
  int32_t elc[4];
  bool fix;
  int32_t fcnt;
  int32_t checkpoint;
  int32_t fcol[3], scol[3];
  int32_t colok;
  bool errd;
  char err[1024]; // JS: a free-form error string; sized generously, truncated if longer
  int32_t roofat;
  int32_t wh;

  // Not part of the JS's own field list; ContO.js has no equivalent, since
  // it never needs to know its own array index. Set only for #initBuf-
  // loaded base models by GameSparker.loadbase() (native equivalent, not
  // yet ported) -- see the JS's own comment on `this.baseIndex` in
  // #initCopy. -1 (unset) otherwise.
  int32_t baseIndex;

  Medium *m;
  Trackers *t;
  // Heap-allocated, sized to EXACTLY `npl` once known -- NOT
  // CONT_O_MAX_PLANES. #initBuf parses into a CONT_O_MAX_PLANES-capacity
  // scratch buffer (npl isn't known upfront) then shrinks with realloc;
  // #initCopy allocates exactly contO->npl slots directly, matching the
  // JS's `objArray(contO.npl)`. A fixed 286-slot array per instance was
  // fine for a handful of cars but not for the ~610 objects a stage places
  // (~71KB/instance * 610 = ~42MB) -- the JS's own `objArray(286)` is cheap
  // because JS arrays are sparse (null until assigned); a C struct array
  // is not.
  Plane *p;
} ContO;

/**
 * Ports web/ContO.js's `#initBuf(buf, m, t)` -- the `.rad`-file parser.
 * `text` is the file's contents (NOT NUL-only-safe binary -- this is a
 * text format, pass a NUL-terminated string as vfs_read_text() returns).
 * `m`/`t` are borrowed, not owned. Zeroes and fully initialises `co`.
 */
void cont_o_init_buf(ContO *co, const char *text, Medium *m, Trackers *t);

/**
 * Ports web/ContO.js's `#initCopy(contO, x, y, z, a)` -- clones a BASE MODEL
 * (a `co` produced by `cont_o_init_buf`, one entry of `GameSparker`'s
 * `ContO[124]` loaded via `loadbase()`) into a placed INSTANCE at world
 * position (x,y,z), rotated `a` degrees. This is how every object other
 * than the player's own base-model reference actually gets into the world
 * -- track pieces, walls, the player's own car once GameSparker's `#draw`
 * rebuilds it (see that method's `array2[n33] = new ContO(this.baseModels[...])`
 * call, not itself ported). Copies each `Plane` (not shares -- new
 * `plane_init` calls, matching `new Plane(...)` in the JS), rotates their
 * `ox`/`oz` by `a`, and re-emits `src`'s `tracks(...)` collision volumes
 * into `dst->t` at the new position/rotation. Allocates the shadow/dust/
 * spark-trail arrays (`sx`/`sy`/`sz`/.../`vrz`) when `dst->shadow` is true
 * -- unlike `#initBuf`, which never touches them (see the JS's own field
 * defaults). `dst` is zeroed and fully initialised.
 *
 * `src` is NOT const: the JS mutates it (`contO.p[i].n = 20` for any
 * `master === 1` plane, growing it before copying -- see the JS's own
 * `#initCopy`), matching how `master` objects grow at draw time too (see
 * `plane_d`'s doc comment). In practice this is dead for every base model
 * this port can currently produce: `master` is 0 unless something sets it
 * nonzero, and nothing does -- `#initBuf` never touches `Plane.master`
 * (only `#initModel`, procedural decoration generation, does, and that
 * constructor isn't ported). Ported literally anyway rather than silently
 * dropped, with a comment at the call site on the real hazard it would
 * cause if it ever DID fire (the source's `ox`/`oz`/`oy` are allocated to
 * its ORIGINAL smaller `n`, not to 20).
 */
void cont_o_init_copy(ContO *dst, ContO *src, int32_t x, int32_t y, int32_t z, int32_t a);

/**
 * Ports ContO.java's procedural constructor (the stage file's
 * `pile(seed,size,height,x,z)`): a five-face dirt hill at (x, y, z) shaped by
 * java.util.Random(seed), plus its five Trackers (four slopes, a flat top).
 * `co` is zeroed and fully initialised.
 */
void cont_o_init_pile(ContO *co, int32_t seed, int32_t n2, int32_t n3, Medium *m, Trackers *t,
                      int32_t x, int32_t z, int32_t y);

/**
 * cont_o_init_copy() over a ContO that may already own allocations:
 * frees `dst` first, then copies. init_copy itself starts with a memset,
 * so calling it on a live ContO drops every Plane and array it held --
 * which the replay ring (record_rec, every ~6 ticks per car), the
 * newcar rebuild and the replays all did, leaking ~50KB per car copy
 * (~55MB per minute of a 7-car race). `dst` must be a valid ContO or
 * all-zero, never uninitialised memory; `dst != src`.
 */
void cont_o_recopy(ContO *dst, ContO *src, int32_t x, int32_t y, int32_t z, int32_t a);

/** Frees every Plane in co->p[0..npl), co->p itself, and any allocated
 * track/shadow/dust/spark arrays. */
void cont_o_free(ContO *co);

// --- Runtime draw path (JS lines 1194-2195) ---
//
// `d()` (the per-object draw entry point), its two tiny unconditional
// helpers (`xs`/`ys`, ContO's OWN perspective projection -- note these
// clamp against a literal 50, not `m.cz` like Medium's/Plane's xs/ys, so
// they are NOT the same function and cannot delegate to medium_xs/
// medium_ys), `rot` (delegates to medium_rot, same as Plane.rot -- see
// plane.h), `electrify`, `pdust`, `dsprk`, `lowshadow`, and `fixit` are
// all ported in full.
//
// All five were originally stubbed to abort loudly if ever actually hit,
// on the theory that a fresh, undamaged car with no stage placed never
// reaches any of them -- but that reasoning breaks down as soon as
// EITHER a real stage, real physics, or real AI opponents exist:
//   - `electrify`: a real stage file's `fix(` command sets `co->elec =
//     true` (game_sparker.c), and `cont_o_d` calls `electrify`
//     unconditionally whenever `elec` is true and `m->noelec == 0`.
//   - `pdust`: `cont_o_d` calls this whenever `co->stg[n] != 0`, which
//     Mad.js's `drive()` sets on almost any tick a wheel's suspension
//     travel differs from its rest position -- true for nearly all real
//     driving, not just skids.
//   - `dsprk`: called unconditionally whenever `co->shadow` is true, and
//     its own spawn logic fires whenever `co->sprk_ != 0` -- set by
//     `sprk()`, which `Mad.js`'s `regy`/`regx`/`regz` call on any wall
//     scrape.
//   - `lowshadow`: reachable the instant the chase camera can be far
//     from an object it's drawing -- true for every AI opponent as soon
//     as `native/core/bots.c`'s sortcars()/`control.c`'s preform() exist
//     and main.c drives more than one car.
//   - `fixit`: gated on `co->fix`, which `mad.c` sets true (matching
//     Mad.java) whenever a damaged car is near a `fix(`-placed repair
//     point -- an ordinary stage feature, not an edge case, so any
//     damaged car that ever reaches one reaches this.
// Each one's abort fired for real the first time it got the chance --
// electrify on the first real stage load, pdust/dsprk within a few
// hundred ticks of wiring mad_drive() into a real game loop (native/
// platform/linux/main.c), lowshadow the first time that same file drove
// AI opponents far from the player's camera, fixit the first time a
// damaged car reached a repair pad -- so all five are ported in full
// rather than re-scoped away. See cont_o.c for each one's translation.
static inline int32_t cont_o_xs(ContO *co, int32_t n, int32_t n2) {
  if (n2 < 50) n2 = 50;
  return (int32_t)((int64_t)(n2 - co->m->focus_point) * (co->m->cx - n) / n2 + n); // see medium_xs
}
static inline int32_t cont_o_ys(ContO *co, int32_t n, int32_t n2) {
  if (n2 < 50) n2 = 50;
  return (int32_t)((int64_t)(n2 - co->m->focus_point) * (co->m->cy - n) / n2 + n);
}

/** Rotate a point set about (n,n2) by n3 degrees -- identical math to
 * medium_rot/plane_rot (ContO.rot() calls this.m.cos/sin too), so this
 * just delegates rather than reimplementing it. */
static inline void cont_o_rot(ContO *co, int32_t *array, int32_t *array2, int32_t n, int32_t n2, float n3, int32_t n4) {
  medium_rot(co->m, array, array2, n, n2, n3, n4);
}

/** Draws one frame's worth of electric-fence bolts for a `fix(`-placed
 * object (co->elec == true). See the doc comment above for why this,
 * `pdust`, `dsprk`, `lowshadow`, and `fixit` are all ported in full. */
void cont_o_electrify(ContO *co, struct Graphics2D *g);

/** Draws one frame's worth of the "Car Fixed" flash for a damaged car
 * sitting on an active `fix(`-placed repair pad (co->fix == true, set by
 * mad.c). Advances co->fcnt itself (0..7, then clears co->fix and resets
 * to 0) -- see cont_o.c's own doc comment for the two effects this
 * drives. Exported (not static) so native/tests/cont_o_test.c can drive
 * it directly, same as cont_o_electrify above. */
void cont_o_fixit(ContO *co, struct Graphics2D *g);

/** Ports `dust()` (the seed-only half -- the JS's own `#dust`, called via
 * the `setDrawPhase`-wrapping public `dust()` which just exists to move
 * this onto the draw PRNG stream, see the JS comment on why). Called from
 * Mad.js's `drive()` on wheelspin/skid to seed one dust-puff particle slot
 * (sx/sy/sz/osmag/scx/scz/stg). The DRAWING half (`pdust`, animates and
 * renders the puff over several frames) is ALSO ported -- see the top of
 * this file for why -- as `cont_o_pdust` below, called from cont_o_d. */
void cont_o_dust(ContO *co, int32_t n, float n2, float n3, float n4, int32_t n5, int32_t n6, float n7, int32_t n8, bool b);

/** Draws/ages one frame of dust-puff particle slot `n` (ContO.java:1950-
 * 2105) -- fades in/out via alpha (see cont_o.c's own comment for the
 * fade formula), tracks the ground/track colour under it, and advances
 * its `stg` life-cycle counter. `b` selects which of cont_o_d's two
 * depth-sorted passes this call is (matches the object-visibility
 * distance cache's own true/false convention -- see cont_o.c). Exported
 * (not static) so native/tests/cont_o_test.c can drive it directly, same
 * as cont_o_electrify/cont_o_fixit above. */
void cont_o_pdust(ContO *co, int32_t n, struct Graphics2D *g, bool b);

/** Ports `sprk()` (the seed-only half -- writes srx/sry/srz/sprk_/rcx/rcy/
 * rcz). Called from Mad.js's `drive()`/`regy`/`regx`/`regz`/`colide()` on
 * scrape/impact to seed the spark-trail origin/velocity-bias state. The
 * DRAWING half (`dsprk`, animates and renders individual spark lines over
 * several frames, and owns the rtg/rbef/rx/ry/rz/vrx/vry/vrz ring buffer)
 * is ALSO ported -- see the top of this file for why -- as `cont_o_dsprk`
 * (static, called from cont_o_d). */
void cont_o_sprk(ContO *co, float n, float n2, float n3, float rcx, float rcy, float rcz, int32_t n4);

/** Ports `stepFix()` -- SIMULATION despite driving a repair-animation
 * visual effect (see the JS's own comment on why it lives here rather
 * than in the drawing code, and why it must run unconditionally once per
 * tick per car, NOT gated on on-screen visibility like the old Java did
 * -- that gating made repair completion diverge between netplay clients
 * with different cameras). No-op if `co->fix` is false. Once `co->fcnt`
 * passes 7, resets `fcnt` to 0 and `fix` to false; `mad_drive()` (see
 * mad.c) is what actually reads `fcnt === 7 || 8` and applies the
 * repair (clearing squash/hitmag/cntdest/dest). Call ONCE PER TICK PER
 * CAR, matching GameSparker.js's own `simulate()` ordering exactly:
 * AFTER that tick's `mad_drive()` call, not before -- `drive()` reads
 * `fcnt` as left by the PREVIOUS tick's `cont_o_step_fix()`, and this
 * tick's call advances it for the NEXT tick's `drive()` to read. */
void cont_o_step_fix(ContO *co);

/** Draws every plane of co (and, if co->shadow, its ground shadow), sorted
 * by Plane.av for the painter's algorithm exactly as the JS does. Also
 * updates co->dist for next frame's inter-object sort (see GameSparker.js's
 * #draw, not ported -- the object-level sort by `dist` across many ContOs
 * happens in the (not yet ported) scene draw loop, not here). */
void cont_o_d(ContO *co, struct Graphics2D *g);

#ifdef __cplusplus
}
#endif

#endif
