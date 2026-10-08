// The career's perks as the race applies them: the 24 of Extended v2.8's
// 40 that its code reads (docs/extended-career.md "Perks"), each written as
// the original computes it, its bugs kept. mad.c and specials.c call these
// with the player's perks (CareerPerks, career.h); outside the career they
// are never reached. Header-only so the physics needs no career.c.
// Tested against the JS transpile's own lines (tests/career_perks_test.c).
#ifndef NFM_CAREER_PERKS_H
#define NFM_CAREER_PERKS_H

#include <stdbool.h>
#include <stdint.h>

#include "career.h"
#include "java_compat.h"

// The original's base tables the perks scale from (Madness.js 222-229:
// push2, revpush2, lift2, powerloss2), by Extended car number.
static const float kPerkPush2[CAREER_CARS] = {2.0f, 2.0f, 3.0f, 3.0f, 2.0f, 2.0f, 2.0f, 4.0f, 2.0f, 2.0f,
                                              2.0f, 4.0f, 2.0f, 2.0f, 2.0f, 2.0f, 4.0f, 2.0f, 3.0f, 3.0f,
                                              7.0f, 2.0f, 8.5f, 2.0f, 2.0f, 3.0f, 3.0f, 2.0f, 2.0f, 2.0f,
                                              4.0f, 2.0f, 2.0f, 2.0f, 4.0f, 2.0f, 2.0f, 2.0f, 2.0f};
static const float kPerkRevpush2[CAREER_CARS] = {2.0f, 3.0f, 2.0f, 2.0f, 2.0f,  2.0f, 2.0f, 1.0f, 2.0f, 1.0f,
                                                 2.0f, 1.0f, 2.0f, 2.0f, 0.25f, 0.4f, 1.0f, 1.0f, 1.0f, 0.0f,
                                                 1.0f, 1.0f, 0.0f, 2.0f, 3.0f,  1.0f, 2.0f, 2.0f, 2.0f, 2.0f,
                                                 1.0f, 2.0f, 1.0f, 2.0f, 1.0f,  2.0f, 2.0f, 0.25f, 0.4f};
static const int32_t kPerkLift2[CAREER_CARS] = {0,  30, 0, 0, 0,  30, 10, 40, 20, 0, 0, 0, 10, 0,  30, 0, 35, 30, 0, 30,
                                                40, 10, 0, 0, 30, 0,  20, 0,  30, 0, 0, 20, 0, 0, 0,  10, 0, 30, 0};
static const int32_t kPerkPowerloss2[CAREER_CARS] = {
    2500000,  2500000, 3500000,  2500000, 4000000, 2500000, 3200000,  3200000,  2750000,  5500000,
    2750000,  4500000, 3500000,  16700000, 3000000, 5500000, 5500000, 12000000, 18000000, 18000000,
    16700000, 4900000, 20000000, 2500000, 2500000, 3500000, 2500000,  4000000,  2500000,  3200000,
    3200000,  2750000, 5500000,  2750000, 4500000, 3500000, 16700000, 3000000,  5500000};

/** ENERGY, PUSHING, RAMPAGE and LIFTING (Madness.js 3013-3036): every tick,
 * the player's power loss, push, recoil and lift move from its car's own
 * toward the perk's model (cars 15, 17, 13; 20; none; 7). Values without
 * the perk are left as they are. */
static inline void career_perk_tables(const CareerPerks *pk, int32_t *powerloss, float *push, float *revpush,
                                      int32_t *lift) {
  const int32_t car = pk->car[0];
  if (car < 0 || car >= CAREER_CARS) return;
  for (int32_t a = 0; a < CAREER_PERK_SLOTS; a++) {
    const int32_t e = career_specialstat(pk, car, 0, a);
    if (e > 0 && e <= 10) *powerloss = kPerkPowerloss2[car] + ((kPerkPowerloss2[15] - kPerkPowerloss2[car]) / 10) * e;
    if (e > 10 && e <= 15)
      *powerloss = kPerkPowerloss2[15] + ((kPerkPowerloss2[17] - kPerkPowerloss2[15]) / 5) * (e - 10);
    if (e > 15 && e <= 20)
      *powerloss = kPerkPowerloss2[17] + ((kPerkPowerloss2[13] - kPerkPowerloss2[17]) / 5) * (e - 15);
    const int32_t p = career_specialstat(pk, car, 7, a);
    if (p > 0) {
      const float inc = (float)((double)(kPerkPush2[20] - kPerkPush2[car]) / 20.0);
      *push = kPerkPush2[car] + inc * (float)p;
    }
    const int32_t r = career_specialstat(pk, car, 11, a);
    if (r > 0) {
      const float inc = (float)((double)kPerkRevpush2[car] / 20.0);
      *revpush = kPerkRevpush2[car] - inc * (float)r;
    }
    const int32_t l = career_specialstat(pk, car, 17, a);
    if (l > 0) *lift = kPerkLift2[car] + jtrunc_d((double)(kPerkLift2[7] - kPerkLift2[car]) / 20.0 * l);
  }
}

/** One contact's damage factors (Madness.js 778-824), in the order the
 * original multiplies them: blmult, bravery, reversestr, fearless,
 * protection, reversedef, lowpowdef, killstr, killdef. The player (slot 0)
 * hitting: BERSERK while its clock runs, CHEAPSHOT on a capsized car on its
 * wheels' side, BACKHIT reversing, BRAVERY on beasts and shadows. The player
 * hit: SAFETY -- which looks for itself on the hitter's car and takes
 * FEARLESS's points, as the original does --, FEARLESS against beasts and
 * shadows, SURVIVAL capsized, AWARENESS reversing, ARMOUR below 73.5 power. */
static inline void career_perk_hit(const CareerPerks *pk, int32_t im, int32_t im2, bool capsized2, bool wtouch2,
                                   float speed, float speed2, float power2, float f[9]) {
  float blmult = 1.0f, fearless = 1.0f, protection = 1.0f, bravery = 1.0f, reversedef = 1.0f, reversestr = 1.0f,
        lowpowdef = 1.0f, killstr = 1.0f, killdef = 1.0f;
  const int32_t me = pk->car[im], them = pk->car[im2];
  for (int32_t a = 0; a < CAREER_PERK_SLOTS; a++) {
    if (im == 0) {
      if (pk->killtime[0] > 0 && career_specialstat(pk, me, 23, a) > 0)
        killstr = 1.0f + (float)((double)career_specialstat(pk, me, 23, a) / 100.0);
      if (capsized2 && wtouch2 && career_specialstat(pk, me, 4, a) > 0)
        blmult = 1.0f + (float)((double)career_specialstat(pk, me, 4, a) / 100.0);
      if (speed < 0.0f && career_specialstat(pk, me, 14, a) > 0)
        reversestr = 1.0f + (float)((double)career_specialstat(pk, me, 14, a) / 100.0);
      if ((pk->beast[im2] || pk->shadow[im2]) && career_specialstat(pk, me, 12, a) > 0)
        bravery = 1.0f + (float)((double)career_specialstat(pk, me, 12, a) / 100.0);
    }
    if (im2 == 0) {
      if (pk->killtime[1] > 0 && career_specialstat(pk, me, 24, a) > 0)
        killdef = 1.0f - (float)((double)career_specialstat(pk, them, 6, a) / 100.0);
      if ((pk->beast[im] || pk->shadow[im]) && career_specialstat(pk, them, 6, a) > 0)
        fearless = 1.0f - (float)((double)((float)career_specialstat(pk, them, 6, a) * 0.75f) / 100.0);
      if (capsized2 && wtouch2 && career_specialstat(pk, them, 10, a) > 0)
        protection = 1.0f - (float)((double)((float)career_specialstat(pk, them, 10, a) * 1.25f) / 50.0);
      if (speed2 < 0.0f && career_specialstat(pk, them, 13, a) > 0)
        reversedef = 1.0f - (float)((double)career_specialstat(pk, them, 13, a) / 100.0);
      if (power2 < 73.5f && career_specialstat(pk, them, 15, a) > 0)
        lowpowdef = 1.0f - (float)((double)career_specialstat(pk, them, 15, a) / 100.0);
    }
  }
  f[0] = blmult;
  f[1] = bravery;
  f[2] = reversestr;
  f[3] = fearless;
  f[4] = protection;
  f[5] = reversedef;
  f[6] = lowpowdef;
  f[7] = killstr;
  f[8] = killdef;
}

static inline float career_perk_hit_apply(float v, const float f[9]) {
  for (int32_t i = 0; i < 9; i++) v = v * f[i];
  return v;
}

/** WEIGHT: how far a hit lifts the player (Madness.js 975-982). */
static inline int32_t career_perk_lifts(const CareerPerks *pk, int32_t lift) {
  int32_t lifts = lift;
  for (int32_t a = 0; a < CAREER_PERK_SLOTS; a++) {
    const int32_t v = career_specialstat(pk, pk->car[0], 16, a);
    if (v > 0) lifts = lift - jtrunc_d((double)lift / 20.0 * v);
  }
  return lifts;
}

/** DRAINER: the share of the player's hit drained off the car's special
 * (Madness.js 450-476). LEAKAGE reads DRAINER's slot -- the original's --
 * so with it the share is DRAINER's only when DRAINER sits after it, never
 * in v2.8's table: none. */
static inline float career_perk_drain(const CareerPerks *pk) {
  const int32_t car = pk->car[0];
  float proportion = 0.0f;
  for (int32_t a = 0; a < CAREER_PERK_SLOTS; a++) {
    if (career_specialstat(pk, car, 18, a) > 0) proportion = (float)career_specialstat(pk, car, 18, a) * 0.05f;
    if (career_specialstat(pk, car, 19, a) > 0) proportion = (float)career_specialstat(pk, car, 18, a) * 0.025f;
  }
  return proportion;
}

/** ESCAPE: the ticks a bad landing takes to right (Madness.js 2979-2985). */
static inline int32_t career_perk_captime(const CareerPerks *pk) {
  int32_t time = 30;
  for (int32_t a = 0; a < CAREER_PERK_SLOTS; a++) {
    const int32_t v = career_specialstat(pk, pk->car[0], 5, a);
    if (v > 0) time = 30 - jtrunc_d(v * 1.2);
  }
  return time;
}

/** FRESHNESS / STEROIDS: a fix's boost, in ticks (Madness.js 3613-3622). */
static inline int32_t career_perk_fixtime(const CareerPerks *pk, int32_t fixtime) {
  for (int32_t a = 0; a < CAREER_PERK_SLOTS; a++) {
    const int32_t f = career_specialstat(pk, pk->car[0], 21, a);
    const int32_t s = career_specialstat(pk, pk->car[0], 22, a);
    if (f > 0) fixtime = 60 + f * 3;
    if (s > 0) fixtime = 60 + jtrunc_d(s * 2.4);
  }
  return fixtime;
}

/** nitroandspecials on the player (xtGraphics.js 6950-6973, 7079-7084):
 * GETAWAY's speed and RECKLESS's strength at 80% damage or more, FRESHNESS's
 * speed and STEROIDS's strength while a fix's boost runs, RUTHLESS's
 * stronger special. `health` is the damage in % (hitmag / maxmag * 100). */
static inline void career_perk_specials(const CareerPerks *pk, float health, int32_t fixtime, double *spdboost,
                                        double *fixspd, float *strboost, float *fixstr, double *specialboost) {
  const int32_t car = pk->car[0];
  *spdboost = 1.0;
  *fixspd = 1.0;
  *strboost = 1.0f;
  *fixstr = 1.0f;
  *specialboost = 1.0;
  for (int32_t b = 0; b < CAREER_PERK_SLOTS; b++) {
    if (health >= 80.0f) {
      if (career_specialstat(pk, car, 3, b) > 0) *spdboost = 1.0 + career_specialstat(pk, car, 3, b) / 100.0;
      if (career_specialstat(pk, car, 9, b) > 0)
        *strboost = 1.0f + (float)((double)career_specialstat(pk, car, 9, b) / 100.0);
    }
    if (fixtime > 0) {
      if (career_specialstat(pk, car, 21, b) > 0) *fixspd = 1.0 + career_specialstat(pk, car, 21, b) / 100.0;
      if (career_specialstat(pk, car, 22, b) > 0)
        *fixstr = 1.0f + (float)((double)career_specialstat(pk, car, 22, b) / 100.0);
    }
  }
  for (int32_t b = 0; b < CAREER_PERK_SLOTS; b++) {
    if (career_specialstat(pk, car, 20, b) > 0) *specialboost = 1.0 + (career_specialstat(pk, car, 20, b) * 1.25) / 100.0;
  }
}

#endif
