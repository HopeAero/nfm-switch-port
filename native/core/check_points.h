// ports web/CheckPoints.js
//
// PARTIAL PORT. Race state: checkpoint/lap layout for a stage, per-car race
// position and lap-clear counts, catch-up "photo finish" detection.
//
// Ported: the full field layout, `calprox` (stage-bounds helper used once
// at stage load), `py` (squared planar distance -- identical shape to
// trackers_py/cont_o's own py-equivalents, delegates rather than
// reimplementing), and `checkstat` (per-tick race position/lap
// computation across every car) -- called once per tick from
// GameSparker.js's own simulate(), same place cont_o_step_fix() is
// called from in this port's main.c (see cont_o.h).
#ifndef NFM_CHECK_POINTS_H
#define NFM_CHECK_POINTS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CHECK_POINTS_MAX 140

// Tagged (not just typedef'd) so other headers can forward-declare
// `struct CheckPoints *` without including this file -- see control.h.
typedef struct CheckPoints {
  int32_t x[CHECK_POINTS_MAX], z[CHECK_POINTS_MAX], y[CHECK_POINTS_MAX];
  int32_t typ[CHECK_POINTS_MAX];
  int32_t pcs;
  int32_t nsp;
  int32_t n;
  int32_t fx[5], fz[5], fy[5];
  bool roted[5], special[5];
  int32_t fn;
  int32_t stage;
  int32_t nlaps;
  int32_t nfix;
  bool notb;
  char name[256];
  char maker[256];
  int32_t pubt;
  char trackname[256];
  int32_t trackvol;
  int32_t top20;
  int32_t nto;
  int32_t pos[8], clear[8], dested[8];
  float magperc[8];
  int32_t wasted;
  bool haltall;
  int32_t pcleared;
  int32_t opx[8], opz[8], onscreen[8], omxz[8];
  int32_t catchfin;
  int32_t postwo;
  float prox;
} CheckPoints;

/** Zeroes and initialises cp -- note `stage` gets a random 1-27 default in
 * the JS (`trunc(random() * 27.0) + 1`), matching game_sparker_loadstage
 * overwriting it from the actual stage file's own numbering, not this
 * default. */
void check_points_init(CheckPoints *cp);

/** Stage-bounds helper: the largest x or z gap between any two
 * checkpoints, scaled down. Called once after a stage's checkpoints are
 * placed (see game_sparker.c). */
void check_points_calprox(CheckPoints *cp);

int32_t check_points_py(int32_t n, int32_t n2, int32_t n3, int32_t n4);

// Forward-declared (real tags, not anonymous -- see mad.h/cont_o.h/
// record.h) rather than #included: mad.h already #includes check_points.h
// (Mad's own drive()/reseto()/colide() take a CheckPoints*), so the
// reverse #include here would cycle. check_points.c #includes all three
// headers directly, where there's no cycle.
struct Mad;
struct ContO;
struct Record;

/**
 * Ports `checkstat(array, array2, record, n, n2, n3)`: per-tick race
 * position/lap-clear/catch-up bookkeeping across every car. `mads`/
 * `contOs` are parallel arrays of length `n` (one Mad+ContO pair per
 * car); `n2` is the local player's `im`; `n3` matches Mad.drive()'s own
 * unused-by-single-player third disambiguator (netplay `im` gating on
 * the `n2 !== n9 || n3 >= 2` wasted-count check, see JS source -- pass 0
 * for a single local session, matching every other call site in this
 * port).
 */
void check_points_checkstat(CheckPoints *cp, struct Mad **mads, struct ContO **contOs,
                             struct Record *record, int32_t n, int32_t n2, int32_t n3);

#ifdef __cplusplus
}
#endif

#endif
