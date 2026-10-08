// See career.h. The save: this port's own text file (career.txt beside the
// progress file), not the jar's byte-swapped savedata.radq -- the same
// fields (GameSparker.writedata, docs/extended-career.md "Save").
#include "career.h"

#include <stdio.h>
#include <string.h>

void career_reset(CareerSave *s) {
  memset(s, 0, sizeof(*s));
  s->unlocked = 1;
  s->laststage = 1;
  for (int32_t a = 0; a < CAREER_CARS; a++) s->level[a] = 1;
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
  if (fclose(f) != 0) return false;
  remove(path);   // rename() will not replace an existing file on every platform
  return rename(tmp, path) == 0;
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
}

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
  // XT 5121-5175 (and the cleared counter, 5108-5111).
  const int32_t me = me_car(r);
  if (!run->noexp) s->winscn[me]++;
  s->wins++;
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
  run->last_gain = (int32_t)(base * mult * bonusdiff * run->expmult * extranerf);
  if (!run->noexp) s->exp[me] += run->last_gain;
}

void career_xp_waste(CareerRun *run, const CareerRace *r, CareerSave *s, int32_t k) {
  // XT 5241-5297 and the wastes counter (5318-5326).
  const int32_t me = me_car(r);
  s->kills++;
  if (!run->noexp) s->killscn[me]++;
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
  if (r->beast[k]) mult *= r->bonus ? 1.2 : 2.5;
  if (r->shadow[k]) mult = (1.0 + (kilimit + winslimit) / 120.0) * 3.0;
  run->last_gain = (int32_t)(base * mult * 0.85 * run->expmult);
  if (!run->noexp) s->exp[me] += run->last_gain;
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
  run->last_gain = (int32_t)(baseline * mult * chk * fakemult * 0.325);
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
  s->exp[me_car(r)] += (int32_t)(powerup * stumultiplier * run->expmult);
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
