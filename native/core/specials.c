// See specials.h. Conditions are written against Extended's car numbers
// (specials_ext_car) so they read as the Java does; `e` below is always
// the Extended number of a car's model.
#include "specials.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "career_perks.h"
#include "java_compat.h"
#include "new_cars.h"

int32_t specials_ext_car(int32_t cn) {
  cn = car_identity(cn);   // a new car takes its donor's special
  return cn < 16 ? cn + 23 : (cn < 39 ? cn - 16 : -1);
}

void specials_reset(Specials *sp) {
  memset(sp, 0, sizeof(*sp));
  for (int32_t a = 0; a < SPECIALS_MAX; a++) sp->healthmulti[a] = 1.0f;
}

/** xtGraphics.randomise (:9974): picks the target once, then keeps it while
 * it lives. Outside career there are no undead cars to skip. */
static void randomise(Specials *sp, Mad *mads, int32_t nplayers, int32_t x) {
  if (!sp->doitonce[x]) {
    sp->okdale[x] = (int32_t)(nfm_random() * (double)nplayers);
    sp->doitonce[x] = !(sp->okdale[x] == x || mads[sp->okdale[x]].dest);
  } else {
    sp->randomcar[x] = sp->okdale[x];
    const bool gone = mads[sp->randomcar[x]].dest;
    sp->doitonce[x] = !gone;
    sp->affected[x] = !gone;
  }
}

/** xtGraphics.sortpower (:7395), outside career: how strongly `a`'s attack
 * lands on `b`, from the two cars' grip. */
static void sortpower(Specials *sp, Mad *mads, int32_t a, int32_t b) {
  const float ga = mads[a].cd->grip[mads[a].cn], gb = mads[b].cd->grip[mads[b].cn];
  const float mainboistat = (ga - 10.0f) / 20.0f, targetstat = (gb - 10.0f) / 20.0f;
  const float diffmod = 50.0f;
  if (ga >= gb) {
    const float difference = (mainboistat - targetstat) * 37.0f;
    sp->specpower[a] = (double)(difference / diffmod) + 1.0;
  } else {
    const float difference = (targetstat - mainboistat) * 37.0f;
    sp->specpower[a] = 1.0 - (double)(difference / diffmod);
    if (sp->specpower[a] < 0.15) sp->specpower[a] = 0.15;
  }
}

static bool any_of(int32_t e, const int32_t *list, int32_t n) {
  for (int32_t i = 0; i < n; i++) if (list[i] == e) return true;
  return false;
}
#define IS(e, ...) any_of((e), (const int32_t[]){__VA_ARGS__}, (int32_t)(sizeof((int32_t[]){__VA_ARGS__}) / sizeof(int32_t)))

/** The five status timers and their stacking on the left (:6213-6370). */
static void status_lines(Specials *sp, Mad *mads, int32_t nplayers) {
  for (int32_t a = 0; a < nplayers; a++) {
    const bool cond[5] = {sp->fixspecials[a], mads[a].frozen, mads[a].strswap, mads[a].redstr, mads[a].leech};
    for (int32_t k = 0; k < 5; k++) {
      if (cond[k]) {
        sp->timershown[k][a]++;
        sp->newtimer[k][a] = sp->timershown[k][a] >= 100;
      } else {
        sp->timershown[k][a] = 0;
        sp->newtimer[k][a] = false;
      }
    }
    for (int32_t b = 0; b < nplayers; b++) {
      if (a == b) continue;
      for (int32_t k = 0; k < 5; k++) sp->over[k][a] = cond[k] && !sp->newtimer[k][a];
      for (int32_t c = 0; c < 5; c++) {
        for (int32_t d = 0; d < 5; d++) {
          if (c == d) continue;
          if (sp->over[c][a]) {
            if (sp->over[c][b]) {
              if (sp->q[c][a] == sp->q[c][b]) {
                if (sp->timershown[c][a] >= sp->timershown[c][b]) sp->q[c][a]++;
                else sp->q[c][b]++;
              }
            } else {
              sp->q[c][b] = 0;
            }
            if (sp->over[d][a]) {
              if (sp->q[c][a] == sp->q[d][a]) {
                if (sp->timershown[c][a] >= sp->timershown[d][a]) sp->q[c][a]++;
                else sp->q[d][a]++;
              }
            } else {
              sp->q[d][a] = 0;
            }
            if (sp->over[d][b]) {
              if (sp->q[c][a] == sp->q[d][b]) {
                if (sp->timershown[c][a] >= sp->timershown[d][b]) sp->q[c][a]++;
                else sp->q[d][b]++;
              }
            } else {
              sp->q[d][b] = 0;
            }
          } else {
            sp->q[c][a] = 0;
          }
        }
      }
    }
    if (!mads[a].dest) {
      for (int32_t h = 0; h < 5; h++) {
        if (!sp->over[h][a]) continue;
        sp->xm[h][a] = 91 + 18 * sp->q[h][a];
        sp->xfade += sp->xfadephase ? 12 : -12;
        if (sp->xfade >= 240) sp->xfadephase = false;
        if (sp->xfade <= 40) sp->xfadephase = true;
      }
    }
  }
}

void specials_tick(Specials *sp, Mad *mads, Control *controls, int32_t nplayers, CheckPoints *cp,
                   const CarDefine *base_all, const CarDefine *slot_bases) {
  if (nplayers > SPECIALS_MAX) nplayers = SPECIALS_MAX;
  for (int32_t a = 0; a < nplayers; a++) {
    if (mads[a].dest) {
      sp->fixspecials[a] = false;
      mads[a].specialact = false;
      mads[a].frozen = mads[a].strswap = mads[a].leech = mads[a].redstr = false;
    }
  }
  status_lines(sp, mads, nplayers);

  // Who has a special running (:6487-6497).
  for (int32_t c = 0; c < nplayers; c++) {
    if (mads[c].specend) {
      // Madness 3143: a spent special is over (both bars refill the same
      // tick, so the speclast2 test below never sees it reach 0).
      sp->fixspecials[c] = false;
      mads[c].specend = false;
    }
    if (mads[c].specialact) sp->fixspecials[c] = true;
    if (sp->fixspecials[c] && mads[c].speclast2 == 0.0f) sp->fixspecials[c] = false;
    if (sp->fixspecials[c] && !mads[c].specialact) mads[c].specialact = true;
  }
  // The AI fires as soon as its bar is full (:6502-6520); the player when
  // it presses the button with a full bar (:6522-6527).
  for (int32_t ai = 1; ai < nplayers; ai++) {
    Mad *md = &mads[ai];
    controls[ai].spatk = md->spatk == 120.0f && md->speclast > 0.0f;
    if ((md->spatk == 120.0f && md->speclast > 0.0f) ||
        (md->spatk == 120.0f && md->speclast != 120.0f && md->speclast > 0.0f)) {
      md->specialact = true;
    }
    if (md->spatk == 120.0f && md->speclast != 0.0f) continue;
    md->specialact = false;
  }
  Mad *you = &mads[0];
  if ((you->spatk == 120.0f && controls[0].spatk && you->speclast > 0.0f) ||
      (you->spatk == 120.0f && you->speclast != 120.0f && you->speclast > 0.0f)) {
    you->specialact = true;
  }
  if (you->spatk != 120.0f || you->speclast == 0.0f ||
      (you->spatk == 120.0f && you->speclast == 120.0f && !controls[0].spatk)) {
    you->specialact = false;
  }

  // Two cars left: no attacks, only boosts (:6550-6557).
  const bool nodebuff = cp->wasted >= nplayers - 2;

  for (int32_t a2 = 0; a2 < nplayers; a2++) {
    Mad *me = &mads[a2];
    const int32_t e = specials_ext_car(me->cn);
    if (sp->fixspecials[a2] && !me->dest) {
      if (IS(e, 1, 24, 2, 25, 9, 32, 12, 35, 16, 17, 22)) me->power = 98.0f;   // unlimited power
      if (IS(e, 0, 23, 7, 30, 2, 25, 20, 10, 33, 13, 14, 37, 36, 15, 17, 18, 19, 22, 38) && !sp->fixhealth[a2][0]) {
        sp->updatehealth[a2] = true;
        sp->proportion[a2] = (float)me->hitmag / (float)me->cd->maxmag[me->cn];
        sp->fixhealth[a2][0] = true;
      }
      // Reduced speed on a random car (:6568-6591).
      if (IS(e, 0, 23, 9, 32, 14, 37, 19)) {
        if (!nodebuff) {
          randomise(sp, mads, nplayers, a2);
          const int32_t t = sp->randomcar[a2];
          if (sp->affected[a2] && !mads[t].frozen) {
            if (cp->pos[t] > 0) {
              sortpower(sp, mads, a2, t);
              double speedmod;
              if (sp->specpower[a2] < 1.0) speedmod = 0.01 + 0.2823529411764706 * (sp->specpower[a2] - 0.15);
              else speedmod = 0.25 * ((sp->specpower[a2] - 1.0) / 1.25 + 1.0);
              if (speedmod > 0.45) speedmod = 0.45;
              speedmod *= career_perk_debuff(sp->perks, a2, t);   // DEBUFF / RESISTANCE
              sp->statreduce[t][0] = 1.0 - speedmod;
              mads[t].frozen = true;
            } else {
              sp->affected[a2] = false;
              sp->doitonce[a2] = false;
            }
          }
        }
        sp->finalfix[0][a2] = true;
      }
      // Swapped strength (:6592-6603).
      if (IS(e, 1, 24)) {
        if (!nodebuff) {
          randomise(sp, mads, nplayers, a2);
          const int32_t t = sp->randomcar[a2];
          if (sp->affected[a2]) {
            mads[t].strswap = true;
            sp->strswapee[t] = a2;
          }
          me->cd->moment[me->cn] = mads[t].strengthreduce;
          mads[t].cd->moment[mads[t].cn] = me->strengthreduce;
          sp->correct[a2] = true;
        }
        sp->finalfix[1][a2] = true;
      }
      // Drained health (:6604-6630).
      if (IS(e, 2, 25, 6, 29, 11, 34, 16)) {
        if (!nodebuff) {
          randomise(sp, mads, nplayers, a2);
          const int32_t t = sp->randomcar[a2];
          if (sp->affected[a2]) {
            sortpower(sp, mads, a2, t);
            mads[t].leech = true;
          }
          const double beastmod = 1.0;
          const double tmax = (double)mads[t].cd->maxmag[mads[t].cn];
          if (sp->specpower[a2] < 1.0) {
            sp->drainrate[a2] = tmax / (1500.0 * beastmod) * sp->specpower[a2];
          } else {
            double elrato = (sp->specpower[a2] - 1.0) * 1.5 + 1.0;
            if (elrato > 2.5 * beastmod) elrato = 2.5 * beastmod;
            sp->drainrate[a2] = tmax / (1500.0 * beastmod) * elrato;
          }
          sp->drainrate[a2] *= career_perk_debuff(sp->perks, a2, t);   // DEBUFF / RESISTANCE
          if (mads[t].hitmag < (int32_t)(0.9 * tmax)) mads[t].hitmag += (int32_t)sp->drainrate[a2];
        }
        sp->finalfix[3][a2] = true;
      }
      // Reduced defence: of the leader for some cars, a random car for the
      // others (:6631-6678).
      if (IS(e, 3, 26, 4, 27, 10, 33, 12, 35, 13, 36, 18, 20)) {
        if (!nodebuff) {
          if (IS(e, 3, 26, 12, 35)) {
            for (int32_t d2 = 0; d2 < nplayers; d2++) {
              if (a2 == d2) continue;
              if (cp->pos[a2] != 0) {
                if (cp->pos[d2] == 0 && !sp->slowonce[a2]) { sp->randomcar[a2] = d2; sp->slowonce[a2] = true; }
              } else if (cp->pos[d2] == 1 && !sp->slowonce[a2]) {
                sp->randomcar[a2] = d2;
                sp->slowonce[a2] = true;
              }
            }
            if (mads[sp->randomcar[a2]].dest) sp->slowonce[a2] = false;
            sp->affected[a2] = true;
          } else {
            randomise(sp, mads, nplayers, a2);
          }
          const int32_t t = sp->randomcar[a2];
          if (sp->affected[a2] && !mads[t].redstr) {
            sortpower(sp, mads, a2, t);
            if (!sp->fixhealth[t][1]) {
              if (sp->specpower[a2] < 1.0) sp->healthloss[t] = 0.01 + 0.3411764705882353 * (sp->specpower[a2] - 0.15);
              else sp->healthloss[t] = 0.3 * ((sp->specpower[a2] - 1.0) / 1.2 + 1.0);
              if (sp->healthloss[t] > 0.55) sp->healthloss[t] = 0.55;
              sp->healthloss[t] *= career_perk_debuff(sp->perks, a2, t);   // DEBUFF / RESISTANCE
              sp->statreduce[t][5] = sp->healthloss[t];
              sp->updatehealth[t] = true;
              sp->proportion[t] = (float)mads[t].hitmag / (float)mads[t].cd->maxmag[mads[t].cn];
              sp->fixhealth[t][1] = true;
            }
            mads[t].redstr = true;
          }
        }
        sp->finalfix[2][a2] = true;
      }
    } else {
      // The special ran out: lift what it did, unless another car's special
      // is still doing the same to the same target (:6680-6713).
      if (sp->fixhealth[a2][0]) {
        sp->proportion[a2] = (float)me->hitmag / (float)me->cd->maxmag[me->cn];
        sp->updatehealth[a2] = true;
        sp->fixhealth[a2][0] = false;
      }
      sp->correct[a2] = false;
      sp->slowonce[a2] = false;
      sp->doitonce[a2] = false;
      sp->affected[a2] = false;
      for (int32_t b3 = 0; b3 < nplayers; b3++) {
        if (a2 == b3) continue;
        const int32_t t = sp->randomcar[a2];
        bool lift[4];
        for (int32_t k = 0; k < 4; k++) {
          lift[k] = sp->finalfix[k][a2] &&
                    (!sp->fixspecials[b3] ||
                     (sp->fixspecials[b3] && (!sp->finalfix[k][b3] || (sp->finalfix[k][b3] && t != sp->randomcar[b3]))));
        }
        if (lift[0] && mads[t].frozen) { mads[t].frozen = false; sp->finalfix[0][a2] = false; }
        if (lift[1] && mads[t].strswap) { mads[t].strswap = false; sp->finalfix[1][a2] = false; }
        if (lift[2] && mads[t].redstr) {
          sp->healthloss[t] = 0.0;
          mads[t].redstr = false;
          if (sp->fixhealth[t][1]) {
            sp->updatehealth[t] = true;
            sp->proportion[t] = (float)mads[t].hitmag / (float)mads[t].cd->maxmag[mads[t].cn];
            sp->fixhealth[t][1] = false;
          }
          sp->finalfix[2][a2] = false;
        }
        if (lift[3] && mads[t].leech) { mads[t].leech = false; sp->finalfix[3][a2] = false; }
      }
    }
  }
  if (nodebuff) {
    for (int32_t a2 = 0; a2 < nplayers; a2++) {
      mads[a2].frozen = false;
      if (mads[a2].redstr) {
        mads[a2].redstr = false;
        if (sp->fixhealth[a2][1]) {
          sp->updatehealth[a2] = true;
          sp->proportion[a2] = (float)mads[a2].hitmag / (float)mads[a2].cd->maxmag[mads[a2].cn];
          sp->fixhealth[a2][1] = false;
        }
        sp->healthloss[a2] = 0.0;
      }
      mads[a2].leech = false;
      mads[a2].strswap = false;
    }
  }
  for (int32_t z = 0; z < nplayers; z++) {
    if (!mads[z].strswap && !sp->correct[z]) mads[z].strengthreduce = mads[z].cd->moment[mads[z].cn];
  }

  // Every car's live stats rebuilt from its own base ones (:6772-7085),
  // outside career and tourney: stataffect 0, so speed..endurance are the
  // base values themselves.
  for (int32_t a3 = 0; a3 < nplayers; a3++) {
    Mad *md = &mads[a3];
    CarDefine *live = md->cd;
    const CarDefine *base = slot_bases ? &slot_bases[a3] : base_all;
    const int32_t c = md->cn, e = specials_ext_car(c);
    const int32_t *bsw = base->swits[c];
    const float *bac = base->acelf[c];
    const double rufreeze = md->frozen ? sp->statreduce[a3][0] : 1.0;
    double spdmod = 1.0 - (1.0 - rufreeze);
    float strmod = 1.0f;
    double specialboost = 1.0;
    if (sp->perks && a3 == 0) {
      // The career's perks on the player (:6950-6973, 7079-7084): GETAWAY's
      // speed and RECKLESS's strength at 80% damage, FRESHNESS's speed and
      // STEROIDS's strength after a fix, RUTHLESS's stronger special.
      double spdboost, fixspd;
      float strboost, fixstr;
      const float health = ((float)md->hitmag / (float)live->maxmag[c]) * 100.0f;
      career_perk_specials(sp->perks, health, md->fixtime, &spdboost, &fixspd, &strboost, &fixstr, &specialboost);
      spdmod = ((1.0 + (spdboost - 1.0)) + (fixspd - 1.0)) - (1.0 - rufreeze);
      spdmod += career_perk_comeback(sp->perks, md->comebacktime) - 1.0;   // COMEBACK
      strmod = (1.0f + (fixstr - 1.0f)) + (strboost - 1.0f);
    }
    double spdspboost = 0.0;
    float strspboost = 0.0f;
    const int32_t speed2 = (int32_t)((double)bsw[2] * 1.0);
    if (!sp->fixspecials[a3]) {
      for (int32_t b = 0; b < 3; b++) live->acelf[c][b] = (float)((double)bac[b] * 1.0);
      live->grip[c] = (float)((double)base->grip[c] * 1.0);
      live->airs[c] = (float)((double)base->airs[c] * 1.0);
      live->airc[c] = (int32_t)((double)base->airc[c] * 1.0);
      sp->healthmulti[a3] = 1.0f;
    } else {
      const float contgrip = base->grip[c], statairs = base->airs[c];
      const int32_t statairc = base->airc[c];
      if (IS(e, 3, 8, 10, 15, 11)) spdspboost = 0.3 * specialboost;
      if (e == 26) spdspboost = 0.35 * specialboost;
      if (e == 12) spdspboost = 0.25 * specialboost;
      if (IS(e, 16, 17, 14, 37, 22, 6, 29, 1, 24)) spdspboost = 0.15 * specialboost;
      if (IS(e, 9, 32, 19)) spdspboost = 0.1 * specialboost;
      if (e == 20) spdspboost = -0.1 * specialboost;
      if (IS(e, 18, 31, 33, 35, 34, 38)) spdspboost = 0.2 * specialboost;
      if (IS(e, 5, 28)) spdspboost = (md->hitmag * 2 < live->maxmag[c] ? 0.25 : 0.5) * specialboost;
      float maxaccel2 = bac[0];
      if (e == 15) maxaccel2 = bac[0] * (1.0f + 0.3f * (float)specialboost);
      if (e == 38) maxaccel2 = bac[0] * (1.0f + 0.2f * (float)specialboost);
      if (e == 22) maxaccel2 = bac[0] * (1.0f + 1.0f * (float)specialboost);
      live->acelf[c][0] = maxaccel2;
      live->acelf[c][1] = bac[1] * maxaccel2 / bac[0];
      live->acelf[c][2] = bac[2] * maxaccel2 / bac[0];
      live->grip[c] = contgrip;
      live->airs[c] = statairs;
      live->airc[c] = statairc;
      // Control boosts: grip in "control" units, (grip - 10) * 5.
      static const struct { int32_t car; float k; } kGrip[] = {{8, 1.0f}, {31, 0.75f}, {16, 0.5f}, {15, 0.3f}, {38, 0.2f}};
      for (int32_t i = 0; i < 5; i++) {
        if (e == kGrip[i].car) {
          const float contstat = (contgrip - 10.0f) * 5.0f * (1.0f + kGrip[i].k * (float)specialboost);
          live->grip[c] = contstat * 0.2f + 10.0f;
        }
      }
      if (IS(e, 17, 7, 30, 37)) sp->healthmulti[a3] = 1.0f + 0.5f * (float)specialboost;
      if (IS(e, 0, 23, 36, 22, 33, 2, 25, 19)) sp->healthmulti[a3] = 1.0f + 0.3f * (float)specialboost;
      if (IS(e, 10, 13, 20)) sp->healthmulti[a3] = 1.0f + 0.4f * (float)specialboost;
      if (e == 14) sp->healthmulti[a3] = 1.0f + 0.7f * (float)specialboost;
      if (e == 18) sp->healthmulti[a3] = 1.0f + 0.25f * (float)specialboost;
      // Stunt boosts.
      static const struct { int32_t car; float kf; double kd; } kStunt[] = {
          {3, 0.75f, 0.75}, {26, 0.6f, 0.6}, {14, 1.0f, 1.0}, {15, 0.3f, 0.3}, {38, 0.2f, 0.2}};
      for (int32_t i = 0; i < 5; i++) {
        if (e == kStunt[i].car) {
          live->airs[c] = statairs * (1.0f + kStunt[i].kf * (float)specialboost);
          live->airc[c] = (int32_t)((double)statairc * (1.0 + kStunt[i].kd * specialboost));
        }
      }
      if (e == 15) sp->healthmulti[a3] = 1.0f + 0.3f * (float)specialboost;
      if (e == 38) sp->healthmulti[a3] = 1.0f + 0.2f * (float)specialboost;
      if (!md->strswap) {
        if (IS(e, 0, 6, 29)) strspboost = 0.5f * (float)specialboost;
        if (e == 23) strspboost = 0.55f * (float)specialboost;
        if (IS(e, 4, 27)) strspboost = 0.7f * (float)specialboost;
        if (IS(e, 20, 7, 30)) strspboost = 0.6f * (float)specialboost;
        if (e == 22) strspboost = 0.15f * (float)specialboost;
        if (IS(e, 19, 38)) strspboost = 0.2f * (float)specialboost;
        if (IS(e, 14, 32, 13, 11, 9, 31)) strspboost = 0.4f * (float)specialboost;
        if (IS(e, 5, 28)) strspboost = (md->hitmag * 2 < live->maxmag[c] ? 0.3f : 0.6f) * (float)specialboost;
        if (IS(e, 33, 15, 16, 34, 36)) strspboost = 0.3f * (float)specialboost;
        if (IS(e, 37, 10)) strspboost = 0.35f * (float)specialboost;
        if (IS(e, 2, 25, 8)) strspboost = 0.45f * (float)specialboost;
        if (e == 18) strspboost = 0.25f * (float)specialboost;
      }
    }
    const float totalhealth = sp->healthmulti[a3] - (float)sp->healthloss[a3];
    live->maxmag[c] = (int32_t)((float)base->maxmag[c] * totalhealth);
    if (sp->updatehealth[a3]) {
      md->hitmag = (int32_t)(sp->proportion[a3] * (float)live->maxmag[c]);
      sp->updatehealth[a3] = false;
    }
    const double totalspd = spdmod + spdspboost;
    const int32_t maxspeed2 = (int32_t)((double)speed2 * totalspd);
    live->swits[c][0] = bsw[0] * maxspeed2 / bsw[2];
    live->swits[c][1] = bsw[1] * maxspeed2 / bsw[2];
    live->swits[c][2] = maxspeed2;
    const float totalstr = strmod + strspboost;   // strmod is 1 but for the career's perks
    if (!md->strswap && !sp->correct[a3]) live->moment[c] = base->moment[c] * totalstr;

    // Each stat against the car's own, for the buff list (:7106-7146).
    const double origcont = ((double)base->grip[c] - 10.0) * 5.0;
    sp->statmod[a3][0] = round(100.0 * live->swits[c][2] / (double)bsw[2]);
    sp->statmod[a3][1] = round(100.0 * live->acelf[c][0] / (double)bac[0]);
    sp->statmod[a3][2] = round(100.0 * (((double)live->grip[c] - 10.0) * 5.0) / origcont);
    sp->statmod[a3][3] = round(100.0 * live->airs[c] / (double)base->airs[c]);
    sp->statmod[a3][4] = round(100.0 * live->moment[c] / (double)base->moment[c]);
    sp->statmod[a3][5] = round(100.0 * live->maxmag[c] / (double)base->maxmag[c]);
  }

  // Outline glow (:7214-7339): a car under conditions outlines in their
  // colours -- special red, frozen blue, drained brown, weakened orange,
  // swapped green -- fading from one to the next every 5 ticks.
  static const int32_t kCond[5][3] = {{185, 0, 0}, {0, 0, 185}, {100, 75, 0}, {240, 120, 0}, {0, 150, 0}};
  const int32_t glowfor = 5;
  for (int32_t a5 = 0; a5 < nplayers; a5++) {
    const bool cond[5] = {sp->fixspecials[a5], mads[a5].frozen, mads[a5].leech, mads[a5].redstr, mads[a5].strswap};
    int32_t ref[5][3], n = 0;
    for (int32_t k = 0; k < 5; k++) {
      if (!cond[k]) continue;
      for (int32_t ch = 0; ch < 3; ch++) ref[n][ch] = kCond[k][ch];
      n++;
    }
    sp->spec_on[a5] = n > 0 && !mads[a5].dest;
    if (n == 1) for (int32_t ch = 0; ch < 3; ch++) sp->spec[a5][ch] = ref[0][ch];
    if (n <= 1) {
      sp->spglow[a5] = 0;
      sp->spglowchange[a5] = 0;
      continue;
    }
    if (sp->spglow[a5] < glowfor) {
      sp->spglow[a5]++;
    } else {
      sp->spglow[a5] = 0;
      sp->spglowchange[a5] = sp->spglowchange[a5] < n - 1 ? sp->spglowchange[a5] + 1 : 0;
    }
    if (sp->spglowchange[a5] >= n) sp->spglowchange[a5] = 0;
    const int32_t from = sp->spglowchange[a5], to = from < n - 1 ? from + 1 : 0;
    for (int32_t ch = 0; ch < 3; ch++) {
      const int32_t step = abs(ref[from][ch] - ref[to][ch]) / glowfor;
      sp->spec[a5][ch] = ref[from][ch] > ref[to][ch] ? ref[from][ch] - sp->spglow[a5] * step
                                                       : ref[from][ch] + sp->spglow[a5] * step;
    }
  }
}

// The car select's description of each special (Extended's carselect, the
// web port's web/ext/specials.js), in Extended's order.
static const char *const kSpecialText[39][5] = {
  {"A random car gets reduced speed.", "Strength/Defence boost: 50%/30%"},
  {"15% speed boost.", "Swaps its strength with a random car.", "Unlimited power."},
  {"Unlimited power.", "Strength/Defence boost: 45%/30%", "Drains a random car's health."},
  {"Reduces the defence of the car in first.", "Speed/Stunting boost: 30%/75%"},
  {"70% strength boost.", "Reduces a random car's defence."},
  {"30% strength and 25% speed boost.", "These boosts double past 50% damage."},
  {"Strength/Speed boost: 50%/15%", "Drains a random car's health."},
  {"Strength/Defence boost: 60%/50%"},
  {"100% control boost.", "45% strength boost.", "30% speed boost."},
  {"A random car gets reduced speed.", "Strength/Speed boost: 40%/10%", "You get unlimited power."},
  {"30% speed boost.", "Strength/Defence boost: 35%/40%", "Reduces a random car's defence."},
  {"Strength/Speed boost: 40%/30%.", "Drains a random car's health."},
  {"25% speed boost.", "You get unlimited power.", "Reduces the defence of the car in first."},
  {"40% strength/defence boost.", "Reduces a random car's defence."},
  {"Speed/Stunting boost: 15%/100%.", "Strength/Defence boost: 40%/70%.", "A random car gets reduced speed."},
  {"Every stat increases by 30%."},
  {"Unlimited power.", "Strength/Speed boost: 30%/15%.", "Control boost: 50%", "Drains a random car's health."},
  {"Unlimited power.", "50% defence boost.", "15% speed boost."},
  {"20% speed boost.", "25% strength/defence boost.", "Reduces the defence of a random car."},
  {"Strength/Speed boost: 20%/10%.", "30% defence boost.", "Reduces the speed of a random car."},
  {"Strength/Defence boost: 60%/40%.", "10% speed cut.", "Reduces the defence of a random car."},
  {NULL},
  {"Strength/Speed boost: 15%.", "Defence boost: 30%.", "Acceleration boost: 100%.", "Unlimited power."},
  {"A random car gets reduced speed.", "Strength/Defence boost: 55%/30%"},
  {"15% speed boost.", "Swaps its strength with a random car.", "Unlimited power."},
  {"Unlimited power.", "Strength/Defence boost: 45%/30%", "Drains a random car's health."},
  {"Reduces the defence of the car in first.", "Speed/Stunting boost: 35%/60%"},
  {"70% strength boost.", "Reduces a random car's defence."},
  {"30% strength and 25% speed boost.", "These boosts double past 50% damage."},
  {"Strength/Speed boost: 50%/15%", "Drains a random car's health."},
  {"Strength/Defence boost: 60%/50%"},
  {"75% control boost.", "40% strength boost.", "20% speed boost."},
  {"A random car gets reduced speed.", "Strength/Speed boost: 35%/10%", "You get unlimited power."},
  {"20% speed/strength boost.", "Strength/Defence boost: 30%", "Reduces a random car's defence."},
  {"Strength/Speed boost: 30%/20%.", "Drains a random car's health."},
  {"20% speed boost.", "You get unlimited power.", "Reduces the defence of the car in first."},
  {"30% strength/defence boost.", "Reduces a random car's defence."},
  {"Speed boost: 15%", "Strength/Defence boost: 35%/50%.", "A random car gets reduced speed."},
  {"Every stat increases by 20%."},
};

const char *const *specials_describe(int32_t cn) {
  const int32_t e = specials_ext_car(cn);
  return (e >= 0 && e < 39) ? kSpecialText[e] : kSpecialText[21];
}
