// See career_stage.h. Line references are web/ext/xtGraphics.js (XT),
// web/ext/Madness.js (MD) and web/ext/GameSparker.js (GS) unless a file is
// named; the Java is decompilation/extended/java-src/.
//
// Numeric conventions (web/TRANSPILE_SPEC.md): every fr(a op b) of two
// float32 operands is one native float op here (the double result rounded
// once is the correctly rounded float, for + - * /); the Java float
// literals 0.20000000298023224 and the like are written as 0.2f. trunc() of
// a float is jtrunc, of a double jtrunc_d. Int arithmetic wraps (-fwrapv).
#include "career_stage.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "java_compat.h"
#include "plane.h"

float (*career_stage_mrandom_hook)(Medium *m) = NULL;

// xtGraphics.outdam (XT 692), by Extended car number.
static const float kOutdam[CAREER_CARS] = {
    0.5f,  0.3f,  0.7f,  0.42f, 0.56f, 0.35f, 0.66f, 0.85f, 0.72f, 0.62f, 0.79f, 1.1f,  0.68f,
    1.5f,  1.0f,  0.85f, 1.1f,  1.25f, 1.4f,  2.35f, 1.9f,  0.85f, 2.15f, 0.6f,  0.3f,  0.7f,
    0.42f, 0.5f,  0.46f, 0.75f, 0.65f, 0.72f, 0.62f, 0.79f, 0.95f, 0.77f, 1.5f,  0.85f, 1.0f};

static float mrand(const CareerStageWorld *w) {
  return career_stage_mrandom_hook ? career_stage_mrandom_hook(w->m) : medium_random(w->m);
}

static int32_t py(int32_t i, int32_t j, int32_t k, int32_t l) { return (i - j) * (i - j) + (k - l) * (k - l); }

/** The original's conto[idx]: a car below nplayers, a stage piece above. */
static ContO *obj(const CareerStageWorld *w, int32_t idx) {
  if (idx >= 0 && idx < w->nplayers) return &w->cars[idx];
  const int32_t k = idx - w->nplayers;
  if (w->pieces && k >= 0 && k < w->npieces) return &w->pieces[k];
  return NULL;
}

static int32_t clamp255(int32_t v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }

/** ContO.invisiblepiece (255 opaque, 0 not drawn) is this engine's 255 -
 * fade. A car keeps the see-through it started with (a shadow car's). */
static void set_inv(CareerStage *cs, const CareerStageWorld *w, int32_t idx, int32_t v) {
  if (idx >= 0 && idx < w->nplayers) {
    cs->car_inv[idx] = v;
    int32_t f = clamp255(255 - v);
    if (f < cs->car_basefade[idx]) f = cs->car_basefade[idx];
    w->cars[idx].fade = f;
    return;
  }
  ContO *o = obj(w, idx);
  if (o) o->fade = clamp255(255 - v);
}

static int32_t get_inv(const CareerStage *cs, const CareerStageWorld *w, int32_t idx) {
  if (idx >= 0 && idx < w->nplayers) return cs->car_inv[idx];
  ContO *o = obj(w, idx);
  return o ? 255 - o->fade : 0;
}

/** ContO.shadow -- only where the model has its shadow buffers (stg). */
static void set_shadow(ContO *o, bool v) {
  if (o && (!v || o->stg)) o->shadow = v;
}

static void set_glow(ContO *o, bool on, int32_t r, int32_t g, int32_t b) {
  if (!o) return;
  o->glowlines = on;
  if (on) {
    o->glowc[0] = r;
    o->glowc[1] = g;
    o->glowc[2] = b;
  }
}

static void unsetfire(ContO *o) {
  if (!o) return;
  for (int32_t i = 0; i < o->npl; i++)
    if (o->p[i].wz == 0 || o->p[i].gr == -17 || o->p[i].gr == -16) o->p[i].embos = 0;
}

static void setfire(ContO *o) {
  if (o) cont_o_setfire(o);
}

static int32_t maxmag_of(const Mad *mad) { return mad->cd->maxmag[mad->cn]; }

static void flash_msg(CareerStage *cs, const char *text) {
  snprintf(cs->msg, sizeof(cs->msg), "%s", text);
  cs->msg_r = cs->flash ? 190 : 95;
  cs->flash = !cs->flash;
}

static int cmp_i32(const void *a, const void *b) {
  const int32_t x = *(const int32_t *)a, y = *(const int32_t *)b;
  return x < y ? -1 : (x > y);
}

// ---------------------------------------------------------------------------
// helpers careermode$m calls

/** xtGraphics.teleport (XT 6212-6285). `user` / `target` are conto indices. */
static void teleport(CareerStage *cs, const CareerStageWorld *w, int32_t user, int32_t target, int32_t bemean,
                     bool reversing, int32_t warning) {
  if (warning == 0) {
    if (user >= 0 && user < CAREER_STAGE_XT) cs->speedhack[user] = 100;
    int32_t distance = 9000;
    if (bemean == 1) distance = 6500;
    if (bemean == 2) distance = 5000;
    if (bemean == 3) distance = 3200;
    if (bemean > 3) distance = bemean;
    ContO *u = obj(w, user), *t = obj(w, target);
    const bool sfu = user >= 0 && user < CAREER_STAGE_XT && cs->specialflag[user];
    const bool sft = target >= 0 && target < CAREER_STAGE_XT && cs->specialflag[target];
    if (u && t) {
      int32_t fix = 0;
      if (sft) fix = t->xz <= 180 ? 1 : -1;
      int32_t inc;
      if (sfu && sft) inc = 1;
      else if (sfu || sft) inc = 0;
      else inc = 1;
      const int32_t reversemod = reversing ? 180 : 0;
      int32_t rotation = t->xz + 180 * fix + reversemod;
      if (rotation > 360) rotation -= 360;
      u->y = 250 - u->grat;
      const float fd = (float)distance;
      if (rotation >= 315 || rotation < 45) {
        u->x = t->x - jtrunc(medium_sin(w->m, (float)rotation) * fd);
        u->z = t->z + distance;
      }
      if (rotation >= 45 && rotation < 135) {
        u->x = t->x - distance;
        u->z = t->z + jtrunc(medium_cos(w->m, (float)rotation) * fd);
      }
      if (rotation >= 135 && rotation < 225) {
        u->x = t->x - jtrunc(medium_sin(w->m, (float)rotation) * fd);
        u->z = t->z - distance;
      }
      if (rotation >= 225 && rotation < 315) {
        u->x = t->x + distance;
        u->z = t->z + jtrunc(medium_cos(w->m, (float)rotation) * fd);
      }
      const int32_t easier = target != 0 ? 1 : 0;
      const int32_t reversefix = reversing ? 180 : 0;
      u->xz = t->xz + 180 * inc + 180 * easier + reversefix;
    }
    cs->teledelay = 0;
  } else if (!cs->playonce) {
    if (!w->mutes) cs->sound_redflash = true;
    cs->playonce = true;
  }
}

/** xtGraphics.attack (XT 6181-6186): undead car b drops onto car a. */
static void attack(CareerStage *cs, const CareerStageWorld *w, int32_t a, int32_t b) {
  ContO *ca = obj(w, a), *cb = obj(w, b);
  if (ca && cb) {
    cb->x = ca->x;
    cb->z = ca->z;
    cb->y = ca->y - 1500;
  }
  cs->undeadswitch++;
}

/** randtele / randtelebig (XT 6188-6210): stage 19's glitches. */
static void randtele(CareerStage *cs, const CareerStageWorld *w, int32_t a, bool waster, bool big) {
  ContO *c = &w->cars[a];
  if (!waster) {
    c->x = (c->x - 8500) + jtrunc(mrand(w) * 17000.0f);
    c->z = (c->z - 8500) + jtrunc(mrand(w) * 17000.0f);
    if (big) c->y = -7000;
  } else if (big) {
    c->y = -350;
  }
  c->xz = jtrunc(mrand(w) * 360.0f);
  if (!big) c->y = -350;
  cs->glitchtimer[a]++;
}

/** Madness.teleport (MD 3691-3697): faces the car along the floor. */

/** Madness.ghostcolide (MD 714-721): the ghost's hit. */
static void ghostcolide(CareerStage *cs, Mad *mad, ContO *co, float strength) {
  for (int32_t a = 0; a < 4; a++) {
    mad_regx(mad, a, strength, co);
    mad_regz(mad, a, strength, co);
  }
  co->xz = -360 + jtrunc_d(career_random() * 720.0);
  cs->ghosthit = false;
}

/** xtGraphics.realwalls (XT 6287-6321): the cars are kept inside the walls. */
static void realwalls(CareerStage *cs, const CareerStageWorld *w) {
  const int32_t i = cs->stage;
  bool exceptions = false;
  if (i == 9 || i == 10 || (i == 18 && cs->bonus != 4)) exceptions = true;
  const int32_t *wc = w->walls;
  for (int32_t a = 0; a < w->nplayers; a++) {
    ContO *c = &w->cars[a];
    int32_t leeway = 1000;
    if (py(c->x / 100, wc[0] / 100, c->z / 100, wc[2] / 100) < 2000) leeway = 100;
    if (py(c->x / 100, wc[0] / 100, c->z / 100, wc[3] / 100) < 2000) leeway = 100;
    if (py(c->x / 100, wc[1] / 100, c->z / 100, wc[2] / 100) < 2000) leeway = 100;
    if (py(c->x / 100, wc[1] / 100, c->z / 100, wc[3] / 100) < 2000) leeway = 100;
    if (!w->mads[a].dest && (!cs->undead[a] || cs->bonus == 4) && !exceptions && !cs->entered[a] &&
        (cs->bonus != 4 || a > 0 || w->mads[0].clear >= 46) && !cs->ghosttele) {
      if (c->x > wc[0] + leeway) c->x = wc[0] + leeway;
      if (c->x < wc[1] - leeway) c->x = wc[1] - leeway;
      if (c->z > wc[2] + leeway) c->z = wc[2] + leeway;
      if (c->z < wc[3] - leeway) c->z = wc[3] - leeway;
    }
  }
}

/** The 0..360 wrap stages 6, 7, 22 and 23 keep conto.xz in (Madness.xz
 * carries the turns). */
static void wrap_xz(CareerStage *cs, ContO *c, int32_t a, bool adjust) {
  if (c->xz < 0) {
    c->xz += 360;
    if (adjust) cs->xzadjust[a]--;
  }
  if (c->xz > 360) {
    c->xz -= 360;
    if (adjust) cs->xzadjust[a]++;
  }
}

// ---------------------------------------------------------------------------
// start / free / frame

void career_stage_free(CareerStage *cs) {
  if (!cs->started) return;
  free(cs->piece_ext);
  free(cs->piece_norender);
  cs->piece_ext = NULL;
  cs->piece_norender = NULL;
  cs->started = false;
}

/** ContO's xextreme/zextreme (ContO.js 546-573, on stages 15): the min and
 * max of every face's points -- each face's points sorted as they are
 * copied in, zeros included, as the original does. */
static void piece_extremes(const ContO *o, int32_t out[4]) {
  out[0] = out[2] = INT32_MAX;
  out[1] = out[3] = INT32_MIN;
  if (o->npl <= 0 || !o->p) return;
  int32_t *xmin = malloc(sizeof(int32_t) * (size_t)o->npl * 4);
  if (!xmin) return;
  int32_t *xmax = xmin + o->npl, *zmin = xmax + o->npl, *zmax = zmin + o->npl;
  for (int32_t j = 0; j < o->npl; j++) {
    const Plane *p = &o->p[j];
    const int32_t n = p->n;
    if (n <= 0) {
      xmin[j] = xmax[j] = zmin[j] = zmax[j] = 0;
      continue;
    }
    int32_t *xp = calloc((size_t)n * 2, sizeof(int32_t));
    if (!xp) {
      xmin[j] = xmax[j] = zmin[j] = zmax[j] = 0;
      continue;
    }
    int32_t *zp = xp + n;
    for (int32_t a = 0; a < n; a++) {
      xp[a] = p->ox[a];
      zp[a] = p->oz[a];
      qsort(xp, (size_t)n, sizeof(int32_t), cmp_i32);
      qsort(zp, (size_t)n, sizeof(int32_t), cmp_i32);
    }
    xmin[j] = xp[0];
    xmax[j] = xp[n - 1];
    zmin[j] = zp[0];
    zmax[j] = zp[n - 1];
    free(xp);
  }
  qsort(xmin, (size_t)o->npl, sizeof(int32_t), cmp_i32);
  qsort(xmax, (size_t)o->npl, sizeof(int32_t), cmp_i32);
  qsort(zmin, (size_t)o->npl, sizeof(int32_t), cmp_i32);
  qsort(zmax, (size_t)o->npl, sizeof(int32_t), cmp_i32);
  out[0] = xmin[0] + o->x;
  out[1] = xmax[o->npl - 1] + o->x;
  out[2] = zmin[0] + o->z;
  out[3] = zmax[o->npl - 1] + o->z;
  free(xmin);
}

void career_stage_start(CareerStage *cs, const CareerRace *r, const CareerSave *s, const CareerStageWorld *w) {
  career_stage_free(cs);
  memset(cs, 0, sizeof(*cs));
  cs->started = true;
  cs->stage = r->stage;
  cs->bonus = r->bonus;
  cs->hard = s->unlocked == r->stage || r->hardstage;
  cs->nplayers = w->nplayers;
  cs->npieces = w->npieces;
  cs->averagelevel = r->averagelevel;
  // resetstat (XT 13799-14030) and Madness.reseto's career fields.
  cs->wallimmunity = 100;
  cs->telecooldown = 11;
  cs->targetcar = 100;
  float sumgrip = 0.0f;
  for (int32_t a = 0; a < NFM_MAX_CARS; a++) {
    cs->floor[a] = 3;
    cs->groundlevel[a] = 250.0f;
    cs->telefade[a] = 255;
    cs->car_inv[a] = 255;
    cs->speedmulti[a] = 1.0f;
    cs->powermulti[a] = 1.0f;
    cs->nuclearmod[a] = 1.0f;
  }
  for (int32_t a = 0; a < w->nplayers && a < NFM_MAX_CARS; a++) {
    int32_t gx, gy, gz, gfloor;
    career_grid(r, s, a, 0, &gx, &gy, &gz, &gfloor);
    cs->floor[a] = gfloor;
    cs->car_basefade[a] = w->cars ? w->cars[a].fade : 0;
    cs->car[a] = r->sc[a];
    cs->beast[a] = r->beast[a];
    cs->shadow[a] = r->shadow[a];
    cs->sp_ts[a] = r->sp[a][CS_TS];
    if (w->mads) {
      cs->last_clear[a] = w->mads[a].clear;
      if (w->orig) sumgrip = sumgrip + w->orig->grip[w->mads[a].cn];
    }
  }
  // Madness 2257-2262: the field's mean base grip (gripreset of every car).
  cs->avgstartgrip = w->nplayers > 0 ? sumgrip / (float)w->nplayers : 0.0f;
  if (w->npieces > 0) {
    cs->piece_norender = calloc((size_t)w->npieces, sizeof(bool));
    // Stage 15's footprints (ContO.js 546, effect[11]).
    if (cs->stage == 15 && cs->bonus != 3 && w->pieces) {
      cs->piece_ext = malloc(sizeof(int32_t) * 4 * (size_t)w->npieces);
      if (cs->piece_ext)
        for (int32_t k = 0; k < w->npieces; k++) piece_extremes(&w->pieces[k], &cs->piece_ext[k * 4]);
    }
  }
}

void career_stage_frame(CareerStage *cs) {
  // Medium.d's effect[2] (Medium.js 1615-1665): every so often night falls
  // over stage 22 for 500 ticks.
  if (cs->stage != 22) return;
  if (cs->dn_makefase > 1000) cs->dn_on = true;
  else cs->dn_makefase++;
  if (!cs->dn_on) return;
  cs->dn_effecttime++;
  if (cs->dn_switchfase < 10) {
    cs->dn_dim += 2;
    cs->dn_changingsnap = true;
    cs->dn_switchfase++;
  } else {
    cs->dn_changingsnap = false;
    cs->dn_verydark = true;
  }
  if (cs->dn_effecttime > 500) {
    // The three colour channels step back together.
    if (cs->dn_dim > 0) {
      cs->dn_dim -= 2;
      cs->dn_changingsnap = true;
    } else {
      cs->dn_verydark = false;
      cs->dn_changingsnap = false;
      cs->dn_makefase = 0;
      cs->dn_switchfase = 0;
      cs->dn_on = false;
      cs->dn_effecttime = 0;
    }
  }
}

void career_stage_before_drive(CareerStage *cs, const CareerStageWorld *w) {
  // Madness.drive 1320-1330: xtGraphics.specialflag.
  for (int32_t a = 0; a < w->nplayers; a++) {
    int32_t l = abs(w->mads[a].pxy);
    while (l > 270) l -= 360;
    l = abs(l);
    cs->specialflag[a] = l > 90;
    cs->portal_arrived[a] = false;
    cs->portal_unbreak[a] = false;
  }
}

// ---------------------------------------------------------------------------
// careermode$m, stage by stage

static void stage15(CareerStage *cs, const CareerStageWorld *w) {
  // XT 7592-7601: off every track piece, stage 15's gravity is low.
  const int32_t np = w->nplayers;
  for (int32_t b = 0; b < np; b++) {
    cs->outoftrack[b] = true;
    for (int32_t a2 = np; a2 < 200; a2++) {
      const int32_t k = a2 - np;
      if (!cs->piece_ext || k >= cs->npieces) break;
      const int32_t *e = &cs->piece_ext[k * 4];
      const ContO *c = &w->cars[b];
      if (c->x >= e[0] && c->x <= e[1] && c->z >= e[2] && c->z <= e[3]) cs->outoftrack[b] = false;
    }
  }
}

static void set_norender(CareerStage *cs, const CareerStageWorld *w, int32_t idx, bool v) {
  if (idx >= 0 && idx < w->nplayers) {
    cs->norender[idx] = v;
    return;
  }
  const int32_t k = idx - w->nplayers;
  if (cs->piece_norender && k >= 0 && k < cs->npieces) cs->piece_norender[k] = v;
}

static void stage13(CareerStage *cs, const CareerStageWorld *w) {
  // XT 7602-7763: the tower's four floors.
  const int32_t np = w->nplayers;
  const int32_t floorcount[4] = {216 + np, 412 + np, 629 + np, 861 + np};
  ContO *me = &w->cars[0];
  for (int32_t a3 = 0; a3 < floorcount[3]; a3++) {
    set_norender(cs, w, a3, false);
    ContO *o = obj(w, a3);
    if (!o) continue;
    if (a3 >= np && o->telechk > -1) {
      set_inv(cs, w, a3, 150);
      if (o->telechk == 0) set_glow(o, true, 255, 0, 0);
      if (o->telechk == 1) set_glow(o, true, 255, 200, 0);
      if (o->telechk == 2) set_glow(o, true, 0, 255, 0);
      if (o->telechk == 3) set_glow(o, true, 255, 255, 255);
    }
    // (ContO.fakegrounded, the shadows' floor, is drawing only.)
    if (o->wallpiece) {
      int32_t wallheight = -(cs->floor[0] * 10000) - o->grat;
      if (cs->floor[0] == 0) wallheight = 250 - o->grat;
      if (o->y != wallheight) set_norender(cs, w, a3, true);
    }
    if (me->y <= -30000 && a3 >= floorcount[2]) set_norender(cs, w, a3, true);
    if (a3 < np) {
      Mad *mad = &w->mads[a3];
      if (cs->floorguardian[a3]) {
        // ContO.dmgcolours: the guardian's flames go from yellow to red with its damage.
        int32_t i66 = jtrunc(60.0f * ((float)mad->hitmag / (float)maxmag_of(mad)));
        int32_t i67 = 244, i68 = 244, i69 = 11;
        if (i66 > 20) i68 = jtrunc(244.0f - 233.0f * ((float)(i66 - 20) / 40.0f));
        i67 = clamp255(jtrunc((float)i67 + (float)i67 * ((float)w->m->snap[0] / 100.0f)));
        i68 = clamp255(jtrunc((float)i68 + (float)i68 * ((float)w->m->snap[1] / 100.0f)));
        i69 = clamp255(jtrunc((float)i69 + (float)i69 * ((float)w->m->snap[2] / 100.0f)));
        cs->dmgcolour[a3][0] = i67;
        cs->dmgcolour[a3][1] = i68;
        cs->dmgcolour[a3][2] = i69;
        mad->power = 98.0f;
        mad->clear = -2;
        w->cp->clear[a3] = -2;
        if (mad->hitmag > maxmag_of(mad)) {
          cs->dmgcolour[a3][0] = 255;
          cs->dmgcolour[a3][1] = 169;
          cs->dmgcolour[a3][2] = 89;
        } else {
          setfire(o);
        }
      }
      cs->groundlevel[a3] = (float)cs->floor[a3] * -10000.0f;
      if (cs->groundlevel[a3] == 0.0f) cs->groundlevel[a3] = 250.0f;
      if (cs->speedhack[a3] > 0) {
        int32_t topspeed = mad->cd->swits[mad->cn][2];
        if (topspeed > 400 && cs->floor[a3] == 2) topspeed = 400;
        mad->speed = (float)topspeed;
        mad->power = 98.0f;
        cs->speedhack[a3]--;
      }
      if (a3 > 0) {
        cs->norender[a3] = false;
        if (cs->floor[0] != cs->floor[a3]) cs->norender[a3] = true;
      }
    }
  }
}

static void stage11(CareerStage *cs, const CareerRace *r, const CareerStageWorld *w) {
  // XT 7764-7861: the junkyard's undead vans, each in its yard.
  const int32_t np = w->nplayers;
  Mad *md = w->mads;
  ContO *co = w->cars;
  CheckPoints *cp = w->cp;
  for (int32_t a = 0; a < np; a++)
    if (w->starcnt == 0) cs->statdrain[a]++;
  for (int32_t a = np + 155; a < np + 221; a++) set_inv(cs, w, a, 130);
  for (int32_t a = 1; a < 5 && a < np; a++) {
    if (!cs->undead[a]) {
      mad_distruct(&md[a], &co[a]);
      cs->undead[a] = true;
    }
  }
  for (int32_t a = 1; a < np; a++) {
    if (!cs->undead[a]) continue;
    md[a].spatk = 0.0f;
    md[a].hitmag = 0;
    cs->noarrow[a] = true;
    md[a].spatk = 0.0f;
    md[a].power = 98.0f;
    if (cs->sp_ts[0] <= r->sp[0][CS_STR] && cp->clear[0] <= 4 && a >= 5) {
      int32_t speedlimit = (w->orig->swits[md[0].cn][2] + cs->sp_ts[0]) - w->orig->swits[md[a].cn][2];
      if (speedlimit < 0) speedlimit = 0;
      cs->sp_ts[a] = speedlimit;
      md[a].power = 60.0f;
    }
    md[a].clear = -2;
    cs->newflame[a] = true;
    cp->clear[a] = -2;
  }
  static const int32_t rightb[4] = {-6100, 26800, -6400, 20418};
  static const int32_t leftb[4] = {-20500, 7600, -25600, 6018};
  static const int32_t topb[4] = {25300, 24400, 54700, 51600};
  static const int32_t bottomb[4] = {1300, -4400, 35500, 32400};
  int32_t carsregion[4] = {0, 0, 0, 0};
  for (int32_t a4 = 0; a4 < np; a4++) {
    if (a4 < 1 || a4 > 4) {
      cs->safezone[a4] = false;
      if (co[a4].x < rightb[0] && co[a4].x > leftb[0] && co[a4].z < topb[0] && co[a4].z > bottomb[0] && !cs->undead[a4])
        cs->safezone[a4] = true;
    }
  }
  for (int32_t a4 = 1; a4 < np - 1; a4++) {
    if (a4 >= 1 && a4 <= 4) {
      if (co[a4].x > rightb[a4 - 1] - 700) co[a4].x = rightb[a4 - 1] - 700;
      if (co[a4].x < leftb[a4 - 1] + 700) co[a4].x = leftb[a4 - 1] + 700;
      if (co[a4].z > topb[a4 - 1] - 700) co[a4].z = topb[a4 - 1] - 700;
      if (co[a4].z < bottomb[a4 - 1] + 700) co[a4].z = bottomb[a4 - 1] + 700;
    } else {
      for (int32_t b2 = 0; b2 < 4; b2++) {
        const bool in = co[a4].x < rightb[b2] && co[a4].x > leftb[b2] && co[a4].z < topb[b2] && co[a4].z > bottomb[b2];
        if (in && !cs->undead[a4]) carsregion[b2]++;
        const bool mein = co[0].x < rightb[b2] && co[0].x > leftb[b2] && co[0].z < topb[b2] && co[0].z > bottomb[b2];
        if (mein || carsregion[b2] == 0) cs->undeadlock[b2 + 1] = 0;
        else if (in) cs->undeadlock[b2 + 1] = a4;
      }
    }
  }
  int32_t carszone = 0;
  for (int32_t a5 = 0; a5 < np - 1; a5++) {
    if (co[a5].x < -6100 && co[a5].x > -20500 && co[a5].z < 25300 && co[a5].z > 1300 && a5 != 1 && !cs->undead[a5])
      carszone++;
  }
  if (carszone == 0 && np > 1) {
    if (co[1].z < 13200) co[1].z += 500;
    if (co[1].z > 14300) co[1].z -= 500;
  }
}

static void stage6(CareerStage *cs, const CareerRace *r, const CareerStageWorld *w) {
  // XT 7862-8129: Ghost Planet. Piece 0 is the ghost that shadows the
  // player, piece 1 the one that carries the field away, pieces 2-4 flames.
  const int32_t np = w->nplayers;
  Mad *md = w->mads;
  ContO *co = w->cars;
  CheckPoints *cp = w->cp;
  ContO *ghost = obj(w, np), *ghost2 = obj(w, np + 1);
  if (cs->wallcountdown) {
    if (cs->wallimmunity > 0) cs->wallimmunity--;
    else cs->wallcountdown = false;
  } else {
    cs->wallimmunity = 100;
  }
  for (int32_t a = np + 5; a < np + 101; a++) {
    if ((cs->ghosttimer >= 230 && cs->ghosttimer < 250) || a < np + 16) set_inv(cs, w, a, jtrunc_d(career_random() * 50.0));
    else set_inv(cs, w, a, 255);
  }
  if (!cs->scareflash) {
    if (jtrunc_d(career_random() * 25000.0) == 0 && cs->ghostattempt >= 3) {
      for (int32_t a = np + 5; a < np + 157; a++) set_glow(obj(w, a), true, 200, 0, 0);
      cs->scareflash = true;
      if (!w->controls[0].mutem) {
        cs->turnbackon = true;
        w->controls[0].mutem = true;
      }
    } else {
      for (int32_t a = np + 5; a < np + 157; a++) set_glow(obj(w, a), false, 0, 0, 0);
    }
  } else {
    for (int32_t a = np + 5; a < np + 157; a++) set_glow(obj(w, a), false, 0, 0, 0);
    cs->scareflashtime++;
    set_inv(cs, w, np + 1, jtrunc_d(career_random() * 50.0));
    if (cs->scareflashtime > 15) {
      if (!cs->playonce && !w->mutes) {
        cs->sound_scare = true;
        cs->playonce = true;
      }
      if (cs->scareflashtime > 225) {
        if (cs->turnbackon) {
          w->controls[0].mutem = false;
          cs->turnbackon = false;
        }
        cs->scareflash = false;
        cs->playonce = false;
        cs->scareflashtime = 0;
      }
    }
  }
  for (int32_t a = 0; a < np; a++) {
    if (a != 0 || !md[a].dest) {
      set_inv(cs, w, a, 255);
      set_shadow(&co[a], true);
    }
    cs->entered[a] = false;
  }
  int32_t timeperiod = 1000;
  const int32_t delay = 250;
  if (cp->clear[0] >= 9 && cs->randomtimes == 0) timeperiod = 500;
  if (!cs->ghostflash[0] && !cs->ghostflash[1] && !cs->ghostflash[2] && !cs->ghosthit) {
    cs->ghostfade = 0;
    if (cs->ghosttimer < timeperiod + delay) cs->ghosttimer++;
    else cs->ghosttimer = 0;
    if (cs->randomtimes == 0) {
      cs->randomtimes = jtrunc_d(career_random() * timeperiod) + 1;
      cs->ghostattack = jtrunc_d(career_random() * 3.0);
    }
  } else {
    cs->randomtimes = 0;
  }
  set_shadow(ghost, false);
  int32_t teledistance = 3000;
  if (cs->ghosttimer == cs->randomtimes + delay && cs->randomtimes != 0) {
    if (!cs->ghostflash[cs->ghostfar]) {
      cs->ghostattempt++;
      cs->ghostflash[cs->ghostfar] = true;
    }
  } else {
    cs->ghostfar = jtrunc_d(career_random() * 3.0);
  }
  for (int32_t a6 = 0; a6 < 3; a6++) {
    if (!cs->ghostflash[a6]) continue;
    cs->ghosttimer = 0;
    if (a6 != cs->ghostattack || cs->ghostattempt < 3) {
      teledistance = 3000 - a6 * 1000;
    } else if (cs->ghostflashtimer < 65) {
      teledistance = 3000 - a6 * 1000;
    } else {
      teledistance = (900 - (cs->ghostflashtimer - 65) * 100) * (3 - a6);
      if (teledistance < 10) teledistance = 10;
      if (cs->ghostflashtimer == 79) cs->ghosthit = true;
    }
    cs->ghostfade = jtrunc_d(career_random() * 80.0);
    if (md[0].mtouch) {
      if (cs->ghostflashtimer < 80) {
        cs->ghostflashtimer++;
      } else {
        cs->ghostflashtimer = 0;
        cs->ghostflash[a6] = false;
      }
      set_shadow(ghost, true);
    } else {
      cs->ghostfade = 0;
    }
  }
  set_inv(cs, w, np, cs->ghostfade);
  wrap_xz(cs, &co[0], 0, true);
  teleport(cs, w, np, 0, teledistance, md[0].speed < 0.0f, 0);
  if (cs->ghosthit) {
    if (cs->whatghostdo == 1) {
      bool racer = false;
      if (cp->clear[0] >= 9) racer = true;
      float damagetake = 200.0f;
      int32_t thelevel = r->averagelevel + 2;
      if (thelevel > 20) thelevel = 20;
      if (!racer) damagetake = 300.0f + (float)thelevel * 25.0f;
      if (racer) cs->wallcountdown = true;
      ghostcolide(cs, &md[0], &co[0], damagetake);
    }
    if (cs->whatghostdo == 0 && !cs->ghosttele) {
      for (int32_t a7 = 0; a7 < np; a7++) {
        cs->oldx[a7] = co[a7].x;
        cs->oldz[a7] = co[a7].z;
      }
      cs->ghosttele = true;
    }
  } else {
    if (!cs->shownghost) cs->whatghostdo = jtrunc_d(career_random() * 2.0);
    else cs->whatghostdo = 1;
    if (cs->ghostteletimer >= 200) {
      if (cs->ghostteletimer < 250) {
        cs->ghostteletimer++;
        for (int32_t a7 = 0; a7 < np; a7++) cs->entered[a7] = true;
        if (ghost2) ghost2->z = 300000;
      } else {
        cs->ghostteletimer = 0;
      }
    } else {
      if (ghost2) ghost2->z = 300000;
      cs->ghostteletimer = 0;
    }
    for (int32_t a7 = 0; a7 < 3; a7++) unsetfire(obj(w, np + 2 + a7));
    cs->ghosttele = false;
  }
  if (cs->ghosttele) {
    cs->shownghost = true;
    set_inv(cs, w, np + 1, jtrunc_d(career_random() * 50.0));
    if (cs->ghostteletimer >= 100)
      for (int32_t a7 = 0; a7 < 3; a7++) setfire(obj(w, np + 2 + a7));
    for (int32_t a7 = 0; a7 < np; a7++) {
      if (!md[a7].dest) {
        cs->entered[a7] = true;
        teleport(cs, w, a7, np + 1, 3000, false, 0);
        co[a7].z = 297000;
        co[a7].y = 250 - co[0].grat;
        if (a7 <= 5) co[a7].x = 0 - a7 * 500;
        else co[a7].x = 0 + (a7 - 5) * 500;
        md[a7].speed = 0.0f;
        md[a7].mtouch = true;
      }
      int32_t howmuchleft = 255 - cs->ghostteletimer * 5;
      if (howmuchleft < 0) howmuchleft = 0;
      if (a7 > 0) {
        set_inv(cs, w, a7, howmuchleft);
        if (howmuchleft == 0) set_shadow(&co[a7], false);
      }
    }
    if (cs->ghostteletimer >= 180 && ghost2) ghost2->z -= 250;
    if (ghost2 && ghost2->z < co[0].z) ghost2->z = co[0].z;
    if (cs->ghostteletimer < 200) {
      cs->ghostteletimer++;
    } else {
      for (int32_t a7 = 0; a7 < np; a7++) {
        co[a7].x = cs->oldx[a7];
        co[a7].z = cs->oldz[a7];
      }
      cs->ghosthit = false;
    }
    cs->ghostfade = 0;
  }
  if (cp->haltall || md[0].dest) cs->ghosttimer = 0;
  if (md[0].dest) {
    for (int32_t a7 = 0; a7 < 3; a7++) setfire(obj(w, np + 2 + a7));
    if (cs->shownghost) {
      if (w->holdcnt > 30) {
        unsetfire(&co[0]);
        if (get_inv(cs, w, 0) > 5) {
          set_inv(cs, w, 0, get_inv(cs, w, 0) - 10);
        } else {
          set_inv(cs, w, 0, 0);
          set_shadow(&co[0], false);
        }
      }
      if (w->holdcnt > 85) {
        set_inv(cs, w, np + 1, jtrunc_d(career_random() * 50.0));
        set_inv(cs, w, np, 0);
      }
    }
  }
}

static void stage7(CareerStage *cs, const CareerStageWorld *w) {
  // XT 8130-8192: the Matrix's shadow follows the player and shows up
  // every 1000 ticks; the ground's green outlines pulse (polyoutline).
  const int32_t np = w->nplayers;
  Mad *md = w->mads;
  ContO *ghost = obj(w, np);
  wrap_xz(cs, &w->cars[0], 0, true);
  teleport(cs, w, np, 0, 750, md[0].speed < 0.0f, 0);
  set_inv(cs, w, np, 0);
  set_shadow(ghost, false);
  if (cs->ghosttimer < 1000) {
    if (!cs->ghostflash[0]) cs->ghosttimer++;
  } else {
    cs->ghostflash[0] = true;
    cs->ghosttimer = 0;
  }
  if (cs->ghostflash[0] && md[0].mtouch && !md[0].dest) {
    set_inv(cs, w, np, 255);
    if (ghost) {
      ghost->spec_on = w->cars[0].spec_on;
      for (int32_t b3 = 0; b3 < 3; b3++) ghost->spec[b3] = w->cars[0].spec[b3];
    }
    set_shadow(ghost, true);
    if (cs->ghostflashtimer < 150) {
      cs->ghostflashtimer++;
    } else {
      cs->ghostflashtimer = 0;
      cs->ghostflash[0] = false;
    }
  }
  if (cs->polyoutline >= 120) cs->ghostflash[1] = true;
  if (cs->polyoutline <= 0) {
    cs->polyoutline = 0;
    cs->ghostflash[1] = false;
  }
  if (!cs->ghostflash[1]) cs->polyoutline += 12;
  else cs->polyoutline -= 12;
  for (int32_t a2 = 0; a2 < np; a2++)
    if (md[a2].dest) set_inv(cs, w, a2, 254);
}

static void stage12(CareerStage *cs, const CareerStageWorld *w) {
  // XT 8193-8231: the factory's pistons and sliding blocks.
  const int32_t np = w->nplayers;
  cs->pieceglitch++;
  for (int32_t a = 0; a < 56; a++) {
    ContO *o = obj(w, np + a);
    if (!o) continue;
    if (a % 2 == 0) {
      if (cs->pieceglitch % 30 < 15) o->y -= 80;
      else o->y += 80;
    } else if (cs->pieceglitch >= 15) {
      if ((cs->pieceglitch + 15) % 30 < 15) o->y -= 80;
      else o->y += 80;
    }
  }
  for (int32_t a = 0; a < 48; a++) {
    ContO *o = obj(w, np + 56 + a);
    if (!o) continue;
    if (a % 2 == 0) {
      if (cs->pieceglitch % 20 < 10) o->x += 400;
      else o->x -= 400;
    } else if (cs->pieceglitch % 20 < 10) {
      o->x -= 400;
    } else {
      o->x += 400;
    }
  }
}

static void bonus4(CareerStage *cs, const CareerRace *r, const CareerStageWorld *w) {
  // XT 8291-8398: the gauntlet -- the player's health drains, the field is
  // undead until the last car shows up at checkpoint 46.
  const int32_t np = w->nplayers;
  Mad *md = w->mads;
  ContO *co = w->cars;
  CheckPoints *cp = w->cp;
  Medium *m = w->m;
  const int32_t c0 = cp->clear[0];
  int32_t yourtime = 340;
  if (c0 == 19) yourtime = 480;
  if (c0 >= 20 && c0 <= 23) yourtime = 450;
  if (c0 == 24 || c0 == 25) yourtime = 600;
  if (c0 >= 26 && c0 <= 40) yourtime = 200;
  if (c0 >= 41 && c0 <= 45) yourtime = 420;
  const int32_t last = np - 1;
  if (c0 >= 46) {
    yourtime = 1000;
    int32_t i66 = jtrunc(250.0f * ((float)md[last].hitmag / (float)maxmag_of(&md[last])));
    int32_t i67 = 244, i68 = 244, i69 = 11;
    if (i66 > 83) i68 = jtrunc(244.0f - 233.0f * ((float)(i66 - 83) / 40.0f));
    i67 = jtrunc((float)i67 + (float)i67 * ((float)m->snap[0] / 100.0f));
    if (i66 > 250) i66 = 250;
    i67 = clamp255(i67);
    i68 = clamp255(jtrunc((float)i68 + (float)i68 * ((float)m->snap[1] / 100.0f)));
    i69 = clamp255(jtrunc((float)i69 + (float)i69 * ((float)m->snap[2] / 100.0f)));
    cs->health_bar = true;
    cs->health_fill = i66;
    cs->health_rgb[0] = i67;
    cs->health_rgb[1] = i68;
    cs->health_rgb[2] = i69;
    cs->health_slot = last;
  }
  const int32_t me = r->sc[0];
  const float end = 4.5f - kOutdam[me >= 0 && me < CAREER_CARS ? me : 0];
  const int32_t defence = jtrunc(end * 100.0f);
  const int32_t hr = w->orig->maxmag[md[0].cn];   // Madness.healthreset
  int32_t healthinc = 500;
  if (hr / 20 >= 500) {
    if (hr <= 25000 || me == 11 || me == 13 || me == 36 || me == 18 || me == 19 || me == 20) healthinc = hr / 20;
    else healthinc = 1250;
  }
  const int32_t totalhealth = hr + defence * healthinc;
  const int32_t maxbs = hr + (defence + 50) * healthinc;
  double dmgratio = (double)maxmag_of(&md[0]) / maxbs;
  if (dmgratio < 1.0) dmgratio = 1.0;
  const int32_t drainrate = jtrunc_d((totalhealth * dmgratio) / yourtime);
  if (w->starcnt == 0 && !w->winner) md[0].hitmag += drainrate;
  if (c0 < 46 && !md[0].dest) {
    md[last].hitmag = 0;
    co[last].x = 500000;
  }
  for (int32_t a8 = 0; a8 < np; a8++) {
    cs->noarrow[a8] = true;
    if (a8 > 0) {
      md[a8].nlaps = 0;
      md[a8].clear = -2;
      cp->clear[a8] = -2;
      md[a8].power = 98.0f;
      if (a8 != last) {
        if (!cs->undead[a8]) {
          mad_distruct(&md[a8], &co[a8]);
          cs->undead[a8] = true;
        }
        md[a8].hitmag = 0;
        cs->newflame[a8] = true;
      }
    }
  }
  if (md[last].dest)
    for (int32_t a8 = 1; a8 < last; a8++) md[a8].hitmag = maxmag_of(&md[a8]) + 1;
}

static void revive_slot(CareerStage *cs, Mad *md, int32_t k) {
  if (k < 0 || k >= cs->nplayers || cs->revive[k]) return;
  md[k].hitmag = 0;
  md[k].dest = false;
  cs->newflame[k] = true;
  cs->revive[k] = true;
}

static void stage23(CareerStage *cs, const CareerRace *r, const CareerStageWorld *w) {
  // XT 8399-8645: Grassland Fires. Winning on the newest stage (or hard)
  // wakes the Titan (slot 1): stat$m holds, then cstimer counts the fight.
  const int32_t np = w->nplayers;
  Mad *md = w->mads;
  ContO *co = w->cars;
  CheckPoints *cp = w->cp;
  for (int32_t a = 0; a < np; a++) {
    if (cs->speedhack[a] > 0) {
      md[a].cd->revpush[md[a].cn] = 0.0f;
      md[a].speed = (float)md[a].cd->swits[md[a].cn][2];
      cs->speedhack[a]--;
    }
    wrap_xz(cs, &co[a], a, true);
  }
  if (np > 1 && !cs->undead[1] && cs->hard) {
    mad_distruct(&md[1], &co[1]);
    cs->undead[1] = true;
  }
  if (cs->cstimer == 2) {
    cp->haltall = false;
    cs->release_hold = true;
    cs->unlimitedlaps = true;
    if (!cs->music_stopped) {
      cs->music_event = CAREER_MUSIC_STOP;
      cs->music_stopped = true;
    }
    md[0].spatk = 0.0f;
    if ((md[0].hitmag > 0 || !md[0].mtouch) && !md[0].dest) flash_msg(cs, "Fix your car at the fix hoop!");
    else if (!md[0].dest) cs->cstimer++;
    else cs->hold_by_boss = true;
  }
  if (cs->cstimer <= 2 && cs->hard && np > 1) co[1].z = -150000;
  if (cs->cstimer == 3) {
    cs->music_event = CAREER_MUSIC_BOSS;
    for (int32_t a = 2; a < np; a++) co[a].z = 500000 + a * 1000;
    teleport(cs, w, 1, 0, 0, md[0].speed < 0.0f, 0);
    cs->stunthealth = 0;
    cs->cstimer++;
  }
  if (cs->cstimer >= 4 && cs->cstimer < 10000 && np > 1) {
    double hishealth = cs->stunthealth / 2400.0;
    if (hishealth > 1.0) hishealth = 1.0;
    cs->boss_bar = true;
    cs->boss_fill = hishealth;
    const double h2 = floor(hishealth * 100.0 * 10.0 + 0.5) / 10.0;   // round(x, 1)
    snprintf(cs->boss_pct, sizeof(cs->boss_pct), "%.1f %%", h2);
    const float str = md[1].cd->moment[md[1].cn] / 2.1f;
    const int32_t strength = jtrunc(str * 100.0f);
    const int32_t speed = md[1].cd->swits[md[1].cn][2] / 2;
    bool worse;
    if (cs->cstimer % 1200 < 400) {
      worse = r->level[1] > r->level[0] + 5;
      snprintf(cs->boss_info, sizeof(cs->boss_info), "Level %d", (int)r->level[1]);
    } else if (cs->cstimer % 1200 < 800) {
      worse = md[1].cd->moment[md[1].cn] > md[0].cd->moment[md[0].cn];
      snprintf(cs->boss_info, sizeof(cs->boss_info), "strength: %d", (int)strength);
    } else {
      worse = md[1].cd->swits[md[1].cn][2] > md[0].cd->swits[md[0].cn][2];
      snprintf(cs->boss_info, sizeof(cs->boss_info), "speed: %d mph", (int)speed);
    }
    cs->boss_info_rgb[0] = worse ? 150 : 0;
    cs->boss_info_rgb[1] = worse ? 0 : 60;
    cs->boss_info_rgb[2] = 0;
  }
  if (cs->cstimer >= 4 && cs->cstimer < 180) {
    cs->cstimer++;
    flash_msg(cs, "Damage Titan by stunting!");
  }
  if (cs->cstimer >= 180 && cs->cstimer < 10000 && np > 1) {
    const double hishealth4 = cs->stunthealth / 2500.0;
    const double hishealth5 = hishealth4 * 100.0;
    for (int32_t a7 = 2; a7 < np; a7++) {
      md[a7].power = 98.0f;
      md[a7].spatk = 0.0f;
    }
    if (cs->cstimer < 9900) cs->cstimer++;
    else cs->cstimer = 180;
    const int32_t health = jtrunc_d(hishealth5);
    if (cs->cstimer % 2000 == 0 && health > 75) md[1].spatk = 120.0f;
    else if (md[1].speclast2 == 0.0f || md[1].speclast == 0.0f) md[1].spatk = 0.0f;
    if (health > 80) revive_slot(cs, md, np - 4);
    else if (health > 60) revive_slot(cs, md, np - 5);
    else if (health > 40) revive_slot(cs, md, np - 9);
    else if (health > 20) revive_slot(cs, md, np - 3);
    double telerange = (1.0 - hishealth4) * 60000.0;
    if (telerange < 30000.0) telerange = 30000.0;
    double modifier = 1.0;
    if (md[0].speed < 0.0f) modifier = 0.4;
    if ((py(co[1].x / 100, co[0].x / 100, co[1].z / 100, co[0].z / 100) > jtrunc_d(telerange * modifier) ||
         cs->teledelay > 300) &&
        !md[0].dest) {
      if (cs->telecooldown > 0) cs->telecooldown--;
      teleport(cs, w, 1, 0, 2, md[0].speed < 0.0f, cs->telecooldown);
    } else {
      cs->playonce = false;
      cs->telecooldown = 11;
      cs->teledelay++;
    }
    if (hishealth4 < 1.0) {
      md[1].hitmag = 0;
    } else {
      for (int32_t a8 = 1; a8 < np; a8++) md[a8].hitmag = maxmag_of(&md[a8]) + 1;
      cs->cstimer = 10000;
      cs->bossbattle = false;
    }
  }
  if (cs->hard && np > 1) {
    md[1].cd->revpush[md[1].cn] = 0.0f;
    cs->noarrow[1] = true;
    md[1].power = 98.0f;
    md[1].clear = -2;
    cs->newflame[1] = true;
    cp->clear[1] = -2;
  }
  int32_t wastestage = cp->wasted / 2 + 1;
  if (wastestage > 5) wastestage = 5;
  int32_t racestage = (w->contva ? w->contva->completed[0] : 0) / 20 + 1;
  if (racestage > 5) racestage = 5;
  cs->greystage = racestage > wastestage ? racestage : wastestage;
}

static int32_t clear_of(const CheckPoints *cp, int32_t k) { return k >= 0 && k < NFM_MAX_CARS ? cp->clear[k] : 0; }
static int32_t completed_of(const CareerStageWorld *w, int32_t k) {
  return w->contva && k >= 0 && k < NFM_MAX_CARS ? w->contva->completed[k] : 0;
}

static void stage22(CareerStage *cs, const CareerStageWorld *w) {
  // XT 8646-8794: Desert Night -- when night falls the last car (a shadow)
  // teleports onto whoever leads, or a random car.
  const int32_t np = w->nplayers;
  Mad *md = w->mads;
  ContO *co = w->cars;
  CheckPoints *cp = w->cp;
  const int32_t last = np - 1;
  bool bellend[NFM_MAX_CARS] = {false};
  for (int32_t a2 = 0; a2 < np; a2++) {
    bellend[a2] = false;
    if (cp->clear[a2] >= 3) bellend[a2] = true;
    if (cs->speedhack[a2] > 0) {
      if (bellend[0] && cs->speedhack[a2] > 75) {
        w->controls[a2].left = false;
        w->controls[a2].right = false;
      }
      md[a2].cd->revpush[md[a2].cn] = 0.0f;
      md[a2].speed = (float)md[a2].cd->swits[md[a2].cn][2];
      cs->speedhack[a2]--;
    }
    wrap_xz(cs, &co[a2], a2, true);
  }
  // (m.changingsnap -> snap(22): the HUD's colours follow the dusk; drawing.)
  bool justrace = false;
  const bool biglead = w->contva && w->contva->biglead[last];
  if ((cp->clear[0] >= 3 && biglead) || completed_of(w, last) >= 70 || cp->wasted >= 7) justrace = true;
  if (w->contva && w->contva->needhelp[last]) justrace = true;
  if (md[last].frozen || md[last].redstr || md[last].strswap) justrace = true;
  if (justrace) cs->targetcar = 100;
  const int32_t tc = cs->targetcar;
  const bool tcar = tc >= 0 && tc < np;
  if (!cs->verydark && cs->dn_verydark && tcar && md[tc].mtouch && !justrace && !md[tc].dest) {
    int32_t telescale = 1;
    if (bellend[tc]) telescale = 3;
    if (md[last].specialact && !bellend[tc]) telescale = 0;
    if (cs->teledelay > 0) {
      cs->teledelay--;
      cs->telecooldown = 13;
      cs->playonce = false;
    } else {
      if (cs->telecooldown > 0) cs->telecooldown--;
      if (cs->targetcar != 0) cs->telecooldown = 0;
      teleport(cs, w, last, cs->targetcar, telescale, md[tc].speed < 0.0f, cs->telecooldown);
      cs->telewait++;
      if (telescale != 3 || cs->telewait > 18) {
        cs->telewait = 0;
        cs->verydark = true;
      }
    }
  }
  if (cs->verydark && !cs->dn_verydark) {
    cs->targetcar = 100;
    cs->verydark = false;
  }
  if (!cs->dn_verydark) cs->teledelay = jtrunc_d(career_random() * 30.0) + 1;
  if (!cs->verydark) {
    for (int32_t a3 = 0; a3 < last; a3++) {
      if (cp->clear[a3] >= 3) {
        cs->targetcar = a3;
      } else if (!md[last].specialact) {
        if (cs->targetcar == 100 || (cs->targetcar >= 0 && cs->targetcar < np && md[cs->targetcar].dest))
          cs->targetcar = jtrunc(mrand(w) * (float)(np - 1));
      } else {
        cs->targetcar = 0;
      }
    }
  } else {
    int32_t telerange2 = 50000;
    if (clear_of(cp, cs->targetcar) >= 3) {
      if (completed_of(w, cs->targetcar) >= 50) telerange2 = 10000;
      else if (completed_of(w, cs->targetcar) >= 33) telerange2 = 15000;
      else telerange2 = 20000;
    }
    int32_t noglitch = cs->targetcar;
    if (noglitch == 100) noglitch = 0;
    if (noglitch < 0 || noglitch >= np) return;
    double modifier2 = 1.0;
    if (md[0].speed < 0.0f) modifier2 = 0.5;
    bool smalldelay = false;
    if (cs->telewait > 0 && cs->telewait < 18 && bellend[noglitch]) smalldelay = true;
    if (md[noglitch].mtouch) {
      if ((py(co[last].x / 100, co[noglitch].x / 100, co[last].z / 100, co[noglitch].z / 100) >
               jtrunc_d(telerange2 * modifier2) ||
           smalldelay) &&
          !justrace && !md[noglitch].dest) {
        cs->telewait++;
        if (cs->telecooldown > 0) cs->telecooldown--;
        int32_t telescale2 = 1;
        if (cs->targetcar != 0) cs->telecooldown = 0;
        if (bellend[noglitch]) telescale2 = 3;
        if (md[last].specialact && !bellend[noglitch]) telescale2 = 0;
        teleport(cs, w, last, noglitch, telescale2, md[noglitch].speed < 0.0f, cs->telecooldown);
      } else {
        cs->telewait = 0;
        cs->playonce = false;
        cs->telecooldown = 13;
      }
    }
  }
}

static void stage21(CareerStage *cs, const CareerStageWorld *w) {
  // XT 8795-8829: Hellzone -- once every opponent is in, the entry road
  // (pieces 0-19, trackers 0-60) crumbles.
  const int32_t np = w->nplayers;
  ContO *co = w->cars;
  for (int32_t a = 0; a < np; a++) {
    if (co[a].z > w->walls[3] - 500) cs->entered[a] = false;
    else cs->entered[a] = true;
    if (a > 0 && cs->entered[a] && co[a].z > -100000 && !cs->countfall[a]) {
      cs->startfalling++;
      cs->countfall[a] = true;
    }
    if (cs->startfalling == np - 1) cs->crumble = true;
  }
  if (cs->crumble) {
    for (int32_t a = 0; a < 20; a++) {
      const int32_t idx = np + a;
      if (get_inv(cs, w, idx) > 0) {
        set_inv(cs, w, idx, get_inv(cs, w, idx) - 15);
      } else {
        set_inv(cs, w, idx, 0);
        ContO *o = obj(w, idx);
        if (o) o->x = -300000;
        for (int32_t b3 = 0; b3 < 61 && b3 < TRACKERS_MAX; b3++) w->t->x[b3] = -300000;
      }
    }
    for (int32_t a = 0; a < np; a++)
      if (co[a].z <= -100000 && cs->entered[a]) cs->crumblefail[a] = true;
  }
}

static void stages20_21(CareerStage *cs, CareerRun *run, const CareerStageWorld *w) {
  // XT 8830-8880: off the edge. Stage 20 respawns at the last checkpoint
  // (1v1 with lives), stage 21 wastes.
  const int32_t np = w->nplayers;
  Mad *md = w->mads;
  ContO *co = w->cars;
  const int32_t *wc = w->walls;
  for (int32_t a = 0; a < np; a++) {
    wrap_xz(cs, &co[a], a, false);
    if (co[a].x > wc[0] + 1000 || co[a].x < wc[1] - 1000 || co[a].z > wc[2] + 1000 || co[a].z < wc[3] - 1000) {
      int32_t deaddelay = 30;
      if (cs->timesfallen[a] < 4) deaddelay = (cs->timesfallen[a] + 1) * 30;
      else deaddelay = 120;
      if (co[a].y > -1000 || (cs->lives[a] > 0 && cs->lives[a] <= deaddelay)) {
        const bool exception = cs->stage == 20;
        md[a].hitmag = maxmag_of(&md[a]) + 1;
        if (exception) {
          cs->lives[a]++;
          if (cs->lives[a] > deaddelay) {
            cs->lives[a] = 1000;
            career_stage_respawn(cs, &md[a], &co[a], w->cp);
            cs->timesfallen[a]++;
            if (run && run->losepoints) {
              run->statgain = 0;
              run->losepoints = false;
            }
          }
        } else {
          cs->lives[a] = 0;
        }
      } else {
        cs->lives[a] = 0;
        cs->respawning[a] = false;
      }
    } else {
      cs->lives[a] = 0;
      cs->respawning[a] = false;
    }
  }
}

static void stage19(CareerStage *cs, const CareerRace *r, const CareerStageWorld *w) {
  // XT 8881-8950: Glitch World -- cars with less grip than the field
  // expects glitch about; 50 pieces spin.
  const int32_t np = w->nplayers;
  Mad *md = w->mads;
  CheckPoints *cp = w->cp;
  for (int32_t a2 = 0; a2 < np; a2++) {
    bool waster = false;
    if (cp->clear[a2] < 5) waster = true;
    float goalgrip = 34.5f + (((float)r->level[np - 1] * 1.75f) - 1.0f) * 0.2f;
    if (goalgrip > 56.0f) goalgrip = 56.0f;
    float gripmod = (md[a2].cd->grip[md[a2].cn] - (goalgrip - 25.0f)) / 25.0f;
    if (gripmod < 0.5f) gripmod = 0.5f;
    if (gripmod >= 1.0f) {
      gripmod = 1.0f;
      cs->glitchtimer[a2] = 0;
    }
    const float gripaffect = (gripmod - 0.5f) / 0.5f;
    const int32_t ttg = 800 + jtrunc(2000.0f * gripaffect);
    const int32_t g = cs->glitchtimer[a2];
    if ((g + a2 * 100) % (ttg * 3) != 0 || g == 0) {
      if ((g + a2 * 100) % ttg != 0 || g == 0 || (g == ttg - a2 * 100 && a2 != 0)) cs->glitchtimer[a2]++;
      else randtele(cs, w, a2, waster, false);
    } else {
      randtele(cs, w, a2, waster, true);
    }
  }
  cs->pieceglitch++;
  for (int32_t a2 = 0; a2 < 50; a2++) {
    if ((cs->pieceglitch + a2 * 10) % 200 == 0 && cs->pieceglitch > 10) cs->piecespin[a2] = true;
    if (cs->piecespin[a2]) {
      cs->spintime[a2]++;
      if (cs->spintime[a2] > 75) {
        cs->piecespin[a2] = false;
        cs->spintime[a2] = 0;
      }
    }
  }
  for (int32_t a2 = 0; a2 < 50; a2++) {
    ContO *o = obj(w, np + a2);
    if (cs->pieceglitch <= 10 && o) {
      cs->origposx[a2] = o->x;
      cs->origposz[a2] = o->z;
      cs->origposxz[a2] = o->xz;
    }
    if (cs->piecespin[a2]) {
      // The draws happen whether the piece is there or not.
      const int32_t dy = jtrunc(mrand(w) * 1250.0f);
      if (o) o->y = (250 - o->grat) - dy;
      if (a2 % 2 == 0) {
        if (o) o->xz += 32;
        const int32_t dx = jtrunc(mrand(w) * 1200.0f);
        if (o) o->x = (cs->origposx[a2] - 600) + dx;
        const int32_t dz = jtrunc(mrand(w) * 1200.0f);
        if (o) o->z = (cs->origposz[a2] - 600) + dz;
      }
    } else if (o) {
      o->y = 250 - o->grat;
      o->x = cs->origposx[a2];
      o->z = cs->origposz[a2];
      o->xz = cs->origposxz[a2];
    }
  }
}

static void wasted_away(CareerStage *cs, const CareerStageWorld *w) {
  // XT 8951-8966: stages 3, 13, 17, 21 -- a wasted opponent is taken off
  // the map after 80 ticks.
  for (int32_t a = 1; a < w->nplayers; a++) {
    if (!w->mads[a].dest) continue;
    if (cs->destimer[a] > 80) {
      w->cars[a].x = 500000 + a * 1000;
      w->cars[a].z = 500000;
      w->cars[a].y = -100000;
      cs->norender[a] = true;
    } else {
      cs->destimer[a]++;
    }
  }
}

static void stage17(CareerStage *cs, const CareerRace *r, const CareerStageWorld *w) {
  // XT 8967-9049: Undead -- three undead cars come every 500 ticks for the
  // leader.
  const int32_t np = w->nplayers;
  Mad *md = w->mads;
  CheckPoints *cp = w->cp;
  if (cs->undeadswitch % 500 != 0 || cs->undeadswitch < 500) {
    cs->undeadswitch++;
    cs->newtarget = false;
    cs->generate = false;
  } else {
    for (int32_t a = 0; a < np; a++) {
      if (a == np - 1 || r->shadow[a] || (a >= 1 && a <= 3)) {
        cs->positions[a] = 1000 + a;
        cs->sortpos[a] = 1000 + a;
      } else if (cp->clear[a] >= 3) {
        cs->positions[a] = cp->pos[a];
        cs->sortpos[a] = cp->pos[a];
      } else {
        cs->positions[a] = 100 + a;
        cs->sortpos[a] = 100 + a;
      }
      // Arrays.sort over all 101 entries, the unused zeros included.
      qsort(cs->sortpos, CAREER_STAGE_XT, sizeof(int32_t), cmp_i32);
      if (cs->sortpos[0] < 100) {
        if (cs->sortpos[0] == cs->positions[a]) {
          cs->undeadtarget = a;
          cs->newtarget = true;
        }
      } else if (!cs->generate) {
        cs->undeadtarget = jtrunc_d(career_random() * (double)(np - 1));
        cs->generate = true;
      } else if (!r->shadow[cs->undeadtarget] && (cs->undeadtarget == 0 || cs->undeadtarget > 3) &&
                 !md[cs->undeadtarget].dest) {
        cs->newtarget = true;
      } else {
        cs->generate = false;
        cs->newtarget = false;
      }
      if (cs->newtarget) {
        cs->sendwarning = true;
        for (int32_t c = 1; c < 4; c++) attack(cs, w, cs->undeadtarget, c);
      }
    }
  }
  for (int32_t a = 1; a < 4 && a < np; a++) {
    if (!cs->undead[a]) {
      mad_distruct(&md[a], &w->cars[a]);
      cs->undead[a] = true;
    }
    md[a].hitmag = 0;
    cs->noarrow[a] = true;
    md[a].spatk = 0.0f;
    md[a].power = 98.0f;
    md[a].clear = -2;
    cs->newflame[a] = true;
    cp->clear[a] = -2;
    if (cs->undeadswitch < 500) {
      w->cars[a].y = -20000;
      w->cars[a].x = w->walls[0] + 1000000;
    }
  }
  if (cs->sendwarning) {
    if (cs->undeadswitch % 500 < 120 && cs->undeadswitch >= 500) {
      char text[96];
      if (cs->undeadtarget != 0) {
        const char *name = (w->names && cs->undeadtarget < np && w->names[cs->undeadtarget]) ? w->names[cs->undeadtarget]
                                                                                              : "a car";
        snprintf(text, sizeof(text), "The undead cars are targeting %s!", name);
      } else {
        snprintf(text, sizeof(text), "The undead cars are targeting you!");
      }
      flash_msg(cs, text);
    } else {
      cs->sendwarning = false;
    }
  }
}

static void start_protection(CareerStage *cs, const CareerRace *r, const CareerStageWorld *w) {
  // XT 9050-9074: nobody can hit anybody for the first 300 ticks (more on
  // 5, 21, 23), nor the field's beasts and three heavy cars for 200.
  const int32_t st = cs->stage;
  int32_t extratime = 0;
  if (st == 21) extratime = 200;
  if (st == 5 || st == 23) extratime = 100;
  if (st >= 9 || st == 5) {
    if (cs->tempinv < 300 + extratime) {
      cs->invulnerable = true;
      cs->tempinv++;
    } else {
      cs->invulnerable = false;
    }
  }
  for (int32_t a2 = 1; a2 < w->nplayers; a2++) {
    const int32_t car = r->sc[a2];
    if ((r->beast[a2] && st < 11 && !cs->bonus) || car == 18 || car == 22 || car == 19) {
      if (cs->tempinv < 200) {
        cs->nohit[a2] = true;
        cs->tempinv++;
      } else {
        cs->nohit[a2] = false;
      }
    }
  }
}

void career_stage_tick(CareerStage *cs, const CareerRace *r, const CareerSave *s, CareerRun *run,
                       const CareerStageWorld *w) {
  (void)s;
  const int32_t st = cs->stage;
  Mad *md = w->mads;
  cs->nplayers = w->nplayers;
  cs->music_event = CAREER_MUSIC_NONE;
  cs->sound_redflash = false;
  cs->sound_scare = false;
  cs->release_hold = false;
  cs->hold_by_boss = false;
  cs->msg[0] = '\0';
  cs->boss_bar = false;
  cs->health_bar = false;

  // The Titan takes the player's landed stunts (Madness.drive 2862-2865).
  if (st == 23 && cs->bossbattle && cs->cstimer >= 4) cs->stunthealth += jtrunc(md[0].stunt_gain);
  // Stage 11's wrecked opponents are undead but count as wasted
  // (CheckPoints.checkstat 144-150: dest || fakedest).
  if (st == 11 && cs->bonus != 2) {
    int32_t wasted = 0;
    for (int32_t k = 1; k < w->nplayers; k++)
      if (md[k].dest || cs->fakedest[k]) wasted++;
    w->cp->wasted = wasted;
  }
  // GS 2481-2485: realwalls, then careermode$m.
  if (w->starcnt == 0) realwalls(cs, w);

  if (st == 15 && cs->bonus != 3) stage15(cs, w);
  if (st == 13) stage13(cs, w);
  if (st == 11 && cs->bonus != 2) stage11(cs, r, w);
  if (st == 6) stage6(cs, r, w);
  if (st == 7) stage7(cs, w);
  if (st == 12) stage12(cs, w);
  // (Stage 8's carnival glow cycle, XT 8232-8290, is drawing only.)
  if (cs->bonus == 4) bonus4(cs, r, w);
  if (st == 23) stage23(cs, r, w);
  if (st == 22) stage22(cs, w);
  if (st == 21) stage21(cs, w);
  if (st == 20 || st == 21) stages20_21(cs, run, w);
  if (st == 19) stage19(cs, r, w);
  if (st == 3 || st == 17 || st == 21 || st == 13) wasted_away(cs, w);
  if (st == 17) stage17(cs, r, w);
  start_protection(cs, r, w);
  // (XT 9075-9389, experience and the HUD's level bar: career.c.)
  for (int32_t k = 0; k < w->nplayers; k++) cs->last_clear[k] = md[k].clear;
  // Plane.d 481-490: the undead burn green, the stage 13 guardians in their health colour.
  for (int32_t k = 0; k < NFM_MAX_CARS; k++) {
    const bool on = k < w->nplayers;
    cs->flame_custom[k] = on && (cs->floorguardian[k] || cs->newflame[k]);
    if (on && cs->floorguardian[k]) {
      for (int32_t q = 0; q < 3; q++) cs->flame_rgb[k][q] = cs->dmgcolour[k][q];
    } else if (on && cs->newflame[k]) {
      cs->flame_rgb[k][0] = 50;
      cs->flame_rgb[k][1] = 180;
      cs->flame_rgb[k][2] = 255;
    }
  }
}

// ---------------------------------------------------------------------------
// stat$m

int32_t career_stage_wasted(const CareerStage *cs, const Mad *mads, int32_t nplayers) {
  int32_t wasted = 0;
  for (int32_t k = 1; k < nplayers && k < NFM_MAX_CARS; k++)
    if (mads[k].dest || cs->fakedest[k]) wasted++;
  return wasted;
}

int32_t career_stage_undeadextra(const CareerStage *cs) {
  int32_t undeadextra = 0;
  if (cs->stage == 17) undeadextra = 3;
  if (cs->stage == 11 && cs->bonus != 2) undeadextra = 4;
  if (cs->stage == 23 && cs->cstimer < 10000 && cs->hard) undeadextra = 1;
  // This port's: the beaten Titan (cstimer 10000) stays undead, never
  // wasted, so the original's count (stat$m, XT 4000-4008) could not end the
  // race -- code past v2.8's END OF BETA wall that never ran. It still does
  // not count.
  if (cs->stage == 23 && cs->cstimer == 10000 && cs->hard) undeadextra = 1;
  return undeadextra;
}

bool career_stage_wasted_end_allowed(const CareerStage *cs) { return cs->cstimer < 2 || cs->cstimer == 10000; }

bool career_stage_finish_end_allowed(const CareerStage *cs) { return cs->cstimer < 2; }

bool career_stage_win_is_boss(CareerStage *cs) {
  const bool triggerboss = cs->stage == 23 && cs->cstimer < 10000 && cs->hard;
  if (triggerboss) cs->bossbattle = true;
  return triggerboss;
}

void career_stage_player_wasted(CareerStage *cs) { cs->bossbattle = false; }

bool career_stage_hold_advance(CareerStage *cs, const CareerStageWorld *w) {
  if (!cs->bossbattle) return false;
  if (cs->cstimer == 0) cs->cstimer++;
  if (cs->cstimer == 1) {
    for (int32_t a3 = 2; a3 < w->nplayers; a3++) w->mads[a3].hitmag = maxmag_of(&w->mads[a3]) + 1;
    if (w->mads[0].hitmag == 0) w->mads[0].hitmag = 1;
    cs->cstimer++;
  }
  return true;
}

// ---------------------------------------------------------------------------
// nitroandspecials' stage effects

bool career_stage_has_stat_effects(const CareerStage *cs) {
  return cs->stage == 9 || (cs->stage == 11 && cs->bonus != 2) || cs->stage == 20 || cs->bonus == 4;
}

/** XT 6979-6985 / 7032-7038: how well the car's grip holds against the fire. */
static float fire_affect(float g, float goal) {
  float fa = ((g - (goal * 5.0f) / 6.0f) * 0.25f) / (goal / 6.0f) + 0.75f;
  if (g < (goal * 5.0f) / 6.0f) fa = ((g - goal / 2.0f) * 0.2f) / (goal / 3.0f) + 0.55f;
  if (fa > 1.0f) fa = 1.0f;
  return fa;
}

void career_stage_stats(CareerStage *cs, const CareerRace *r, int32_t slot, CarDefine *base, const CarDefine *orig,
                        int32_t cn, float live_grip, const CheckPoints *cp) {
  *base = *orig;
  const int32_t car = r->sc[slot];
  int32_t sp[CS_N];
  for (int32_t k = 0; k < CS_N; k++) sp[k] = r->sp[slot][k];
  sp[CS_TS] = cs->sp_ts[slot];
  career_apply_stats(base, cn, car, sp, r->level[slot], r->shadow[slot], slot == 0);
  if (r->beast[slot]) {
    base->powerloss[cn] *= 3;
    base->clrad[cn] *= 2;
  }
  if (!career_stage_has_stat_effects(cs)) return;
  const int32_t np = cs->nplayers;
  const int32_t lastlevel = np > 0 ? r->level[np - 1] : 1;
  int32_t speedcut = 0;
  if (cs->stage == 20) speedcut = 400;
  if (cs->bonus == 4 && slot == 0 && cp) {
    const double proportion = (0.3 * cp->clear[0]) / (cp->nlaps * cp->nsp);
    speedcut = jtrunc_d(proportion * (orig->swits[cn][2] + cs->sp_ts[0]));
  }
  float firemod = 1.0f, statfall = 0.0f;
  double statdrop = 0.0;
  if (cs->stage == 11 && cs->bonus != 2) {
    float goalgrip = 24.2f + (float)(lastlevel * 3 - 1) * 0.2f;
    if (goalgrip > 45.6f) goalgrip = 45.6f;
    const float fireaffect = fire_affect(live_grip, goalgrip);
    const float nuclearmod = (fireaffect * fireaffect) * 0.9f + 0.1f;
    const double roundinterval = floor((double)(((nuclearmod * nuclearmod) * nuclearmod) * 2000.0f) + 0.5);
    const int32_t redinterval = jtrunc_d(roundinterval);
    if (redinterval < 2000) {
      statdrop = cs->statdrain[slot] / (double)redinterval;
      cs->nuclearmod[slot] = nuclearmod;
    } else {
      cs->nuclearmod[slot] = 1.0f;
    }
    statfall = (float)statdrop * 0.075f;
    const float statfalllimit = (10.0f + ((1.0f - nuclearmod) * 400.0f) / 9.0f) * 0.01f;
    if (statfall > statfalllimit) statfall = statfalllimit;
  }
  // Speed (XT 7002-7014).
  const int32_t maxspeed = jtrunc_d((double)(orig->swits[cn][2] + sp[CS_TS]) - speedcut);
  int32_t speed[3];
  speed[0] = orig->swits[cn][2] ? (orig->swits[cn][0] * maxspeed) / orig->swits[cn][2] : 0;
  speed[1] = orig->swits[cn][2] ? (orig->swits[cn][1] * maxspeed) / orig->swits[cn][2] : 0;
  speed[2] = maxspeed;
  for (int32_t k = 0; k < 3; k++)
    if (speed[k] < 20) speed[k] = 20;
  if (cs->stage == 9) {
    float goalgrip2 = 24.2f + (float)(lastlevel * 3 - 1) * 0.2f;
    if (goalgrip2 > 42.0f) goalgrip2 = 42.0f;
    const float g = live_grip;
    float gripmod = ((g - (goalgrip2 * 5.0f) / 6.0f) * 0.4f) / (goalgrip2 / 6.0f) + 0.6f;
    if (g < (goalgrip2 * 5.0f) / 6.0f) gripmod = ((g - goalgrip2 / 2.0f) * 0.35f) / (goalgrip2 / 3.0f) + 0.25f;
    if (gripmod < 0.25f) gripmod = 0.25f;
    if (gripmod > 1.0f) gripmod = 1.0f;
    const float powermod = gripmod * gripmod;
    cs->powermulti[slot] = 1.0f / powermod;
    const float fireaffect2 = fire_affect(g, goalgrip2);
    firemod = (fireaffect2 * fireaffect2) * 0.645f + 0.355f;
    if (firemod < 0.55f) firemod = 0.55f;
  }
  const float a0 = orig->acelf[cn][0];
  const float maxaccel = ((a0 + (float)sp[CS_ACC] / 10.0f) * ((1.0f - firemod) / 2.0f + firemod)) * (1.0f - statfall * 0.5f);
  base->acelf[cn][0] = maxaccel;
  base->acelf[cn][1] = (orig->acelf[cn][1] * maxaccel) / a0;
  base->acelf[cn][2] = (orig->acelf[cn][2] * maxaccel) / a0;
  base->grip[cn] = (orig->grip[cn] + (float)sp[CS_GRIP] * 0.2f) * (1.0f - statfall * 0.25f);
  base->airs[cn] = (orig->airs[cn] + (float)sp[CS_STU] * 0.025f) * (1.0f - statfall);
  base->airc[cn] = jtrunc((float)(orig->airc[cn] + sp[CS_STU]) * (1.0f - statfall));
  for (int32_t k = 0; k < 3; k++) base->swits[cn][k] = speed[k];
  base->maxmag[cn] = career_healthcalc(orig->maxmag[cn], sp[CS_END], car, firemod);
}

// ---------------------------------------------------------------------------
// GameSparker's collision gating

void career_stage_ghostmode(const CareerStage *cs, const CareerRace *r, const CareerStageWorld *w,
                            bool ghost[NFM_MAX_CARS][NFM_MAX_CARS]) {
  const int32_t np = w->nplayers, st = cs->stage;
  const CheckPoints *cp = w->cp;
  const Mad *md = w->mads;
  for (int32_t a = 0; a < NFM_MAX_CARS; a++)
    for (int32_t b = 0; b < NFM_MAX_CARS; b++) ghost[a][b] = false;
  const bool racingstage = (st == 5 && !cs->bonus) || st == 9 || st == 10 || st == 14 || st == 20;
  bool racer[NFM_MAX_CARS];
  for (int32_t a = 0; a < np; a++) racer[a] = cs->sp_ts[a] >= r->sp[a][CS_STR] || cp->clear[a] >= 3;
  const bool noff = st == 23 && cs->bossbattle;
  for (int32_t a6 = 0; a6 < np; a6++) {
    if (racingstage && racer[a6]) {
      for (int32_t b = 0; b < np; b++)
        if (a6 != b && cp->pos[a6] == 0 && cp->pos[b] == 1 && cp->clear[a6] == cp->clear[b]) ghost[a6][b] = true;
    }
    if (st == 13) {
      if (cs->forcehandb[a6] || cs->teleinvul[a6] > 0)
        for (int32_t b = 0; b < np; b++) ghost[a6][b] = true;
      for (int32_t b = 0; b < np; b++)
        if (a6 != b && md[a6].isabot && md[b].isabot) ghost[a6][b] = true;
      if (cs->floorguardian[a6] && !r->nolevels) {
        for (int32_t b = 1; b < np; b++)
          if (a6 != b && (!r->beast[b] || cs->floorguardian[b])) ghost[a6][b] = true;
      }
    }
    if (st == 17 && a6 >= 1 && a6 <= 3) {
      for (int32_t b = 0; b < np; b++)
        if (b != cs->undeadtarget) ghost[a6][b] = true;
    }
    if (cs->entered[a6])
      for (int32_t b = 0; b < np; b++) ghost[a6][b] = true;
  }
  for (int32_t a6 = 1; a6 < np; a6++) {
    if (st == 11 && cs->bonus != 2 && cs->hard) {
      const bool brk = w->botbreak ? w->botbreak[a6] : false;
      if (r->sc[a6] == 12 && !brk) {
        for (int32_t b = 1; b < np; b++) ghost[a6][b] = true;
        if (r->sp[0][CS_STR] <= cs->sp_ts[0]) {
          for (int32_t b = 0; b < np; b++)
            if (a6 != b && cp->pos[a6] == 0 && cp->pos[b] == 1 && cp->clear[a6] == cp->clear[b]) ghost[a6][b] = true;
          if (cp->clear[0] >= 3 && abs(cp->clear[0] - cp->clear[a6]) <= 3) ghost[a6][0] = true;
        }
      }
      if (cs->undead[a6]) {
        for (int32_t b = 0; b < np; b++)
          if (a6 != b && a6 > 4 && (b != w->controls[a6].acr || cs->safezone[b])) ghost[a6][b] = true;
      }
    }
    if (cs->invulnerable || cs->nohit[a6] || cs->bonus || noff)
      for (int32_t b = 1; b < np; b++) ghost[a6][b] = true;
  }
}

// ---------------------------------------------------------------------------
// stage 13's portals

void career_stage_portal_move(CareerStage *cs, const CareerStageWorld *w, int32_t im) {
  if (im < 0 || im >= w->nplayers || im >= NFM_MAX_CARS) return;
  career_phys_portal_move(cs, &w->mads[im], &w->cars[im], &w->controls[im], w->cp);
}

void career_stage_portal_detect(CareerStage *cs, const CareerStageWorld *w, int32_t im) {
  if (im < 0 || im >= w->nplayers || im >= NFM_MAX_CARS) return;
  const Mad *mad = &w->mads[im];
  const ContO *co = &w->cars[im];
  const CheckPoints *cp = w->cp;
  for (int32_t j = 0; j < cp->n; j++) career_phys_portal_check(cs, mad, co, cp, j);
  // MD 3379-3381 / 3401-3403: a checkpoint cleared while braking into the
  // portal gives a broken-off bot its recording back on arrival.
  if (cs->forcehandb[im] && mad->clear != cs->last_clear[im]) cs->telechk[im] = true;
}

// ---------------------------------------------------------------------------
// respawn

void career_stage_respawn(CareerStage *cs, Mad *mad, ContO *co, const CheckPoints *cp) {
  int32_t j1 = mad->pcleared - 1;
  if (j1 < 0) j1 = 0;   // the original reads typ[-1] (undefined, <= 0 is false) and keeps j1
  int32_t guard = 0;
  while (cp->n > 0 && cp->typ[j1] <= 0 && guard++ <= cp->n) {
    if (++j1 == cp->n) j1 = 0;
  }
  if (cp->clear[0] > 0) co->xz = cp->rotation[j1];
  else co->xz = 0;
  mad->squash = 0;
  mad->nbsq = 0;
  mad->hitmag = 0;
  mad->cntdest = 0;
  mad->dest = false;
  mad->newcar = true;
  if (mad->im >= 0 && mad->im < NFM_MAX_CARS) cs->respawning[mad->im] = true;
  if (cp->clear[0] > 0) {
    co->x = cp->x[j1];
    co->z = cp->z[j1];
    co->y = cp->y[j1] - 250;
  } else {
    co->x = -380;
    co->z = 380;
    co->y = -20250;
  }
}

void career_stage_respawn_reset(CareerStage *cs, Mad *mad, int32_t cn, ContO *co, CheckPoints *cp) {
  // Madness.reseto with respawning set keeps the race's progress and power
  // (MD 1135-1140).
  const int32_t pcleared = mad->pcleared, clear = mad->clear, nlaps = mad->nlaps;
  const float power = mad->power;
  mad_reseto(mad, cn, co, cp);
  mad->pcleared = pcleared;
  mad->clear = clear;
  mad->nlaps = nlaps;
  mad->power = power;
  if (mad->im >= 0 && mad->im < NFM_MAX_CARS) {
    cs->respawning[mad->im] = false;
    cs->groundlevel[mad->im] = 250.0f;   // Madness.reseto
    cs->speedmulti[mad->im] = 1.0f;
    cs->powermulti[mad->im] = 1.0f;
    cs->nuclearmod[mad->im] = 1.0f;
    cs->forcehandb[mad->im] = false;
    cs->teletimer[mad->im] = 0;
    cs->initialspeed[mad->im] = 0.0f;
    cs->teleinvul[mad->im] = 0;
    cs->telechk[mad->im] = false;
    cs->fakedest[mad->im] = false;
    cs->nostunts[mad->im] = 0;
    cs->xzadjust[mad->im] = 0;
  }
}

void career_stage_export_ai(const CareerStage *cs, const CareerStageWorld *w, XtCareerAI *ai) {
  for (int32_t k = 0; k < NFM_MAX_CARS; k++) {
    const bool on = k < w->nplayers;
    ai->undead[k] = on && cs->undead[k];
    ai->entered[k] = on && cs->entered[k];
    ai->floor[k] = cs->floor[k];
    ai->undeadlock[k] = cs->undeadlock[k];
    ai->nostunts[k] = cs->nostunts[k];
    ai->nofix[k] = on && career_phys_nofix(cs, &w->mads[k]);
    ai->groundlevel[k] = cs->groundlevel[k];
    ai->floorguardian[k] = on && cs->floorguardian[k];
    ai->guardswitch[k] = on && cs->guardswitch[k];
  }
  ai->undeadtarget = cs->undeadtarget;
  ai->targetcar = cs->targetcar;
  ai->verydark = cs->verydark;
  ai->bossbattle = cs->bossbattle;
  ai->invulnerable = cs->invulnerable;
}
