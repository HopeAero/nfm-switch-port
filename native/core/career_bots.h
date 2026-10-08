// Extended's recorded bots (Bots.java, GameSparker.loadbots): on some career
// stages one or more opponents replay recorded key presses instead of the
// AI -- Files/Bots/stageN.radq, an entry "<slot>.txt" of up(t) / down(t) /
// left(t) / right(t) / handb(t) lines, t the tick the key is down (offset(t,
// set) shifts the ones after it). A bot stops for good (botbreak) on any hit,
// when it is far ahead of a player close by, or when it is not the car it was
// recorded with; the AI drives from there.
#ifndef NFM_CAREER_BOTS_H
#define NFM_CAREER_BOTS_H

#include <stdbool.h>
#include <stdint.h>

#include "nfm_limits.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CAREER_BOTS_TICKS 20000

enum { BOT_UP = 1, BOT_DOWN = 2, BOT_LEFT = 4, BOT_RIGHT = 8, BOT_HANDB = 16 };

typedef struct {
  uint8_t *keys[NFM_MAX_CARS];   // per slot, per tick: BOT_* bits (NULL: no bot)
  bool brk[NFM_MAX_CARS];        // botbreak
  int32_t timer;
  int32_t specialtimer[NFM_MAX_CARS];
  // Bots.botoffset[set][slot]: offset(t, set) lines -- where each recorded
  // set starts; stage 13 restarts a car's timer there when a portal drops it
  // on a floor (Madness.js 3291: specialtimer = botoffset[whichset]).
  int32_t botoffset[10][NFM_MAX_CARS];
} CareerBots;

void career_bots_free(CareerBots *b);

/** Which slots replay a recording on `stage` (GameSparker.java 1749-1790):
 * fills `slots`, returns how many. `hard`: the newest stage or hard mode;
 * `bonus`: the bonus stage being raced (0 none). */
int32_t career_bots_slots(int32_t stage, int32_t nplayers, bool hard, int32_t bonus, int32_t *slots);

/** Loads slot `slot`'s recording from the stage's archive text (the
 * entry's content). False when it has none. */
bool career_bots_parse(CareerBots *b, int32_t slot, const char *text);

/** The keys slot `slot` holds this tick (0 when it has no recording or has
 * broken off). `special`: stage 13 hard's per-car timers. */
uint8_t career_bots_keys(const CareerBots *b, int32_t slot, bool special);

#ifdef __cplusplus
}
#endif

#endif
