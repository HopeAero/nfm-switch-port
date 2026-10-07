// Ports a LEAN subset of web/GameSparker.js: loadbase() in full, and
// loadstage() reduced to real geometry + colours only.
//
// The JS's own loadbase()/loadstage() (see its header comment: "Ported:
// loadbase(), loadstage(), getint(), getstring(), and the fase == 0 race
// tick") is already the load-only slice of the Java GameSparker -- but
// loadstage() ITSELF still reaches into CheckPoints.js (lap/checkpoint
// bookkeeping), the pieces of XtGraphics.js it calls (player start slots,
// HUD reset), Control.js and Record.js (`.reset()`), Medium's procedural
// generators (newpolys/newclouds/newmountains/newstars), and ContO's
// #initModel constructor (the `pile` command's procedural debris) -- none
// of which are ported, and none of which a static scene (M1) needs. Scope
// decision confirmed with the user rather than assumed -- see
// TASKS_NATIVE.md's "Stage loading" entry.
//
// Kept: `snap(`/`sky(`/`ground(`/`fog(` (-> Medium's colour setters),
// `set(`/`chk(`/`fix(`/`maxr`/`maxl`/`maxt`/`maxb` (-> placing objects via
// cont_o_init_copy and populating Trackers, dropping only the
// CheckPoints/XtGraphics bookkeeping those commands also do in the JS).
//
// (`pile` is now placed, via cont_o_init_pile.)
//
// Dropped: `nlaps`/`name`/`soundtrack`
// (race/audio metadata, not geometry), `clouds(`/`texture(`/`polys(`/
// `density(`/`fadefrom(`/`lightson`/`mountains(` (colour/atmosphere
// refinements on top of what setgrnd/setsky/setfade already establish),
// the newpolys/newclouds/newmountains/newstars calls (procedural
// decoration -- see medium.h's own note on why omitting them is safe, not
// an approximation), and everything after the parse loop that depends on
// CheckPoints/XtGraphics/Control/Record (player start positioning, lap
// setup).
#ifndef NFM_GAME_SPARKER_H
#define NFM_GAME_SPARKER_H

#include <stdbool.h>
#include <stdint.h>
#include "cont_o.h"
#include "medium.h"
#include "trackers.h"
#include "check_points.h"

#ifdef __cplusplus
extern "C" {
#endif

// 16 cars (slots 0-15) + 68 track/decoration pieces (slots 56-123) -- the
// JS's own CAR_NAMES/TRACK_NAMES slot layout (see game_sparker.c), sized
// to the gap between them (16-55 unused, matching the JS array's own
// sparse layout) rather than repacked, so a stage file's `set(N)` where N
// is a JS slot number (minus the +46 offset the JS itself applies) needs
// no translation.
#define GAME_SPARKER_NUM_BASE_MODELS 124

// TRACK_NAMES[15] ("hpground", game_sparker.c) + the same +56 offset every
// track-piece baseIndex gets -- the flat filler tile a stage's route sits
// on, which tiles the whole play area. Exposed for callers that want to
// filter it out of a stage's placed objects (e.g. an overhead preview,
// where drawing every hpground tile buries the route in a uniform grid --
// see web/preview.js's own GROUND filter for the same reason).
#define GAME_SPARKER_HPGROUND_BASE_INDEX 71

/**
 * Ports GameSparker.js's loadbase(): unpacks `fpath + zip_path` (a zip
 * archive, e.g. "data/models.zip") into `base_models` via
 * vfs_read_zip + cont_o_init_buf, one entry per matching CAR_NAMES/
 * TRACK_NAMES prefix. `base_models` must point at
 * GAME_SPARKER_NUM_BASE_MODELS already-zeroed ContO structs (e.g. via
 * `calloc`); slots with no matching zip entry are left zeroed (npl == 0).
 * `m`/`t` are borrowed by every parsed ContO, same as cont_o_init_buf.
 *
 * Returns false if the zip can't be read, or if its total uncompressed
 * size doesn't match 621172 bytes -- the exact constant the JS's own
 * loadbase() checks the real data/models.zip against, to flag a
 * tampered/short download. On false, `base_models` may be partially
 * populated; the caller should treat the whole load as failed.
 */
bool game_sparker_loadbase(ContO *base_models, Medium *m, Trackers *t, const char *zip_path);

/**
 * Ports the lean subset of GameSparker.js's loadstage() described in this
 * file's header comment. `text` is a stage file's contents (e.g.
 * stages/<n>.txt, already read via vfs_read_text). `base_models` must
 * already be loaded via game_sparker_loadbase. `m`/`t` must already be
 * initialised (medium_init/trackers_init) -- this sets medium.ground,
 * medium.trk, and the colour fields per the stage file's `snap`/`sky`/
 * `ground`/`fog` commands, and populates `t` via the boundary-wall
 * trackers and trackers_devidetrackers.
 *
 * `cp` must already be initialised (check_points_init) -- this populates
 * the REAL per-stage checkpoint/lap layout from the stage file's own
 * `chk(`/`fix(`/`nlaps(` lines (JS lines ~195-243): `chk(` appends a real
 * typ=1/2 checkpoint (cp->x/y/z/typ, cp->pcs, cp->n, cp->nsp), `fix(`
 * appends a "fix point" (a car-repositioning/respawn marker, cp->fx/fy/fz/
 * roted/special, capped at 5 same as the JS's own `checkPoints.nfix !== 5`
 * guard), `nlaps(` sets cp->nlaps (clamped 1-15, matching the JS -- always
 * applied here since that line is itself gated on `xtGraphics.multion ===
 * 0`, always true in this single-player-only port). Does NOT call
 * check_points_calprox -- matches the JS's own loadstage(), which calls it
 * itself right after the parse loop, not inside loadstage(); the caller
 * should do the same. NOT ported: the `set(...)p` sub-case (JS lines
 * 179-190) that also appends special typ<=0 "marker" entries to
 * cp->x/z/typ -- confirmed harmless to skip for lap/arrow purposes, since
 * every reader of cp->typ that matters here (mad.c's own focus-search
 * loop, checkPoints.checkstat's tie-break search) explicitly skips
 * typ<=0 entries already.
 *
 * Writes up to `out_capacity` placed objects into `out_objects` (caller-
 * owned, e.g. GAME_SPARKER_NUM_BASE_MODELS or the JS's own 610 if
 * matching its ContO[610] headroom) and the number actually placed to
 * `*out_count`. Returns false (having placed as many objects as fit) if
 * the stage file would place more than `out_capacity` -- malformed input
 * or too small a buffer, not something well-formed repo stage files hit.
 *
 * `out_center_x`/`out_center_z` (nullable -- pass NULL to skip) receive
 * the stage's bounding-box center: GameSparker.java:2736-2737's
 * `medium.trx = (getint2 + getint) / 2` / `medium.trz = (getint3 +
 * getint4) / 2`, derived here from the same maxr/maxl/maxt/maxb bounds
 * already fed to trackers_devidetrackers just above those two lines in
 * the Java. This is the one value the stage-select 3D preview's dive-
 * and-orbit camera (medium_aroundtrack) needs that isn't recoverable
 * from Trackers afterwards -- t->sx/sz/ncx/ncz lose precision through
 * devidetrackers's integer division (see trackers.c) -- so it has to
 * come from here instead of being reconstructed by the caller.
 */
bool game_sparker_loadstage(ContO *out_objects, int32_t out_capacity, int32_t *out_count,
                             ContO *base_models, Medium *m, Trackers *t, CheckPoints *cp,
                             const char *text, int32_t *out_center_x, int32_t *out_center_z);

#ifdef __cplusplus
}
#endif

#endif
