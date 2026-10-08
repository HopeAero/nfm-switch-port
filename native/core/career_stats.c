// ports web/ext/xtGraphics.js: spcalc, beastspcalc, healthcalc, reqneed,
// round, and airpgstats (with getstats's nolevels branch). See career.h.
//
// airpgstats is a mechanical line-for-line conversion of the JS (lines
// 10052-12042 of xtGraphics.js, by a regex converter), then the tail
// (12043-12093) by hand. Its oddities are the original's and are kept on
// purpose -- a replay against the JS reaches all but a handful of
// unreachable branches (native/tests/career_stats_test.c). Don't tidy them.
//
// Float vs double follows the JS exactly: (float)(...) where the JS has
// fr(...), double everywhere else. Built with -fwrapv, so int32 + - * wrap
// like Java's.
#include "career.h"
#include "java_compat.h"

#include <math.h>
#include <string.h>

/** Default Math.random(): java.js's random() is java_compat's nfm_random (the
 * same xorshift32 sim stream), so the career draws from the game's stream. */
static double default_random(void) { return nfm_random(); }

/** Default Medium.random() (web/ext/Medium.js:331, the sim bank; no
 * interpolation replay or recording): three tenths digits that drift up or
 * down, re-rolled from career_random every 21 calls. Its state lives for the
 * session, as the one Medium object's does. A caller that owns a Medium can
 * point career_mrandom at medium_random instead. */
static int32_t m_cntrn, m_trn, m_rand[3];
static bool m_diup[3];
static double default_mrandom(void) {
  if (m_cntrn == 0) {
    int32_t i = 0;
    do {
      m_rand[i] = jtrunc_d(10.0 * career_random());
      // `if (random() > random())`: the left draw comes first.
      double a = career_random();
      double b = career_random();
      if (a > b) {
        m_diup[i] = false;
      } else {
        m_diup[i] = true;
      }
    } while (++i < 3);
    m_cntrn = 20;
  } else {
    m_cntrn = m_cntrn - 1;
  }
  int32_t j = 0;
  do {
    if (m_diup[j]) {
      ++m_rand[j];
      if (m_rand[j] == 10) m_rand[j] = 0;
    } else {
      --m_rand[j];
      if (m_rand[j] == -1) m_rand[j] = 9;
    }
  } while (++j < 3);
  int32_t trn = m_trn + 1;
  if (trn == 3) trn = 0;
  m_trn = trn;
  return (float)((float)(m_rand[trn]) / 10.0);
}

double (*career_random)(void) = default_random;
double (*career_mrandom)(void) = default_mrandom;

// xtGraphics.maxlevel, and Madness's per-car base tables (Extended's 39 cars).
// Generated from the JS port's constructors.
static const int32_t maxlevel[31] = {4, 7, 10, 13, 16, 20, 23, 26, 30, 33, 36, 39, 42, 45, 49, 52, 55, 58, 62, 65, 70, 74, 77, 84, 90, 97, 103, 111, 120, 125, 125};
static const float momentreset_tab[CAREER_CARS] = {
  1.25f, 0.75f, 1.5f, 1.0f, 0.8500000238418579f, 1.25f, 1.3250000476837158f, 1.399999976158142f, 1.399999976158142f, 1.5f, 1.4249999523162842f, 2.0999999046325684f, 1.2999999523162842f, 3.0f, 1.524999976158142f, 2.0999999046325684f, 2.5f, 2.0999999046325684f, 6.0f, 3.200000047683716f, 6.195000171661377f, 1.5499999523162842f, 11.0f, 1.2000000476837158f, 0.75f, 1.399999976158142f, 1.0f, 1.100000023841858f, 1.25f, 1.399999976158142f, 1.2999999523162842f, 1.2000000476837158f, 1.4500000476837158f, 1.375f, 2.0f, 1.2000000476837158f, 3.0f, 1.5f, 2.0f};
static const int32_t nitroswits_tab[CAREER_CARS][3] = {
  {50, 180, 280}, {100, 200, 310}, {60, 180, 271}, {70, 200, 300}, {70, 170, 280}, {60, 200, 290}, {60, 170, 280}, {60, 180, 275}, {90, 210, 295}, {90, 190, 276}, {70, 200, 295}, {50, 160, 270}, {90, 200, 305}, {70, 150, 250}, {80, 200, 300}, {70, 210, 290}, {90, 200, 285}, {140, 225, 320}, {70, 180, 260}, {135, 210, 300}, {50, 130, 210}, {150, 250, 335}, {80, 170, 260}, {50, 180, 280}, {100, 200, 310}, {60, 180, 275}, {70, 200, 295}, {70, 170, 275}, {60, 200, 290}, {60, 170, 280}, {60, 180, 280}, {90, 210, 295}, {90, 190, 276}, {70, 200, 295}, {50, 160, 270}, {90, 200, 305}, {50, 130, 210}, {80, 200, 300}, {70, 210, 290}};

// The JS's int helpers. i32() of an int32 sum is the identity under -fwrapv.
static inline int32_t i32(int32_t x) { return x; }
static inline int32_t imul(int32_t a, int32_t b) { return (int32_t)((uint32_t)a * (uint32_t)b); }
static inline int32_t imin(int32_t a, int32_t b) { return a < b ? a : b; }
static inline int32_t imax(int32_t a, int32_t b) { return a > b ? a : b; }
// ext-ident: car numbers here are already Extended's.
static inline int32_t id(int32_t car) { return car; }

/** xtGraphics.round: half-up to `precision` decimals, in double. */
static double xround(double value, int32_t precision) {
  int32_t scale = jtrunc_d(pow(10.0, precision));
  return floor(value * scale + 0.5) / scale;
}

/** this.maxlevel[i]. The JS reads it at unlocked-2, which only a save with
 * unlocked < 2 or > 32 puts out of range (JS: undefined); clamp rather than
 * read past the table. */
static int32_t maxlevel_at(int32_t i) {
  if (i < 0) i = 0;
  if (i > 30) i = 30;
  return maxlevel[i];
}

/** madness[0].moment[sc[0]]: the player car's live strength, which the
 * original leaves at whatever the last race in that car set it to (the
 * Madness objects live for the whole session). With no modifiers that is
 * strength = momentreset + aistrsp * 0.025 (xtGraphics.js line 7050), which
 * is what this uses. */
static float player_moment(const CareerRace *r) {
  return (float)(momentreset_tab[r->sc[0]] + (float)((float)(r->sp[0][CS_STR]) * 0.02500000037252903));
}

int32_t career_spcalc(int32_t level) {
  int32_t factor = jtrunc_d(level / 15.0);
  int32_t leftovers = 0;
  if (factor == 1) leftovers = 52;
  if (factor >= 2) {
    leftovers = 52 + jtrunc_d(xround(0.5 * (imul(imul(15, factor - 1), factor - 1) + imul(135, factor - 1)), 0));
  }
  int32_t leveladj = imul(factor, 15) - 1;
  if (factor == 0) leveladj = 1;
  return imul(level - leveladj, factor + 4) + leftovers;
}

int32_t career_beastspcalc(int32_t level) {
  int32_t levelsp = career_spcalc(level) + 320;
  if (level <= 21) levelsp = career_spcalc(level) + imul(level - 1, 16);
  return levelsp;
}

int32_t career_healthcalc(int32_t initialhealth, int32_t statpoints, int32_t car, float modifier) {
  int32_t increment = 500;
  if (jtrunc_d(initialhealth / 20.0) >= 500) {
    if (jtrunc_d(initialhealth / 20.0) <= 1250 || id(car) == 11 || id(car) == 13 || id(car) == 36 ||
        id(car) == 18 || id(car) == 19 || id(car) == 20 || id(car) == 22) {
      increment = jdiv(initialhealth, 20);
    } else {
      increment = 1250;
    }
  }
  // Java's modifier is a double; the header's float widens exactly.
  return jtrunc_d((double)(initialhealth + imul(statpoints, increment)) * (double)modifier);
}

int32_t career_reqneed(int32_t a, int32_t b) {
  // this.expneededform: every b falls in exactly one case, so no state survives.
  int32_t expneededform = 0;
  if (b < 8 || (b >= 23 && b < 31)) expneededform = 1;
  if (b >= 8 && b < 16) expneededform = b - 6;
  if (b >= 16 && b <= 21 && b != 19 && b != 18) expneededform = b + 1;
  if (b == 18) expneededform = 20;
  if (b == 19) expneededform = 24;
  if (b == 22) expneededform = 25;
  if (b >= 31) expneededform = b - 22;
  double level = a;
  double half = jdiv(a, 2);
  double multi = 11.36 + (a * 0.02);
  double multiplier = 1.0 + (a * 0.04166666667);
  double value = (((((((((((level * 259.0) + (half * half)) + (level * half)) + 721.0) + (((level * level) + level)))) * 1.53) * multi) * 0.1) * multiplier) * ((1.0 + (0.125 * expneededform)))) + (((level * level) * level) * 1.15);
  double totalmulti = 0.705;
  return jtrunc_d(value * totalmulti);
}

void career_airpgstats(CareerRace *r, const CareerSave *s, const float *gripreset) {
  // nplayers is 2..19 in the career; anything else would index slot -1.
  if (r->nplayers < 2 || r->nplayers > CAREER_MAX_PLAYERS) return;
  if (r->nolevels) {
    // getstats's other branch: level 1 and nostatspls() for every opponent.
    for (int32_t a = 1; a < r->nplayers; a++) {
      r->level[a] = 1;
      memset(r->sp[a], 0, sizeof(r->sp[a]));
    }
    return;
  }
  // The original zeroes every opponent's ai*sp before sortcars (xtGraphics.js
  // line 14044); the code below then only sets some and adds to all six.
  for (int32_t a = 1; a < r->nplayers && a < CAREER_MAX_PLAYERS; a++) memset(r->sp[a], 0, sizeof(r->sp[a]));
  int32_t totalsp[CAREER_MAX_PLAYERS] = {0};
  int32_t bonuspoints[CAREER_MAX_PLAYERS] = {0};
  // (this.setlevels is false here: the function runs once per race.)
  {
      if (!r->scalelevels) {
        for (int32_t b = 1; b < 31; b++) {
          if (r->stage == b) {
            int32_t variance = 3;
            if (((b == 5) || (b == 14)) || (b == 26)) {
              variance = 4;
            }
            if (((b == 4) || (b == 6)) || (b == 24)) {
              variance = 2;
            }
            if ((((((((b == 8) || (b == 10)) || (b == 12)) || (b == 16)) || (b == 18)) || (b == 22)) || (b == 25)) || (b == 29)) {
              variance = 1;
            }
            for (int32_t a = 1; a < ((r->nplayers - 1)); a++) {
              r->level[a] = i32(jtrunc_d((career_random() * variance)) + ((i32(maxlevel_at(i32(b - 1)) - variance))));
            }
            r->level[(r->nplayers - 1)] = maxlevel_at(i32(b - 1));
          }
        }
        if ((r->bonus == 1)) {
          for (int32_t a2 = 1; a2 < ((r->nplayers - 1)); a2++) {
            r->level[a2] = imin(imax(6, i32(r->level[0] - 9)), 20);
          }
          r->level[(r->nplayers - 1)] = imin(imax(7, i32(r->level[0] - 8)), 21);
        }
        if ((r->bonus == 2)) {
          int32_t level = i32(r->level[0] - 6);
          if (level < 30) {
            level = 30;
          }
          if (level > 65) {
            level = 65;
          }
          r->level[(r->nplayers - 1)] = i32(level + 5);
          for (int32_t a3 = 1; a3 < ((r->nplayers - 1)); a3++) {
            r->level[a3] = level;
          }
        }
        if ((r->bonus == 3)) {
          int32_t level = i32(r->level[0] - 4);
          if (level < 40) {
            level = 40;
          }
          if (level > 80) {
            level = 80;
          }
          for (int32_t a3 = 1; a3 < 11; a3++) {
            r->level[a3] = level;
          }
        }
        if ((r->stage == 11) && !(r->bonus == 2)) {
          for (int32_t a2 = 1; a2 < 5; a2++) {
            r->level[a2] = 35;
          }
          for (int32_t a2 = 5; a2 < ((r->nplayers - 1)); a2++) {
            if (((r->hardstage || (s->unlocked == 11))) && (a2 >= ((r->nplayers - 3)))) {
              r->level[a2] = 35;
            } else {
              r->level[a2] = i32(jtrunc_d((career_random() * 2.0)) + 34);
            }
          }
          r->level[(r->nplayers - 1)] = 36;
        }
        if (r->stage == 13) {
          for (int32_t a2 = 1; a2 < ((r->nplayers - 1)); a2++) {
            if (a2 < ((r->nplayers - 6))) {
              r->level[a2] = i32(jtrunc_d((career_random() * 2.0)) + 40);
            } else {
              r->level[a2] = 41;
            }
          }
          r->level[(r->nplayers - 1)] = 42;
        }
        if (r->stage == 17) {
          for (int32_t a2 = 1; a2 < 4; a2++) {
            r->level[a2] = 55;
          }
          for (int32_t a2 = 4; a2 < ((r->nplayers - 1)); a2++) {
            r->level[a2] = i32(jtrunc_d((career_random() * 3.0)) + 52);
          }
          r->level[(r->nplayers - 1)] = 55;
        }
        if ((r->bonus == 4)) {
          for (int32_t a2 = 1; a2 < r->nplayers; a2++) {
            if (r->level[0] <= 60) {
              r->level[a2] = 60;
            } else {
              r->level[a2] = r->level[0];
            }
          }
        }
        if (r->stage == 21) {
          for (int32_t a2 = 1; a2 < ((r->nplayers - 3)); a2++) {
            r->level[a2] = i32(jtrunc_d((career_random() * 4.0)) + 66);
          }
          r->level[(r->nplayers - 1)] = 70;
          r->level[(r->nplayers - 2)] = 69;
          r->level[(r->nplayers - 3)] = 69;
        }
        if (r->stage == 23) {
          int32_t whichnum = 1;
          if ((s->unlocked == 23) || r->hardstage) {
            r->level[1] = 80;
            whichnum = 2;
          }
          for (int32_t a3 = whichnum; a3 < ((r->nplayers - 1)); a3++) {
            r->level[a3] = i32(jtrunc_d((career_random() * 2.0)) + 75);
          }
          r->level[(r->nplayers - 1)] = 77;
        }
      } else {
        int32_t level = r->level[0];
        if (level < 3) {
          level = 3;
        }
        if (level > (i32(maxlevel_at(i32(s->unlocked - 2)) - 1))) {
          level = i32(maxlevel_at(i32(s->unlocked - 2)) - 1);
        }
        r->level[(r->nplayers - 1)] = i32(level + 1);
        for (int32_t a3 = 1; a3 < ((r->nplayers - 1)); a3++) {
          r->level[a3] = i32(jtrunc_d((career_random() * 2.0)) + ((i32(level - 1))));
        }
      }
      for (int32_t g = 1; g < r->nplayers; g++) {
        if (r->beast[g] || (r->stage < 16)) {
          bonuspoints[g] = 0;
        } else {
          int32_t carcode = r->sc[g];
          if ((carcode < 15) || (carcode >= 23)) {
            carcode = 15;
          }
          if (carcode == 20) {
            carcode = 18;
          }
          if (carcode == 21) {
            carcode = 19;
          }
          int32_t statvariance = i32((jdiv(r->level[g], 6)) + 1);
          double multip = 1.0;
          if (r->stage < 20) {
            multip = (1.0 + (((r->stage - 16.0)) * 0.3)) + (((carcode - 15.0)) * 0.3);
          }
          if (r->stage == 20) {
            multip = 2.0;
            if ((g == ((r->nplayers - 1))) && !r->scalelevels) {
              statvariance = 0;
            }
          }
          if ((r->stage >= 21) && (r->stage <= 23)) {
            multip = (2.1 + (((r->stage - 21.0)) * 0.1)) + (((carcode - 15.0)) * 0.3);
          }
          if (r->stage >= 24) {
            multip = (2.5 + (((r->stage - 24.0)) * 0.25)) + (((carcode - 15.0)) * 0.5);
          }
          bonuspoints[g] = i32(jtrunc_d((r->level[g] * multip)) + jtrunc_d(((float)(career_mrandom() * (float)(statvariance)))));
        }
        if (!r->beast[g]) {
          totalsp[g] = i32(career_spcalc(r->level[g]) + bonuspoints[g]);
        } else {
          totalsp[g] = career_beastspcalc(r->level[g]);
        }
        int32_t statadjust[6];
        for (int32_t c = 0; c < 6; c++) {
          statadjust[c] = 0;
          if (id(r->sc[g]) >= 31) {
            statadjust[c] = 7;
          }
          if (id(r->sc[g]) == 36) {
            statadjust[0] = 47;
          }
        }
        if ((r->bonus != 0)) {
          if ((((r->bonus == 1) || (r->bonus == 2))) && (g == ((r->nplayers - 1)))) {
            r->sp[g][CS_END] = jdiv(totalsp[g], 4);
            r->sp[g][CS_STR] = jdiv(totalsp[g], 2);
            r->sp[g][CS_TS] = jdiv(totalsp[g], 4);
          }
          if ((((r->bonus == 3) || (r->bonus == 4))) && (g == ((r->nplayers - 1)))) {
            r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 3)), 4);
            r->sp[g][CS_END] = 60;
            r->sp[g][CS_TS] = i32(totalsp[g] - ((i32(r->sp[g][CS_STR] + r->sp[g][CS_END]))));
          }
        } else if (g == ((r->nplayers - 1))) {
          if (r->stage == 1) {
            r->sp[g][CS_TS] = jdiv(totalsp[g], 2);
            r->sp[g][CS_GRIP] = jdiv(totalsp[g], 2);
          }
          if (r->stage == 2) {
            r->sp[g][CS_END] = jdiv((imul(totalsp[g], 5)), 6);
            r->sp[g][CS_STR] = i32(totalsp[g] - r->sp[g][CS_END]);
          }
          if (r->stage == 3) {
            r->sp[g][CS_END] = jdiv((imul(totalsp[g], 5)), 12);
            r->sp[g][CS_STR] = i32(totalsp[g] - r->sp[g][CS_END]);
          }
          if (r->stage == 4) {
            r->sp[g][CS_END] = jdiv(totalsp[g], 4);
            r->sp[g][CS_TS] = jdiv(totalsp[g], 6);
            r->sp[g][CS_STR] = i32((i32(totalsp[g] - r->sp[g][CS_END])) - r->sp[g][CS_TS]);
          }
          if (r->stage == 5) {
            r->sp[g][CS_ACC] = i32((jdiv((imul(totalsp[g], 1)), 6)) + 2);
            r->sp[g][CS_TS] = i32(totalsp[g] - r->sp[g][CS_ACC]);
          }
          if (r->stage == 6) {
            r->sp[g][CS_END] = jdiv((imul(totalsp[g], 15)), 19);
            r->sp[g][CS_TS] = i32(totalsp[g] - r->sp[g][CS_END]);
          }
          if (r->stage == 7) {
            r->sp[g][CS_TS] = jdiv(totalsp[g], 4);
            r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 5)), 8);
            r->sp[g][CS_END] = i32((i32(totalsp[g] - r->sp[g][CS_STR])) - r->sp[g][CS_TS]);
          }
          if (r->stage == 8) {
            r->sp[g][CS_END] = jdiv(totalsp[g], 5);
            r->sp[g][CS_TS] = jdiv(totalsp[g], 5);
            r->sp[g][CS_STR] = i32((i32(totalsp[g] - r->sp[g][CS_END])) - r->sp[g][CS_TS]);
          }
          if (r->stage == 9) {
            float goalgrip = (float)(24.200000762939453 + ((float)((float)(((i32((imul(r->level[(r->nplayers - 1)], 3)) - 1)))) * 0.20000000298023224)));
            if (goalgrip > 42.0) {
              goalgrip = 42.0;
            }
            double startgrip = gripreset[r->sc[g]] + ((((r->level[g] + statadjust[2]) - 1.0)) / 5.0);
            double needgrip = xround(goalgrip - startgrip, 1);
            int32_t gripstats = jtrunc_d((needgrip * 5.0));
            if (needgrip < 0.0) {
              gripstats = 0;
            }
            if (totalsp[g] < gripstats) {
              r->sp[g][CS_GRIP] = totalsp[g];
            } else {
              r->sp[g][CS_GRIP] = gripstats;
              int32_t remaining = i32(totalsp[g] - gripstats);
              r->sp[g][CS_TS] = jdiv((imul(remaining, 5)), 6);
              r->sp[g][CS_END] = i32(remaining - r->sp[g][CS_TS]);
            }
          }
          if (r->stage == 10) {
            r->sp[g][CS_TS] = totalsp[g];
          }
          if (r->stage == 11) {
            r->sp[g][CS_END] = jdiv((imul(totalsp[g], 5)), 28);
            r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 2)), 13);
            r->sp[g][CS_STR] = i32((i32(totalsp[g] - r->sp[g][CS_END])) - r->sp[g][CS_TS]);
          }
          if (r->stage == 12) {
            r->sp[g][CS_END] = jdiv((imul(totalsp[g], 35)), 152);
            r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 33)), 152);
            r->sp[g][CS_STR] = i32((i32(totalsp[g] - r->sp[g][CS_END])) - r->sp[g][CS_TS]);
          }
          if (r->stage == 13) {
            r->sp[g][CS_END] = jdiv((imul(totalsp[g], 4)), 5);
            r->sp[g][CS_TS] = i32(totalsp[g] - r->sp[g][CS_END]);
          }
          if (r->stage == 14) {
            r->sp[g][CS_STU] = jdiv((imul(totalsp[g], 148)), 224);
            r->sp[g][CS_TS] = i32(totalsp[g] - r->sp[g][CS_STU]);
          }
          if (r->stage == 15) {
            r->sp[g][CS_END] = jdiv((imul(totalsp[g], 5)), 96);
            r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 35)), 96);
            r->sp[g][CS_STR] = i32((i32(totalsp[g] - r->sp[g][CS_END])) - r->sp[g][CS_TS]);
          }
          if (r->stage == 16) {
            r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 5)), 16);
            r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 7)), 16);
            r->sp[g][CS_END] = i32(totalsp[g] - ((i32(r->sp[g][CS_TS] + r->sp[g][CS_STR]))));
          }
          if (r->stage == 17) {
            r->sp[g][CS_TS] = jdiv(totalsp[g], 4);
            r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 9)), 16);
            r->sp[g][CS_END] = i32(totalsp[g] - ((i32(r->sp[g][CS_TS] + r->sp[g][CS_STR]))));
          }
          if (r->stage == 18) {
            r->sp[g][CS_TS] = 163;
            r->sp[g][CS_END] = jdiv(totalsp[g], 16);
            r->sp[g][CS_STR] = i32((i32(totalsp[g] - 163)) - r->sp[g][CS_END]);
          }
          if (r->stage == 19) {
            r->sp[g][CS_END] = jdiv((imul(totalsp[g], 23)), 32);
            r->sp[g][CS_GRIP] = jdiv((imul(totalsp[g], 3)), 32);
            r->sp[g][CS_TS] = i32((i32(totalsp[g] - r->sp[g][CS_END])) - r->sp[g][CS_GRIP]);
          }
          if (r->stage == 20) {
            r->sp[g][CS_TS] = totalsp[g];
          }
          if (r->stage == 21) {
            r->sp[g][CS_END] = 40;
            r->sp[g][CS_TS] = 120;
            r->sp[g][CS_STR] = i32(totalsp[g] - 160);
          }
          if (r->stage == 22) {
            r->sp[g][CS_TS] = 70;
            r->sp[g][CS_STR] = i32(totalsp[g] - 70);
          }
          if (r->stage == 23) {
            r->sp[g][CS_END] = 40;
            r->sp[g][CS_TS] = 135;
            r->sp[g][CS_STR] = i32(totalsp[g] - 175);
          }
          if (r->stage == 24) {
            if (player_moment(r) <= 13.649999618530273) {
              r->sp[g][CS_TS] = 98;
              r->sp[g][CS_STR] = 490;
              r->sp[g][CS_END] = i32(totalsp[g] - 588);
            } else {
              r->sp[g][CS_TS] = 98;
              r->sp[g][CS_END] = i32(totalsp[g] - 98);
            }
          }
          if (r->stage > 24) {
            r->sp[g][CS_END] = 40;
            r->sp[g][CS_TS] = 135;
            r->sp[g][CS_STR] = i32(totalsp[g] - 175);
          }
        }
        if (g < ((r->nplayers - 1))) {
          if ((id(r->sc[g]) == 0) || (id(r->sc[g]) == 23)) {
            if (((r->stage != 6) && (r->stage != 9)) && (r->stage != 11)) {
              if (!r->beast[g]) {
                r->sp[g][CS_STU] = jdiv(totalsp[g], 2);
                r->sp[g][CS_TS] = jdiv(totalsp[g], 6);
                r->sp[g][CS_STR] = i32((i32(totalsp[g] - r->sp[g][CS_STU])) - r->sp[g][CS_TS]);
              } else {
                r->sp[g][CS_STR] = jdiv(totalsp[g], 2);
                r->sp[g][CS_END] = jdiv((imul(totalsp[g], 3)), 8);
                r->sp[g][CS_TS] = jdiv(totalsp[g], 8);
              }
            } else {
              if (r->stage == 6) {
                r->sp[g][CS_TS] = jdiv(totalsp[g], 4);
                r->sp[g][CS_STR] = jdiv(totalsp[g], 4);
                r->sp[g][CS_END] = jdiv(totalsp[g], 2);
              }
              if ((r->stage == 9) || (r->stage == 11)) {
                double startgrip2 = gripreset[r->sc[g]] + ((((r->level[g] + statadjust[2]) - 1.0)) / 5.0);
                double whatneed = 24.2 + (((i32((imul(r->level[(r->nplayers - 1)], 3)) - 1))) * 0.2);
                if (whatneed > 42.0) {
                  whatneed = 42.0;
                }
                if (r->stage == 11) {
                  whatneed = 24.2 + (((i32((imul(r->level[(r->nplayers - 1)], 3)) - 1))) * 0.2);
                  if (whatneed > 45.6) {
                    whatneed = 45.6;
                  }
                }
                double needgrip2 = xround(whatneed - startgrip2, 1);
                int32_t gripstats2 = jtrunc_d((needgrip2 * 5.0));
                if (needgrip2 < 0.0) {
                  gripstats2 = 0;
                }
                if (totalsp[g] < gripstats2) {
                  r->sp[g][CS_GRIP] = totalsp[g];
                } else {
                  r->sp[g][CS_GRIP] = gripstats2;
                  int32_t remaining2 = i32(totalsp[g] - gripstats2);
                  if (r->stage == 9) {
                    r->sp[g][CS_TS] = jdiv((imul(remaining2, 3)), 4);
                    r->sp[g][CS_END] = i32(remaining2 - r->sp[g][CS_TS]);
                  }
                  if (r->stage == 11) {
                    if (!r->beast[g]) {
                      r->sp[g][CS_END] = jdiv((imul(remaining2, 3)), 4);
                      r->sp[g][CS_TS] = i32(remaining2 - r->sp[g][CS_END]);
                    } else {
                      r->sp[g][CS_END] = jdiv(remaining2, 2);
                      r->sp[g][CS_STR] = jdiv((imul(remaining2, 3)), 8);
                      r->sp[g][CS_TS] = i32((i32(remaining2 - r->sp[g][CS_END])) - r->sp[g][CS_STR]);
                    }
                  }
                }
              }
            }
          }
          if ((id(r->sc[g]) == 1) || (id(r->sc[g]) == 24)) {
            if (((r->stage != 6) && !r->beast[g]) && (r->stage != 10)) {
              if ((r->stage != 14) && (r->stage != 5)) {
                r->sp[g][CS_TS] = jdiv(totalsp[g], 2);
                r->sp[g][CS_END] = jdiv(totalsp[g], 2);
              } else {
                if (r->stage == 14) {
                  r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 3)), 4);
                  r->sp[g][CS_END] = jdiv(totalsp[g], 4);
                }
                if (r->stage == 5) {
                  float userstrength = (float)(momentreset_tab[r->sc[0]] + ((float)((float)(r->sp[0][CS_STR]) * 0.02500000037252903)));
                  if (userstrength >= 2.5) {
                    r->sp[g][CS_TS] = jdiv(totalsp[g], 2);
                    r->sp[g][CS_END] = jdiv(totalsp[g], 2);
                  } else {
                    r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 3)), 4);
                    r->sp[g][CS_END] = jdiv(totalsp[g], 4);
                  }
                }
              }
            } else {
              if (r->stage == 10) {
                float userstrength = (float)(momentreset_tab[r->sc[0]] + ((float)((float)(r->sp[0][CS_STR]) * 0.02500000037252903)));
                if (userstrength < 4.0) {
                  r->sp[g][CS_END] = jdiv(totalsp[g], 4);
                  r->sp[g][CS_TS] = i32(totalsp[g] - r->sp[g][CS_END]);
                } else {
                  r->sp[g][CS_END] = jdiv((imul(totalsp[g], 3)), 4);
                  r->sp[g][CS_TS] = i32(totalsp[g] - r->sp[g][CS_END]);
                }
              } else {
                r->sp[g][CS_TS] = jdiv(totalsp[g], 4);
                r->sp[g][CS_END] = jdiv((imul(totalsp[g], 3)), 4);
              }
              if ((r->stage == 5) && r->beast[g]) {
                r->sp[g][CS_TS] = career_spcalc(jdiv(r->level[g], 2));
                r->sp[g][CS_END] = i32(totalsp[g] - r->sp[g][CS_TS]);
              }
            }
          }
          if ((id(r->sc[g]) == 2) || (id(r->sc[g]) == 25)) {
            if (((((r->stage != 6) && (r->stage != 7)) && (r->stage != 8))) || !r->beast[g]) {
              r->sp[g][CS_STR] = jdiv(totalsp[g], 2);
              r->sp[g][CS_END] = jdiv(totalsp[g], 2);
            } else {
              bool afuckingracer = false;
              int32_t totalpoints = i32(career_spcalc(r->level[0]) + s->extpoints[r->sc[0]]);
              int32_t instrength = jdiv(totalpoints, 8);
              if ((i32((i32(r->sp[0][CS_STR] - r->level[0])) + 1)) < instrength) {
                afuckingracer = true;
              }
              int32_t extras = 65;
              if (r->scalelevels) {
                extras = 0;
              }
              if (!afuckingracer) {
                if (r->stage == 6) {
                  r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 3)), 16);
                  r->sp[g][CS_TS] = jdiv(totalsp[g], 8);
                  r->sp[g][CS_ACC] = extras;
                  r->sp[g][CS_STU] = extras;
                  r->sp[g][CS_END] = i32((i32((i32(totalsp[g] - r->sp[g][CS_STR])) - r->sp[g][CS_TS])) - (imul(extras, 2)));
                }
                if (r->stage == 7) {
                  r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 7)), 32);
                  r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 5)), 32);
                  r->sp[g][CS_STU] = i32(extras - (jdiv(totalsp[g], 32)));
                  r->sp[g][CS_ACC] = i32(extras - (jdiv(totalsp[g], 32)));
                  r->sp[g][CS_END] = i32((i32((i32((i32(totalsp[g] - r->sp[g][CS_STR])) - r->sp[g][CS_TS])) - r->sp[g][CS_STU])) - r->sp[g][CS_ACC]);
                }
              } else {
                r->sp[g][CS_END] = jdiv((imul(totalsp[g], 3)), 16);
                if (r->stage == 6) {
                  r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 5)), 16);
                }
                if (r->stage == 7) {
                  r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 3)), 16);
                }
                r->sp[g][CS_STU] = extras;
                r->sp[g][CS_TS] = i32((i32((i32(totalsp[g] - r->sp[g][CS_END])) - r->sp[g][CS_STR])) - extras);
              }
              if (r->stage == 8) {
                if (afuckingracer) {
                  r->sp[g][CS_END] = jdiv(totalsp[g], 2);
                } else {
                  r->sp[g][CS_END] = jdiv((imul(totalsp[g], 5)), 16);
                }
                r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 5)), 16);
                r->sp[g][CS_TS] = i32((i32(totalsp[g] - r->sp[g][CS_END])) - r->sp[g][CS_STR]);
              }
            }
          }
          if ((id(r->sc[g]) == 3) || (id(r->sc[g]) == 26)) {
            if ((r->stage != 10) && (r->stage != 5)) {
              if ((r->stage != 14) && (r->stage != 9)) {
                r->sp[g][CS_TS] = jdiv(totalsp[g], 2);
                r->sp[g][CS_END] = jdiv(totalsp[g], 2);
              } else {
                if (r->stage == 14) {
                  r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 3)), 4);
                  r->sp[g][CS_END] = jdiv(totalsp[g], 4);
                }
                if (r->stage == 9) {
                  double startgrip2 = gripreset[r->sc[g]] + ((((r->level[g] + statadjust[2]) - 1.0)) / 5.0);
                  double whatneed = 24.2 + (((i32((imul(r->level[(r->nplayers - 1)], 3)) - 1))) * 0.2);
                  if (whatneed > 42.0) {
                    whatneed = 42.0;
                  }
                  double needgrip2 = xround(whatneed - startgrip2, 1);
                  int32_t gripstats2 = jtrunc_d((needgrip2 * 5.0));
                  if (needgrip2 < 0.0) {
                    gripstats2 = 0;
                  }
                  if (totalsp[g] < gripstats2) {
                    r->sp[g][CS_GRIP] = totalsp[g];
                  } else {
                    r->sp[g][CS_GRIP] = gripstats2;
                    int32_t remaining2 = i32(totalsp[g] - gripstats2);
                    r->sp[g][CS_TS] = jdiv((imul(remaining2, 3)), 4);
                    r->sp[g][CS_END] = i32(remaining2 - r->sp[g][CS_TS]);
                  }
                }
              }
            } else {
              float userstrength = (float)(momentreset_tab[r->sc[0]] + ((float)((float)(r->sp[0][CS_STR]) * 0.02500000037252903)));
              if ((userstrength < 4.0) || (((userstrength < 2.5) && (r->stage == 5)))) {
                r->sp[g][CS_END] = jdiv((imul(totalsp[g], 5)), 16);
                r->sp[g][CS_TS] = i32(totalsp[g] - r->sp[g][CS_END]);
              } else {
                r->sp[g][CS_END] = jdiv((imul(totalsp[g], 3)), 4);
                r->sp[g][CS_TS] = i32(totalsp[g] - r->sp[g][CS_END]);
              }
            }
          }
          if ((id(r->sc[g]) == 4) || (id(r->sc[g]) == 27)) {
            r->sp[g][CS_END] = jdiv((imul(totalsp[g], 9)), 16);
            r->sp[g][CS_STR] = jdiv(totalsp[g], 4);
            r->sp[g][CS_TS] = i32(totalsp[g] - ((i32(r->sp[g][CS_END] + r->sp[g][CS_STR]))));
          }
          if ((id(r->sc[g]) == 5) || (id(r->sc[g]) == 28)) {
            if ((((((r->stage != 6) && (r->stage != 10)) && (r->stage != 5)) && (r->stage != 7)) && (r->stage != 9)) && (r->stage != 13)) {
              r->sp[g][CS_END] = jdiv((imul(totalsp[g], 3)), 8);
              r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 3)), 8);
              if (r->beast[g]) {
                r->sp[g][CS_STR] = jdiv(totalsp[g], 2);
              }
              r->sp[g][CS_TS] = i32((i32(totalsp[g] - r->sp[g][CS_STR])) - r->sp[g][CS_END]);
            } else {
              if (r->stage == 5) {
                float userstrength = (float)(momentreset_tab[r->sc[0]] + ((float)((float)(r->sp[0][CS_STR]) * 0.02500000037252903)));
                if (userstrength < 2.5) {
                  r->sp[g][CS_END] = jdiv((imul(totalsp[g], 3)), 8);
                  r->sp[g][CS_TS] = i32(totalsp[g] - r->sp[g][CS_END]);
                } else {
                  r->sp[g][CS_END] = jdiv((imul(totalsp[g], 5)), 8);
                  r->sp[g][CS_TS] = i32(totalsp[g] - r->sp[g][CS_END]);
                }
                if (r->beast[g]) {
                  r->sp[g][CS_TS] = jdiv((imul(career_spcalc(r->level[g]), 21)), 32);
                  r->sp[g][CS_STR] = jdiv(totalsp[g], 3);
                  r->sp[g][CS_END] = i32((i32(totalsp[g] - r->sp[g][CS_STR])) - r->sp[g][CS_TS]);
                }
              }
              if ((r->stage == 6) || (r->stage == 7)) {
                if (!r->beast[g]) {
                  r->sp[g][CS_STR] = jdiv(totalsp[g], 2);
                  r->sp[g][CS_END] = jdiv(totalsp[g], 2);
                } else {
                  bool afuckingracer = false;
                  int32_t totalpoints = i32(career_spcalc(r->level[0]) + s->extpoints[r->sc[0]]);
                  int32_t instrength = jdiv(totalpoints, 8);
                  if ((i32((i32(r->sp[0][CS_STR] - r->level[0])) + 1)) < instrength) {
                    afuckingracer = true;
                  }
                  int32_t extras = 65;
                  if (r->scalelevels) {
                    extras = 0;
                  }
                  if (!afuckingracer) {
                    if (r->stage == 6) {
                      r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 3)), 16);
                      r->sp[g][CS_TS] = jdiv(totalsp[g], 8);
                      r->sp[g][CS_ACC] = extras;
                      r->sp[g][CS_STU] = extras;
                      r->sp[g][CS_END] = i32((i32((i32(totalsp[g] - r->sp[g][CS_STR])) - r->sp[g][CS_TS])) - (imul(extras, 2)));
                    }
                    if (r->stage == 7) {
                      r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 7)), 32);
                      r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 5)), 32);
                      r->sp[g][CS_STU] = i32(extras - (jdiv(totalsp[g], 32)));
                      r->sp[g][CS_ACC] = i32(extras - (jdiv(totalsp[g], 32)));
                      r->sp[g][CS_END] = i32((i32((i32((i32(totalsp[g] - r->sp[g][CS_STR])) - r->sp[g][CS_TS])) - r->sp[g][CS_STU])) - r->sp[g][CS_ACC]);
                    }
                  } else {
                    r->sp[g][CS_END] = jdiv((imul(totalsp[g], 3)), 16);
                    if (r->stage == 6) {
                      r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 5)), 16);
                    }
                    if (r->stage == 7) {
                      r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 3)), 16);
                    }
                    r->sp[g][CS_STU] = extras;
                    r->sp[g][CS_TS] = i32((i32((i32(totalsp[g] - r->sp[g][CS_END])) - r->sp[g][CS_STR])) - extras);
                  }
                }
              }
              if (r->stage == 9) {
                double startgrip2 = gripreset[r->sc[g]] + ((((r->level[g] + statadjust[2]) - 1.0)) / 5.0);
                double whatneed = 24.2 + (((i32((imul(r->level[(r->nplayers - 1)], 3)) - 1))) * 0.2);
                if (whatneed > 42.0) {
                  whatneed = 42.0;
                }
                double needgrip2 = xround(whatneed - startgrip2, 1);
                int32_t gripstats2 = jtrunc_d((needgrip2 * 5.0));
                if (needgrip2 < 0.0) {
                  gripstats2 = 0;
                }
                if (totalsp[g] < gripstats2) {
                  r->sp[g][CS_GRIP] = totalsp[g];
                } else {
                  r->sp[g][CS_GRIP] = gripstats2;
                  int32_t remaining2 = i32(totalsp[g] - gripstats2);
                  r->sp[g][CS_TS] = jdiv((imul(remaining2, 3)), 4);
                  r->sp[g][CS_END] = i32(remaining2 - r->sp[g][CS_TS]);
                }
              }
              if (r->stage == 10) {
                r->sp[g][CS_END] = jdiv((imul(totalsp[g], 3)), 8);
                r->sp[g][CS_TS] = i32(totalsp[g] - r->sp[g][CS_END]);
              }
              if (r->stage == 13) {
                r->sp[g][CS_STR] = jdiv(totalsp[g], 2);
                r->sp[g][CS_TS] = jdiv(totalsp[g], 8);
                r->sp[g][CS_END] = i32((i32(totalsp[g] - r->sp[g][CS_STR])) - r->sp[g][CS_TS]);
              }
            }
          }
          if ((id(r->sc[g]) == 6) || (id(r->sc[g]) == 29)) {
            if (!r->beast[g] && (r->stage != 13)) {
              if ((r->stage != 9) && (r->stage != 11)) {
                r->sp[g][CS_END] = jdiv((imul(totalsp[g], 9)), 16);
                r->sp[g][CS_STR] = i32(totalsp[g] - r->sp[g][CS_END]);
              } else {
                if (r->stage == 9) {
                  double startgrip2 = gripreset[r->sc[g]] + ((((r->level[g] + statadjust[2]) - 1.0)) / 5.0);
                  double whatneed = 24.2 + (((i32((imul(r->level[(r->nplayers - 1)], 3)) - 1))) * 0.2);
                  if (whatneed > 42.0) {
                    whatneed = 42.0;
                  }
                  double needgrip2 = xround(whatneed - startgrip2, 1);
                  int32_t gripstats2 = jtrunc_d((needgrip2 * 5.0));
                  if (needgrip2 < 0.0) {
                    gripstats2 = 0;
                  }
                  if (totalsp[g] < gripstats2) {
                    r->sp[g][CS_GRIP] = totalsp[g];
                  } else {
                    r->sp[g][CS_GRIP] = gripstats2;
                    int32_t remaining2 = i32(totalsp[g] - gripstats2);
                    r->sp[g][CS_TS] = jdiv((imul(remaining2, 3)), 4);
                    r->sp[g][CS_END] = i32(remaining2 - r->sp[g][CS_TS]);
                  }
                }
                if (r->stage == 11) {
                  double startgrip2 = gripreset[r->sc[g]] + ((((r->level[g] + statadjust[2]) - 1.0)) / 5.0);
                  double whatneed = 24.2 + (((i32((imul(r->level[(r->nplayers - 1)], 3)) - 1))) * 0.2);
                  if (whatneed > 45.6) {
                    whatneed = 45.6;
                  }
                  double needgrip2 = xround(whatneed - startgrip2, 1);
                  int32_t gripstats2 = jtrunc_d((needgrip2 * 5.0));
                  if (needgrip2 < 0.0) {
                    gripstats2 = 0;
                  }
                  if (totalsp[g] < gripstats2) {
                    r->sp[g][CS_GRIP] = totalsp[g];
                  } else {
                    r->sp[g][CS_GRIP] = gripstats2;
                    int32_t remaining2 = i32(totalsp[g] - gripstats2);
                    r->sp[g][CS_END] = jdiv(remaining2, 2);
                    r->sp[g][CS_STR] = jdiv((imul(remaining2, 3)), 8);
                    r->sp[g][CS_TS] = i32((i32(remaining2 - r->sp[g][CS_END])) - r->sp[g][CS_STR]);
                  }
                }
              }
            } else if ((r->beast[g] && (r->stage >= 6)) && (r->stage <= 8)) {
              bool afuckingracer = false;
              int32_t totalpoints = i32(career_spcalc(r->level[0]) + s->extpoints[r->sc[0]]);
              int32_t instrength = jdiv(totalpoints, 8);
              if ((i32((i32(r->sp[0][CS_STR] - r->level[0])) + 1)) < instrength) {
                afuckingracer = true;
              }
              int32_t extras = 65;
              if (r->scalelevels) {
                extras = 0;
              }
              if (!afuckingracer) {
                if (r->stage == 6) {
                  r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 3)), 16);
                  r->sp[g][CS_TS] = jdiv(totalsp[g], 8);
                  r->sp[g][CS_ACC] = extras;
                  r->sp[g][CS_STU] = extras;
                  r->sp[g][CS_END] = i32((i32((i32(totalsp[g] - r->sp[g][CS_STR])) - r->sp[g][CS_TS])) - (imul(extras, 2)));
                }
                if (r->stage == 7) {
                  r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 7)), 32);
                  r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 5)), 32);
                  r->sp[g][CS_STU] = i32(extras - (jdiv(totalsp[g], 32)));
                  r->sp[g][CS_ACC] = i32(extras - (jdiv(totalsp[g], 32)));
                  r->sp[g][CS_END] = i32((i32((i32((i32(totalsp[g] - r->sp[g][CS_STR])) - r->sp[g][CS_TS])) - r->sp[g][CS_STU])) - r->sp[g][CS_ACC]);
                }
              } else {
                r->sp[g][CS_END] = jdiv((imul(totalsp[g], 3)), 16);
                if (r->stage == 6) {
                  r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 5)), 16);
                }
                if (r->stage == 7) {
                  r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 3)), 16);
                }
                r->sp[g][CS_STU] = extras;
                r->sp[g][CS_TS] = i32((i32((i32(totalsp[g] - r->sp[g][CS_END])) - r->sp[g][CS_STR])) - extras);
              }
              if (r->stage == 8) {
                if (afuckingracer) {
                  r->sp[g][CS_END] = jdiv(totalsp[g], 2);
                } else {
                  r->sp[g][CS_END] = jdiv((imul(totalsp[g], 5)), 16);
                }
                r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 5)), 16);
                r->sp[g][CS_TS] = i32((i32(totalsp[g] - r->sp[g][CS_END])) - r->sp[g][CS_STR]);
              }
            } else {
              r->sp[g][CS_END] = jdiv(totalsp[g], 2);
              r->sp[g][CS_STR] = jdiv(totalsp[g], 2);
            }
          }
          if ((id(r->sc[g]) == 7) || (id(r->sc[g]) == 30)) {
            if (((r->stage != 6) && (r->stage != 7)) && (r->stage != 8)) {
              if (!r->beast[g]) {
                r->sp[g][CS_STR] = jdiv(totalsp[g], 3);
                r->sp[g][CS_TS] = jdiv(totalsp[g], 6);
                r->sp[g][CS_END] = i32((i32(totalsp[g] - r->sp[g][CS_TS])) - r->sp[g][CS_STR]);
              } else {
                r->sp[g][CS_TS] = jdiv(totalsp[g], 4);
                r->sp[g][CS_END] = jdiv((imul(totalsp[g], 3)), 8);
                r->sp[g][CS_STR] = i32((i32(totalsp[g] - r->sp[g][CS_TS])) - r->sp[g][CS_END]);
              }
            } else if (!r->beast[g]) {
              r->sp[g][CS_STR] = jdiv(totalsp[g], 2);
              r->sp[g][CS_END] = jdiv(totalsp[g], 2);
            } else {
              bool afuckingracer = false;
              int32_t totalpoints = i32(career_spcalc(r->level[0]) + s->extpoints[r->sc[0]]);
              int32_t instrength = jdiv(totalpoints, 8);
              if ((i32((i32(r->sp[0][CS_STR] - r->level[0])) + 1)) < instrength) {
                afuckingracer = true;
              }
              int32_t extras = 65;
              if (r->scalelevels) {
                extras = 0;
              }
              if (!afuckingracer) {
                if (r->stage == 6) {
                  r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 3)), 16);
                  r->sp[g][CS_TS] = jdiv(totalsp[g], 8);
                  r->sp[g][CS_ACC] = extras;
                  r->sp[g][CS_STU] = extras;
                  r->sp[g][CS_END] = i32((i32((i32(totalsp[g] - r->sp[g][CS_STR])) - r->sp[g][CS_TS])) - (imul(extras, 2)));
                }
                if (r->stage == 7) {
                  r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 7)), 32);
                  r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 5)), 32);
                  r->sp[g][CS_STU] = i32(extras - (jdiv(totalsp[g], 32)));
                  r->sp[g][CS_ACC] = i32(extras - (jdiv(totalsp[g], 32)));
                  r->sp[g][CS_END] = i32((i32((i32((i32(totalsp[g] - r->sp[g][CS_STR])) - r->sp[g][CS_TS])) - r->sp[g][CS_STU])) - r->sp[g][CS_ACC]);
                }
              } else {
                r->sp[g][CS_END] = jdiv((imul(totalsp[g], 3)), 16);
                if (r->stage == 6) {
                  r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 5)), 16);
                }
                if (r->stage == 7) {
                  r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 3)), 16);
                }
                r->sp[g][CS_STU] = extras;
                r->sp[g][CS_TS] = i32((i32((i32(totalsp[g] - r->sp[g][CS_END])) - r->sp[g][CS_STR])) - extras);
              }
              if (r->stage == 8) {
                if (afuckingracer) {
                  r->sp[g][CS_END] = jdiv(totalsp[g], 2);
                } else {
                  r->sp[g][CS_END] = jdiv((imul(totalsp[g], 5)), 16);
                }
                r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 5)), 16);
                r->sp[g][CS_TS] = i32((i32(totalsp[g] - r->sp[g][CS_END])) - r->sp[g][CS_STR]);
              }
            }
          }
          if ((id(r->sc[g]) == 8) || (id(r->sc[g]) == 31)) {
            if ((((r->stage < 5) || (r->stage > 14))) && !r->beast[g]) {
              r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 3)), 8);
              r->sp[g][CS_TS] = jdiv(totalsp[g], 8);
              r->sp[g][CS_END] = i32((i32(totalsp[g] - r->sp[g][CS_STR])) - r->sp[g][CS_TS]);
            } else if (r->beast[g]) {
              if ((r->stage >= 6) && (r->stage <= 8)) {
                bool afuckingracer = false;
                int32_t totalpoints = i32(career_spcalc(r->level[0]) + s->extpoints[r->sc[0]]);
                int32_t instrength = jdiv(totalpoints, 8);
                if ((i32((i32(r->sp[0][CS_STR] - r->level[0])) + 1)) < instrength) {
                  afuckingracer = true;
                }
                int32_t extras = 65;
                if (r->scalelevels) {
                  extras = 0;
                }
                if (!afuckingracer) {
                  if (r->stage == 6) {
                    r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 3)), 16);
                    r->sp[g][CS_TS] = jdiv(totalsp[g], 8);
                    r->sp[g][CS_ACC] = extras;
                    r->sp[g][CS_STU] = extras;
                    r->sp[g][CS_END] = i32((i32((i32(totalsp[g] - r->sp[g][CS_STR])) - r->sp[g][CS_TS])) - (imul(extras, 2)));
                  }
                  if (r->stage == 7) {
                    r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 7)), 32);
                    r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 5)), 32);
                    r->sp[g][CS_STU] = i32(extras - (jdiv(totalsp[g], 32)));
                    r->sp[g][CS_ACC] = i32(extras - (jdiv(totalsp[g], 32)));
                    r->sp[g][CS_END] = i32((i32((i32((i32(totalsp[g] - r->sp[g][CS_STR])) - r->sp[g][CS_TS])) - r->sp[g][CS_STU])) - r->sp[g][CS_ACC]);
                  }
                } else {
                  r->sp[g][CS_END] = jdiv((imul(totalsp[g], 3)), 16);
                  if (r->stage == 6) {
                    r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 5)), 16);
                  }
                  if (r->stage == 7) {
                    r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 3)), 16);
                  }
                  r->sp[g][CS_STU] = extras;
                  r->sp[g][CS_TS] = i32((i32((i32(totalsp[g] - r->sp[g][CS_END])) - r->sp[g][CS_STR])) - extras);
                }
                if (r->stage == 8) {
                  if (afuckingracer) {
                    r->sp[g][CS_END] = jdiv(totalsp[g], 2);
                  } else {
                    r->sp[g][CS_END] = jdiv((imul(totalsp[g], 5)), 16);
                  }
                  r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 5)), 16);
                  r->sp[g][CS_TS] = i32((i32(totalsp[g] - r->sp[g][CS_END])) - r->sp[g][CS_STR]);
                }
              } else if ((r->stage == 12) || (r->stage == 13)) {
                if (r->stage == 12) {
                  r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 9)), 64);
                  r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 5)), 16);
                  r->sp[g][CS_GRIP] = jdiv(totalsp[g], 16);
                  r->sp[g][CS_END] = i32(totalsp[g] - ((i32((i32(r->sp[g][CS_TS] + r->sp[g][CS_STR])) + r->sp[g][CS_GRIP]))));
                } else {
                  r->sp[g][CS_STR] = jdiv(totalsp[g], 2);
                  r->sp[g][CS_TS] = jdiv(totalsp[g], 8);
                  r->sp[g][CS_END] = i32((i32(totalsp[g] - r->sp[g][CS_STR])) - r->sp[g][CS_TS]);
                }
              } else {
                r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 3)), 32);
                r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 3)), 8);
                r->sp[g][CS_END] = i32(totalsp[g] - ((i32(r->sp[g][CS_TS] + r->sp[g][CS_STR]))));
              }
            } else {
              if ((r->stage == 10) || (r->stage == 5)) {
                float userstrength = (float)(momentreset_tab[r->sc[0]] + ((float)((float)(r->sp[0][CS_STR]) * 0.02500000037252903)));
                if ((userstrength < 4.0) || (((userstrength < 2.5) && (r->stage == 5)))) {
                  r->sp[g][CS_END] = jdiv((imul(totalsp[g], 3)), 8);
                  r->sp[g][CS_TS] = i32(totalsp[g] - r->sp[g][CS_END]);
                } else {
                  r->sp[g][CS_END] = jdiv((imul(totalsp[g], 3)), 4);
                  r->sp[g][CS_TS] = i32(totalsp[g] - r->sp[g][CS_END]);
                }
              }
              if (r->stage == 9) {
                r->sp[g][CS_END] = jdiv(totalsp[g], 2);
                r->sp[g][CS_STR] = jdiv(totalsp[g], 2);
              }
              if (r->stage == 11) {
                r->sp[g][CS_END] = totalsp[g];
              }
              if (r->stage == 12) {
                r->sp[g][CS_STR] = jdiv(totalsp[g], 4);
                r->sp[g][CS_END] = jdiv((imul(totalsp[g], 3)), 4);
              }
              if (r->stage == 14) {
                r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 3)), 4);
                r->sp[g][CS_END] = jdiv(totalsp[g], 4);
              }
              if ((r->stage == 6) || (r->stage == 7)) {
                r->sp[g][CS_TS] = jdiv(totalsp[g], 8);
                r->sp[g][CS_END] = jdiv((imul(totalsp[g], 9)), 16);
                r->sp[g][CS_STR] = i32((i32(totalsp[g] - r->sp[g][CS_TS])) - r->sp[g][CS_END]);
              }
              if (r->stage == 8) {
                r->sp[g][CS_STR] = jdiv(totalsp[g], 2);
                r->sp[g][CS_TS] = jdiv(totalsp[g], 8);
                r->sp[g][CS_END] = i32((i32(totalsp[g] - r->sp[g][CS_STR])) - r->sp[g][CS_TS]);
              }
              if (r->stage == 13) {
                r->sp[g][CS_STR] = jdiv(totalsp[g], 2);
                r->sp[g][CS_TS] = jdiv(totalsp[g], 8);
                r->sp[g][CS_END] = i32((i32(totalsp[g] - r->sp[g][CS_STR])) - r->sp[g][CS_TS]);
              }
            }
          }
          if ((id(r->sc[g]) == 9) || (id(r->sc[g]) == 32)) {
            if (((((((((((((r->stage != 11) && (r->stage != 5)) && (r->stage != 8)) && (r->stage != 6)) && (r->stage != 7)) && (r->stage != 12)) && (r->stage != 16)) && (r->stage != 17)) && (r->stage != 18))) || r->beast[g])) && !(r->bonus == 3)) {
              if (!r->beast[g] || (r->stage != 12)) {
                if ((((((r->stage != 16) && (r->stage != 17)) && (r->stage != 18)) && (r->stage != 6)) && (r->stage != 7)) && (r->stage != 8)) {
                  if (((r->stage != 10) && (r->stage != 11)) && (r->stage != 13)) {
                    r->sp[g][CS_END] = jdiv(totalsp[g], 2);
                    r->sp[g][CS_TS] = jdiv(totalsp[g], 4);
                    r->sp[g][CS_STR] = jdiv(totalsp[g], 4);
                  } else {
                    if (r->stage == 10) {
                      float userstrength = (float)(momentreset_tab[r->sc[0]] + ((float)((float)(r->sp[0][CS_STR]) * 0.02500000037252903)));
                      if (userstrength < 4.0) {
                        r->sp[g][CS_STR] = jdiv(totalsp[g], 4);
                        r->sp[g][CS_END] = jdiv(totalsp[g], 4);
                        r->sp[g][CS_TS] = i32((i32(totalsp[g] - r->sp[g][CS_STR])) - r->sp[g][CS_END]);
                      } else {
                        r->sp[g][CS_END] = jdiv((imul(totalsp[g], 3)), 4);
                        r->sp[g][CS_TS] = jdiv(totalsp[g], 4);
                      }
                    }
                    if (r->stage == 13) {
                      r->sp[g][CS_STR] = jdiv(totalsp[g], 2);
                      r->sp[g][CS_TS] = jdiv(totalsp[g], 8);
                      r->sp[g][CS_END] = i32((i32(totalsp[g] - r->sp[g][CS_STR])) - r->sp[g][CS_TS]);
                    }
                    if (r->stage == 11) {
                      double startgrip2 = gripreset[r->sc[g]] + ((((r->level[g] + statadjust[2]) - 1.0)) / 5.0);
                      double whatneed = 24.2 + (((i32((imul(r->level[(r->nplayers - 1)], 3)) - 1))) * 0.2);
                      if (whatneed > 45.6) {
                        whatneed = 45.6;
                      }
                      double needgrip2 = xround(whatneed - startgrip2, 1);
                      int32_t gripstats2 = jtrunc_d((needgrip2 * 5.0));
                      if (needgrip2 < 0.0) {
                        gripstats2 = 0;
                      }
                      if (totalsp[g] < gripstats2) {
                        r->sp[g][CS_GRIP] = totalsp[g];
                      } else {
                        r->sp[g][CS_GRIP] = gripstats2;
                        int32_t remaining2 = i32(totalsp[g] - gripstats2);
                        r->sp[g][CS_STR] = jdiv((imul(remaining2, 13)), 32);
                        r->sp[g][CS_TS] = jdiv((imul(remaining2, 3)), 32);
                        r->sp[g][CS_END] = i32((i32(remaining2 - r->sp[g][CS_STR])) - r->sp[g][CS_TS]);
                      }
                    }
                  }
                } else if (((r->stage != 6) && (r->stage != 7)) && (r->stage != 8)) {
                  r->sp[g][CS_STR] = jdiv(totalsp[g], 2);
                  r->sp[g][CS_END] = jdiv(totalsp[g], 4);
                  r->sp[g][CS_TS] = jdiv(totalsp[g], 4);
                } else {
                  bool afuckingracer = false;
                  int32_t totalpoints = i32(career_spcalc(r->level[0]) + s->extpoints[r->sc[0]]);
                  int32_t instrength = jdiv(totalpoints, 8);
                  if ((i32((i32(r->sp[0][CS_STR] - r->level[0])) + 1)) < instrength) {
                    afuckingracer = true;
                  }
                  int32_t extras = 65;
                  if (r->scalelevels) {
                    extras = 0;
                  }
                  if (!afuckingracer) {
                    if (r->stage == 6) {
                      r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 3)), 16);
                      r->sp[g][CS_TS] = jdiv(totalsp[g], 8);
                      r->sp[g][CS_ACC] = extras;
                      r->sp[g][CS_STU] = extras;
                      r->sp[g][CS_END] = i32((i32((i32(totalsp[g] - r->sp[g][CS_STR])) - r->sp[g][CS_TS])) - (imul(extras, 2)));
                    }
                    if (r->stage == 7) {
                      r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 7)), 32);
                      r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 5)), 32);
                      r->sp[g][CS_STU] = i32(extras - (jdiv(totalsp[g], 32)));
                      r->sp[g][CS_ACC] = i32(extras - (jdiv(totalsp[g], 32)));
                      r->sp[g][CS_END] = i32((i32((i32((i32(totalsp[g] - r->sp[g][CS_STR])) - r->sp[g][CS_TS])) - r->sp[g][CS_STU])) - r->sp[g][CS_ACC]);
                    }
                  } else {
                    r->sp[g][CS_END] = jdiv((imul(totalsp[g], 3)), 16);
                    if (r->stage == 6) {
                      r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 5)), 16);
                    }
                    if (r->stage == 7) {
                      r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 3)), 16);
                    }
                    r->sp[g][CS_STU] = extras;
                    r->sp[g][CS_TS] = i32((i32((i32(totalsp[g] - r->sp[g][CS_END])) - r->sp[g][CS_STR])) - extras);
                  }
                  if (r->stage == 8) {
                    if (afuckingracer) {
                      r->sp[g][CS_END] = jdiv(totalsp[g], 2);
                    } else {
                      r->sp[g][CS_END] = jdiv((imul(totalsp[g], 5)), 16);
                    }
                    r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 5)), 16);
                    r->sp[g][CS_TS] = i32((i32(totalsp[g] - r->sp[g][CS_END])) - r->sp[g][CS_STR]);
                  }
                }
              } else {
                r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 9)), 64);
                r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 5)), 16);
                r->sp[g][CS_GRIP] = jdiv(totalsp[g], 16);
                r->sp[g][CS_END] = i32(totalsp[g] - ((i32((i32(r->sp[g][CS_TS] + r->sp[g][CS_STR])) + r->sp[g][CS_GRIP]))));
              }
            } else {
              if (r->stage == 11) {
                double startgrip2 = gripreset[r->sc[g]] + ((((r->level[g] + statadjust[2]) - 1.0)) / 5.0);
                double whatneed = 24.2 + (((i32((imul(r->level[(r->nplayers - 1)], 3)) - 1))) * 0.2);
                if (whatneed > 45.6) {
                  whatneed = 45.6;
                }
                double needgrip2 = xround(whatneed - startgrip2, 1);
                int32_t gripstats2 = jtrunc_d((needgrip2 * 5.0));
                if (needgrip2 < 0.0) {
                  gripstats2 = 0;
                }
                if (totalsp[g] < gripstats2) {
                  r->sp[g][CS_GRIP] = totalsp[g];
                } else {
                  r->sp[g][CS_GRIP] = gripstats2;
                  int32_t remaining2 = i32(totalsp[g] - gripstats2);
                  r->sp[g][CS_STR] = jdiv((imul(remaining2, 3)), 4);
                  r->sp[g][CS_END] = i32(remaining2 - r->sp[g][CS_STR]);
                }
              }
              if ((r->stage == 12) || (((r->stage == 17) && !r->shadow[g]))) {
                r->sp[g][CS_END] = jdiv((imul(totalsp[g], 2)), 3);
                r->sp[g][CS_STR] = i32(totalsp[g] - r->sp[g][CS_END]);
              }
              if (r->stage == 16) {
                if (!r->shadow[g] || r->scalelevels) {
                  r->sp[g][CS_END] = jdiv((imul(totalsp[g], 2)), 3);
                  r->sp[g][CS_STR] = i32(totalsp[g] - r->sp[g][CS_END]);
                } else {
                  r->sp[g][CS_END] = jdiv((imul(totalsp[g], 2)), 3);
                  int32_t alreadyhave = i32((i32(r->level[g] + statadjust[2])) + 49);
                  r->sp[g][CS_GRIP] = i32(122 - alreadyhave);
                  r->sp[g][CS_STR] = i32((i32(totalsp[g] - r->sp[g][CS_END])) - r->sp[g][CS_GRIP]);
                }
              }
              if (r->stage == 5) {
                r->sp[g][CS_STR] = jdiv(totalsp[g], 2);
                r->sp[g][CS_TS] = jdiv(totalsp[g], 2);
              }
              if ((r->stage == 7) || (r->stage == 8)) {
                r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 5)), 8);
                r->sp[g][CS_TS] = jdiv(totalsp[g], 8);
                r->sp[g][CS_END] = i32((i32(totalsp[g] - r->sp[g][CS_STR])) - r->sp[g][CS_TS]);
              }
              if ((((r->stage == 17) || (r->stage == 18))) && r->shadow[g]) {
                r->sp[g][CS_TS] = jdiv(totalsp[g], 4);
                r->sp[g][CS_END] = jdiv((imul(totalsp[g], 3)), 4);
              }
              if ((r->stage == 18) && !r->shadow[g]) {
                r->sp[g][CS_END] = jdiv(totalsp[g], 2);
                r->sp[g][CS_TS] = jdiv(totalsp[g], 2);
              }
              if (r->stage == 6) {
                r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 3)), 4);
                r->sp[g][CS_END] = jdiv(totalsp[g], 4);
              }
              if ((r->bonus == 3)) {
                r->sp[g][CS_STR] = jdiv(totalsp[g], 2);
                r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 3)), 32);
                r->sp[g][CS_END] = i32(totalsp[g] - ((i32(r->sp[g][CS_STR] + r->sp[g][CS_TS]))));
              }
            }
          }
          if ((id(r->sc[g]) == 10) || (id(r->sc[g]) == 33)) {
            if ((((r->stage < 8) || (r->stage > 16))) && !r->beast[g]) {
              r->sp[g][CS_TS] = jdiv(totalsp[g], 4);
              r->sp[g][CS_END] = i32(totalsp[g] - r->sp[g][CS_TS]);
            } else {
              if ((((r->beast[g] && (((r->stage != 15) || (r->bonus == 3)))) && (r->stage != 7)) && (r->stage != 8)) && (r->stage != 13)) {
                if (!(r->bonus != 0)) {
                  if ((r->stage != 16) || r->scalelevels) {
                    if (r->stage < 8) {
                      r->sp[g][CS_END] = jdiv((imul(totalsp[g], 3)), 4);
                      r->sp[g][CS_STR] = i32(totalsp[g] - r->sp[g][CS_END]);
                    } else if (r->stage == 11) {
                      double startgrip2 = gripreset[r->sc[g]] + ((((r->level[g] + statadjust[2]) - 1.0)) / 5.0);
                      double whatneed = 24.2 + (((i32((imul(r->level[(r->nplayers - 1)], 3)) - 1))) * 0.2);
                      if (whatneed > 45.6) {
                        whatneed = 45.6;
                      }
                      double needgrip2 = xround(whatneed - startgrip2, 1);
                      int32_t gripstats2 = jtrunc_d((needgrip2 * 5.0));
                      if (needgrip2 < 0.0) {
                        gripstats2 = 0;
                      }
                      if (totalsp[g] < gripstats2) {
                        r->sp[g][CS_GRIP] = totalsp[g];
                      } else {
                        r->sp[g][CS_GRIP] = gripstats2;
                        int32_t remaining2 = i32(totalsp[g] - gripstats2);
                        r->sp[g][CS_END] = jdiv(remaining2, 2);
                        r->sp[g][CS_STR] = jdiv((imul(remaining2, 3)), 8);
                        r->sp[g][CS_TS] = i32((i32(remaining2 - r->sp[g][CS_END])) - r->sp[g][CS_STR]);
                      }
                    } else if (r->stage == 12) {
                      r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 9)), 64);
                      r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 5)), 16);
                      r->sp[g][CS_GRIP] = jdiv(totalsp[g], 16);
                      r->sp[g][CS_END] = i32(totalsp[g] - ((i32((i32(r->sp[g][CS_TS] + r->sp[g][CS_STR])) + r->sp[g][CS_GRIP]))));
                    } else {
                      r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 7)), 32);
                      r->sp[g][CS_STR] = jdiv(totalsp[g], 2);
                      r->sp[g][CS_END] = i32(totalsp[g] - ((i32(r->sp[g][CS_TS] + r->sp[g][CS_STR]))));
                    }
                  } else {
                    int32_t alreadyhave = i32((i32(r->level[g] + statadjust[2])) - 1);
                    r->sp[g][CS_GRIP] = i32(120 - alreadyhave);
                    int32_t remaining3 = i32(totalsp[g] - r->sp[g][CS_GRIP]);
                    r->sp[g][CS_STR] = jdiv(remaining3, 2);
                    r->sp[g][CS_END] = jdiv(remaining3, 4);
                    r->sp[g][CS_TS] = i32((i32((i32(totalsp[g] - r->sp[g][CS_STR])) - r->sp[g][CS_END])) - r->sp[g][CS_GRIP]);
                  }
                } else if (!(r->bonus == 3)) {
                  r->sp[g][CS_END] = jdiv(totalsp[g], 2);
                  r->sp[g][CS_STR] = jdiv(totalsp[g], 2);
                } else {
                  r->sp[g][CS_END] = jdiv((imul(totalsp[g], 5)), 8);
                  r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 3)), 32);
                  r->sp[g][CS_STR] = i32(totalsp[g] - ((i32(r->sp[g][CS_END] + r->sp[g][CS_TS]))));
                }
              }
              if ((r->stage == 9) || (((r->stage == 11) && !(r->bonus == 3)))) {
                double startgrip2 = gripreset[r->sc[g]] + ((((r->level[g] + statadjust[2]) - 1.0)) / 5.0);
                double whatneed = 24.2 + (((i32((imul(r->level[(r->nplayers - 1)], 3)) - 1))) * 0.2);
                if (whatneed > 42.0) {
                  whatneed = 42.0;
                }
                if (r->stage == 11) {
                  whatneed = 24.2 + (((i32((imul(r->level[(r->nplayers - 1)], 3)) - 1))) * 0.2);
                  if (whatneed > 45.6) {
                    whatneed = 45.6;
                  }
                }
                double needgrip2 = xround(whatneed - startgrip2, 1);
                int32_t gripstats2 = jtrunc_d((needgrip2 * 5.0));
                if (needgrip2 < 0.0) {
                  gripstats2 = 0;
                }
                if (totalsp[g] < gripstats2) {
                  r->sp[g][CS_GRIP] = totalsp[g];
                } else {
                  r->sp[g][CS_GRIP] = gripstats2;
                  int32_t remaining2 = i32(totalsp[g] - gripstats2);
                  if (r->stage == 9) {
                    r->sp[g][CS_TS] = jdiv((imul(remaining2, 3)), 4);
                    r->sp[g][CS_END] = i32(remaining2 - r->sp[g][CS_TS]);
                  }
                  if (r->stage == 11) {
                    r->sp[g][CS_END] = jdiv((imul(remaining2, 3)), 4);
                    r->sp[g][CS_TS] = i32(remaining2 - r->sp[g][CS_TS]);
                  }
                }
              }
              if ((r->stage == 12) && !r->beast[g]) {
                r->sp[g][CS_END] = totalsp[g];
              }
              if (r->stage == 13) {
                r->sp[g][CS_TS] = jdiv(totalsp[g], 8);
                if ((s->unlocked == r->stage) || r->hardstage) {
                  r->sp[g][CS_TS] = i32((i32(356 - nitroswits_tab[r->sc[g]][2])) - ((i32((i32(r->level[g] + statadjust[0])) - 1))));
                }
                r->sp[g][CS_END] = i32(totalsp[g] - r->sp[g][CS_TS]);
                if (r->beast[g]) {
                  r->sp[g][CS_STR] = jdiv(totalsp[g], 2);
                  r->sp[g][CS_TS] = jdiv(totalsp[g], 8);
                  r->sp[g][CS_END] = i32((i32(totalsp[g] - r->sp[g][CS_STR])) - r->sp[g][CS_TS]);
                }
              }
              if ((r->stage == 15) && !(r->bonus == 3)) {
                r->sp[g][CS_TS] = jdiv(totalsp[g], 8);
                r->sp[g][CS_END] = i32(totalsp[g] - r->sp[g][CS_TS]);
              }
              if ((r->stage == 16) && !r->beast[g]) {
                if (!r->shadow[g] || r->scalelevels) {
                  r->sp[g][CS_TS] = jdiv(totalsp[g], 8);
                  r->sp[g][CS_END] = i32(totalsp[g] - r->sp[g][CS_TS]);
                } else {
                  int32_t alreadyhave = i32((i32(r->level[g] + statadjust[2])) + 49);
                  r->sp[g][CS_GRIP] = i32(120 - alreadyhave);
                  r->sp[g][CS_TS] = jdiv(totalsp[g], 8);
                  r->sp[g][CS_END] = i32((i32(totalsp[g] - r->sp[g][CS_TS])) - r->sp[g][CS_GRIP]);
                }
              }
              if ((r->stage == 8) && !r->beast[g]) {
                r->sp[g][CS_END] = jdiv(totalsp[g], 4);
                r->sp[g][CS_TS] = jdiv(totalsp[g], 4);
                r->sp[g][CS_STR] = jdiv(totalsp[g], 2);
              }
              if (r->beast[g]) {
                bool afuckingracer = false;
                int32_t totalpoints = i32(career_spcalc(r->level[0]) + s->extpoints[r->sc[0]]);
                int32_t instrength = jdiv(totalpoints, 8);
                if ((i32((i32(r->sp[0][CS_STR] - r->level[0])) + 1)) < instrength) {
                  afuckingracer = true;
                }
                int32_t extras = 65;
                if (r->scalelevels) {
                  extras = 0;
                }
                if (r->stage == 7) {
                  if (!afuckingracer) {
                    r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 7)), 32);
                    r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 5)), 32);
                    r->sp[g][CS_STU] = i32(extras - (jdiv(totalsp[g], 32)));
                    r->sp[g][CS_ACC] = i32(extras - (jdiv(totalsp[g], 32)));
                    r->sp[g][CS_END] = i32((i32((i32((i32(totalsp[g] - r->sp[g][CS_STR])) - r->sp[g][CS_TS])) - r->sp[g][CS_STU])) - r->sp[g][CS_ACC]);
                  } else {
                    r->sp[g][CS_END] = jdiv((imul(totalsp[g], 3)), 16);
                    r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 3)), 16);
                    r->sp[g][CS_STU] = extras;
                    r->sp[g][CS_TS] = i32((i32((i32(totalsp[g] - r->sp[g][CS_END])) - r->sp[g][CS_STR])) - extras);
                  }
                }
                if (r->stage == 8) {
                  if (afuckingracer) {
                    r->sp[g][CS_END] = jdiv(totalsp[g], 2);
                  } else {
                    r->sp[g][CS_END] = jdiv((imul(totalsp[g], 5)), 16);
                  }
                  r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 5)), 16);
                  r->sp[g][CS_TS] = i32((i32(totalsp[g] - r->sp[g][CS_END])) - r->sp[g][CS_STR]);
                }
              }
              if ((id(r->sc[g]) == 33) && (r->bonus == 2)) {
                r->sp[g][CS_STR] = jdiv(totalsp[g], 4);
                r->sp[g][CS_TS] = jdiv(totalsp[g], 4);
                r->sp[g][CS_END] = jdiv(totalsp[g], 2);
              }
              if (r->stage == 10) {
                float userstrength = (float)(momentreset_tab[r->sc[0]] + ((float)((float)(r->sp[0][CS_STR]) * 0.02500000037252903)));
                if (userstrength < 4.0) {
                  r->sp[g][CS_END] = jdiv((imul(totalsp[g], 3)), 8);
                  r->sp[g][CS_TS] = i32(totalsp[g] - r->sp[g][CS_END]);
                } else {
                  r->sp[g][CS_END] = jdiv((imul(totalsp[g], 3)), 4);
                  r->sp[g][CS_TS] = i32(totalsp[g] - r->sp[g][CS_END]);
                }
              }
              if (r->stage == 14) {
                r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 3)), 4);
                r->sp[g][CS_END] = jdiv(totalsp[g], 8);
                r->sp[g][CS_STU] = i32(totalsp[g] - ((i32(r->sp[g][CS_END] + r->sp[g][CS_TS]))));
              }
            }
          }
          if ((id(r->sc[g]) == 11) || (id(r->sc[g]) == 34)) {
            if (((((((r->stage != 11) && !r->beast[g]) && (r->stage != 12)) && (r->stage != 15)) && (r->stage != 16)) && (r->stage != 17)) && (r->stage != 18)) {
              if (r->stage == 9) {
                double startgrip2 = gripreset[r->sc[g]] + ((((r->level[g] + statadjust[2]) - 1.0)) / 5.0);
                double whatneed = 24.2 + (((i32((imul(r->level[(r->nplayers - 1)], 3)) - 1))) * 0.2);
                if (whatneed > 42.0) {
                  whatneed = 42.0;
                }
                double needgrip2 = xround(whatneed - startgrip2, 1);
                int32_t gripstats2 = jtrunc_d((needgrip2 * 5.0));
                if (needgrip2 < 0.0) {
                  gripstats2 = 0;
                }
                if (totalsp[g] < gripstats2) {
                  r->sp[g][CS_GRIP] = totalsp[g];
                } else {
                  r->sp[g][CS_GRIP] = gripstats2;
                  int32_t remaining2 = i32(totalsp[g] - gripstats2);
                  r->sp[g][CS_TS] = jdiv(remaining2, 2);
                  r->sp[g][CS_END] = i32(remaining2 - r->sp[g][CS_TS]);
                }
              } else {
                bool nostrength = false;
                if ((r->stage == 13) && (((s->unlocked == r->stage) || r->hardstage))) {
                  float userstrength2 = (float)(momentreset_tab[r->sc[0]] + ((float)((float)(r->sp[0][CS_STR]) * 0.02500000037252903)));
                  if (userstrength2 > 5.0) {
                    nostrength = true;
                  }
                }
                if (!nostrength) {
                  r->sp[g][CS_STR] = jdiv(totalsp[g], 2);
                }
                if (r->stage < 18) {
                  r->sp[g][CS_TS] = jdiv(totalsp[g], 6);
                  if ((r->stage == 13) && (((s->unlocked == r->stage) || r->hardstage))) {
                    r->sp[g][CS_TS] = i32((i32(335 - nitroswits_tab[r->sc[g]][2])) - ((i32((i32(r->level[g] + statadjust[0])) - 1))));
                  }
                }
                if (r->stage == 19) {
                  r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 17)), 32);
                  r->sp[g][CS_TS] = jdiv(totalsp[g], 4);
                }
                r->sp[g][CS_END] = i32(totalsp[g] - ((i32(r->sp[g][CS_STR] + r->sp[g][CS_TS]))));
              }
            } else if ((!(r->bonus == 2) && !(r->bonus == 3)) && !(r->bonus == 4)) {
              if ((((((((r->stage != 12) && (r->stage != 15)) && (r->stage != 16))) || r->beast[g])) && (r->stage != 17)) && (r->stage != 18)) {
                if (r->stage == 11) {
                  if (g <= 4) {
                    if (!r->scalelevels) {
                      r->sp[g][CS_TS] = 80;
                      r->sp[g][CS_STR] = 210;
                      r->sp[g][CS_GRIP] = i32(totalsp[g] - ((i32(r->sp[g][CS_TS] + r->sp[g][CS_STR]))));
                    } else {
                      double spdportion = 80.0 / career_beastspcalc(35);
                      double strportion = 210.0 / career_beastspcalc(35);
                      r->sp[g][CS_TS] = jtrunc_d((totalsp[g] * spdportion));
                      r->sp[g][CS_STR] = jtrunc_d((totalsp[g] * strportion));
                      r->sp[g][CS_GRIP] = i32((i32(totalsp[g] - r->sp[g][CS_TS])) - r->sp[g][CS_STR]);
                    }
                  } else {
                    double startgrip2 = gripreset[r->sc[g]] + ((((r->level[g] + statadjust[2]) - 1.0)) / 5.0);
                    double whatneed = 24.2 + (((i32((imul(r->level[(r->nplayers - 1)], 3)) - 1))) * 0.2);
                    if (whatneed > 45.6) {
                      whatneed = 45.6;
                    }
                    double needgrip2 = xround(whatneed - startgrip2, 1);
                    int32_t gripstats2 = jtrunc_d((needgrip2 * 5.0));
                    if (needgrip2 < 0.0) {
                      gripstats2 = 0;
                    }
                    if (totalsp[g] < gripstats2) {
                      r->sp[g][CS_GRIP] = totalsp[g];
                    } else {
                      r->sp[g][CS_GRIP] = gripstats2;
                      int32_t remaining2 = i32(totalsp[g] - gripstats2);
                      if (!r->beast[g]) {
                        r->sp[g][CS_STR] = jdiv(remaining2, 2);
                        r->sp[g][CS_END] = i32((i32(totalsp[g] - r->sp[g][CS_STR])) - r->sp[g][CS_GRIP]);
                      } else {
                        r->sp[g][CS_END] = jdiv((imul(remaining2, 15)), 32);
                        r->sp[g][CS_STR] = jdiv((imul(remaining2, 7)), 16);
                        r->sp[g][CS_TS] = i32((i32(remaining2 - r->sp[g][CS_END])) - r->sp[g][CS_STR]);
                      }
                    }
                  }
                } else {
                  r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 9)), 64);
                  r->sp[g][CS_STR] = jdiv(totalsp[g], 2);
                  r->sp[g][CS_END] = i32(totalsp[g] - ((i32(r->sp[g][CS_TS] + r->sp[g][CS_STR]))));
                }
              } else {
                if ((!r->shadow[g] || (r->stage < 16)) || r->scalelevels) {
                  r->sp[g][CS_STR] = jdiv(totalsp[g], 2);
                  r->sp[g][CS_END] = jdiv((imul(totalsp[g], 7)), 16);
                  r->sp[g][CS_TS] = i32(totalsp[g] - ((i32(r->sp[g][CS_STR] + r->sp[g][CS_END]))));
                } else {
                  r->sp[g][CS_STR] = jdiv(totalsp[g], 2);
                  r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 3)), 8);
                  int32_t alreadyhave = i32((i32(r->level[g] + statadjust[2])) + 49);
                  r->sp[g][CS_GRIP] = i32(120 - alreadyhave);
                  r->sp[g][CS_END] = i32((i32(totalsp[g] - ((i32(r->sp[g][CS_STR] + r->sp[g][CS_TS]))))) - r->sp[g][CS_GRIP]);
                }
                if (r->stage == 17) {
                  if (!r->shadow[g]) {
                    r->sp[g][CS_STR] = jdiv(totalsp[g], 2);
                    r->sp[g][CS_END] = jdiv((imul(totalsp[g], 7)), 16);
                    r->sp[g][CS_TS] = i32(totalsp[g] - ((i32(r->sp[g][CS_STR] + r->sp[g][CS_END]))));
                  } else {
                    r->sp[g][CS_TS] = jdiv(totalsp[g], 8);
                    r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 3)), 8);
                    r->sp[g][CS_END] = i32(totalsp[g] - ((i32(r->sp[g][CS_STR] + r->sp[g][CS_TS]))));
                  }
                }
                if (r->stage == 18) {
                  if (r->shadow[g] && !r->scalelevels) {
                    r->sp[g][CS_TS] = jdiv(totalsp[g], 8);
                    r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 5)), 8);
                    int32_t alreadyhave = i32((i32(r->level[g] + 49)) + statadjust[2]);
                    r->sp[g][CS_GRIP] = i32(135 - alreadyhave);
                    r->sp[g][CS_END] = i32((i32((i32(totalsp[g] - r->sp[g][CS_TS])) - r->sp[g][CS_STR])) - r->sp[g][CS_GRIP]);
                  } else {
                    r->sp[g][CS_STR] = jdiv(totalsp[g], 2);
                    r->sp[g][CS_END] = jdiv((imul(totalsp[g], 7)), 16);
                    r->sp[g][CS_TS] = i32(totalsp[g] - ((i32(r->sp[g][CS_STR] + r->sp[g][CS_END]))));
                  }
                }
              }
            } else {
              if ((r->bonus == 2)) {
                if (g == 5) {
                  r->sp[g][CS_END] = jdiv(totalsp[g], 2);
                  r->sp[g][CS_STR] = i32(totalsp[g] - r->sp[g][CS_END]);
                } else {
                  r->sp[g][CS_END] = jdiv(totalsp[g], 2);
                  r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 13)), 32);
                  r->sp[g][CS_TS] = i32((i32(totalsp[g] - r->sp[g][CS_END])) - r->sp[g][CS_STR]);
                }
              }
              if ((r->bonus == 3)) {
                r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 3)), 32);
                r->sp[g][CS_STR] = jdiv(totalsp[g], 2);
                r->sp[g][CS_END] = i32(totalsp[g] - ((i32(r->sp[g][CS_TS] + r->sp[g][CS_STR]))));
              }
              if ((r->bonus == 4)) {
                r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 7)), 32);
                r->sp[g][CS_STR] = i32(totalsp[g] - r->sp[g][CS_TS]);
              }
            }
          }
          if ((id(r->sc[g]) == 12) || (id(r->sc[g]) == 35)) {
            if (((((((r->stage != 11) && !r->beast[g]) && (r->stage != 13)) && (r->stage != 14)) && (r->stage != 15)) && (r->stage != 16)) && (r->stage != 18)) {
              r->sp[g][CS_TS] = jdiv(totalsp[g], 2);
              r->sp[g][CS_END] = jdiv(totalsp[g], 4);
              r->sp[g][CS_GRIP] = jdiv(totalsp[g], 4);
            } else if (((r->stage != 14) && (r->stage != 13)) && (((((r->stage != 18) && (r->stage != 16))) || r->scalelevels))) {
              if (r->beast[g]) {
                if ((r->stage != 11) && !(r->bonus == 3)) {
                  r->sp[g][CS_TS] = jdiv(totalsp[g], 4);
                  r->sp[g][CS_END] = jdiv((imul(totalsp[g], 3)), 4);
                } else {
                  if (r->stage == 11) {
                    double startgrip2 = gripreset[r->sc[g]] + ((((r->level[g] + statadjust[2]) - 1.0)) / 5.0);
                    double whatneed = 24.2 + (((i32((imul(r->level[(r->nplayers - 1)], 3)) - 1))) * 0.2);
                    if (whatneed > 45.6) {
                      whatneed = 45.6;
                    }
                    double needgrip2 = xround(whatneed - startgrip2, 1);
                    int32_t gripstats2 = jtrunc_d((needgrip2 * 5.0));
                    if (needgrip2 < 0.0) {
                      gripstats2 = 0;
                    }
                    if (totalsp[g] < gripstats2) {
                      r->sp[g][CS_GRIP] = totalsp[g];
                    } else {
                      int32_t extrabeast = i32(career_beastspcalc(r->level[g]) - career_spcalc(r->level[g]));
                      r->sp[g][CS_GRIP] = gripstats2;
                      r->sp[g][CS_TS] = i32((i32(career_spcalc(r->level[g]) - gripstats2)) + (jdiv(extrabeast, 8)));
                      bool userracer = false;
                      if (r->sp[0][CS_TS] > r->sp[0][CS_STR]) {
                        userracer = true;
                      }
                      if (userracer) {
                        r->sp[g][CS_END] = jdiv(extrabeast, 2);
                      } else {
                        r->sp[g][CS_END] = jdiv((imul(extrabeast, 7)), 8);
                      }
                      r->sp[g][CS_STR] = i32((i32((i32(totalsp[g] - r->sp[g][CS_GRIP])) - r->sp[g][CS_TS])) - r->sp[g][CS_END]);
                    }
                  }
                  if ((r->bonus == 3)) {
                    r->sp[g][CS_TS] = i32(158 - r->level[g]);
                    r->sp[g][CS_END] = i32(totalsp[g] - r->sp[g][CS_TS]);
                  }
                }
              } else if ((r->stage != 13) && (r->stage != 15)) {
                if (!(r->bonus == 2)) {
                  if (r->stage == 11) {
                    double startgrip2 = gripreset[r->sc[g]] + ((((r->level[g] + statadjust[2]) - 1.0)) / 5.0);
                    double whatneed = 24.2 + (((i32((imul(r->level[(r->nplayers - 1)], 3)) - 1))) * 0.2);
                    if (whatneed > 45.6) {
                      whatneed = 45.6;
                    }
                    double needgrip2 = xround(whatneed - startgrip2, 1);
                    int32_t gripstats2 = jtrunc_d((needgrip2 * 5.0));
                    if (needgrip2 < 0.0) {
                      gripstats2 = 0;
                    }
                    if (totalsp[g] < gripstats2) {
                      r->sp[g][CS_GRIP] = totalsp[g];
                    } else {
                      r->sp[g][CS_GRIP] = gripstats2;
                      int32_t remaining2 = i32(totalsp[g] - gripstats2);
                      r->sp[g][CS_TS] = remaining2;
                    }
                  } else {
                    r->sp[g][CS_TS] = jdiv(totalsp[g], 2);
                    r->sp[g][CS_END] = jdiv(totalsp[g], 2);
                  }
                } else {
                  r->sp[g][CS_TS] = i32(88 - r->level[g]);
                  r->sp[g][CS_END] = i32(totalsp[g] - r->sp[g][CS_TS]);
                }
              } else {
                r->sp[g][CS_TS] = jdiv(totalsp[g], 4);
                r->sp[g][CS_END] = jdiv((imul(totalsp[g], 3)), 4);
              }
            } else {
              if (r->stage == 13) {
                r->sp[g][CS_TS] = jdiv(totalsp[g], 8);
                r->sp[g][CS_END] = i32(totalsp[g] - r->sp[g][CS_TS]);
              }
              if (r->stage == 14) {
                int32_t stupoints = 10;
                if (!r->scalelevels) {
                  stupoints = 0;
                }
                r->sp[g][CS_TS] = i32(totalsp[g] - stupoints);
                r->sp[g][CS_STU] = stupoints;
              } else {
                if (r->stage == 18) {
                  int32_t speedsp = 149;
                  int32_t shadboost = 0;
                  if (r->shadow[g]) {
                    shadboost = 50;
                    speedsp = 109;
                  }
                  r->sp[g][CS_TS] = speedsp;
                  int32_t alreadyhave2 = i32((i32((i32(r->level[g] - 1)) + statadjust[2])) + shadboost);
                  r->sp[g][CS_GRIP] = i32(125 - alreadyhave2);
                  r->sp[g][CS_STU] = i32(90 - r->sp[g][CS_GRIP]);
                  r->sp[g][CS_END] = i32((i32(totalsp[g] - speedsp)) - 90);
                }
                if (r->stage == 16) {
                  int32_t shadboost2 = 0;
                  if (r->shadow[g]) {
                    shadboost2 = 50;
                  }
                  int32_t alreadyhave3 = i32((i32((i32(r->level[g] - 1)) + statadjust[2])) + shadboost2);
                  r->sp[g][CS_GRIP] = i32(110 - alreadyhave3);
                  int32_t remaining4 = i32(totalsp[g] - r->sp[g][CS_GRIP]);
                  r->sp[g][CS_TS] = jdiv(remaining4, 4);
                  r->sp[g][CS_END] = i32((i32(totalsp[g] - r->sp[g][CS_GRIP])) - r->sp[g][CS_TS]);
                }
              }
            }
          }
          if ((id(r->sc[g]) == 13) || (id(r->sc[g]) == 36)) {
            if ((!(r->bonus == 2) && !(r->bonus == 3)) && !(r->bonus == 4)) {
              if (((r->stage != 16) && (r->stage != 17)) && (r->stage != 18)) {
                if ((((((r->stage != 13) && (r->stage != 15)) && (r->stage != 21)) && (r->stage != 19)) && (r->stage != 22)) && (r->stage != 23)) {
                  r->sp[g][CS_END] = jdiv(totalsp[g], 2);
                  r->sp[g][CS_STR] = jdiv(totalsp[g], 2);
                } else {
                  if (r->stage == 21) {
                    if (r->beast[g] && !r->scalelevels) {
                      r->sp[g][CS_TS] = i32(76 - ((i32((i32(r->level[g] + statadjust[0])) - 66))));
                      r->sp[g][CS_END] = jdiv(totalsp[g], 4);
                      r->sp[g][CS_STR] = i32((i32(totalsp[g] - r->sp[g][CS_END])) - r->sp[g][CS_TS]);
                    } else {
                      r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 3)), 16);
                      r->sp[g][CS_END] = jdiv(totalsp[g], 4);
                      r->sp[g][CS_STR] = i32((i32(totalsp[g] - r->sp[g][CS_END])) - r->sp[g][CS_TS]);
                    }
                  }
                  if (r->stage == 22) {
                    if (r->beast[g] && !r->scalelevels) {
                      r->sp[g][CS_TS] = i32(76 - ((i32((i32(r->level[g] + statadjust[0])) - 66))));
                      r->sp[g][CS_END] = jdiv(totalsp[g], 4);
                      r->sp[g][CS_STR] = i32((i32(totalsp[g] - r->sp[g][CS_END])) - r->sp[g][CS_TS]);
                    } else {
                      r->sp[g][CS_END] = jdiv(totalsp[g], 3);
                      r->sp[g][CS_STR] = i32(totalsp[g] - r->sp[g][CS_END]);
                    }
                  }
                  if (r->stage == 23) {
                    if (r->shadow[g]) {
                      r->sp[g][CS_TS] = jdiv(totalsp[g], 6);
                      r->sp[g][CS_STR] = i32(totalsp[g] - r->sp[g][CS_TS]);
                    } else {
                      r->sp[g][CS_END] = jdiv(totalsp[g], 3);
                      r->sp[g][CS_STR] = i32(totalsp[g] - r->sp[g][CS_END]);
                    }
                  }
                  if (r->stage == 19) {
                    if (!r->shadow[g]) {
                      r->sp[g][CS_TS] = jdiv(totalsp[g], 8);
                      r->sp[g][CS_END] = jdiv((imul(totalsp[g], 5)), 16);
                      r->sp[g][CS_STR] = i32((i32(totalsp[g] - r->sp[g][CS_END])) - r->sp[g][CS_TS]);
                    } else {
                      r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 3)), 16);
                      r->sp[g][CS_GRIP] = jdiv((imul(totalsp[g], 3)), 32);
                      r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 19)), 32);
                      r->sp[g][CS_END] = i32((i32((i32(totalsp[g] - r->sp[g][CS_GRIP])) - r->sp[g][CS_STR])) - r->sp[g][CS_TS]);
                    }
                  }
                  if ((r->stage == 13) || (r->stage == 15)) {
                    if (!r->beast[g]) {
                      r->sp[g][CS_STR] = jdiv(totalsp[g], 2);
                      r->sp[g][CS_TS] = jdiv(totalsp[g], 6);
                      if ((((s->unlocked == r->stage) || r->hardstage)) && (r->stage == 13)) {
                        r->sp[g][CS_TS] = i32((i32(320 - nitroswits_tab[r->sc[g]][2])) - ((i32((i32(r->level[g] + statadjust[0])) - 1))));
                      }
                      r->sp[g][CS_END] = i32((i32(totalsp[g] - r->sp[g][CS_STR])) - r->sp[g][CS_TS]);
                    } else {
                      r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 3)), 16);
                      r->sp[g][CS_END] = jdiv(totalsp[g], 4);
                      r->sp[g][CS_STR] = i32((i32(totalsp[g] - r->sp[g][CS_END])) - r->sp[g][CS_TS]);
                    }
                  }
                }
              } else if ((g >= 4) || (r->stage != 17)) {
                if (!r->beast[g]) {
                  if (!r->shadow[g] || r->scalelevels) {
                    r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 3)), 4);
                    r->sp[g][CS_END] = i32(totalsp[g] - r->sp[g][CS_STR]);
                  } else {
                    if (r->stage == 18) {
                      int32_t alreadyhave = i32((i32(r->level[g] + statadjust[2])) + 49);
                      r->sp[g][CS_GRIP] = i32(131 - alreadyhave);
                      r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 3)), 4);
                      r->sp[g][CS_TS] = i32((i32(totalsp[g] - r->sp[g][CS_STR])) - r->sp[g][CS_GRIP]);
                    }
                    if (r->stage == 16) {
                      int32_t alreadyhave = i32((i32(r->level[g] + statadjust[2])) + 49);
                      r->sp[g][CS_GRIP] = i32(116 - alreadyhave);
                      r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 3)), 4);
                      r->sp[g][CS_TS] = i32((i32(totalsp[g] - r->sp[g][CS_STR])) - r->sp[g][CS_GRIP]);
                    }
                    if (r->stage == 17) {
                      r->sp[g][CS_END] = jdiv((imul(totalsp[g], 3)), 4);
                      r->sp[g][CS_TS] = i32(totalsp[g] - r->sp[g][CS_END]);
                    }
                  }
                } else if ((r->stage != 18) || r->scalelevels) {
                  r->sp[g][CS_STR] = jdiv(totalsp[g], 2);
                  r->sp[g][CS_END] = i32(totalsp[g] - r->sp[g][CS_STR]);
                } else {
                  int32_t alreadyhave = i32((i32(r->level[g] - 1)) + statadjust[2]);
                  r->sp[g][CS_GRIP] = i32(116 - alreadyhave);
                  r->sp[g][CS_STR] = jdiv(totalsp[g], 2);
                  r->sp[g][CS_END] = i32((i32(totalsp[g] - r->sp[g][CS_STR])) - r->sp[g][CS_GRIP]);
                }
              } else if (!r->scalelevels) {
                r->sp[g][CS_TS] = 105;
                r->sp[g][CS_STR] = 195;
                r->sp[g][CS_ACC] = i32(totalsp[g] - ((i32(r->sp[g][CS_TS] + r->sp[g][CS_STR]))));
              } else {
                double spdportion = 105.0 / career_beastspcalc(55);
                double strportion = 195.0 / career_beastspcalc(55);
                r->sp[g][CS_TS] = jtrunc_d((totalsp[g] * spdportion));
                r->sp[g][CS_STR] = jtrunc_d((totalsp[g] * strportion));
                r->sp[g][CS_ACC] = i32((i32(totalsp[g] - r->sp[g][CS_TS])) - r->sp[g][CS_STR]);
              }
            } else if (!(r->bonus == 3) && !(r->bonus == 4)) {
              if ((g == 1) || (g == 2)) {
                r->sp[g][CS_END] = jdiv(totalsp[g], 2);
                r->sp[g][CS_STR] = i32(totalsp[g] - r->sp[g][CS_END]);
              } else {
                r->sp[g][CS_END] = jdiv(totalsp[g], 2);
                r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 3)), 8);
                r->sp[g][CS_TS] = i32((i32(totalsp[g] - r->sp[g][CS_END])) - r->sp[g][CS_STR]);
              }
            } else if ((r->bonus == 3)) {
              r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 9)), 16);
              r->sp[g][CS_END] = jdiv(totalsp[g], 4);
              r->sp[g][CS_TS] = i32(totalsp[g] - ((i32(r->sp[g][CS_STR] + r->sp[g][CS_END]))));
            } else {
              r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 3)), 16);
              r->sp[g][CS_STR] = i32(totalsp[g] - r->sp[g][CS_TS]);
            }
          }
          if ((id(r->sc[g]) == 14) || (id(r->sc[g]) == 37)) {
            if ((r->stage != 21) && (r->stage != 22)) {
              if (((!r->beast[g] || (r->stage == 16)) || (r->stage == 17)) || (r->stage == 18)) {
                if ((r->stage < 15) && (r->stage > 19)) {
                  r->sp[g][CS_TS] = jdiv(totalsp[g], 4);
                  r->sp[g][CS_END] = i32(totalsp[g] - ((i32(r->sp[g][CS_TS] + r->sp[g][CS_ACC]))));
                } else if ((r->stage != 17) && (r->stage != 19)) {
                  if ((r->stage != 18) || r->scalelevels) {
                    if ((r->stage != 16) || r->scalelevels) {
                      r->sp[g][CS_TS] = jdiv(totalsp[g], 4);
                      r->sp[g][CS_END] = jdiv((imul(totalsp[g], 3)), 4);
                    } else if (r->beast[g] || r->shadow[g]) {
                      r->sp[g][CS_TS] = jdiv(totalsp[g], 8);
                      r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 3)), 8);
                      int32_t shadboost2 = 0;
                      if (r->shadow[g]) {
                        shadboost2 = 50;
                      }
                      int32_t alreadyhave3 = i32((i32((i32(r->level[g] - 1)) + statadjust[2])) + shadboost2);
                      r->sp[g][CS_GRIP] = i32(120 - alreadyhave3);
                      r->sp[g][CS_END] = i32(totalsp[g] - ((i32((i32(r->sp[g][CS_TS] + r->sp[g][CS_STR])) + r->sp[g][CS_GRIP]))));
                    } else {
                      r->sp[g][CS_TS] = jdiv(totalsp[g], 4);
                      int32_t alreadyhave = i32((i32(r->level[g] - 1)) + statadjust[2]);
                      r->sp[g][CS_GRIP] = i32(120 - alreadyhave);
                      r->sp[g][CS_END] = i32((i32(totalsp[g] - r->sp[g][CS_GRIP])) - r->sp[g][CS_TS]);
                    }
                  } else if (!r->beast[g] && !r->shadow[g]) {
                    r->sp[g][CS_TS] = jdiv(totalsp[g], 2);
                    r->sp[g][CS_END] = i32(totalsp[g] - r->sp[g][CS_TS]);
                  } else {
                    r->sp[g][CS_TS] = jdiv(totalsp[g], 8);
                    r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 3)), 8);
                    int32_t shadboost2 = 0;
                    if (r->shadow[g]) {
                      shadboost2 = 50;
                    }
                    int32_t alreadyhave3 = i32((i32((i32(r->level[g] - 1)) + shadboost2)) + statadjust[2]);
                    r->sp[g][CS_GRIP] = i32(135 - alreadyhave3);
                    r->sp[g][CS_END] = i32(totalsp[g] - ((i32((i32(r->sp[g][CS_TS] + r->sp[g][CS_STR])) + r->sp[g][CS_GRIP]))));
                  }
                } else if (r->shadow[g]) {
                  if (r->stage == 17) {
                    r->sp[g][CS_TS] = jdiv(totalsp[g], 4);
                    r->sp[g][CS_END] = i32(totalsp[g] - r->sp[g][CS_TS]);
                  }
                  if (r->stage == 19) {
                    r->sp[g][CS_TS] = jdiv(totalsp[g], 8);
                    r->sp[g][CS_STR] = i32(totalsp[g] - r->sp[g][CS_TS]);
                  }
                } else if (!r->beast[g]) {
                  r->sp[g][CS_STR] = jdiv(totalsp[g], 2);
                  r->sp[g][CS_END] = jdiv((imul(totalsp[g], 3)), 16);
                  r->sp[g][CS_TS] = i32(totalsp[g] - ((i32(r->sp[g][CS_STR] + r->sp[g][CS_END]))));
                } else {
                  r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 3)), 16);
                  r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 7)), 16);
                  r->sp[g][CS_END] = i32(totalsp[g] - ((i32(r->sp[g][CS_TS] + r->sp[g][CS_STR]))));
                }
              } else if (id(r->sc[g]) == 37) {
                r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 3)), 32);
                r->sp[g][CS_STR] = jdiv(totalsp[g], 4);
                r->sp[g][CS_END] = i32(totalsp[g] - ((i32(r->sp[g][CS_TS] + r->sp[g][CS_STR]))));
              } else if (r->stage != 19) {
                r->sp[g][CS_TS] = jdiv(totalsp[g], 8);
                r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 3)), 8);
                r->sp[g][CS_END] = i32(totalsp[g] - ((i32(r->sp[g][CS_TS] + r->sp[g][CS_STR]))));
              } else {
                r->sp[g][CS_END] = jdiv((imul(totalsp[g], 3)), 4);
                r->sp[g][CS_TS] = jdiv(totalsp[g], 4);
              }
            } else {
              if ((r->stage == 21) && !r->scalelevels) {
                r->sp[g][CS_TS] = i32(66 - ((i32((i32(r->level[g] + statadjust[0])) - 66))));
                r->sp[g][CS_END] = i32(totalsp[g] - r->sp[g][CS_TS]);
              }
              if ((r->stage == 22) || (((r->stage == 21) && r->scalelevels))) {
                if (!r->beast[g]) {
                  r->sp[g][CS_STR] = jdiv(totalsp[g], 2);
                  r->sp[g][CS_END] = jdiv((imul(totalsp[g], 5)), 16);
                  r->sp[g][CS_TS] = i32(totalsp[g] - ((i32(r->sp[g][CS_STR] + r->sp[g][CS_END]))));
                } else {
                  r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 3)), 16);
                  r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 9)), 16);
                  r->sp[g][CS_END] = i32(totalsp[g] - ((i32(r->sp[g][CS_TS] + r->sp[g][CS_STR]))));
                }
              }
            }
          }
          if ((id(r->sc[g]) == 15) || (id(r->sc[g]) == 38)) {
            if (!(r->bonus != 0)) {
              if ((r->stage < 17) || r->scalelevels) {
                r->sp[g][CS_STR] = jdiv(totalsp[g], 2);
                r->sp[g][CS_END] = jdiv(totalsp[g], 4);
                r->sp[g][CS_TS] = jdiv(totalsp[g], 4);
              } else if (r->shadow[g] && (r->stage == 17)) {
                r->sp[g][CS_TS] = jdiv(totalsp[g], 4);
                r->sp[g][CS_END] = jdiv((imul(totalsp[g], 3)), 4);
              } else if (!r->beast[g]) {
                if (r->stage == 21) {
                  r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 19)), 32);
                  r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 5)), 32);
                  r->sp[g][CS_END] = i32((i32(totalsp[g] - r->sp[g][CS_STR])) - r->sp[g][CS_TS]);
                }
                bool toostrong = false;
                float userstrength2 = (float)(momentreset_tab[r->sc[0]] + ((float)((float)(r->sp[0][CS_STR]) * 0.02500000037252903)));
                if ((((userstrength2 > 13.75) && (((s->unlocked == r->stage) || r->hardstage))) && !r->shadow[g]) && (((id(r->sc[0]) == 18) || (id(r->sc[0]) == 22)))) {
                  toostrong = true;
                }
                if (r->stage == 22) {
                  r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 23)), 32);
                  r->sp[g][CS_TS] = jdiv(totalsp[g], 8);
                  r->sp[g][CS_END] = i32((i32(totalsp[g] - r->sp[g][CS_TS])) - r->sp[g][CS_STR]);
                }
                if (r->stage == 23) {
                  int32_t endpoints = jdiv((imul(totalsp[g], 5)), 24);
                  if (r->shadow[g]) {
                    endpoints = 0;
                  }
                  r->sp[g][CS_END] = endpoints;
                  r->sp[g][CS_TS] = jdiv(totalsp[g], 4);
                  r->sp[g][CS_STR] = i32((i32(totalsp[g] - r->sp[g][CS_END])) - r->sp[g][CS_TS]);
                }
                if (r->stage == 24) {
                  if (toostrong) {
                    r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 3)), 16);
                    r->sp[g][CS_GRIP] = 100;
                    r->sp[g][CS_END] = i32((i32(totalsp[g] - r->sp[g][CS_TS])) - 100);
                  } else if (!r->shadow[g]) {
                    r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 11)), 16);
                    r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 3)), 16);
                    r->sp[g][CS_END] = i32((i32(totalsp[g] - r->sp[g][CS_TS])) - r->sp[g][CS_STR]);
                  } else {
                    int32_t alreadyhave2 = i32((i32(r->level[g] + 49)) + statadjust[2]);
                    r->sp[g][CS_GRIP] = i32(203 - alreadyhave2);
                    r->sp[g][CS_TS] = jdiv(totalsp[g], 8);
                    r->sp[g][CS_STR] = i32((i32(totalsp[g] - r->sp[g][CS_GRIP])) - r->sp[g][CS_TS]);
                  }
                }
                if (((r->stage == 17) || (r->stage == 19)) || (((r->stage == 18) && !r->shadow[g]))) {
                  int32_t gripboost = 0;
                  if ((r->stage == 19) && r->shadow[g]) {
                    gripboost = jdiv(totalsp[g], 32);
                  }
                  r->sp[g][CS_TS] = i32((jdiv(totalsp[g], 4)) - (imul(gripboost, 2)));
                  r->sp[g][CS_STR] = i32((jdiv(totalsp[g], 2)) + (imul(gripboost, 3)));
                  r->sp[g][CS_GRIP] = imul(gripboost, 3);
                  r->sp[g][CS_END] = i32(totalsp[g] - ((i32((i32(r->sp[g][CS_TS] + r->sp[g][CS_STR])) + r->sp[g][CS_GRIP]))));
                }
                if ((r->stage == 18) && r->shadow[g]) {
                  r->sp[g][CS_TS] = jdiv(totalsp[g], 4);
                  r->sp[g][CS_STR] = jdiv(totalsp[g], 2);
                  int32_t alreadyhave2 = i32((i32(r->level[g] + 49)) + statadjust[2]);
                  r->sp[g][CS_GRIP] = i32(131 - alreadyhave2);
                  r->sp[g][CS_END] = i32(totalsp[g] - ((i32((i32(r->sp[g][CS_TS] + r->sp[g][CS_STR])) + r->sp[g][CS_GRIP]))));
                }
              } else {
                if (r->stage == 22) {
                  r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 3)), 16);
                  r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 9)), 16);
                  r->sp[g][CS_END] = i32(totalsp[g] - ((i32(r->sp[g][CS_TS] + r->sp[g][CS_STR]))));
                }
                if (r->stage == 19) {
                  r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 3)), 16);
                  r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 5)), 8);
                  r->sp[g][CS_END] = i32(totalsp[g] - ((i32(r->sp[g][CS_TS] + r->sp[g][CS_STR]))));
                }
                if (((r->stage == 17) || (r->stage == 18)) || (r->stage == 21)) {
                  r->sp[g][CS_TS] = jdiv(totalsp[g], 8);
                  r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 7)), 16);
                  r->sp[g][CS_END] = i32(totalsp[g] - ((i32(r->sp[g][CS_TS] + r->sp[g][CS_STR]))));
                }
              }
            } else if (!(r->bonus == 4)) {
              r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 3)), 16);
              r->sp[g][CS_STR] = jdiv(totalsp[g], 2);
              r->sp[g][CS_END] = i32(totalsp[g] - ((i32(r->sp[g][CS_STR] + r->sp[g][CS_TS]))));
            } else {
              r->sp[g][CS_TS] = jdiv(totalsp[g], 4);
              r->sp[g][CS_STR] = i32(totalsp[g] - r->sp[g][CS_TS]);
            }
          }
          if (id(r->sc[g]) == 16) {
            if (!r->beast[g]) {
              if ((r->stage < 21) || r->scalelevels) {
                int32_t gripboost2 = 0;
                if (((r->stage == 19) && r->shadow[g]) && !r->scalelevels) {
                  gripboost2 = jdiv(totalsp[g], 32);
                }
                r->sp[g][CS_STR] = i32((jdiv((imul(totalsp[g], 17)), 32)) + (imul(gripboost2, 2)));
                r->sp[g][CS_TS] = i32((jdiv(totalsp[g], 4)) - (imul(gripboost2, 2)));
                r->sp[g][CS_GRIP] = imul(gripboost2, 3);
                r->sp[g][CS_END] = i32(totalsp[g] - ((i32((i32(r->sp[g][CS_TS] + r->sp[g][CS_STR])) + r->sp[g][CS_GRIP]))));
              } else {
                if (r->stage == 21) {
                  r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 19)), 32);
                  r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 7)), 32);
                  r->sp[g][CS_END] = i32((i32(totalsp[g] - r->sp[g][CS_TS])) - r->sp[g][CS_STR]);
                }
                bool toostrong = false;
                float userstrength2 = (float)(momentreset_tab[r->sc[0]] + ((float)((float)(r->sp[0][CS_STR]) * 0.02500000037252903)));
                if ((((userstrength2 > 13.75) && (((s->unlocked == r->stage) || r->hardstage))) && !r->shadow[g]) && (((id(r->sc[0]) == 18) || (id(r->sc[0]) == 22)))) {
                  toostrong = true;
                }
                if ((r->stage == 22) || (r->stage == 23)) {
                  int32_t endpoints = jdiv(totalsp[g], 8);
                  if (r->shadow[g]) {
                    endpoints = 0;
                  }
                  r->sp[g][CS_END] = endpoints;
                  r->sp[g][CS_TS] = jdiv(totalsp[g], 4);
                  r->sp[g][CS_STR] = i32((i32(totalsp[g] - r->sp[g][CS_TS])) - r->sp[g][CS_END]);
                }
                if (r->stage == 24) {
                  if (toostrong) {
                    r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 3)), 16);
                    r->sp[g][CS_GRIP] = 100;
                    r->sp[g][CS_END] = i32((i32(totalsp[g] - r->sp[g][CS_TS])) - 100);
                  } else {
                    int32_t endpoints = jdiv(totalsp[g], 8);
                    int32_t spdpoints = 0;
                    int32_t grippoints = 0;
                    if (r->shadow[g]) {
                      endpoints = 0;
                      spdpoints = 40;
                      int32_t alreadyhave4 = i32((i32(r->level[g] + 49)) + statadjust[2]);
                      grippoints = i32(207 - alreadyhave4);
                    }
                    r->sp[g][CS_GRIP] = grippoints;
                    r->sp[g][CS_END] = endpoints;
                    r->sp[g][CS_TS] = i32((jdiv(totalsp[g], 8)) - spdpoints);
                    r->sp[g][CS_STR] = i32((i32((i32(totalsp[g] - r->sp[g][CS_TS])) - endpoints)) - grippoints);
                  }
                }
              }
            } else {
              if ((r->stage == 21) || (r->stage == 22)) {
                r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 23)), 32);
                r->sp[g][CS_END] = jdiv((imul(totalsp[g], 3)), 16);
                r->sp[g][CS_TS] = i32((i32(totalsp[g] - r->sp[g][CS_STR])) - r->sp[g][CS_END]);
              }
              if (r->stage == 19) {
                r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 3)), 16);
                r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 5)), 8);
                r->sp[g][CS_END] = i32(totalsp[g] - ((i32(r->sp[g][CS_TS] + r->sp[g][CS_STR]))));
              }
              if (r->stage < 19) {
                r->sp[g][CS_STR] = jdiv(totalsp[g], 2);
                r->sp[g][CS_END] = jdiv(totalsp[g], 4);
                r->sp[g][CS_TS] = jdiv(totalsp[g], 4);
              }
            }
          }
          if (id(r->sc[g]) == 17) {
            if ((r->stage < 21) || r->scalelevels) {
              r->sp[g][CS_END] = jdiv((imul(totalsp[g], 3)), 4);
              r->sp[g][CS_TS] = i32(totalsp[g] - r->sp[g][CS_END]);
            } else {
              if (r->stage == 24) {
                int32_t less = 0;
                int32_t alreadyhave3 = 0;
                if (r->shadow[g]) {
                  less = 40;
                  alreadyhave3 = i32((i32((i32(r->level[g] - 1)) + 50)) + statadjust[2]);
                } else {
                  alreadyhave3 = 101;
                }
                r->sp[g][CS_TS] = i32((i32(110 - ((i32((i32(r->level[g] + statadjust[0])) - 81))))) - less);
                r->sp[g][CS_GRIP] = i32(201 - alreadyhave3);
                r->sp[g][CS_END] = i32((i32(totalsp[g] - r->sp[g][CS_TS])) - r->sp[g][CS_GRIP]);
              }
              if (r->stage == 21) {
                r->sp[g][CS_TS] = i32(145 - ((i32((i32(r->level[g] + statadjust[0])) - 66))));
                r->sp[g][CS_END] = i32(totalsp[g] - r->sp[g][CS_TS]);
              }
              if (r->stage == 23) {
                int32_t comp = 0;
                int32_t accpoints = 0;
                if (r->shadow[g]) {
                  comp = 24;
                  accpoints = 50;
                }
                r->sp[g][CS_TS] = i32((i32(190 - ((i32((i32(r->level[g] + statadjust[0])) - 75))))) - comp);
                r->sp[g][CS_ACC] = accpoints;
                r->sp[g][CS_END] = i32((i32((i32(totalsp[g] - r->sp[g][CS_TS])) + comp)) - accpoints);
              }
              if (r->stage == 22) {
                r->sp[g][CS_TS] = 78;
                r->sp[g][CS_END] = jdiv((imul(totalsp[g], 3)), 16);
                r->sp[g][CS_STR] = i32((i32(totalsp[g] - r->sp[g][CS_END])) - 78);
              }
            }
          }
          if (id(r->sc[g]) == 18) {
            if ((r->stage < 24) || r->scalelevels) {
              if ((r->stage != 23) || r->scalelevels) {
                if (!r->beast[g]) {
                  r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 11)), 16);
                  r->sp[g][CS_TS] = jdiv(totalsp[g], 8);
                  r->sp[g][CS_END] = i32((i32(totalsp[g] - r->sp[g][CS_TS])) - r->sp[g][CS_STR]);
                } else {
                  r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 11)), 16);
                  r->sp[g][CS_TS] = jdiv(totalsp[g], 8);
                  r->sp[g][CS_END] = i32((i32(totalsp[g] - r->sp[g][CS_TS])) - r->sp[g][CS_STR]);
                }
              } else if (g == 1) {
                r->sp[g][CS_TS] = 191;
                r->sp[g][CS_STR] = i32(totalsp[g] - 191);
              }
            } else if (r->stage == 24) {
              if (!r->shadow[g]) {
                r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 3)), 16);
                r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 11)), 16);
                r->sp[g][CS_END] = i32((i32(totalsp[g] - r->sp[g][CS_TS])) - r->sp[g][CS_STR]);
              } else {
                r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 5)), 32);
                int32_t alreadyhave = i32((i32((i32(r->level[g] - 1)) + statadjust[2])) + 50);
                r->sp[g][CS_GRIP] = i32(209 - alreadyhave);
                r->sp[g][CS_STR] = i32((i32(totalsp[g] - r->sp[g][CS_TS])) - r->sp[g][CS_GRIP]);
              }
            }
          }
          if (id(r->sc[g]) == 19) {
            r->sp[g][CS_END] = jdiv(totalsp[g], 4);
            r->sp[g][CS_TS] = jdiv(totalsp[g], 8);
            r->sp[g][CS_STR] = i32((i32(totalsp[g] - r->sp[g][CS_END])) - r->sp[g][CS_TS]);
          }
          if (id(r->sc[g]) == 20) {
            r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 9)), 32);
            r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 5)), 8);
            r->sp[g][CS_END] = i32((i32(totalsp[g] - r->sp[g][CS_TS])) - r->sp[g][CS_STR]);
          }
          if (id(r->sc[g]) == 21) {
            r->sp[g][CS_END] = jdiv(totalsp[g], 2);
            r->sp[g][CS_GRIP] = jdiv(totalsp[g], 4);
            r->sp[g][CS_TS] = jdiv(totalsp[g], 4);
          }
          if (id(r->sc[g]) == 22) {
            r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 3)), 16);
            r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 11)), 16);
            r->sp[g][CS_END] = i32((i32(totalsp[g] - r->sp[g][CS_TS])) - r->sp[g][CS_STR]);
          }
          if (r->stage == 13) {
            bool afuckingracer = false;
            int32_t totalpoints = i32(career_spcalc(r->level[0]) + s->extpoints[r->sc[0]]);
            int32_t instrength = jdiv(totalpoints, 4);
            float userstrength3 = (float)(momentreset_tab[r->sc[0]] + ((float)((float)(r->sp[0][CS_STR]) * 0.02500000037252903)));
            float strengthneed = (float)(5.0 + ((float)((float)(((i32(r->level[(r->nplayers - 1)] - 42)))) * 0.07500000298023224)));
            if (((i32((i32(r->sp[0][CS_STR] - r->level[0])) + 1)) < instrength) || (userstrength3 < strengthneed)) {
              afuckingracer = true;
            }
            if ((((((g == 2) || (g == 3)) || (g == 5)) || (g == 6)) || (g == 8)) || (g == 9)) {
              if (!afuckingracer) {
                r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 9)), 64);
                r->sp[g][CS_STR] = jdiv(totalsp[g], 2);
                r->sp[g][CS_END] = i32(totalsp[g] - ((i32(r->sp[g][CS_TS] + r->sp[g][CS_STR]))));
              } else {
                r->sp[g][CS_TS] = jdiv((imul(totalsp[g], 7)), 32);
                r->sp[g][CS_STR] = jdiv((imul(totalsp[g], 7)), 16);
                r->sp[g][CS_GRIP] = jdiv(totalsp[g], 8);
                r->sp[g][CS_END] = i32(totalsp[g] - ((i32(r->sp[g][CS_TS] + r->sp[g][CS_STR]))));
              }
            }
          }
        }
        int32_t minlvl = r->level[g];
        if (minlvl < 6) {
          minlvl = 6;
        }
        if (r->shadow[g]) {
          r->sp[g][CS_END] = i32(r->sp[g][CS_END] + imul(minlvl, 4));
          r->sp[g][CS_ACC] = i32(r->sp[g][CS_ACC] + 50);
          r->sp[g][CS_GRIP] = i32(r->sp[g][CS_GRIP] + 50);
          r->sp[g][CS_TS] = i32(r->sp[g][CS_TS] + 40);
        }
        r->sp[g][CS_TS] = i32(r->sp[g][CS_TS] + (i32(i32(r->level[g] - 1) + statadjust[0])));
        r->sp[g][CS_ACC] = i32(r->sp[g][CS_ACC] + (i32(i32(r->level[g] - 1) + statadjust[1])));
        r->sp[g][CS_GRIP] = i32(r->sp[g][CS_GRIP] + (i32(i32(r->level[g] - 1) + statadjust[2])));
        r->sp[g][CS_STU] = i32(r->sp[g][CS_STU] + (i32(i32(r->level[g] - 1) + statadjust[3])));
        r->sp[g][CS_STR] = i32(r->sp[g][CS_STR] + (i32(i32(r->level[g] - 1) + statadjust[4])));
        r->sp[g][CS_END] = i32(r->sp[g][CS_END] + (i32(i32(r->level[g] - 1) + statadjust[5])));
      }
      // this.totallevel = sumAll(opponents' levels)
      int32_t totallevel = 0;
      for (int32_t a3 = 1; a3 < r->nplayers; a3++) {
        totallevel = i32(totallevel + r->level[a3]);
      }
      r->averagelevel = jdiv(totallevel, (r->nplayers - 1));
      r->softlevelcap = i32(r->level[(r->nplayers - 1)] + 5);
      if ((r->bonus != 0)) {
        int32_t bsboost = 5;
        if ((r->bonus == 1)) {
          bsboost = 9;
        }
        r->softlevelcap = imin(i32(r->level[(r->nplayers - 1)] + bsboost), i32(maxlevel_at(i32(s->unlocked - 2)) + 5));
      }
  }
}
