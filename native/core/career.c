// See career.h. The save: this port's own text file (career.txt beside the
// progress file), not the jar's byte-swapped savedata.radq -- the same
// fields (GameSparker.writedata, docs/extended-career.md "Save").
#include "career.h"

#include <stdio.h>
#include <string.h>

// xtGraphics.statsalc (XT 776): each car's six perks.
// Extended v2.8 left NFM 2's first nine cars (its 23-31) without perks: six
// empty slots, every one perk 0 (ENERGY). This port gives each a set in its
// own style, from the perks the game applies (career_perk_applied).
const int8_t career_statsalc[CAREER_CARS][CAREER_PERK_SLOTS] = {
    {0, 1, 5, 20, 27, 39},   {3, 8, 15, 25, 31, 36},   {4, 12, 18, 19, 26, 39},  {0, 6, 10, 13, 32, 38},
    {2, 18, 20, 28, 33, 37}, {7, 11, 21, 23, 30, 34},  {9, 10, 14, 22, 24, 38},  {1, 2, 6, 8, 27, 36},
    {0, 13, 16, 21, 30, 39}, {9, 12, 20, 23, 24, 32},  {1, 2, 15, 16, 31, 33},   {4, 5, 18, 19, 29, 34},
    {3, 6, 16, 25, 36, 39},  {7, 14, 17, 26, 28, 32},  {0, 12, 19, 27, 30, 37},  {4, 8, 11, 17, 26, 35},
    {7, 9, 19, 20, 23, 28},  {5, 13, 16, 21, 25, 37},  {4, 10, 14, 18, 22, 34},  {3, 11, 23, 24, 32, 33},
    {4, 19, 26, 28, 29, 32}, {3, 8, 10, 31, 33, 38},   {8, 14, 23, 26, 29, 36},
    {0, 3, 5, 20, 21, 30},     // Tornado Shark: all-rounder
    {2, 3, 5, 21, 27, 39},     // Formula 7: speed, fragile
    {0, 1, 5, 20, 30, 37},     // Wow Caninaro: stunts
    {4, 7, 11, 15, 23, 29},    // La Vita Crab: bruiser
    {1, 2, 5, 6, 37, 39},      // Nimi: light stunter
    {6, 9, 12, 19, 28, 33},    // MAX Revenge: fighter
    {7, 11, 15, 16, 25, 35},   // Lead Oxide: tank
    {0, 2, 3, 21, 25, 27},     // Kool Kat: racer
    {4, 9, 23, 29, 33, 38},    // Drifter X: waster
    {1, 2, 9, 12, 22, 30},   {6, 10, 13, 15, 25, 37},  {5, 11, 15, 27, 29, 34},  {21, 30, 31, 33, 36, 39},
    {7, 16, 17, 20, 28, 38}, {0, 3, 13, 25, 27, 37},   {17, 18, 22, 24, 34, 35}};

const char *const career_perk_name[CAREER_PERKS] = {
    "ENERGY",   "GAMBLER",  "GREED",   "GETAWAY",  "CHEAPSHOT", "ESCAPE",  "FEARLESS",  "PUSHING",
    "RESISTANCE", "RECKLESS", "SURVIVAL", "RAMPAGE", "BRAVERY",  "AWARENESS", "BACKHIT", "ARMOUR",
    "WEIGHT",   "LIFTING",  "DRAINER", "LEAKAGE",  "RUTHLESS",  "FRESHNESS", "STEROIDS", "BERSERK",
    "SAFETY",   "RECOVERY", "BLEED",   "TURNING",  "DEBUFF",    "LEECH",   "CHARGING",  "REFLECT",
    "FREEZE",   "SAVIOUR",  "UNDEAD",  "GROUNDED", "GRAVITY",   "COMEBACK", "HEALING",  "STABILITY"};

// writeboosts (XT 18407-18690), word for word.
const CareerPerkText career_perk_text[CAREER_PERKS] = {
    {{"makes your power drain", "less quickly (powersave)."}, {"stampede level at maximum."}},
    {{"increases your chances", "of receiving bonus stat points."}, {"20% increase at maximum."}},
    {{"increases experience", "gained."}, {"20% increase at maximum."}},
    {{"boosts your speed above", "80% damage."}, {"20% increase at maximum."}},
    {{"increases damage to badly", "landed opponents."}, {"20% increase at maximum."}},
    {{"lowers the time taken to", "recover from bad landings."}, {"80% decrease at maximum."}},
    {{"lowers damage taken from", "beast and shadow cars."}, {"15% decrease at maximum."}},
    {{"pushes cars further away", "when wasting."}, {"equal to agent waster at", "maximum."}},
    {{"the effects of debuffs", "are reduced."}, {"50% decrease at maximum."}},
    {{"boosts your strength at", "over 80% damage."}, {"20% increase at maximum."}},
    {{"increases your defence in", "a bad landing."}, {"25% increase at maximum."}},
    {{"lowers momentum lost", "when wasting other cars."}, {"100% decrease at maximum."}},
    {{"increases damage dealt to", "beast and shadow cars."}, {"20% increase at maximum."}},
    {{"increases defence whilst", "reversing."}, {"20% increase at maximum."}},
    {{"increases strength whilst", "reversing."}, {"20% increase at maximum."}},
    {{"increases defence when", "power falls below 75%."}, {"20% increase at maximum."}},
    {{"lowers how high you are", "launched by opponents."}, {"100% decrease at maximum."}},
    {{"increases how high you", "launch opponents."}, {"equal to revonater at", "maximum."}},
    {{"hitting other cars drains", "their special."}, {"100% of damage dealt is", "drained at maximum."}},
    {{"hitting other cars drains", "their power."}, {"50% of damage dealt is", "drained at maximum."}},
    {{"increases your stat boosts", "in your special attack."}, {"25% increase at maximum."}},
    {{"raises your speed after", "you fix (temporary)."}, {"20% speed/100% duration", "increase at maximum."}},
    {{"raises your strength after", "you fix (temporary)."}, {"20% strength/80% duration", "increase at maximum."}},
    {{"wasting cars increases", "strength (temporary)."}, {"20% strength/80% duration", "increase at maximum."}},
    {{"wasting cars increases", "defence (temporary)."}, {"20% defence/100% duration", "increase at maximum."}},
    {{"restores some health when", "clearing checkpoints."}, {"4% recovery/checkpoint", "at maximum."}},
    {{"hitting cars drains their,", "health further over time."}, {"20% of each hit is drained", "overall at maximum."}},
    {{"increases the sharpness of", "your turning."}, {"Speedy 7 level at maximum."}},
    {{"increases the power of", "your special's debuffs."}, {"50% increase at maximum."}},
    {{"hitting cars recovers", "your health."}, {"15% of damage is recovered", "at maximum."}},
    {{"increases the rate that", "your special charges at."}, {"20% increase at maximum."}},
    {{"increases recoil taken", "by your attackers."}, {"50% increase at maximum."}},
    {{"hitting cars decreases", "their speed temporarily."}, {"50% increase in duration", "at maximum."}},
    {{"increases your chances of", "surviving a fatal hit."}, {"20% chance of survival at", "maximum."}},
    {{"you can become an undead car after", "you're wasted for a short time."},
     {"100% increase in duration and 25%", "chance of occurring at maximum."}},
    {{"reduces how much you get", "lifted when hitting cars."}, {"100% decrease at maximum."}},
    {{"allows you to increase gravity", "on your car by pressing \"G\"."}, {"gravity increases 5x at maximum."}},
    {{"increases speed after a", "bad landing (temporary)."}, {"20% speed/100% duration", "increase at maximum."}},
    {{"health recovers steadily", "during your special."}, {"60% health restored in", "total at maximum."}},
    {{"reduces the bounciness of", "your car."}, {"stampede level at maximum."}},
};

bool career_perk_applied(int32_t perk) {
  // v2.8's 0-24 but RESISTANCE, and the eleven this port adds (career.h).
  return perk >= 0 && perk < CAREER_PERKS && perk != 26 && perk != 31 && perk != 32 && perk != 34 && perk != 36;
}

int32_t career_perk_rows(int32_t car, int32_t *slots) {
  int32_t n = 0;
  if (car < 0 || car >= CAREER_CARS) return 0;
  for (int32_t a = 0; a < CAREER_PERK_SLOTS; a++)
    if (career_perk_applied(career_statsalc[car][a])) slots[n++] = a;
  return n;
}

void career_reset(CareerSave *s) {
  memset(s, 0, sizeof(*s));
  s->unlocked = 1;
  s->laststage = 1;
  for (int32_t a = 0; a < CAREER_CARS; a++) {
    s->level[a] = 1;
    s->rebsp[a] = 1.0;
  }
}

/** specialstats[car][perk][slot] off the save. */
static int32_t save_special(const CareerSave *s, int32_t car, int32_t perk, int32_t slot) {
  if (car < 0 || car >= CAREER_CARS) return 0;
  return career_statsalc[car][slot] == perk ? s->perk[car][slot] : 0;
}

double career_perk_mod(const CareerSave *s, int32_t car, int32_t perk) {
  // The original's loop over the six slots: the last one holding the perk
  // with points sets it (XT 4030, 5094, 5165).
  double mod = 1.0;
  for (int32_t a = 0; a < CAREER_PERK_SLOTS; a++) {
    const int32_t v = save_special(s, car, perk, a);
    if (v > 0) mod = 1.0 + v / 100.0;
  }
  return mod;
}

static int32_t clampi(int32_t v, int32_t lo, int32_t hi) { return v < lo ? lo : (v > hi ? hi : v); }

bool career_load(const char *path, CareerSave *s) {
  career_reset(s);
  FILE *f = fopen(path, "r");
  if (!f) {
    // A save cut off between career_save's remove and rename is still whole aside.
    char tmp[600];
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    f = fopen(tmp, "r");
  }
  if (!f) return false;
  char line[256];
  while (fgets(line, sizeof(line), f)) {
    int v[13];
    double d[2];
    if (sscanf(line, "unlocked=%d", &v[0]) == 1) s->unlocked = clampi(v[0], 1, CAREER_STAGES);
    if (sscanf(line, "laststage=%d", &v[0]) == 1) s->laststage = clampi(v[0], 1, CAREER_STAGES);
    if (sscanf(line, "lastcar=%d", &v[0]) == 1) s->lastcar = clampi(v[0], 0, CAREER_CARS - 1);
    if (sscanf(line, "kills=%d", &v[0]) == 1) s->kills = v[0];
    if (sscanf(line, "wins=%d", &v[0]) == 1) s->wins = v[0];
    if (sscanf(line, "carpoints=%d", &v[0]) == 1) s->carpoints = v[0];
    if (sscanf(line, "changers=%d,%d", &v[0], &v[1]) == 2) {
      s->statchangers[0] = v[0];
      s->statchangers[1] = v[1];
    }
    if (sscanf(line, "bonus=%d,%d,%d,%d,%d,%d", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) == 6) {
      for (int32_t k = 0; k < 6; k++) s->boncomp[k] = clampi(v[k], 0, 3);
    }
    int a;
    if (sscanf(line, "car=%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", &a, &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6],
               &v[7], &v[8], &v[9], &v[10], &v[11]) == 13 &&
        a >= 0 && a < CAREER_CARS) {
      s->level[a] = clampi(v[0], 1, 999);
      s->exp[a] = v[1] < 0 ? 0 : v[1];
      s->statpoints[a] = v[2] < 0 ? 0 : v[2];
      for (int32_t k = 0; k < CS_N; k++) s->sp[a][k] = v[3 + k] < 0 ? 0 : v[3 + k];
      s->killscn[a] = v[9];
      s->winscn[a] = v[10];
      s->extpoints[a] = v[11];
    }
    // Lines older saves do not have (they keep career_reset's values).
    if (sscanf(line, "perk=%d,%d,%d,%d,%d,%d,%d", &a, &v[0], &v[1], &v[2], &v[3], &v[4], &v[5]) == 7 && a >= 0 &&
        a < CAREER_CARS) {
      for (int32_t k = 0; k < CAREER_PERK_SLOTS; k++) s->perk[a][k] = clampi(v[k], 0, CAREER_PERK_MAX);
    }
    if (sscanf(line, "bsp=%d,%lf,%lf", &a, &d[0], &d[1]) == 3 && a >= 0 && a < CAREER_CARS) {
      s->rebsp[a] = d[0];
      s->xbsp[a] = d[1];
    }
  }
  fclose(f);
  return true;
}

bool career_save(const char *path, const CareerSave *s) {
  // Written aside and renamed over, so a crash mid-write keeps the old save.
  char tmp[600];
  snprintf(tmp, sizeof(tmp), "%s.tmp", path);
  FILE *f = fopen(tmp, "w");
  if (!f) return false;
  fprintf(f, "unlocked=%d\nlaststage=%d\nlastcar=%d\nkills=%d\nwins=%d\ncarpoints=%d\nchangers=%d,%d\n",
          (int)s->unlocked, (int)s->laststage, (int)s->lastcar, (int)s->kills, (int)s->wins, (int)s->carpoints,
          (int)s->statchangers[0], (int)s->statchangers[1]);
  fprintf(f, "bonus=%d,%d,%d,%d,%d,%d\n", (int)s->boncomp[0], (int)s->boncomp[1], (int)s->boncomp[2],
          (int)s->boncomp[3], (int)s->boncomp[4], (int)s->boncomp[5]);
  for (int32_t a = 0; a < CAREER_CARS; a++) {
    fprintf(f, "car=%d,%d,%d,%d", (int)a, (int)s->level[a], (int)s->exp[a], (int)s->statpoints[a]);
    for (int32_t k = 0; k < CS_N; k++) fprintf(f, ",%d", (int)s->sp[a][k]);
    fprintf(f, ",%d,%d,%d\n", (int)s->killscn[a], (int)s->winscn[a], (int)s->extpoints[a]);
  }
  for (int32_t a = 0; a < CAREER_CARS; a++) {
    bool any = false;
    for (int32_t k = 0; k < CAREER_PERK_SLOTS; k++) any = any || s->perk[a][k] != 0;
    if (any) {
      fprintf(f, "perk=%d", (int)a);
      for (int32_t k = 0; k < CAREER_PERK_SLOTS; k++) fprintf(f, ",%d", (int)s->perk[a][k]);
      fprintf(f, "\n");
    }
    if (s->rebsp[a] != 1.0 || s->xbsp[a] != 0.0) fprintf(f, "bsp=%d,%.17g,%.17g\n", (int)a, s->rebsp[a], s->xbsp[a]);
  }
  if (fclose(f) != 0) return false;
  remove(path);   // rename() will not replace an existing file on every platform
  return rename(tmp, path) == 0;
}

void career_resetstats(CareerSave *s, int32_t car, int32_t addon) {
  // XT 18264-18307: each bonus car's points in its own make.
  static const int32_t kBase[8][CS_N] = {
      {10, 10, 60, 25, 15, 10},   // 31
      {10, 15, 10, 10, 15, 20},   // 32
      {7, 12, 12, 12, 8, 21},     // 33
      {10, 17, 17, 17, 17, 24},   // 34
      {10, 17, 17, 17, 17, 24},   // 35
      {50, 10, 20, 20, 15, 25},   // 36
      {10, 17, 17, 17, 17, 24},   // 37
      {10, 17, 17, 17, 17, 24}};  // 38
  if (car < 31 || car >= CAREER_CARS) return;
  for (int32_t k = 0; k < CS_N; k++) s->sp[car][k] = kBase[car - 31][k] + addon;
}

void career_bonus_car_points(CareerSave *s, int32_t car) {
  if (car >= 31 && car < CAREER_CARS && s->sp[car][CS_TS] == 0) career_resetstats(s, car, 0);
}

bool career_spend_perk(CareerSave *s, int32_t car, int32_t slot) {
  if (car < 0 || car >= CAREER_CARS || slot < 0 || slot >= CAREER_PERK_SLOTS || s->carpoints <= 0) return false;
  if (s->perk[car][slot] >= CAREER_PERK_MAX || !career_perk_applied(career_statsalc[car][slot])) return false;
  s->perk[car][slot]++;
  s->carpoints--;
  return true;
}

int32_t career_reshuffle_cost(const CareerSave *s, int32_t car) {
  int32_t statloss = (int32_t)(s->extpoints[car] * 0.35);
  if (statloss < 1 && s->extpoints[car] > 0) statloss = 1;
  if (s->statchangers[0] > 0) statloss = 0;
  return statloss;
}

/** Every point spent back: the stats to level - 1 (a bonus car's own on top). */
static void reset_spent(CareerSave *s, int32_t car, int32_t level) {
  if (car >= 31) {
    career_resetstats(s, car, level - 1);
  } else {
    for (int32_t k = 0; k < CS_N; k++) s->sp[car][k] = level - 1;
  }
}

void career_reshuffle(CareerSave *s, int32_t car) {
  s->extpoints[car] -= career_reshuffle_cost(s, car);
  if (s->extpoints[car] < 0) s->extpoints[car] = 0;
  s->statpoints[car] = career_spcalc(s->level[car]) + s->extpoints[car];
  reset_spent(s, car, s->level[car]);
  if (s->statchangers[0] > 0) s->statchangers[0]--;
}

int32_t career_sell_price(const CareerSave *s, int32_t car) { return s->level[car] / 3; }

/** A car back to level 1 with nothing earned (XT 16595-16612, 17343-17361). */
static void reset_car(CareerSave *s, int32_t car) {
  s->statpoints[car] = 0;
  s->killscn[car] = 0;
  s->winscn[car] = 0;
  s->level[car] = 1;
  s->exp[car] = 0;
  s->extpoints[car] = 0;
  s->xbsp[car] = 0.0;
  s->rebsp[car] = 1.0;
  if (car < 31) {
    memset(s->sp[car], 0, sizeof(s->sp[car]));
  } else {
    career_resetstats(s, car, 0);
  }
}

void career_sell(CareerSave *s, int32_t car) {
  s->carpoints += career_sell_price(s, car);
  reset_car(s, car);
}

/** xtGraphics.xbspratio (XT 796): what a car's bonus points are worth. */
static const double kXbspratio[CAREER_CARS] = {1.0,  1.0,  1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.25, 1.25, 1.5,  1.5, 1.75,
                                               1.75, 2.0,  2.0, 3.0, 3.5, 4.0, 4.5, 4.0, 4.0, 5.0,  1.0,  1.0,  1.0,
                                               1.0,  1.0,  1.0, 1.0, 1.0, 2.0, 2.0, 2.0, 2.25, 2.25, 2.5, 2.75, 2.75};

void career_transfer(CareerSave *s, int32_t from, int32_t to) {
  if (from == to || from < 0 || to < 0 || from >= CAREER_CARS || to >= CAREER_CARS) return;
  // shufflefase 7: `to` takes it all.
  s->level[to] = s->level[from];
  s->rebsp[to] = s->rebsp[from] * (kXbspratio[to] / kXbspratio[from]);
  double adjcap = kXbspratio[to] / kXbspratio[from];
  if (s->rebsp[to] > 1.0) adjcap = 1.0 / s->rebsp[from];
  if (s->rebsp[from] > 1.0) adjcap = adjcap * s->rebsp[from];
  const int32_t bspoints = (int32_t)(s->extpoints[from] * adjcap);
  s->statpoints[to] = career_spcalc(s->level[from]) + bspoints;
  s->exp[to] = 0;
  s->killscn[to] = s->killscn[from];
  s->winscn[to] = s->winscn[from];
  s->extpoints[to] = bspoints;
  reset_spent(s, to, s->level[from]);
  // shufflefase 8: `from` starts over.
  reset_car(s, from);
  if (s->statchangers[1] > 0) s->statchangers[1]--;
}

// xtGraphics.outdam (XT 692), by Extended car number.
static const float kOutdam[CAREER_CARS] = {
    0.5f,  0.3f,  0.7f,  0.42f, 0.56f, 0.35f, 0.66f, 0.85f, 0.72f, 0.62f, 0.79f, 1.1f,  0.68f,
    1.5f,  1.0f,  0.85f, 1.1f,  1.25f, 1.4f,  2.35f, 1.9f,  0.85f, 2.15f, 0.6f,  0.3f,  0.7f,
    0.42f, 0.5f,  0.46f, 0.75f, 0.65f, 0.72f, 0.62f, 0.79f, 0.95f, 0.77f, 1.5f,  0.85f, 1.0f};

void career_scout_stats(const CarDefine *cd, int32_t cn, int32_t car, const int32_t sp[CS_N], int32_t out[CS_N]) {
  out[0] = (cd->swits[cn][2] + sp[CS_TS]) / 2;
  const float *ac = cd->acelf[cn];
  float r0 = (ac[0] + (float)sp[CS_ACC] * 0.1f) - 6.0f;
  float r1 = ((ac[1] * r0) / ac[0]) - 3.0f;
  float r2 = ((ac[2] * r0) / ac[0]) - 2.0f;
  const float accelf = ((r0 * 21.0f + r1 * 6.0f) + r2 * 3.0f) / 201.0f;
  out[1] = (int32_t)(accelf * 100.0f);
  const float contgri = ((cd->grip[cn] + (float)sp[CS_GRIP] * 0.2f) - 10.0f) / 20.0f;
  out[2] = (int32_t)(contgri * 100.0f);
  const float stunts = ((float)(cd->airc[cn] + sp[CS_STU]) + (cd->airs[cn] + (float)sp[CS_STU] * 0.025f) * 10.0f) / 125.0f;
  out[3] = (int32_t)(stunts * 100.0f);
  const float str = (cd->moment[cn] + (float)sp[CS_STR] * 0.025f) / 2.1f;
  out[4] = (int32_t)(str * 100.0f);
  const float end = (car >= 0 && car < CAREER_CARS ? kOutdam[car] : 1.0f) + (float)sp[CS_END] * 0.01f;
  out[5] = (int32_t)(end * 100.0f);
}

int32_t career_car_lock(const CareerSave *s, int32_t car) {
  // xtGraphics.carselect's notunlocked (XT 15274-15309).
  if (car <= 17 && (car - 7) * 2 >= s->unlocked) return (car - 7) * 2;
  if ((car == 18 || car == 19) && (car - 11) * 3 + 2 >= s->unlocked) return (car - 11) * 3 + 2;
  if (car == 22 && s->unlocked <= 30) return 30;
  if (s->boncomp[0] == 0 && car >= 31 && car <= 33) return -1;
  if (s->boncomp[1] == 0 && car >= 34 && car <= 36) return -2;
  if ((s->boncomp[1] == 1 && car == 36) || (s->boncomp[1] == 2 && car == 35)) return -2;
  if (s->boncomp[2] == 0 && (car == 37 || car == 38)) return -3;
  if ((car == 20 && s->boncomp[4] == 0) || (car == 21 && s->boncomp[5] == 0)) return 99;
  return 0;
}

/** The bonus cars' handicap for the player (Madness.js:1424-1440, 1575-1589):
 * their stats come part free, so their boosts count less. */
static int32_t cheat_grip(int32_t car) {
  if (car == 31) return 60;
  if (car == 32) return 10;
  if (car == 33) return 12;
  if (car == 36) return 20;
  if (car == 34 || car == 35 || car == 37 || car == 38) return 17;
  return 0;
}

static int32_t cheat_accel(int32_t car) {
  if (car == 32) return 15;
  if (car == 33) return 12;
  if (car == 31 || car == 36) return 10;
  if (car == 34 || car == 35 || car == 37 || car == 38) return 17;
  return 0;
}

void career_apply_stats(CarDefine *cd, int32_t cn, int32_t car, const int32_t sp[CS_N], int32_t level, bool shadow,
                        bool player) {
  // nitroandspecials' career rebuild (XT 7001-7051), without the stage
  // effects (fire, stat drain): the car's own values plus its points.
  int32_t maxspeed = cd->swits[cn][2] + sp[CS_TS];
  const int32_t s0 = cd->swits[cn][0], s1 = cd->swits[cn][1], s2 = cd->swits[cn][2];
  cd->swits[cn][0] = s0 * maxspeed / s2;
  cd->swits[cn][1] = s1 * maxspeed / s2;
  cd->swits[cn][2] = maxspeed;
  for (int32_t k = 0; k < 3; k++)
    if (cd->swits[cn][k] < 20) cd->swits[cn][k] = 20;
  const float a0 = cd->acelf[cn][0];
  const float maxaccel = a0 + (float)sp[CS_ACC] / 10.0f;
  cd->acelf[cn][1] = cd->acelf[cn][1] * maxaccel / a0;
  cd->acelf[cn][2] = cd->acelf[cn][2] * maxaccel / a0;
  cd->acelf[cn][0] = maxaccel;
  cd->grip[cn] = cd->grip[cn] + (float)sp[CS_GRIP] * 0.2f;
  cd->airs[cn] = cd->airs[cn] + (float)sp[CS_STU] * 0.025f;
  cd->airc[cn] = cd->airc[cn] + sp[CS_STU];
  cd->moment[cn] = cd->moment[cn] + (float)sp[CS_STR] * 0.025f;
  cd->maxmag[cn] = career_healthcalc(cd->maxmag[cn], sp[CS_END], car, 1.0f);
  // Madness.drive's handling boost (Madness.js:1418-1449).
  const int32_t handbboost = sp[CS_GRIP] - (level - 1) - (shadow ? 50 : 0) - (player ? cheat_grip(car) : 0);
  // ponytail: handb is an int here (Extended's is a float), so tenths of a
  // point are dropped; make CarDefine.handb a float if that ever shows.
  cd->handb[cn] = (int32_t)((float)cd->handb[cn] + (float)handbboost * 0.1f);
  cd->turn[cn] = (float)((double)cd->turn[cn] + handbboost * 0.1);
  if (cd->turn[cn] > 15.0f) cd->turn[cn] = 15.0f;
}

double career_power_factor(int32_t car, const int32_t sp[CS_N], int32_t level) {
  // The player's power handicap (Madness.js:1573-1594): 0.76 of full, less
  // so with acceleration points.
  int32_t accelboost = sp[CS_ACC] - (level - 1) - cheat_accel(car);
  if (accelboost > 75) accelboost = 75;
  return 0.76 + accelboost * 0.24 / 75.0;
}

/** xtGraphics.maxlevel: the opponents' top level on each stage. */
static const int32_t kMaxLevel[CAREER_STAGES] = {4,  7,  10, 13, 16, 20, 23, 26, 30,  33,  36,  39,  42,  45,  49, 52,
                                                 55, 58, 62, 65, 70, 74, 77, 84, 90, 97, 103, 111, 120, 125, 125};

static int32_t me_car(const CareerRace *r) { return r->sc[0]; }

/** The stage a level belongs to, capped at the one reached (the
 * `actualstage` loops, XT 4033-4045 and 5121-5133). */
static int32_t actual_stage(const CareerRace *r, const CareerSave *s) {
  const int32_t lp = s->level[me_car(r)], ll = r->level[r->nplayers - 1];
  const int32_t whichlevel = ll < lp ? ll : lp;
  int32_t st = 1;
  for (int32_t a = 1; a < 31; a++) {
    if (a < 30) {
      if (whichlevel >= kMaxLevel[a - 1] && whichlevel < kMaxLevel[a]) st = a;
    } else if (whichlevel >= kMaxLevel[a - 1]) {
      st = a;
    }
  }
  return st > s->unlocked ? s->unlocked : st;
}

void career_run_start(CareerRun *run, const CareerRace *r, const CareerSave *s) {
  memset(run, 0, sizeof(*run));
  run->startinglevel = s->level[me_car(r)];
  run->expmult = 1.0;
  run->expneeded = career_reqneed(s->level[me_car(r)], me_car(r));
  // XT 13866-13869, and the popups' resting place (5672-5675).
  run->winchance[1] = run->killchance[1] = 1001;
  run->pop_x[0] = run->pop_x[1] = -55;
  for (int32_t car = 0; car < CAREER_CARS; car++) {
    for (int32_t a = 0; a < CAREER_PERK_SLOTS; a++) run->perks.special[car][career_statsalc[car][a]][a] = (int8_t)s->perk[car][a];
  }
  for (int32_t i = 0; i < CAREER_MAX_PLAYERS; i++) {
    run->perks.car[i] = r->sc[i];
    run->perks.beast[i] = r->beast[i];
    run->perks.shadow[i] = r->shadow[i];
  }
}

/** XT 5038-5061 / 5333-5356: the level below which a car rolls 1.3 times as often. */
static int32_t level_barrier(int32_t car) {
  int32_t lb = (car - 7) * 5;
  if (car <= 7 || (car >= 23 && car <= 30)) lb = 0;
  if (car == 20) lb = 55;
  if (car == 21) lb = 60;
  if (car == 22) lb = 70;
  if (car >= 31 && car <= 33) lb = 15;
  if (car >= 34 && car <= 36) lb = 30;
  if (car == 37 || car == 38) lb = 40;
  return lb;
}

/** XT 5072-5091 / 5361-5380: a car past level 10 with few bonus points so far
 * rolls 1.5 times as often. */
static double pls_help(const CareerSave *s, int32_t car) {
  const int32_t lv = s->level[car], ep = s->extpoints[car];
  double plshelp = 1.0;
  if (lv >= 10) {
    if ((car < 8 || (car >= 23 && car <= 30)) && ep < (int32_t)(lv * 0.5)) plshelp = 1.5;
    if ((car == 8 || car == 9 || car == 10) && ep < lv) plshelp = 1.5;
    if ((car == 11 || car == 12 || (car >= 31 && car <= 35)) && ep < (int32_t)(lv * 1.5)) plshelp = 1.5;
    if (((car >= 13 && car <= 17) || (car >= 36 && car <= 38)) && ep < lv * 2) plshelp = 1.5;
    if ((car == 18 || car == 20 || car == 21) && ep < (int32_t)(lv * 2.5)) plshelp = 1.5;
    if ((car == 19 || car == 22) && ep < lv * 3) plshelp = 1.5;
  }
  return plshelp;
}

/** whichlevel3/4 (XT 5099-5102, 5416-5419): the level the soft cap is held to. */
static int32_t roll_level(const CareerRun *run, const CareerRace *r, const CareerSave *s) {
  const int32_t me = me_car(r);
  if ((r->stage < s->unlocked || r->bonus) && s->unlocked >= 2 && s->level[me] >= kMaxLevel[s->unlocked - 2] + 5)
    return s->level[me];
  return run->startinglevel;
}

static int32_t roll1000(void) { return (int32_t)(career_random() * 1000.0) + 1; }

void career_tick(CareerRun *run, const CareerRace *r, CareerSave *s, bool started, bool full_power) {
  const int32_t me = me_car(r);
  // careermode$m (XT 7568-7582): experience halves past the field's level.
  int32_t whichlevel = run->startinglevel;
  if ((r->stage < s->unlocked || r->bonus) && s->unlocked >= 2 && s->level[me] >= kMaxLevel[s->unlocked - 2] + 5)
    whichlevel = s->level[me];
  if (whichlevel >= r->softlevelcap) {
    int32_t leveldiff = run->startinglevel - r->softlevelcap;
    if (leveldiff > 5) leveldiff = 5;
    run->expmult = 0.5 - leveldiff * 0.05;
  } else {
    run->expmult = 1.0;
  }
  if ((r->hardstage || r->stage == s->unlocked || r->bonus) && s->level[me] < r->averagelevel - 2) run->isithard = true;
  if (!started) {
    run->startexp = s->exp[me];
    run->startsp = s->statpoints[me];
  }
  // Full power (XT 9086-9108).
  if (full_power && !run->noexp) {
    const double lvmulti = (s->level[me] * 0.1 + 1.0) * run->expmult;
    run->fullpownit += lvmulti;
    run->powxpadjust += (int32_t)lvmulti;
    int32_t extraxp = (int32_t)run->fullpownit - run->powxpadjust;
    s->exp[me] += (int32_t)lvmulti + extraxp;
    if (extraxp > 0) run->powxpadjust = (int32_t)run->fullpownit;
  }
  // No experience past the stage's cap on the newest stage (XT 9109-9111).
  if (r->nolevels || (r->stage == s->unlocked && s->unlocked > 1 && s->unlocked < CAREER_STAGES &&
                      s->level[me] >= kMaxLevel[s->unlocked] && !r->bonus))
    run->noexp = true;
  // Level up (XT 9112-9143).
  if (s->exp[me] > run->expneeded && started && !run->noexp) {
    s->exp[me] -= run->expneeded;
    s->level[me]++;
    s->statpoints[me] += 4 + (int32_t)(s->level[me] / 15.0);
    run->startexp = 0;
    run->startsp = s->statpoints[me];
    run->statgain = 0;
    for (int32_t k = 0; k < CS_N; k++) s->sp[me][k]++;
    run->levelups++;
  }
  if (s->exp[me] < 0) s->exp[me] = 0;
  run->expneeded = career_reqneed(s->level[me], me);
}

void career_xp_checkpoint(CareerRun *run, const CareerRace *r, CareerSave *s, int32_t clear0) {
  // stat$m XT 5021-5113: the counter, the bonus stat point roll (GAMBLER
  // raises its chance), the car's counter; then the experience, 5121-5175
  // (GREED raises it).
  const int32_t me = me_car(r);
  s->wins++;
  run->rcestatgain = false;
  {
    const int32_t chlimit = clear0 > 25 ? 25 : clear0;
    run->winchance[0] = !run->noexp ? roll1000() : 1000000;
    const double doublechance = s->level[me] <= level_barrier(me) ? 1.3 : 1.0;
    int32_t leveldiff = r->averagelevel - run->startinglevel;
    if (leveldiff > 40) leveldiff = 40;
    const int32_t bspbuff = run->isithard ? leveldiff : 0;
    const double chkproba =
        (double)(18 + chlimit + bspbuff) * doublechance * pls_help(s, me) * career_perk_mod(s, me, 1);
    if (run->winchance[0] <= (int32_t)chkproba && roll_level(run, r, s) < r->softlevelcap)
      run->winchance[1] = roll1000();
    else
      run->winchance[1] = 1001;
  }
  if (!run->noexp) s->winscn[me]++;
  const int32_t st = actual_stage(r, s);
  int32_t clearlimit = clear0;
  if (clearlimit > 25 + (int32_t)(st * 0.5)) clearlimit = 25 + (int32_t)(st * 0.5);
  const int32_t base = 67 + st * 7 + clearlimit * 6;
  int32_t stagehigh = st - 10;
  if (stagehigh < 0) stagehigh = 0;
  const double bonusdiff = 1.0 + stagehigh * 0.06;
  double winlimit = s->winscn[me], newlimit = 1.0, extranerf = 1.0;
  if (run->expmult < 1.0) {
    extranerf = 0.325;
    newlimit = 1.0 / (run->expmult * 4.0);
    if (newlimit > 1.0) newlimit = 1.0;
  }
  if (winlimit > 5000.0 / newlimit) winlimit = 5000.0 / newlimit;
  double killslimit = s->killscn[me] * 4.0;
  if (killslimit > 4800.0 / newlimit) killslimit = 4800.0 / newlimit;
  const double mult = 1.0 + (winlimit + killslimit) / 200.0;
  run->last_gain = (int32_t)(base * mult * bonusdiff * career_perk_mod(s, me, 2) * run->expmult * extranerf);
  if (!run->noexp) s->exp[me] += run->last_gain;
}

void career_xp_waste(CareerRun *run, const CareerRace *r, CareerSave *s, int32_t k) {
  // stat$m XT 5241-5297: the experience, from the counters as they stood
  // before this waste (GREED raises it; BERSERK and SAFETY start their
  // clocks); then the counters and the bonus stat point roll, 5318-5426.
  const int32_t me = me_car(r);
  int32_t cpn = r->sc[k];
  if (cpn <= 7) cpn = 0;
  if (cpn >= 23) cpn -= 23;
  int32_t stopcap = r->level[k];
  if (stopcap > s->level[me]) stopcap = s->level[me];
  const int32_t base = 300 + cpn * 9 + stopcap * 37;
  double newlimit = 1.0;
  if (run->expmult < 1.0) {
    newlimit = 1.0 / (run->expmult * 4.0);
    if (newlimit > 1.0) newlimit = 1.0;
  }
  double kilimit = s->killscn[me];
  if (kilimit > 1200.0 * newlimit) kilimit = 1200.0 * newlimit;
  double winslimit = s->winscn[me] / 4.0;
  if (winslimit > 1250.0 * newlimit) winslimit = 1250.0 * newlimit;
  double mult = 1.0 + (kilimit + winslimit) / 120.0;
  double chancemod = 1.0;
  if (r->beast[k]) {
    mult *= r->bonus ? 1.2 : 2.5;
    chancemod = r->bonus ? 1.3 : 2.55;
  }
  if (r->shadow[k]) {
    mult = (1.0 + (kilimit + winslimit) / 120.0) * 3.0;
    chancemod = 2.8;
  }
  for (int32_t a = 0; a < CAREER_PERK_SLOTS; a++) {
    if (save_special(s, me, 23, a) > 0) run->perks.killtime[0] = 75 + (int32_t)(save_special(s, me, 23, a) * 3.75);
    if (save_special(s, me, 24, a) > 0) run->perks.killtime[1] = 75 + (int32_t)(save_special(s, me, 24, a) * 3.75);
  }
  run->last_gain = (int32_t)(base * mult * 0.85 * career_perk_mod(s, me, 2) * run->expmult);
  if (!run->noexp) s->exp[me] += run->last_gain;

  s->kills++;
  if (!run->noexp) s->killscn[me]++;
  run->killchance[0] = !run->noexp ? roll1000() : 1000000;
  const double doublechance = s->level[me] <= level_barrier(me) ? 1.3 : 1.0;
  double levelboost = 1.0;
  int32_t leveldiff = r->level[k] - s->level[me];
  if (leveldiff > 40) leveldiff = 40;
  if (!run->isithard) leveldiff = 0;
  if (leveldiff > 2) {
    int32_t bspbuff = 5;
    if (leveldiff >= 5 && leveldiff <= 10) bspbuff = leveldiff;
    if (leveldiff > 10 && leveldiff <= 20) bspbuff = 10 + (leveldiff - 10) * 2;
    if (leveldiff > 20 && leveldiff <= 30) bspbuff = 30 + (leveldiff - 20) * 3;
    if (leveldiff > 30 && leveldiff <= 40) bspbuff = 60 + (leveldiff - 30) * 4;
    levelboost = bspbuff / 100.0 + 1.0;
  }
  const int32_t totalchance =
      (int32_t)(140.0 * doublechance * chancemod * pls_help(s, me) * career_perk_mod(s, me, 1) * levelboost);
  if (run->killchance[0] <= totalchance && roll_level(run, r, s) < r->softlevelcap)
    run->killchance[1] = roll1000();
  else
    run->killchance[1] = 1001;
}

/** One popup's slide (XT 5603-5627): in 8 a tick to x 15, 30 ticks there,
 * back out. True when it has gone (x at or past `gone`). */
static bool pop_slide(CareerRun *run, int32_t i, int32_t gone) {
  if (run->pop_x[i] < 15 && !run->pop_back[i]) run->pop_x[i] += 8;
  if (run->pop_x[i] >= 15) run->pop_show[i]++;
  run->pop_back[i] = run->pop_show[i] >= 30;
  if (run->pop_back[i]) run->pop_x[i] -= 8;
  if (run->pop_x[i] <= gone) {
    run->pop_back[i] = false;
    return true;
  }
  return false;
}

static void pop_rest(CareerRun *run, int32_t i) {
  run->pop_show[i] = 0;
  run->pop_x[i] = -55;
  run->pop_back[i] = false;
  run->pop_amount[i] = 0;
}

/** The points a popup's roll is worth (XT 5631-5667): 4, 2 or 1. */
static int32_t pop_points(int32_t chance, double morechance) {
  if (chance <= (int32_t)((0.05 + 0.2 * morechance) * 1000.0)) return 4;
  if (chance <= (int32_t)((0.3 + morechance) * 1000.0)) return 2;
  return 1;
}

static void pop_award(CareerRun *run, CareerSave *s, int32_t me, int32_t points) {
  s->statpoints[me] += points;
  s->extpoints[me] += points;
  run->statgain += points;
}

void career_popups_tick(CareerRun *run, const CareerRace *r, CareerSave *s) {
  // stat$m's HUD half (XT 5585-5835), as it draws each tick: a popup pays
  // as it shows, once; the waste's will not pay again until it has gone
  // (wststatgain clears only at rest), a checkpoint clears its own.
  const int32_t me = me_car(r);
  int32_t leveldiff = r->averagelevel - run->startinglevel;
  if (leveldiff > 50) leveldiff = 50;
  if (leveldiff < 5) leveldiff = 5;
  const double morechance = run->isithard ? leveldiff / 100.0 : 0.0;
  if (run->killchance[1] <= 1000) {
    if (pop_slide(run, 0, -340)) run->killchance[1] = 1001;
    if (run->killchance[1] <= 1000) {
      run->pop_amount[0] = pop_points(run->killchance[1], morechance);
      if (!run->wststatgain && !run->losepoints) {
        pop_award(run, s, me, run->pop_amount[0]);
        run->wststatgain = true;
      }
    }
  } else {
    pop_rest(run, 0);
    run->wststatgain = false;
  }
  if (run->winchance[1] <= 1000) {
    if (pop_slide(run, 1, -400)) run->winchance[1] = 1001;
    if (run->winchance[1] <= 1000) {
      run->pop_amount[1] = pop_points(run->winchance[1], morechance);
      if (!run->rcestatgain && !run->losepoints) {
        pop_award(run, s, me, run->pop_amount[1]);
        run->rcestatgain = true;
      }
    }
  } else {
    pop_rest(run, 1);
  }
}

void career_xp_win(CareerRun *run, const CareerRace *r, CareerSave *s, int32_t nsp, int32_t nlaps, bool racing) {
  // XT 4026-4078 (by wasting) and 4146-4198 (by racing).
  if (run->winfix) return;
  const int32_t me = me_car(r);
  const int32_t st = actual_stage(r, s);
  const int32_t baseline = 217 + st * 7;
  double newlimit = 1.0 / (run->expmult * 4.0);
  if (newlimit > 1.0) newlimit = 1.0;
  double winlimit, killslimit;
  if (racing) {
    winlimit = s->winscn[me] * 1.5;
    if (winlimit > 7500.0 * newlimit) winlimit = 7500.0 * newlimit;
    killslimit = s->killscn[me] * 2.0;
    if (killslimit > 2400.0 * newlimit) killslimit = 4800.0 * newlimit;   // sic, XT 4180-4182
  } else {
    winlimit = s->winscn[me] / 2.0;
    if (winlimit > 2500.0 * newlimit) winlimit = 2500.0 * newlimit;
    killslimit = s->killscn[me] * 6.0;
    if (killslimit > 7200.0 * newlimit) killslimit = 7200.0 * newlimit;
  }
  const double mult = 1.0 + (winlimit + killslimit) / 200.0;
  int32_t chk = (int32_t)(nsp * nlaps / 2.0);
  if (chk > 25 + (int32_t)(st * 0.5)) chk = 25 + (int32_t)(st * 0.5);
  double fakemult = run->expmult;
  if (run->isithard && fakemult > 0.25) fakemult = 0.25;
  run->last_gain = (int32_t)(baseline * mult * chk * fakemult * career_perk_mod(s, me, 2) * 0.325);
  if (!run->noexp) s->exp[me] += run->last_gain;
  run->winfix = true;
}

void career_xp_lose(CareerRun *run, const CareerRace *r, CareerSave *s) {
  // Losing the newest stage takes back what the race gave (XT 4104-4112).
  const int32_t me = me_car(r);
  if ((r->stage == s->unlocked && !r->bonus && s->unlocked > 1) || r->stage == 20) {
    s->exp[me] = run->startexp;
    s->statpoints[me] = run->startsp;
    if (!run->losepoints) {
      s->extpoints[me] -= run->statgain;
      run->losepoints = true;
    }
  }
}

void career_xp_stunt(CareerRun *run, const CareerRace *r, CareerSave *s, float powerup, int32_t stat3) {
  // Madness.js:2848-2860: the player's landed stunt, by its stunt stat.
  if (run->noexp) return;
  const double stumultiplier = stat3 / 50.0;
  s->exp[me_car(r)] += (int32_t)(powerup * stumultiplier * career_perk_mod(s, me_car(r), 2) * run->expmult);
}

int32_t career_stunt_stat(const CarDefine *cd, int32_t cn, const int32_t sp[CS_N]) {
  // getstats$1's stunts line (XT 18318), on the car's own values.
  const float stunts =
      ((float)(cd->airc[cn] + sp[CS_STU]) + (cd->airs[cn] + (float)sp[CS_STU] * 0.025f) * 10.0f) / 125.0f;
  return (int32_t)(stunts * 100.0f);
}

void career_grid(const CareerRace *r, const CareerSave *s, int32_t j, int32_t grat, int32_t *x, int32_t *y, int32_t *z,
                 int32_t *floor) {
  // GameSparker.loadstage's start grid (GameSparker.js 1270-1481), the
  // career's: rows of three, the stages' own layouts ("specialar"), and the
  // field moved back on 3, 6, 17 and 19.
  const int32_t n = r->nplayers, st = r->stage;
  const bool hard = s->unlocked == st || r->hardstage;
  int32_t moveback = 0;
  if (st == 17) moveback = j >= 4 ? 760 : (j > 0 ? 100000 : 0);
  if (st == 19) moveback = 760;
  if (st == 3) moveback = 3040;
  if (st == 6) moveback = -16800;
  const bool specialar = ((st == 5 || st == 11) && !r->bonus) || st == 13 || st == 14 || st == 20 || st == 21 ||
                         (st == 23 && hard) || (r->bonus == 4 && j > 0);
  *floor = 0;
  int32_t ybase = 250;   // Medium.ground
  *x = 0;
  *z = 0;
  if (specialar) {
    if (st == 5 && !r->bonus) {
      if (j < n - 1) {
        static const int32_t kX[3] = {0, -350, 350};
        *x = kX[j % 3];
        *z = (j % 3 == 0 ? 760 : 1140) + (j / 3) * 760;
      } else {
        *z = 2280;
      }
    }
    if (st == 11 && r->bonus != 2) {
      if (j >= 1 && j <= 4) {
        *z = -500000;   // the undead vans wait off the map
      } else if (j > 0) {
        const int32_t off = j - 4;
        if (j < n - 1) {
          static const int32_t kX[3] = {0, -350, 350};
          *x = kX[off % 3];
          *z = (off % 3 == 0 ? -760 : -380) + (off / 3) * 760;
        } else {
          *z = -760 + (off / 3) * 760;
        }
      } else {
        *z = -760;
      }
    }
    if (st == 14) {
      if (j < n - 3) {
        if (j == 0) *z = 0;
        if (j == n - 4) { *x = -350; *z = -38000; }
        if (j == n - 5) { *x = 350; *z = -38000; }
      } else {
        if (j == n - 1) *z = 760;
        if (j == n - 2) { *x = -350; *z = -380; }
        if (j == n - 3) { *x = 350; *z = -380; }
      }
    }
    if (r->bonus == 4) {
      *x = 100000;
      *z = 100000;
    }
    if (st == 20) {
      ybase = -20000;
      *z = 760;
    }
    if (st == 13) {
      if (j == 0 || j >= 10) {
        *floor = 3;
        const int32_t off = j == 0 ? 0 : j - 9;
        ybase = *floor * -10000;
        if (j == n - 1) {
          *z = (off / 3) * 760;
        } else {
          static const int32_t kX[3] = {0, -350, 350};
          *x = kX[j % 3];
          *z = (j % 3 == 0 ? -380 : 0) + (off / 3) * 760;
        }
      } else {
        *floor = (j - 1) / 3;   // the floor guardians wait on their floors
        ybase = *floor * -10000;
        *x = -10000;
        *z = j * 5000;
      }
    }
    if (st == 21) {
      ybase = -293500;
      if (j == n - 1) {
        *z = (j / 3) * 760 - 202000;
      } else {
        static const int32_t kX[3] = {0, -350, 350};
        *x = kX[j % 3];
        *z = (j % 3 == 0 ? -760 : -380) + (j / 3) * 760 - 202000;
      }
    }
    if (st == 23 && hard) {
      if (j == n - 1) {
        *z = ((j - 1) / 3) * 760;
      } else if (j > 1) {
        static const int32_t kX[3] = {350, 0, -350};   // (j % 3) 0, 1, 2
        *x = kX[j % 3];
        *z = (j % 3 == 1 ? -760 : -380) + ((j - 1) / 3) * 760;
      } else {
        *z = j == 0 ? -760 : -150000;   // the Titan waits off the map
      }
    }
    *y = ybase - grat;
    return;
  }
  if (j < n - 1 || n % 3 == 1) {
    static const int32_t kX[3] = {0, -350, 350};
    *x = kX[j % 3];
    *z = (j % 3 == 0 ? -760 : -380) + (j / 3) * 760 - moveback;
    *y = (r->bonus == 2 && r->beast[j]) ? -5000 : ybase - grat;   // bonus 2's beasts drop in
  } else {
    *z = (j / 3) * 760 - moveback;   // a last car out of a row of three, centred behind
    *y = ybase - grat;
  }
}
