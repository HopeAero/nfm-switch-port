// Tests native/core/control.c's career AI (control_preform_career, the
// generated control_career.inc, and contva_sortvariables) against the real
// web/ext/Control.js and Contva.js.
//
// Both sides build the same random race state from a seed -- this file's
// gen_case()/gen_tick() and tools/control_career_oracle.mjs's mirror of them
// draw from one xorshift32 in the same order -- then run preform for a few
// ticks with Medium.random() replaying a fixed list of tenths and
// Math.random() on java.js's seeded stream, and fold every field preform
// can touch (Control, the car's Madness, Contva, xtGraphics.findi/endsp, how
// many draws each stream made) into an FNV-1a digest per tick. The table
// below is the oracle's digests (node tools/control_career_oracle.mjs
// <path to nfm-master/web/ext> > control_career_cases.h); every stage 1-31
// is run, and the bonus stages hanging off 5, 11, 15 and 18.
//
// The integers the state is drawn from favour the constants preform tests
// (pcleared/point/oupnt/clear values), so the per-stage scripted branches
// are reached; coverage of control_career.inc under this test is noted in
// the commit that added it.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "../core/control.h"
#include "../core/mad.h"
#include "../core/check_points.h"
#include "../core/cont_o.h"
#include "../core/trackers.h"
#include "../core/car_define.h"
#include "../core/medium.h"
#include "../core/java_compat.h"
#include "../core/xt_graphics.h"

#include "control_career_cases.h"

static int failures = 0;

// ---- the shared generator ----------------------------------------------------

static uint32_t g_s;
static uint32_t gnext(void) {
  uint32_t x = g_s;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  g_s = x;
  return x;
}
static int32_t gi(int32_t n) { return (int32_t)(gnext() % (uint32_t)n); }
static bool gb(int32_t pct) { return gi(100) < pct; }

// The integers preform compares pcleared/point/oupnt/clear and friends with.
static const int32_t POOL[] = {
    -1,  0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,  15,  16,  17,  18,  19,
    20,  21,  22,  23,  24,  25,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,
    41,  42,  43,  44,  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,
    62,  63,  64,  65,  66,  67,  68,  69,  70,  71,  72,  73,  74,  75,  76,  77,  78,  79,  80,  81,  82,
    83,  84,  85,  86,  87,  88,  89,  90,  91,  93,  95,  96,  99,  100, 101, 103, 105, 106, 107, 110, 111,
    113, 114, 115, 117, 120, 122, 124, 125, 126, 127, 128, 130, 131, 132, 134, 140, 142, 144, 148, 149, 151,
    156, 158, 159, 162, 165, 166, 172, 173, 175, 176, 177, 182, 183, 184, 188, 189, 190, 193, 195, 197, 198,
    199, 203, 206, 207, 208, 210, 211, 221, 226, 230, 234, 240, 243, 248, 259, 272, 278, 285, 289, 292, 297,
    298, 301, 305, 311, 316, 320, 321, 331, 332};
#define NPOOL ((int32_t)(sizeof(POOL) / sizeof(POOL[0])))
static int32_t gpool(void) { return POOL[gi(NPOOL)]; }
// A route index: in range for the 340 points every case's track has.
static int32_t groute(void) {
  int32_t v = gpool();
  return v < 0 ? 0 : v;
}
static float gtenth(void) { return (float)((double)gi(10) / 10.0); }
static float gpower(void) {
  static const float P[] = {98.0f, 98.0f, 90.0f, 80.0f, 75.0f, 70.0f, 65.0f, 60.0f, 50.0f, 30.0f};
  return P[gi(10)];
}

#define NPTS 340   // route points on every generated track
#define NTRK 64    // trackers
#define NRLOG 61   // Medium.random's replayed tenths

// Extended's car number (the JS's) -> this port's.
static int32_t port_cn(int32_t x) { return x >= 23 ? x - 23 : x + 16; }

typedef struct {
  Medium m;
  CarDefine cd;
  XtGraphicsStub xt;
  Mad mad[NFM_MAX_CARS];
  ContO conto;
  CheckPoints cp;
  Trackers tr;
  Control c;
  Control u[NFM_MAX_CARS];   // the race's Controls, for contva_sortvariables' u[im].trfix
  int32_t nplayers, im, stage;
} World;

static World W;

static void gen_tables(void) {
  for (int32_t x = 0; x < 39; x++) {
    const int32_t k = port_cn(x);
    W.cd.maxmag[k] = 200 + gi(2000);
    for (int32_t j = 0; j < 3; j++) W.cd.swits[k][j] = 50 + gi(450);
    W.cd.moment[k] = (float)((double)(5 + gi(60)) / 10.0);
  }
}

static void gen_mad(Mad *a, bool full) {
  // Half the time one of the values this stage's code tests pcleared against.
  const int32_t np = CC_SPOOLN[W.stage];
  if (np > 0 && gb(60)) {
    a->pcleared = CC_SPOOL[W.stage][gi(np)];
  } else {
    a->pcleared = gpool();
  }
  a->clear = gi(40);
  a->point = groute();
  a->power = gpower();
  a->speed = (float)gi(600);
  a->hitmag = gi(2200);
  a->specialact = gb(20);
  a->mtouch = gb(85);
  a->wtouch = gb(70);
  if (!full) return;
  a->frozen = gb(10);
  a->redstr = gb(10);
  a->strswap = gb(10);
  a->leech = gb(10);
  a->dest = gb(3);
  a->gtouch = gb(60);
  a->capsized = gb(15);
  for (int32_t j = 0; j < 4; j++) a->scy[j] = (float)(gi(400) - 200);
  a->pxy = gi(720) - 360;
  a->pzy = gi(720) - 360;
  a->lcomp = (float)gi(30);
  a->rcomp = (float)gi(30);
  a->ucomp = (float)gi(30);
  a->dcomp = (float)gi(30);
  a->missedcp = gi(3) - 1;
  a->nlaps = gi(3);
  a->nofocus = gb(30);
  a->spatk = (float)gi(121);
}

static void gen_others(void) {
  CheckPoints *cp = &W.cp;
  for (int32_t k = 0; k < W.nplayers; k++) {
    cp->pos[k] = gi(W.nplayers);
    cp->clear[k] = gi(40);
    cp->dested[k] = gb(15) ? 1 : 0;
    cp->opx[k] = gi(120000) - 60000;
    cp->opz[k] = gi(120000) - 60000;
    cp->omxz[k] = gi(720) - 360;
  }
  cp->wasted = gi(W.nplayers);
  cp->pcleared = gpool();
  W.conto.x = gi(120000) - 60000;
  W.conto.z = gi(120000) - 60000;
  W.conto.y = gi(2000) - 1500;
  W.conto.xz = gi(720) - 360;
  W.conto.fix = gb(5);
  // Often near a route point, and others near it, so the distance tests fire.
  if (gb(40)) {
    const int32_t j = groute();
    W.conto.x = cp->x[j] + gi(3000) - 1500;
    W.conto.z = cp->z[j] + gi(3000) - 1500;
  }
  for (int32_t k = 0; k < W.nplayers; k++) {
    if (gb(30)) {
      cp->opx[k] = W.conto.x + gi(4000) - 2000;
      cp->opz[k] = W.conto.z + gi(4000) - 2000;
    }
  }
}

static void gen_case(uint32_t seed, int32_t stage, int32_t bonus) {
  memset(&W.cd, 0, sizeof W.cd);
  memset(&W.cp, 0, sizeof W.cp);
  memset(&W.conto, 0, sizeof W.conto);
  memset(W.mad, 0, sizeof W.mad);
  xt_graphics_stub_init(&W.xt);
  trackers_init(&W.tr);
  g_s = seed;
  W.stage = stage;

  W.nplayers = 7 + gi(13);
  W.im = 1 + gi(W.nplayers - 1);
  XtCareerAI *cx = &W.xt.career;
  W.xt.extended = true;
  cx->careermode = true;
  cx->nplayers = W.nplayers;
  cx->bonus = bonus;
  cx->hardstage = gb(30);
  cx->unlocked = 1 + gi(31);
  for (int32_t k = 0; k < 39; k++) cx->extpoints[k] = gi(40);
  for (int32_t k = 0; k < W.nplayers; k++) {
    // The AI's car: half the time one this stage's code names.
    const int32_t nc = CC_SCARSN[stage];
    if (k == W.im && nc > 0 && gb(50)) {
      cx->sc[k] = CC_SCARS[stage][gi(nc)];
    } else {
      cx->sc[k] = gi(39);
    }
    cx->beastopponent[k] = gb(25);
    cx->undead[k] = gb(20);
    cx->entered[k] = gb(50);
    cx->fixspecials[k] = gb(20);
    cx->floor[k] = gi(4);
    cx->randomcar[k] = gi(W.nplayers);
    cx->undeadlock[k] = gi(W.nplayers);
    cx->beast[k] = gb(25);
    cx->shadowcar[k] = gb(15);
    cx->level[k] = 1 + gi(30);
    cx->aistrsp[k] = gi(40);
    cx->nostunts[k] = gi(3);
    cx->nofix[k] = gb(20);
    cx->groundlevel[k] = gb(50) ? 0.0f : (float)(-10000 * gi(4));
    cx->floorguardian[k] = gb(15);
    cx->guardswitch[k] = gb(50);
    cx->botbreak[k] = gb(20);
    cx->endsp[k] = gi(60);
  }
  cx->undeadtarget = gi(W.nplayers);
  cx->verydark = gb(30);
  // xtGraphics only aims a car at targetcar while verydark holds, and then
  // it is a slot (100 would read past every per-car array here).
  cx->targetcar = cx->verydark ? gi(W.nplayers) : (gb(50) ? 100 : gi(W.nplayers));
  cx->bossbattle = gb(30);
  cx->invulnerable = gb(30);

  XtContva *cv = &cx->contva;
  for (int32_t k = 0; k < NFM_MAX_CARS; k++) {
    cv->vulnerable[k] = gb(15);
    cv->freeze[k] = gb(10);
    cv->weaken[k] = gb(10);
    cv->biglead[k] = gb(20);
    cv->completed[k] = gi(101);
    cv->needhelp[k] = gb(20);
    cv->nearchk[k] = gb(30);
    cv->chkcircle[k] = gi(200);
    cv->moreslow[k] = gi(81);
    cv->slowdown[k] = 150 + gi(100);
    cv->slowrange[k] = gi(9000);
    cv->opbackloops[k] = gb(10);
    cv->dontstunt[k] = gb(20);
    cv->spdexception[k] = gb(20);
    cv->dontmiss[k] = gb(30);
    cv->layoff[k] = gb(20);
    cv->sharpturn[k] = gi(3);
  }
  cv->lotswasted = gb(30);
  cv->numfixes = 8;
  for (int32_t k = 0; k < 8; k++) cv->fixpoint[k] = gi(NPTS);
  cv->whichfix = gi(8);

  gen_tables();
  for (int32_t k = 0; k < W.nplayers; k++) {
    W.mad[k].cn = port_cn(cx->sc[k]);
    W.mad[k].im = k;
    W.mad[k].cd = &W.cd;
    W.mad[k].xt = &W.xt;
    gen_mad(&W.mad[k], k == W.im);
  }
  cx->usermad = &W.mad[0];

  CheckPoints *cp = &W.cp;
  cp->stage = stage;
  cp->n = NPTS;
  cp->nsp = 1 + gi(20);
  cp->nlaps = 1 + gi(3);
  cp->nplayers = W.nplayers;
  for (int32_t i = 0; i < NPTS; i++) {
    cp->x[i] = gi(120000) - 60000;
    cp->z[i] = gi(120000) - 60000;
    cp->typ[i] = gi(9) - 4;
    cp->floor[i] = gi(4);
    cp->telefloor[i] = gi(4);
    cp->chkcode[i] = gi(4);
  }
  cp->fn = 1 + gi(8);
  for (int32_t i = 0; i < 8; i++) {
    cp->fx[i] = gi(120000) - 60000;
    cp->fz[i] = gi(120000) - 60000;
  }
  gen_others();
  for (int32_t i = 0; i < NTRK; i++) {
    W.tr.dam[i] = gb(50) ? 0 : 1;
    W.tr.skd[i] = gi(3);
  }

  // The AI's Control: everything preform reads, drawn.
  Control *c = &W.c;
  control_init(c, &W.m);
  c->pan = gi(360) - 180;
  c->attack = gb(50) ? 0 : gi(300);
  c->acr = gi(W.nplayers);
  c->afta = gb(50);
  for (int32_t k = 0; k < 50; k++) c->fpnt[k] = gi(NPTS);
  c->trfix = gi(5);
  c->forget = gb(30);
  c->bulistc = gb(30);
  c->runbul = gi(100);
  c->acuracy = gi(40);
  c->upwait = gi(40);
  c->agressed = gb(50);
  c->skiplev = gtenth();
  c->clrnce = gi(10);
  c->rampp = gi(5) - 2;
  c->turntyp = gi(3);
  c->aim = gtenth();
  c->saftey = gi(30);
  c->perfection = gb(50);
  c->mustland = gtenth();
  c->usebounce = gb(20);
  c->trickprf = gtenth();
  c->stuntf = gi(15);
  c->zyinv = gb(20);
  c->lastl = gb(50);
  c->wlastl = gb(50);
  c->hold = gb(70) ? 0 : gi(40);
  c->wall = gb(60) ? -1 : gi(NTRK);
  c->lwall = gb(50) ? -1 : gi(NTRK);
  c->stcnt = gi(30);
  c->statusque = gi(20);
  c->turncnt = gi(10);
  c->randtcnt = gi(10);
  c->upcnt = gi(40);
  c->trickfase = gi(4) - 1;
  c->swat = gi(4);
  c->udcomp = gb(50);
  c->lrcomp = gb(50);
  c->udbare = gb(50);
  c->lrbare = gb(50);
  c->onceu = gb(50);
  c->onced = gb(50);
  c->oncel = gb(50);
  c->oncer = gb(50);
  c->lrdirect = gi(3) - 1;
  c->uddirect = gi(3) - 1;
  c->lrstart = gi(30);
  c->udstart = gi(30);
  c->oxy = gi(720) - 360;
  c->ozy = gi(720) - 360;
  c->flycnt = gi(40);
  c->lrswt = gb(30);
  c->udswt = gb(30);
  c->gowait = gb(20);
  c->actwait = gi(40);
  c->cntrn = gi(6);
  c->revstart = gb(70) ? 0 : gi(30);
  c->oupnt = gb(10) ? -gi(200) : gpool();
  c->wtz = gi(120000) - 60000;
  c->wtx = gi(120000) - 60000;
  c->frx = gi(120000) - 60000;
  c->frz = gi(120000) - 60000;
  c->frad = gi(90000);
  c->apunch = gi(21);
  c->exitattack = gb(30);
  c->stuck = gi(90);
  c->downuse = gi(10);
  c->fewsecs = gi(50);
  c->fewsecson = gb(20);
  c->fixby = 40 + gi(61);
  c->abboost = gb(70) ? 0 : gi(4000);
  c->abdelay = gi(4);
  c->campchk = gi(NPTS + 1) - 1;
  c->campcool = gi(100);
  c->chkahead = gi(5);
  c->waitforuser = gb(20);
  c->intercept = gb(20);
  c->l1 = gi(2000);
  c->l3 = gi(120000) - 60000;
  c->k5 = gi(120000) - 60000;
  c->backfix = gb(30);
  c->switchspot = gb(50);
  c->waited = gb(30);
  c->delayturn = gb(30);
  c->dontback = gb(30);
  c->needtofix = gb(30);
  c->staythere = gi(60);
  c->waitman = gi(60);
  c->wrongfloor = gb(30);
  c->setfixfloor = gb(30);
  c->gotofloor = gi(4);
  for (int32_t k = 0; k < NFM_MAX_CARS; k++) {
    c->neverhit[k] = gb(30);
    c->xavoidnlev[k] = gb(50) ? 0 : gi(20000);
  }
  for (int32_t k = 0; k < NFM_MAX_CARS; k++) {
    memset(&W.u[k], 0, sizeof W.u[k]);
    W.u[k].trfix = gi(5);
  }

  // Medium.random(): NRLOG tenths, replayed from the start.
  for (int32_t i = 0; i < NRLOG; i++) W.m.rlog[i] = gtenth();
  W.m.rn = NRLOG;
  W.m.rp = 0;
  W.m.interpolating = true;
  nfm_set_seed(gnext());
}

// Between ticks: the race moves on.
static void gen_tick(void) {
  gen_mad(&W.mad[W.im], false);
  W.mad[0].speed = (float)gi(600);
  W.mad[0].specialact = gb(20);
  gen_others();
  for (int32_t k = 0; k < W.nplayers; k++) W.xt.career.floor[k] = gi(4);
  if (gb(30)) W.c.stcnt = W.c.statusque + 1;   // make the decision block run more often
}

// ---- the digest ----------------------------------------------------------------

static uint32_t g_h;
static bool g_dump = false;
static int32_t g_dumpi = 0;
static void hw(uint32_t w) {
  if (g_dump) printf("%d %d\n", g_dumpi++, (int32_t)w);
  g_h ^= w;
  g_h *= 16777619u;
}
static void hi(int32_t v) { hw((uint32_t)v); }
static void hf(float v) {
  uint32_t w;
  memcpy(&w, &v, 4);
  hw(w);
}

static void digest_tick(void) {
  const Control *c = &W.c;
  hi(c->pan); hi(c->attack); hi(c->acr); hi(c->afta); hi(c->trfix); hi(c->forget); hi(c->bulistc);
  hi(c->runbul); hi(c->acuracy); hi(c->upwait); hi(c->agressed); hf(c->skiplev); hi(c->clrnce); hi(c->rampp);
  hi(c->turntyp); hf(c->aim); hi(c->saftey); hi(c->perfection); hf(c->mustland); hi(c->usebounce);
  hf(c->trickprf); hi(c->stuntf); hi(c->zyinv); hi(c->lastl); hi(c->wlastl); hi(c->hold); hi(c->wall);
  hi(c->lwall); hi(c->stcnt); hi(c->statusque); hi(c->turncnt); hi(c->randtcnt); hi(c->upcnt);
  hi(c->trickfase); hi(c->swat); hi(c->udcomp); hi(c->lrcomp); hi(c->udbare); hi(c->lrbare); hi(c->onceu);
  hi(c->onced); hi(c->oncel); hi(c->oncer); hi(c->lrdirect); hi(c->uddirect); hi(c->lrstart); hi(c->udstart);
  hi(c->oxy); hi(c->ozy); hi(c->flycnt); hi(c->lrswt); hi(c->udswt); hi(c->gowait); hi(c->actwait);
  hi(c->cntrn); hi(c->revstart); hi(c->oupnt); hi(c->wtz); hi(c->wtx); hi(c->frx); hi(c->frz); hi(c->frad);
  hi(c->apunch); hi(c->exitattack); hi(c->stuck); hi(c->downuse); hi(c->fewsecs); hi(c->fewsecson);
  hi(c->fixby); hi(c->abboost); hi(c->abdelay); hi(c->campchk); hi(c->campcool); hi(c->chkahead);
  hi(c->waitforuser); hi(c->intercept); hi(c->l1); hi(c->l3); hi(c->k5); hi(c->backfix); hi(c->switchspot);
  hi(c->waited); hi(c->delayturn); hi(c->dontback); hi(c->needtofix); hi(c->staythere); hi(c->waitman);
  hi(c->wrongfloor); hi(c->setfixfloor); hi(c->gotofloor); hi(c->left); hi(c->right); hi(c->up); hi(c->down);
  hi(c->handb); hi(c->spatk);
  for (int32_t k = 0; k < 50; k++) hi(c->fpnt[k]);
  for (int32_t k = 0; k < NFM_MAX_CARS; k++) {
    hi(c->neverhit[k]);
    hi(c->xavoidnlev[k]);
  }
  const Mad *a = &W.mad[W.im];
  hi(a->pcleared); hi(a->clear); hi(a->nofocus);
  const XtCareerAI *cx = &W.xt.career;
  const XtContva *cv = &cx->contva;
  for (int32_t k = 0; k < NFM_MAX_CARS; k++) {
    hi(cv->nearchk[k]); hi(cv->dontmiss[k]); hi(cv->dontstunt[k]); hi(cv->sharpturn[k]);
    hi(cv->slowrange[k]); hi(cx->findi[k]);
  }
  hi(cv->whichfix);
  for (int32_t k = 0; k < XT_CAREER_ENDSP; k++) hi(cx->endsp[k]);
  hi(W.m.rp);
  hw((uint32_t)(nfm_random() * 4294967296.0));   // Math.random draws so far
}

// contva_sortvariables against Contva.js, folded in too.
static void digest_contva(void) {
  const XtContva *cv = &W.xt.career.contva;
  for (int32_t k = 0; k < NFM_MAX_CARS; k++) {
    hi(cv->vulnerable[k]); hi(cv->freeze[k]); hi(cv->swapped[k]); hi(cv->leeching[k]); hi(cv->weaken[k]);
    hi(cv->specon[k]); hi(cv->biglead[k]); hi(cv->hugelead[k]); hi(cv->completed[k]); hi(cv->needhelp[k]);
    hi(cv->chkcircle[k]); hi(cv->moreslow[k]); hi(cv->slowdown[k]); hi(cv->slowrange[k]);
    hi(cv->opbackloops[k]); hi(cv->dontdistract[k]); hi(cv->spdexception[k]); hi(cv->layoff[k]);
  }
  hi(cv->lotswasted);
}

static uint32_t run_case(uint32_t seed, int32_t stage, int32_t bonus) {
  gen_case(seed, stage, bonus);
  g_h = 2166136261u;
  for (int32_t t = 0; t < CC_TICKS; t++) {
    if (t != 0) gen_tick();
    control_preform(&W.c, &W.mad[W.im], &W.conto, &W.cp, &W.tr);
    digest_tick();
  }
  for (int32_t k = 0; k < W.nplayers; k++) {
    contva_sortvariables(&W.xt.career.contva, &W.mad[k], &W.cp, W.u, true, W.nplayers, &W.xt.career);
  }
  digest_contva();
  return g_h;
}

// control_reset_career against Control.reset, for every stage.
static uint32_t run_reset(uint32_t seed, int32_t stage, int32_t bonus) {
  gen_case(seed, stage, bonus);
  g_h = 2166136261u;
  control_reset_career(&W.c, &W.cp, W.mad[W.im].cn, &W.xt, &W.mad[W.im]);
  digest_tick();
  return g_h;
}

int main(void) {
  medium_init(&W.m);
  // CC_DUMP=<stage>,<bonus>,<seed>[,reset]: print every digested word, as the
  // oracle's --dump does, to find the first field that differs.
  const char *d = getenv("CC_DUMP");
  if (d != NULL) {
    int st = 0, bon = 0, rs = 0;
    unsigned int seed = 0;
    if (sscanf(d, "%d,%d,%u,%d", &st, &bon, &seed, &rs) < 3) return 2;
    g_dump = true;
    const uint32_t r = rs ? run_reset(seed, st, bon) : run_case(seed, st, bon);
    printf("digest 0x%08x\n", r);
    medium_free(&W.m);
    return 0;
  }
  // Each group: ncases preform runs and nresets resets, seeds stepped by an LCG
  // from seed0, their digests chained. CC_LIST=1 prints each case's digest
  // (the oracle's --list prints Control.js's) to find the case that differs.
  const bool list = getenv("CC_LIST") != NULL;
  int32_t ran = 0;
  for (int32_t i = 0; i < CC_NGROUPS; i++) {
    const CareerGroup *e = &CC_GROUPS[i];
    uint32_t s = e->seed0, h = 2166136261u;
    for (int32_t k = 0; k < e->ncases + e->nresets; k++) {
      s = s * 1664525u + 1013904223u;
      const bool reset = k >= e->ncases;
      const uint32_t dg = reset ? run_reset(s, e->stage, e->bonus) : run_case(s, e->stage, e->bonus);
      if (list) printf("%d %d %s 0x%08x 0x%08x\n", e->stage, e->bonus, reset ? "reset" : "preform", s, dg);
      h = (h ^ dg) * 16777619u;
      ran++;
    }
    if (h != e->digest) {
      fprintf(stderr, "FAIL: stage %d bonus %d (seed0 0x%08x): digest 0x%08x, Control.js 0x%08x\n", e->stage,
              e->bonus, e->seed0, h, e->digest);
      failures++;
    }
  }
  // careermode off: control_preform must not take the career path.
  {
    gen_case(0x1234567u, 3, 0);
    W.xt.career.careermode = false;
    Control before = W.c;
    W.c.stcnt = 0;
    W.c.statusque = 99;
    before = W.c;
    control_preform(&W.c, &W.mad[W.im], &W.conto, &W.cp, &W.tr);
    if (W.c.campchk != before.campchk || W.c.abboost != before.abboost || memcmp(W.c.xavoidnlev, before.xavoidnlev, sizeof before.xavoidnlev) != 0) {
      fprintf(stderr, "FAIL: careermode off touched the career-only fields\n");
      failures++;
    }
  }
  medium_free(&W.m);
  if (failures == 0) {
    printf("all tests passed (%d career cases in %d groups)\n", ran, CC_NGROUPS);
    return 0;
  }
  fprintf(stderr, "%d failure(s) of %d\n", failures, ran);
  return 1;
}
