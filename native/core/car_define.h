// ports web/CarDefine.js
//
// CarDefine.js's real job is a car roster: gameplay stat tables for 56
// car slots (16 built-in + 40 for user-created cars). Ported here: the
// 20 stat tables Mad.js's drive()/colide() actually read (car
// acceleration, grip, turning, bounce, weight, damage multipliers,
// engine sound signature -- literal values transcribed from the JS
// constructor for cars 0-15), PLUS `loadstat()` which computes those
// same tables for a USER-LOADED car (slot >= 16) by interpolating
// between the built-ins based on the .rad file's own `stat(...)`/
// `physics(...)`/`handling(...)` lines. Also PLUS `loadcar()` which
// wraps validation + loadstat; see car_define_loadstat below.
//
// NOT ported: the online car-sharing/upload fields and methods
// (loadonlinecar/loadmystages/loadclanstages/servervalue/
// sparkactionloader/.../run() -- leftover from the original Java
// applet's server-backed feature; the JS port's own run() is an empty
// stub, so there's nothing working there to port), and `getSvalue` +
// the string tables it writes (names, createdby -- menu-display only,
// not read by any physics or drawing code).
#ifndef NFM_CAR_DEFINE_H
#define NFM_CAR_DEFINE_H

#include <stdbool.h>
#include <stdint.h>

#include "cont_o.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CAR_DEFINE_NUM_CARS 56 // 16 built-in + 40 user-car slots (unused here, all zero)

typedef struct {
  int32_t handb[CAR_DEFINE_NUM_CARS];
  float airs[CAR_DEFINE_NUM_CARS];
  int32_t airc[CAR_DEFINE_NUM_CARS];
  int32_t turn[CAR_DEFINE_NUM_CARS];
  float grip[CAR_DEFINE_NUM_CARS];
  float bounce[CAR_DEFINE_NUM_CARS];
  float simag[CAR_DEFINE_NUM_CARS];
  float moment[CAR_DEFINE_NUM_CARS];
  float comprad[CAR_DEFINE_NUM_CARS];
  int32_t push[CAR_DEFINE_NUM_CARS];
  float revpush[CAR_DEFINE_NUM_CARS];   // float in Extended (Radical One 0.25, DR Monstaa 0.4)
  int32_t lift[CAR_DEFINE_NUM_CARS];
  int32_t revlift[CAR_DEFINE_NUM_CARS];
  int32_t powerloss[CAR_DEFINE_NUM_CARS];
  int32_t flipy[CAR_DEFINE_NUM_CARS];
  int32_t msquash[CAR_DEFINE_NUM_CARS];
  int32_t clrad[CAR_DEFINE_NUM_CARS];
  float dammult[CAR_DEFINE_NUM_CARS];
  int32_t maxmag[CAR_DEFINE_NUM_CARS];
  int32_t enginsignature[CAR_DEFINE_NUM_CARS];

  // acelf/swits are per-car float[3]/int[3] (three tuning points, e.g.
  // low/mid/high gear), not flat arrays -- see the JS's own nested-array
  // constructor literal.
  float acelf[CAR_DEFINE_NUM_CARS][3];
  int32_t swits[CAR_DEFINE_NUM_CARS][3];

  // Extra fields loadstat WRITES but Mad.drive()/colide() never reads --
  // kept as real fields so a verbatim loadstat port doesn't have to
  // branch around them. `outdam` (0.35..1.0 damage-out multiplier), used
  // by web/GameSparker.js's netplay damage-share code (not yet
  // reachable). `cclass` (0..4 car-class bucket, sum-of-stats/40 - 13),
  // used by menus for grouping and by the AI driver for difficulty
  // scaling. `dishandle` (0.25..1.0 stunt-difficulty scaler), also used
  // by Control.preform() (AI driver, not ported).
  float outdam[CAR_DEFINE_NUM_CARS];
  int32_t cclass[CAR_DEFINE_NUM_CARS];
  float dishandle[CAR_DEFINE_NUM_CARS];
} CarDefine;

/** Fills the built-in car slots (0-15) with the JS constructor's literal
 * values. Pure data, no allocation, safe to call more than once. */
void car_define_init(CarDefine *cd);

/** Extended Mode's retuned stats for NFM 2's 16 cars, over car_define_init's. */
void car_define_extended(CarDefine *cd);

/**
 * Ports web/CarDefine.js's `loadstat(buf, s, n, n2, n3, n4)`. Given a
 * .rad file's text (`text` -- the ENTIRE file, not just the stat lines,
 * since loadstat scans for lines starting with `stat(`/`physics(`/
 * `handling(`) and the numeric ContO.js-derived params from parsing that
 * same file (`maxR`, `roofat`, `wh`), fills slot `slot` (>= 16) of every
 * relevant table by INTERPOLATING between two built-in cars keyed on the
 * declared `stat(...)` values. See car_define.c for the (verbatim from
 * JS) interpolation tables.
 *
 * Returns true if the file's stat/physics lines were present and parsed
 * successfully; false otherwise (in which case slot `slot` is left as
 * whatever car_define_init put there, or all-zeros if it wasn't init'd).
 */
bool car_define_loadstat(CarDefine *cd, const char *text, int32_t maxR, int32_t roofat, int32_t wh, int32_t slot);

/**
 * Ports web/CarDefine.js's `loadcar(str, n, text)` minus the ContO
 * ownership (this port's main.c already parses the ContO itself, per
 * PORT_SPEC.md's per-platform boundary). Validates that the parsed
 * ContO's wheel positions are in the expected quadrants (rear-left/
 * rear-right/front-right/front-left order) and then calls
 * car_define_loadstat -- returns true if both validation and stat load
 * succeeded, false otherwise. `co` is only READ, never mutated.
 */
bool car_define_loadcar(CarDefine *cd, const char *text, const ContO *co, int32_t maxR, int32_t roofat, int32_t wh, int32_t slot);

#ifdef __cplusplus
}
#endif

#endif
