// Tests career_perks.h -- the 24 perks the career applies -- against the
// perks' own lines in web/ext/Madness.js and xtGraphics.js: each piece was
// cut out of the JS by brace matching and run under node on stub objects
// over random cars, perk points and situations (scratch oracle
// perks_oracle.mjs, not in the repo); career_perks_cases.h is its output,
// floats and doubles as their bits.
//   table_cases: ENERGY / PUSHING / RAMPAGE / LIFTING's tables, every car.
//   hit_cases:   the nine damage factors of a contact, WEIGHT's lift.
//   misc_cases:  DRAINER / LEAKAGE's drain, ESCAPE, FRESHNESS / STEROIDS'
//                fix time, the specials' GETAWAY, RECKLESS, FRESHNESS,
//                STEROIDS and RUTHLESS factors.
#include <stdio.h>
#include <string.h>

#include "../core/career_perks.h"
#include "career_perks_cases.h"

static int failures = 0;
#define CHECK(cond, ...)                                     \
  do {                                                       \
    if (!(cond)) {                                           \
      if (failures < 20) {                                   \
        fprintf(stderr, "FAIL (%s:%d) ", __FILE__, __LINE__); \
        fprintf(stderr, __VA_ARGS__);                        \
        fprintf(stderr, "\n");                               \
      }                                                      \
      failures++;                                            \
    }                                                        \
  } while (0)

static uint32_t fbits(float v) {
  uint32_t u;
  memcpy(&u, &v, 4);
  return u;
}
static float bitsf(uint32_t u) {
  float v;
  memcpy(&v, &u, 4);
  return v;
}
static uint64_t dbits(double v) {
  uint64_t u;
  memcpy(&u, &v, 8);
  return u;
}

static void set_perks(CareerPerks *pk, int32_t car, const int32_t pts[6]) {
  for (int32_t a = 0; a < CAREER_PERK_SLOTS; a++) pk->special[car][career_statsalc[car][a]][a] = (int8_t)pts[a];
}

// Extended's 23-31 race this port's own perk sets (career.c), not the JS's
// empty ones: their cases do not compare.
static bool port_perks(int32_t car) { return car >= 23 && car <= 31; }

int main(void) {
  static CareerPerks pk;
  const int32_t nt = (int32_t)(sizeof(table_cases) / sizeof(table_cases[0]));
  for (int32_t i = 0; i < nt; i++) {
    const TableCase *c = &table_cases[i];
    if (port_perks(c->car)) continue;
    memset(&pk, 0, sizeof(pk));
    pk.car[0] = c->car;
    set_perks(&pk, c->car, c->pts);
    int32_t pl = c->pl0, lift = c->lift0;
    float push = bitsf(c->push0), rev = bitsf(c->rev0);
    career_perk_tables(&pk, &pl, &push, &rev, &lift);
    CHECK(pl == c->pl && fbits(push) == c->push && fbits(rev) == c->rev && lift == c->lift,
          "table case %d (car %d): powerloss %d/%d push %08x/%08x revpush %08x/%08x lift %d/%d", (int)i, (int)c->car,
          (int)pl, (int)c->pl, fbits(push), c->push, fbits(rev), c->rev, (int)lift, (int)c->lift);
  }

  const int32_t nh = (int32_t)(sizeof(hit_cases) / sizeof(hit_cases[0]));
  for (int32_t i = 0; i < nh; i++) {
    const HitCase *c = &hit_cases[i];
    if (port_perks(c->car) || port_perks(c->car2)) continue;
    memset(&pk, 0, sizeof(pk));
    pk.car[c->im] = c->car;
    pk.car[c->im2] = c->car2;
    if (c->im != 0 && c->im2 != 0) pk.car[0] = 0;
    set_perks(&pk, c->car, c->p1);
    set_perks(&pk, c->car2, c->p2);
    pk.beast[c->im] = c->beastA;
    pk.shadow[c->im] = c->shadowA;
    pk.beast[c->im2] = c->beastB;
    pk.shadow[c->im2] = c->shadowB;
    pk.killtime[0] = c->killtime[0];
    pk.killtime[1] = c->killtime[1];
    float f[9];
    career_perk_hit(&pk, c->im, c->im2, c->capsized2, c->wtouch2, bitsf(c->speed), bitsf(c->speed2), bitsf(c->power2), f);
    for (int32_t k = 0; k < 9; k++)
      CHECK(fbits(f[k]) == c->f[k], "hit case %d factor %d: %08x, JS %08x", (int)i, (int)k, fbits(f[k]), c->f[k]);
    // WEIGHT reads the player's car (the one hit, when im2 is 0).
    if (c->im2 == 0) {
      const int32_t lifts = career_perk_lifts(&pk, c->lift);
      CHECK(lifts == c->lifts, "hit case %d lifts %d, JS %d", (int)i, (int)lifts, (int)c->lifts);
    }
  }

  const int32_t nm = (int32_t)(sizeof(misc_cases) / sizeof(misc_cases[0]));
  for (int32_t i = 0; i < nm; i++) {
    const MiscCase *c = &misc_cases[i];
    if (port_perks(c->car)) continue;
    memset(&pk, 0, sizeof(pk));
    pk.car[0] = c->car;
    set_perks(&pk, c->car, c->pts);
    // The drain, as mad.c's perk_drain applies it.
    float spatk = bitsf(c->before[0]), speclast = bitsf(c->before[1]), speclast2 = bitsf(c->before[2]);
    const float proportion = career_perk_drain(&pk);
    if (proportion != 0.0f) {
      const float percentage = (__builtin_fabsf(bitsf(c->f3)) * 100.0f) / (float)c->maxmag;
      const float drain = (percentage * proportion) * 1.2f;
      if (!c->specialact) {
        spatk = spatk - drain;
      } else {
        speclast = speclast - drain;
        speclast2 = speclast2 - drain;
      }
    }
    CHECK(fbits(spatk) == c->after[0] && fbits(speclast) == c->after[1] && fbits(speclast2) == c->after[2],
          "drain case %d (car %d)", (int)i, (int)c->car);
    CHECK(career_perk_captime(&pk) == c->captime, "captime case %d: %d, JS %d", (int)i, (int)career_perk_captime(&pk),
          (int)c->captime);
    CHECK(career_perk_fixtime(&pk, c->fix0) == c->fix, "fixtime case %d", (int)i);
    double spdboost, fixspd, specialboost;
    float strboost, fixstr;
    const float health = ((float)c->hitmag / (float)c->maxmag) * 100.0f;
    career_perk_specials(&pk, health, c->fixtime, &spdboost, &fixspd, &strboost, &fixstr, &specialboost);
    CHECK(dbits(spdboost) == c->spdboost && dbits(fixspd) == c->fixspd && fbits(strboost) == c->strboost &&
              fbits(fixstr) == c->fixstr && dbits(specialboost) == c->specialboost,
          "specials case %d (car %d)", (int)i, (int)c->car);
  }

  if (failures == 0) {
    printf("all tests passed (%d table, %d hit, %d other cases)\n", (int)nt, (int)nh, (int)nm);
    return 0;
  }
  fprintf(stderr, "%d failure(s)\n", failures);
  return 1;
}
