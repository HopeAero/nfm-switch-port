// ports web/Plane.js
//
// Same fr()-translation rules as native/core/medium.c (see its header
// comment): single binary op under one fr() -> native `float` arithmetic
// is exact; multi-op expression under one fr(), or trunc() applied to an
// fr()-rounded value plus further UNWRAPPED arithmetic -> compute in
// `double`, round/truncate once (jtrunc_d). Each non-trivial site below
// says which case it is.
//
// rot()/xs()/ys() are IDENTICAL math to Medium's own (Plane.rot() calls
// this.m.cos()/sin(), exactly what medium_rot() already does), so they
// delegate to medium_rot/medium_xs/medium_ys rather than reimplementing.
#include "plane.h"
#include "java_compat.h"
#include "gfx.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

void plane_init(Plane *p, Medium *m, Trackers *t, const int32_t *ox, const int32_t *oz,
                 const int32_t *oy, int32_t n, int32_t *oc, int32_t glass, int32_t gr,
                 int32_t fs, int32_t wx, int32_t wy, int32_t wz, int32_t disline,
                 int32_t bfase, bool road, int32_t light, bool solo) {
  memset(p, 0, sizeof(*p));
  p->deltaf = 1.0f;
  p->projf = 1.0f;
  p->m = m;
  p->t = t;
  p->n = n;
  p->cap = n;
  p->ox = malloc(sizeof(int32_t) * (size_t)n);
  p->oz = malloc(sizeof(int32_t) * (size_t)n);
  p->oy = malloc(sizeof(int32_t) * (size_t)n);
  for (int32_t i = 0; i < n; i++) {
    p->ox[i] = ox[i];
    p->oy[i] = oy[i];
    p->oz[i] = oz[i];
  }
  for (int32_t j = 0; j < 3; j++) p->oc[j] = oc[j];

  if (gr == -15) {
    if (oc[0] == 211) {
      // trunc(random()*40.0 - 20.0) -- no fr() at all, genuinely double.
      int32_t dx = jtrunc_d(nfm_random() * 40.0 - 20.0);
      int32_t dz = jtrunc_d(nfm_random() * 40.0 - 20.0);
      for (int32_t k = 0; k < n; k++) {
        p->ox[k] += dx;
        p->oz[k] += dz;
      }
    }
    int32_t n6 = jtrunc_d(185.0 + nfm_random() * 20.0);
    oc[0] = (217 + n6) / 2;
    if (oc[0] == 211) oc[0] = 210;
    oc[1] = (189 + n6) / 2;
    oc[2] = (132 + n6) / 2;
    for (int32_t l = 0; l < n; l++) {
      // `if (random() > random())` evaluates both operands unconditionally,
      // left first (JS order); a further random() only happens inside the
      // body. Each axis is its own independent condition+body pair.
      double rx1 = nfm_random(), rx2 = nfm_random();
      if (rx1 > rx2) p->ox[l] = jtrunc_d((double)p->ox[l] + (8.0 * nfm_random() - 4.0));
      double ry1 = nfm_random(), ry2 = nfm_random();
      if (ry1 > ry2) p->oy[l] = jtrunc_d((double)p->oy[l] + (8.0 * nfm_random() - 4.0));
      double rz1 = nfm_random(), rz2 = nfm_random();
      if (rz1 > rz2) p->oz[l] = jtrunc_d((double)p->oz[l] + (8.0 * nfm_random() - 4.0));
    }
  }

  if (oc[0] == oc[1] && oc[1] == oc[2]) p->nocol = true;

  if (glass == 0) {
    for (int32_t i = 0; i < 3; i++) {
      // trunc(array4[i] + fr(array4[i] * fr(snap[i]/100.0))) -- the ADD is
      // NOT under an outer fr(), unlike Medium's snapped() helper. Case 3.
      float snap_frac = (float)m->snap[i] / 100.0f;
      float product = (float)oc[i] * snap_frac;
      p->c[i] = jtrunc_d((double)oc[i] + (double)product);
      if (p->c[i] > 255) p->c[i] = 255;
      if (p->c[i] < 0) p->c[i] = 0;
    }
  }
  if (glass == 1) {
    for (int32_t i = 0; i < 3; i++) {
      p->c[i] = (m->csky[i] * m->fade[0] * 2 + m->cfade[i] * 3000) / (m->fade[0] * 2 + 3000);
    }
  }
  if (glass == 2) {
    for (int32_t i = 0; i < 3; i++) {
      p->c[i] = jtrunc((float)m->crgrnd[i] * 0.925f);
    }
  }
  if (glass == 3) {
    for (int32_t i = 0; i < 3; i++) p->c[i] = oc[i];
  }

  p->disline = disline;
  p->bfase = bfase;
  p->glass = glass;
  rgb_to_hsb(p->c[0], p->c[1], p->c[2], p->hsb);
  if (glass == 3 && m->trk != 2) {
    p->hsb[1] += 0.05f;
    if (p->hsb[1] > 1.0f) p->hsb[1] = 1.0f;
  }
  if (!p->nocol && p->glass != 1) {
    if (p->bfase > 20 && p->hsb[1] > 0.25f) p->hsb[1] = 0.25f;
    if (p->bfase > 25 && p->hsb[2] > 0.7f) p->hsb[2] = 0.7f;
    if (p->bfase > 30 && p->hsb[1] > 0.15f) p->hsb[1] = 0.15f;
    if (p->bfase > 35 && p->hsb[2] > 0.6f) p->hsb[2] = 0.6f;
    if (p->bfase > 40) p->hsb[0] = 0.075f;
    if (p->bfase > 50 && p->hsb[2] > 0.5f) p->hsb[2] = 0.5f;
    if (p->bfase > 60) p->hsb[0] = 0.05f;
  }
  p->road = road;
  p->light = light;
  p->solo = solo;
  p->gr = gr;
  p->fs = fs;
  p->wx = wx;
  p->wy = wy;
  p->wz = wz;
  plane_deltafntyp(p);
}

void plane_free(Plane *p) {
  free(p->ox);
  free(p->oy);
  free(p->oz);
  memset(p, 0, sizeof(*p));
}

void plane_deltafntyp(Plane *p) {
  int32_t a1 = abs(p->ox[2] - p->ox[1]);
  int32_t a2 = abs(p->oy[2] - p->oy[1]);
  int32_t a3 = abs(p->oz[2] - p->oz[1]);
  if (a2 <= a1 && a2 <= a3) p->typ = 2;
  if (a1 <= a2 && a1 <= a3) p->typ = 1;
  if (a3 <= a1 && a3 <= a2) p->typ = 3;
  p->deltaf = 1.0f;
  for (int32_t i = 0; i < 3; i++) {
    for (int32_t j = 0; j < 3; j++) {
      if (j != i) {
        int32_t dx = p->ox[j] - p->ox[i], dy = p->oy[j] - p->oy[i], dz = p->oz[j] - p->oz[i];
        int32_t sumsq = dx * dx + dy * dy + dz * dz;
        // fr(Math.sqrt(sumsq) / 100.0) -- sqrt unrounded (no fr() on it
        // alone), single division op wrapped by fr(): double, round once.
        float dist = (float)(sqrt((double)sumsq) / 100.0);
        p->deltaf = p->deltaf * dist; // fr(deltaf * dist), single op
      }
    }
  }
  p->deltaf = p->deltaf / 3.0f; // fr(deltaf / 3.0), single op
}

void plane_loadprojf(Plane *p) {
  p->projf = 1.0f;
  for (int32_t i = 0; i < 3; i++) {
    for (int32_t j = 0; j < 3; j++) {
      if (j != i) {
        int32_t dx = p->ox[i] - p->ox[j], dz = p->oz[i] - p->oz[j];
        int32_t sumsq = dx * dx + dz * dz;
        float dist = (float)(sqrt((double)sumsq) / 100.0);
        p->projf = p->projf * dist;
      }
    }
  }
  p->projf = p->projf / 3.0f;
}


int32_t plane_spy(Plane *p, int32_t n, int32_t n2) {
  int32_t d = n - p->m->cx;
  int32_t sum = d * d + n2 * n2;
  return jtrunc_d(sqrt((double)sum)); // trunc(Math.sqrt(...)) -- no fr() at all
}

// java.awt.Color helpers used at plane_d's call sites.
static void set_hsb(Graphics2D *g, float h, float s, float b) {
  int32_t rgb = hsb_to_rgb(h, s, b);
  gfx_set_color(g, (rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255);
}

#define DARKER_FACTOR 0.7f

// java.awt.Color.darker()
static void set_darker(Graphics2D *g, int32_t r, int32_t gg, int32_t b) {
  int32_t rr = jtrunc((float)r * DARKER_FACTOR);
  int32_t gv = jtrunc((float)gg * DARKER_FACTOR);
  int32_t bb = jtrunc((float)b * DARKER_FACTOR);
  if (rr < 0) rr = 0;
  if (gv < 0) gv = 0;
  if (bb < 0) bb = 0;
  gfx_set_color(g, rr, gv, bb);
}

// java.awt.Color.brighter() -- note the JDK's special case for near-black.
static void set_brighter(Graphics2D *g, int32_t r, int32_t gg, int32_t b) {
  int32_t i = jtrunc(1.0f / (1.0f - DARKER_FACTOR));
  if (r == 0 && gg == 0 && b == 0) {
    gfx_set_color(g, i, i, i);
    return;
  }
  if (r > 0 && r < i) r = i;
  if (gg > 0 && gg < i) gg = i;
  if (b > 0 && b < i) b = i;
  int32_t rr = jtrunc((float)r / DARKER_FACTOR); if (rr > 255) rr = 255;
  int32_t gv = jtrunc((float)gg / DARKER_FACTOR); if (gv > 255) gv = 255;
  int32_t bb = jtrunc((float)b / DARKER_FACTOR); if (bb > 255) bb = 255;
  gfx_set_color(g, rr, gv, bb);
}

void plane_d(Plane *p, Graphics2D *g, int32_t n, int32_t n2, int32_t n3, int32_t cxz,
             int32_t n4, int32_t n5, int32_t n6, int32_t n7, bool b, int32_t n8) {
  g->faceCalls++;
  g->projVerts += p->n;
  if (p->master == 1) {
    if (p->av > 1500 && !p->m->crs) p->n = 12;
    else p->n = 20;
  }
  if (p->n > PLANE_MAX_N) return; // unreachable for parsed models, see plane.h
  int32_t array[PLANE_MAX_N];
  int32_t array2[PLANE_MAX_N];
  int32_t array3[PLANE_MAX_N];

  if (p->embos == 0) {
    for (int32_t i = 0; i < p->n; i++) {
      array[i] = p->ox[i] + n;
      array3[i] = p->oy[i] + n2;
      array2[i] = p->oz[i] + n3;
    }
    if ((p->gr == -11 || p->gr == -12 || p->gr == -13) && p->m->lastmaf == 1) {
      for (int32_t j = 0; j < p->n; j++) {
        array[j] = -p->ox[j] + n;
        array3[j] = p->oy[j] + n2;
        array2[j] = -p->oz[j] + n3;
      }
    }
  } else {
    if (p->embos <= 11 && medium_random(p->m) > 0.5f && p->glass != 1) {
      for (int32_t k = 0; k < p->n; k++) {
        // trunc(ox[k]+n+fr(15.0-fr(rand*30.0))) -- add is unwrapped: case 3.
        float jx = 15.0f - (medium_random(p->m) * 30.0f);
        array[k] = jtrunc_d((double)p->ox[k] + (double)n + (double)jx);
        float jy = 15.0f - (medium_random(p->m) * 30.0f);
        array3[k] = jtrunc_d((double)p->oy[k] + (double)n2 + (double)jy);
        float jz = 15.0f - (medium_random(p->m) * 30.0f);
        array2[k] = jtrunc_d((double)p->oz[k] + (double)n3 + (double)jz);
      }
      plane_rot(p, array, array3, n, n2, n4, p->n);
      plane_rot(p, array3, array2, n2, n3, n5, p->n);
      plane_rot(p, array, array2, n, n3, cxz, p->n);
      plane_rot(p, array, array2, p->m->cx, p->m->cz, p->m->xz + p->m->fxz, p->n);
      plane_rot(p, array3, array2, p->m->cy, p->m->cz, p->m->zy + p->m->fzy, p->n);
      int32_t array4[PLANE_MAX_N];
      int32_t array5[PLANE_MAX_N];
      for (int32_t l = 0; l < p->n; l++) {
        array4[l] = plane_xs(p, array[l], array2[l]);
        array5[l] = plane_ys(p, array3[l], array2[l]);
      }
      gfx_set_color(g, 230, 230, 230);
      gfx_fill_polygon(g, array4, array5, p->n);
    }
    float n9 = 1.0f;
    if (p->embos <= 4) n9 = 1.0f + (medium_random(p->m) / 5.0f); // fr(1+fr(rand/5)), single ops chained
    if (p->embos > 4 && p->embos <= 7) n9 = 1.0f + (medium_random(p->m) / 4.0f);
    if (p->embos > 7 && p->embos <= 9) {
      n9 = 1.0f + (medium_random(p->m) / 3.0f);
      if (p->hsb[2] > 0.7f) p->hsb[2] = 0.7f;
    }
    if (p->embos > 9 && p->embos <= 10) {
      n9 = 1.0f + (medium_random(p->m) / 2.0f);
      if (p->hsb[2] > 0.6f) p->hsb[2] = 0.6f;
    }
    if (p->embos > 10 && p->embos <= 12) {
      n9 = 1.0f + (medium_random(p->m) / 1.0f);
      if (p->hsb[2] > 0.5f) p->hsb[2] = 0.5f;
    }
    if (p->embos == 12) {
      p->chip = 1;
      p->ctmag = 2.0f;
      p->bfase = -7;
    }
    if (p->embos == 13) {
      p->hsb[1] = 0.2f;
      p->hsb[2] = 0.4f;
    }
    if (p->embos == 16) {
      p->pa = jtrunc(medium_random(p->m) * (float)p->n);
      p->pb = jtrunc(medium_random(p->m) * (float)p->n);
      // Re-rolls until the two differ, as the Java does -- bounded, so a
      // random source that keeps answering the same (see medium_random's
      // replay note) cannot hang the game; then any other vertex will do.
      for (int32_t tries = 0; p->pa == p->pb && tries < 64; tries++) {
        p->pb = jtrunc(medium_random(p->m) * (float)p->n);
      }
      if (p->pa == p->pb) p->pb = (p->pa + 1) % (p->n > 1 ? p->n : 1);
    }
    if (p->embos >= 16) {
      int32_t n10 = 1, n11 = 1;
      int32_t a = abs(n5);
      while (a > 270) a -= 360;
      if (abs(a) > 90) n10 = -1;
      int32_t a2 = abs(n4);
      while (a2 > 270) a2 -= 360;
      if (abs(a2) > 90) n11 = -1;
      int32_t array6[3], array7[3];
      array[0] = p->ox[p->pa] + n;
      array3[0] = p->oy[p->pa] + n2;
      array2[0] = p->oz[p->pa] + n3;
      array[1] = p->ox[p->pb] + n;
      array3[1] = p->oy[p->pb] + n2;
      array2[1] = p->oz[p->pb] + n3;
      while (abs(array[0] - array[1]) > 100) {
        if (array[1] > array[0]) array[1] -= 30; else array[1] += 30;
      }
      while (abs(array2[0] - array2[1]) > 100) {
        if (array2[1] > array2[0]) array2[1] -= 30; else array2[1] += 30;
      }
      // trunc(idiv(abs(a[0]-a[1]),3) * (0.5-rand)) -- idiv result is int,
      // (0.5-rand) is fr()'d nowhere -- genuinely double throughout (no fr()
      // in this whole statement at all). Case: plain double, jtrunc_d.
      int32_t n16 = jtrunc_d((double)(abs(array[0] - array[1]) / 3) * (0.5 - (double)medium_random(p->m)));
      int32_t n17 = jtrunc_d((double)(abs(array2[0] - array2[1]) / 3) * (0.5 - (double)medium_random(p->m)));
      array[2] = (array[0] + array[1]) / 2 + n16;
      array2[2] = (array2[0] + array2[1]) / 2 + n17;
      // trunc((abs(dx)+abs(dz))/1.5 * (fr(rand/2)+0.5)) -- again no fr()
      // wrapping the outer product/sum at all: double throughout.
      double n18d = ((double)(abs(array[0] - array[1]) + abs(array2[0] - array2[1])) / 1.5) *
                     ((double)(medium_random(p->m) / 2.0f) + 0.5);
      int32_t n18 = jtrunc_d(n18d);
      array3[2] = (array3[0] + array3[1]) / 2 - (n10 * n11) * n18;
      plane_rot(p, array, array3, n, n2, n4, 3);
      plane_rot(p, array3, array2, n2, n3, n5, 3);
      plane_rot(p, array, array2, n, n3, cxz, 3);
      plane_rot(p, array, array2, p->m->cx, p->m->cz, p->m->xz + p->m->fxz, 3);
      plane_rot(p, array3, array2, p->m->cy, p->m->cz, p->m->zy + p->m->fzy, 3);
      for (int32_t i = 0; i < 3; i++) {
        array6[i] = plane_xs(p, array[i], array2[i]);
        array7[i] = plane_ys(p, array3[i], array2[i]);
      }
      // trunc(fr(255.0 + fr(255.0 * fr(snap[0]/400.0)))) -- three nested
      // single-op fr()s, native float chaining matches.
      float snap0 = (float)p->m->snap[0] / 400.0f;
      int32_t r = jtrunc(255.0f + (255.0f * snap0));
      if (r > 255) r = 255;
      if (r < 0) r = 0;
      float snap1 = (float)p->m->snap[1] / 300.0f;
      int32_t gval = jtrunc(169.0f + (169.0f * snap1));
      if (gval > 255) gval = 255;
      if (gval < 0) gval = 0;
      float snap2 = (float)p->m->snap[2] / 200.0f;
      int32_t bval = jtrunc(89.0f + (89.0f * snap2));
      if (bval > 255) bval = 255;
      if (bval < 0) bval = 0;
      gfx_set_color(g, r, gval, bval);
      gfx_fill_polygon(g, array6, array7, 3);

      array[0] = p->ox[p->pa] + n;
      array3[0] = p->oy[p->pa] + n2;
      array2[0] = p->oz[p->pa] + n3;
      array[1] = p->ox[p->pb] + n;
      array3[1] = p->oy[p->pb] + n2;
      array2[1] = p->oz[p->pb] + n3;
      while (abs(array[0] - array[1]) > 100) {
        if (array[1] > array[0]) array[1] -= 30; else array[1] += 30;
      }
      while (abs(array2[0] - array2[1]) > 100) {
        if (array2[1] > array2[0]) array2[1] -= 30; else array2[1] += 30;
      }
      array[2] = (array[0] + array[1]) / 2 + n16;
      array2[2] = (array2[0] + array2[1]) / 2 + n17;
      array3[2] = (array3[0] + array3[1]) / 2 - (n10 * n11) * jtrunc_d((double)n18 * 0.8);
      plane_rot(p, array, array3, n, n2, n4, 3);
      plane_rot(p, array3, array2, n2, n3, n5, 3);
      plane_rot(p, array, array2, n, n3, cxz, 3);
      plane_rot(p, array, array2, p->m->cx, p->m->cz, p->m->xz + p->m->fxz, 3);
      plane_rot(p, array3, array2, p->m->cy, p->m->cz, p->m->zy + p->m->fzy, 3);
      for (int32_t i = 0; i < 3; i++) {
        array6[i] = plane_xs(p, array[i], array2[i]);
        array7[i] = plane_ys(p, array3[i], array2[i]);
      }
      float snap0b = (float)p->m->snap[0] / 400.0f;
      int32_t r2 = jtrunc(255.0f + (255.0f * snap0b));
      if (r2 > 255) r2 = 255;
      if (r2 < 0) r2 = 0;
      float snap1b = (float)p->m->snap[1] / 300.0f;
      int32_t g2v = jtrunc(207.0f + (207.0f * snap1b));
      if (g2v > 255) g2v = 255;
      if (g2v < 0) g2v = 0;
      float snap2b = (float)p->m->snap[2] / 200.0f;
      int32_t b2v = jtrunc(136.0f + (136.0f * snap2b));
      if (b2v > 255) b2v = 255;
      if (b2v < 0) b2v = 0;
      gfx_set_color(g, r2, g2v, b2v);
      gfx_fill_polygon(g, array6, array7, 3);
    }
    for (int32_t i25 = 0; i25 < p->n; i25++) {
      if (p->typ == 1) array[i25] = jtrunc((float)p->ox[i25] * n9) + n; // fr(ox*n9), single op
      else array[i25] = p->ox[i25] + n;
      if (p->typ == 2) array3[i25] = jtrunc((float)p->oy[i25] * n9) + n2;
      else array3[i25] = p->oy[i25] + n2;
      if (p->typ == 3) array2[i25] = jtrunc((float)p->oz[i25] * n9) + n3;
      else array2[i25] = p->oz[i25] + n3;
    }
    if (!p->m->interpolating) {
      if (p->embos != 70) ++p->embos;
      else p->embos = 16;
    }
  }

  // JS: `if (this.wz !== 0)` / `if (this.wx !== 0)` -- gated on THIS
  // PLANE's own wheel-center offset (nonzero only for the ~19 planes per
  // wheel the `w(...)` command tags, see cont_o_init_buf), NOT on
  // whether the car-wide wzy/wxz angle happens to be nonzero. Getting
  // this backwards (checking n7/n6 instead of p->wz/p->wx) rotated every
  // plane in the WHOLE car -- body panels included -- around a
  // wheel-roll/steer pivot any time the car had any speed or steering
  // input at all, visibly warping the chassis (reported as "the car
  // looks like it turned into a tricycle" once real driving exercised
  // this for the first time -- a fresh/parked car has wzy=wxz=0, so M1's
  // static screenshots never hit it).
  if (p->wz != 0) plane_rot(p, array3, array2, p->wy + n2, p->wz + n3, n7, p->n);
  if (p->wx != 0) plane_rot(p, array, array2, p->wx + n, p->wz + n3, n6, p->n);

  if (p->chip == 1 && !p->m->interpolating && (medium_random(p->m) > 0.6f || p->bfase == 0)) {
    p->chip = 0;
    if (p->bfase == 0 && p->nocol) p->bfase = 1;
  }
  if (p->chip != 0) {
    if (p->chip == 1 && !p->m->interpolating) {
      p->cxz = cxz;
      p->cxy = n4;
      p->czy = n5;
      int32_t n26 = jtrunc(medium_random(p->m) * (float)p->n);
      p->cox[0] = p->ox[n26];
      p->coz[0] = p->oz[n26];
      p->coy[0] = p->oy[n26];
      if (p->ctmag > 3.0f) p->ctmag = 3.0f;
      if (p->ctmag < -3.0f) p->ctmag = -3.0f;
      // trunc(cox0 + fr(ctmag*fr(10.0-fr(rand*20.0)))) -- add unwrapped: case 3.
      // JS computes BOTH cox[1]/cox[2] first, then BOTH coy[1]/coy[2], then
      // BOTH coz[1]/coz[2] -- grouped by axis, not interleaved by index.
      // The random() draw order matters (see the module comment), so this
      // loop shape has to match exactly, not just produce "a" 1/2 index loop.
      for (int32_t i = 1; i < 3; i++) {
        float jitter_x = p->ctmag * (10.0f - (medium_random(p->m) * 20.0f));
        p->cox[i] = jtrunc_d((double)p->cox[0] + (double)jitter_x);
      }
      for (int32_t i = 1; i < 3; i++) {
        float jitter_y = p->ctmag * (10.0f - (medium_random(p->m) * 20.0f));
        p->coy[i] = jtrunc_d((double)p->coy[0] + (double)jitter_y);
      }
      for (int32_t i = 1; i < 3; i++) {
        float jitter_z = p->ctmag * (10.0f - (medium_random(p->m) * 20.0f));
        p->coz[i] = jtrunc_d((double)p->coz[0] + (double)jitter_z);
      }
      p->dx = 0; p->dy = 0; p->dz = 0;
      if (p->bfase != -7) {
        p->vx = jtrunc(p->ctmag * (30.0f - (medium_random(p->m) * 60.0f)));
        p->vz = jtrunc(p->ctmag * (30.0f - (medium_random(p->m) * 60.0f)));
        p->vy = jtrunc(p->ctmag * (30.0f - (medium_random(p->m) * 60.0f)));
      } else {
        p->vx = jtrunc(p->ctmag * (10.0f - (medium_random(p->m) * 20.0f)));
        p->vz = jtrunc(p->ctmag * (10.0f - (medium_random(p->m) * 20.0f)));
        p->vy = jtrunc(p->ctmag * (10.0f - (medium_random(p->m) * 20.0f)));
      }
      p->chip = 2;
    }
    int32_t array16[3], array17[3], array18[3];
    for (int32_t i27 = 0; i27 < 3; i27++) {
      array16[i27] = p->cox[i27] + n;
      array18[i27] = p->coy[i27] + n2;
      array17[i27] = p->coz[i27] + n3;
    }
    plane_rot(p, array16, array18, n, n2, p->cxy, 3);
    plane_rot(p, array18, array17, n2, n3, p->czy, 3);
    plane_rot(p, array16, array17, n, n3, p->cxz, 3);
    for (int32_t i28 = 0; i28 < 3; i28++) {
      array16[i28] += p->dx;
      array18[i28] += p->dy;
      array17[i28] += p->dz;
    }
    if (!p->m->interpolating) {
      p->dx = p->dx + p->vx;
      p->dz = p->dz + p->vz;
      p->dy = p->dy + p->vy;
      p->vy = p->vy + 7;
      if (array18[0] > p->m->ground) p->chip = 19;
    }
    plane_rot(p, array16, array17, p->m->cx, p->m->cz, p->m->xz + p->m->fxz, 3);
    plane_rot(p, array18, array17, p->m->cy, p->m->cz, p->m->zy + p->m->fzy, 3);
    int32_t array22[3], array23[3];
    for (int32_t i32 = 0; i32 < 3; i32++) {
      array22[i32] = plane_xs(p, array16[i32], array17[i32]);
      array23[i32] = plane_ys(p, array18[i32], array17[i32]);
    }
    int32_t n33 = jtrunc(medium_random(p->m) * 3.0f);
    if (p->bfase != -7) {
      if (n33 == 0) set_darker(g, p->c[0], p->c[1], p->c[2]);
      if (n33 == 1) gfx_set_color(g, p->c[0], p->c[1], p->c[2]);
      if (n33 == 2) set_brighter(g, p->c[0], p->c[1], p->c[2]);
    } else {
      set_hsb(g, p->hsb[0], p->hsb[1], p->hsb[2]);
    }
    gfx_fill_polygon(g, array22, array23, 3);
    if (!p->m->interpolating) {
      ++p->chip;
      if (p->chip == 20) p->chip = 0;
    }
  }

  plane_rot(p, array, array3, n, n2, n4, p->n);
  plane_rot(p, array3, array2, n2, n3, n5, p->n);
  plane_rot(p, array, array2, n, n3, cxz, p->n);
  if ((n4 != 0 || n5 != 0 || cxz != 0) && p->m->trk != 2) {
    p->projf = 1.0f;
    for (int32_t a34 = 0; a34 < 3; a34++) {
      for (int32_t a35 = 0; a35 < 3; a35++) {
        if (a35 != a34) {
          int32_t ddx = array[a34] - array[a35], ddz = array2[a34] - array2[a35];
          int32_t sumsq = ddx * ddx + ddz * ddz;
          float dist = (float)(sqrt((double)sumsq) / 100.0);
          p->projf = p->projf * dist;
        }
      }
    }
    p->projf = p->projf / 3.0f;
  }
  plane_rot(p, array, array2, p->m->cx, p->m->cz, p->m->xz + p->m->fxz, p->n);

  bool b4 = false;
  int32_t array24[PLANE_MAX_N];
  int32_t array25[PLANE_MAX_N];
  int32_t n36 = 500;
  for (int32_t a37 = 0; a37 < p->n; a37++) {
    array24[a37] = plane_xs(p, array[a37], array2[a37]);
    array25[a37] = plane_ys(p, array3[a37], array2[a37]);
  }
  int32_t n38 = 0, n39 = 1;
  for (int32_t a40 = 0; a40 < p->n; a40++) {
    for (int32_t a41 = a40; a41 < p->n; a41++) {
      if (a40 != a41 && abs(array24[a40] - array24[a41]) - abs(array25[a40] - array25[a41]) < n36) {
        n39 = a40;
        n38 = a41;
        n36 = abs(array24[a40] - array24[a41]) - abs(array25[a40] - array25[a41]);
      }
    }
  }
  if (array25[n38] < array25[n39]) {
    int32_t tmp = n38; n38 = n39; n39 = tmp;
  }
  if (plane_spy(p, array[n38], array2[n38]) > plane_spy(p, array[n39], array2[n39])) {
    b4 = true;
    int32_t n43 = 0;
    for (int32_t a44 = 0; a44 < p->n; a44++) {
      if (array2[a44] < 50 && array3[a44] > p->m->cy) {
        b4 = false;
      } else if (array3[a44] == array3[0]) {
        n43++;
      }
    }
    if (n43 == p->n && array3[0] > p->m->cy) b4 = false;
  }

  plane_rot(p, array3, array2, p->m->cy, p->m->cz, p->m->zy + p->m->fzy, p->n);
  int32_t n45 = 1;
  int32_t array26[PLANE_MAX_N];
  int32_t array27[PLANE_MAX_N];
  int32_t n46 = 0, n47 = 0, n48 = 0, n49 = 0, n50 = 0;
  for (int32_t a51 = 0; a51 < p->n; a51++) {
    array26[a51] = plane_xs(p, array[a51], array2[a51]);
    array27[a51] = plane_ys(p, array3[a51], array2[a51]);
    if (array27[a51] < p->m->ih || array2[a51] < 10) n46++;
    if (array27[a51] > p->m->h || array2[a51] < 10) n47++;
    if (array26[a51] < p->m->iw || array2[a51] < 10) n48++;
    if (array26[a51] > p->m->w || array2[a51] < 10) n49++;
    if (array2[a51] < 10) n50++;
  }
  if (n48 == p->n || n46 == p->n || n47 == p->n || n49 == p->n) n45 = 0;
  if ((p->m->trk == 1 || p->m->trk == 4) && (n48 != 0 || n46 != 0 || n47 != 0 || n49 != 0)) n45 = 0;
  if (p->m->trk == 3 && n50 != 0) n45 = 0;
  if (n50 != 0) b = true;
  if (n45 != 0 && n8 != -1) {
    // The source takes the largest |difference| over every vertex PAIR,
    // which for integers is just max - min of the projected extent: O(n)
    // instead of O(n^2) (~6% of a race frame). The pair loop's abs() of a
    // wrapped int32 difference only agrees with max - min while the range
    // fits in 31 bits, so a range that doesn't (never, for on-screen
    // geometry) falls back to the original loop to stay exact.
    int32_t abs3 = 0, abs4 = 0;
    int32_t mnx = p->n > 0 ? array26[0] : 0, mxx = mnx;
    int32_t mny = p->n > 0 ? array27[0] : 0, mxy = mny;
    for (int32_t a52 = 1; a52 < p->n; a52++) {
      if (array26[a52] < mnx) mnx = array26[a52];
      if (array26[a52] > mxx) mxx = array26[a52];
      if (array27[a52] < mny) mny = array27[a52];
      if (array27[a52] > mxy) mxy = array27[a52];
    }
    int64_t rangex = (int64_t)mxx - mnx, rangey = (int64_t)mxy - mny;
    if (p->n < 1) {
      // no vertex pairs: the source's loop leaves both at 0
    } else if (rangex <= INT32_MAX && rangey <= INT32_MAX) {
      abs3 = (int32_t)rangex;
      abs4 = (int32_t)rangey;
    } else {
      for (int32_t a52 = 0; a52 < p->n; a52++) {
        for (int32_t a53 = a52; a53 < p->n; a53++) {
          if (a52 != a53) {
            if (abs(array26[a52] - array26[a53]) > abs3) abs3 = abs(array26[a52] - array26[a53]);
            if (abs(array27[a52] - array27[a53]) > abs4) abs4 = abs(array27[a52] - array27[a53]);
          }
        }
      }
    }
    if (abs3 == 0 || abs4 == 0) {
      n45 = 0;
    } else if (abs3 < 3 && abs4 < 3 && ((n8 / abs3 > 15 && n8 / abs4 > 15) || b) &&
               (!p->m->lightson || p->light == 0)) {
      n45 = 0;
    }
  }
  if (n45 != 0) {
    int32_t lastmaf = 1;
    int32_t gr = p->gr;
    if (gr < 0 && gr >= -15) gr = 0;
    if (p->gr == -11) gr = -90;
    if (p->gr == -12) gr = -75;
    if (p->gr == -14 || p->gr == -15) gr = -50;
    if (p->glass == 2) gr = 200;
    if (p->fs != 0) {
      int32_t n54, n55;
      if (abs(array27[0] - array27[1]) > abs(array27[2] - array27[1])) {
        n54 = 0; n55 = 2;
      } else {
        n54 = 2; n55 = 0;
        lastmaf *= -1;
      }
      if (array27[1] > array27[n54]) lastmaf *= -1;
      if (array26[1] > array26[n55]) lastmaf *= -1;
      if (p->fs != 22) {
        lastmaf *= p->fs;
        if (lastmaf == -1) {
          gr += 40;
          lastmaf = -111;
        }
      }
    }
    if (p->m->lightson && p->light == 2) gr -= 40;
    int32_t n56 = array3[0], n57 = array3[0];
    int32_t n58 = array[0], n59 = array[0];
    int32_t n60 = array2[0], n61 = array2[0];
    for (int32_t a62 = 0; a62 < p->n; a62++) {
      if (array3[a62] > n56) n56 = array3[a62];
      if (array3[a62] < n57) n57 = array3[a62];
      if (array[a62] > n58) n58 = array[a62];
      if (array[a62] < n59) n59 = array[a62];
      if (array2[a62] > n60) n60 = array2[a62];
      if (array2[a62] < n61) n61 = array2[a62];
    }
    int32_t n63 = (n56 + n57) / 2;
    int32_t n64 = (n58 + n59) / 2;
    int32_t n65 = (n60 + n61) / 2;
    // trunc(Math.sqrt(imul+imul+imul+imul)) -- no fr(), genuinely double.
    // Each Math.imul is its own int32-wrapping multiply; the final one
    // wraps gr*gr*gr as TWO chained int32 multiplies (imul(imul(gr,gr),gr)).
    // Summed as plain int32_t (wraps under -fwrapv, matching Java's iadd).
    int32_t d1 = p->m->cy - n63, d2 = p->m->cx - n64;
    int32_t sumsq32 = (d1 * d1) + (d2 * d2) + (n65 * n65) + ((gr * gr) * gr);
    p->av = jtrunc_d(sqrt((double)sumsq32));
    if (p->m->trk == 0 && (p->av > p->m->fade[p->disline] || p->av == 0)) n45 = 0;
    if (lastmaf == -111 && p->av > 4500 && !p->road) n45 = 0;
    if (lastmaf == -111 && p->av > 1500) b = true;
    if (p->av > 3000 && p->m->adv <= 900) b = true;
    if (p->fs == 22 && p->av < 11200) p->m->lastmaf = lastmaf;
    if (p->gr == -13 && (!p->m->lastcheck || n8 != -1)) n45 = 0;
    if (p->master == 2 && p->av > 1500 && !p->m->crs) n45 = 0;
    if ((p->gr == -14 || p->gr == -15 || p->gr == -12) &&
        (p->av > 11000 || b4 || lastmaf == -111 || p->m->resdown == 2) &&
        p->m->trk != 2 && p->m->trk != 3) n45 = 0;
    if (p->gr == -11 && p->av > 11000 && p->m->trk != 2 && p->m->trk != 3) n45 = 0;
    if (p->glass == 2 && (p->m->trk != 0 || p->av > 6700)) n45 = 0;
    if (p->flx != 0 && medium_random(p->m) > 0.3f && p->flx != 77) n45 = 0;
  }

  if (n45 != 0) {
    // fr(projf/deltaf + 0.3) -- TWO ops (divide, then add) under ONE fr():
    // case 2, needs double.
    float n66 = (float)((double)p->projf / (double)p->deltaf + 0.3);
    if (b && !p->solo) {
      bool b5 = false;
      if (n66 > 1.0f) {
        if (n66 >= 1.27f) b5 = true;
        n66 = 1.0f;
      }
      if (b5) n66 = n66 * 0.89f; else n66 = n66 * 0.86f;
      if (n66 < 0.37f) n66 = 0.37f;
      if (p->gr == -9) n66 = 0.7f;
      if (p->gr == -4) n66 = 0.74f;
      if (p->gr != -7 && p->m->trk == 0 && b4) n66 = 0.32f;
      if (p->gr == -8 || p->gr == -14 || p->gr == -15) n66 = 1.0f;
      if (p->gr == -11 || p->gr == -12) {
        n66 = 0.6f;
        if (n8 == -1) {
          if (p->m->cpflik || (p->m->nochekflk && !p->m->lastcheck)) n66 = 1.0f;
          else n66 = 0.76f;
        }
      }
      if (p->gr == -13 && n8 == -1) {
        if (p->m->cpflik) n66 = 0.0f;
        else n66 = 0.76f;
      }
      if (p->gr == -6) n66 = 0.62f;
      if (p->gr == -5) n66 = 0.55f;
    } else {
      if (n66 > 1.0f) n66 = 1.0f;
      if (n66 < 0.6f || b4) n66 = 0.6f;
    }
    // n66 = fr(n66) -- already a float, no-op in C.
    int32_t rgb = hsb_to_rgb(p->hsb[0], p->hsb[1], p->hsb[2] * n66); // fr(hsb2*n66), single op
    if (p->m->trk == 1) {
      float hsbvals[3];
      rgb_to_hsb(p->oc[0], p->oc[1], p->oc[2], hsbvals);
      hsbvals[0] = 0.15f;
      hsbvals[1] = 0.3f;
      rgb = hsb_to_rgb(hsbvals[0], hsbvals[1], (hsbvals[2] * n66) + 0.0f);
    }
    if (p->m->trk == 3) {
      float hsbvals2[3];
      rgb_to_hsb(p->oc[0], p->oc[1], p->oc[2], hsbvals2);
      hsbvals2[0] = 0.6f;
      hsbvals2[1] = 0.14f;
      rgb = hsb_to_rgb(hsbvals2[0], hsbvals2[1], (hsbvals2[2] * n66) + 0.0f);
    }
    int32_t red = (rgb >> 16) & 255;
    int32_t green = (rgb >> 8) & 255;
    int32_t blue = rgb & 255;
    if (p->m->lightson && (p->light != 0 || ((p->gr == -11 || p->gr == -12) && n8 == -1))) {
      red = p->oc[0]; if (red > 255) red = 255; if (red < 0) red = 0;
      green = p->oc[1]; if (green > 255) green = 255; if (green < 0) green = 0;
      blue = p->oc[2]; if (blue > 255) blue = 255; if (blue < 0) blue = 0;
    }
    if (p->m->trk == 0) {
      for (int32_t a67 = 0; a67 < 16; a67++) {
        if (p->av > p->m->fade[a67]) {
          red = (red * p->m->fogd + p->m->cfade[0]) / (p->m->fogd + 1);
          green = (green * p->m->fogd + p->m->cfade[1]) / (p->m->fogd + 1);
          blue = (blue * p->m->fogd + p->m->cfade[2]) / (p->m->fogd + 1);
        }
      }
    }
    gfx_set_color(g, red, green, blue);
    gfx_fill_polygon(g, array26, array27, p->n);
    if (p->m->trk != 0 && p->gr == -10) b = false;
    if (!b) {
      if (p->flx == 0) {
        if (!p->solo) {
          int32_t r3 = 0, g3 = 0, b6 = 0;
          if (p->m->lightson && p->light != 0) {
            r3 = p->oc[0] / 2; if (r3 > 255) r3 = 255; if (r3 < 0) r3 = 0;
            g3 = p->oc[1] / 2; if (g3 > 255) g3 = 255; if (g3 < 0) g3 = 0;
            b6 = p->oc[2] / 2; if (b6 > 255) b6 = 255; if (b6 < 0) b6 = 0;
          }
          gfx_set_rendering_hint(g); // Madness.anti === 1, always true in this port
          gfx_set_color(g, r3, g3, b6);
          gfx_draw_polygon(g, array26, array27, p->n);
          gfx_set_rendering_hint(g);
        }
      } else {
        if (p->flx == 2) {
          gfx_set_color(g, 0, 0, 0);
          gfx_draw_polygon(g, array26, array27, p->n);
        }
        if (p->flx == 1) {
          int32_t r4 = 0;
          float sn1 = (float)p->m->snap[1] / 100.0f;
          int32_t g4 = jtrunc(223.0f + (223.0f * sn1)); if (g4 > 255) g4 = 255; if (g4 < 0) g4 = 0;
          float sn2 = (float)p->m->snap[2] / 100.0f;
          int32_t b7 = jtrunc(255.0f + (255.0f * sn2)); if (b7 > 255) b7 = 255; if (b7 < 0) b7 = 0;
          gfx_set_color(g, r4, g4, b7);
          gfx_draw_polygon(g, array26, array27, p->n);
          p->flx = 2;
        }
        if (p->flx == 3) {
          int32_t r5 = 0;
          float sn1b = (float)p->m->snap[1] / 100.0f;
          int32_t g5 = jtrunc(255.0f + (255.0f * sn1b)); if (g5 > 255) g5 = 255; if (g5 < 0) g5 = 0;
          float sn2b = (float)p->m->snap[2] / 100.0f;
          int32_t b8 = jtrunc(223.0f + (223.0f * sn2b)); if (b8 > 255) b8 = 255; if (b8 < 0) b8 = 0;
          gfx_set_color(g, r5, g5, b8);
          gfx_draw_polygon(g, array26, array27, p->n);
          p->flx = 2;
        }
        if (p->flx == 77) {
          gfx_set_color(g, 16, 198, 255);
          gfx_draw_polygon(g, array26, array27, p->n);
          p->flx = 0;
        }
      }
    } else if (p->road && p->av <= 3000 && p->m->trk == 0 && p->m->fade[0] > 4000) {
      red -= 10; if (red < 0) red = 0;
      green -= 10; if (green < 0) green = 0;
      blue -= 10; if (blue < 0) blue = 0;
      gfx_set_color(g, red, green, blue);
      gfx_draw_polygon(g, array26, array27, p->n);
    }
    if (p->gr == -10) {
      if (p->m->trk == 0) {
        int32_t r6 = p->c[0], g6 = p->c[1], b9 = p->c[2];
        if (n8 == -1 && p->m->cpflik) {
          // Procyon renders these as `r6 *= (int)1.6` (a no-op, ×1). The
          // bytecode is a §2 Case A compound assignment: r6 = (int)(r6*1.6),
          // a real 60% brighten. No fr() in the JS -- genuinely double.
          r6 = jtrunc_d((double)r6 * 1.6); if (r6 > 255) r6 = 255;
          g6 = jtrunc_d((double)g6 * 1.6); if (g6 > 255) g6 = 255;
          b9 = jtrunc_d((double)b9 * 1.6); if (b9 > 255) b9 = 255;
        }
        for (int32_t a68 = 0; a68 < 16; a68++) {
          if (p->av > p->m->fade[a68]) {
            r6 = (r6 * p->m->fogd + p->m->cfade[0]) / (p->m->fogd + 1);
            g6 = (g6 * p->m->fogd + p->m->cfade[1]) / (p->m->fogd + 1);
            b9 = (b9 * p->m->fogd + p->m->cfade[2]) / (p->m->fogd + 1);
          }
        }
        gfx_set_color(g, r6, g6, b9);
        gfx_draw_polygon(g, array26, array27, p->n);
      } else if (p->m->cpflik && p->m->hit == 5000) {
        // trunc(random()*115.0) -- module-level random(), no fr(): double.
        int32_t g7 = jtrunc_d(nfm_random() * 115.0);
        int32_t r7 = g7 * 2 - 54; if (r7 < 0) r7 = 0; if (r7 > 255) r7 = 255;
        int32_t b10 = 202 + g7 * 2; if (b10 < 0) b10 = 0; if (b10 > 255) b10 = 255;
        g7 += 101; if (g7 < 0) g7 = 0; if (g7 > 255) g7 = 255;
        gfx_set_color(g, r7, g7, b10);
        gfx_draw_polygon(g, array26, array27, p->n);
      }
    }
    if (p->gr == -18 && p->m->trk == 0) {
      int32_t r8 = p->c[0], g8 = p->c[1], b11 = p->c[2];
      if (p->m->cpflik && p->m->elecr >= 0.0f) {
        r8 = jtrunc(25.5f * p->m->elecr); if (r8 > 255) r8 = 255;
        g8 = jtrunc(128.0f + (12.8f * p->m->elecr)); if (g8 > 255) g8 = 255;
        b11 = 255;
      }
      for (int32_t a69 = 0; a69 < 16; a69++) {
        if (p->av > p->m->fade[a69]) {
          r8 = (r8 * p->m->fogd + p->m->cfade[0]) / (p->m->fogd + 1);
          g8 = (g8 * p->m->fogd + p->m->cfade[1]) / (p->m->fogd + 1);
          b11 = (b11 * p->m->fogd + p->m->cfade[2]) / (p->m->fogd + 1);
        }
      }
      gfx_set_color(g, r8, g8, b11);
      gfx_draw_polygon(g, array26, array27, p->n);
    }
  }
}

void plane_s(Plane *p, Graphics2D *g, int32_t n, int32_t n2, int32_t n3, int32_t n4,
             int32_t n5, int32_t n6, int32_t n7) {
  if (p->n > PLANE_MAX_N) return; // unreachable for parsed models, see plane.h
  int32_t array[PLANE_MAX_N];
  int32_t array2[PLANE_MAX_N];
  int32_t array3[PLANE_MAX_N];
  for (int32_t i = 0; i < p->n; i++) {
    array[i] = p->ox[i] + n;
    array3[i] = p->oy[i] + n2;
    array2[i] = p->oz[i] + n3;
  }
  plane_rot(p, array, array3, n, n2, n5, p->n);
  plane_rot(p, array3, array2, n2, n3, n6, p->n);
  plane_rot(p, array, array2, n, n3, n4, p->n);

  // trunc(fr(fr(crgrnd[i])/1.5)) -- fr(x) on a plain int is a no-op value-
  // wise (int is exactly representable as float32 here), then fr(that/1.5)
  // is a single-op divide: native float chaining is fine.
  int32_t r = jtrunc((float)p->m->crgrnd[0] / 1.5f);
  int32_t rg = jtrunc((float)p->m->crgrnd[1] / 1.5f);
  int32_t rb = jtrunc((float)p->m->crgrnd[2] / 1.5f);
  for (int32_t j = 0; j < p->n; j++) array3[j] = p->m->ground;

  if (n7 == 0) {
    int32_t n8x = 0, n9z = 0, n10x = 0, n11z = 0;
    for (int32_t k = 0; k < p->n; k++) {
      int32_t c12 = 0, c13 = 0, c14 = 0, c15 = 0;
      for (int32_t l = 0; l < p->n; l++) {
        if (array[k] >= array[l]) c12++;
        if (array[k] <= array[l]) c13++;
        if (array2[k] >= array2[l]) c14++;
        if (array2[k] <= array2[l]) c15++;
      }
      if (c12 == p->n) n8x = array[k];
      if (c13 == p->n) n9z = array[k];
      if (c14 == p->n) n10x = array2[k];
      if (c15 == p->n) n11z = array2[k];
    }
    int32_t n16 = (n8x + n9z) / 2;
    int32_t n17 = (n10x + n11z) / 2;
    int32_t ncx = (n16 - p->t->sx + p->m->x) / 3000;
    if (ncx > p->t->ncx) ncx = p->t->ncx;
    if (ncx < 0) ncx = 0;
    int32_t ncz = (n17 - p->t->sz + p->m->z) / 3000;
    if (ncz > p->t->ncz) ncz = p->t->ncz;
    if (ncz < 0) ncz = 0;
    for (int32_t n18i = p->t->sect_len[ncx][ncz] - 1; n18i >= 0; n18i--) {
      int32_t n19 = p->t->sect[ncx][ncz][n18i];
      int32_t n20 = 0;
      if (abs(p->t->zy[n19]) != 90 && abs(p->t->xy[n19]) != 90 && p->t->rady[n19] != 801 &&
          abs(n16 - (p->t->x[n19] - p->m->x)) < p->t->radx[n19] &&
          abs(n17 - (p->t->z[n19] - p->m->z)) < p->t->radz[n19] &&
          (!p->t->decor[n19] || p->m->resdown != 2)) {
        n20++;
      }
      if (n20 != 0) {
        for (int32_t n21 = 0; n21 < p->n; n21++) {
          array3[n21] = p->t->y[n19] - p->m->y;
          if (p->t->zy[n19] != 0) {
            // trunc(a3 + A*sin/sin - B*sin/sin) -- no fr() anywhere in this
            // whole statement: genuinely double throughout.
            float sin_zy = medium_sin(p->m, (float)p->t->zy[n19]);
            float sin_90mzy = medium_sin(p->m, (float)(90 - p->t->zy[n19]));
            double term1 = (double)(array2[n21] - (p->t->z[n19] - p->m->z - p->t->radz[n19])) *
                            (double)sin_zy / (double)sin_90mzy;
            double term2 = (double)p->t->radz[n19] * (double)sin_zy / (double)sin_90mzy;
            array3[n21] = jtrunc_d((double)array3[n21] + term1 - term2);
          }
          if (p->t->xy[n19] != 0) {
            float sin_xy = medium_sin(p->m, (float)p->t->xy[n19]);
            float sin_90mxy = medium_sin(p->m, (float)(90 - p->t->xy[n19]));
            double term1 = (double)(array[n21] - (p->t->x[n19] - p->m->x - p->t->radx[n19])) *
                            (double)sin_xy / (double)sin_90mxy;
            double term2 = (double)p->t->radx[n19] * (double)sin_xy / (double)sin_90mxy;
            array3[n21] = jtrunc_d((double)array3[n21] + term1 - term2);
          }
        }
        r = jtrunc((float)p->t->c[n19][0] / 1.5f);
        rg = jtrunc((float)p->t->c[n19][1] / 1.5f);
        rb = jtrunc((float)p->t->c[n19][2] / 1.5f);
        break;
      }
    }
  }

  int32_t n24 = 1;
  int32_t array6[PLANE_MAX_N];
  int32_t array7[PLANE_MAX_N];
  if (n7 == 2) {
    r = 87; rg = 85; rb = 57;
  } else {
    for (int32_t n25 = 0; n25 < p->m->nsp; n25++) {
      for (int32_t n26 = 0; n26 < p->n; n26++) {
        if (abs(array[n26] - p->m->spx[n25]) < p->m->sprad[n25] &&
            abs(array2[n26] - p->m->spz[n25]) < p->m->sprad[n25]) {
          n24 = 0;
        }
      }
    }
  }
  if (n24 != 0) {
    plane_rot(p, array, array2, p->m->cx, p->m->cz, p->m->xz + p->m->fxz, p->n);
    plane_rot(p, array3, array2, p->m->cy, p->m->cz, p->m->zy + p->m->fzy, p->n);
    int32_t n27 = 0, n28 = 0, n29 = 0, n30 = 0;
    for (int32_t n31 = 0; n31 < p->n; n31++) {
      array6[n31] = plane_xs(p, array[n31], array2[n31]);
      array7[n31] = plane_ys(p, array3[n31], array2[n31]);
      if (array7[n31] < p->m->ih || array2[n31] < 10) n27++;
      if (array7[n31] > p->m->h || array2[n31] < 10) n28++;
      if (array6[n31] < p->m->iw || array2[n31] < 10) n29++;
      if (array6[n31] > p->m->w || array2[n31] < 10) n30++;
    }
    if (n29 == p->n || n27 == p->n || n28 == p->n || n30 == p->n) n24 = 0;
  }
  if (n24 != 0) {
    for (int32_t n32 = 0; n32 < 16; n32++) {
      if (p->av > p->m->fade[n32]) {
        r = (r * p->m->fogd + p->m->cfade[0]) / (p->m->fogd + 1);
        rg = (rg * p->m->fogd + p->m->cfade[1]) / (p->m->fogd + 1);
        rb = (rb * p->m->fogd + p->m->cfade[2]) / (p->m->fogd + 1);
      }
    }
    gfx_set_color(g, r, rg, rb);
    gfx_fill_polygon(g, array6, array7, p->n);
  }
}
