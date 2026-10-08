// ports web/ext/xtGraphics.js randomno (career branch, 18144-18192),
// sortcars (career + bonus branches, 12546-13161), beasts (9391-9607) and
// sortshadows (9616-9673), driven in the pass structure of
// GameSparker.js loadstage (1239-1258), plus the lineup-time undead[] that
// careermode$m sets (7764-7781, 8291/8376-8387, 8399/8421-8423, 8967/9007-9010).
//
// The original does not pick a career lineup in one call. Fase 6476 runs
// resetbeasts/resetshadows/randomno (GameSparker.js:1803-1813), then fase 2
// calls loadstage every frame, and every loadstage runs resetstat ->
// sortcars (re-rolling the whole field) -> beasts, which is a rejection
// loop spread across frames: one call rolls beastcar[]/shadow[], the next
// call checks them against the new field and either accepts (ssdone,
// alldone) or rolls again. A load that STARTS with alldone already set is
// the last one (loadstage sets fase -69 near its end, GameSparker.js:1239,
// but still runs resetstat/sortcars/beasts once more after it). Flags that
// only resetbeasts/resetshadows clear (beastopponent, Madness.shadowcar)
// therefore accumulate over the passes, and the final sc[] comes from the
// final pass. All of that is replicated here, random draw for random draw;
// the other random() consumers inside loadstage (Medium.newclouds etc.)
// between passes are not part of this file.
#include "career.h"
#include "java_compat.h"

#include <string.h>

// xtGraphics.proba (xtGraphics.js:691), Float32Array.
static const float PROBA[CAREER_CARS] = {
  0.5f, 0.5f, 0.4f, 0.3f, 0.3f, 0.4f, 0.3f, 0.3f, 0.3f, 0.1f, 0.1f, 0.5f, 0.1f, 0.0f, 0.0f, 0.0f,
  0.0f, 0.1f, 0.1f, 0.5f, 0.85f, 0.85f, 0.0f, 0.5f, 0.5f, 0.4f, 0.3f, 0.5f, 0.4f, 0.3f, 0.3f, 0.3f,
  0.1f, 0.1f, 0.5f, 0.1f, 0.0f, 0.0f, 0.0f,
};

// xtGraphics.maxlevel (xtGraphics.js:780).
static const int32_t MAXLEVEL[CAREER_STAGES] = {
  4, 7, 10, 13, 16, 20, 23, 26, 30, 33, 36, 39, 42, 45, 49, 52, 55, 58, 62, 65, 70, 74, 77, 84,
  90, 97, 103, 111, 120, 125, 125,
};

// The game loops until the field is accepted; a field that can never be
// accepted hangs the original. This only keeps a bad input from hanging us.
#define MAX_PASSES 100000

typedef struct {
  CareerRace *r;
  const CareerSave *s;
  bool bonstage;
  bool bonusstage[4];       // xtGraphics.bonusstage[0..3] = bonus 1..4
  bool rollonce;            // xtGraphics.rollonce / chance (fase 6476 clears them)
  float chance;
  int32_t beastcar[2];      // xtGraphics.beastcar[0..1] (only these two are used)
  int32_t shadow[2];        // xtGraphics.shadow[0..1]
  bool ssdone, alldone, sortedcars;
} Lineup;

static double rnd(void) { return career_random(); }

// `random() > random()`: JS evaluates left to right; C does not, so sequence it.
static bool rgt(void) {
  double a = career_random();
  double b = career_random();
  return a > b;
}

static bool mgt(void) {
  double a = career_mrandom();
  double b = career_mrandom();
  return a > b;
}

static bool hard_or_latest(const Lineup *L, int32_t i) {
  return L->r->hardstage || L->s->unlocked == i;
}

// --- sortcars ---------------------------------------------------------------

// The career rejections of the latest-stage / hard field (xtGraphics.js:12610-12651).
static bool latest_reject(int32_t i, int32_t k, int32_t c, int32_t bestcar) {
  if (i == 4 && ((c <= 4 && c != 2) || (c >= 23 && c <= 27 && c != 25))) return true;
  if (i == 12 && (c <= 4 || c == 7 || c == 12 || c == 30 || c == 35 || (c >= 23 && c <= 27))) return true;
  if (i == 6 && ((c <= 4 && c != 2) || c == 9 || c == 32 || (c >= 23 && c <= 27 && c != 25))) return true;
  if (i == 7 && ((c <= 4 && c != 2) || c == 9 || c == 10 || c == 32 || c == 33 ||
                 (c >= 23 && c <= 27 && c != 25))) return true;
  if (i == 8 && ((c <= 5 && c != 2) || (c >= 23 && c <= 28 && c != 25))) return true;
  if (i == 11 && ((c <= 7 && c != 0) || c == 8 || c == 11 || c == 12 || (c >= 23 && c <= 30 && c != 29) ||
                  c == 31 || c == 34 || c == 35)) return true;
  if (i == 13 && k % 3 == 2 && k < 9 &&
      ((c <= 7 && c != 5) || (c >= 11 && c <= 13) || (c >= 23 && c <= 30 && c != 28) || (c >= 34 && c <= 36)))
    return true;
  if (i == 17 && c != 14 && c != 13 && c != 11 && c != 15 && c != 37 && c != 36 && c != 34 && c != 38) return true;
  if (i == 19 && c != 15 && c != 16 && c != 38) return true;
  if (i == 23 && (c <= 12 || c == 14 || (c >= 23 && c <= 35) || c == 37)) return true;
  if (i == 24 && (c <= 14 || c == 17 || (c >= 23 && c <= 37))) return true;
  if (c == bestcar || (bestcar == 18 && (c == 21 || c == 19)) || (bestcar == 19 && c == 21 && i < 25)) return true;
  if (c == 21) return true;
  return false;
}

// The latest-stage (or hard) field: random opponents under bestcar, then the
// stage's fixed slots (xtGraphics.js:12556-12903).
static void sortcars_latest(Lineup *L, int32_t i, bool *aflag) {
  CareerRace *r = L->r;
  int32_t *sc = r->sc;
  const int32_t np = r->nplayers;
  int32_t bestcar = 7 + (i + 1) / 2;
  if (i >= 21) bestcar = 11 + i / 3;
  if (bestcar == 20 || bestcar == 21) bestcar = 22;
  sc[np - 1] = bestcar;
  int32_t k = 1;
  do {
    aflag[k] = false;
  } while (++k < np - 1);
  k = 1;
  for (;;) {
    if (aflag[k]) {
      if (++k >= np - 1) break;
      continue;
    }
    int32_t randomiser = jtrunc_d(rnd() * bestcar);
    if (bestcar == 18 || bestcar == 19) randomiser = jtrunc_d(rnd() * 22.0);
    int32_t gcboost = 0;
    if (mgt() && randomiser <= 15) gcboost = 23;
    sc[k] = gcboost + randomiser;
    aflag[k] = true;
    bool stopception = i == 16 || i == 12 || i == 6 || i == 7 || i == 8 || i == 11 || i == 13 ||
                       i == 17 || i == 19 || i == 23 || i == 24;
    // allowrepeats is false whenever careermode is (12596-12599).
    int32_t l = 0;
    do {
      if (k != l && sc[k] == sc[l] && !stopception) aflag[k] = false;
    } while (++l < np);
    (void)rnd();  // 12605: a roll whose value is thrown away
    if (latest_reject(i, k, sc[k], bestcar)) aflag[k] = false;
  }

  // 12653-12903: each stage's fixed opponents.
  const int32_t c0 = sc[0];
  switch (i) {
  case 2:
    sc[9] = c0 != 29 ? 29 : 25;
    sc[8] = c0 != 6 ? 6 : 25;
    break;
  case 3:
    if (c0 != 24) sc[np - 2] = 24; else sc[np - 2] = 26;
    if (c0 != 1) sc[np - 3] = 1; else sc[np - 2] = 26;   // sic: np-2 both times
    break;
  case 5:
    sc[5] = 3; sc[4] = 1; sc[3] = 8; sc[2] = 31; sc[1] = 24;
    break;
  case 6:
    sc[9] = 9; sc[8] = 32; sc[7] = 31; sc[6] = 29; sc[5] = 7; sc[4] = 9; sc[3] = 25; sc[2] = 5;
    break;
  case 7:
    sc[9] = 10; sc[8] = 10; sc[7] = 33; sc[6] = 32; sc[5] = 8; sc[4] = 32; sc[3] = 31; sc[2] = 9;
    break;
  case 8:
    sc[1] = mgt() ? 32 : 9;
    sc[2] = mgt() ? 33 : 10;
    sc[np - 3] = 33; sc[np - 4] = 9; sc[np - 5] = 8; sc[np - 7] = 7; sc[np - 8] = 31;
    break;
  case 9:
    sc[9] = 10; sc[8] = 33; sc[7] = 28; sc[6] = 10; sc[5] = 0; sc[4] = 29; sc[3] = 34; sc[2] = 11; sc[1] = 5;
    break;
  case 10:
    sc[5] = 3; sc[1] = 24; sc[7] = 8; sc[6] = 33; sc[8] = 9; sc[4] = 26; sc[9] = 10; sc[2] = 31;
    if ((c0 >= 8 && c0 <= 10) || c0 == 1 || c0 == 3 || (c0 >= 31 && c0 <= 33) || c0 == 24 || c0 == 26) sc[3] = 11;
    if (c0 == 11 || c0 == 34) sc[3] = 10;
    break;
  case 11:
    sc[9] = 12; sc[8] = 12; sc[7] = 11; sc[6] = 34; sc[5] = 33; sc[4] = 32;
    break;
  case 12:
    sc[1] = 34; sc[2] = 8; sc[3] = 9; sc[4] = 10; sc[5] = 33; sc[6] = 10; sc[7] = 32; sc[8] = 11; sc[9] = 34;
    break;
  case 13:
    sc[1] = 36;
    sc[4] = 10;
    if (rgt()) {
      sc[7] = rgt() ? 9 : 32;
    } else {
      sc[7] = rgt() ? 8 : 31;
    }
    sc[3] = 34; sc[6] = 11; sc[9] = 34; sc[10] = 13; sc[11] = 13; sc[12] = 11; sc[13] = 10; sc[14] = 10;
    break;
  case 14:
    sc[4] = 12; sc[3] = 3; sc[2] = 13; sc[1] = 11;
    break;
  case 15:
    sc[13] = 14; sc[12] = 14; sc[11] = 13; sc[10] = 13; sc[9] = 11; sc[8] = 10;
    for (int32_t a = 3; a < 8; a++) {
      if (sc[a] < 10 || sc[a] == 13) sc[a] = mgt() ? 10 : 14;
    }
    sc[2] = 13; sc[1] = 11;
    break;
  case 16:
    sc[9] = 14; sc[8] = 37; sc[7] = 13; sc[6] = 11; sc[5] = 35; sc[4] = 36; sc[3] = 34; sc[2] = 33; sc[1] = 9;
    break;
  case 17:
    sc[np - 2] = 15; sc[np - 3] = 38; sc[np - 4] = 14; sc[np - 5] = 13; sc[np - 6] = 36;
    sc[np - 7] = 11; sc[np - 8] = 34; sc[np - 9] = 9; sc[np - 10] = 32;
    break;
  case 18:
    sc[5] = 12; sc[4] = 14; sc[3] = 15; sc[2] = 38; sc[1] = 13;
    break;
  case 19:
    sc[np - 2] = 16; sc[np - 3] = 16; sc[np - 4] = 36; sc[np - 5] = 15; sc[np - 6] = 38;
    sc[np - 7] = 16; sc[np - 8] = 13;
    sc[np - 9] = mgt() ? 15 : 38;
    sc[np - 10] = 16;
    break;
  case 21:
    sc[9] = 17; sc[8] = 17; sc[7] = 16; sc[6] = 15; sc[5] = 16; sc[4] = 38; sc[3] = 37; sc[2] = 13; sc[1] = 20;
    break;
  case 22:
    sc[9] = 16; sc[8] = 15; sc[7] = 16; sc[6] = 38; sc[5] = 20; sc[4] = 15; sc[3] = 14; sc[2] = 17; sc[1] = 17;
    break;
  case 23:
    sc[10] = 17; sc[9] = 17; sc[8] = 16; sc[7] = 15; sc[6] = 17; sc[5] = 38; sc[4] = 16;
    break;
  case 24:
    sc[9] = 17; sc[8] = 18; sc[7] = 18; sc[6] = 16; sc[5] = 15; sc[4] = 16;
    break;
  default:
    break;
  }
}

// A replayed stage: random opponents up to the best car unlocked so far,
// thinned by proba[] and the level bans (xtGraphics.js:12904-13060).
static void sortcars_replay(Lineup *L, int32_t i, bool *aflag) {
  CareerRace *r = L->r;
  int32_t *sc = r->sc;
  const int32_t np = r->nplayers;
  const int32_t far = L->s->unlocked;
  int32_t byte0 = np;
  int32_t bestcar2 = 7 + (i + 1) / 2;
  if (i >= 21) bestcar2 = 11 + i / 3;
  if (bestcar2 > 19) bestcar2 = 22;
  bool excep = i >= 21;
  if (sc[0] != bestcar2 || excep) {
    sc[np - 1] = bestcar2;
    byte0 = np - 1;
  }
  for (int32_t k2 = 1; k2 < byte0; k2++) {
    aflag[k2] = false;
    while (!aflag[k2]) {
      int32_t bestunlocked = 7 + (far + 1) / 2;
      if (far >= 21) bestunlocked = 11 + far / 3;
      if (bestunlocked > 19) bestunlocked = 22;
      int32_t randomiser2 = jtrunc_d(rnd() * (double)(bestunlocked + 1));
      if (bestunlocked == 18 || bestunlocked == 19) randomiser2 = jtrunc_d(rnd() * 22.0);
      int32_t gcboost2 = 0;
      if (mgt() && randomiser2 <= 15) gcboost2 = 23;
      sc[k2] = gcboost2 + randomiser2;
      aflag[k2] = true;
      int32_t g = 0;
      do {
        const int32_t c = sc[k2];
        // allowrepeats2 is false whenever careermode is (12966-12969).
        if (k2 != g && c == sc[g]) aflag[k2] = false;
        if (bestunlocked <= 15 && c == bestunlocked + 23) aflag[k2] = false;
        int32_t usercar = sc[0];
        if (usercar == 20) usercar = 18;
        if (usercar == 21) usercar = 17;
        if (usercar >= 23) usercar = sc[0] - 23;
        if ((bestunlocked == 18 && (c == 21 || c == 19)) || (bestcar2 == 19 && c == 21 && far < 25)) aflag[k2] = false;
        if (c == 21) aflag[k2] = false;
        const int32_t avg = r->averagelevel;
        bool ban0 = (c == 13 || c == 36) && avg < MAXLEVEL[5] && usercar < 13;
        bool ban1 = (c == 18 || c == 20) && avg < MAXLEVEL[10] && usercar < 18;
        bool ban2 = (c == 19 || c == 21) && avg < MAXLEVEL[17] && usercar < 18;
        bool ban3 = c == 22 && avg < MAXLEVEL[24];
        bool specialonly = false;
        if (!L->rollonce) {
          L->chance = (float)(rnd() * 1.0);
          L->rollonce = true;
        }
        if (usercar >= 18) {
          double probability = usercar == 22 ? 0.25 : 0.5;
          if ((c == 18 || c == 19) && (double)L->chance >= probability) specialonly = true;
        }
        if (!specialonly && ((ban0 && c == 13) || (ban1 && (c == 18 || c == 20)) ||
                             (ban2 && (c == 19 || c == 21)) || (ban3 && c == 22)))
          aflag[k2] = false;
      } while (++g < np);
      float f = PROBA[sc[k2]];
      if (i - sc[k2] > 4 && i != 28) {
        f = f + (float)(i - sc[k2] - 4) / 10.0f;
        if ((double)f > 0.9) f = 0.9f;
      }
      if (i == 16 && (double)f < 0.9) f = 0.9f;
      if (rnd() < (double)f) aflag[k2] = false;
    }
  }
}

static void sortcars(Lineup *L, int32_t i) {
  CareerRace *r = L->r;
  int32_t *sc = r->sc;
  const int32_t np = r->nplayers;
  if (!L->bonstage) {
    bool aflag[CAREER_MAX_PLAYERS];
    memset(aflag, 0, sizeof aflag);
    bool lateststage = L->s->unlocked == i && L->s->unlocked != 31;
    if (lateststage || r->hardstage) sortcars_latest(L, i, aflag);
    else sortcars_replay(L, i, aflag);
    // 13104-13120
    if (i == 17) { sc[1] = 13; sc[2] = 13; sc[3] = 13; }
    if (i == 11) for (int32_t a = 1; a < 5; a++) sc[a] = 11;
    if (i == 20) sc[np - 1] = 17;
    if (i == 23 && hard_or_latest(L, 23)) sc[1] = 18;
  } else {
    // 13121-13159
    if (L->bonusstage[0]) {
      sc[10] = 32; sc[9] = 33; sc[8] = 30; sc[7] = 26; sc[6] = 31; sc[5] = 25; sc[4] = 28; sc[3] = 29;
      sc[2] = 27; sc[1] = 23;
    }
    if (L->bonusstage[1]) {
      sc[10] = 33; sc[9] = 35; sc[8] = 35;
      for (int32_t a = 1; a < 5; a++) sc[a] = 36;
      for (int32_t a = 5; a < 8; a++) sc[a] = 34;
    }
    if (L->bonusstage[2]) {
      sc[8] = 38; sc[7] = 37; sc[6] = 34; sc[5] = 31; sc[4] = 32; sc[3] = 33; sc[2] = 35; sc[1] = 36;
    }
    if (L->bonusstage[3]) {
      sc[4] = 20; sc[3] = 38; sc[2] = 36; sc[1] = 34;
    }
  }
  L->sortedcars = true;
}

// --- beasts / sortshadows --------------------------------------------------

static int32_t roll_slot(int32_t span, int32_t base) {
  return jtrunc_d(rnd() * (double)span) + base;
}

// Mark exactly beastcar[0] and beastcar[1] among slots 0..np-1.
static void mark_two(Lineup *L) {
  for (int32_t a = 0; a < L->r->nplayers; a++)
    L->r->beast[a] = a == L->beastcar[0] || a == L->beastcar[1];
  L->ssdone = true;
}

static void sortshadows(Lineup *L, int32_t i, int32_t num) {
  CareerRace *r = L->r;
  const int32_t *sc = r->sc;
  const int32_t np = r->nplayers;
  bool hard = hard_or_latest(L, i);
  bool specialar = i == 22 || (i == 19 && hard);
  if (num == 1) {
    if (specialar) {
      if (i == 19) {
        if (sc[L->beastcar[0]] == 16 && sc[L->beastcar[1]] == 16) {
          if (L->shadow[0] == 0 || r->beast[L->shadow[0]] || sc[L->shadow[0]] == 16) {
            L->shadow[0] = roll_slot(np - 2, 1);
          } else {
            r->shadow[L->shadow[0]] = true;
            L->alldone = true;
          }
        } else if (L->shadow[0] == 0 || r->beast[L->shadow[0]] || sc[L->shadow[0]] != 16) {
          L->shadow[0] = roll_slot(np - 2, 1);
        } else {
          r->shadow[L->shadow[0]] = true;
          L->alldone = true;
        }
      }
      if (i == 22) {
        r->shadow[np - 1] = true;
        L->alldone = true;
      }
    } else if (L->shadow[0] == 0 || r->beast[L->shadow[0]]) {
      L->shadow[0] = roll_slot(np - 2, 1);
    } else {
      r->shadow[L->shadow[0]] = true;
      L->alldone = true;
    }
  } else {
    int32_t undeadsort = 0;
    if (i == 17) undeadsort = 3;
    if (i == 23 && hard_or_latest(L, 23)) undeadsort = 1;
    for (int32_t a = 0; a < num; a++) {
      for (int32_t b = 0; b < num; b++) {
        if (a == b) continue;
        if (L->shadow[a] == 0 || L->shadow[a] == L->shadow[b] || r->beast[L->shadow[a]] || r->beast[L->shadow[b]]) {
          L->shadow[a] = roll_slot(np - 2 - undeadsort, 1) + undeadsort;
        } else {
          r->shadow[L->shadow[a]] = true;
          L->alldone = true;
        }
      }
    }
  }
}

static void beasts(Lineup *L, int32_t i) {
  CareerRace *r = L->r;
  const int32_t *sc = r->sc;
  const int32_t np = r->nplayers;
  int32_t *bc = L->beastcar;
  if (!L->bonstage && i != 5 && i != 6 && i != 7 && i != 8 && i != 11 && i != 12 && i != 13 && i != 17 &&
      i != 21 && i != 18 && i != 19 && i != 22 && i != 23 && i != 24) {
    if (bc[0] == 0 || bc[0] == np - 1) {
      bc[0] = roll_slot(np - 2, 1);
    } else {
      for (int32_t a = 1; a < np; a++) r->beast[a] = a == bc[0];
      L->ssdone = true;
    }
  } else {
    if (L->bonusstage[0] || L->bonusstage[2] || L->bonusstage[3]) {
      for (int32_t a = 1; a < np; a++) r->beast[a] = true;
      L->ssdone = true;
    }
    if (L->bonusstage[1]) {
      for (int32_t a = 1; a < 8; a++) r->beast[a] = true;
      L->ssdone = true;
      for (int32_t a = 8; a < 11; a++) r->beast[a] = false;
    }
    if (i == 5 && !L->bonstage) {
      for (int32_t a = 1; a < np; a++) r->beast[a] = a == 1;
      L->ssdone = true;
    }
    if (i == 6 || i == 7) {
      int32_t c = sc[bc[0]];
      if (bc[0] <= 1 || bc[0] == np - 1 ||
          ((c == 10 || c == 6 || c == 7 || c == 2 || c == 33 || c == 29 || c == 30 || c == 25) &&
           hard_or_latest(L, i))) {
        bc[0] = roll_slot(np - 2, 1);
      } else {
        for (int32_t a = 1; a < np; a++) r->beast[a] = a == bc[0];
        L->ssdone = true;
      }
    }
    if ((i == 8 || i == 19 || i == 12 || i == 21 || i == 22) && !r->hardstage && L->s->unlocked > i) {
      if (bc[0] == 0 || bc[1] == 0 || bc[0] == np - 1 || bc[1] == np - 1 || bc[0] == bc[1]) {
        bc[0] = roll_slot(np - 2, 1);
        bc[1] = roll_slot(np - 2, 1);
      } else {
        mark_two(L);
      }
    }
    if (i == 8 && hard_or_latest(L, 8)) {
      for (int32_t a = 0; a < np; a++) r->beast[a] = a == 1 || a == 2;
      L->ssdone = true;
    }
    if (i == 12 && hard_or_latest(L, 12)) {
      bool done0 = false, done1 = false;
      if (bc[0] == 0 || (sc[bc[0]] != 11 && sc[bc[0]] != 34)) {
        bc[0] = roll_slot(np - 2, 1);
      } else {
        done0 = true;
      }
      int32_t c1 = sc[bc[1]];
      if (bc[1] == 0 || (c1 != 10 && c1 != 9 && c1 != 8 && c1 != 31 && c1 != 33 && c1 != 32) || bc[1] == bc[0]) {
        bc[1] = roll_slot(np - 2, 1);
      } else {
        done1 = true;
      }
      if (done0 && done1) mark_two(L);
    }
    if (i == 13) {
      for (int32_t a = 1; a < np; a++) r->beast[a] = a <= 9;
      L->ssdone = true;
    }
    if ((i == 19 || i == 22) && hard_or_latest(L, i)) {
      bool done0 = false, done1 = false;
      if (bc[0] == 0 || sc[bc[0]] != 16) {
        bc[0] = roll_slot(np - 2, 1);
      } else {
        done0 = true;
      }
      if (bc[1] == 0 || (sc[bc[1]] != 15 && sc[bc[1]] != 38)) {
        bc[1] = roll_slot(np - 2, 1);
      } else {
        done1 = true;
      }
      if (done0 && done1) mark_two(L);
    }
    if (i == 21 && hard_or_latest(L, 21)) {
      if (bc[0] == 0 || bc[1] == 0 || bc[0] >= 8 || bc[1] >= 8 || bc[0] == bc[1]) {
        bc[0] = roll_slot(np - 2, 1);
        bc[1] = roll_slot(np - 2, 1);
      } else {
        mark_two(L);
      }
    }
    if (i == 11 && !L->bonstage) {
      for (int32_t a = 1; a < 5; a++) r->beast[a] = true;
      if (bc[0] <= 4 || bc[0] == np - 1) {
        if (hard_or_latest(L, 11)) bc[0] = 9;
        else bc[0] = roll_slot(np - 6, 5);
      } else {
        for (int32_t a = 5; a < np; a++) r->beast[a] = a == bc[0];
        L->ssdone = true;
      }
    }
    if (i == 17) {
      if (bc[0] == 0 || bc[0] == np - 1) {
        bc[0] = roll_slot(np - 5, 4);
        bc[1] = roll_slot(np - 5, 4);
      } else {
        for (int32_t a = 0; a < np; a++) r->beast[a] = a == bc[0] || a == bc[1];
        for (int32_t a = 1; a < 4; a++) r->beast[a] = true;
        L->ssdone = true;
      }
    }
    if (i == 24 || (i == 18 && !L->bonusstage[3])) {
      for (int32_t a = 1; a < np; a++) r->beast[a] = false;
      L->ssdone = true;
    }
    if (i == 23) {
      if (hard_or_latest(L, 23)) {
        r->beast[1] = true;
        for (int32_t a = 2; a < np; a++) r->beast[a] = false;
      } else {
        for (int32_t a = 1; a < np; a++) r->beast[a] = false;
      }
      L->ssdone = true;
    }
  }
  if (i >= 15 && !L->bonusstage[2] && L->ssdone) sortshadows(L, i, r->noshadows);
  if (L->ssdone && (i < 15 || L->bonusstage[2] || L->bonusstage[3] || i == 20 || i == 21)) L->alldone = true;
}

// --- public -----------------------------------------------------------------

void career_randomno(CareerRace *r, const CareerSave *s) {
  const int32_t st = r->stage;
  const bool bonstage = r->bonus != 0;
  int32_t np = 11;
  if (st == 15 && r->bonus == 3) np = 9;
  if (st == 3 || st == 17) np = 19;
  if (st == 8) np = 15;
  if (st == 13) np = 16;
  if (st == 11 && !bonstage && s->unlocked > 11 && !r->hardstage) np = 17;
  if (st == 20) np = 2;
  if ((st == 5 && !bonstage) || st == 18) np = 7;
  if (st == 23) np = (s->unlocked == 23 || r->hardstage) ? 12 : 11;
  if (st == 14) np = 6;
  if (r->bonus == 4) np = 5;
  r->nplayers = np;
  if (st < 15 || r->bonus == 3 || st == 20 || st == 21 || r->bonus == 4) {
    r->noshadows = 0;
  } else if (st == 17 || st == 23 || st == 24 || st == 18) {
    r->noshadows = 2;
  } else {
    r->noshadows = 1;
  }
}

void career_sortcars(CareerRace *r, const CareerSave *s) {
  Lineup L;
  memset(&L, 0, sizeof L);
  L.r = r;
  L.s = s;
  L.bonstage = r->bonus != 0;
  for (int k = 0; k < 4; k++) L.bonusstage[k] = r->bonus == k + 1;
  // fase 6476: resetbeasts, resetshadows (GameSparker.js:1804-1807)
  for (int a = 0; a < CAREER_MAX_PLAYERS; a++) {
    r->beast[a] = false;
    r->shadow[a] = false;
  }
  const int32_t i = r->stage;
  const bool except = i == 9 || i == 10 || i == 14 || i == 20;
  for (int pass = 0; pass < MAX_PASSES; pass++) {
    const bool last = L.alldone;       // GameSparker.js:1239: alldone -> fase -69
    for (int a = 0; a < CAREER_MAX_PLAYERS; a++) r->undead[a] = false;   // resetstat 14013
    L.sortedcars = false;
    sortcars(&L, i);
    if (i >= 4 && !except && !r->nolevels) {   // GameSparker.js:1246-1256
      if (L.sortedcars) beasts(&L, i);
    } else {
      L.alldone = true;
    }
    if (last) break;
  }
  // careermode$m sets these on the race's first frame.
  const int32_t np = r->nplayers;
  if (i == 11 && r->bonus != 2)                       // 7764, 7777-7781
    for (int a = 1; a < 5; a++) r->undead[a] = true;
  if (r->bonus == 4)                                  // 8291, 8376-8387
    for (int a = 1; a < np - 1; a++) r->undead[a] = true;
  if (i == 17)                                        // 8967, 9007-9010
    for (int a = 1; a < 4; a++) r->undead[a] = true;
  if (i == 23 && hard_or_latest(&L, 23))              // 8399, 8421-8423
    r->undead[1] = true;
}
