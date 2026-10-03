// ports Record.java in full (no equivalent web/Record.js port exists to
// cross-check against -- web/GameSparker.js's own lean scope never needed
// the replay, so this goes straight to the decompiled Java, same as
// Medium.around()/aroundtrack() did for the other two camera features
// this same investigation thread turned up). See record.h for the exact
// scope and the confirmed live-vs-replay recolor divergence.
#include "record.h"
#include "mad.h"
#include "java_compat.h"
#include "trackers.h"
#include <string.h>
#include <math.h>
#include <stdlib.h>

// Frees every ContO the Record owns (car[][], starcar[], ocar[]). The
// Record must be valid or all-zero. record_init() deliberately does NOT
// do this itself -- it is the constructor and may be handed uninitialised
// memory -- so a caller reusing a Record across races calls this first.
void record_free(Record *r) {
  for (int32_t j = 0; j < 6; j++)
    for (int32_t k = 0; k < 8; k++) cont_o_free(&r->car[j][k]);
  for (int32_t i = 0; i < 8; i++) {
    cont_o_free(&r->starcar[i]);
    cont_o_free(&r->ocar[i]);
  }
}

void record_init(Record *r) {
  memset(r, 0, sizeof(*r));
  // Record.java:104,152-153 -- the constructor's own non-zero defaults.
  // `fix`/`dest` (live) stay 0 here; only record_reset() sets THOSE to -1
  // (Record.java:203-204), matching the Java's own split between
  // construction-time and per-race-reset-time defaults exactly.
  r->cntf = 50;
  for (int32_t i = 0; i < 8; i++) {
    r->hfix[i] = -1;
    r->hdest[i] = -1;
  }
}

void record_reset(Record *r, ContO *cars[8]) {
  r->caught = 0;
  r->hcaught = false;
  r->wasted = 0;
  r->whenwasted = 0;
  r->closefinish = 0;
  r->powered = 0;
  for (int32_t i = 0; i < 8; i++) {
    if (r->prepit) cont_o_recopy(&r->starcar[i], cars[i], 0, 0, 0, 0);
    r->fix[i] = -1;
    r->dest[i] = -1;
    r->cntdest[i] = 0;
  }
  for (int32_t j = 0; j < 6; j++) {
    for (int32_t k = 0; k < 8; k++) {
      cont_o_recopy(&r->car[j][k], cars[k], 0, 0, 0, 0);
      r->squash[j][k] = 0;
    }
  }
  for (int32_t l = 0; l < 8; l++) {
    r->nr[l] = 0;
    for (int32_t n = 0; n < 200; n++) r->rspark[l][n] = -1;
    for (int32_t n2 = 0; n2 < 20; n2++) {
      r->ns[l][n2] = 0;
      for (int32_t n3 = 0; n3 < 30; n3++) r->sspark[l][n2][n3] = -1;
    }
    for (int32_t n4 = 0; n4 < 4; n4++) {
      r->nry[l][n4] = 0;
      r->nrx[l][n4] = 0;
      r->nrz[l][n4] = 0;
      for (int32_t n5 = 0; n5 < 7; n5++) {
        r->ry[l][n4][n5] = -1;
        r->rx[l][n4][n5] = -1;
        r->rz[l][n4][n5] = -1;
      }
    }
  }
  r->prepit = false;
}

void record_recy(Record *r, int32_t n, double n2, bool b, int32_t n3) {
  int32_t slot = r->nry[n3][n];
  r->ry[n3][n][slot] = 300;
  r->magy[n3][n][slot] = jtrunc_d(n2);
  r->mtouch[n3][slot] = b;
  r->nry[n3][n]++;
  if (r->nry[n3][n] == 7) r->nry[n3][n] = 0;
}

void record_recx(Record *r, int32_t n, double n2, int32_t n3) {
  // Indexes via nry, not nrx, despite incrementing nrx -- matches the
  // JS's own comment ("uses nry per original Java bytecode/decompilation,
  // game bug preserved").
  int32_t slot = r->nry[n3][n];
  r->rx[n3][n][slot] = 300;
  r->magx[n3][n][slot] = jtrunc_d(n2);
  r->nrx[n3][n]++;
  if (r->nrx[n3][n] == 7) r->nrx[n3][n] = 0;
}

void record_recz(Record *r, int32_t n, double n2, int32_t n3) {
  int32_t slot = r->nry[n3][n]; // same quirk as recx
  r->rz[n3][n][slot] = 300;
  r->magz[n3][n][slot] = jtrunc_d(n2);
  r->nrz[n3][n]++;
  if (r->nrz[n3][n] == 7) r->nrz[n3][n] = 0;
}

// Record.java:305-418.
void record_rec(Record *r, ContO *contO, int32_t n, int32_t squash, int32_t lastcolido,
                 int32_t cntdest, int32_t im) {
  if (n == im) r->caught++;

  if (r->cntf == 50) {
    // The keyframe ring shifts down one slot. The source rebuilds slots
    // 0-4 as fresh ContO copies of slots 1-5, but a copy of a copy is the
    // same ContO as the copy (cont_o_init_copy reads only fields it also
    // writes unchanged), and ring slots are never modified after they are
    // made -- they are only ever copied FROM. So the slots are MOVED down
    // (ownership of their Planes with them) and only the new keyframe is
    // copied: one deep copy instead of six, which was a ~2.8M-instruction
    // spike every ~7 ticks. The exception is a gr == -15 face, whose mesh
    // ContO's constructor re-randomises from the SIMULATION random stream
    // on every copy: skipping those copies would shift every later sim
    // random, so a car with one keeps the original six copies. No shipped
    // car has one (only off-road track pieces do).
    bool rerandomised = false;
    for (int32_t k = 0; k < r->car[1][n].npl && !rerandomised; k++) {
      if (r->car[1][n].p[k].gr == -15) rerandomised = true;
    }
    if (rerandomised) {
      for (int32_t i = 0; i < 5; i++) {
        cont_o_recopy(&r->car[i][n], &r->car[i + 1][n], 0, 0, 0, 0);
        r->squash[i][n] = r->squash[i + 1][n];
      }
    } else {
      cont_o_free(&r->car[0][n]);
      for (int32_t i = 0; i < 5; i++) {
        r->car[i][n] = r->car[i + 1][n];
        r->squash[i][n] = r->squash[i + 1][n];
      }
      memset(&r->car[5][n], 0, sizeof(r->car[5][n]));
    }
    cont_o_recopy(&r->car[5][n], contO, 0, 0, 0, 0);
    r->squash[5][n] = squash;
    r->cntf = 0;
  } else {
    r->cntf++;
  }

  r->fix[n]--;
  if (cntdest != 0) r->dest[n]--;
  if (r->dest[n] == 230) {
    if (n == im) {
      record_cotchinow(r, im);
      r->whenwasted = 229;
    } else if (lastcolido != 0) {
      record_cotchinow(r, n);
      r->whenwasted = 165 + lastcolido;
    }
  }

  for (int32_t j = 0; j < 299; j++) {
    r->x[j][n] = r->x[j + 1][n];
    r->y[j][n] = r->y[j + 1][n];
    r->z[j][n] = r->z[j + 1][n];
    r->zy[j][n] = r->zy[j + 1][n];
    r->xy[j][n] = r->xy[j + 1][n];
    r->xz[j][n] = r->xz[j + 1][n];
    r->wxz[j][n] = r->wxz[j + 1][n];
    r->wzy[j][n] = r->wzy[j + 1][n];
  }
  r->x[299][n] = contO->x;
  r->y[299][n] = contO->y;
  r->z[299][n] = contO->z;
  r->xy[299][n] = contO->xy;
  r->zy[299][n] = contO->zy;
  r->xz[299][n] = contO->xz;
  r->wxz[299][n] = contO->wxz;
  r->wzy[299][n] = contO->wzy;

  if (n == im) {
    for (int32_t k = 0; k < 299; k++) {
      r->checkpoint[k] = r->checkpoint[k + 1];
      r->lastcheck[k] = r->lastcheck[k + 1];
    }
    r->checkpoint[299] = contO->m->checkpoint;
    r->lastcheck[299] = contO->m->lastcheck;
  }

  for (int32_t l = 0; l < 20; l++) {
    if (contO->stg && contO->stg[l] == 1) {
      int32_t slot = r->ns[n][l];
      r->sspark[n][l][slot] = 300;
      r->sx[n][l][slot] = contO->sx[l];
      r->sy[n][l][slot] = contO->sy[l];
      r->sz[n][l][slot] = contO->sz[l];
      r->smag[n][l][slot] = contO->osmag[l];
      r->scx[n][l][slot] = contO->scx[l];
      r->scz[n][l][slot] = contO->scz[l];
      r->ns[n][l]++;
      if (r->ns[n][l] == 30) r->ns[n][l] = 0;
    }
    for (int32_t n7 = 0; n7 < 30; n7++) r->sspark[n][l][n7]--;
  }

  if (contO->sprk_ != 0) {
    int32_t slot = r->nr[n];
    r->rspark[n][slot] = 300;
    r->sprk[n][slot] = contO->sprk_;
    r->srx[n][slot] = contO->srx;
    r->sry[n][slot] = contO->sry;
    r->srz[n][slot] = contO->srz;
    r->rcx[n][slot] = contO->rcx;
    r->rcy[n][slot] = contO->rcy;
    r->rcz[n][slot] = contO->rcz;
    r->nr[n]++;
    if (r->nr[n] == 200) r->nr[n] = 0;
  }
  for (int32_t n9 = 0; n9 < 200; n9++) r->rspark[n][n9]--;

  for (int32_t n11 = 0; n11 < 4; n11++) {
    for (int32_t n12 = 0; n12 < 7; n12++) {
      r->ry[n][n11][n12]--;
      r->rx[n][n11][n12]--;
      r->rz[n][n11][n12]--;
    }
  }
}

// Record.java:238-303 -- a purely mechanical deep-copy of every live ring
// into its h* twin, so plain struct/array assignment (matching layouts
// exactly) stands in for the Java's own explicit loops.
void record_cotchinow(Record *r, int32_t wasted) {
  if (r->caught < 300) return;
  r->wasted = wasted;
  for (int32_t i = 0; i < 8; i++) {
    cont_o_recopy(&r->starcar[i], &r->car[0][i], 0, 0, 0, 0);
    r->hsquash[i] = r->squash[0][i];
    r->hfix[i] = r->fix[i];
    r->hdest[i] = r->dest[i];
  }
  memcpy(r->hx, r->x, sizeof(r->hx));
  memcpy(r->hy, r->y, sizeof(r->hy));
  memcpy(r->hz, r->z, sizeof(r->hz));
  memcpy(r->hxy, r->xy, sizeof(r->hxy));
  memcpy(r->hzy, r->zy, sizeof(r->hzy));
  memcpy(r->hxz, r->xz, sizeof(r->hxz));
  memcpy(r->hwxz, r->wxz, sizeof(r->hwxz));
  memcpy(r->hwzy, r->wzy, sizeof(r->hwzy));
  memcpy(r->hcheckpoint, r->checkpoint, sizeof(r->hcheckpoint));
  memcpy(r->hlastcheck, r->lastcheck, sizeof(r->hlastcheck));
  memcpy(r->hsspark, r->sspark, sizeof(r->hsspark));
  memcpy(r->hsx, r->sx, sizeof(r->hsx));
  memcpy(r->hsy, r->sy, sizeof(r->hsy));
  memcpy(r->hsz, r->sz, sizeof(r->hsz));
  memcpy(r->hsmag, r->smag, sizeof(r->hsmag));
  memcpy(r->hscx, r->scx, sizeof(r->hscx));
  memcpy(r->hscz, r->scz, sizeof(r->hscz));
  memcpy(r->hrspark, r->rspark, sizeof(r->hrspark));
  memcpy(r->hsprk, r->sprk, sizeof(r->hsprk));
  memcpy(r->hsrx, r->srx, sizeof(r->hsrx));
  memcpy(r->hsry, r->sry, sizeof(r->hsry));
  memcpy(r->hsrz, r->srz, sizeof(r->hsrz));
  memcpy(r->hrcx, r->rcx, sizeof(r->hrcx));
  memcpy(r->hrcy, r->rcy, sizeof(r->hrcy));
  memcpy(r->hrcz, r->rcz, sizeof(r->hrcz));
  memcpy(r->hry, r->ry, sizeof(r->hry));
  memcpy(r->hmagy, r->magy, sizeof(r->hmagy));
  memcpy(r->hrx, r->rx, sizeof(r->hrx));
  memcpy(r->hmagx, r->magx, sizeof(r->hmagx));
  memcpy(r->hrz, r->rz, sizeof(r->hrz));
  memcpy(r->hmagz, r->magz, sizeof(r->hmagz));
  memcpy(r->hmtouch, r->mtouch, sizeof(r->hmtouch));
  r->hcaught = true;
}

// Shared by record_regy/regx/regz: recolor a damaged plane's HSB from its
// bfase, ported from Record.java's own regy/regx/regz (all three repeat
// this identical clamp chain -- kept as one helper here rather than
// copy-pasted three times, same reasoning as mad.c's own
// mad_recolor_plane). CONFIRMED DIFFERENT from mad_recolor_plane's clamp
// values (0.25/0.7/0.15/0.6/0.075/0.5/0.05) by direct side-by-side
// comparison of the two decompiled methods -- this is Record.java's own
// distinct replay-time recolor curve, not a bug to unify with the live
// one.
static void record_recolor_plane(Plane *p) {
  if (p->bfase > 20 && p->hsb[1] > 0.2f) p->hsb[1] = 0.2f;
  if (p->bfase > 30) {
    if (p->hsb[2] < 0.5f) p->hsb[2] = 0.5f;
    if (p->hsb[1] > 0.1f) p->hsb[1] = 0.1f;
  }
  if (p->bfase > 40) p->hsb[1] = 0.05f;
  if (p->bfase > 50) {
    if (p->hsb[2] > 0.8f) p->hsb[2] = 0.8f;
    p->hsb[0] = 0.075f;
    p->hsb[1] = 0.05f;
  }
  if (p->bfase > 60) p->hsb[0] = 0.05f;
  int32_t rgb = hsb_to_rgb(p->hsb[0], p->hsb[1], p->hsb[2]);
  p->c[0] = (rgb >> 16) & 255;
  p->c[1] = (rgb >> 8) & 255;
  p->c[2] = rgb & 255;
}

// Record.java:610-735.
void record_regy(Record *r, int32_t n, float a, bool b, ContO *contO, Mad *mad) {
  if (a <= 100.0f) return;
  a -= 100.0f;
  int32_t n3 = 0, n4 = 0;
  int32_t i = contO->zy;
  while (i < 360) i += 360;
  while (i > 360) i -= 360;
  if (i < 210 && i > 150) n3 = -1;
  if (i > 330 || i < 30) n3 = 1;
  int32_t j = contO->xy;
  while (j < 360) j += 360;
  while (j > 360) j -= 360;
  if (j < 210 && j > 150) n4 = -1;
  if (j > 330 || j < 30) n4 = 1;

  if (n4 * n3 == 0 || b) {
    for (int32_t k = 0; k < contO->npl; k++) {
      float n5 = 0.0f;
      for (int32_t l = 0; l < contO->p[k].n; l++) {
        if (contO->p[k].wz == 0 &&
            trackers_py(contO->keyx[n], contO->p[k].ox[l], contO->keyz[n], contO->p[k].oz[l]) < mad->cd->clrad[mad->cn]) {
          n5 = (a / 20.0f) * medium_random(mad->m);
          contO->p[k].oz[l] = jtrunc((float)contO->p[k].oz[l] + n5 * medium_sin(mad->m, (float)i));
          contO->p[k].ox[l] = jtrunc((float)contO->p[k].ox[l] - n5 * medium_sin(mad->m, (float)j));
        }
      }
      if (n5 != 0.0f) {
        if (fabsf(n5) >= 1.0f) {
          contO->p[k].chip = 1;
          contO->p[k].ctmag = n5;
        }
        if (!contO->p[k].nocol && contO->p[k].glass != 1) {
          contO->p[k].bfase = jtrunc((float)contO->p[k].bfase + n5);
          record_recolor_plane(&contO->p[k]);
        }
        if (contO->p[k].glass == 1) {
          contO->p[k].gr = jtrunc((float)contO->p[k].gr + fabsf(n5 * 1.5f));
        }
      }
    }
  }
  if (n4 * n3 == -1) {
    int32_t n8 = 0, n9 = 1;
    for (int32_t n10 = 0; n10 < contO->npl; n10++) {
      float n11 = 0.0f;
      for (int32_t n12 = 0; n12 < contO->p[n10].n; n12++) {
        if (contO->p[n10].wz == 0) {
          n11 = (a / 15.0f) * medium_random(mad->m);
          if ((abs(contO->p[n10].oy[n12] - mad->cd->flipy[mad->cn] - r->squash[0][mad->im]) < mad->cd->msquash[mad->cn] * 3 ||
               contO->p[n10].oy[n12] < mad->cd->flipy[mad->cn] + r->squash[0][mad->im]) &&
              r->squash[0][mad->im] < mad->cd->msquash[mad->cn]) {
            contO->p[n10].oy[n12] = jtrunc((float)contO->p[n10].oy[n12] + n11);
            n8 = jtrunc((float)n8 + n11);
            n9++;
          }
        }
      }
      if (contO->p[n10].glass == 1) {
        contO->p[n10].gr += 5;
      } else if (n11 != 0.0f) {
        contO->p[n10].bfase = jtrunc((float)contO->p[n10].bfase + n11);
      }
      if (fabsf(n11) >= 1.0f) {
        contO->p[n10].chip = 1;
        contO->p[n10].ctmag = n11;
      }
    }
    r->squash[0][mad->im] += n8 / n9;
  }
}

// Record.java:737-803.
void record_regx(Record *r, int32_t n, float a, ContO *contO, Mad *mad) {
  (void)r;
  if (fabsf(a) <= 100.0f) return;
  if (a > 100.0f) a -= 100.0f;
  if (a < -100.0f) a += 100.0f;
  for (int32_t i = 0; i < contO->npl; i++) {
    float a2 = 0.0f;
    for (int32_t j = 0; j < contO->p[i].n; j++) {
      if (contO->p[i].wz == 0 &&
          trackers_py(contO->keyx[n], contO->p[i].ox[j], contO->keyz[n], contO->p[i].oz[j]) < mad->cd->clrad[mad->cn]) {
        a2 = (a / 20.0f) * medium_random(mad->m);
        contO->p[i].oz[j] = jtrunc((float)contO->p[i].oz[j] - a2 * medium_sin(mad->m, (float)contO->xz) * medium_cos(mad->m, (float)contO->zy));
        contO->p[i].ox[j] = jtrunc((float)contO->p[i].ox[j] + a2 * medium_cos(mad->m, (float)contO->xz) * medium_cos(mad->m, (float)contO->xy));
      }
    }
    if (a2 != 0.0f) {
      if (fabsf(a2) >= 1.0f) {
        contO->p[i].chip = 1;
        contO->p[i].ctmag = a2;
      }
      if (!contO->p[i].nocol && contO->p[i].glass != 1) {
        contO->p[i].bfase = jtrunc((float)contO->p[i].bfase + fabsf(a2));
        record_recolor_plane(&contO->p[i]);
      }
      if (contO->p[i].glass == 1) {
        contO->p[i].gr = jtrunc((float)contO->p[i].gr + fabsf(a2 * 1.5f));
      }
    }
  }
}

// Record.java:805-871.
void record_regz(Record *r, int32_t n, float a, ContO *contO, Mad *mad) {
  (void)r;
  if (fabsf(a) <= 100.0f) return;
  if (a > 100.0f) a -= 100.0f;
  if (a < -100.0f) a += 100.0f;
  for (int32_t i = 0; i < contO->npl; i++) {
    float a2 = 0.0f;
    for (int32_t j = 0; j < contO->p[i].n; j++) {
      if (contO->p[i].wz == 0 &&
          trackers_py(contO->keyx[n], contO->p[i].ox[j], contO->keyz[n], contO->p[i].oz[j]) < mad->cd->clrad[mad->cn]) {
        a2 = (a / 20.0f) * medium_random(mad->m);
        contO->p[i].oz[j] = jtrunc((float)contO->p[i].oz[j] + a2 * medium_cos(mad->m, (float)contO->xz) * medium_cos(mad->m, (float)contO->zy));
        contO->p[i].ox[j] = jtrunc((float)contO->p[i].ox[j] + a2 * medium_sin(mad->m, (float)contO->xz) * medium_cos(mad->m, (float)contO->xy));
      }
    }
    if (a2 != 0.0f) {
      if (fabsf(a2) >= 1.0f) {
        contO->p[i].chip = 1;
        contO->p[i].ctmag = a2;
      }
      if (!contO->p[i].nocol && contO->p[i].glass != 1) {
        contO->p[i].bfase = jtrunc((float)contO->p[i].bfase + fabsf(a2));
        record_recolor_plane(&contO->p[i]);
      }
      if (contO->p[i].glass == 1) {
        contO->p[i].gr = jtrunc((float)contO->p[i].gr + fabsf(a2 * 1.5f));
      }
    }
  }
}

// Record.java:873-894 and :896-917 -- chipx/chipz are IDENTICAL bodies in
// the decompiled Java (no mesh offset, unlike regx/regz -- just flags a
// one-shot "chip" spark), preserved as two separate functions rather than
// merged, matching the Java's own (probably unintentional) duplication.
void record_chipx(int32_t n, float a, ContO *contO, Mad *mad) {
  if (fabsf(a) <= 100.0f) return;
  if (a > 100.0f) a -= 100.0f;
  if (a < -100.0f) a += 100.0f;
  for (int32_t i = 0; i < contO->npl; i++) {
    float n2 = 0.0f;
    for (int32_t j = 0; j < contO->p[i].n; j++) {
      if (contO->p[i].wz == 0 &&
          trackers_py(contO->keyx[n], contO->p[i].ox[j], contO->keyz[n], contO->p[i].oz[j]) < mad->cd->clrad[mad->cn]) {
        n2 = (a / 20.0f) * medium_random(mad->m);
      }
    }
    if (n2 != 0.0f && fabsf(n2) >= 1.0f) {
      contO->p[i].chip = 1;
      contO->p[i].ctmag = n2;
    }
  }
}

void record_chipz(int32_t n, float a, ContO *contO, Mad *mad) {
  if (fabsf(a) <= 100.0f) return;
  if (a > 100.0f) a -= 100.0f;
  if (a < -100.0f) a += 100.0f;
  for (int32_t i = 0; i < contO->npl; i++) {
    float n2 = 0.0f;
    for (int32_t j = 0; j < contO->p[i].n; j++) {
      if (contO->p[i].wz == 0 &&
          trackers_py(contO->keyx[n], contO->p[i].ox[j], contO->keyz[n], contO->p[i].oz[j]) < mad->cd->clrad[mad->cn]) {
        n2 = (a / 20.0f) * medium_random(mad->m);
      }
    }
    if (n2 != 0.0f && fabsf(n2) >= 1.0f) {
      contO->p[i].chip = 1;
      contO->p[i].ctmag = n2;
    }
  }
}

// Record.java:420-492.
void record_play(Record *r, ContO *contO, Mad *mad, int32_t n, int32_t n2) {
  contO->x = r->x[n2][n];
  contO->y = r->y[n2][n];
  contO->z = r->z[n2][n];
  contO->zy = r->zy[n2][n];
  contO->xy = r->xy[n2][n];
  contO->xz = r->xz[n2][n];
  contO->wxz = r->wxz[n2][n];
  contO->wzy = r->wzy[n2][n];
  if (n == 0) {
    contO->m->checkpoint = r->checkpoint[n2];
    contO->m->lastcheck = r->lastcheck[n2];
  }
  if (n2 == 0) r->cntdest[n] = 0;
  if (r->dest[n] == n2) r->cntdest[n] = 7;
  if (n2 == 0 && r->dest[n] < -1) {
    for (int32_t i = 0; i < contO->npl; i++) {
      if (contO->p[i].wz == 0 || contO->p[i].gr == -17 || contO->p[i].gr == -16) contO->p[i].embos = 13;
    }
  }
  if (r->cntdest[n] != 0) {
    for (int32_t j = 0; j < contO->npl; j++) {
      if (contO->p[j].wz == 0 || contO->p[j].gr == -17 || contO->p[j].gr == -16) contO->p[j].embos = 1;
    }
    r->cntdest[n]--;
  }
  for (int32_t k = 0; k < 20; k++) {
    for (int32_t l = 0; l < 30; l++) {
      if (r->sspark[n][k][l] == n2) {
        contO->stg[k] = 1;
        contO->sx[k] = r->sx[n][k][l];
        contO->sy[k] = r->sy[n][k][l];
        contO->sz[k] = r->sz[n][k][l];
        contO->osmag[k] = r->smag[n][k][l];
        contO->scx[k] = r->scx[n][k][l];
        contO->scz[k] = r->scz[n][k][l];
      }
    }
  }
  for (int32_t n3 = 0; n3 < 200; n3++) {
    if (r->rspark[n][n3] == n2) {
      contO->sprk_ = r->sprk[n][n3];
      contO->srx = r->srx[n][n3];
      contO->sry = r->sry[n][n3];
      contO->srz = r->srz[n][n3];
      contO->rcx = r->rcx[n][n3];
      contO->rcy = r->rcy[n][n3];
      contO->rcz = r->rcz[n][n3];
    }
  }
  for (int32_t n4 = 0; n4 < 4; n4++) {
    for (int32_t n5 = 0; n5 < 7; n5++) {
      if (r->ry[n][n4][n5] == n2) record_regy(r, n4, (float)r->magy[n][n4][n5], r->mtouch[n][n5], contO, mad);
      if (r->rx[n][n4][n5] == n2) record_regx(r, n4, (float)r->magx[n][n4][n5], contO, mad);
      if (r->rz[n][n4][n5] == n2) record_regz(r, n4, (float)r->magz[n][n4][n5], contO, mad);
    }
  }
}

// Record.java:494-577.
void record_playh(Record *r, ContO *contO, Mad *mad, int32_t n, int32_t lastfr, int32_t n2) {
  contO->x = r->hx[lastfr][n];
  contO->y = r->hy[lastfr][n];
  contO->z = r->hz[lastfr][n];
  contO->zy = r->hzy[lastfr][n];
  contO->xy = r->hxy[lastfr][n];
  contO->xz = r->hxz[lastfr][n];
  contO->wxz = r->hwxz[lastfr][n];
  contO->wzy = r->hwzy[lastfr][n];
  if (n == n2) {
    contO->m->checkpoint = r->hcheckpoint[lastfr];
    contO->m->lastcheck = r->hlastcheck[lastfr];
  }
  if (lastfr == 0) r->cntdest[n] = 0;
  if (r->hdest[n] == lastfr) r->cntdest[n] = 7;
  if (lastfr == 0 && r->hdest[n] < -1) {
    for (int32_t i = 0; i < contO->npl; i++) {
      if (contO->p[i].wz == 0 || contO->p[i].gr == -17 || contO->p[i].gr == -16) contO->p[i].embos = 13;
    }
  }
  if (r->cntdest[n] != 0) {
    for (int32_t j = 0; j < contO->npl; j++) {
      if (contO->p[j].wz == 0 || contO->p[j].gr == -17 || contO->p[j].gr == -16) contO->p[j].embos = 1;
    }
    r->cntdest[n]--;
  }
  for (int32_t k = 0; k < 20; k++) {
    for (int32_t l = 0; l < 30; l++) {
      if (r->hsspark[n][k][l] == lastfr) {
        contO->stg[k] = 1;
        contO->sx[k] = r->hsx[n][k][l];
        contO->sy[k] = r->hsy[n][k][l];
        contO->sz[k] = r->hsz[n][k][l];
        contO->osmag[k] = r->hsmag[n][k][l];
        contO->scx[k] = r->hscx[n][k][l];
        contO->scz[k] = r->hscz[n][k][l];
      }
    }
  }
  for (int32_t n3 = 0; n3 < 200; n3++) {
    if (r->hrspark[n][n3] == lastfr) {
      contO->sprk_ = r->hsprk[n][n3];
      contO->srx = r->hsrx[n][n3];
      contO->sry = r->hsry[n][n3];
      contO->srz = r->hsrz[n][n3];
      contO->rcx = r->hrcx[n][n3];
      contO->rcy = r->hrcy[n][n3];
      contO->rcz = r->hrcz[n][n3];
    }
  }
  for (int32_t n4 = 0; n4 < 4; n4++) {
    for (int32_t n5 = 0; n5 < 7; n5++) {
      if (r->hry[n][n4][n5] == lastfr && r->lastfr != lastfr) {
        record_regy(r, n4, (float)r->hmagy[n][n4][n5], r->hmtouch[n][n5], contO, mad);
      }
      if (r->hrx[n][n4][n5] == lastfr) {
        if (r->lastfr != lastfr) record_regx(r, n4, (float)r->hmagx[n][n4][n5], contO, mad);
        else record_chipx(n4, (float)r->hmagx[n][n4][n5], contO, mad);
      }
      if (r->hrz[n][n4][n5] == lastfr) {
        if (r->lastfr != lastfr) record_regz(r, n4, (float)r->hmagz[n][n4][n5], contO, mad);
        else record_chipz(n4, (float)r->hmagz[n][n4][n5], contO, mad);
      }
    }
  }
  r->lastfr = lastfr;
}
