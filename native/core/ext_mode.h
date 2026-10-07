// Extended Mode v2.8's normal mode (its "UNAVAILABLE!" menu entry, whose
// code works): 28 stages from tracks.radq (26 is the Premier Tournament,
// matchtracks.radq), 11 cars, unlocked by winning, opponents from its own
// sortcars. Spec: docs/extended-normal-mode.md. Pure logic -- game.c draws.
#ifndef NFM_EXT_MODE_H
#define NFM_EXT_MODE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EXT_NORMAL_STAGES 28
#define EXT_NORMAL_PLAYERS 11
#define EXT_PT_STAGE 26
#define EXT_PT_MATCHES 5

/** Extended's car number for one of this port's (NFM 2's 0-15 are its
 * 23-38, its own 0-22 are this port's 16-38), and back; -1 off the end. */
int32_t ext_car_of(int32_t cn);
int32_t ext_car_to_port(int32_t ecar);

/** xtGraphics.sortcars(i) outside career and classic (XT 12707-13280): the
 * opponents for stage `stage` given progress `unlocked` (unlocked[0], the
 * highest stage reached). `sc[0]` is the player's car; slots 1..nplayers-1
 * are filled, all in THIS PORT's car numbers. `ptmatch` 1-5 on stage 26
 * puts every car, the player's too, on the match's own car. */
void ext_sortcars(int32_t *sc, int32_t nplayers, int32_t stage, int32_t unlocked, int32_t ptmatch);

/** A stage's music: ext/data/Files/music/<file>.radq and the loadMod
 * parameters Extended plays it with (XT 2450-2534). */
typedef struct {
  char file[24];
  int32_t amp, rate, tempo;
} ExtMusic;
ExtMusic ext_stage_music(int32_t stage);
ExtMusic ext_menu_music(const char *which);   // "menu", "cars", "stages"

/** The stage's entry in its archive: "tracks" + "N.txt", or for the
 * tournament "matchtracks" + "26mM.txt". */
void ext_stage_entry(int32_t stage, int32_t ptmatch, char *pack, int32_t pack_len, char *entry, int32_t entry_len);

/** finish() on a win (XT 12648-12672): the next stage unlocks when the one
 * just won is the newest; 28 is the last. Returns the new `unlocked`. */
int32_t ext_unlock_after_win(int32_t unlocked, int32_t stage);

/** The car finish() shows when a stage's win unlocks it (cosmetic: nothing
 * is locked in normal mode), this port's number, or -1. */
int32_t ext_showcase_car(int32_t stage);

/** What Extended's modes remember, in ext_progress.txt beside the progress
 * file (key=value lines, so later modes can add theirs). */
typedef struct {
  int32_t normal_unlocked;   // 1..28: the highest normal-mode stage reached
} ExtProgress;
void ext_progress_reset(ExtProgress *p);
void ext_progress_load(const char *progress_path, ExtProgress *p);
bool ext_progress_save(const char *progress_path, const ExtProgress *p);

#ifdef __cplusplus
}
#endif

#endif
