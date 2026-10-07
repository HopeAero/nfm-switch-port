// Extended's Premier Tournament (normal mode, stage 26): five matches on
// matchtracks.radq, 11 drivers all in the same car, points by finishing
// place, the most points after five wins. Ports xtGraphics.tourney (XT
// 5636-5970) and the scoring of stat()/scoreshow (XT 3772-3791, 695-926).
//
// v2.8 cannot be finished as shipped (docs/extended-normal-mode.md §4):
// nobody ever scores in matches 1 and 4, match 5 can only eliminate the
// player, and nothing ends the tournament after match 5. This port mends
// those three: a wasting scores +1 for the car that did it, match 5
// eliminates the last-placed car, and after match 5 the standings decide.
#ifndef NFM_EXT_PT_H
#define NFM_EXT_PT_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define EXT_PT_CARS 11

typedef struct {
  int32_t match;                  // 1..5
  int32_t points[EXT_PT_CARS];    // the tournament's totals
  int32_t gained[EXT_PT_CARS];    // this match's points, once it is over
  // This match.
  int32_t score[EXT_PT_CARS];     // wasting matches (1, 4, 5): +1 a kill, -1 a death
  int32_t position[EXT_PT_CARS];  // 0 = first
  bool eliminated[EXT_PT_CARS];
  bool counted_death[EXT_PT_CARS];
  int32_t revive[EXT_PT_CARS];    // ticks spent wasted, back after 40 (1, 4, 5)
  bool over;
  int32_t winner;
  int32_t ptimer, neliminated;    // match 5's elimination clock
  bool eliminate, eliminateonce;
} ExtPT;

/** The ten opponents' names and the four the player picks from (XT 14413,
 * 18208-18220); the player's goes in names[0]. */
extern const char *const kExtPTOpponents[EXT_PT_CARS];
extern const char *const kExtPTPlayerNames[4];

void ext_pt_start_tournament(ExtPT *pt);
void ext_pt_start_match(ExtPT *pt);   // before each match's race

/** What the match needs from a car each tick, and what it does to it. */
typedef struct {
  bool dest;           // wasted
  int32_t hitmag;      // damage (the match may set it)
  int32_t killer;      // who wasted it this tick (-1 unknown), for the +1
  bool revive_now;     // out: the game should fix the car and put it back
  bool force_wreck;    // out: the game should waste it (eliminated)
  bool no_damage;      // out: hold damage at 0 this tick (matches 2, 3)
  bool no_special;     // out: empty its special bar (matches 1, 2, 3)
} ExtPTCar;

/** One race tick of match pt->match (tourney()). `pos`/`clear` are the
 * checkpoints' race positions and clears; `nlaps_nsp` = nlaps * nsp, `wasted`
 * the count, `racing` false during the countdown. */
void ext_pt_tick(ExtPT *pt, ExtPTCar *cars, const int32_t *pos, const int32_t *clear, int32_t nlaps_nsp,
                 int32_t wasted, bool racing);

/** The match is over: adds each car's points (10 - place, or 20 - 2*place
 * in matches 4 and 5). */
void ext_pt_award(ExtPT *pt);

/** After match 5: the tournament's winner (most points; the lower index on
 * a tie, as scoreshow's sort). */
int32_t ext_pt_champion(const ExtPT *pt);

/** The rules text Extended shows before each match (ptstart). */
const char *const *ext_pt_rules(int32_t match);

#ifdef __cplusplus
}
#endif

#endif
