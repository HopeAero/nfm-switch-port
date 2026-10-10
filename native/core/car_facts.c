#include "car_facts.h"

#include <stdio.h>

// A stat of slot `cn`, for the rankings.
typedef float (*StatFn)(const CarDefine *cd, int32_t cn);

static float top_speed(const CarDefine *cd, int32_t cn) { return (float)cd->swits[cn][2]; }
static float weight(const CarDefine *cd, int32_t cn) { return cd->moment[cn]; }
static float health(const CarDefine *cd, int32_t cn) { return (float)cd->maxmag[cn]; }
static float grip(const CarDefine *cd, int32_t cn) { return cd->grip[cn]; }
static float accel(const CarDefine *cd, int32_t cn) { return cd->acelf[cn][0]; }
static float stunts(const CarDefine *cd, int32_t cn) { return cd->airs[cn] * (float)cd->airc[cn]; }
static float push(const CarDefine *cd, int32_t cn) { return cd->push[cn]; }

// How many of the 39 base cars (`cn` itself aside) beat `v` (dir 1: by being
// higher, -1: lower). 0 means `cn` leads them all.
static int32_t beaten_by(const float *vals, int32_t cn, float v, int32_t dir) {
  int32_t n = 0;
  for (int32_t i = 0; i < CAR_FACTS_BASE; i++)
    if (i != cn && (dir > 0 ? vals[i] > v : vals[i] < v)) n++;
  return n;
}

int32_t car_facts(const CarDefine *cd, int32_t cn, int32_t ident, const int32_t npl[CAR_FACTS_BASE], int32_t my_npl,
                  char out[][CAR_FACT_LEN], int32_t max) {
  int32_t n = 0;
#define FACT(...)                                                   \
  do {                                                              \
    if (n < max) snprintf(out[n++], CAR_FACT_LEN, __VA_ARGS__);     \
  } while (0)

  // The rankings: first or last of the 39.
  static const struct {
    StatFn fn;
    const char *best, *worst;   // NULL: not said
  } kRank[] = {
      {top_speed, "The fastest car of all: %d MPH top speed.", "The slowest car of all: %d MPH top speed."},
      {accel, "The quickest off the line of all.", "The slowest off the line of all."},
      {grip, "The best grip of all cars.", "The least grip of all cars."},
      {stunts, "The best car of all for stunts.", "The worst car of all for stunts."},
      {weight, "The heaviest car of all: it hits the hardest and is the hardest to push.",
       "The lightest car of all: easy to push around."},
      {push, "Pushes other cars away harder than any other car.", NULL},
      {health, "The toughest car of all: %d health.", "The most fragile car of all: %d health."},
  };
  for (size_t r = 0; r < sizeof(kRank) / sizeof(kRank[0]); r++) {
    float vals[CAR_FACTS_BASE];
    for (int32_t i = 0; i < CAR_FACTS_BASE; i++) vals[i] = kRank[r].fn(cd, i);
    const float v = kRank[r].fn(cd, cn);
    const int32_t shown = kRank[r].fn == top_speed ? cd->swits[cn][2] / 2 : cd->maxmag[cn];
    if (beaten_by(vals, cn, v, 1) == 0) FACT(kRank[r].best, shown);
    else if (kRank[r].worst && beaten_by(vals, cn, v, -1) == 0) FACT(kRank[r].worst, shown);
  }
  {
    float vals[CAR_FACTS_BASE];
    for (int32_t i = 0; i < CAR_FACTS_BASE; i++) vals[i] = (float)npl[i];
    if (beaten_by(vals, cn, (float)my_npl, 1) == 0) FACT("The most detailed model of all: %d polygons.", my_npl);
    else if (beaten_by(vals, cn, (float)my_npl, -1) == 0) FACT("The simplest model of all: %d polygons.", my_npl);
  }

  // The rules the engine has for it, by Extended's number.
  const int32_t e = ident >= 0 && ident < 16 ? ident + 23 : ident >= 16 && ident < 39 ? ident - 16 : -1;
  if (e == 15 || e == 25 || e == 38)   // mad.c mad_colide, no_knockback
    FACT("Takes no knockback from its own hits while its special runs.");
  if (e == 18 || e == 20 || e == 22)   // game.c viewboost
    FACT("So big the camera pulls back %d further for it.", e == 18 ? 65 : e == 20 ? 130 : 300);
  // mad.c mad_dent_limit's dmgby
  if (e == 2) FACT("Its body dents twice as easily as other cars'.");
  if (e == 20) FACT("Its body dents half as much as other cars'.");
  if (e == 11 || e == 13 || e == 18 || e == 36) FACT("Its body dents 2.5 times less than other cars'.");
  if (e == 19 || e == 22) FACT("Its body dents 4 times less than other cars'.");
  if (e == 36)   // mad.c mad_init (dammult 0.225 against 0.3) and masheen_hit
    FACT("In Classic Mode it takes a quarter less damage, but hits on it count 1.27 times.");
  return n;
#undef FACT
}
