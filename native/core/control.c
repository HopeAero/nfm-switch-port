// ports web/Control.js -- see control.h for scope.
#include "control.h"
#include "java_compat.h"
#include "trackers.h"
#include "check_points.h"
#include "mad.h"
#include "cont_o.h"
#include "car_define.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

void control_init(Control *c, Medium *m) {
  memset(c, 0, sizeof(*c));
  c->skiplev = 1.0f;
  c->clrnce = 5;
  c->saftey = 30;
  c->mustland = 0.5f;
  c->trickprf = 0.5f;
  c->wall = -1;
  c->lwall = -1;
  c->m = m;
}

void control_falseo(Control *c, int32_t n) {
  c->left = false;
  c->right = false;
  c->up = false;
  c->down = false;
  c->handb = false;
  c->lookback = 0;
  c->enter = false;
  c->exit = false;
  if (n != 1) {
    c->radar = false;
    c->arrace = false;
    c->chatup = 0;
    if (n != 2) c->multion = 0;
    if (n != 3) {
      c->mutem = false;
      c->mutes = false;
    }
  }
}

int32_t control_py(int32_t n, int32_t n2, int32_t n3, int32_t n4) {
  return trackers_py(n, n2, n3, n4);
}

int32_t control_pys(int32_t n, int32_t n2, int32_t n3, int32_t n4) {
  return jtrunc_d(sqrt((double)trackers_py(n, n2, n3, n4)));
}

void control_reset(Control *c, CheckPoints *cp, int32_t n) {
  c->pan = 0;
  c->attack = 0;
  c->acr = 0;
  c->afta = false;
  c->trfix = 0;
  c->acuracy = 0;
  c->upwait = 0;
  c->forget = false;
  c->bulistc = false;
  c->runbul = 0;
  c->revstart = 0;
  c->oupnt = 0;
  c->gowait = false;
  c->apunch = 0;
  c->exitattack = false;

  if (cp->stage == 16 || cp->stage == 18) c->hold = 50;
  if (cp->stage == 17) c->hold = 10;
  if (cp->stage == 20) c->hold = 30;
  if (cp->stage == 21) {
    if (n != 13) {
      c->hold = 35;
      c->revstart = 25;
    } else {
      c->hold = 5;
    }
    c->statusque = 0;
  }
  if (cp->stage == 22) {
    if (n != 13) {
      // trunc(20.0 + 10.0*random()) -- no fr() at all, genuinely double.
      c->hold = jtrunc_d(20.0 + 10.0 * (double)medium_random(c->m));
      c->revstart = jtrunc_d(10.0 + 10.0 * (double)medium_random(c->m));
    } else {
      c->hold = 5;
    }
    c->statusque = 0;
  }
  if (cp->stage == 24) {
    c->hold = 30;
    c->statusque = 0;
    if (n != 14) c->revstart = 1;
  }
  if (cp->stage == 25) c->hold = 40;
  if (cp->stage == 26) c->hold = 20;

  if (cp->stage != 19 && cp->stage != 26) {
    for (int32_t i = 0; i < cp->fn; i++) {
      int32_t py = -10;
      for (int32_t j = 0; j < cp->n; j++) {
        int32_t d = control_py(cp->fx[i] / 100, cp->x[j] / 100, cp->fz[i] / 100, cp->z[j] / 100);
        if (d < py || py == -10) {
          py = d;
          c->fpnt[i] = j;
        }
      }
    }
    for (int32_t k = 0; k < cp->fn; k++) {
      c->fpnt[k] -= 4;
      if (c->fpnt[k] < 0) c->fpnt[k] += cp->nsp;
      // Control.java:267-271 stops at the single `+= nsp` above, which
      // only lands back in range when nsp >= 4. A stage with fewer
      // checkpoints than that leaves the index still negative -- nsp == 3
      // gives 0 - 4 + 3 == -1 -- and Control.java:1779 then evaluates
      // checkPoints.x[-1]. Java throws ArrayIndexOutOfBoundsException
      // there; C instead reads whatever precedes CheckPoints.x, which is
      // real memory corruption rather than a clean failure, and
      // AddressSanitizer flags it as a stack-buffer-overflow on an
      // ordinary race (CheckPoints is one of game_run()'s own locals).
      // Clamping only in the case the original would have thrown keeps
      // every index the original produced untouched.
      if (c->fpnt[k] < 0) c->fpnt[k] = 0;
    }
  } else {
    if (cp->stage == 19) {
      c->fpnt[0] = 14;
      c->fpnt[1] = 36;
    }
    if (cp->stage == 26) {
      c->fpnt[3] = 39;
    }
  }

  c->left = false;
  c->right = false;
  c->up = false;
  c->down = false;
  c->handb = false;
  c->lookback = 0;
  c->arrace = false;
  c->mutem = false;
  c->mutes = false;
}

// Math.atan(...)/DEG == atan's result converted from radians to degrees --
// the exact literal Java/JS use at every one of preform()'s bearing
// calculations (kept as a named double for readability without changing
// its value).
#define NFM_DEG 0.017453292519943295

// web/Control.js's own `Math.random() > Math.random()` idiom (JS/Java
// guarantee left-to-right operand evaluation; C doesn't) -- same fix
// bots.c's own rand_gt_rand() already applies, duplicated locally since
// preform() uses `this.m.random()` (Medium's correlated PRNG, NOT
// nfm_random() -- Control.java holds its own `this.m` reference and
// calls ITS random() method throughout, unlike xtGraphics.java's
// sortcars() which called raw Math.random(); see bots.h's doc comment on
// that distinction).
static bool control_rand_gt_rand(Medium *m) {
  float a = medium_random(m);
  float b = medium_random(m);
  return a > b;
}

// ---- Extended Mode (xt->extended) ------------------------------------------
//
// Extended's Control.preform is NFM 2's with its per-stage tuning rewritten
// (decompilation/extended/java-src/Control.java; research/extended-mode/
// engine-analysis.md sections 3 and 6). Ported here for Classic Mode: career
// and tourney branches are left out. Extended numbers NFM 2's stages 1-17
// (this port races them as 11-27) and its cars 23-38 (NFM 2's 0-15 here), so
// the code below compares Extended's own numbers through `st` and ext_cn().

// Extended's number for a car: NFM 2's 0-15 are its 23-38; its own 0-22
// will sit at 16-38 here.
static int32_t ext_cn(int32_t cn) { return cn < 16 ? cn + 23 : cn - 16; }

// Extended counts the race's own cars (xtgraphics.nplayers): 7 in Classic,
// 11 in its normal mode.
#define EXT_NPLAYERS (cp->nplayers)

/** One decision cycle (stcnt > statusque) of Extended's preform,
 * Control.java:159-4894, outside career. Rubber-banding is gone: acuracy and
 * upwait are never computed here, so they stay at reset's 0.
 *
 * `st` is Extended's own stage number: Classic's 1-17 (this port races them
 * as 11-27) or, in its normal mode, tracks.radq's 1-28 as they are. Checks
 * Extended wraps in `if (xtgraphics.classicmode)` are written `classic &&`;
 * the rest it runs in every mode. `ptm` is the Premier Tournament's match
 * (normal-mode stage 26, Extended's `dontdisplay`), 0 otherwise. */
static void decide_ext(Control *c, Mad *mad, ContO *contO, CheckPoints *cp) {
  Medium *m = c->m;
  const bool classic = mad->xt->classicmode;
  const int32_t st = classic ? cp->stage - 10 : cp->stage;
  const int32_t ptm = (!classic && cp->stage == 26) ? mad->xt->ptmatch : 0;
  const bool pt_race = ptm == 2 || ptm == 3;   // the tourney's racing matches
  const int32_t cn = ext_cn(mad->cn);
  const int32_t im = mad->im;

  // :159-271 -- clrnce only changes in career and the tourney (:161-166).
  c->clrnce = 5;
  if (pt_race) c->clrnce = 3;

  // :272-305 -- skiplev: of NFM 2's per-stage caps only Classic 4 and 5 stay.
  float f = 0.0f;
  if (classic && st == 4) f = 0.5f;
  if (classic && st == 5) f = 0.2f;
  if (cp->pos[im] - cp->pos[0] < -1) {
    c->skiplev = (float)((double)c->skiplev + 0.2);
    if (c->skiplev > f) c->skiplev = f;
  } else {
    c->skiplev = (float)((double)c->skiplev - 0.1);
    if (c->skiplev < 0.0f) c->skiplev = 0.0f;
  }
  if (st == 14) c->skiplev = 1.0f;
  if (st == 16 || st == 15) c->skiplev = 0.0f;

  // :306-317 -- NFM 2 cleared rampp -1 below 75 power; Extended only at 75.
  c->rampp = jtrunc(medium_random(m) * 4.0f - 2.0f);
  if (mad->power == 98.0f) c->rampp = -1;
  if (mad->power == 75.0f && c->rampp == -1) c->rampp = 0;
  if (mad->power < 60.0f) c->rampp = 1;

  // :335-440 -- stage 3's turn type now for every car, not one.
  if (c->cntrn != 0) {
    c->cntrn--;
  } else {
    c->agressed = false;
    c->turntyp = jtrunc(medium_random(m) * 4.0f);
    if (st == 3) c->turntyp = 1;
    if (cp->pos[0] - cp->pos[im] < 0) c->turntyp = jtrunc(medium_random(m) * 2.0f);
    if (classic && st == 8) c->turntyp = 2;
    if (pt_race) c->turntyp = 0;   // :377-385
    if (st == 14 || st == 20 || st == 6) c->turntyp = 0;
    if (c->attack != 0) {
      c->turntyp = 2;
      if (classic && st == 11) c->turntyp = jtrunc(medium_random(m) * 3.0f);
      if (st == 16 && cp->clear[im] - cp->clear[0] >= 5) c->turntyp = 0;
    }
    if (st == 6 || st == 7 || st == 10 || st == 11 || st == 12 || st == 14 || st == 16 || st == 17) c->agressed = true;
    if (pt_race) c->agressed = true;   // :430-438
    c->cntrn = 5;
  }

  // :441-452
  c->saftey = jtrunc_d((double)((98.0f - mad->power) / 2.0f) * ((double)(medium_random(m) / 2.0f) + 0.5));
  if (c->saftey > 20) c->saftey = 20;

  // :453-486 -- no more NFM 2's landing-care multipliers per stage.
  f = (st == 1) ? 0.9f : 0.0f;
  c->mustland = f + (float)((double)(medium_random(m) / 2.0f) - 0.25);
  if (mad->power <= 50.0f) c->mustland -= 0.5f;
  else c->mustland = 0.0f;
  if (st == 8 || st == 10 || st == 12 || st == 14 || st == 22 || st == 6) c->mustland = 0.0f;
  if (pt_race) c->mustland = 0.0f;   // :482-490

  // :487-655 -- stunt plans.
  c->stuntf = 0;
  if (classic && st == 8 && mad->pcleared == 57) c->stuntf = 1;
  if (st == 10) {
    if (cp->pos[0] >= cp->pos[im] && abs(cp->clear[0] - mad->clear) < 2 && mad->clear >= 2) {
      c->stuntf = 3;
    } else {
      c->stuntf = 4;
      c->saftey = 10;
    }
    if (cn == 12) c->stuntf = 1;
  }
  if (pt_race) c->stuntf = 2;   // :524-532
  if (classic && st == 11 && mad->pcleared == 21) c->stuntf = 1;
  if (st == 10) c->stuntf = (mad->pcleared != 44 && mad->pcleared < 140) ? 2 : 1;
  if (classic && st == 14) {
    c->saftey = 10;
    if (mad->pcleared >= 4 && mad->pcleared < 70) c->stuntf = 4;
    else if (cn == 12 || cn == 8) c->stuntf = 2;
    if (cn == 14) c->stuntf = 6;
  }
  if (pt_race) {   // :886-895
    c->saftey = 10;
    c->stuntf = 4;
  }
  if (classic && st == 16) {
    c->mustland = 0.0f;
    c->saftey = 10;
    if ((mad->pcleared == 15 || mad->pcleared == 51) && ((double)medium_random(m) > 0.4 || c->trfix != 0)) c->stuntf = 7;
    if (mad->pcleared == 42) c->stuntf = 1;
    if (mad->pcleared == 77) c->stuntf = 7;
    c->avoidnlev = jtrunc(2700.0f * medium_random(m));
  }

  // :656-760
  c->trickprf = (mad->power - 38.0f) / 50.0f - medium_random(m) / 2.0f;
  if (mad->power < 60.0f) c->trickprf = -1.0f;
  if (classic && st == 3 && im == 10 && (double)c->trickprf > 0.7) c->trickprf = 0.7f;
  if (st == 6 && (double)c->trickprf > 0.3) c->trickprf = 0.3f;
  if (st == 8 && (double)c->trickprf > 0.2) c->trickprf = 0.2f;
  if (classic && st == 9) {
    if ((double)c->trickprf > 0.5) c->trickprf = 0.5f;
    if ((im == 10 || im == 9) && (double)c->trickprf > 0.3) c->trickprf = 0.3f;
  }
  if (classic && st == 11 && c->trickprf != -1.0f) c->trickprf *= 0.75f;
  if (classic && st == 12 && (mad->pcleared == 55 || mad->pcleared == 7)) {
    c->trickprf = -1.0f;
    c->stuntf = 5;
  }
  if (classic && st == 13 && (double)c->trickprf > 0.4) c->trickprf = 0.4f;
  if (classic && st == 14 && (double)c->trickprf > 0.5) c->trickprf = 0.5f;
  if (st == 17) c->trickprf = -1.0f;

  // :761-800 -- usebounce is never rolled in Extended (stays false).
  const float dmg = 100.0f * (float)mad->hitmag / (float)mad->cd->maxmag[mad->cn];
  c->perfection = medium_random(m) <= (float)mad->hitmag / (float)mad->cd->maxmag[mad->cn];
  if (dmg > 60.0f) c->perfection = true;
  if (st == 6 || st == 8 || st == 9 || st == 10 || st == 11 || st == 12 || st == 14 || st == 16) c->perfection = true;
  if (pt_race) c->perfection = true;   // :1197-1205

  // :1220-2420 -- picking a car to attack. No 0.7 cap on the odds, and a
  // wasted car is never the one aimed at.
  if (c->attack == 0) {
    bool flag1 = true;
    if (st == 1 || st == 4 || st == 9 || st == 13 || (classic && st == 16)) flag1 = c->afta;
    if (st == 8 || st == 6 || st == 10 || st == 14) flag1 = false;
    bool flag2 = false;
    if (st == 3 && (cn == 9 || cn == 32)) flag2 = true;
    if (classic) {   // :1249-1278
      if (st == 8 && (cn == 11 || cn == 34)) flag2 = true;
      if (st == 9 && cp->clear[0] >= 20) flag2 = true;
      if (st == 11 || st == 13 || st == 15 || st == 16) flag2 = true;
    }
    int32_t j2 = 60;
    if (st == 3 || st == 11 || st == 17 || st == 10 || st == 8) j2 = 30;
    if ((st == 2 || st == 13) && (cn == 13 || cn == 36)) j2 = 50;
    if (st == 4) j2 = 20;
    if (st == 5 && im != 6) j2 = 40;
    if (st == 7) j2 = 40;
    if (st == 8 && (cn == 11 || cn == 34)) j2 = 40;
    if (st == 9 && flag2) j2 = 30;
    if (st == 11 && c->bulistc) j2 = 30;
    if (st == 12) j2 = 50;
    if (st == 15 && c->bulistc) j2 = 40;
    if (classic && st == 16) {
      if (cn == 11 && cp->clear[0] == 27) j2 = 0;
      if (cn == 15 || cn == 9) j2 = 50;
      if (cn == 11) j2 = 40;
      if (cp->pos[0] > cp->pos[im]) j2 = 80;
    }

    for (int32_t i4 = 0; i4 < EXT_NPLAYERS; i4++) {
      if (i4 == im || cp->clear[i4] == -1) continue;
      int32_t heading = contO->xz;
      if (c->zyinv) heading += 180;
      while (heading < 0) heading += 360;
      while (heading > 180) heading -= 360;
      const int32_t sign = (cp->opx[i4] - contO->x >= 0) ? 180 : 0;
      int32_t bearing = jtrunc_d(90.0 + (double)sign +
          atan((double)(cp->opz[i4] - contO->z) / (double)(cp->opx[i4] - contO->x)) / NFM_DEG);
      while (bearing < 0) bearing += 360;
      while (bearing > 180) bearing -= 360;
      int32_t k8 = abs(heading - bearing);
      if (k8 > 180) k8 = abs(k8 - 360);

      const int32_t dcl = abs(cp->clear[i4] - mad->clear);
      int32_t l6 = 2000 * (dcl + 1);
      if (st == 3 && (cn == 9 || cn == 32) && l6 < 12000) l6 = 12000;
      if (st == 4 && l6 < 4000) l6 = 4000;
      if (st == 8 && (cn == 11 || cn == 34)) {
        if (l6 < 12000) l6 = 12000;
        k8 = 10;
      }
      if (st == 9 && (mad->pcleared == 13 || mad->pcleared == 33 || flag2) && l6 < 12000) l6 = 12000;
      if (classic && st == 11) {
        if (!c->bulistc) {
          if (l6 < 6000) l6 = 6000;
        } else {
          l6 = 8000;
          k8 = 10;
          c->afta = true;
        }
      }
      if (st == 12 && c->bulistc) { l6 = 6000; k8 = 10; }
      if (st == 13) l6 = 21000;
      if (classic && st == 15) {
        l6 *= dcl + 1;
        if (c->bulistc) { l6 = 4000 * (dcl + 1); k8 = 10; }
      }
      if (st == 10) l6 = 16000;
      if (ptm == 1 || ptm == 4) l6 = 80000;   // :1476-1485, the tourney's wasting matches
      if (classic && st == 16) {
        if (cn == 13 && c->bulistc) {
          if (c->oupnt == 33) l6 = 17000;
          if (c->oupnt == 51) l6 = 30000;
          if (c->oupnt == 15 && cp->clear[0] >= 14) l6 = 60000;
          k8 = 10;
        }
        if (cn == 15 || cn == 9) l6 *= dcl + 1;
        if (cn == 11) l6 = 4000 * (dcl + 1);
      }
      int32_t i6 = 85 + 15 * (dcl + 1);
      if (classic && st == 13) i6 = 45;
      if (classic && st == 16 && (cn == 15 || cn == 9 || cn == 11 || cn == 14)) i6 = 50 + 70 * dcl;

      if (k8 < i6 && control_py(contO->x / 100, cp->opx[i4] / 100, contO->z / 100, cp->opz[i4] / 100) < l6 &&
          mad->power > (float)j2) {
        float f2 = (float)(35 - dcl * 10);
        if (f2 < 1.0f) f2 = 1.0f;
        float f3 = (float)((cp->pos[im] + 1) * (5 - cp->pos[i4])) / f2;
        if (classic) {   // :1558-1683
        if (st == 8) {
          if (cn == 34 || (cn == 36 && c->bulistc)) f3 *= 1.5f;
          else f3 = 0.0f;
        }
        if (st == 9) {
          if (i4 != 0) f3 = (float)((double)f3 * 0.5);
          if (mad->pcleared != 13 && mad->pcleared != 33 && !flag2) f3 *= 0.5f;
          if ((im == 10 || im == 9) && i4 != 0) f3 = 0.0f;
        }
        if (st == 6 || (st == 10 && !c->bulistc) || st == 14) f3 = 0.0f;
        if (st == 11 && cn == 36 && c->bulistc && i4 == 0) f3 = 1.0f;
        if (st == 12) {
          if (cn != 34 && cn != 36) f3 = 0.0f;
          if (cn == 36 && i4 == 0) f3 = 1.0f;
        }
        if (st == 15) {
          if (cp->pos[im] == 0) f3 = (float)((double)f3 * 0.5);
          if (cp->pos[0] < cp->pos[im]) f3 *= 2.0f;
          if (c->bulistc && i4 == 0) f3 = 1.0f;
        }
        if (st == 16) {
          if (cn == 37) f3 = (float)((double)f3 * 0.5);
          else if (cp->pos[0] < cp->pos[im] && cp->clear[0] - cp->clear[im] != 1) f3 *= 2.0f;
          if (cn == 36 && i4 == 0) f3 = 1.0f;
          if (cp->pos[im] == 0 || (cp->pos[im] == 1 && cp->pos[0] == 0)) f3 = 0.0f;
          if (cp->clear[im] - cp->clear[0] >= 5 && i4 == 0) f3 = 1.0f;
          if (cn == 33 || cn == 35) f3 = 0.0f;
        }
        }
        // :1684-1700 -- the tourney: the wasting matches always attack, the
        // racing ones never.
        if (ptm == 1 || ptm == 4 || ptm == 5) f3 = 900.0f;
        if (pt_race) f3 = 0.0f;
        if (i4 != 0 && cp->pos[0] < cp->pos[im]) f3 = 0.0f;
        if (i4 != 0 && flag2) f3 = 0.0f;
        if (classic && st == 7 && im == EXT_NPLAYERS - 1 && i4 == 0) f3 = (float)((double)f3 * 1.5);

        if (medium_random(m) < f3) {
          c->attack = 40 * (dcl + 1);
          if (c->attack > 500) c->attack = 500;
          c->aim = 0.0f;
          if (st == 3 && im == EXT_NPLAYERS - 1 && control_rand_gt_rand(m)) c->aim = 1.0f;
          if (st == 4) c->aim = (i4 == 0 && cp->pos[0] < cp->pos[im]) ? 1.5f : medium_random(m);
          if (st == 5) c->aim = medium_random(m) * 1.5f;
          if (st == 8 && (cn == 11 || cn == 34) && control_rand_gt_rand(m)) c->aim = 0.76f + medium_random(m) * 0.76f;
          if (st == 9 && (mad->pcleared == 13 || mad->pcleared == 33)) c->aim = 1.0f;
          if (st == 12 || st == 6 || st == 15) {
            if (!c->bulistc) {
              c->aim = medium_random(m);
            } else {
              c->aim = 0.75f + medium_random(m) / 2.0f;
              if (c->attack > 150) c->attack = 150;
            }
          }
          if (classic && st == 12) {
            if (control_rand_gt_rand(m)) c->aim = 0.7f;
            if (c->bulistc && c->attack > 150) c->attack = 150;
          }
          if (classic && st == 13 && c->attack > 60) c->attack = 60;
          if (classic && st == 15) {
            c->aim = medium_random(m) * 1.5f;
            c->attack /= 2;
            c->exitattack = control_rand_gt_rand(m);
          }
          if (classic && st == 16) {
            if (cn != 36) {
              c->aim = medium_random(m) * 1.5f;
              if (dcl <= 2 || cn == 37) c->attack /= 3;
            } else {
              c->aim = 0.76f;
              c->attack = 150;
            }
          }
          if (cp->dested[i4] == 0) c->acr = i4;
          c->turntyp = jtrunc(1.0f + medium_random(m) * 2.0f);
        }
      }
      if (flag1 && k8 > 100 &&
          control_py(contO->x / 100, cp->opx[i4] / 100, contO->z / 100, cp->opz[i4] / 100) < 300 &&
          (double)medium_random(m) > 0.6 - (double)((float)cp->pos[im] / 10.0f)) {
        c->clrnce = 0;
        c->acuracy = 0;
      }
    }
  }

  // :4535-4610 -- when to go and get fixed.
  bool flag3 = false;
  if (st == 6 || st == 10 || (classic && st == 17)) flag3 = true;
  if ((classic && st == 8 && mad->pcleared != 73) || st == 14 || st == 20) flag3 = true;
  if (ptm >= 1 && ptm <= 5) flag3 = true;   // :4231-4235, no fixing in the tourney
  if (c->trfix == 3) {
    c->upwait = 0;
    c->acuracy = 0;
    c->skiplev = 1.0f;
    c->clrnce = 2;
  } else {
    c->trfix = 0;
    const int32_t j3 = (st == 16) ? 40 : 50;
    if (dmg > (float)j3) c->trfix = 1;
    if (!flag3) {
      int32_t k9 = 80;
      if (st == 9) k9 = 70;
      if (classic && st == 15 && mad->pcleared == 91) k9 = 50;
      if (classic && st == 16 && cp->clear[im] - cp->clear[0] >= 5 && cn != 10 && cn != 12 && cn != 33 && cn != 35) k9 = 50;
      if (dmg > (float)k9) c->trfix = 2;
      c->fixby = k9;
    } else {
      c->fixby = 100;
    }
  }

  // :4611-4890 -- bulistc, the scripted detours.
  if (c->bulistc) {
    if (classic && st == 8) {
      c->runbul--;
      if (mad->pcleared == 10) c->runbul = 0;
      if (c->runbul <= 0) c->bulistc = false;
    }
  } else {
    if (classic && st == 8 && cn == 34 && mad->pcleared == 35) {
      mad->pcleared = 73;
      mad->clear = 0;
      c->bulistc = true;
      c->runbul = jtrunc(100.0f * medium_random(m));
    }
    if (classic && (st == 11 || st == 12) && cn == 36) c->bulistc = true;
    if (classic && st == 15 && cp->clear[0] - mad->clear >= 3 && c->trfix == 0) {
      c->bulistc = true;
      c->oupnt = -1;
    }
    if (classic && st == 16) {
      if (cn == 36 && cp->pcleared == 8) {
        c->bulistc = true;
        c->attack = 0;
      }
      if (cn == 34 && cp->clear[0] - mad->clear >= 2 && c->trfix == 0) {
        c->bulistc = true;
        c->oupnt = -1;
      }
    }
    if ((st == 2 || st == 3 || st == 4 || (classic && st == 8) || st == 10) && (cn == 13 || cn == 36) &&
        abs(cp->clear[0] - mad->clear) >= 2) {
      c->bulistc = true;
    }
  }

  c->stcnt = 0;
  c->statusque = jtrunc(20.0f * medium_random(m));
}

void control_reset_ext(Control *c, CheckPoints *cp, int32_t n, bool classic) {
  // NFM 2's per-stage hold/revstart/statusque in control_reset give way to
  // Extended's (Control.java:9824-9937, outside career).
  const int32_t hold0 = c->hold, statusque0 = c->statusque;
  control_reset(c, cp, n);
  const int32_t st = classic ? cp->stage - 10 : cp->stage, cn = ext_cn(n);
  c->hold = hold0;
  c->statusque = statusque0;
  c->revstart = 0;
  c->stuck = 0;
  c->downuse = 0;
  c->fewsecs = 0;
  c->fewsecson = false;
  c->fixby = 80;
  if (classic && st == 8) c->hold = 50;
  if (st == 10) c->hold = 30;
  if (st == 11) {
    if (cn != 13 && cn != 18 && cn != 19) {
      c->hold = 35;
      c->revstart = 25;
    } else {
      c->hold = 5;
    }
    c->statusque = 0;
  }
  if (classic && st == 12) {
    if (cn != 13) {
      c->hold = jtrunc(20.0f + 10.0f * medium_random(c->m));
      c->revstart = jtrunc(10.0f + 10.0f * medium_random(c->m));
    } else {
      c->hold = 5;
    }
    c->statusque = 0;
  }
  if (st == 16) c->hold = 20;
}

void control_preform(Control *c, Mad *mad, ContO *contO, CheckPoints *checkPoints, Trackers *trackers) {
  Medium *m = c->m;
  c->left = false;
  c->right = false;
  c->up = false;
  c->down = false;
  c->handb = false;
  // Extended Mode: its own decisions and the few per-tick changes below.
  const bool ext = mad->xt != NULL && mad->xt->extended;
  // Extended's normal mode races its own stages 1-28. The per-tick checks
  // below are in NFM 2's numbers (Extended Classic 1-17 = 11-27), so `pst`
  // lifts a normal stage onto them; those Extended keeps to Classic Mode
  // add `!ext_normal`.
  const bool ext_normal = ext && !mad->xt->classicmode;
  const int32_t pst = ext_normal ? checkPoints->stage + 10 : checkPoints->stage;
  if (ext) c->spatk = false; // Control.java:152; the AI's special fires from nitroandspecials
  if (mad->dest) return; // Control.js:219 `if (!mad.dest) { ... }` wraps the entire rest of the method

  if (mad->mtouch) {
    if (c->stcnt > c->statusque && ext) {
      decide_ext(c, mad, contO, checkPoints);
    } else if (c->stcnt > c->statusque) {
      // ======================================================================
      // SECTION 1 -- per-decision-cycle personality/difficulty tuning +
      // attack-target selection. Control.js:221-1065. Only re-rolls when
      // stcnt exceeds the last-rolled statusque threshold (a few ticks'
      // worth), not every single tick -- see the very end of this block.
      // ======================================================================
      int32_t stage = checkPoints->stage;
      if (stage > 10) stage -= 10;
      c->acuracy = (7 - checkPoints->pos[mad->im]) * checkPoints->pos[0] * (6 - stage * 2);
      if (c->acuracy < 0 || checkPoints->stage == -1) c->acuracy = 0;

      c->clrnce = 5;
      if (checkPoints->stage == 16 || checkPoints->stage == 21) c->clrnce = 2;
      if (checkPoints->stage == 22 && (mad->pcleared == 27 || mad->pcleared == 17)) c->clrnce = 3;
      if (checkPoints->stage == 26 && mad->pcleared == 33) c->clrnce = 3;

      float diffN = 0.0f;
      if (checkPoints->stage == 1) diffN = 2.0f;
      if (checkPoints->stage == 2) diffN = 1.5f;
      if (checkPoints->stage == 3 && mad->cn != 6) diffN = 0.5f;
      if (checkPoints->stage == 4) diffN = 0.5f;
      if (checkPoints->stage == 11) diffN = 2.0f;
      if (checkPoints->stage == 12) diffN = 1.5f;
      if (checkPoints->stage == 13 && mad->cn != 9) diffN = 0.5f;
      if (checkPoints->stage == 14) diffN = 0.5f;
      {
        int32_t posdiff = checkPoints->pos[0] - checkPoints->pos[mad->im];
        c->upwait = jtrunc_d((double)posdiff * (double)posdiff * (double)posdiff * (double)diffN);
      }
      if (c->upwait > 80) c->upwait = 80;
      if ((checkPoints->stage == 11 || checkPoints->stage == 1) && c->upwait < 20) c->upwait = 20;

      float skiplevTarget = 0.0f;
      if (checkPoints->stage == 1 || checkPoints->stage == 2) skiplevTarget = 1.0f;
      if (checkPoints->stage == 4) skiplevTarget = 0.5f;
      if (checkPoints->stage == 7) skiplevTarget = 0.5f;
      if (checkPoints->stage == 10) skiplevTarget = 0.5f;
      if (checkPoints->stage == 11 || checkPoints->stage == 12) skiplevTarget = 1.0f;
      if (checkPoints->stage == 13) skiplevTarget = 0.5f;
      if (checkPoints->stage == 14) skiplevTarget = 0.5f;
      if (checkPoints->stage == 15) skiplevTarget = 0.2f;
      if (checkPoints->pos[mad->im] - checkPoints->pos[0] >= -1) {
        c->skiplev = c->skiplev - 0.1f;
        if (c->skiplev < 0.0f) c->skiplev = 0.0f;
      } else {
        c->skiplev = c->skiplev + 0.2f;
        if (c->skiplev > skiplevTarget) c->skiplev = skiplevTarget;
      }
      if (checkPoints->stage == 18) {
        c->skiplev = (mad->pcleared >= 10 && mad->pcleared <= 24) ? 1.0f : 0.0f;
      }
      if (checkPoints->stage == 21) {
        c->skiplev = 0.0f;
        if (mad->pcleared == 5) c->skiplev = 1.0f;
        if (mad->pcleared == 28 || mad->pcleared == 35) c->skiplev = 0.5f;
      }
      if (checkPoints->stage == 23) c->skiplev = 0.5f;
      if (checkPoints->stage == 24 || checkPoints->stage == 22) c->skiplev = 1.0f;
      if (checkPoints->stage == 26 || checkPoints->stage == 25 || checkPoints->stage == 20) c->skiplev = 0.0f;

      c->rampp = jtrunc(medium_random(m) * 4.0f - 2.0f);
      if (mad->power == 98.0f) c->rampp = -1;
      if (mad->power < 75.0f && c->rampp == -1) c->rampp = 0;
      if (mad->power < 60.0f) c->rampp = 1;
      if (checkPoints->stage == 6) c->rampp = 2;
      if (checkPoints->stage == 18 && mad->pcleared >= 45) c->rampp = 2;
      if (checkPoints->stage == 22 && mad->pcleared == 17) c->rampp = 2;
      if (checkPoints->stage == 25 || checkPoints->stage == 26) c->rampp = 0;

      if (c->cntrn == 0) {
        c->agressed = false;
        c->turntyp = jtrunc(medium_random(m) * 4.0f);
        if (checkPoints->stage == 3 && mad->cn == 6) {
          c->turntyp = 1;
          if (c->attack == 0) c->agressed = true;
        }
        if (checkPoints->stage == 9 && mad->cn == 15) {
          c->turntyp = 1;
          if (c->attack == 0) c->agressed = true;
        }
        if (checkPoints->stage == 13 && mad->cn == 9) {
          c->turntyp = 1;
          if (c->attack == 0) c->agressed = true;
        }
        if (checkPoints->pos[0] - checkPoints->pos[mad->im] < 0) c->turntyp = jtrunc(medium_random(m) * 2.0f);
        if (checkPoints->stage == 10) c->turntyp = 2;
        if (checkPoints->stage == 18) c->turntyp = 2;
        if (checkPoints->stage == 20) c->turntyp = 0;
        if (checkPoints->stage == 23) c->turntyp = 1;
        if (checkPoints->stage == 24) c->turntyp = 0;
        if (c->attack != 0) {
          c->turntyp = 2;
          if (checkPoints->stage == 9 || checkPoints->stage == 10 || checkPoints->stage == 19 ||
              checkPoints->stage == 21 || checkPoints->stage == 23 || checkPoints->stage == 27) {
            c->turntyp = jtrunc(medium_random(m) * 3.0f);
          }
          if (checkPoints->stage == 26 && checkPoints->clear[mad->im] - checkPoints->clear[0] >= 5) c->turntyp = 0;
        }
        if (checkPoints->stage == 6) { c->turntyp = 1; c->agressed = true; }
        if (checkPoints->stage == 7 || checkPoints->stage == 9 || checkPoints->stage == 10 || checkPoints->stage == 16 ||
            checkPoints->stage == 17 || checkPoints->stage == 19 || checkPoints->stage == 20 || checkPoints->stage == 21 ||
            checkPoints->stage == 22 || checkPoints->stage == 24 || checkPoints->stage == 26 || checkPoints->stage == 27) {
          c->agressed = true;
        }
        if (checkPoints->stage == -1) {
          c->agressed = control_rand_gt_rand(m);
        }
        c->cntrn = 5;
      } else {
        c->cntrn--;
      }

      c->saftey = jtrunc((((98.0f - mad->power) / 2.0f)) * ((medium_random(m) / 2.0f) + 0.5f));
      if (c->saftey > 20) c->saftey = 20;

      float landN2 = 0.0f;
      if (checkPoints->stage == 1 || checkPoints->stage == 11) landN2 = 0.9f;
      if (checkPoints->stage == 2 || checkPoints->stage == 12) landN2 = 0.7f;
      if (checkPoints->stage == 4 || checkPoints->stage == 13) landN2 = 0.4f;
      c->mustland = landN2 + ((medium_random(m) / 2.0f) - 0.25f);

      float landN3 = 1.0f;
      if (checkPoints->stage == 1 || checkPoints->stage == 11) landN3 = 5.0f;
      if (checkPoints->stage == 2 || checkPoints->stage == 12) landN3 = 2.0f;
      if (checkPoints->stage == 4 || checkPoints->stage == 13) landN3 = 1.5f;
      if (mad->power > 50.0f) {
        if (checkPoints->pos[0] - checkPoints->pos[mad->im] > 0) {
          c->saftey = jtrunc((float)c->saftey * landN3);
        } else {
          c->mustland = 0.0f;
        }
      } else {
        c->mustland = c->mustland - 0.5f;
      }
      if (checkPoints->stage == 18 || checkPoints->stage == 20 || checkPoints->stage == 22 || checkPoints->stage == 24) {
        c->mustland = 0.0f;
      }

      c->stuntf = 0;
      if (checkPoints->stage == 8) c->stuntf = 17;
      if (checkPoints->stage == 18 && mad->pcleared == 57) c->stuntf = 1;
      if (checkPoints->stage == 19 && mad->pcleared == 3) c->stuntf = 2;
      if (checkPoints->stage == 20) {
        if (checkPoints->pos[0] < checkPoints->pos[mad->im] || abs(checkPoints->clear[0] - mad->clear) >= 2 || mad->clear < 2) {
          c->stuntf = 4;
          c->saftey = 10;
        } else {
          c->stuntf = 3;
        }
      }
      if (checkPoints->stage == 21 && mad->pcleared == 21) c->stuntf = 1;
      if (checkPoints->stage == 24) {
        c->saftey = 10;
        if (mad->pcleared >= 4 && mad->pcleared < 70) {
          c->stuntf = 4;
        } else if (mad->cn == 12 || mad->cn == 8) {
          c->stuntf = 2;
        }
        if (mad->cn == 14) c->stuntf = 6;
      }
      if (checkPoints->stage == 26) {
        c->mustland = 0.0f;
        c->saftey = 10;
        if ((mad->pcleared == 15 || mad->pcleared == 51) && (medium_random(m) > 0.4f || c->trfix != 0)) c->stuntf = 7;
        if (mad->pcleared == 42) c->stuntf = 1;
        if (mad->pcleared == 77) c->stuntf = 7;
        c->avoidnlev = jtrunc(2700.0f * medium_random(m));
      }

      c->trickprf = ((mad->power - 38.0f) / 50.0f) - (medium_random(m) / 2.0f);
      if (mad->power < 60.0f) c->trickprf = -1.0f;
      if (checkPoints->stage == 6 && c->trickprf > 0.5f) c->trickprf = 0.5f;
      if (checkPoints->stage == 3 && mad->cn == 6 && c->trickprf > 0.7f) c->trickprf = 0.7f;
      if (checkPoints->stage == 13 && mad->cn == 9 && c->trickprf > 0.7f) c->trickprf = 0.7f;
      if (checkPoints->stage == 16 && c->trickprf > 0.3f) c->trickprf = 0.3f;
      if (checkPoints->stage == 18 && c->trickprf > 0.2f) c->trickprf = 0.2f;
      if (checkPoints->stage == 19) {
        if (c->trickprf > 0.5f) c->trickprf = 0.5f;
        if ((mad->im == 6 || mad->im == 5) && c->trickprf > 0.3f) c->trickprf = 0.3f;
      }
      if (checkPoints->stage == 21 && c->trickprf != -1.0f) c->trickprf = c->trickprf * 0.75f;
      if (checkPoints->stage == 22 && (mad->pcleared == 55 || mad->pcleared == 7)) {
        c->trickprf = -1.0f;
        c->stuntf = 5;
      }
      if (checkPoints->stage == 23 && c->trickprf > 0.4f) c->trickprf = 0.4f;
      if (checkPoints->stage == 24 && c->trickprf > 0.5f) c->trickprf = 0.5f;
      if (checkPoints->stage == 27) c->trickprf = -1.0f;

      c->usebounce = medium_random(m) > (mad->power / 100.0f);
      if (checkPoints->stage == 9) c->usebounce = false;
      if (checkPoints->stage == 14 || checkPoints->stage == 16) c->usebounce = true;
      if (checkPoints->stage == 20 || checkPoints->stage == 24) c->usebounce = false;

      c->perfection = medium_random(m) <= ((float)mad->hitmag / (float)mad->cd->maxmag[mad->cn]);
      if (((100.0f * (float)mad->hitmag) / (float)mad->cd->maxmag[mad->cn]) > 60.0f) c->perfection = true;
      if (checkPoints->stage == 3 && mad->cn == 6) c->perfection = true;
      if (checkPoints->stage == 6 || checkPoints->stage == 8 || checkPoints->stage == 9 || checkPoints->stage == 10 ||
          checkPoints->stage == 16 || checkPoints->stage == 18 || checkPoints->stage == 19 || checkPoints->stage == 20 ||
          checkPoints->stage == 21 || checkPoints->stage == 22 || checkPoints->stage == 24 || checkPoints->stage == 26 ||
          checkPoints->stage == 27) {
        c->perfection = true;
      }

      // Control.js:576-966 -- attack-target selection: scans the other 6
      // potential opponents, scores each as a candidate to attack this
      // decision-cycle (bearing, distance, stage-specific scripted
      // priority), and probabilistically commits to attacking the
      // highest-scoring one via `this.m.random() < n10`.
      if (c->attack == 0) {
        bool afta = true;
        if (checkPoints->stage == 3 || checkPoints->stage == 1 || checkPoints->stage == 4 || checkPoints->stage == 9 ||
            checkPoints->stage == 13 || checkPoints->stage == 11 || checkPoints->stage == 14 || checkPoints->stage == 19 ||
            checkPoints->stage == 23 || checkPoints->stage == 26) {
          afta = c->afta;
        }
        if (checkPoints->stage == 8 || checkPoints->stage == 6 || checkPoints->stage == 18 || checkPoints->stage == 16 ||
            checkPoints->stage == 20 || checkPoints->stage == 24) {
          afta = false;
        }
        if (checkPoints->stage == 3 && mad->cn == 6) afta = false;
        if (checkPoints->stage == -1 && control_rand_gt_rand(m)) afta = false;

        bool atkB = false;
        if (checkPoints->stage == 13 && mad->cn == 9) atkB = true;
        if (checkPoints->stage == 18 && mad->cn == 11) atkB = true;
        if (checkPoints->stage == 19 && checkPoints->clear[0] >= 20) atkB = true;
        if (checkPoints->stage == 4 || checkPoints->stage == 10 || checkPoints->stage == 21 || checkPoints->stage == 22 ||
            checkPoints->stage == 23 || checkPoints->stage == 25 || checkPoints->stage == 26) {
          atkB = true;
        }
        if (checkPoints->stage == 3 && mad->cn == 6) atkB = true;

        int32_t atkPowerThresh = 60;
        if (checkPoints->stage == 5) atkPowerThresh = 40;
        if (checkPoints->stage == 6 && c->bulistc) atkPowerThresh = 40;
        if (checkPoints->stage == 9 && c->bulistc) atkPowerThresh = 30;
        if (checkPoints->stage == 3 || checkPoints->stage == 13 || checkPoints->stage == 21 || checkPoints->stage == 27 ||
            checkPoints->stage == 20 || checkPoints->stage == 18) {
          atkPowerThresh = 30;
        }
        if ((checkPoints->stage == 12 || checkPoints->stage == 23) && mad->cn == 13) atkPowerThresh = 50;
        if (checkPoints->stage == 14) atkPowerThresh = 20;
        if (checkPoints->stage == 15 && mad->im != 6) atkPowerThresh = 40;
        if (checkPoints->stage == 17) atkPowerThresh = 40;
        if (checkPoints->stage == 18 && mad->cn == 11) atkPowerThresh = 40;
        if (checkPoints->stage == 19 && atkB) atkPowerThresh = 30;
        if (checkPoints->stage == 21 && c->bulistc) atkPowerThresh = 30;
        if (checkPoints->stage == 22) atkPowerThresh = 50;
        if (checkPoints->stage == 25 && c->bulistc) atkPowerThresh = 40;
        if (checkPoints->stage == 26) {
          if (mad->cn == 11 && checkPoints->clear[0] == 27) atkPowerThresh = 0;
          if (mad->cn == 15 || mad->cn == 9) atkPowerThresh = 50;
          if (mad->cn == 11) atkPowerThresh = 40;
          if (checkPoints->pos[0] > checkPoints->pos[mad->im]) atkPowerThresh = 80;
        }

        for (int32_t i = 0; i < 7; i++) {
          if (i == mad->im || checkPoints->clear[i] == -1) continue;

          int32_t myHeading = contO->xz;
          if (c->zyinv) myHeading += 180;
          while (myHeading < 0) myHeading += 360;
          while (myHeading > 180) myHeading -= 360;
          int32_t bearingSign = (checkPoints->opx[i] - contO->x >= 0) ? 180 : 0;
          int32_t bearingToOpp = jtrunc_d(90.0 + (double)bearingSign +
              atan((double)(checkPoints->opz[i] - contO->z) / (double)(checkPoints->opx[i] - contO->x)) / NFM_DEG);
          while (bearingToOpp < 0) bearingToOpp += 360;
          while (bearingToOpp > 180) bearingToOpp -= 360;
          int32_t n6 = abs(myHeading - bearingToOpp);
          if (n6 > 180) n6 = abs(n6 - 360);

          int32_t n7 = 2000 * (abs(checkPoints->clear[i] - mad->clear) + 1);
          if ((checkPoints->stage == 6 || checkPoints->stage == 9) && c->bulistc) n7 = 6000;
          if (checkPoints->stage == 3 && mad->cn == 6 && checkPoints->wasted < 2 && n7 > 4000) n7 = 4000;
          if (checkPoints->stage == 13 && mad->cn == 9 && n7 < 12000) n7 = 12000;
          if (checkPoints->stage == 14 && n7 < 4000) n7 = 4000;
          // Java Control.java:786-807 -- three SEPARATE `if`s (stage 18,
          // then stage 19, then stage 21), not one if/else chain. This
          // used to merge the stage-21 else-branch onto stage 18's
          // condition (so it never ran when stage==18, and ran on EVERY
          // other stage rather than only 21) and drop the stage-19 block
          // entirely, plus set c->afta unconditionally for stage 18
          // instead of only inside stage 21's bulistc branch. Affects
          // only the attack-target detection range for stages 18/19/21.
          if (checkPoints->stage == 18 && mad->cn == 11) {
            if (n7 < 12000) n7 = 12000;
            n6 = 10;
          }
          if (checkPoints->stage == 19 &&
              (mad->pcleared == 13 || mad->pcleared == 33 || atkB) && n7 < 12000) {
            n7 = 12000;
          }
          if (checkPoints->stage == 21) {
            if (c->bulistc) {
              n7 = 8000;
              n6 = 10;
              c->afta = true;
            } else if (n7 < 6000) {
              n7 = 6000;
            }
          }
          if (checkPoints->stage == 22 && c->bulistc) { n7 = 6000; n6 = 10; }
          if (checkPoints->stage == 23) n7 = 21000;
          if (checkPoints->stage == 25) {
            n7 *= abs(checkPoints->clear[i] - mad->clear) + 1;
            if (c->bulistc) { n7 = 4000 * (abs(checkPoints->clear[i] - mad->clear) + 1); n6 = 10; }
          }
          if (checkPoints->stage == 20) n7 = 16000;
          if (checkPoints->stage == 26) {
            if (mad->cn == 13 && c->bulistc) {
              if (c->oupnt == 33) n7 = 17000;
              if (c->oupnt == 51) n7 = 30000;
              if (c->oupnt == 15 && checkPoints->clear[0] >= 14) n7 = 60000;
              n6 = 10;
            }
            if (mad->cn == 15 || mad->cn == 9) n7 *= abs(checkPoints->clear[i] - mad->clear) + 1;
            if (mad->cn == 11) n7 = 4000 * (abs(checkPoints->clear[i] - mad->clear) + 1);
          }

          int32_t n8 = 85 + 15 * (abs(checkPoints->clear[i] - mad->clear) + 1);
          if (checkPoints->stage == 23) n8 = 45;
          if (checkPoints->stage == 26 && (mad->cn == 15 || mad->cn == 9 || mad->cn == 11 || mad->cn == 14)) {
            n8 = 50 + 70 * abs(checkPoints->clear[i] - mad->clear);
          }

          if (n6 < n8 &&
              control_py(contO->x / 100, checkPoints->opx[i] / 100, contO->z / 100, checkPoints->opz[i] / 100) < n7 &&
              afta && mad->power > (float)atkPowerThresh) {
            float n9 = 35.0f - (float)abs(checkPoints->clear[i] - mad->clear) * 10.0f;
            if (n9 < 1.0f) n9 = 1.0f;
            float n10 = (float)((checkPoints->pos[mad->im] + 1) * (5 - checkPoints->pos[i])) / n9;
            if (checkPoints->stage != 27 && n10 > 0.7f) n10 = 0.7f;
            if (i != 0 && checkPoints->pos[0] < checkPoints->pos[mad->im]) n10 = 0.0f;
            if (i != 0 && atkB) n10 = 0.0f;
            if (atkB && checkPoints->stage == 3 && i == 0) {
              n10 = (checkPoints->wasted >= 2) ? n10 * 0.5f : 0.0f;
            }
            if ((checkPoints->stage == 3 || checkPoints->stage == 9) && i == 4) n10 = 0.0f;
            if (checkPoints->stage == 6) {
              n10 = 0.0f;
              if (c->bulistc && i == 0) n10 = 1.0f;
            }
            if (checkPoints->stage == 8) {
              n10 = 0.0f;
              if (c->bulistc && mad->cn != 11 && mad->cn != 13) n10 = 1.0f;
            }
            if (checkPoints->stage == 9 && mad->cn == 15) n10 = 0.0f;
            if (checkPoints->stage == 9 && c->bulistc) n10 = (i == 0) ? 1.0f : 0.0f;
            if (checkPoints->stage == 9 && (checkPoints->pos[i] == 4 || checkPoints->pos[i] == 3)) n10 = 0.0f;
            if (checkPoints->stage == 13) {
              n10 = (mad->cn == 9 || (mad->cn == 13 && c->bulistc)) ? n10 * 2.0f : n10 * 0.5f;
            }
            if (checkPoints->stage == 16) n10 = 0.0f;
            if (checkPoints->stage == 17 && mad->im == 6 && i == 0) n10 = n10 * 1.5f;
            if (checkPoints->stage == 18) {
              n10 = (mad->cn == 11 || (mad->cn == 13 && c->bulistc)) ? n10 * 1.5f : 0.0f;
            }
            if (checkPoints->stage == 19) {
              if (i != 0) n10 = n10 * 0.5f;
              if (mad->pcleared != 13 && mad->pcleared != 33 && !atkB) n10 = n10 * 0.5f;
              if ((mad->im == 6 || mad->im == 5) && i != 0) n10 = 0.0f;
            }
            if (checkPoints->stage == 20) {
              n10 = 0.0f;
              if (c->bulistc && mad->cn != 11 && mad->cn != 13) n10 = 1.0f;
            }
            if (checkPoints->stage == 21 && c->bulistc && i == 0) n10 = 1.0f;
            if (checkPoints->stage == 22) {
              if (mad->cn != 11 && mad->cn != 13) n10 = 0.0f;
              if (mad->cn == 13 && i == 0) n10 = 1.0f;
            }
            if (checkPoints->stage == 24) n10 = 0.0f;
            if (checkPoints->stage == 25) {
              if (checkPoints->pos[mad->im] == 0) n10 = n10 * 0.5f;
              if (checkPoints->pos[0] < checkPoints->pos[mad->im]) n10 = n10 * 2.0f;
              if (c->bulistc && i == 0) n10 = 1.0f;
            }
            if (checkPoints->stage == 26) {
              if (mad->cn != 14) {
                if (checkPoints->pos[0] < checkPoints->pos[mad->im] && checkPoints->clear[0] - checkPoints->clear[mad->im] != 1) {
                  n10 = n10 * 2.0f;
                }
              } else {
                n10 = n10 * 0.5f;
              }
              if (mad->cn == 13 && i == 0) n10 = 1.0f;
              if (checkPoints->pos[mad->im] == 0 || (checkPoints->pos[mad->im] == 1 && checkPoints->pos[0] == 0)) n10 = 0.0f;
              if (checkPoints->clear[mad->im] - checkPoints->clear[0] >= 5 && i == 0) n10 = 1.0f;
              if (mad->cn == 10 || mad->cn == 12) n10 = 0.0f;
            }

            if (medium_random(m) < n10) {
              c->attack = 40 * (abs(checkPoints->clear[i] - mad->clear) + 1);
              if (c->attack > 500) c->attack = 500;
              c->aim = 0.0f;
              if (checkPoints->stage == 13 && mad->cn == 9 && control_rand_gt_rand(m)) c->aim = 1.0f;
              if (checkPoints->stage == 14) {
                if (i == 0 && checkPoints->pos[0] < checkPoints->pos[mad->im]) c->aim = 1.5f;
                else c->aim = medium_random(m);
              }
              if (checkPoints->stage == 15) c->aim = medium_random(m) * 1.5f;
              if (checkPoints->stage == 17 && mad->im != 6 &&
                  (control_rand_gt_rand(m) || checkPoints->pos[0] < checkPoints->pos[mad->im])) {
                c->aim = 1.0f;
              }
              if (checkPoints->stage == 18 && mad->cn == 11 && control_rand_gt_rand(m)) {
                c->aim = 0.76f + (medium_random(m) * 0.76f);
              }
              if (checkPoints->stage == 19 && (mad->pcleared == 13 || mad->pcleared == 33)) c->aim = 1.0f;
              if (checkPoints->stage == 21) {
                if (c->bulistc) {
                  c->aim = 0.7f;
                  if (c->attack > 150) c->attack = 150;
                } else {
                  c->aim = medium_random(m);
                }
              }
              if (checkPoints->stage == 22) {
                if (control_rand_gt_rand(m)) c->aim = 0.7f;
                if (c->bulistc && c->attack > 150) c->attack = 150;
              }
              if (checkPoints->stage == 23 && c->attack > 60) c->attack = 60;
              if (checkPoints->stage == 25) {
                c->aim = medium_random(m) * 1.5f;
                c->attack = c->attack / 2;
                c->exitattack = control_rand_gt_rand(m);
              }
              if (checkPoints->stage == 26) {
                if (mad->cn == 13) {
                  c->aim = 0.76f;
                  c->attack = 150;
                } else {
                  c->aim = medium_random(m) * 1.5f;
                  if (abs(checkPoints->clear[i] - mad->clear) <= 2 || mad->cn == 14) c->attack = c->attack / 3;
                }
              }
              if (checkPoints->stage == -1 && control_rand_gt_rand(m)) c->aim = medium_random(m) * 1.5f;
              c->acr = i;
              c->turntyp = jtrunc(1.0f + (medium_random(m) * 2.0f));
            }
          }
          if (afta && n6 > 100 &&
              control_py(contO->x / 100, checkPoints->opx[i] / 100, contO->z / 100, checkPoints->opz[i] / 100) < 300 &&
              medium_random(m) > (0.6f - (float)checkPoints->pos[mad->im] / 10.0f)) {
            c->clrnce = 0;
            c->acuracy = 0;
          }
        }
      }

      // Control.js:967-1063 -- damage-triggered "flee and fix" state
      // machine (trfix) + the bulistc ("bullet list"/scripted-detour)
      // latch that several stages use to route the AI through a
      // specific world-coordinate encounter (see SECTION 2's own
      // gowait/oupnt handling further down for where bulistc changes
      // navigation behaviour).
      bool fixExempt = false;
      if (checkPoints->stage == 6 || checkPoints->stage == 8) fixExempt = true;
      if (checkPoints->stage == 9 && mad->cn == 15) fixExempt = true;
      if (checkPoints->stage == 16 || checkPoints->stage == 20 || checkPoints->stage == 21 || checkPoints->stage == 27) fixExempt = true;
      if (checkPoints->stage == 18 && mad->pcleared != 73) fixExempt = true;
      if (checkPoints->stage == -1 && control_rand_gt_rand(m)) fixExempt = true;

      if (c->trfix != 3) {
        c->trfix = 0;
        int32_t fixThresh1 = 50;
        if (checkPoints->stage == 26) fixThresh1 = 40;
        if (((100.0f * (float)mad->hitmag) / (float)mad->cd->maxmag[mad->cn]) > (float)fixThresh1) c->trfix = 1;
        if (!fixExempt) {
          int32_t fixThresh2 = 80;
          if (checkPoints->stage == 18 && mad->cn != 11) fixThresh2 = 50;
          if (checkPoints->stage == 19) fixThresh2 = 70;
          if (checkPoints->stage == 25 && mad->pcleared == 91) fixThresh2 = 50;
          if (checkPoints->stage == 26 && checkPoints->clear[mad->im] - checkPoints->clear[0] >= 5 && mad->cn != 10 && mad->cn != 12) {
            fixThresh2 = 50;
          }
          if (((100.0f * (float)mad->hitmag) / (float)mad->cd->maxmag[mad->cn]) > (float)fixThresh2) c->trfix = 2;
        }
      } else {
        c->upwait = 0;
        c->acuracy = 0;
        c->skiplev = 1.0f;
        c->clrnce = 2;
      }

      if (!c->bulistc) {
        if (checkPoints->stage == 18 && mad->cn == 11 && mad->pcleared == 35) {
          mad->pcleared = 73;
          mad->clear = 0;
          c->bulistc = true;
          c->runbul = jtrunc(100.0f * medium_random(m));
        }
        if (checkPoints->stage == 21 && mad->cn == 13) c->bulistc = true;
        if (checkPoints->stage == 22 && mad->cn == 13) c->bulistc = true;
        if (checkPoints->stage == 25 && checkPoints->clear[0] - mad->clear >= 3 && c->trfix == 0) {
          c->bulistc = true;
          c->oupnt = -1;
        }
        if (checkPoints->stage == 26) {
          if (mad->cn == 13 && checkPoints->pcleared == 8) { c->bulistc = true; c->attack = 0; }
          if (mad->cn == 11 && checkPoints->clear[0] - mad->clear >= 2 && c->trfix == 0) { c->bulistc = true; c->oupnt = -1; }
        }
        if ((checkPoints->stage == 6 || checkPoints->stage == 8 || checkPoints->stage == 12 || checkPoints->stage == 13 ||
             checkPoints->stage == 14 || checkPoints->stage == 15 || checkPoints->stage == 18 || checkPoints->stage == 20 ||
             checkPoints->stage == 23) && mad->cn == 13 && abs(checkPoints->clear[0] - mad->clear) >= 2) {
          c->bulistc = true;
        }
        if ((checkPoints->stage == 8 || checkPoints->stage == 20) && mad->cn == 11 && abs(checkPoints->clear[0] - mad->clear) >= 1) {
          c->bulistc = true;
        }
        if (checkPoints->stage == 6 && mad->cn == 11) c->bulistc = true;
        if (checkPoints->stage == 9 && c->afta && (checkPoints->pos[mad->im] == 4 || checkPoints->pos[mad->im] == 3) &&
            mad->cn != 15 && c->trfix != 0) {
          c->bulistc = true;
        }
      } else if (checkPoints->stage == 18) {
        c->runbul--;
        if (mad->pcleared == 10) c->runbul = 0;
        if (c->runbul <= 0) c->bulistc = false;
      }

      c->stcnt = 0;
      c->statusque = jtrunc(20.0f * medium_random(m));
    } else {
      c->stcnt++;
    }

    // ======================================================================
    // SECTION 2 -- main per-tick steering. Control.js:1070-1727. Picks a
    // target waypoint (or combat-intercept aim point when this.attack!=0)
    // and turns it into a `pan` bearing; SECTION 3 (right after) turns
    // that bearing into left/right/handb/down.
    // ======================================================================
    bool touchingGround = (c->usebounce && !ext) ? mad->wtouch : mad->mtouch;
    if (touchingGround) {
      if (c->trickfase != 0) c->trickfase = 0;
      if (c->trfix == 2 || c->trfix == 3) c->attack = 0;

      if (c->attack == 0) {
        if (c->upcnt < 30) {
          if (c->revstart <= 0) {
            if (!ext || !c->fewsecson) c->up = true;
          } else {
            c->down = true;
            c->revstart--;
          }
        }
        if (c->upcnt < 25 + c->actwait) {
          c->upcnt++;
        } else {
          c->upcnt = 0;
          c->actwait = c->upwait;
        }

        int32_t waypoint = mad->point;
        int32_t bulPowerThresh = 50;
        if (pst == 9) bulPowerThresh = 20;
        if (pst == 18) bulPowerThresh = 20;
        if (pst == 25) bulPowerThresh = 40;
        if (pst == 26) bulPowerThresh = 20;

        if (!c->bulistc || c->trfix == 2 || c->trfix == 3 || c->trfix == 4 || mad->power < (float)bulPowerThresh) {
          // Control.js:1113-1375 -- normal forward waypoint advance, with
          // ramp-shortcut (rampp) and ahead-of-lap-count skip checks.
          if (c->rampp == 1 && checkPoints->typ[waypoint] <= 0) {
            int32_t n15 = waypoint + 1;
            if (n15 >= checkPoints->n) n15 = 0;
            if (checkPoints->typ[n15] == -2) waypoint = n15;
          }
          if (c->rampp == -1 && checkPoints->typ[waypoint] == -2) {
            waypoint++;
            if (waypoint >= checkPoints->n) waypoint = 0;
          }
          if (medium_random(m) > c->skiplev) {
            int32_t n16 = waypoint;
            int32_t n17 = 0;
            if (checkPoints->typ[n16] > 0) {
              int32_t n18 = 0;
              for (int32_t l = 0; l < checkPoints->n; l++) {
                if (checkPoints->typ[l] > 0 && l < n16) n18++;
              }
              n17 = (mad->clear != n18 + mad->nlaps * checkPoints->nsp) ? 1 : 0;
            }
            // Bounded by one trip round the route: a stop it can reach, it
            // reaches within that (the Java's loop, unchanged); one it
            // cannot -- a mad->clear no gate answers to -- must not hang.
            int32_t skip_guard = 0;
            while ((checkPoints->typ[n16] == 0 || checkPoints->typ[n16] == -1 || checkPoints->typ[n16] == -3 || n17 != 0) &&
                   skip_guard++ <= checkPoints->n) {
              waypoint = n16;
              n16++;
              if (n16 >= checkPoints->n) n16 = 0;
              n17 = 0;
              if (checkPoints->typ[n16] > 0) {
                int32_t n19 = 0;
                for (int32_t l = 0; l < checkPoints->n; l++) {
                  if (checkPoints->typ[l] > 0 && l < n16) n19++;
                }
                n17 = (mad->clear != n19 + mad->nlaps * checkPoints->nsp) ? 1 : 0;
              }
            }
          } else if (medium_random(m) > c->skiplev) {
            while (checkPoints->typ[waypoint] == -1) {
              waypoint++;
              if (waypoint >= checkPoints->n) waypoint = 0;
            }
          }

          if (!ext_normal && checkPoints->stage == 18 && mad->pcleared == 73 && c->trfix == 0 && mad->clear != 0) waypoint = 10;
          if (!ext_normal && checkPoints->stage == 19 && mad->pcleared == 18 && c->trfix == 0) waypoint = 27;
          if (!ext_normal && checkPoints->stage == 21) {
            if (mad->pcleared == 5 && c->trfix == 0 && mad->power < 70.0f) {
              waypoint = (waypoint <= 16) ? 16 : 21;
            }
            if (mad->pcleared == 50) waypoint = 57;
          }
          if (!ext_normal && checkPoints->stage == 22 && (mad->pcleared == 27 || mad->pcleared == 37)) {
            while (checkPoints->typ[waypoint] == -1) {
              waypoint++;
              if (waypoint >= checkPoints->n) waypoint = 0;
            }
          }
          if (checkPoints->stage == 23 && !ext) { // gone in Extended
            while (checkPoints->typ[waypoint] == -1) {
              waypoint++;
              if (waypoint >= checkPoints->n) waypoint = 0;
            }
          }
          // Extended runs Classic 14's routing on Classic 9 as well (Control.java:~30357).
          if (!ext_normal && (checkPoints->stage == 24 || (ext && checkPoints->stage == 19))) {
            while (checkPoints->typ[waypoint] == -1) {
              waypoint++;
              if (waypoint >= checkPoints->n) waypoint = 0;
            }
            if (!mad->gtouch) {
              while (checkPoints->typ[waypoint] == -2) {
                waypoint++;
                if (waypoint >= checkPoints->n) waypoint = 0;
              }
            }
            if (c->oupnt >= 68) {
              waypoint = 70;
            } else {
              c->oupnt = waypoint;
            }
          }
          if (!ext_normal && checkPoints->stage == 25) {
            if ((mad->pcleared != 91 && checkPoints->pos[0] < checkPoints->pos[mad->im] && mad->cn != 13) ||
                (checkPoints->pos[mad->im] == 0 && (mad->clear == 12 || mad->clear == 20))) {
              while (checkPoints->typ[waypoint] == -4) {
                waypoint++;
                if (waypoint >= checkPoints->n) waypoint = 0;
              }
            }
            if (mad->pcleared == 9) {
              if (control_py(contO->x / 100, 297, contO->z / 100, 347) < 400) c->oupnt = 1;
              if (c->oupnt == 1 && waypoint < 22) waypoint = 22;
            }
            if (mad->pcleared == 67) {
              if (control_py(contO->x / 100, 28, contO->z / 100, 494) < 4000) c->oupnt = 2;
              if (c->oupnt == 2) waypoint = 76;
            }
            if (mad->pcleared == 76) {
              if (control_py(contO->x / 100, -50, contO->z / 100, 0) < 2000) c->oupnt = 3;
              waypoint = (c->oupnt == 3) ? 91 : 89;
            }
          }
          if (!ext_normal && checkPoints->stage == 26) {
            if (mad->pcleared == 128) {
              if (control_py(contO->x / 100, 0, contO->z / 100, 229) < 1500 || contO->z > 23000) c->oupnt = 128;
              if (c->oupnt != 128) waypoint = 3;
            }
            if (mad->pcleared == 8) {
              if (control_py(contO->x / 100, -207, contO->z / 100, 549) < 1500 || contO->x < -20700) c->oupnt = 8;
              if (c->oupnt != 8) waypoint = 12;
            }
            if (mad->pcleared == 33) {
              if (control_py(contO->x / 100, -60, contO->z / 100, 168) < 250 || contO->z > 17000) c->oupnt = 331;
              if (control_py(contO->x / 100, -112, contO->z / 100, 414) < 10000 || contO->z > 40000) c->oupnt = 332;
              if (c->oupnt != 331 && c->oupnt != 332) waypoint = (c->trfix != 1) ? 38 : 39;
              if (c->oupnt == 331) waypoint = 71;
            }
            if (mad->pcleared == 42) {
              if (control_py(contO->x / 100, -269, contO->z / 100, 493) < 100 || contO->x < -27000) c->oupnt = 142;
              if (c->oupnt != 142) waypoint = 47;
            }
            if (mad->pcleared == 51) {
              if (control_py(contO->x / 100, -352, contO->z / 100, 260) < 100 || contO->z < 25000) c->oupnt = 511;
              if (control_py(contO->x / 100, -325, contO->z / 100, 10) < 2000 || contO->x > -32000) c->oupnt = 512;
              if (c->oupnt != 511 && c->oupnt != 512) waypoint = 80;
              if (c->oupnt == 511) waypoint = 61;
            }
            if (mad->pcleared == 77) {
              if (control_py(contO->x / 100, -371, contO->z / 100, 319) < 100 || contO->z < 31000) c->oupnt = 77;
              if (c->oupnt != 77) { waypoint = 78; mad->nofocus = true; }
            }
            if (mad->pcleared == 105) {
              if (control_py(contO->x / 100, -179, contO->z / 100, 10) < 2300 || contO->z < 1050) c->oupnt = 105;
              waypoint = (c->oupnt != 105) ? 65 : 125;
            }
            if (c->trfix == 3) {
              if (control_py(contO->x / 100, -52, contO->z / 100, 448) < 100 || contO->z > 45000) c->oupnt = 176;
              waypoint = (c->oupnt != 176) ? 41 : 43;
            }
            if (checkPoints->clear[mad->im] - checkPoints->clear[0] >= 2 && (!ext || checkPoints->dested[0] == 0) &&
                control_py(contO->x / 100, checkPoints->opx[0] / 100, contO->z / 100, checkPoints->opz[0] / 100) < 1000 + c->avoidnlev) {
              int32_t xzB = contO->xz;
              if (c->zyinv) xzB += 180;
              while (xzB < 0) xzB += 360;
              while (xzB > 180) xzB -= 360;
              int32_t n21 = (checkPoints->opx[0] - contO->x >= 0) ? 180 : 0;
              int32_t n22 = jtrunc_d(90.0 + (double)n21 +
                  atan((double)(checkPoints->opz[0] - contO->z) / (double)(checkPoints->opx[0] - contO->x)) / NFM_DEG);
              while (n22 < 0) n22 += 360;
              while (n22 > 180) n22 -= 360;
              int32_t n23 = abs(xzB - n22);
              if (n23 > 180) n23 = abs(n23 - 360);
              if (n23 < 90) c->wall = 0;
            }
          }
          if (c->rampp == 2) {
            int32_t n24 = waypoint + 1;
            if (n24 >= checkPoints->n) n24 = 0;
            if (checkPoints->typ[n24] == -2 && waypoint != mad->point) {
              waypoint--;
              if (waypoint < 0) waypoint += checkPoints->n;
            }
          }
          if (c->bulistc) {
            mad->nofocus = true;
            if (c->gowait) c->gowait = false;
          }
        } else {
          // Control.js:1376-1635 -- bulistc's scripted-detour path: walks
          // BACKWARD through checkpoints (or drives to an explicit
          // wtx/wtz world-coordinate "wait point" then an frx/frz
          // "release" point) instead of the normal forward advance.
          // Stages 21/22/26 script full ambush encounters here with
          // hardcoded world coordinates -- see this file's own top
          // comment on why these aren't hand-wavable/generalizable.
          if ((pst != 25 && pst != 26) || c->runbul == 0) {
            waypoint -= 2;
            if (waypoint < 0) waypoint += checkPoints->n;
            if (checkPoints->stage == 9 && waypoint > 76) waypoint = 76;
            while (checkPoints->typ[waypoint] == -4) {
              waypoint--;
              if (waypoint < 0) waypoint += checkPoints->n;
            }
          }
          if (!ext_normal && checkPoints->stage == 21) {
            if (waypoint >= 14 && waypoint <= 19) waypoint = 13;
            if (c->oupnt == 72 && waypoint != 56) {
              waypoint = 57;
            } else if (c->oupnt == 54 && waypoint != 52) {
              waypoint = 53;
            } else if (c->oupnt == 39 && waypoint != 37) {
              waypoint = 38;
            } else {
              c->oupnt = waypoint;
            }
          }
          if (checkPoints->stage == 22 && !ext) { // Extended dropped this ambush
            if (!c->gowait) {
              if (checkPoints->clear[0] == 0) {
                c->wtx = -3500; c->wtz = 19000; c->frx = -3500; c->frz = 39000; c->frad = 12000;
                c->oupnt = 37; c->gowait = true; c->afta = false;
              }
              if (checkPoints->clear[0] == 7) {
                c->wtx = -44800; c->wtz = 40320; c->frx = -44800; c->frz = 34720; c->frad = 30000;
                c->oupnt = 27; c->gowait = true; c->afta = false;
              }
              if (checkPoints->clear[0] == 10) {
                c->wtx = 0; c->wtz = 48739; c->frx = 0; c->frz = 38589; c->frad = 90000;
                c->oupnt = 55; c->gowait = true; c->afta = false;
              }
              if (checkPoints->clear[0] == 14) {
                c->wtx = -3500; c->wtz = 19000; c->frx = -14700; c->frz = 39000; c->frad = 45000;
                c->oupnt = 37; c->gowait = true; c->afta = false;
              }
              if (checkPoints->clear[0] == 18) {
                c->wtx = -48300; c->wtz = -4550; c->frx = -48300; c->frz = 5600; c->frad = 90000;
                c->oupnt = 17; c->gowait = true; c->afta = false;
              }
            }
            if (c->gowait) {
              if (control_py(contO->x / 100, c->wtx / 100, contO->z / 100, c->wtz / 100) < 10000 && mad->speed > 50.0f) {
                c->up = false;
              }
              if (control_py(contO->x / 100, c->wtx / 100, contO->z / 100, c->wtz / 100) < 200) {
                c->up = false;
                c->handb = true;
              }
              if (checkPoints->pcleared == c->oupnt &&
                  control_py(checkPoints->opx[0] / 100, c->frx / 100, checkPoints->opz[0] / 100, c->frz / 100) < c->frad) {
                c->afta = true;
                c->gowait = false;
              }
              if (control_py(contO->x / 100, checkPoints->opx[0] / 100, contO->z / 100, checkPoints->opz[0] / 100) < 25) {
                c->afta = true;
                c->gowait = false;
                c->attack = 200;
                c->acr = 0;
              }
            }
          }
          if (!ext_normal && checkPoints->stage == 25) {
            if (c->oupnt == -1) {
              int32_t py = -10;
              for (int32_t o = 0; o < checkPoints->n; o++) {
                if ((checkPoints->typ[o] == -2 || checkPoints->typ[o] == -4) && (o < 50 || o > 54)) {
                  int32_t d = control_py(contO->x / 100, checkPoints->x[o] / 100, contO->z / 100, checkPoints->z[o] / 100);
                  if (d < py || py == -10) { py = d; c->oupnt = o; }
                }
              }
              c->oupnt--;
              if (waypoint < 0) c->oupnt += checkPoints->n;
            }
            if (c->oupnt >= 0 && c->oupnt < checkPoints->n) {
              waypoint = c->oupnt;
              if (control_py(contO->x / 100, checkPoints->x[waypoint] / 100, contO->z / 100, checkPoints->z[waypoint] / 100) < 800) {
                c->oupnt = -jtrunc(75.0f + (medium_random(m) * 200.0f));
                c->runbul = jtrunc(50.0f + (medium_random(m) * 100.0f));
              }
            }
            if (c->oupnt < -1) c->oupnt++;
            if (c->runbul != 0) c->runbul--;
          }
          if (!ext_normal && checkPoints->stage == 26) {
            bool b4 = false;
            if (mad->cn == 13) {
              if (!c->gowait) {
                if (checkPoints->clear[0] == 1) {
                  if (medium_random(m) > 0.5f) {
                    c->wtx = -14000; c->wtz = 48000; c->frx = -5600; c->frz = 47600; c->frad = 88000; c->oupnt = 33;
                  } else {
                    c->wtx = -5600; c->wtz = 8000; c->frx = -7350; c->frz = -4550; c->frad = 22000; c->oupnt = 15;
                  }
                  c->gowait = true;
                  c->afta = false;
                }
                if (checkPoints->clear[0] == 4) {
                  c->wtx = -12700; c->wtz = 14000; c->frx = -31000; c->frz = 1050; c->frad = 11000;
                  c->oupnt = 51; c->gowait = true; c->afta = false;
                }
                if (checkPoints->clear[0] == 14) {
                  c->wtx = -35350; c->wtz = 6650; c->frx = -48300; c->frz = 54950; c->frad = 11000;
                  c->oupnt = 15; c->gowait = true; c->afta = false;
                }
                if (checkPoints->clear[0] == 17) {
                  c->wtx = -42700; c->wtz = 41000; c->frx = -40950; c->frz = 49350; c->frad = 7000;
                  c->oupnt = 42; c->gowait = true; c->afta = false;
                }
                if (checkPoints->clear[0] == 21) {
                  c->wtx = -1750; c->wtz = -15750; c->frx = -25900; c->frz = -14000; c->frad = 11000;
                  c->oupnt = 125; c->gowait = true; c->afta = false;
                }
              }
              if (c->gowait) {
                if (control_py(contO->x / 100, c->wtx / 100, contO->z / 100, c->wtz / 100) < 10000 && mad->speed > 50.0f) {
                  c->up = false;
                }
                if (control_py(contO->x / 100, c->wtx / 100, contO->z / 100, c->wtz / 100) < 200) {
                  c->up = false;
                  c->handb = true;
                }
                if (checkPoints->pcleared == c->oupnt &&
                    control_py(checkPoints->opx[0] / 100, c->frx / 100, checkPoints->opz[0] / 100, c->frz / 100) < c->frad) {
                  c->runbul = 0;
                  c->afta = true;
                  c->gowait = false;
                }
                if (control_py(contO->x / 100, checkPoints->opx[0] / 100, contO->z / 100, checkPoints->opz[0] / 100) < 25) {
                  c->afta = true;
                  c->gowait = false;
                  c->attack = 200;
                  c->acr = 0;
                }
                if (checkPoints->clear[0] == 21 && c->oupnt != 125) c->gowait = false;
              }
              if ((checkPoints->clear[0] >= 11 && !c->gowait) || (mad->power < 60.0f && checkPoints->clear[0] < 21)) {
                b4 = true;
                if (!c->exitattack) {
                  c->oupnt = -1;
                  c->exitattack = true;
                }
              } else if (c->exitattack) {
                c->exitattack = false;
              }
            }
            if (mad->cn == 11) b4 = true;
            if (b4) {
              if (c->oupnt == -1) {
                int32_t py2 = -10;
                for (int32_t o = 0; o < checkPoints->n; o++) {
                  if (checkPoints->typ[o] == -4) {
                    int32_t d = control_py(contO->x / 100, checkPoints->x[o] / 100, contO->z / 100, checkPoints->z[o] / 100);
                    if ((d < py2 && control_rand_gt_rand(m)) || py2 == -10) { py2 = d; c->oupnt = o; }
                  }
                }
                c->oupnt--;
                if (waypoint < 0) c->oupnt += checkPoints->n;
              }
              if (c->oupnt >= 0 && c->oupnt < checkPoints->n) {
                waypoint = c->oupnt;
                if (control_py(contO->x / 100, checkPoints->x[waypoint] / 100, contO->z / 100, checkPoints->z[waypoint] / 100) < 800) {
                  c->oupnt = -jtrunc(75.0f + (medium_random(m) * 200.0f));
                  c->runbul = jtrunc(50.0f + (medium_random(m) * 100.0f));
                }
              }
              if (c->oupnt < -1) c->oupnt++;
              if (c->runbul != 0) c->runbul--;
            }
          }
          mad->nofocus = true;
        }

        // Control.js:1637-1681 -- "forget"/fix-point steering override:
        // when the AI has gone too long without clearing a real
        // checkpoint (missedcp==0 signals a fresh miss) it re-targets
        // the nearest FIX point (the anti-stuck waypoints control_reset
        // pre-computed into c->fpnt[]) instead of the normal advance.
        if (checkPoints->stage != 27 || ext) { // Extended repairs on Classic 17 too
          if (pst == 10 || pst == 19 || (pst == 18 && mad->pcleared == 73) || pst == 26) {
            c->forget = true;
          }
          if ((mad->missedcp == 0 || c->forget || c->trfix == 4) && c->trfix != 0) {
            int32_t n25 = 0;
            if (!ext_normal && (checkPoints->stage == 25 || checkPoints->stage == 26)) n25 = 3;
            if (c->trfix == 2) {
              int32_t py3 = -10;
              int32_t n26 = 0;
              for (int32_t n27 = n25; n27 < checkPoints->fn; n27++) {
                int32_t d = control_py(contO->x / 100, checkPoints->x[c->fpnt[n27]] / 100,
                                        contO->z / 100, checkPoints->z[c->fpnt[n27]] / 100);
                if (d < py3 || py3 == -10) { py3 = d; n26 = n27; }
              }
              if (checkPoints->stage == 18 || checkPoints->stage == 22) n26 = 1;
              if (ext_normal) n26 = 0;   // Control.java:9279, whichfix 0 outside Classic and career
              waypoint = c->fpnt[n26];
              // Java Control.java:1794 -- `forget` is a PERSISTENT field
              // that gates re-entry into this exact block on later ticks,
              // not the inverse. Was `!checkPoints->special[n26]`: a
              // special fix point should LATCH forget=true (keep steering
              // here across ticks) and a normal one should CLEAR it, but
              // the negation flipped both, making bots abandon recovery
              // early on special points and re-trigger it repeatedly on
              // normal ones -- exactly the "wanders after getting stuck"
              // symptom this fixes.
              c->forget = checkPoints->special[n26];
            }
            for (int32_t n28 = n25; n28 < checkPoints->fn; n28++) {
              if (control_py(contO->x / 100, checkPoints->x[c->fpnt[n28]] / 100,
                              contO->z / 100, checkPoints->z[c->fpnt[n28]] / 100) < 2000) {
                c->forget = false;
                c->actwait = 0;
                c->upwait = 0;
                c->turntyp = 2;
                c->randtcnt = -1;
                c->acuracy = 0;
                c->rampp = 0;
                c->trfix = 3;
              }
            }
            if (c->trfix == 3) mad->nofocus = true;
          }
        }

        if (c->turncnt > c->randtcnt) {
          if (!c->gowait) {
            int32_t n29 = (checkPoints->x[waypoint] - contO->x >= 0) ? 180 : 0;
            c->pan = jtrunc_d(90.0 + (double)n29 +
                atan((double)(checkPoints->z[waypoint] - contO->z) / (double)(checkPoints->x[waypoint] - contO->x)) / NFM_DEG);
          } else {
            int32_t n30 = (c->wtx - contO->x >= 0) ? 180 : 0;
            c->pan = jtrunc_d(90.0 + (double)n30 +
                atan((double)(c->wtz - contO->z) / (double)(c->wtx - contO->x)) / NFM_DEG);
          }
          c->turncnt = 0;
          c->randtcnt = jtrunc((float)c->acuracy * medium_random(m));
        } else {
          c->turncnt++;
        }
      } else {
        // Control.js:1701-1727 -- combat aiming (c->attack != 0): steers
        // toward a PREDICTED intercept point ahead of the target (this.acr)
        // rather than its current position, offset along its own heading
        // by aim*(half the closing distance).
        if (!ext || !c->fewsecson) c->up = true;
        int32_t n33, n34;
        if (ext) {
          // Extended keeps the lead distance in an int field, l1 (Control.java:~1905).
          const int32_t l1 = jtrunc((float)control_pys(contO->x, checkPoints->opx[c->acr], contO->z, checkPoints->opz[c->acr]) / 2.0f * c->aim);
          n33 = jtrunc((float)checkPoints->opx[c->acr] - (float)l1 * medium_sin(m, (float)checkPoints->omxz[c->acr]));
          n34 = jtrunc((float)checkPoints->opz[c->acr] + (float)l1 * medium_cos(m, (float)checkPoints->omxz[c->acr]));
        } else {
          float n32 = (control_pys(contO->x, checkPoints->opx[c->acr], contO->z, checkPoints->opz[c->acr]) / 2.0f) * c->aim;
          n33 = jtrunc_d((double)checkPoints->opx[c->acr] - (double)(n32 * medium_sin(m, (float)checkPoints->omxz[c->acr])));
          n34 = jtrunc_d((double)checkPoints->opz[c->acr] + (double)(n32 * medium_cos(m, (float)checkPoints->omxz[c->acr])));
        }
        int32_t n31 = (n33 - contO->x >= 0) ? 180 : 0;
        c->pan = jtrunc_d(90.0 + (double)n31 + atan((double)(n34 - contO->z) / (double)(n33 - contO->x)) / NFM_DEG);
        c->attack--;
        if (c->attack <= 0) c->attack = 0;
        if (pst == 25 && c->exitattack && !c->bulistc && mad->missedcp != 0) c->attack = 0;
        if (ext && pst == 11 && c->exitattack) c->attack = 0; // Control.java:~1925
        // Control.java:5813, Extended's cars 13 and 36 (this port's 29 and 13)
        if (pst == 26 && (mad->cn == 13 || (ext && mad->cn == 29)) &&
            (checkPoints->clear[0] == 4 || checkPoints->clear[0] == 13 || checkPoints->clear[0] == 21)) {
          c->attack = 0;
        }
        if (pst == 26 && mad->missedcp != 0 &&
            (checkPoints->pos[mad->im] == 0 || (checkPoints->pos[mad->im] == 1 && checkPoints->pos[0] == 0))) {
          c->attack = 0;
        }
        if (pst == 26 && checkPoints->pos[0] > checkPoints->pos[mad->im] && mad->power < 80.0f) {
          c->attack = 0;
        }
      }

      // ======================================================================
      // SECTION 3 -- turn the `pan` bearing computed above into actual
      // left/right/handb/down input. Control.js:1728-1789. Shared by
      // both the navigation and combat-aiming branches above.
      // ======================================================================
      int32_t xz2 = contO->xz;
      if (c->zyinv) xz2 += 180;
      while (xz2 < 0) xz2 += 360;
      while (xz2 > 180) xz2 -= 360;
      while (c->pan < 0) c->pan += 360;
      while (c->pan > 180) c->pan -= 360;

      if (c->wall != -1 && c->hold == 0) c->clrnce = 0;
      if (c->hold == 0) {
        if (abs(xz2 - c->pan) < 180) {
          if (abs(xz2 - c->pan) > c->clrnce) {
            if (xz2 < c->pan) { c->left = true; c->lastl = true; }
            else { c->right = true; c->lastl = false; }
            if (abs(xz2 - c->pan) > 50 && mad->speed > (float)mad->cd->swits[mad->cn][0] && c->turntyp != 0) {
              if (c->turntyp == 1 && (!ext || c->stuck == 0)) c->down = true;
              if (c->turntyp == 2) c->handb = true;
              if (!c->agressed) c->up = false;
            }
          }
        } else if (abs(xz2 - c->pan) < 360 - c->clrnce) {
          if (xz2 < c->pan) { c->right = true; c->lastl = false; }
          else { c->left = true; c->lastl = true; }
          if (abs(xz2 - c->pan) < 310 && mad->speed > (float)mad->cd->swits[mad->cn][0] && c->turntyp != 0) {
            if (c->turntyp == 1 && (!ext || c->stuck == 0)) c->down = true;
            if (c->turntyp == 2) c->handb = true;
            if (!c->agressed) c->up = false;
          }
        }
      }

      // ======================================================================
      // SECTION 4 -- wall-avoidance/recovery. Control.js:1790-1828.
      // ======================================================================
      if (!ext_normal && checkPoints->stage == 24 && c->wall != -1) {
        if (trackers->dam[c->wall] == 0 || mad->pcleared == 45) c->wall = -1;
        if (mad->pcleared == 58 && checkPoints->opz[mad->im] < 36700) { c->wall = -1; c->hold = 0; }
      }
      if (c->wall != -1) {
        if (c->lwall != c->wall) {
          if (c->lastl) c->left = true; else c->right = true;
          c->wlastl = c->lastl;
          c->lwall = c->wall;
        } else if (c->wlastl) {
          c->left = true;
        } else {
          c->right = true;
        }
        if (ext) {
          // Extended (Control.java:~9690-9712): pinned against a wall for
          // over 70 ticks, the car stops trying to drive on and reverses.
          if (c->stuck <= 70) {
            c->down = false;
            c->fewsecson = false;
            c->stuck++;
            c->downuse = 0;
          } else {
            c->downuse++;
            c->up = false;
            if (c->downuse <= 5) {
              c->down = false;
              c->fewsecson = false;
            } else {
              c->fewsecson = true;
            }
          }
        }
        if (trackers->dam[c->wall] != 0) {
          int32_t n35 = (trackers->skd[c->wall] == 1) ? 3 : 1;
          c->hold += n35;
          if (c->hold > 10 * n35) c->hold = 10 * n35;
        } else {
          c->hold = ext ? 0 : 1;
        }
        c->wall = -1;
      } else {
        if (c->hold != 0) c->hold--;
        if (ext) c->stuck = 0;
      }
      if (ext) {
        // ...for 40 ticks, then drives on.
        if (!c->fewsecson) {
          c->fewsecs = 0;
        } else {
          c->fewsecs++;
          c->up = false;
          if (c->fewsecs >= 40) {
            c->down = false;
            c->fewsecson = false;
          } else {
            c->down = true;
          }
        }
        c->apunch = 0;
      }
    } else {
      // ======================================================================
      // SECTION 5 -- airborne stunt/trick control + landing correction,
      // taken instead of SECTIONS 2-4 whenever the car isn't touching the
      // ground (per usebounce, either wtouch or mtouch). Control.js:1829-
      // 2166. The three `trickfase` checks below run as separate `if`s
      // (NOT else-if), matching the source exactly -- a fase 0->1
      // transition falls straight through into fase 1's body the SAME
      // tick.
      // ======================================================================
      if (c->trickfase == 0) {
        float n36f = (mad->scy[0] + mad->scy[1] + mad->scy[2] + mad->scy[3]) * (float)(contO->y - 300);
        int32_t n36 = jtrunc(n36f / 4000.0f);
        int32_t n37 = 3;
        if (pst == 25) n37 = 10;
        if (n36 > 7 && (medium_random(m) > (c->trickprf / (float)n37) || c->stuntf == 4 || c->stuntf == 3 ||
                        c->stuntf == 5 || c->stuntf == 6 || pst == 26)) {
          c->oxy = mad->pxy;
          c->ozy = mad->pzy;
          c->flycnt = 0;
          c->uddirect = 0;
          c->lrdirect = 0;
          c->udswt = false;
          c->lrswt = false;
          c->trickfase = 1;
          if (n36 < 16) {
            if (c->stuntf != 6) {
              c->uddirect = -1;
              c->udstart = 0;
              c->udswt = false;
            } else if (ext || c->oupnt != 70) {
              c->uddirect = 1;
              c->udstart = 0;
              c->udswt = false;
            }
          } else if ((control_rand_gt_rand(m) && c->stuntf != 1) || c->stuntf == 4 || c->stuntf == 6 ||
                     c->stuntf == 7 || c->stuntf == 17) {
            if ((control_rand_gt_rand(m) || c->stuntf == 2 || c->stuntf == 7) && c->stuntf != 4 && c->stuntf != 6) {
              c->uddirect = -1;
            } else {
              c->uddirect = 1;
            }
            c->udstart = jtrunc((10.0f * medium_random(m)) * c->trickprf);
            if (c->stuntf == 6 || (ext && c->stuntf == 4)) c->udstart = 0;
            if (pst == 26) c->udstart = 0;
            if (!ext_normal && checkPoints->stage == 24 && (c->oupnt == 68 || c->oupnt == 69)) {
              c->apunch = 20;
              c->oupnt = 70;
            }
            if (medium_random(m) > 0.85f && c->stuntf != 4 && c->stuntf != 3 && c->stuntf != 6 &&
                c->stuntf != 17 && pst != 26) {
              c->udswt = true;
            }
            if (medium_random(m) > (c->trickprf + 0.3f) && c->stuntf != 4 && c->stuntf != 6) {
              c->lrdirect = control_rand_gt_rand(m) ? -1 : 1;
              c->lrstart = jtrunc(30.0f * medium_random(m));
              if (medium_random(m) > 0.75f) c->lrswt = true;
            }
          } else {
            c->lrdirect = control_rand_gt_rand(m) ? -1 : 1;
            c->lrstart = jtrunc((10.0f * medium_random(m)) * c->trickprf);
            if (medium_random(m) > 0.75f && pst != 26) c->lrswt = true;
            if (medium_random(m) > (c->trickprf + 0.3f)) {
              c->uddirect = control_rand_gt_rand(m) ? -1 : 1;
              c->udstart = jtrunc(30.0f * medium_random(m));
              if (medium_random(m) > 0.85f) c->udswt = true;
            }
          }
          if ((c->trfix == 3 || c->trfix == 4) && !ext_normal) {   // Extended's normal mode adds nothing here
            if (checkPoints->stage != 18 && checkPoints->stage != 8) {
              if (checkPoints->stage != 25 && c->lrdirect == -1) {
                c->uddirect = (checkPoints->stage != 19) ? -1 : 1;
              }
              c->lrdirect = 0;
              if ((checkPoints->stage == 19 || checkPoints->stage == 25) && c->uddirect == -1) c->uddirect = 1;
              if (mad->power < 60.0f) c->uddirect = -1;
            } else {
              if (c->uddirect != 0) c->uddirect = -1;
              c->lrdirect = 0;
            }
            if (checkPoints->stage == 20 && !ext) { // gone in Extended
              c->uddirect = 1;
              c->lrdirect = 0;
            }
            if (checkPoints->stage == 26) {
              c->uddirect = -1;
              c->lrdirect = 0;
              if (mad->cn != 11 && mad->cn != 13) {
                c->udstart = 7;
                if (mad->cn == 14 && mad->power > 30.0f) c->udstart = 14;
              } else {
                c->udstart = 0;
              }
              if (mad->cn == 11) {
                c->lrdirect = -1;
                c->lrstart = 0;
              }
            }
          }
        } else {
          c->trickfase = -1;
        }
        if (!c->afta) c->afta = true;
        if (c->trfix == 3) {
          c->trfix = 4;
          c->statusque += 30;
        }
      }
      if (c->trickfase == 1) {
        c->flycnt++;
        if (c->lrdirect != 0 && c->flycnt > c->lrstart) {
          if (c->lrswt && abs(mad->pxy - c->oxy) > 180) {
            c->lrdirect = (c->lrdirect == -1) ? 1 : -1;
            c->lrswt = false;
          }
          if (c->lrdirect == -1) {
            c->handb = true;
            c->left = true;
          } else {
            c->handb = true;
            c->right = true;
          }
        }
        if (c->uddirect != 0 && c->flycnt > c->udstart) {
          if (c->udswt && abs(mad->pzy - c->ozy) > 180) {
            c->uddirect = (c->uddirect == -1) ? 1 : -1;
            c->udswt = false;
          }
          if (c->uddirect == -1) {
            c->handb = true;
            c->down = true;
          } else {
            c->handb = true;
            c->up = true;
            if (c->apunch > 0) {
              c->down = true;
              c->apunch--;
            }
          }
        }
        float landingRatio = (mad->scy[0] + mad->scy[1] + mad->scy[2] + mad->scy[3]) * 100.0f;
        if (landingRatio / (float)(contO->y - 300) < -(float)c->saftey) {
          c->onceu = false;
          c->onced = false;
          c->oncel = false;
          c->oncer = false;
          c->lrcomp = false;
          c->udcomp = false;
          c->udbare = false;
          c->lrbare = false;
          c->trickfase = 2;
          c->swat = 0;
        }
      }
      if (c->trickfase == 2) {
        if (c->swat == 0) {
          if (mad->dcomp != 0.0f || mad->ucomp != 0.0f) c->udbare = true;
          if (mad->lcomp != 0.0f || mad->rcomp != 0.0f) c->lrbare = true;
          c->swat = 1;
        }
        if (mad->wtouch) {
          if (c->swat == 1) c->swat = 2;
        } else if (c->swat == 2) {
          if (mad->capsized && medium_random(m) > c->mustland) {
            if (c->udbare) {
              c->lrbare = true;
              c->udbare = false;
            } else if (c->lrbare) {
              c->udbare = true;
              c->lrbare = false;
            }
          }
          c->swat = 3;
        }
        if (c->udbare) {
          int32_t a = mad->pzy + 90;
          while (a < 0) a += 360;
          while (a > 180) a -= 360;
          int32_t absA = abs(a);
          if (mad->lcomp - mad->rcomp < 5.0f && (c->onced || c->onceu)) c->udcomp = true;
          if (mad->dcomp > mad->ucomp) {
            if (mad->capsized) {
              if (c->udcomp) {
                if (absA > 90) c->up = true; else c->down = true;
              } else if (!c->onced) {
                c->down = true;
              }
            } else {
              if (c->udcomp) {
                if (c->perfection && abs(absA - 90) > 30) {
                  if (absA > 90) c->up = true; else c->down = true;
                }
              } else if (medium_random(m) > c->mustland) {
                c->up = true;
              }
              c->onced = true;
            }
          } else if (mad->capsized) {
            if (c->udcomp) {
              if (absA > 90) c->up = true; else c->down = true;
            } else if (!c->onceu) {
              c->up = true;
            }
          } else {
            if (c->udcomp) {
              if (c->perfection && abs(absA - 90) > 30) {
                if (absA > 90) c->up = true; else c->down = true;
              }
            } else if (medium_random(m) > c->mustland) {
              c->down = true;
            }
            c->onceu = true;
          }
        }
        if (c->lrbare) {
          int32_t a2 = mad->pxy + 90;
          if (c->zyinv) a2 += 180;
          while (a2 < 0) a2 += 360;
          while (a2 > 180) a2 -= 360;
          int32_t absA2 = abs(a2);
          if (mad->lcomp - mad->rcomp < 10.0f && (c->oncel || c->oncer)) c->lrcomp = true;
          if (mad->lcomp > mad->rcomp) {
            if (mad->capsized) {
              if (c->lrcomp) {
                if (absA2 > 90) c->left = true; else c->right = true;
              } else if (!c->oncel) {
                c->left = true;
              }
            } else {
              if (c->lrcomp) {
                if (c->perfection && abs(absA2 - 90) > 30) {
                  if (absA2 > 90) c->left = true; else c->right = true;
                }
              } else if (medium_random(m) > c->mustland) {
                c->right = true;
              }
              c->oncel = true;
            }
          } else if (mad->capsized) {
            if (c->lrcomp) {
              if (absA2 > 90) c->left = true; else c->right = true;
            } else if (!c->oncer) {
              c->right = true;
            }
          } else {
            if (c->lrcomp) {
              if (c->perfection && abs(absA2 - 90) > 30) {
                if (absA2 > 90) c->left = true; else c->right = true;
              }
            } else if (medium_random(m) > c->mustland) {
              c->left = true;
            }
            c->oncer = true;
          }
        }
      }
    }
  }
}
