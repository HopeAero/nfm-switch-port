// Ports xtGraphics.java's sortcars(int) (xtGraphics.java:7052-7303) -- the
// opponent-car-selection step run once per race (called from resetstat()
// right as the game enters fase 2/loading, xtGraphics.java:1528-1530,
// matching this port's STATE_STAGE_LOADING transition). NOT part of any
// web/*.js port target -- XtGraphics.js itself is out of the web port's
// scope (see its own header comment), so this is translated straight
// from the decompiled Java, same "no verified JS to lean on" situation
// PORT_SPEC.md flags for the handful of xtGraphics.java methods this
// native port needs that the web port never touched.
//
// Scope: the real single-player campaign/free-play car-selection logic
// is ported in full, including every stage-specific "boss car" placement
// (xtGraphics.java:7071-7263) and the unlocked[]-gated reroll rules
// (:7142-7163). NOT ported: the `cd.lastload==1/2` custom-car-pool
// blocks (:7264-7301) -- multiplayer/custom-car-list features, out of
// scope for the same reason every other multiplayer path in this port
// is (see native/PORT_SPEC.md).
#ifndef NFM_BOTS_H
#define NFM_BOTS_H

#include <stdint.h>
#include "progress.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BOTS_MAX_PLAYERS 7 // sc[0..6] -- Java's own fixed 7-slot array

/**
 * Fills `sc[1..6]` with randomly-chosen opponent car indices (0..15),
 * matching xtGraphics.java's sortcars(n) exactly. `sc[0]` (the player's
 * own car, already chosen at car-select) is read but never written.
 * `stage_num` is the Java `n` parameter (checkPoints.stage -- pass -1 for
 * Free Play's "any stage" case, matching Java's own `if (n < 0) n = 27`).
 *
 * Uses nfm_random() (java_compat.h) directly, NOT medium_random() --
 * xtGraphics.java's sortcars() calls raw `Math.random()`, never
 * `this.m.random()`, so this needs the TRUE Math.random() replacement,
 * not Medium's own `random()` method (medium_random() ports THAT
 * specific method -- see medium.h's "the game's own correlated PRNG"
 * doc comment: it caches/interpolates across a 3-value window for
 * smooth environmental effects, wrong shape entirely for a one-shot
 * reroll loop that needs fresh independent draws every call. Using it
 * here by mistake produced literal infinite loops during testing --
 * the cached window can repeat the same few values indefinitely while
 * this function is stuck rerolling).
 *
 * A no-op if `stage_num == 0` (matches Java's own `if (n != 0)` guard --
 * menu contexts with no real race in progress).
 */
void bots_sortcars(int32_t sc[BOTS_MAX_PLAYERS], GameMode gmode,
                    const GameProgress *progress, int32_t stage_num);

#ifdef __cplusplus
}
#endif

#endif
