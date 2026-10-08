// Tests career.c's bonus stat points and the experience around them against
// the real web/ext/xtGraphics.js: stat$m's checkpoint block (the counters,
// the winchance roll with GAMBLER, the experience with GREED), its wastes
// loop (the experience, BERSERK/SAFETY's clocks, the counters, the
// killchance roll) and its popup half (the slide, the 4/2/1 points paid
// once per popup). Those three pieces of the JS were run under node on a
// stub `this` (scratch oracle bonus_oracle.mjs, not in the repo) through
// random event sequences -- checkpoints, wastes, runs of popup ticks -- and
// the state after each event is in career_bonus_cases.h. Both sides draw
// from the same xorshift32 stream, so the number of draws is checked too.
#include <stdio.h>
#include <string.h>

#include "../core/career.h"
#include "career_bonus_cases.h"

static uint32_t xs;
static int32_t nrand;
static double xr(void) {
  xs ^= xs << 13;
  xs ^= xs >> 17;
  xs ^= xs << 5;
  nrand++;
  return xs / 4294967296.0;
}

int main(void) {
  career_random = xr;
  const int32_t ncases = (int32_t)(sizeof(bonus_cases) / sizeof(bonus_cases[0]));
  int32_t failures = 0, checked = 0;
  for (int32_t c = 0; c < ncases; c++) {
    const BonusCase *bc = &bonus_cases[c];
    const int32_t me = bc->me;
    CareerSave s;
    career_reset(&s);
    s.unlocked = bc->unlocked;
    s.level[me] = bc->level;
    s.exp[me] = bc->exp;
    s.statpoints[me] = bc->statpoints;
    s.extpoints[me] = bc->extpoints;
    s.winscn[me] = bc->winscn;
    s.killscn[me] = bc->killscn;
    s.wins = bc->wins;
    s.kills = bc->kills;
    for (int32_t a = 0; a < CAREER_PERK_SLOTS; a++) s.perk[me][a] = bc->perk[a];
    CareerRace r;
    memset(&r, 0, sizeof(r));
    r.stage = bc->stage;
    r.bonus = bc->bonus;
    r.nplayers = bc->nplayers;
    for (int32_t k = 0; k < bc->nplayers; k++) {
      r.sc[k] = bc->sc[k];
      r.level[k] = bc->olevel[k];
      r.beast[k] = (bc->beast >> k) & 1u;
      r.shadow[k] = (bc->shadow >> k) & 1u;
    }
    r.averagelevel = bc->averagelevel;
    r.softlevelcap = bc->softlevelcap;
    CareerRun run;
    career_run_start(&run, &r, &s);
    run.startinglevel = bc->startinglevel;
    run.expmult = bc->expmult;
    run.isithard = bc->isithard != 0;
    run.noexp = bc->noexp != 0;
    xs = bc->seed;
    nrand = 0;
    for (int32_t e = 0; e < bc->nev; e++) {
      const BonusEvent *ev = &bonus_events[bc->first + e];
      if (ev->kind == 0) career_xp_checkpoint(&run, &r, &s, ev->arg);
      else if (ev->kind == 1) career_xp_waste(&run, &r, &s, ev->arg);
      else
        for (int32_t q = 0; q < ev->arg; q++) career_popups_tick(&run, &r, &s);
      const int32_t got[17] = {s.wins,          s.kills,          s.winscn[me],         s.killscn[me],
                               s.exp[me],       s.statpoints[me], s.extpoints[me],      run.statgain,
                               run.winchance[0], run.winchance[1], run.killchance[0],    run.killchance[1],
                               run.pop_x[0],    run.pop_x[1],     run.perks.killtime[0], run.perks.killtime[1],
                               nrand};
      static const char *const kName[17] = {"wins",       "kills",       "winscn",      "killscn",  "exp",
                                            "statpoints", "extpoints",   "statgain",    "winchance0", "winchance1",
                                            "killchance0", "killchance1", "popx_waste", "popx_chk", "killtime0",
                                            "killtime1",  "draws"};
      checked++;
      for (int32_t i = 0; i < 17; i++) {
        if (got[i] != ev->v[i]) {
          if (failures < 20)
            fprintf(stderr, "FAIL case %d event %d (kind %d arg %d): %s %d, JS %d\n", (int)c, (int)e, (int)ev->kind,
                    (int)ev->arg, kName[i], (int)got[i], (int)ev->v[i]);
          failures++;
          e = bc->nev;   // the rest of this case would only repeat it
          break;
        }
      }
    }
  }
  if (failures == 0) {
    printf("all tests passed (%d cases, %d events)\n", (int)ncases, (int)checked);
    return 0;
  }
  fprintf(stderr, "%d failure(s)\n", (int)failures);
  return 1;
}
