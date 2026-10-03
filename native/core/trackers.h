// ports web/Trackers.js
#ifndef NFM_TRACKERS_H
#define NFM_TRACKERS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TRACKERS_MAX 6700

// ~350KB of fixed arrays -- heap- or static-allocate, never put this on the
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

// Squared planar distance. Wraps at 32 bits on both the multiplies and the
// addition, matching Java `iadd`/`imul` -- see web/Trackers.js's comment on
// why that is load-bearing for which trackers land in which sector.
int32_t trackers_py(int32_t n, int32_t n2, int32_t n3, int32_t n4);

#ifdef __cplusplus
}
#endif

#endif
