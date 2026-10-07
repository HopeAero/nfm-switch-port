// Extended Mode's specials: each car's special power, charged by stunts and
// fired with a button (the AI fires its own as soon as the bar is full).
// Ports xtGraphics.nitroandspecials (Extended's xtGraphics.java:6135-7148)
// outside career and tourney: the per-tick stat rebuild from each car's
// base values, the self boosts, and the four attacks on another car --
// reduced speed (frozen), swapped strength, drained health (leech) and
// reduced defence (redstr). The bar itself lives in mad.c (Madness.drive).
#ifndef NFM_SPECIALS_H
#define NFM_SPECIALS_H

#include <stdbool.h>
#include <stdint.h>

#include "car_define.h"
#include "check_points.h"
#include "control.h"
#include "mad.h"
#include "nfm_limits.h"

#define SPECIALS_MAX NFM_MAX_CARS

typedef struct {
  bool fixspecials[SPECIALS_MAX];   // the car's special is running
  int32_t randomcar[SPECIALS_MAX];  // its target
  int32_t okdale[SPECIALS_MAX];
  bool doitonce[SPECIALS_MAX], affected[SPECIALS_MAX], slowonce[SPECIALS_MAX], correct[SPECIALS_MAX];
  bool finalfix[4][SPECIALS_MAX];
  bool fixhealth[SPECIALS_MAX][2], updatehealth[SPECIALS_MAX];
  float proportion[SPECIALS_MAX], healthmulti[SPECIALS_MAX];
  double healthloss[SPECIALS_MAX], specpower[SPECIALS_MAX], drainrate[SPECIALS_MAX];
  double statreduce[SPECIALS_MAX][6];
  int32_t strswapee[SPECIALS_MAX];
  double statmod[SPECIALS_MAX][6];  // each stat against the car's own, in % (the buff list)

  // The status lines down the left (Extended's `over`/`q`/`xm`): which of
  // the five conditions each car shows, at what height, and the fade.
  int32_t timershown[5][SPECIALS_MAX];
  bool newtimer[5][SPECIALS_MAX], over[5][SPECIALS_MAX];
  int32_t q[5][SPECIALS_MAX], xm[5][SPECIALS_MAX];   // xm: each car's line height (Extended draws as it goes)
  int32_t xfade;
  bool xfadephase;

  // Outline glow: each car's colour, cycling through its conditions.
  bool spec_on[SPECIALS_MAX];
  int32_t spec[SPECIALS_MAX][3];
  int32_t spglow[SPECIALS_MAX], spglowchange[SPECIALS_MAX];
} Specials;

/** Extended's number for one of this port's cars: NFM 2's 0-15 are its
 * 23-38, and its own 0-22 will be this port's 16-38. */
int32_t specials_ext_car(int32_t cn);

void specials_reset(Specials *sp);

/** One race tick, after every car's drive (Extended calls it each frame of
 * the race, GameSparker.java:2565). `base` holds each car's own stats, which
 * every car's live copy (mad->cd) is rebuilt from. `nplayers` <= 8. */
void specials_tick(Specials *sp, Mad *mads, Control *controls, int32_t nplayers, CheckPoints *cp,
                   const CarDefine *base);

/** The car-select description of `cn`'s special, up to 4 lines (NULL-ended). */
const char *const *specials_describe(int32_t cn);

#endif
