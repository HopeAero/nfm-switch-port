// Tests career.c's car-select options against the real web/ext/xtGraphics.js:
// RESHUFFLE STATS (its statloss and shufflefase 4), LEVEL TRANSFER
// (shufflefase 7 then 8, xbspratio and rebsp), SELL / RESET CAR
// (resetoption[0]) and a car point into a perk (statincrease with
// nclicked). Those blocks were cut out of the JS by brace matching and run on
// a stub `this` and Madness over random saves (scratch oracle
// shuffle_oracle.mjs, not in the repo); career_shuffle_cases.h holds the two
// cars involved before and after, the car points and the stat changers.
// Also scouting's six numbers per car (career_scout_stats) against
// xtGraphics.scouting's own lines on Madness's tables (scout_oracle.mjs,
// career_scout_cases.h).
#include <stdio.h>
#include <string.h>

#include "../core/career.h"
#include "career_scout_cases.h"
#include "career_shuffle_cases.h"

static void load(CareerSave *s, const int32_t *v, const uint64_t *d, const int32_t cars[2]) {
  for (int32_t i = 0; i < 2; i++) {
    const int32_t a = cars[i];
    const int32_t *r = v + i * 18;
    s->level[a] = r[0];
    s->exp[a] = r[1];
    s->statpoints[a] = r[2];
    s->extpoints[a] = r[3];
    s->killscn[a] = r[4];
    s->winscn[a] = r[5];
    for (int32_t k = 0; k < CS_N; k++) s->sp[a][k] = r[6 + k];
    for (int32_t k = 0; k < CAREER_PERK_SLOTS; k++) s->perk[a][k] = r[12 + k];
    memcpy(&s->rebsp[a], &d[i * 2], 8);
    memcpy(&s->xbsp[a], &d[i * 2 + 1], 8);
  }
  s->carpoints = v[36];
  s->statchangers[0] = v[37];
  s->statchangers[1] = v[38];
}

static void store(const CareerSave *s, int32_t *v, uint64_t *d, const int32_t cars[2]) {
  for (int32_t i = 0; i < 2; i++) {
    const int32_t a = cars[i];
    int32_t *r = v + i * 18;
    r[0] = s->level[a];
    r[1] = s->exp[a];
    r[2] = s->statpoints[a];
    r[3] = s->extpoints[a];
    r[4] = s->killscn[a];
    r[5] = s->winscn[a];
    for (int32_t k = 0; k < CS_N; k++) r[6 + k] = s->sp[a][k];
    for (int32_t k = 0; k < CAREER_PERK_SLOTS; k++) r[12 + k] = s->perk[a][k];
    memcpy(&d[i * 2], &s->rebsp[a], 8);
    memcpy(&d[i * 2 + 1], &s->xbsp[a], 8);
  }
  v[36] = s->carpoints;
  v[37] = s->statchangers[0];
  v[38] = s->statchangers[1];
}

int main(void) {
  static const char *const kOp[4] = {"reshuffle", "transfer", "sell", "perk"};
  const int32_t n = (int32_t)(sizeof(shuffle_cases) / sizeof(shuffle_cases[0]));
  int32_t failures = 0;
  for (int32_t c = 0; c < n; c++) {
    const ShuffleCase *sc = &shuffle_cases[c];
    // This port spends no points on a perk that does nothing (REFLECT, BLEED,
    // FREEZE, GRAVITY, UNDEAD) and gives Extended's 23-31 their own perks:
    // those cases leave the JS's rules.
    if (sc->op == 3 && (!career_perk_applied(career_statsalc[sc->car][sc->slot]) || (sc->car >= 23 && sc->car <= 31)))
      continue;
    const int32_t cars[2] = {sc->car, sc->to};
    CareerSave s;
    career_reset(&s);
    load(&s, sc->before, sc->dbefore, cars);
    if (sc->op == 0) career_reshuffle(&s, sc->car);
    if (sc->op == 1) career_transfer(&s, sc->car, sc->to);
    if (sc->op == 2) career_sell(&s, sc->car);
    if (sc->op == 3) career_spend_perk(&s, sc->car, sc->slot);
    int32_t v[SHUFFLE_INTS];
    uint64_t d[SHUFFLE_DBLS];
    store(&s, v, d, cars);
    for (int32_t i = 0; i < SHUFFLE_INTS; i++) {
      if (v[i] != sc->after[i]) {
        if (failures < 20)
          fprintf(stderr, "FAIL case %d (%s %d -> %d): value %d is %d, JS %d\n", (int)c, kOp[sc->op], (int)sc->car,
                  (int)sc->to, (int)i, (int)v[i], (int)sc->after[i]);
        failures++;
        break;
      }
    }
    for (int32_t i = 0; i < SHUFFLE_DBLS; i++) {
      if (d[i] != sc->dafter[i]) {
        if (failures < 20) fprintf(stderr, "FAIL case %d (%s): ratio %d differs\n", (int)c, kOp[sc->op], (int)i);
        failures++;
        break;
      }
    }
  }
  // Scouting's numbers, each car's tables in a CarDefine slot of its own.
  {
    static CarDefine cd;
    memset(&cd, 0, sizeof(cd));
    for (int32_t c = 0; c < CAREER_CARS; c++) {
      const ScoutCar *k = &scout_cars[c];
      for (int32_t i = 0; i < 3; i++) {
        cd.swits[c][i] = k->swits[i];
        memcpy(&cd.acelf[c][i], &k->acelf[i], 4);
      }
      memcpy(&cd.grip[c], &k->grip, 4);
      memcpy(&cd.airs[c], &k->airs, 4);
      cd.airc[c] = k->airc;
      memcpy(&cd.moment[c], &k->moment, 4);
    }
    const int32_t ns = (int32_t)(sizeof(scout_cases) / sizeof(scout_cases[0]));
    for (int32_t i = 0; i < ns; i++) {
      const ScoutCase *sc = &scout_cases[i];
      int32_t out[CS_N];
      career_scout_stats(&cd, sc->car, sc->car, sc->sp, out);
      if (memcmp(out, sc->out, sizeof(out)) != 0) {
        if (failures < 20)
          fprintf(stderr, "FAIL scout case %d (car %d): %d %d %d %d %d %d, JS %d %d %d %d %d %d\n", (int)i, (int)sc->car,
                  (int)out[0], (int)out[1], (int)out[2], (int)out[3], (int)out[4], (int)out[5], (int)sc->out[0],
                  (int)sc->out[1], (int)sc->out[2], (int)sc->out[3], (int)sc->out[4], (int)sc->out[5]);
        failures++;
      }
    }
  }
  // A bonus car shown in the car select with no points gets its own.
  {
    CareerSave s;
    career_reset(&s);
    career_bonus_car_points(&s, 31);
    career_bonus_car_points(&s, 5);
    if (s.sp[31][CS_GRIP] != 60 || s.sp[31][CS_TS] != 10 || s.sp[5][CS_TS] != 0) {
      fprintf(stderr, "FAIL bonus car points\n");
      failures++;
    }
  }
  if (failures == 0) {
    printf("all tests passed (%d cases, %d scouting)\n", (int)n, (int)(sizeof(scout_cases) / sizeof(scout_cases[0])));
    return 0;
  }
  fprintf(stderr, "%d failure(s)\n", (int)failures);
  return 1;
}
