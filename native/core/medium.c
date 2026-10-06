// ports web/Medium.js (partial -- see medium.h for what's not here yet)
//
// Requires -fwrapv (see native/CMakeLists.txt / native/tests/CMakeLists.txt).
//
// A note on fr()-translation that applies to every method below: JS numbers
// are always double precision. `fr(X)` rounds X to float32 exactly once,
// at that call. Where X is a SINGLE binary op on operands that are already
// either exact integers or already-float32-rounded values, native C `float`
// arithmetic for that one op is provably identical to "compute in double,
// round once" (a double has enough mantissa to hold any float*float or
// float+float exactly before rounding). Where a SINGLE fr() wraps a
// MULTI-op expression (no inner fr() splitting it), or where trunc() is
// applied directly to an fr()-rounded value combined with further
// UNWRAPPED arithmetic (no fr() at all on that part), native float
// chaining would round more often than the JS does -- those cases compute
// in `double` and round/truncate once, using jtrunc_d(). Each site below
// says which case it is.
#include "medium.h"
#include "java_compat.h"
#include "trig.h"
#include "gfx.h"
#include "cont_o.h" // ContO -- only medium_around() needs it (co->x/y/z)
#include <math.h>
#include <stdlib.h>
#include <string.h>

// The 16 fog bands' distances (Medium.java's `fade` initializer). Objects past
// fade[disline] are not drawn, and colours fade toward the fog by band.
static const int32_t fade_init[16] = {3000, 4500, 6000, 7500, 9000, 10500, 12000, 13500,
                                      15000, 16500, 18000, 19500, 21000, 22500, 24000, 25500};

void medium_draw_distance(Medium *m, int32_t percent) {
  for (int32_t i = 0; i < 16; i++) m->fade[i] = fade_init[i] * percent / 100;
}

void medium_init(Medium *m) {
  memset(m, 0, sizeof(*m));

  m->focus_point = 400;
  m->ground = 250;
  m->skyline = -300;
  memcpy(m->fade, fade_init, sizeof(fade_init));
  static const int32_t cldd_init[5] = {210, 210, 210, 1, -1000};
  memcpy(m->cldd, cldd_init, sizeof(cldd_init));
  static const int32_t clds_init[3] = {210, 210, 210};
  memcpy(m->clds, clds_init, sizeof(clds_init));
  static const int32_t osky_init[3] = {170, 220, 255};
  memcpy(m->osky, osky_init, sizeof(osky_init));
  memcpy(m->csky, osky_init, sizeof(osky_init));
  static const int32_t ogrnd_init[3] = {205, 200, 200};
  memcpy(m->ogrnd, ogrnd_init, sizeof(ogrnd_init));
  memcpy(m->cgrnd, ogrnd_init, sizeof(ogrnd_init));
  static const int32_t texture_init[4] = {0, 0, 0, 50};
  memcpy(m->texture, texture_init, sizeof(texture_init));
  static const int32_t cpol_init[3] = {215, 210, 210};
  memcpy(m->cpol, cpol_init, sizeof(cpol_init));
  memcpy(m->crgrnd, ogrnd_init, sizeof(ogrnd_init));
  static const int32_t cfade_init[3] = {255, 220, 220};
  memcpy(m->cfade, cfade_init, sizeof(cfade_init));
  // snap = {0,0,0}, already zeroed.
  m->fogd = 7;
  // JS: trunc(random() * 100000.0) -- genuinely double in the Java (no fr()
  // in the JS), and bounded to [0,100000), so a plain truncating cast is
  // exact; jtrunc()'s float32 round-trip would be a needless (if usually
  // invisible) precision change here.
  m->mgen = (int32_t)(nfm_random() * 100000.0);
  m->lightn = -1;
  m->lilo = 217;
  m->cx = 400;
  m->cy = 225;
  m->cz = 50;
  m->w = 800;
  m->h = 450;
  m->vxz = 180;
  m->adv = 500;
  m->checkpoint = -1;
  m->rlog_cap = 8192;
  m->rlog = malloc(sizeof(float) * (size_t)m->rlog_cap);
  m->hit = 45000;
  m->ptcnt = -10;
  m->fo = 1.0f;
  // JS: fr(0.33000001311302185 + random() * 1.34) -- double arithmetic,
  // THEN rounded to float32 once at the end. The (float) cast here does
  // exactly that (C rounds the double result to float on assignment/cast).
  m->gofo = (float)(0.33000001311302185 + nfm_random() * 1.34);
  m->fvect = 200;
  m->resdown = 0;
  m->rescnt = 5;

  // Baked, not computed -- see trig.js / web/Medium.js's comment on why.
  memcpy(m->tcos, TCOS, sizeof(TCOS));
  memcpy(m->tsin, TSIN, sizeof(TSIN));
}

void medium_free(Medium *m) {
  free(m->rlog);
  m->rlog = NULL;

  // Procedural-generation state: not allocated by anything ported yet, but
  // written to be safe to call once it is (all pointers are NULL until
  // then, and free(NULL) is a no-op).
  if (m->ogpx) { for (int32_t i = 0; i < m->nrw * m->ncl; i++) free(m->ogpx[i]); free(m->ogpx); }
  if (m->ogpz) { for (int32_t i = 0; i < m->nrw * m->ncl; i++) free(m->ogpz[i]); free(m->ogpz); }
  if (m->pvr)  { for (int32_t i = 0; i < m->nrw * m->ncl; i++) free(m->pvr[i]);  free(m->pvr); }
  free(m->cgpx);
  free(m->cgpz);
  free(m->pmx);
  free(m->pcv);

  if (m->clax) {
    for (int32_t i = 0; i < m->noc; i++) {
      for (int32_t j = 0; j < 3; j++) { free(m->clax[i][j]); free(m->clay[i][j]); free(m->claz[i][j]); }
      free(m->clax[i]); free(m->clay[i]); free(m->claz[i]);
    }
    free(m->clax); free(m->clay); free(m->claz);
  }
  if (m->clc) {
    for (int32_t i = 0; i < m->noc; i++) {
      for (int32_t j = 0; j < 2; j++) {
        for (int32_t k = 0; k < 6; k++) free(m->clc[i][j][k]);
        free(m->clc[i][j]);
      }
      free(m->clc[i]);
    }
    free(m->clc);
  }
  free(m->clx);
  free(m->clz);
  free(m->cmx);

  if (m->mtx) {
    for (int32_t i = 0; i < m->nmt; i++) { free(m->mtx[i]); free(m->mty[i]); free(m->mtz[i]); }
    free(m->mtx); free(m->mty); free(m->mtz);
  }
  if (m->mtc) {
    for (int32_t i = 0; i < m->nmt; i++) {
      for (int32_t j = 0; j < m->nmv[i]; j++) free(m->mtc[i][j]);
      free(m->mtc[i]);
    }
    free(m->mtc);
  }
  free(m->mrd);
  free(m->nmv);

  if (m->stc) {
    for (int32_t i = 0; i < m->nst; i++) {
      for (int32_t j = 0; j < 2; j++) free(m->stc[i][j]);
      free(m->stc[i]);
    }
    free(m->stc);
  }
  free(m->stx);
  free(m->stz);
  free(m->bst);
  free(m->twn);

  memset(m, 0, sizeof(*m));
}



// JS computes `fr(a + (b - a) * (i - i0))` with a/b/i/i0 all JS numbers
// (i.e. double precision) and rounds to float32 ONCE, at the very end.
// Doing the same arithmetic in native `float` would round after every
// intermediate op instead (double rounding) -- usually identical, not
// provably always identical, so this deliberately computes in double and
// casts once, matching the JS instead of trusting the usual case.



float medium_random(Medium *m) {
  if (m->interpolating && m->rn != 0) {
    return m->rlog[(m->rp++) % m->rn];
  }
  bool draw = nfm_in_draw_phase();
  int32_t *rnd = draw ? m->drand : m->rand;
  bool *diup = draw ? m->ddiup : m->diup;
  if ((draw ? m->dcntrn : m->cntrn) == 0) {
    for (int32_t i = 0; i < 3; i++) {
      rnd[i] = (int32_t)(10.0 * nfm_random());
      // JS: `if (random() > random()) diup[i] = false; else diup[i] = true;`
      // -- JS evaluates left-to-right; C does not guarantee operand
      // evaluation order, so the two random() calls are sequenced through
      // named temporaries to preserve which one is "first".
      double r1 = nfm_random();
      double r2 = nfm_random();
      diup[i] = !(r1 > r2);
    }
    if (draw) m->dcntrn = 20; else m->cntrn = 20;
  } else {
    if (draw) --m->dcntrn; else --m->cntrn;
  }
  for (int32_t j = 0; j < 3; j++) {
    if (diup[j]) {
      ++rnd[j];
      if (rnd[j] == 10) rnd[j] = 0;
    } else {
      --rnd[j];
      if (rnd[j] == -1) rnd[j] = 9;
    }
  }
  int32_t trn = (draw ? m->dtrn : m->trn) + 1;
  if (trn == 3) trn = 0;
  if (draw) m->dtrn = trn; else m->trn = trn;
  // JS: fr(rand[trn] / 10.0) -- double division, rounded once at the end.
  float v = (float)((double)rnd[trn] / 10.0);
  if (m->recording) {
    if (m->rn == m->rlog_cap) {
      int32_t grown_cap = m->rlog_cap * 2;
      float *grown = malloc(sizeof(float) * (size_t)grown_cap);
      memcpy(grown, m->rlog, sizeof(float) * (size_t)m->rlog_cap);
      free(m->rlog);
      m->rlog = grown;
      m->rlog_cap = grown_cap;
    }
    m->rlog[m->rn++] = v;
  }
  return v;
}

void medium_follow(Medium *m, int32_t car_x, int32_t car_y, int32_t car_z, int32_t n, int32_t n2) {
  m->zy = 10;
  int32_t n3 = 2 + (int32_t)labs(m->bcxz) / 4;
  if (n3 > 20) n3 = 20;
  if (n2 != 0) {
    if (n2 == 1) {
      if (m->bcxz < 180) m->bcxz += n3;
      if (m->bcxz > 180) m->bcxz = 180;
    }
    if (n2 == -1) {
      if (m->bcxz > -180) m->bcxz -= n3;
      if (m->bcxz < -180) m->bcxz = -180;
    }
  } else if (labs(m->bcxz) > n3) {
    if (m->bcxz > 0) m->bcxz -= n3;
    else m->bcxz += n3;
  } else if (m->bcxz != 0) {
    m->bcxz = 0;
  }
  n += m->bcxz;
  m->xz = -n;
  float sin_n = medium_sin(m, (float)n);
  float cos_n = medium_cos(m, (float)n);
  // JS: `contO.z - 800 - contO.z` -- an identity (= -800), kept literal
  // per web/TRANSPILE_SPEC.md §0 rather than "simplified".
  int32_t dz = car_z - 800 - car_z;
  m->x = car_x - m->cx + jtrunc((float)(-dz) * sin_n); // fr(single multiply)
  m->z = car_z - m->cz + jtrunc((float)dz * cos_n);    // fr(single multiply)
  m->y = car_y - 250 - m->cy;
}

// Single-multiply fr() sites (17000.0f * cos/sin of a whole-number angle) --
// native float arithmetic here is exact, same reasoning as medium_follow above.
void medium_aroundtrack(Medium *m, CheckPoints *cp) {
  m->y = -m->hit;
  m->x = m->cx + m->trx + jtrunc(17000.0f * medium_cos(m, (float)m->vxz));
  m->z = m->trz + jtrunc(17000.0f * medium_sin(m, (float)m->vxz));
  if (m->hit > 5000) {
    if (m->hit == 45000) {
      m->fo = 1.0f;
      m->zy = 67;
      m->atrx = (cp->x[0] - m->trx) / 116;
      m->atrz = (cp->z[0] - m->trz) / 116;
      m->focus_point = 400;
    }
    if (m->hit == 20000) {
      m->fallen = 500;
      m->fo = 1.0f;
      m->zy = 67;
      m->atrx = (cp->x[0] - m->trx) / 116;
      m->atrz = (cp->z[0] - m->trz) / 116;
      m->focus_point = 400;
    }
    m->hit -= m->fallen;
    m->fallen += 7;
    m->trx += m->atrx;
    m->trz += m->atrz;
    if (m->hit < 17600) m->zy -= 2;
    if (m->fallen > 500) m->fallen = 500;
    if (m->hit <= 5000) {
      m->hit = 5000;
      m->fallen = 0;
    }
    m->vxz += 3;
  } else {
    m->focus_point = jtrunc(400.0f * m->fo);
    if (fabsf(m->fo - m->gofo) > 0.005f) {
      if (m->fo < m->gofo) m->fo += 0.005f;
      else m->fo -= 0.005f;
    } else {
      // fr()-equivalent double-then-narrow cast, same idiom as medium_init's
      // own gofo seed a few lines above in this file.
      m->gofo = (float)(0.3499999940395355 + nfm_random() * 1.3);
    }
    ++m->vxz;
    m->trx -= (m->trx - cp->x[m->ptr]) / 10;
    m->trz -= (m->trz - cp->z[m->ptr]) / 10;
    if (m->ptcnt == 7) {
      ++m->ptr;
      if (m->ptr == cp->n) {
        m->ptr = 0;
        ++m->nrnd;
      }
      m->ptcnt = 0;
    } else {
      ++m->ptcnt;
    }
  }
  if (m->vxz > 360) m->vxz -= 360;
  m->xz = -m->vxz - 90;
  m->cpflik = !m->cpflik;
}

// x/z: single-multiply fr() site ((int - int - int identity) * cos/sin of
// a whole-number angle) -- native float arithmetic here is exact, same
// reasoning as medium_follow. n3: multi-op (int)cast on a double
// expression straight from the decompile (sqrt/atan/divide chained, no
// per-op fr() to split it) -- computed in double and truncated once via
// jtrunc_d(), same idiom as cont_o.c's/control.c's own atan-angle sites.
void medium_around(Medium *m, ContO *co, bool b) {
  if (!b) {
    if (!m->vert) m->adv += 2;
    else m->adv -= 2;
    if (m->adv > 900) m->vert = true;
    if (m->adv < -500) m->vert = false;
  } else {
    m->adv -= 14;
    if (m->adv < 617) m->adv = 617;
  }
  int32_t n = 500 + m->adv;
  if (b && n < 1300) n = 1300;
  if (n < 1000) n = 1000;
  m->y = co->y - m->adv;
  if (m->y > 10) m->vert = false;
  // Medium.java:412-413 -- `contO.x - n - contO.x` is a deliberate
  // identity (= -n, contO.x cancels out) in BOTH the x and z lines --
  // kept literal per web/TRANSPILE_SPEC.md §0 rather than "simplified",
  // same as medium_follow's own `car_z - 800 - car_z`. Note the z line
  // also uses `co->x` (not `co->z`) for this term in the original -- since
  // it's an identity that always reduces to -n regardless of which
  // variable is used, this isn't a divergence worth "fixing".
  int32_t radius_term = co->x - n - co->x;
  m->x = co->x + jtrunc((float)radius_term * medium_cos(m, (float)m->vxz));
  m->z = co->z + jtrunc((float)radius_term * medium_sin(m, (float)m->vxz));
  if (!b) m->vxz += 2;
  else m->vxz += 4;
  int32_t n2 = 0;
  int32_t y = m->y;
  if (y > 0) y = 0;
  if (co->y - y - m->cy < 0) n2 = -180;
  int32_t dz = co->z - m->z + m->cz;
  int32_t dx = co->x - m->x - m->cx;
  int32_t dist = jtrunc_d(sqrt((double)(dz * dz + dx * dx)));
  int32_t n3 = jtrunc_d(90.0 + (double)n2 -
                         atan((double)dist / (double)(co->y - y - m->cy)) / 0.017453292519943295);
  m->xz = -m->vxz + 90;
  if (b) n3 -= 15;
  m->zy += (n3 - m->zy) / 10;
}

// Same fr()-classification as medium_around() just above: x/z are single-
// multiply sites (native float exact), n7 is the multi-op sqrt/atan chain
// (double, truncated once via jtrunc_d).
void medium_transaround(Medium *m, ContO *from, ContO *to, int32_t t) {
  int32_t n2 = (from->x * (20 - t) + to->x * t) / 20;
  int32_t n3 = (from->y * (20 - t) + to->y * t) / 20;
  int32_t n4 = (from->z * (20 - t) + to->z * t) / 20;

  if (!m->vert) m->adv += 2;
  else m->adv -= 2;
  if (m->adv > 900) m->vert = true;
  if (m->adv < -500) m->vert = false;
  int32_t n5 = 500 + m->adv;
  if (n5 < 1000) n5 = 1000;

  m->y = n3 - m->adv;
  if (m->y > 10) m->vert = false;
  // Medium.java:606-607 -- `n2 - n5 - n2` is the same "cancels to -n5"
  // identity medium_around()'s own radius_term is, kept literal.
  int32_t radius_term = n2 - n5 - n2;
  m->x = n2 + jtrunc((float)radius_term * medium_cos(m, (float)m->vxz));
  m->z = n4 + jtrunc((float)radius_term * medium_sin(m, (float)m->vxz));
  m->vxz += 2;

  int32_t n6 = 0;
  int32_t y = m->y;
  if (y > 0) y = 0;
  if (n3 - y - m->cy < 0) n6 = -180;
  int32_t dz = n4 - m->z + m->cz;
  int32_t dx = n2 - m->x - m->cx;
  int32_t dist = jtrunc_d(sqrt((double)(dz * dz + dx * dx)));
  int32_t n7 = jtrunc_d(90.0 + (double)n6 -
                         atan((double)dist / (double)(n3 - y - m->cy)) / 0.017453292519943295);
  m->xz = -m->vxz + 90;
  m->zy += (n7 - m->zy) / 10;
}

// Medium.java:274-302 -- the third of the three in-race camera views
// (GameSparker.java cycles view 0/1/2 = follow/around/watch on the V key).
// A fixed "TV tripod" that plants itself once, tracks the car by angle
// only, and re-plants whenever the car gets more than 6000 units away.
//
// `td` is that re-plant latch: true means "pick a new spot this frame".
// The planting expression in the decompile is written as
// `contO.x + 400 - contO.x` / `contO.z + 5000 - contO.z`, i.e. literal
// 400 and 5000 with the car's own coordinate added and subtracted again;
// those are folded to the constants here, which is exactly equivalent.
void medium_watch(Medium *m, ContO *co, int32_t n) {
  if (m->td) {
    m->y = jtrunc((float)(co->y - 300) - 1100.0f * medium_random(m));
    float c = medium_cos(m, (float)n), s = medium_sin(m, (float)n);
    m->x = co->x + jtrunc(400.0f * c - 5000.0f * s);
    m->z = co->z + jtrunc(400.0f * s + 5000.0f * c);
    m->td = false;
  }
  int32_t n2 = 0;
  if (co->x - m->x - m->cx > 0) n2 = 180;
  int32_t i = -jtrunc_d(90.0 + (double)n2 +
                         atan((double)(co->z - m->z) / (double)(co->x - m->x - m->cx)) /
                             0.017453292519943295);
  int32_t n3 = 0;
  if (co->y - m->y - m->cy < 0) n3 = -180;
  int32_t dz = co->z - m->z;
  int32_t dx = co->x - m->x - m->cx;
  int32_t dy = co->y - m->y - m->cy;
  // The sqrt is truncated to an int BEFORE the divide inside atan -- the
  // decompile has `(int)Math.sqrt(...)` there explicitly, so the rounding
  // happens at that inner step, not just on the final angle.
  int32_t flat = jtrunc_d(sqrt((double)(dz * dz + dx * dx)));
  int32_t n4 = jtrunc_d(90.0 + (double)n3 -
                         atan((double)flat / (double)dy) / 0.017453292519943295);
  while (i < 0) i += 360;
  while (i > 360) i -= 360;
  m->xz = i;
  m->zy += (n4 - m->zy) / 5;
  if (jtrunc_d(sqrt((double)(dz * dz + dx * dx + dy * dy))) > 6000) m->td = true;
}

void medium_groundpolys(Medium *m, Graphics2D *g) {
  int32_t n = (m->x - m->sgpx) / 1200 - 12;
  if (n < 0) n = 0;
  int32_t nrw = n + 25;
  if (nrw > m->nrw) nrw = m->nrw;
  if (nrw < n) nrw = n;
  int32_t n2 = (m->z - m->sgpz) / 1200 - 12;
  if (n2 < 0) n2 = 0;
  int32_t ncl = n2 + 25;
  if (ncl > m->ncl) ncl = m->ncl;
  if (ncl < n2) ncl = n2;

  int32_t rows = nrw - n > 0 ? nrw - n : 0;
  int32_t cols = ncl - n2 > 0 ? ncl - n2 : 0;
  int32_t *arr = NULL;
  if (rows > 0 && cols > 0) arr = calloc((size_t)rows * (size_t)cols, sizeof(int32_t));

  float cos_xz = medium_cos(m, (float)m->xz);
  float sin_xz = medium_sin(m, (float)m->xz);
  float cos_zy = medium_cos(m, (float)m->zy);
  float sin_zy = medium_sin(m, (float)m->zy);

  for (int32_t i = n; i < nrw; i++) {
    for (int32_t j = n2; j < ncl; j++) {
      int32_t idx = (i - n) * cols + (j - n2);
      arr[idx] = 0;
      int32_t n3 = i + j * m->nrw;
      if (m->resdown < 2 || n3 % 2 == 0) {
        // n4 = cx + trunc(fr(fr(A*cos_xz) - fr(B*sin_xz))) -- outer fr()
        // wraps one subtraction of two single-op-fr()'d products.
        float A = (float)(m->cgpx[n3] - m->x - m->cx) * cos_xz;
        float B = (float)(m->cgpz[n3] - m->z - m->cz) * sin_xz;
        int32_t n4 = m->cx + jtrunc(A - B);
        // n8 = (cz + trunc(fr(fr(A2*sin_xz) + fr(B2*cos_xz)))) - cz
        float A2 = (float)(m->cgpx[n3] - m->x - m->cx) * sin_xz;
        float B2 = (float)(m->cgpz[n3] - m->z - m->cz) * cos_xz;
        int32_t n8 = (m->cz + jtrunc(A2 + B2)) - m->cz;
        // n5 = cz + trunc(fr(fr(C*sin_zy) + fr(n8*cos_zy)))
        float C = (float)(250 - m->y - m->cy) * sin_zy;
        float D = (float)n8 * cos_zy;
        int32_t n5 = m->cz + jtrunc(C + D);

        if (medium_xs(m, n4 + m->pmx[n3], n5) > 0 &&
            medium_xs(m, n4 - m->pmx[n3], n5) < m->w &&
            n5 > -m->pmx[n3] && n5 < m->fade[2]) {
          arr[idx] = n5;
          int32_t a2[8], a3[8], a4[8];
          for (int32_t k = 0; k < 8; k++) {
            // trunc(fr(ogpx*pvr) + cgpx - x) -- fr() wraps ONLY the
            // multiply; the +cgpx-x is unwrapped JS-double arithmetic, so
            // this needs jtrunc_d, not native float chaining.
            float ogpx_pvr = (float)m->ogpx[n3][k] * m->pvr[n3][k];
            a2[k] = jtrunc_d((double)ogpx_pvr + (double)m->cgpx[n3] - (double)m->x);
            float ogpz_pvr = (float)m->ogpz[n3][k] * m->pvr[n3][k];
            a3[k] = jtrunc_d((double)ogpz_pvr + (double)m->cgpz[n3] - (double)m->z);
            a4[k] = m->ground;
          }
          medium_rot(m, a2, a3, m->cx, m->cz, m->xz, 8);
          medium_rot(m, a4, a3, m->cy, m->cz, m->zy, 8);
          int32_t a5[8], a6[8];
          int32_t c1 = 0, c2 = 0, c3 = 0, c4 = 0;
          bool ok = true;
          for (int32_t l = 0; l < 8; l++) {
            a5[l] = medium_xs(m, a2[l], a3[l]);
            a6[l] = medium_ys(m, a4[l], a3[l]);
            if (a6[l] < 0 || a3[l] < 10) c1++;
            if (a6[l] > m->h || a3[l] < 10) c2++;
            if (a5[l] < 0 || a3[l] < 10) c3++;
            if (a5[l] > m->w || a3[l] < 10) c4++;
          }
          if (c3 == 8 || c1 == 8 || c2 == 8 || c4 == 8) ok = false;
          if (ok) {
            // r = trunc(fr(fr(fr(cpol*pcv) + cgrnd) / 2.0)) -- three nested
            // single-op fr()s, native float chaining matches each.
            float P = (float)m->cpol[0] * m->pcv[n3];
            float Q = P + (float)m->cgrnd[0];
            int32_t r = jtrunc(Q / 2.0f);
            float Pg = (float)m->cpol[1] * m->pcv[n3];
            int32_t gg = jtrunc((Pg + (float)m->cgrnd[1]) / 2.0f);
            float Pb = (float)m->cpol[2] * m->pcv[n3];
            int32_t bb = jtrunc((Pb + (float)m->cgrnd[2]) / 2.0f);
            if (n5 - m->pmx[n3] > m->fade[0]) {
              r = (r * 7 + m->cfade[0]) / 8;
              gg = (gg * 7 + m->cfade[1]) / 8;
              bb = (bb * 7 + m->cfade[2]) / 8;
            }
            if (n5 - m->pmx[n3] > m->fade[1]) {
              r = (r * 7 + m->cfade[0]) / 8;
              gg = (gg * 7 + m->cfade[1]) / 8;
              bb = (bb * 7 + m->cfade[2]) / 8;
            }
            gfx_set_color(g, r, gg, bb);
            gfx_fill_polygon(g, a5, a6, 8);
          }
        }
      }
    }
  }

  for (int32_t i = n; i < nrw; i++) {
    for (int32_t j = n2; j < ncl; j++) {
      int32_t idx = (i - n) * cols + (j - n2);
      if (arr[idx] != 0) {
        int32_t n12 = i + j * m->nrw;
        int32_t a7[8], a8[8], a9[8];
        for (int32_t k = 0; k < 8; k++) {
          a7[k] = m->ogpx[n12][k] + m->cgpx[n12] - m->x;
          a8[k] = m->ogpz[n12][k] + m->cgpz[n12] - m->z;
          a9[k] = m->ground;
        }
        medium_rot(m, a7, a8, m->cx, m->cz, m->xz, 8);
        medium_rot(m, a9, a8, m->cy, m->cz, m->zy, 8);
        int32_t a10[8], a11[8];
        int32_t c1 = 0, c2 = 0, c3 = 0, c4 = 0;
        bool ok = true;
        for (int32_t l = 0; l < 8; l++) {
          a10[l] = medium_xs(m, a7[l], a8[l]);
          a11[l] = medium_ys(m, a9[l], a8[l]);
          if (a11[l] < 0 || a8[l] < 10) c1++;
          if (a11[l] > m->h || a8[l] < 10) c2++;
          if (a10[l] < 0 || a8[l] < 10) c3++;
          if (a10[l] > m->w || a8[l] < 10) c4++;
        }
        if (c3 == 8 || c1 == 8 || c2 == 8 || c4 == 8) ok = false;
        if (ok) {
          int32_t r2 = jtrunc((float)m->cpol[0] * m->pcv[n12]);
          int32_t g2v = jtrunc((float)m->cpol[1] * m->pcv[n12]);
          int32_t b4 = jtrunc((float)m->cpol[2] * m->pcv[n12]);
          if (arr[idx] - m->pmx[n12] > m->fade[0]) {
            r2 = (r2 * 7 + m->cfade[0]) / 8;
            g2v = (g2v * 7 + m->cfade[1]) / 8;
            b4 = (b4 * 7 + m->cfade[2]) / 8;
          }
          if (arr[idx] - m->pmx[n12] > m->fade[1]) {
            r2 = (r2 * 7 + m->cfade[0]) / 8;
            g2v = (g2v * 7 + m->cfade[1]) / 8;
            b4 = (b4 * 7 + m->cfade[2]) / 8;
          }
          gfx_set_color(g, r2, g2v, b4);
          gfx_fill_polygon(g, a10, a11, 8);
        }
      }
    }
  }

  free(arr);
}

static int32_t snapped(int32_t v, int32_t snap) {
  // trunc(fr(v + fr(v * fr(snap/100.0)))) -- three nested single-op fr()s.
  float p1 = (float)snap / 100.0f;
  float p2 = (float)v * p1;
  float p3 = (float)v + p2;
  int32_t c = jtrunc(p3);
  if (c > 255) c = 255;
  if (c < 0) c = 0;
  return c;
}

void medium_d(Medium *m, Graphics2D *g) {
  if (m->interpolating) {
    m->rp = 0;
  } else {
    m->rn = 0;
    m->recording = true;
  }
  m->nsp = 0;
  if (m->zy > 90) m->zy = 90;
  if (m->zy < -90) m->zy = -90;
  if (m->xz > 360) m->xz -= 360;
  if (m->xz < 0) m->xz += 360;
  if (m->y > 0) m->y = 0;
  m->ground = 250 - m->y;

  int32_t arr_x[4], arr_y[4];
  int32_t r = m->cgrnd[0], gg = m->cgrnd[1], bb = m->cgrnd[2];
  int32_t n = m->crgrnd[0], n2 = m->crgrnd[1], n3 = m->crgrnd[2];
  int32_t h = m->h;
  float cos_zy = medium_cos(m, (float)m->zy);
  float sin_zy = medium_sin(m, (float)m->zy);
  for (int32_t i = 0; i < 16; i++) {
    int32_t n4 = m->fade[i];
    int32_t ground_local = m->ground;
    if (m->zy != 0) {
      // Single-op fr() chains, same pattern as groundpolys' n4/n8.
      float A = (float)(m->ground - m->cy) * cos_zy;
      float B = (float)(m->fade[i] - m->cz) * sin_zy;
      ground_local = m->cy + jtrunc(A - B);
      float A2 = (float)(m->ground - m->cy) * sin_zy;
      float B2 = (float)(m->fade[i] - m->cz) * cos_zy;
      n4 = m->cz + jtrunc(A2 + B2);
    }
    arr_x[0] = m->iw;
    arr_y[0] = medium_ys(m, ground_local, n4);
    if (arr_y[0] < m->ih) arr_y[0] = m->ih;
    if (arr_y[0] > m->h) arr_y[0] = m->h;
    arr_x[1] = m->iw; arr_y[1] = h;
    arr_x[2] = m->w;  arr_y[2] = h;
    arr_x[3] = m->w;  arr_y[3] = arr_y[0];
    h = arr_y[0];
    if (i > 0) {
      n = (n * 7 + m->cfade[0]) / 8;
      n2 = (n2 * 7 + m->cfade[1]) / 8;
      n3 = (n3 * 7 + m->cfade[2]) / 8;
      if (i < 3) {
        r = (r * 7 + m->cfade[0]) / 8;
        gg = (gg * 7 + m->cfade[1]) / 8;
        bb = (bb * 7 + m->cfade[2]) / 8;
      } else {
        r = n; gg = n2; bb = n3;
      }
    }
    if (arr_y[0] < m->h && arr_y[1] > m->ih) {
      gfx_set_color(g, r, gg, bb);
      gfx_fill_polygon(g, arr_x, arr_y, 4);
    }
  }

  if (m->lightn != -1 && m->lton) {
    if (!m->interpolating) {
      if (m->lightn < 16) {
        if (m->lilo > m->lightn + 217) {
          m->lilo -= 3;
        } else {
          float p = 16.0f * medium_random(m); // fr(16*rand), single op
          m->lightn = jtrunc(16.0f + p);       // fr(16+p), single op
        }
      } else if (m->lilo < m->lightn + 217) {
        m->lilo += 7;
      } else {
        m->lightn = jtrunc(16.0f * medium_random(m)); // single op
      }
    }
    m->csky[0] = snapped(m->lilo, m->snap[0]);
    m->csky[1] = snapped(m->lilo, m->snap[1]);
    m->csky[2] = snapped(m->lilo, m->snap[2]);
  }

  int32_t r2 = m->csky[0], g2 = m->csky[1], b2 = m->csky[2];
  int32_t r3 = r2, g3 = g2, b3 = b2;
  {
    float A = (float)(m->skyline - 700 - m->cy) * cos_zy;
    float B = (float)(7000 - m->cz) * sin_zy;
    int32_t ys_x = m->cy + jtrunc(A - B);
    float A2 = (float)(m->skyline - 700 - m->cy) * sin_zy;
    float B2 = (float)(7000 - m->cz) * cos_zy;
    int32_t ys_z = m->cz + jtrunc(A2 + B2);
    // (reused below as the mutable `ys` cutoff)
    int32_t ys = medium_ys(m, ys_x, ys_z);
    int32_t ih = m->ih;
    for (int32_t j = 0; j < 16; j++) {
      int32_t n5 = m->fade[j];
      int32_t skyline_local = m->skyline;
      if (m->zy != 0) {
        float As = (float)(m->skyline - m->cy) * cos_zy;
        float Bs = (float)(m->fade[j] - m->cz) * sin_zy;
        skyline_local = m->cy + jtrunc(As - Bs);
        float As2 = (float)(m->skyline - m->cy) * sin_zy;
        float Bs2 = (float)(m->fade[j] - m->cz) * cos_zy;
        n5 = m->cz + jtrunc(As2 + Bs2);
      }
      arr_x[0] = m->iw;
      arr_y[0] = medium_ys(m, skyline_local, n5);
      if (arr_y[0] > m->h) arr_y[0] = m->h;
      if (arr_y[0] < m->ih) arr_y[0] = m->ih;
      arr_x[1] = m->iw; arr_y[1] = ih;
      arr_x[2] = m->w;  arr_y[2] = ih;
      arr_x[3] = m->w;  arr_y[3] = arr_y[0];
      ih = arr_y[0];
      if (j > 0) {
        r2 = (r2 * 7 + m->cfade[0]) / 8;
        g2 = (g2 * 7 + m->cfade[1]) / 8;
        b2 = (b2 * 7 + m->cfade[2]) / 8;
      }
      if (arr_y[1] < ys) {
        r3 = r2; g3 = g2; b3 = b2;
      }
      if (arr_y[0] > m->ih && arr_y[1] < m->h) {
        gfx_set_color(g, r2, g2, b2);
        gfx_fill_polygon(g, arr_x, arr_y, 4);
      }
    }

    arr_x[0] = m->iw; arr_y[0] = ih;
    arr_x[1] = m->iw; arr_y[1] = h;
    arr_x[2] = m->w;  arr_y[2] = h;
    arr_x[3] = m->w;  arr_y[3] = ih;
    if (arr_y[0] < m->h && arr_y[1] > m->ih) {
      // n6 = fr((abs(y)-250.0) / (fade[0]*2)) -- TWO ops (subtract, then
      // divide) under ONE fr(): must compute in double, round once.
      double num = (double)labs(m->y) - 250.0;
      double den = (double)(m->fade[0] * 2);
      float n6 = (float)(num / den);
      if (n6 < 0.0f) n6 = 0.0f;
      if (n6 > 1.0f) n6 = 1.0f;
      // Each channel: trunc(fr(fr(fr(r2*fr(1-n6)) + fr(n*fr(1+n6)))/2.0)) --
      // five nested single-op fr()s, native float chaining matches.
      float lo = 1.0f - n6, hi = 1.0f + n6;
      int32_t red = jtrunc((((float)r2 * lo) + ((float)n * hi)) / 2.0f);
      int32_t green = jtrunc((((float)g2 * lo) + ((float)n2 * hi)) / 2.0f);
      int32_t blue = jtrunc((((float)b2 * lo) + ((float)n3 * hi)) / 2.0f);
      gfx_set_color(g, red, green, blue);
      gfx_fill_polygon(g, arr_x, arr_y, 4);
    }

    if (m->resdown != 2) {
      for (int32_t k = 1; k < 20; k++) {
        int32_t n7 = 7000;
        int32_t n8 = m->skyline - 700 - k * 70;
        if (m->zy != 0 && k != 19) {
          float Ak = (float)(m->skyline - 700 - k * 70 - m->cy) * cos_zy;
          float Bk = (float)(7000 - m->cz) * sin_zy;
          n8 = m->cy + jtrunc(Ak - Bk);
          float Ak2 = (float)(m->skyline - 700 - k * 70 - m->cy) * sin_zy;
          float Bk2 = (float)(7000 - m->cz) * cos_zy;
          n7 = m->cz + jtrunc(Ak2 + Bk2);
        }
        arr_x[0] = m->iw;
        if (k != 19) {
          arr_y[0] = medium_ys(m, n8, n7);
          if (arr_y[0] > m->h) arr_y[0] = m->h;
          if (arr_y[0] < m->ih) arr_y[0] = m->ih;
        } else {
          arr_y[0] = m->ih;
        }
        arr_x[1] = m->iw; arr_y[1] = ys;
        arr_x[2] = m->w;  arr_y[2] = ys;
        arr_x[3] = m->w;  arr_y[3] = arr_y[0];
        ys = arr_y[0];
        // No fr() at all on these three -- genuinely double in the JS
        // (double literals 0.991/0.998, no fr() call), so jtrunc_d.
        r3 = jtrunc_d((double)r3 * 0.991);
        g3 = jtrunc_d((double)g3 * 0.991);
        b3 = jtrunc_d((double)b3 * 0.998);
        if (arr_y[1] > m->ih && arr_y[0] < m->h) {
          gfx_set_color(g, r3, g3, b3);
          gfx_fill_polygon(g, arr_x, arr_y, 4);
        }
      }
      // NOT ported: drawstars/drawmountains/drawclouds -- see medium.h.
      // Safe: noc/nmt/nst all stay 0 until newclouds/newmountains/newstars
      // exist, at which point those three would loop zero times anyway.
    }
  }

  medium_groundpolys(m, g);

  if (m->interpolating) return;
  if (m->noelec != 0) --m->noelec;
  if (m->cpflik) {
    m->cpflik = false;
  } else {
    m->cpflik = true;
    float t = medium_random(m) * 15.0f; // fr(rand*15), single op
    m->elecr = t - 6.0f;                // fr(t-6), single op
  }
}

void medium_addsp(Medium *m, int32_t x, int32_t z, int32_t n3) {
  if (m->nsp != 7) {
    m->spx[m->nsp] = x;
    m->spz[m->nsp] = z;
    m->sprad[m->nsp] = n3;
    m->nsp++;
  }
}

int32_t medium_snapped(int32_t v, int32_t snap) {
  // trunc(fr(v + fr(v * fr(snap/100.0)))) -- three single-op fr()s chained,
  // each case 1 (native float exact), plain trunc() at the end since
  // nothing unwrapped is mixed in after the outer fr().
  float sf = (float)snap / 100.0f;
  float scaled = (float)v * sf;
  float summed = (float)v + scaled;
  int32_t c = jtrunc(summed);
  if (c > 255) c = 255;
  if (c < 0) c = 0;
  return c;
}

void medium_setsnap(Medium *m, int32_t n, int32_t n2, int32_t n3) {
  m->snap[0] = n;
  m->snap[1] = n2;
  m->snap[2] = n3;
}

void medium_setsky(Medium *m, int32_t n, int32_t n2, int32_t n3) {
  m->osky[0] = n;
  m->osky[1] = n2;
  m->osky[2] = n3;
  for (int32_t i = 0; i < 3; i++) {
    int32_t v = (m->osky[i] * m->cldd[3] + m->cldd[i]) / (m->cldd[3] + 1);
    m->clds[i] = medium_snapped(v, m->snap[i]);
  }
  m->csky[0] = medium_snapped(n, m->snap[0]);
  m->csky[1] = medium_snapped(n2, m->snap[1]);
  m->csky[2] = medium_snapped(n3, m->snap[2]);
  float hsb[3];
  rgb_to_hsb(m->csky[0], m->csky[1], m->csky[2], hsb);
  m->darksky = hsb[2] < 0.6f;
}

void medium_setgrnd(Medium *m, int32_t n, int32_t n2, int32_t n3) {
  m->ogrnd[0] = n;
  m->ogrnd[1] = n2;
  m->ogrnd[2] = n3;
  for (int32_t i = 0; i < 3; i++) {
    int32_t v = (m->ogrnd[i] * m->texture[3] + m->texture[i]) / (1 + m->texture[3]);
    m->cpol[i] = medium_snapped(v, m->snap[i]);
  }
  m->cgrnd[0] = medium_snapped(n, m->snap[0]);
  m->cgrnd[1] = medium_snapped(n2, m->snap[1]);
  m->cgrnd[2] = medium_snapped(n3, m->snap[2]);
  for (int32_t j = 0; j < 3; j++) {
    // trunc((cpol*0.99 + cgrnd)/2.0) -- no fr() at all, genuinely double.
    m->crgrnd[j] = jtrunc_d(((double)m->cpol[j] * 0.99 + (double)m->cgrnd[j]) / 2.0);
  }
}

// Ports Medium.java's setexture()/setpolys(): both refine cpol (the ground
// POLYGON color plane_s() blends 99% toward for both the ground surface's
// own tint and, via crgrnd, the shadow tint under every car/object -- see
// cont_o.c's own shadow-drawing comment). Neither was previously ported at
// all, and no stage-file directive called them either (game_sparker.c's
// loadstage() had no "texture"/"polys" case) -- m->texture stayed at its
// {0,0,0,50} constructor default for every single stage, so cpol (and
// crgrnd) were computed from the WRONG texture tint/strength on 31 of this
// game's 32 stages (only one uses `polys(...)` instead of `texture(...)`,
// see game_sparker.c) -- the actual root cause of shadows (and the ground
// itself) reading a different color/intensity than the original.
void medium_setexture(Medium *m, int32_t n, int32_t n2, int32_t n3, int32_t n4) {
  if (n4 < 20) n4 = 20;
  if (n4 > 60) n4 = 60;
  m->texture[0] = n;
  m->texture[1] = n2;
  m->texture[2] = n3;
  m->texture[3] = n4;
  int32_t v0 = (m->ogrnd[0] * n4 + n) / (1 + n4);
  int32_t v1 = (m->ogrnd[1] * n4 + n2) / (1 + n4);
  int32_t v2 = (m->ogrnd[2] * n4 + n3) / (1 + n4);
  m->cpol[0] = medium_snapped(v0, m->snap[0]);
  m->cpol[1] = medium_snapped(v1, m->snap[1]);
  m->cpol[2] = medium_snapped(v2, m->snap[2]);
  for (int32_t i = 0; i < 3; i++) {
    // trunc((cpol*0.99 + cgrnd)/2.0) -- no fr() at all, genuinely double.
    m->crgrnd[i] = jtrunc_d(((double)m->cpol[i] * 0.99 + (double)m->cgrnd[i]) / 2.0);
  }
}

void medium_setpolys(Medium *m, int32_t n, int32_t n2, int32_t n3) {
  m->cpol[0] = medium_snapped(n, m->snap[0]);
  m->cpol[1] = medium_snapped(n2, m->snap[1]);
  m->cpol[2] = medium_snapped(n3, m->snap[2]);
  for (int32_t i = 0; i < 3; i++) {
    m->crgrnd[i] = jtrunc_d(((double)m->cpol[i] * 0.99 + (double)m->cgrnd[i]) / 2.0);
  }
}

void medium_setfade(Medium *m, int32_t n, int32_t n2, int32_t n3) {
  m->cfade[0] = medium_snapped(n, m->snap[0]);
  m->cfade[1] = medium_snapped(n2, m->snap[1]);
  m->cfade[2] = medium_snapped(n3, m->snap[2]);
}
