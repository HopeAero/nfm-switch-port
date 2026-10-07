// Need for Madness 2 Extended's models and stages on this engine.
//
// ext_loadbase ports Extended's GameSparker.loadbase (its GameSparker.java:
// 335-444): the 129 models of ext/data/models.radq, in Extended's own order
// and numbering -- 0-22 its own cars, 23-38 NFM 2's sixteen, 39-77 track
// pieces, 78-116 every car's "B" (beast) model, 117-128 its extra scenery.
// ext_loadstage ports its GameSparker.loadstage (:445-1206) for the stages of
// tracks.radq, classictracks.radq and matchtracks.radq (and parses the
// career packs' directives too): new directives, Extended's model ids and
// walls, its own wall trackers. Everything is built from this engine's own
// ContO / Trackers / CheckPoints / Medium, so it draws and drives the way
// this port does; ext_stage.c names what is Extended's and what is ours.
#ifndef NFM_EXT_STAGE_H
#define NFM_EXT_STAGE_H

#include <stdbool.h>
#include <stdint.h>

#include "check_points.h"
#include "cont_o.h"
#include "medium.h"
#include "trackers.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EXT_NUM_MODELS 129
#define EXT_FIRST_CAR 16      // this port's car number of Extended's car 0 (its 0-22 are our 16-38)
#define EXT_MODEL_WALL 64     // "thewall", Extended's boundary wall
#define EXT_MODEL_BEAST 78    // + Extended car number: that car's beast model

/** What a stage tells the race beyond the objects it places. */
typedef struct {
  char name[96];         // name(...), '|' read as ','
  int64_t musicswitch;   // switch(n): when the music changes, n * 1e6 (0 none)
  int32_t fixpoint[CHECK_POINTS_MAX_FIX];   // route points marked setpoint / specialchk (Contva.fixpoint)
  int32_t numfixes;
  int32_t wallr, walll, wallt, wallb;       // the boundary walls (ground patches stay inside)
  int32_t center_x, center_z;               // the stage-select preview camera's centre
} ExtStageInfo;

/** Extended's model table into `models` (EXT_NUM_MODELS, zeroed by the
 * caller or fresh). False when the archive is missing. */
bool ext_loadbase(ContO *models, Medium *m, Trackers *t);

/** A stage's text from one of Extended's packs: `pack` "tracks",
 * "classictracks", "matchtracks", "careertracks"; `entry` e.g. "1.txt",
 * "26m3.txt", "bonus/2.txt". Old-model stages of tracks.radq come back
 * renumbered (ext_renumber_old_stage). Caller frees; NULL if missing. */
char *ext_stage_text(const char *pack, const char *entry);

/** tracks.radq's 24 stages built on an older model list have every id 4
 * too high (a checkpoint on 44 gives them away; web/ext/stagecompat.js
 * renumberOldStage): lowers the ids of set* / chk* / fix / teleset lines by
 * 4, in place. True if it did. */
bool ext_renumber_old_stage(char *text);

/**
 * Places `text`'s stage into `out` (capacity `cap`), as Extended's
 * loadstage does, from Extended's model table `models`. `player_ext_car` is
 * the player's car in Extended's numbering (0-38), for the stage codes 666
 * (the player's model) and 616 (its beast model). Fills `t` (with its
 * coverage sector grid, trackers_devidetrackers_cover), `cp` (checkpoints,
 * route points, fix hoops), `m` (sky, fog, ground, clouds, mountains, ground
 * patches) and `info`. False when Extended would have failed the stage or
 * this engine cannot race it (unknown model, out of room, fewer than two
 * checkpoints).
 */
bool ext_loadstage(ContO *out, int32_t cap, int32_t *out_count, ContO *models, Medium *m, Trackers *t,
                   CheckPoints *cp, const char *text, int32_t player_ext_car, ExtStageInfo *info);

#ifdef __cplusplus
}
#endif

#endif
