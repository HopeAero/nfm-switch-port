// ports web/Trackers.js
#ifndef NFM_TRACKERS_H
#define NFM_TRACKERS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// NFM 2 holds 6700; Extended's own stages need more (its Trackers has 67000;
// its biggest career stage places ~5000), so 20000. Stages fill trackers
// from the front, so NFM 2's stages are untouched.
#define TRACKERS_MAX 20000

// ~1.2MB of fixed arrays -- heap- or static-allocate, never put this on the
// stack. `trackers_init` only zeroes it; the caller owns storage.
typedef struct Trackers {
  int32_t x[TRACKERS_MAX];
  int32_t y[TRACKERS_MAX];
  int32_t z[TRACKERS_MAX];
  int32_t xy[TRACKERS_MAX];
  int32_t zy[TRACKERS_MAX];
  int32_t skd[TRACKERS_MAX];
  int32_t dam[TRACKERS_MAX];
  bool notwall[TRACKERS_MAX];
  bool decor[TRACKERS_MAX];
  int32_t c[TRACKERS_MAX][3];
  int32_t radx[TRACKERS_MAX];
  int32_t radz[TRACKERS_MAX];
  int32_t rady[TRACKERS_MAX];
  int32_t nt;
  int32_t sx;
  int32_t sz;
  // NOTE (ports the JS verbatim): after devidetrackers() runs, ncx/ncz are
  // ONE LESS than the actual grid dimensions used to build `sect` -- the
  // JS decrements them post-build (Trackers.js:83-84) and callers (e.g.
  // Plane.s()) clamp against this already-decremented value. Preserve the
  // off-by-one; it is load-bearing, not a bug to "fix" here.
  int32_t ncx;
  int32_t ncz;
  // Ragged [ncx+1][ncz+1] grid (see the ncx/ncz note above for why it's
  // +1), each cell a heap array of tracker indices. NULL until
  // trackers_devidetrackers() runs.
  int32_t ***sect;
  int32_t **sect_len; // sect_len[i][j] = length of sect[i][j]
} Trackers;

void trackers_init(Trackers *t);

// Frees `sect`/`sect_len` (safe to call on a zeroed or already-freed
// Trackers). devidetrackers() calls this itself before rebuilding.
void trackers_free_sect(Trackers *t);

void trackers_devidetrackers(Trackers *t, int32_t sx, int32_t n, int32_t sz, int32_t n2);

/**
 * The same sector grid (same sx/sz/ncx/ncz, same `sect` layout and the same
 * post-build decrement, so every reader works unchanged) for Extended's
 * stages, built by COVERAGE instead of distance: a tracker goes into every
 * cell its x/z rectangle, grown by 1500, touches -- cells clamped to the
 * grid, as readers clamp a car's cell. Extended has no grid (it scans every
 * tracker, Madness.java:1980, 2259); NFM 2's distance-from-centre buckets
 * miss its long and floating pieces and its walls (no 167 marker), so a
 * car would drive through them. With the margin, any tracker a wheel within
 * 1500 of the car's centre can touch is in the centre's cell (as
 * web/ext/trackgrid.js); each cell lists its trackers ascending.
 */
void trackers_devidetrackers_cover(Trackers *t, int32_t sx, int32_t n, int32_t sz, int32_t n2);

// Squared planar distance. Wraps at 32 bits on both the multiplies and the
// addition, matching Java `iadd`/`imul` -- see web/Trackers.js's comment on
// why that is load-bearing for which trackers land in which sector.
int32_t trackers_py(int32_t n, int32_t n2, int32_t n3, int32_t n4);

#ifdef __cplusplus
}
#endif

#endif
