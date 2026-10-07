// ports web/ContO.js's #initBuf only -- see cont_o.h for full scope.
//
// Same fr()-translation rules as native/core/medium.c/plane.c/wheels.c.
#include "cont_o.h"
#include "java_compat.h"
#include "wheels.h"
#include "vfs.h"
#include "gfx.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>
#include <stdio.h>

bool cont_o_shadows = true;
bool cont_o_particles = true;

static bool starts_with(const char *s, const char *prefix) {
  return strncmp(s, prefix, strlen(prefix)) == 0;
}

/** Matches JS String.trim(): strips ASCII whitespace from both ends. */
static char *trim_line(char *s) {
  while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r' || *s == '\f' || *s == '\v') s++;
  size_t len = strlen(s);
  while (len > 0) {
    char c = s[len - 1];
    if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') len--;
    else break;
  }
  s[len] = '\0';
  return s;
}

/**
 * ports web/ContO.js's getvalue(s, s2, n): extracts the Nth comma/paren-
 * separated field after "s(" in s2, parses it as a float, truncates.
 * Transliterated index-for-index from the JS `for` loop, INCLUDING its
 * double-increment-on-separator quirk (the body does `++i` on a separator
 * AND the for-loop's own `++i` still fires) -- see the commit message for
 * a worked trace of why this is not a bug to "clean up".
 */
static int32_t cont_o_getvalue(const char *cmd, const char *line, int32_t n) {
  size_t cmd_len = strlen(cmd);
  size_t line_len = strlen(line);
  int32_t n2 = 0;
  char buf[256];
  size_t buf_len = 0;
  for (size_t i = cmd_len + 1; i < line_len; i++) {
    char c1 = line[i];
    if (c1 == ',' || c1 == ')') {
      n2++;
      i++;
    }
    if (n2 == n) {
      if (i < line_len && buf_len < sizeof(buf) - 1) buf[buf_len++] = line[i];
    }
  }
  buf[buf_len] = '\0';
  // JS: trunc(parseFloat(string)) -- parseFloat returns NaN on no valid
  // prefix, and trunc(NaN) is 0; strtod returns 0.0 with no conversion in
  // the same case, so the truncated result matches either way.
  double val = strtod(buf, NULL);
  return jtrunc_d(val);
}

void cont_o_free(ContO *co) {
  for (int32_t i = 0; i < co->npl; i++) plane_free(&co->p[i]);
  free(co->p);
  free(co->txy); free(co->tzy); free(co->tc);
  free(co->tradx); free(co->tradz); free(co->trady);
  free(co->tx); free(co->ty); free(co->tz);
  free(co->skd); free(co->dam); free(co->notwall);
  free(co->stg); free(co->rtg);
  free(co->sx); free(co->sy); free(co->sz); free(co->scx); free(co->scz);
  free(co->osmag); free(co->sav); free(co->smag); free(co->srgb); free(co->sbln);
  free(co->rbef); free(co->rx); free(co->ry); free(co->rz);
  free(co->vrx); free(co->vry); free(co->vrz);
  memset(co, 0, sizeof(*co));
}

void cont_o_recopy(ContO *dst, ContO *src, int32_t x, int32_t y, int32_t z, int32_t a) {
  cont_o_free(dst);
  cont_o_init_copy(dst, src, x, y, z, a);
}

// Ports ContO.java's procedural constructor (ContO.java:1072-1300), written
// from the Java rather than web/ContO.js where the two round differently:
// `2.2f + n20` and `Math.abs(n16 - n20)` are float in Java.
void cont_o_init_pile(ContO *co, int32_t seed, int32_t n2, int32_t n3, Medium *m, Trackers *t,
                      int32_t x, int32_t z, int32_t y) {
  memset(co, 0, sizeof(*co));
  co->m = m;
  co->t = t;
  co->baseIndex = -1;
  co->x = x;
  co->z = z;
  co->y = y;
  co->disline = 4;
  co->noline = true;
  co->grounded = 115.0f;
  co->decor = true;
  co->npl = 5;
  co->p = calloc(5, sizeof(Plane));
  JavaRandom random;
  jrandom_init(&random, seed);
  int32_t a1[8] = {0}, a2[8] = {0}, a3[8] = {0}, a4[8] = {0}, a5[8] = {0};
  float n4 = (float)n2, n5 = (float)n3;
  if (n5 < 2.0f) n5 = 2.0f;
  if (n5 > 6.0f) n5 = 6.0f;
  if (n4 < 2.0f) n4 = 2.0f;
  if (n4 > 6.0f) n4 = 6.0f;
  const float n6 = n4 / 1.5f;
  const float n7 = n5 / 1.5f * (1.0f + (n6 - 2.0f) * 0.1786f);
  const float n8 = (float)(50.0 + 100.0 * jrandom_next_double(&random));
  a1[0] = -jtrunc(n8 * n6 * 0.7071f);
  a2[0] = jtrunc(n8 * n6 * 0.7071f);
  const float n9 = (float)(50.0 + 100.0 * jrandom_next_double(&random));
  a2[1] = jtrunc(n9 * n6);
  const float n10 = (float)(50.0 + 100.0 * jrandom_next_double(&random));
  a1[2] = jtrunc_d((double)(n10 * n6) * 0.7071);
  a2[2] = jtrunc_d((double)(n10 * n6) * 0.7071);
  a1[3] = jtrunc((float)(50.0 + 100.0 * jrandom_next_double(&random)) * n6);
  const float n11 = (float)(50.0 + 100.0 * jrandom_next_double(&random));
  a1[4] = jtrunc_d((double)(n11 * n6) * 0.7071);
  a2[4] = -jtrunc_d((double)(n11 * n6) * 0.7071);
  const float n12 = (float)(50.0 + 100.0 * jrandom_next_double(&random));
  a2[5] = -jtrunc(n12 * n6);
  const float n13 = (float)(50.0 + 100.0 * jrandom_next_double(&random));
  a1[6] = -jtrunc_d((double)(n13 * n6) * 0.7071);
  a2[6] = -jtrunc_d((double)(n13 * n6) * 0.7071);
  a1[7] = -jtrunc((float)(50.0 + 100.0 * jrandom_next_double(&random)) * n6);
  for (int32_t i = 0; i < 8; i++) {
    a3[i] = jtrunc_d(a1[i] * (0.2 + 0.4 * jrandom_next_double(&random)));
    a4[i] = jtrunc_d(a2[i] * (0.2 + 0.4 * jrandom_next_double(&random)));
    a5[i] = -jtrunc_d((10.0 + 15.0 * jrandom_next_double(&random)) * n7);
  }
  co->maxR = 0;
  for (int32_t j = 0; j < 8; j++) {
    const int32_t p = j == 0 ? 7 : j - 1, q = j == 7 ? 0 : j + 1;
    a1[j] = ((a1[p] + a1[q]) / 2 + a1[j]) / 2;
    a2[j] = ((a2[p] + a2[q]) / 2 + a2[j]) / 2;
    a3[j] = ((a3[p] + a3[q]) / 2 + a3[j]) / 2;
    a4[j] = ((a4[p] + a4[q]) / 2 + a4[j]) / 2;
    a5[j] = ((a5[p] + a5[q]) / 2 + a5[j]) / 2;
    const int32_t r1 = jtrunc_d(sqrt((double)(a1[j] * a1[j] + a2[j] * a2[j])));
    if (r1 > co->maxR) co->maxR = r1;
    const int32_t r2 = jtrunc_d(sqrt((double)(a3[j] * a3[j] + a5[j] * a5[j] + a4[j] * a4[j])));
    if (r2 > co->maxR) co->maxR = r2;
  }
  co->disp = co->maxR / 17;
  int32_t col[3];
  float n16 = -1.0f;
  float n17 = (n6 / n7 - 0.33f) / 33.4f;
  if (n17 < 0.005) n17 = 0.0f;
  if (n17 > 0.057) n17 = 0.057f;
  // The four slopes, each a hexagon from two outer points to the inner ring.
  for (int32_t k = 0; k < 4; k++) {
    const int32_t n18 = k * 2, n19 = (n18 + 2) % 8;
    int32_t ox[6] = {a1[n18], a1[n18 + 1], a1[n19], a3[n19], a3[n18 + 1], a3[n18]};
    int32_t oz[6] = {a2[n18], a2[n18 + 1], a2[n19], a4[n19], a4[n18 + 1], a4[n18]};
    int32_t oy[6] = {0, 0, 0, a5[n19], a5[n18 + 1], a5[n18]};
    // Re-rolled until the slope's shade differs from the last one's. Java's
    // own Random, not the replayed draw randoms, so it always ends.
    float n20;
    do {
      n20 = (float)((0.17 - n17) * jrandom_next_double(&random));
    } while (fabsf(n16 - n20) < 0.03 - n17 * 0.176f);
    n16 = n20;
    for (int32_t l = 0; l < 3; l++) {
      col[l] = m->trk == 2 ? jtrunc(390.0f / (2.2f + n20 - n17))
                           : jtrunc((float)(m->cpol[l] + m->cgrnd[l]) / (2.2f + n20 - n17));
    }
    plane_init(&co->p[k], m, t, ox, oz, oy, 6, col, 3, -8, 0, 0, 0, 0, co->disline, 0, true, 0, false);
  }
  const float n21 = (float)(0.02 * jrandom_next_double(&random));
  for (int32_t l = 0; l < 3; l++) {
    col[l] = m->trk == 2 ? jtrunc(390.0f / (2.15f + n21))
                         : jtrunc((float)(m->cpol[l] + m->cgrnd[l]) / (2.15f + n21));
  }
  plane_init(&co->p[4], m, t, a3, a4, a5, 8, col, 3, -8, 0, 0, 0, 0, co->disline, 0, true, 0, false);

  // Collision: four ramps up the slopes and a flat top. Full Trackers: none
  // added, and game_sparker_loadstage rejects the stage.
  if (t->nt + 5 > TRACKERS_MAX) {
    t->nt = TRACKERS_MAX;
    return;
  }
  int32_t ex[2] = {0, 0}, ez[2] = {0, 0};
  for (int32_t n23 = 0; n23 < 4; n23++) {
    const int32_t n24 = n23 * 2 + 1, nt = t->nt, nx = (n23 * 2 + 2) % 8;
    t->y[nt] = a5[n24] / 2;
    t->rady[nt] = abs(a5[n24] / 2);
    if (n23 == 0 || n23 == 2) {
      t->z[nt] = (a2[n24] + a4[n24]) / 2;
      t->radz[nt] = abs(t->z[nt] - a2[n24]);
      t->x[nt] = (a1[n23 * 2] + a1[nx]) / 2;
      t->radx[nt] = abs(t->x[nt] - a1[n23 * 2]);
    } else {
      t->x[nt] = (a1[n24] + a3[n24]) / 2;
      t->radx[nt] = abs(t->x[nt] - a1[n24]);
      t->z[nt] = (a2[n23 * 2] + a2[nx]) / 2;
      t->radz[nt] = abs(t->z[nt] - a2[n23 * 2]);
    }
    const double deg = 0.017453292519943295;
    if (n23 == 0) {
      ez[0] = t->z[nt] - t->radz[nt];
      t->zy[nt] = jtrunc_d(atan(t->rady[nt] / (double)t->radz[nt]) / deg);
      if (t->zy[nt] > 40) t->zy[nt] = 40;
      t->xy[nt] = 0;
    }
    if (n23 == 1) {
      ex[0] = t->x[nt] - t->radx[nt];
      t->xy[nt] = jtrunc_d(atan(t->rady[nt] / (double)t->radx[nt]) / deg);
      if (t->xy[nt] > 40) t->xy[nt] = 40;
      t->zy[nt] = 0;
    }
    if (n23 == 2) {
      ez[1] = t->z[nt] + t->radz[nt];
      t->zy[nt] = -jtrunc_d(atan(t->rady[nt] / (double)t->radz[nt]) / deg);
      if (t->zy[nt] < -40) t->zy[nt] = -40;
      t->xy[nt] = 0;
    }
    if (n23 == 3) {
      ex[1] = t->x[nt] + t->radx[nt];
      t->xy[nt] = -jtrunc_d(atan(t->rady[nt] / (double)t->radx[nt]) / deg);
      if (t->xy[nt] < -40) t->xy[nt] = -40;
      t->zy[nt] = 0;
    }
    t->x[nt] += co->x;
    t->z[nt] += co->z;
    t->y[nt] += co->y;
    for (int32_t l = 0; l < 3; l++) t->c[nt][l] = co->p[n23].oc[l];
    t->skd[nt] = 2;
    t->dam[nt] = 1;
    t->notwall[nt] = false;
    t->decor[nt] = true;
    t->rady[nt] += 10;
    t->nt++;
  }
  const int32_t nt = t->nt;
  t->y[nt] = 0;
  for (int32_t i = 0; i < 8; i++) t->y[nt] += a5[i];
  t->y[nt] = t->y[nt] / 8 + co->y;
  t->rady[nt] = 200;
  t->radx[nt] = ex[0] - ex[1];
  t->radz[nt] = ez[0] - ez[1];
  t->x[nt] = (ex[0] + ex[1]) / 2 + co->x;
  t->z[nt] = (ez[0] + ez[1]) / 2 + co->z;
  t->zy[nt] = 0;
  t->xy[nt] = 0;
  for (int32_t l = 0; l < 3; l++) t->c[nt][l] = co->p[4].oc[l];
  t->skd[nt] = 4;
  t->dam[nt] = 1;
  t->notwall[nt] = false;
  t->decor[nt] = true;
  t->nt++;
}

void cont_o_init_copy(ContO *dst, ContO *src, int32_t x, int32_t y, int32_t z, int32_t a) {
  memset(dst, 0, sizeof(*dst));
  dst->m = src->m;
  dst->t = src->t;
  dst->baseIndex = src->baseIndex;
  dst->npl = src->npl;
  dst->maxR = src->maxR;
  dst->disp = src->disp;
  dst->disline = src->disline;
  dst->noline = src->noline;
  dst->shadow = src->shadow;
  dst->grounded = src->grounded;
  dst->decor = src->decor;
  if (dst->m->loadnew && (a == 90 || a == -90)) {
    dst->grounded = dst->grounded + 10000.0f; // fr(grounded+10000), single op, native float exact
  }
  dst->grat = src->grat;
  dst->sprkat = src->sprkat;

  dst->p = malloc((size_t)dst->npl * sizeof(Plane));
  for (int32_t i = 0; i < dst->npl; i++) {
    if (src->p[i].master == 1) {
      // Dead in practice -- see cont_o.h's doc comment on this function.
      // If it ever DID fire, src->p[i].ox/oz/oy are allocated to src->p[i]'s
      // ORIGINAL (smaller) n, not 20, so plane_init below would read past
      // the end of them. Ported literally anyway, matching the JS.
      src->p[i].n = 20;
    }
    plane_init(&dst->p[i], dst->m, dst->t, src->p[i].ox, src->p[i].oz, src->p[i].oy,
               src->p[i].n, src->p[i].oc, src->p[i].glass, src->p[i].gr, src->p[i].fs,
               src->p[i].wx, src->p[i].wy, src->p[i].wz, src->disline, src->p[i].bfase,
               src->p[i].road, src->p[i].light, src->p[i].solo);
  }

  dst->x = x;
  dst->y = y;
  dst->z = z;
  dst->xz = 0;
  dst->xy = 0;
  dst->zy = 0;
  for (int32_t j = 0; j < dst->npl; j++) {
    dst->p[j].colnum = src->p[j].colnum;
    dst->p[j].master = src->p[j].master;
    plane_rot(&dst->p[j], dst->p[j].ox, dst->p[j].oz, 0, 0, a, dst->p[j].n);
    plane_loadprojf(&dst->p[j]);
  }

  if (src->tnt != 0) {
    float cosA = medium_cos(dst->m, (float)a), sinA = medium_sin(dst->m, (float)a);
    int32_t absA = abs(a);
    if (absA == 180) absA = 0;
    float cosAbs = medium_cos(dst->m, (float)absA), sinAbs = medium_sin(dst->m, (float)absA);
    // Full Trackers: stop adding (Java's int[6700] threw here) and let
    // game_sparker_loadstage reject the stage.
    for (int32_t k = 0; k < src->tnt && dst->t->nt < TRACKERS_MAX; k++) {
      Trackers *t = dst->t;
      // t.xy/t.zy: trunc(fr(fr(A)-+fr(B))), no center term mixed in -- both
      // inner fr()s and the outer fr() wrap a single op each, case 1 all
      // the way, native float exact.
      float txyA = (float)src->txy[k] * cosA, txyB = (float)src->tzy[k] * sinA;
      t->xy[t->nt] = jtrunc(txyA - txyB);
      float tzyA = (float)src->tzy[k] * cosA, tzyB = (float)src->txy[k] * sinA;
      t->zy[t->nt] = jtrunc(tzyA + tzyB);

      for (int32_t l = 0; l < 3; l++) {
        // trunc(fr(tc + fr(tc*fr(snap/100)))), clamped [0,255] -- exactly
        // medium.js's own snapped() helper, reused rather than duplicated.
        t->c[t->nt][l] = medium_snapped(src->tc[k][l], dst->m->snap[l]);
      }

      // t.x/t.z: trunc(this.x + fr(fr(A)-+fr(B))) -- the inner fr(fr(A)-+
      // fr(B)) is case 1 (native float exact), but the OUTER trunc() wraps
      // an addition that is NOT itself inside another fr() -- this.x (an
      // int) is added to that float BEFORE truncating, so per case 3 this
      // must happen in double, not native float chaining (unlike t.xy/t.zy
      // above, which have no such outer addition mixed in).
      float txA = (float)src->tx[k] * cosA, txB = (float)src->tz[k] * sinA;
      t->x[t->nt] = jtrunc_d((double)dst->x + (double)(txA - txB));
      float tzA = (float)src->tz[k] * cosA, tzB = (float)src->tx[k] * sinA;
      t->z[t->nt] = jtrunc_d((double)dst->z + (double)(tzA + tzB));

      t->y[t->nt] = dst->y + src->ty[k]; // i32(y+ty), plain int32 add, -fwrapv wraps identically
      t->skd[t->nt] = src->skd[k];
      t->dam[t->nt] = src->dam[k];
      t->notwall[t->nt] = src->notwall[k];
      t->decor[t->nt] = dst->decor;

      // radx/radz: trunc(abs(fr(fr(A)+fr(B)))) -- case 1 (single addition
      // wrapped in fr()), abs() on an exact float is exact, plain trunc().
      float rxA = (float)src->tradx[k] * cosAbs, rxB = (float)src->tradz[k] * sinAbs;
      t->radx[t->nt] = jtrunc(fabsf(rxA + rxB));
      float rzA = (float)src->tradx[k] * sinAbs, rzB = (float)src->tradz[k] * cosAbs;
      t->radz[t->nt] = jtrunc(fabsf(rzA + rzB));
      t->rady[t->nt] = src->trady[k];

      t->nt++;
    }
  }

  for (int32_t n = 0; n < 4; n++) {
    dst->keyx[n] = src->keyx[n];
    dst->keyz[n] = src->keyz[n];
  }

  if (dst->shadow) {
    // intArray()/floatArray() zero-initialise in the JS; calloc matches
    // that directly, so the JS's own explicit stg[]/rtg[] zero loops
    // (redundant there too) aren't reproduced here.
    dst->stg = calloc(20, sizeof(int32_t));
    dst->sx = calloc(20, sizeof(int32_t));
    dst->sy = calloc(20, sizeof(int32_t));
    dst->sz = calloc(20, sizeof(int32_t));
    dst->scx = calloc(20, sizeof(int32_t));
    dst->scz = calloc(20, sizeof(int32_t));
    dst->osmag = calloc(20, sizeof(float));
    dst->sav = calloc(20, sizeof(int32_t));
    dst->smag = calloc(20, sizeof(float[8]));
    dst->srgb = calloc(20, sizeof(int32_t[3]));
    dst->sbln = calloc(20, sizeof(float));
    dst->ust = 0;
    dst->rtg = calloc(100, sizeof(int32_t));
    dst->rbef = calloc(100, sizeof(bool));
    dst->rx = calloc(100, sizeof(int32_t));
    dst->ry = calloc(100, sizeof(int32_t));
    dst->rz = calloc(100, sizeof(int32_t));
    dst->vrx = calloc(100, sizeof(float));
    dst->vry = calloc(100, sizeof(float));
    dst->vrz = calloc(100, sizeof(float));
  }
}

// --- Runtime draw path -- see cont_o.h for exact scope ---



// Ports `lowshadow(graphics2D, n)` (web/ContO.js:1345-1449) -- a flat,
// fog-faded shadow quad drawn under a car once it's far enough from the
// camera (n3 >= 2000 at the cont_o_d call site) that the real per-wheel
// shadow polygons aren't worth computing. Was originally stubbed to abort
// loudly on the theory that nothing exercised it with only one always-
// nearby camera-followed car; genuinely reachable now that AI opponents
// (native/core/bots.c/control.c) can sit far from the player's chase
// camera -- same "reasoning breaks down once it's for real" story as
// electrify/pdust/dsprk/fixit before it (all now fully ported too, see
// each one's own comment in this file for what tripped it up).
static void cont_o_lowshadow(ContO *co, struct Graphics2D *g, int32_t n) {
  Medium *m = co->m;
  Trackers *t = co->t;
  int32_t array[4], array2[4], array3[4];

  int32_t n2 = 1;
  int32_t i = co->zy < 0 ? -co->zy : co->zy;
  while (i > 270) i -= 360;
  if ((i < 0 ? -i : i) > 90) n2 = -1;

  // (int)(keyx[k]*1.2 + x - m.x) -- no fr() at all in the JS (genuinely
  // double-precision: keyx is int, 1.2 a double literal, no single-op fr()
  // wraps the sum), so this is jtrunc_d, not plain float arithmetic.
  double n2d = (double)n2;
  array[0] = jtrunc_d((double)co->keyx[0] * 1.2 + (double)co->x - (double)m->x);
  array3[0] = jtrunc_d((double)(co->keyz[0] + 30) * n2d * 1.2 + (double)co->z - (double)m->z);
  array[1] = jtrunc_d((double)co->keyx[1] * 1.2 + (double)co->x - (double)m->x);
  array3[1] = jtrunc_d((double)(co->keyz[1] + 30) * n2d * 1.2 + (double)co->z - (double)m->z);
  array[2] = jtrunc_d((double)co->keyx[3] * 1.2 + (double)co->x - (double)m->x);
  array3[2] = jtrunc_d((double)(co->keyz[3] - 30) * n2d * 1.2 + (double)co->z - (double)m->z);
  array[3] = jtrunc_d((double)co->keyx[2] * 1.2 + (double)co->x - (double)m->x);
  array3[3] = jtrunc_d((double)(co->keyz[2] - 30) * n2d * 1.2 + (double)co->z - (double)m->z);
  cont_o_rot(co, array, array3, co->x - m->x, co->z - m->z, co->xz, 4);

  // idiv(crgrnd[k], 1.5) -- genuinely double-precision truncating divide
  // (java.js's idiv is `(a/b)|0`), same as every other no-fr() trunc()
  // in this function. `gg` avoids shadowing this function's own `g`
  // (Graphics2D*) parameter.
  int32_t r = jtrunc_d((double)m->crgrnd[0] / 1.5);
  int32_t gg = jtrunc_d((double)m->crgrnd[1] / 1.5);
  int32_t b = jtrunc_d((double)m->crgrnd[2] / 1.5);
  for (int32_t j = 0; j < 4; j++) array2[j] = m->ground;

  if (t->ncx != 0 || t->ncz != 0) {
    int32_t ncx = (co->x - t->sx) / 3000;
    if (ncx > t->ncx) ncx = t->ncx;
    if (ncx < 0) ncx = 0;
    int32_t ncz = (co->z - t->sz) / 3000;
    if (ncz > t->ncz) ncz = t->ncz;
    if (ncz < 0) ncz = 0;
    for (int32_t k = t->sect_len[ncx][ncz] - 1; k >= 0; k--) {
      int32_t n3 = t->sect[ncx][ncz][k];
      int32_t n4 = 0;
      for (int32_t l = 0; l < 4; l++) {
        if (abs(t->zy[n3]) != 90 && abs(t->xy[n3]) != 90 && t->rady[n3] != 801 &&
            abs(array[l] - (t->x[n3] - m->x)) < t->radx[n3] &&
            abs(array3[l] - (t->z[n3] - m->z)) < t->radz[n3] &&
            (!t->decor[n3] || m->resdown != 2)) {
          n4++;
        }
      }
      if (n4 > 2) {
        for (int32_t n5 = 0; n5 < 4; n5++) {
          array2[n5] = t->y[n3] - m->y;
          if (t->zy[n3] != 0) {
            // fr(fr(A) - fr(B)), each of A/B a single mul-then-div chain
            // (case 1: plain C float arithmetic already rounds the same
            // way at each operator Java would), added onto the plain int
            // array2[n5] and trunc()'d as a whole -- matches the JS's own
            // `array2[n5] = trunc(array2[n5] + fr(fr(A) - fr(B)))` exactly
            // (not the decompiled Java's `+= (int)(...)`, which rounds at
            // a different point -- see this project's convention of
            // trusting the verified web/*.js transpile over decompiled
            // Java's approximate cast placement).
            float sinZy = medium_sin(m, (float)t->zy[n3]);
            float sin90mZy = medium_sin(m, (float)(90 - t->zy[n3]));
            float A = (float)(array3[n5] - (t->z[n3] - m->z - t->radz[n3])) * sinZy / sin90mZy;
            float B = (float)t->radz[n3] * sinZy / sin90mZy;
            array2[n5] = jtrunc((float)array2[n5] + (A - B));
          }
          if (t->xy[n3] != 0) {
            float sinXy = medium_sin(m, (float)t->xy[n3]);
            float sin90mXy = medium_sin(m, (float)(90 - t->xy[n3]));
            float A2 = (float)(array[n5] - (t->x[n3] - m->x - t->radx[n3])) * sinXy / sin90mXy;
            float B2 = (float)t->radx[n3] * sinXy / sin90mXy;
            array2[n5] = jtrunc((float)array2[n5] + (A2 - B2));
          }
        }
        r = jtrunc_d((double)t->c[n3][0] / 1.5);
        gg = jtrunc_d((double)t->c[n3][1] / 1.5);
        b = jtrunc_d((double)t->c[n3][2] / 1.5);
        break;
      }
    }
  }

  cont_o_rot(co, array, array3, m->cx, m->cz, m->xz + m->fxz, 4);
  cont_o_rot(co, array2, array3, m->cy, m->cz, m->zy + m->fzy, 4);
  bool ok = true;
  int32_t n8 = 0, n9 = 0, n10 = 0, n11 = 0;
  for (int32_t n12 = 0; n12 < 4; n12++) {
    array[n12] = cont_o_xs(co, array[n12], array3[n12]);
    array2[n12] = cont_o_ys(co, array2[n12], array3[n12]);
    if (array2[n12] < m->ih || array3[n12] < 10) n8++;
    if (array2[n12] > m->h || array3[n12] < 10) n9++;
    if (array[n12] < m->iw || array3[n12] < 10) n10++;
    if (array[n12] > m->w || array3[n12] < 10) n11++;
  }
  if (n10 == 4 || n8 == 4 || n9 == 4 || n11 == 4) ok = false;

  if (ok) {
    for (int32_t n13 = 0; n13 < 16; n13++) {
      if (n > m->fade[n13]) {
        r = (r * m->fogd + m->cfade[0]) / (m->fogd + 1);
        gg = (gg * m->fogd + m->cfade[1]) / (m->fogd + 1);
        b = (b * m->fogd + m->cfade[2]) / (m->fogd + 1);
      }
    }
    gfx_set_color(g, r, gg, b);
    gfx_fill_polygon(g, array, array2, 4);
  }
}


// Ports `#dust`, wrapped exactly as the JS's own public `dust()` wraps it
// -- see cont_o.h's doc comment for scope. The `setDrawPhase(true)` guard
// is load-bearing, not cosmetic: `#dust`'s one `this.m.random()` call
// must draw from the DRAW PRNG stream, not the SIM stream drive() itself
// runs on (see the JS's own comment on `dust()`: seeding a particle is a
// PRESENTATION detail, and letting it consume a sim-stream draw would
// shift every later simulation random by one whenever a puff spawns).
// This was missed in the initial port (the private #dust body was
// transcribed directly without its public wrapper) and caused exactly
// the symptom its absence predicts: a narrow, sustained one-draw-per-tick
// divergence from web/Mad.js once dust() starts firing on the sim stream
// (see native/tests/mad_test.c's drive_wall_scenario doc comment, where
// this was first caught).
void cont_o_dust(ContO *co, int32_t n, float n2, float n3, float n4, int32_t n5, int32_t n6, float n7, int32_t n8, bool b) {
  nfm_set_draw_phase(true);
  bool b2 = false;
  if (n8 > 5 && (n == 0 || n == 2)) b2 = true;
  if (n8 < -5 && (n == 1 || n == 3)) b2 = true;
  // n9 = fr(fr(sqrt(n5*n5+n6*n6) - 40.0) / 160.0). n5/n6 are ints here (the
  // JS's own comment on the wheel-scx/scz call sites always passes
  // trunc(...) in) so the sum-of-squares is int arithmetic (wraps, i32) then
  // widened to double for sqrt -- not a float32 op, so no fr() case-1
  // shortcut applies to it. The outer fr(fr(sqrt(...)-40.0)/160.0) is a
  // chain of single ops on the resulting double-then-float value: case 1
  // from the sqrt's rounding onward.
  int32_t sumSq = n5 * n5 + n6 * n6;
  float sq = (float)sqrt((double)sumSq);
  float n9 = (sq - 40.0f) / 160.0f;
  if (n9 > 1.0f) n9 = 1.0f;
  if (n9 > 0.2f && !b2) {
    co->ust++;
    if (co->ust == 20) co->ust = 0;
    if (!b) {
      float random = medium_random(co->m);
      // trunc(fr(n2 + fr(x*random)) / fr(1.0+random)) -- both fr()s inside
      // are single ops (case 1), the outer division is float/float (no
      // fr() wraps it, so it's just native float division), then trunc.
      float numX = n2 + (float)co->x * random;
      float numZ = n4 + (float)co->z * random;
      float numY = n3 + (float)co->y * random;
      float den = 1.0f + random;
      co->sx[co->ust] = jtrunc(numX / den);
      co->sz[co->ust] = jtrunc(numZ / den);
      co->sy[co->ust] = jtrunc(numY / den);
    } else {
      // trunc(fr(n2 + (this.x+n5)) / 2.0) -- this.x+n5 is genuinely int
      // (both operands ints), n2 is float, so "n2 + intSum" is a single
      // float op (case 1); the /2.0 is native float division, not fr()-
      // wrapped.
      int32_t sumX = co->x + n5;
      int32_t sumZ = co->z + n6;
      co->sx[co->ust] = jtrunc((n2 + (float)sumX) / 2.0f);
      co->sz[co->ust] = jtrunc((n4 + (float)sumZ) / 2.0f);
      co->sy[co->ust] = jtrunc(n3);
    }
    if (co->sy[n] > 250) co->sy[n] = 250;
    co->osmag[co->ust] = n7 * n9;
    co->scx[co->ust] = (float)n5;
    co->scz[co->ust] = (float)n6;
    co->stg[co->ust] = 1;
  }
  nfm_set_draw_phase(false);
}

// Ports `sprk()` -- see cont_o.h's doc comment for scope (seed-only, the
// drawing half `dsprk` stays stubbed).
void cont_o_sprk(ContO *co, float n, float n2, float n3, float rcx, float rcy, float rcz, int32_t n4) {
  Medium *m = co->m;
  if (n4 != 1) {
    // trunc(n - fr(sprkat*sin(xz))) -- single-op fr() inside, case 1.
    co->srx = jtrunc(n - (float)co->sprkat * medium_sin(m, (float)co->xz));
    co->sry = jtrunc(n2 - (float)co->sprkat * medium_cos(m, (float)co->zy) * medium_cos(m, (float)co->xy));
    co->srz = jtrunc(n3 + (float)co->sprkat * medium_cos(m, (float)co->xz));
    co->sprk_ = 1;
  } else {
    co->sprk_++;
    if (co->sprk_ == 4) {
      co->srx = jtrunc((float)co->x + rcx);
      co->sry = jtrunc(n2);
      co->srz = jtrunc((float)co->z + rcz);
      co->sprk_ = 5;
    } else {
      co->srx = jtrunc(n);
      co->sry = jtrunc(n2);
      co->srz = jtrunc(n3);
    }
  }
  if (n4 == 2) co->sprk_ = 6;
  co->rcx = rcx;
  co->rcy = rcy;
  co->rcz = rcz;
}

void cont_o_step_fix(ContO *co) {
  if (!co->fix) return;
  if (co->fcnt > 7) {
    co->fcnt = 0;
    co->fix = false;
  } else {
    co->fcnt++;
  }
}

// Draws the electric-fence bolt effect for one `fix(`-placed object (4
// bolts, one per side, each redrawn from a fresh random zigzag every ~60
// frames on average -- see the elc[i] cooldown at the end of the loop).
// Ported in full: unlike lowshadow/fixit/pdust/dsprk, this is genuinely
// reachable as soon as a real stage with a `fix(` command loads -- see
// cont_o.h's doc comment on why this one, alone of the five, isn't stubbed.
void cont_o_electrify(ContO *co, struct Graphics2D *g) {
  Medium *m = co->m;
  for (int32_t i = 0; i < 4; i++) {
    if (co->elc[i] == 0 && !m->interpolating) {
      // fr(380-fr(rand*760)), single outer op each -- case 1, native float exact.
      co->edl[i] = jtrunc(380.0f - medium_random(m) * 760.0f);
      co->edr[i] = jtrunc(380.0f - medium_random(m) * 760.0f);
      co->elc[i] = 1;
    }

    // fr(edl[i] + fr(190 - fr(rand*380))) -- chained single-op fr()s, case
    // 1 all the way (edl[i] is an int, but "int + float" under fr() is
    // still ONE binary op).
    float n_ = (float)co->edl[i] + (190.0f - medium_random(m) * 380.0f);
    int32_t n_i = jtrunc(n_);
    float n2_ = (float)co->edr[i] + (190.0f - medium_random(m) * 380.0f);
    int32_t n2_i = jtrunc(n2_);
    // trunc(rand*126.0) -- no fr() at all, genuinely double.
    int32_t n3_i = jtrunc_d((double)medium_random(m) * 126.0);
    int32_t n4_i = jtrunc_d((double)medium_random(m) * 126.0);

    int32_t arr[8], arr2[8], arr3[8];
    for (int32_t j = 0; j < 8; j++) arr3[j] = co->z - m->z;
    // Each array2[k] is trunc(intExpr - trunc(rand*5.0)) -- the inner
    // trunc(rand*5.0) has no fr() (double, jtrunc_d); everything around it
    // is already-integer arithmetic, so the outer trunc() is a no-op over
    // plain int32 (-fwrapv) arithmetic.
    arr[0] = co->x - m->x - 504;
    arr2[0] = co->y - m->y - co->edl[i] - 5 - jtrunc_d((double)medium_random(m) * 5.0);
    arr[1] = co->x - m->x - 252 + n4_i;
    arr2[1] = co->y - m->y - n_i - 5 - jtrunc_d((double)medium_random(m) * 5.0);
    arr[2] = co->x - m->x + 252 - n3_i;
    arr2[2] = co->y - m->y - n2_i - 5 - jtrunc_d((double)medium_random(m) * 5.0);
    arr[3] = co->x - m->x + 504;
    arr2[3] = co->y - m->y - co->edr[i] - 5 - jtrunc_d((double)medium_random(m) * 5.0);
    arr[4] = co->x - m->x + 504;
    arr2[4] = co->y - m->y - co->edr[i] + 5 + jtrunc_d((double)medium_random(m) * 5.0);
    arr[5] = co->x - m->x + 252 - n3_i;
    arr2[5] = co->y - m->y - n2_i + 5 + jtrunc_d((double)medium_random(m) * 5.0);
    arr[6] = co->x - m->x - 252 + n4_i;
    arr2[6] = co->y - m->y - n_i + 5 + jtrunc_d((double)medium_random(m) * 5.0);
    arr[7] = co->x - m->x - 504;
    arr2[7] = co->y - m->y - co->edl[i] + 5 + jtrunc_d((double)medium_random(m) * 5.0);

    if (co->roted) {
      cont_o_rot(co, arr, arr3, co->x - m->x, co->z - m->z, 90, 8);
    }
    cont_o_rot(co, arr, arr3, m->cx, m->cz, m->xz + m->fxz, 8);
    cont_o_rot(co, arr2, arr3, m->cy, m->cz, m->zy + m->fzy, 8);

    bool b = true;
    int32_t n5 = 0, n6 = 0, n7 = 0, n8 = 0;
    int32_t arr4[8], arr5[8];
    for (int32_t k = 0; k < 8; k++) {
      arr4[k] = cont_o_xs(co, arr[k], arr3[k]);
      arr5[k] = cont_o_ys(co, arr2[k], arr3[k]);
      if (arr5[k] < m->ih || arr3[k] < 10) n5++;
      if (arr5[k] > m->h || arr3[k] < 10) n6++;
      if (arr4[k] < m->iw || arr3[k] < 10) n7++;
      if (arr4[k] > m->w || arr3[k] < 10) n8++;
    }
    if (n7 == 8 || n5 == 8 || n6 == 8 || n8 == 8) b = false;

    if (b) {
      // trunc(fr(K + K*fr(snap/500))) -- the outer fr() wraps a MULTI-op
      // expression (multiply then add, not itself split by an inner fr()
      // around the whole product) -- case 2, compute in double, round to
      // float32 once, then trunc.
      float snap0 = (float)m->snap[0] / 500.0f, snap1 = (float)m->snap[1] / 500.0f,
            snap2 = (float)m->snap[2] / 500.0f;
      int32_t n9 = jtrunc((float)(160.0 + 160.0 * (double)snap0));
      if (n9 > 255) n9 = 255;
      if (n9 < 0) n9 = 0;
      int32_t n10 = jtrunc((float)(238.0 + 238.0 * (double)snap1));
      if (n10 > 255) n10 = 255;
      if (n10 < 0) n10 = 0;
      int32_t b2 = jtrunc((float)(255.0 + 255.0 * (double)snap2));
      if (b2 > 255) b2 = 255;
      if (b2 < 0) b2 = 0;

      int32_t r = (n9 * 2 + 214 * (co->elc[i] - 1)) / (co->elc[i] + 1);
      int32_t gg = (n10 * 2 + 236 * (co->elc[i] - 1)) / (co->elc[i] + 1);
      if (m->trk == 1) {
        r = 255;
        gg = 128;
        b2 = 0;
      }
      gfx_set_color(g, r, gg, b2);
      gfx_fill_polygon(g, arr4, arr5, 8);

      if (arr3[0] < 4000) {
        int32_t r2 = jtrunc((float)(150.0 + 150.0 * (double)snap0));
        if (r2 > 255) r2 = 255;
        if (r2 < 0) r2 = 0;
        int32_t g2 = jtrunc((float)(227.0 + 227.0 * (double)snap1));
        if (g2 > 255) g2 = 255;
        if (g2 < 0) g2 = 0;
        int32_t b3 = jtrunc((float)(255.0 + 255.0 * (double)snap2));
        if (b3 > 255) b3 = 255;
        if (b3 < 0) b3 = 0;
        gfx_set_color(g, r2, g2, b3);
        gfx_draw_polygon(g, arr4, arr5, 8);
      }
    }

    if (!m->interpolating) {
      float randVal = medium_random(m) * 60.0f; // fr(rand*60), case 1
      if ((float)co->elc[i] > randVal) co->elc[i] = 0;
      else co->elc[i]++;
    }
  }

  if (m->interpolating) return;
  if (!co->roted || co->xz != 0) {
    co->xy += 11;
    if (co->xy > 360) co->xy -= 360;
  } else {
    co->zy += 11;
    if (co->zy > 360) co->zy -= 360;
  }
}

// Ports `fixit(Graphics2D)` (ContO.java:1582-1766) -- the "Car Fixed" flash
// drawn while a damaged car sits on an active `fix(`-placed repair pad
// (co->fix, set true by mad.c whenever a damaged car is near one, mirroring
// Mad.java exactly). Was stubbed to abort on the same "M1 never triggers
// this" theory as electrify/pdust/dsprk/lowshadow before it (see this
// file's own comments on those) -- but mad.c has set co->fix for real since
// this port's very first damage-and-repair-pad stage, so any damaged car
// that ever reaches a repair pad hit this and aborted the whole process.
// Two effects, both keyed off co->fcnt (advanced 0->7 by cont_o_step_fix,
// see cont_o.h's own doc comment on that split):
// 1. Retints every body Plane bright cyan-ish (HSB 0.57/0.8/0.8, snap-
//    tinted) via p[i].hsb/p[i].flx -- Plane.d() (plane.c) already has the
//    full flx flash-state-machine ported, this only needs to arm it.
// 2. Two overlapping 8-point "star" polygons (light blue, then near-white)
//    drawn around the car's on-screen silhouette while fcnt is 1 or 3-7.
void cont_o_fixit(ContO *co, struct Graphics2D *g) {
  Medium *m = co->m;

  if (co->fcnt == 1) {
    for (int32_t i = 0; i < co->npl; i++) {
      co->p[i].hsb[0] = 0.57f;
      co->p[i].hsb[2] = 0.8f;
      co->p[i].hsb[1] = 0.8f;
      int32_t hsbColor = hsb_to_rgb(co->p[i].hsb[0], co->p[i].hsb[1], co->p[i].hsb[2]);
      int32_t hr = (hsbColor >> 16) & 0xff, hg = (hsbColor >> 8) & 0xff, hb = hsbColor & 0xff;
      int32_t r = jtrunc((float)hr + (float)hr * ((float)m->snap[0] / 100.0f));
      if (r > 255) r = 255;
      if (r < 0) r = 0;
      int32_t gg = jtrunc((float)hg + (float)hg * ((float)m->snap[1] / 100.0f));
      if (gg > 255) gg = 255;
      if (gg < 0) gg = 0;
      int32_t b = jtrunc((float)hb + (float)hb * ((float)m->snap[2] / 100.0f));
      if (b > 255) b = 255;
      if (b < 0) b = 0;
      rgb_to_hsb(r, gg, b, co->p[i].hsb);
      co->p[i].flx = 1;
    }
  }
  if (co->fcnt == 2) {
    for (int32_t j = 0; j < co->npl; j++) co->p[j].flx = 1;
  }
  if (co->fcnt == 4) {
    for (int32_t k = 0; k < co->npl; k++) co->p[k].flx = 3;
  }

  if ((co->fcnt == 1 || co->fcnt > 2) && co->fcnt != 9) {
    int32_t array[8], array2[8], array3[4];
    for (int32_t l = 0; l < 4; l++) {
      array[l] = co->keyx[l] + co->x - m->x;
      array2[l] = co->grat + co->y - m->y;
      array3[l] = co->keyz[l] + co->z - m->z;
    }
    cont_o_rot(co, array, array2, co->x - m->x, co->y - m->y, co->xy, 4);
    // ContO.java:1634 -- `this.z - this.m.y`, NOT `this.m.z`. Preserved
    // exactly: this codebase's fidelity mandate is literal translation,
    // and getting this "wrong" on purpose only matches the real applet.
    cont_o_rot(co, array2, array3, co->y - m->y, co->z - m->y, co->zy, 4);
    cont_o_rot(co, array, array3, co->x - m->x, co->z - m->z, co->xz, 4);
    cont_o_rot(co, array, array3, m->cx, m->cz, m->xz + m->fxz, 4);
    cont_o_rot(co, array2, array3, m->cy, m->cz, m->zy + m->fzy, 4);

    int32_t absx = 0, absy = 0, py = 0;
    for (int32_t n = 0; n < 4; n++) {
      for (int32_t n2 = 0; n2 < 4; n2++) {
        int32_t dx = abs(array[n] - array[n2]);
        if (dx > absx) absx = dx;
        int32_t dy = abs(array2[n] - array2[n2]);
        if (dy > absy) absy = dy;
        int32_t p = trackers_py(array[n], array[n2], array2[n], array2[n2]);
        if (p > py) py = p;
      }
    }
    int32_t n3 = jtrunc_d(sqrt((double)py) / 1.5);
    if (absx < n3) absx = n3;
    if (absy < n3) absy = n3;

    float cosXZ = medium_cos(m, m->xz + m->fxz), sinXZ = medium_sin(m, m->xz + m->fxz);
    int32_t n4 = m->cx + jtrunc((float)(co->x - m->x - m->cx) * cosXZ - (float)(co->z - m->z - m->cz) * sinXZ);
    int32_t n5 = m->cz + jtrunc((float)(co->x - m->x - m->cx) * sinXZ + (float)(co->z - m->z - m->cz) * cosXZ);
    float cosZY = medium_cos(m, m->zy + m->fzy), sinZY = medium_sin(m, m->zy + m->fzy);
    int32_t n6 = m->cy + jtrunc((float)(co->y - m->y - m->cy) * cosZY - (float)(n5 - m->cz) * sinZY);
    int32_t n7 = m->cz + jtrunc((float)(co->y - m->y - m->cy) * sinZY + (float)(n5 - m->cz) * cosZY);

    array[0] = cont_o_xs(co, jtrunc_d((double)n4 - (double)absx / 0.8 - (double)medium_random(m) * ((double)absx / 2.4)), n7);
    array2[0] = cont_o_ys(co, jtrunc_d((double)n6 - (double)absy / 1.92 - (double)medium_random(m) * ((double)absy / 5.67)), n7);
    array[1] = cont_o_xs(co, jtrunc_d((double)n4 - (double)absx / 0.8 - (double)medium_random(m) * ((double)absx / 2.4)), n7);
    array2[1] = cont_o_ys(co, jtrunc_d((double)n6 + (double)absy / 1.92 + (double)medium_random(m) * ((double)absy / 5.67)), n7);
    array[2] = cont_o_xs(co, jtrunc_d((double)n4 - (double)absx / 1.92 - (double)medium_random(m) * ((double)absx / 5.67)), n7);
    array2[2] = cont_o_ys(co, jtrunc_d((double)n6 + (double)absy / 0.8 + (double)medium_random(m) * ((double)absy / 2.4)), n7);
    array[3] = cont_o_xs(co, jtrunc_d((double)n4 + (double)absx / 1.92 + (double)medium_random(m) * ((double)absx / 5.67)), n7);
    array2[3] = cont_o_ys(co, jtrunc_d((double)n6 + (double)absy / 0.8 + (double)medium_random(m) * ((double)absy / 2.4)), n7);
    array[4] = cont_o_xs(co, jtrunc_d((double)n4 + (double)absx / 0.8 + (double)medium_random(m) * ((double)absx / 2.4)), n7);
    array2[4] = cont_o_ys(co, jtrunc_d((double)n6 + (double)absy / 1.92 + (double)medium_random(m) * ((double)absy / 5.67)), n7);
    array[5] = cont_o_xs(co, jtrunc_d((double)n4 + (double)absx / 0.8 + (double)medium_random(m) * ((double)absx / 2.4)), n7);
    array2[5] = cont_o_ys(co, jtrunc_d((double)n6 - (double)absy / 1.92 - (double)medium_random(m) * ((double)absy / 5.67)), n7);
    array[6] = cont_o_xs(co, jtrunc_d((double)n4 + (double)absx / 1.92 + (double)medium_random(m) * ((double)absx / 5.67)), n7);
    array2[6] = cont_o_ys(co, jtrunc_d((double)n6 - (double)absy / 0.8 - (double)medium_random(m) * ((double)absy / 2.4)), n7);
    array[7] = cont_o_xs(co, jtrunc_d((double)n4 - (double)absx / 1.92 - (double)medium_random(m) * ((double)absx / 5.67)), n7);
    array2[7] = cont_o_ys(co, jtrunc_d((double)n6 - (double)absy / 0.8 - (double)medium_random(m) * ((double)absy / 2.4)), n7);

    int32_t cx_screen = cont_o_xs(co, n4, n7), cy_screen = cont_o_ys(co, n6, n7);
    if (co->fcnt == 3) cont_o_rot(co, array, array2, cx_screen, cy_screen, 22, 8);
    if (co->fcnt == 4) cont_o_rot(co, array, array2, cx_screen, cy_screen, 22, 8);
    if (co->fcnt == 5) cont_o_rot(co, array, array2, cx_screen, cy_screen, 0, 8);
    if (co->fcnt == 6) cont_o_rot(co, array, array2, cx_screen, cy_screen, -22, 8);
    if (co->fcnt == 7) cont_o_rot(co, array, array2, cx_screen, cy_screen, -22, 8);

    int32_t r2 = jtrunc(191.0f + 191.0f * ((float)m->snap[0] / 350.0f));
    if (r2 > 255) r2 = 255;
    if (r2 < 0) r2 = 0;
    int32_t g2 = jtrunc(232.0f + 232.0f * ((float)m->snap[1] / 350.0f));
    if (g2 > 255) g2 = 255;
    if (g2 < 0) g2 = 0;
    int32_t b2 = jtrunc(255.0f + 255.0f * ((float)m->snap[2] / 350.0f));
    if (b2 > 255) b2 = 255;
    if (b2 < 0) b2 = 0;
    gfx_set_color(g, r2, g2, b2);
    gfx_fill_polygon(g, array, array2, 8);

    array[0] = cont_o_xs(co, jtrunc_d((double)n4 - (double)absx - (double)medium_random(m) * ((double)absx / 4)), n7);
    array2[0] = cont_o_ys(co, jtrunc_d((double)n6 - (double)absy / 2.4 - (double)medium_random(m) * ((double)absy / 9.6)), n7);
    array[1] = cont_o_xs(co, jtrunc_d((double)n4 - (double)absx - (double)medium_random(m) * ((double)absx / 4)), n7);
    array2[1] = cont_o_ys(co, jtrunc_d((double)n6 + (double)absy / 2.4 + (double)medium_random(m) * ((double)absy / 9.6)), n7);
    array[2] = cont_o_xs(co, jtrunc_d((double)n4 - (double)absx / 2.4 - (double)medium_random(m) * ((double)absx / 9.6)), n7);
    array2[2] = cont_o_ys(co, jtrunc_d((double)n6 + (double)absy + (double)medium_random(m) * ((double)absy / 4)), n7);
    array[3] = cont_o_xs(co, jtrunc_d((double)n4 + (double)absx / 2.4 + (double)medium_random(m) * ((double)absx / 9.6)), n7);
    array2[3] = cont_o_ys(co, jtrunc_d((double)n6 + (double)absy + (double)medium_random(m) * ((double)absy / 4)), n7);
    array[4] = cont_o_xs(co, jtrunc_d((double)n4 + (double)absx + (double)medium_random(m) * ((double)absx / 4)), n7);
    array2[4] = cont_o_ys(co, jtrunc_d((double)n6 + (double)absy / 2.4 + (double)medium_random(m) * ((double)absy / 9.6)), n7);
    array[5] = cont_o_xs(co, jtrunc_d((double)n4 + (double)absx + (double)medium_random(m) * ((double)absx / 4)), n7);
    array2[5] = cont_o_ys(co, jtrunc_d((double)n6 - (double)absy / 2.4 - (double)medium_random(m) * ((double)absy / 9.6)), n7);
    array[6] = cont_o_xs(co, jtrunc_d((double)n4 + (double)absx / 2.4 + (double)medium_random(m) * ((double)absx / 9.6)), n7);
    array2[6] = cont_o_ys(co, jtrunc_d((double)n6 - (double)absy - (double)medium_random(m) * ((double)absy / 4)), n7);
    array[7] = cont_o_xs(co, jtrunc_d((double)n4 - (double)absx / 2.4 - (double)medium_random(m) * ((double)absx / 9.6)), n7);
    array2[7] = cont_o_ys(co, jtrunc_d((double)n6 - (double)absy - (double)medium_random(m) * ((double)absy / 4)), n7);

    int32_t r3 = jtrunc(213.0f + 213.0f * ((float)m->snap[0] / 350.0f));
    if (r3 > 255) r3 = 255;
    if (r3 < 0) r3 = 0;
    int32_t g3 = jtrunc(239.0f + 239.0f * ((float)m->snap[1] / 350.0f));
    if (g3 > 255) g3 = 255;
    if (g3 < 0) g3 = 0;
    int32_t b3 = jtrunc(255.0f + 255.0f * ((float)m->snap[2] / 350.0f));
    if (b3 > 255) b3 = 255;
    if (b3 < 0) b3 = 0;
    gfx_set_color(g, r3, g3, b3);
    gfx_fill_polygon(g, array, array2, 8);
  }
  // fcnt is advanced once per tick by cont_o_step_fix, and only there. It
  // was ALSO advanced here, per draw: with a draw per display frame it ran
  // past the 7/8 that mad_drive reads once a tick to finish the repair, so
  // the car was never rebuilt and kept the repair tint.
}
// Ports `pdust()` -- the DRAWING half of the dust-puff particle (see
// cont_o_dust for the seed-only half), ContO.java:1950-2105. Was stubbed
// to abort on the "M1 never seeds a particle, so this never fires" theory,
// same as electrify originally was -- but Mad.js's `drive()` calls
// `dust()` (cont_o_dust) any time a wheel's suspension travel differs
// from its rest position, which is true for almost any real driving over
// uneven ground, not just skids. First real physics-driven run (native/
// platform/linux/main.c, once mad_drive() was wired in) hit this within
// a few hundred ticks -- but the version that was ported then turned out
// to have been translated from an intermediate JS layer that itself
// diverged from the real Java in several ways (found via a later racing-
// VFX audit against ContO.java directly, not just web/ContO.js): a
// completely different 8-point puff geometry (uniform 45-degree/0.7071
// octagon here vs Java's uneven 22.5-degree-based shape with 1.5x/1.7x
// stretched vertices), no alpha blending at all (Java fades the puff via
// AlphaComposite, aging it from `sbln[n]` toward 0 as `stg[n]` climbs),
// wrong/incomplete `sbln[n]` base values, a `t->x[n2]`-vs-camera-relative
// coordinate mismatch that made the track-color sector lookup almost
// never match, an always-{0,0,0} tint-add array that should be a real
// snap-derived value, and a wrong `smag` growth rate/update order. This
// is a from-scratch rewrite against ContO.java directly rather than a
// patch of the old translation, since the divergence turned out to be
// structural, not a handful of constants.
void cont_o_pdust(ContO *co, int32_t n, struct Graphics2D *g, bool b) {
  Medium *m = co->m;
  Trackers *t = co->t;

  // ContO.java:1951-1954 -- sav[n] (squared-distance-derived) is a CACHE:
  // only recomputed on the "far objects" pass (b==true) and reused as-is
  // on the same frame's "near objects" pass (b==false), not recomputed
  // per-pass. Matches this file's existing dsprk/lowshadow distance-cache
  // convention (see their own comments) -- same reasoning, same author.
  if (b) {
    int32_t dx = m->x + m->cx - co->sx[n];
    int32_t dy = m->y + m->cy - co->sy[n];
    int32_t dz = m->z - co->sz[n];
    co->sav[n] = jtrunc_d(sqrt((double)dx * (double)dx + (double)dy * (double)dy + (double)dz * (double)dz));
  }
  if ((b && co->sav[n] > co->dist) || (!b && co->sav[n] <= co->dist)) {
    // Frames between ticks draw the puff where the tick left it: the roll at
    // stage 1, the drift, the growth and the stage advance run on a tick only.
    // Without this a puff lives 8 draws (~133 ms at 60 Hz) instead of 8 ticks.
    const bool tick = !m->interpolating;
    if (co->stg[n] == 1 && tick) {
      co->sbln[n] = 0.6f;
      bool b2 = false;
      int32_t array[3];
      for (int32_t i = 0; i < 3; i++) {
        array[i] = jtrunc(255.0f + 255.0f * ((float)m->snap[i] / 100.0f));
        if (array[i] > 255) array[i] = 255;
        if (array[i] < 0) array[i] = 0;
      }
      int32_t ncx = (co->x - t->sx) / 3000;
      if (ncx > t->ncx) ncx = t->ncx;
      if (ncx < 0) ncx = 0;
      int32_t ncz = (co->z - t->sz) / 3000;
      if (ncz > t->ncz) ncz = t->ncz;
      if (ncz < 0) ncz = 0;
      // ContO.java:1982-1999 iterates this sector's entries forward
      // (0..length-1); the LAST matching entry wins sbln[n]/srgb[n] if
      // more than one matches, so the iteration direction matters here
      // (unlike a pure "does anything match" search) -- must go forward,
      // not backward like this file's other sect[][] scans.
      for (int32_t j = 0; j < t->sect_len[ncx][ncz]; j++) {
        int32_t n2 = t->sect[ncx][ncz][j];
        if (abs(t->zy[n2]) != 90 && abs(t->xy[n2]) != 90 &&
            abs(co->sx[n] - t->x[n2]) < t->radx[n2] &&
            abs(co->sz[n] - t->z[n2]) < t->radz[n2]) {
          if (t->skd[n2] == 0) co->sbln[n] = 0.2f;
          if (t->skd[n2] == 1) co->sbln[n] = 0.4f;
          if (t->skd[n2] == 2) co->sbln[n] = 0.45f;
          for (int32_t k = 0; k < 3; k++) {
            co->srgb[n][k] = (t->c[n2][k] + array[k]) / 2;
          }
          b2 = true;
        }
      }
      if (!b2) {
        for (int32_t l = 0; l < 3; l++) {
          co->srgb[n][l] = (m->crgrnd[l] + array[l]) / 2;
        }
      }
      float n3 = (float)(0.1 + (double)medium_random(m));
      if (n3 > 1.0f) n3 = 1.0f;
      // `scx[n] *= n3` -- the bytecode is iaload; i2f; fmul; f2i (a float
      // decay), not `*= (int)n3` as the decompiler prints it.
      co->scx[n] = jtrunc((float)co->scx[n] * n3);
      co->scz[n] = jtrunc((float)co->scx[n] * n3);
      for (int32_t n4 = 0; n4 < 8; n4++) {
        co->smag[n][n4] = co->osmag[n] * medium_random(m) * 50.0f;
      }
      for (int32_t n5 = 0; n5 < 8; n5++) {
        int32_t n6 = n5 - 1;
        if (n6 == -1) n6 = 7;
        int32_t n7 = n5 + 1;
        if (n7 == 8) n7 = 0;
        co->smag[n][n5] = ((co->smag[n][n6] + co->smag[n][n7]) / 2.0f + co->smag[n][n5]) / 2.0f;
      }
      co->smag[n][6] = co->smag[n][7];
    }

    float cosXZ = medium_cos(m, m->xz + m->fxz), sinXZ = medium_sin(m, m->xz + m->fxz);
    int32_t n8 = m->cx + jtrunc((float)(co->sx[n] - m->x - m->cx) * cosXZ - (float)(co->sz[n] - m->z - m->cz) * sinXZ);
    int32_t n9 = m->cz + jtrunc((float)(co->sx[n] - m->x - m->cx) * sinXZ + (float)(co->sz[n] - m->z - m->cz) * cosXZ);
    float cosZY = medium_cos(m, m->zy + m->fzy), sinZY = medium_sin(m, m->zy + m->fzy);
    // ContO.java:2029-2030 -- `smag[n][7]` is subtracted BEFORE the
    // multiply by cos/sin (one float value, one multiply each), not
    // subtracted as a separately-multiplied term after -- mathematically
    // the same, but not bit-identical under IEEE float rounding, and this
    // codebase's own convention elsewhere is to match Java's exact
    // operation grouping rather than an equivalent regrouping.
    float ySum = (float)(co->sy[n] - m->y - m->cy) - co->smag[n][7];
    int32_t n10 = m->cy + jtrunc(ySum * cosZY - (float)(n9 - m->cz) * sinZY);
    int32_t n11 = m->cz + jtrunc(ySum * sinZY + (float)(n9 - m->cz) * cosZY);
    if (tick) {
      co->sx[n] = co->sx[n] + co->scx[n] / (co->stg[n] + 1);
      co->sz[n] = co->sz[n] + co->scz[n] / (co->stg[n] + 1);
    }

    int32_t array2[8], array3[8];
    array2[0] = cont_o_xs(co, jtrunc((float)n8 + co->smag[n][0] * 0.9238f * 1.5f), n11);
    array3[0] = cont_o_ys(co, jtrunc((float)n10 + co->smag[n][0] * 0.3826f * 1.5f), n11);
    array2[1] = cont_o_xs(co, jtrunc((float)n8 + co->smag[n][1] * 0.9238f * 1.5f), n11);
    array3[1] = cont_o_ys(co, jtrunc((float)n10 - co->smag[n][1] * 0.3826f * 1.5f), n11);
    array2[2] = cont_o_xs(co, jtrunc((float)n8 + co->smag[n][2] * 0.3826f), n11);
    array3[2] = cont_o_ys(co, jtrunc((float)n10 - co->smag[n][2] * 0.9238f), n11);
    array2[3] = cont_o_xs(co, jtrunc((float)n8 - co->smag[n][3] * 0.3826f), n11);
    array3[3] = cont_o_ys(co, jtrunc((float)n10 - co->smag[n][3] * 0.9238f), n11);
    array2[4] = cont_o_xs(co, jtrunc((float)n8 - co->smag[n][4] * 0.9238f * 1.5f), n11);
    array3[4] = cont_o_ys(co, jtrunc((float)n10 - co->smag[n][4] * 0.3826f * 1.5f), n11);
    array2[5] = cont_o_xs(co, jtrunc((float)n8 - co->smag[n][5] * 0.9238f * 1.5f), n11);
    array3[5] = cont_o_ys(co, jtrunc((float)n10 + co->smag[n][5] * 0.3826f * 1.5f), n11);
    array2[6] = cont_o_xs(co, jtrunc((float)n8 - co->smag[n][6] * 0.3826f * 1.7f), n11);
    array3[6] = cont_o_ys(co, jtrunc((float)n10 + co->smag[n][6] * 0.9238f), n11);
    array2[7] = cont_o_xs(co, jtrunc((float)n8 + co->smag[n][7] * 0.3826f * 1.7f), n11);
    array3[7] = cont_o_ys(co, jtrunc((float)n10 + co->smag[n][7] * 0.9238f), n11);

    if (tick) {
      for (int32_t n12 = 0; n12 < 7; n12++) {
        co->smag[n][n12] = co->smag[n][n12] + (5.0f + medium_random(m) * 15.0f);
      }
      co->smag[n][7] = co->smag[n][6];
    }

    bool b3 = true;
    int32_t n14 = 0, n15 = 0, n16 = 0, n17 = 0;
    for (int32_t n18 = 0; n18 < 8; n18++) {
      if (array3[n18] < m->ih || n11 < 10) n14++;
      if (array3[n18] > m->h || n11 < 10) n15++;
      if (array2[n18] < m->iw || n11 < 10) n16++;
      if (array2[n18] > m->w || n11 < 10) n17++;
    }
    // ContO.java:2078 -- the real threshold is 4 (half the 8 points), not
    // 8/"all of them" like this file's other visibility checks (electrify/
    // fixit/dsprk). Preserved exactly: a real, if unusually lenient,
    // culling rule in the source, not a typo to "correct" to match its
    // siblings.
    if (n16 == 4 || n14 == 4 || n15 == 4 || n17 == 4) b3 = false;

    if (b3) {
      int32_t r = co->srgb[n][0], gg = co->srgb[n][1], b4 = co->srgb[n][2];
      for (int32_t n19 = 0; n19 < 16; n19++) {
        if (co->sav[n] > m->fade[n19]) {
          r = (r * m->fogd + m->cfade[0]) / (m->fogd + 1);
          gg = (gg * m->fogd + m->cfade[1]) / (m->fogd + 1);
          b4 = (b4 * m->fogd + m->cfade[2]) / (m->fogd + 1);
        }
      }
      gfx_set_color(g, r, gg, b4);
      gfx_set_composite(g, co->sbln[n] - (float)co->stg[n] * (co->sbln[n] / 8.0f));
      gfx_fill_polygon(g, array2, array3, 8);
      gfx_set_composite(g, 1.0f);
    }

    if (tick) {
      if (co->stg[n] == 7) co->stg[n] = 0;
      else co->stg[n]++;
    }
  }
}
// Unlike pdust (only called from a `co->stg[n]!=0` guard, so genuinely
// never invoked while stg stays all-zero), dsprk is called UNCONDITIONALLY
// every frame co->shadow is true -- but its own body is entirely gated on
// co->sprk_/co->rtg[], both of which stay zero until the (unported) sprk()
// runtime method sets them from a physics tick. So this ports the JS's
// real outer structure (making the every-frame call safe and correct for
// an undamaged car) while stubbing only the truly unreachable-for-M1 inner
// bodies -- the spawn logic (needs co->rbef/rx/ry/rz/vrx/vry/vrz, which
// are still untyped void* placeholders, see cont_o.h) and the per-spark
// trail draw/physics/decay logic.
// Ports `dsprk()` in full -- see cont_o_pdust's doc comment for why a
// stub that "M1 never seeds a particle" theory doesn't hold once
// mad_drive()/regy/regx/regz actually call sprk() on wall scrapes: first
// real physics-driven run hit the spawn abort within a few hundred
// ticks.
static void cont_o_dsprk(ContO *co, struct Graphics2D *g, bool b) {
  Medium *m = co->m;
  // rcx/rcy/rcz*rcx/rcy/rcz + sqrt + trunc: no fr() anywhere in the JS,
  // so this is genuinely double throughout -- computed once here since
  // it's a pure function of rcx/rcy/rcz (no PRNG draw), reused below for
  // both `n` (spark count) and the per-spark `n3`, matching the JS's own
  // two textually-identical-but-independently-evaluated calls exactly
  // (recomputing a pure expression is behaviorally identical to caching
  // it -- unlike this.m.random(), there's no draw-count to preserve).
  double sumSqD = (double)co->rcx * co->rcx + (double)co->rcy * co->rcy + (double)co->rcz * co->rcz;
  int32_t sparkMagTrunc = jtrunc_d(sqrt(sumSqD));

  if (b && co->sprk_ != 0) {
    int32_t n = sparkMagTrunc / 10;
    if (n > 5) {
      bool b2 = false;
      int32_t dx = m->x + m->cx - co->srx;
      int32_t dy = m->y + m->cy - co->sry;
      int32_t dz = m->z - co->srz;
      int32_t distSumSq = dx * dx + dy * dy + dz * dz; // wraps int32, matches i32(imul+imul+imul)
      if ((double)co->dist < sqrt((double)distSumSq)) b2 = true;
      if (n > 33) n = 33;
      // Spawning is once per tick -- unguarded on an interpolated frame,
      // this would reseed n sparks from the same crash every draw.
      int32_t n2 = m->interpolating ? n : 0;
      for (int32_t i = 0; i < 100 && n2 != n; i++) {
        if (co->rtg[i] == 0) {
          co->rtg[i] = 1;
          co->rbef[i] = b2;
          n2++;
        }
        if (n2 == n) break;
      }
    }
  }
  for (int32_t j = 0; j < 100; j++) {
    if (co->rtg[j] != 0 && ((co->rbef[j] && b) || (!co->rbef[j] && !b))) {
      if (co->rtg[j] == 1 && !m->interpolating) {
        if (co->sprk_ < 5) {
          // trunc(srx + 3 - fr(rand*6.7)) -- no fr() around the whole
          // thing, genuinely double; fr(rand*6.7) itself is case 1.
          float rt = medium_random(m) * 6.7f;
          co->rx[j] = jtrunc_d((double)co->srx + 3.0 - (double)rt);
          rt = medium_random(m) * 6.7f;
          co->ry[j] = jtrunc_d((double)co->sry + 3.0 - (double)rt);
          rt = medium_random(m) * 6.7f;
          co->rz[j] = jtrunc_d((double)co->srz + 3.0 - (double)rt);
        } else {
          float rt = medium_random(m) * 20.0f;
          co->rx[j] = jtrunc_d((double)co->srx + 10.0 - (double)rt);
          rt = medium_random(m) * 4.0f;
          co->ry[j] = jtrunc_d((double)co->sry - (double)rt);
          rt = medium_random(m) * 20.0f;
          co->rz[j] = jtrunc_d((double)co->srz + 10.0 - (double)rt);
        }
        int32_t n3 = sparkMagTrunc;
        // fr(0.2 + 0.4*rand) -- one fr() over two chained ops, case 2.
        double n4_d = 0.2 + 0.4 * (double)medium_random(m);
        float n4 = (float)n4_d;
        // fr(fr(rand*rand)*rand) -- case 1 chain, three draws in order.
        float ra = medium_random(m);
        float rb = medium_random(m);
        float n5 = ra * rb;
        float rc = medium_random(m);
        n5 = n5 * rc;
        float n6 = 1.0f;

        // Each block: two `rand()>rand()` comparisons, evaluated as
        // separate statements (not inline in the `>`) to pin the
        // left-then-right draw order C doesn't otherwise guarantee for
        // an operator's operands, matching JS's guaranteed left-to-right
        // evaluation exactly.
        float ra1 = medium_random(m), rb1 = medium_random(m);
        if (ra1 > rb1) {
          float ra2 = medium_random(m), rb2 = medium_random(m);
          if (ra2 > rb2) n6 *= -1.0f;
          // fr(n3*(1.0-rcx/n3)) -- single fr() over a multi-op double
          // expression, case 2. fr(inner*n5*n6) -- two chained mults
          // under one fr(), also treated as case 2 (not assumed
          // native-float-exact just because the operands are already
          // floats -- see mad.c's own note on this same pattern).
          double ratio = 1.0 - (double)co->rcx / (double)n3;
          float inner1 = (float)((double)n3 * ratio);
          float layer2 = (float)((double)inner1 * (double)n5 * (double)n6);
          float layer3 = co->rcx + layer2; // case 1
          co->vrx[j] = -(layer3 * n4); // case 1, then negate
        }
        float ra3 = medium_random(m), rb3 = medium_random(m);
        if (ra3 > rb3) {
          float ra4 = medium_random(m), rb4 = medium_random(m);
          if (ra4 > rb4) n6 *= -1.0f;
          if (co->sprk_ == 5) n6 = 1.0f;
          double ratio = 1.0 - (double)co->rcy / (double)n3;
          float inner1 = (float)((double)n3 * ratio);
          float layer2 = (float)((double)inner1 * (double)n5 * (double)n6);
          float layer3 = co->rcy + layer2;
          co->vry[j] = -(layer3 * n4);
        }
        float ra5 = medium_random(m), rb5 = medium_random(m);
        if (ra5 > rb5) {
          float ra6 = medium_random(m), rb6 = medium_random(m);
          if (ra6 > rb6) n6 *= -1.0f;
          double ratio = 1.0 - (double)co->rcz / (double)n3;
          float inner1 = (float)((double)n3 * ratio);
          float layer2 = (float)((double)inner1 * (double)n5 * (double)n6);
          float layer3 = co->rcz + layer2;
          co->vrz[j] = -(layer3 * n4);
        }
      }
      if (!m->interpolating) {
        co->rx[j] = jtrunc_d((double)co->rx[j] + (double)co->vrx[j]);
        co->ry[j] = jtrunc_d((double)co->ry[j] + (double)co->vry[j]);
        co->rz[j] = jtrunc_d((double)co->rz[j] + (double)co->vrz[j]);
      }

      // n10/n11: rotate (rx[j]-x, rz[j]-z) around (cx,cz) by xz -- pure
      // int inputs, so this is exactly cont_o_rot's own already-verified
      // formula (case 1: the multiply-by-cos/sin is a single op on an
      // exact int difference). n12/n13: rotate (ry[j]-y, n11) around
      // (cy,cz) by zy, same reasoning.
      int32_t rxz[1] = { co->rx[j] - m->x };
      int32_t rzz[1] = { co->rz[j] - m->z };
      cont_o_rot(co, rxz, rzz, m->cx, m->cz, m->xz + m->fxz, 1);
      int32_t n10 = rxz[0], n11 = rzz[0];
      int32_t ryy[1] = { co->ry[j] - m->y };
      int32_t rzz2[1] = { n11 };
      cont_o_rot(co, ryy, rzz2, m->cy, m->cz, m->zy + m->fzy, 1);
      int32_t n12 = ryy[0], n13 = rzz2[0];

      // n14/n15: same rotation, but the point is (rx[j]-x-cx+vrx[j],
      // rz[j]-z-cz+vrz[j]) -- a float-valued sum (vrx[j]/vrz[j] are
      // float), so this can't reuse cont_o_rot's int-array formula.
      // fr(fr(sum*cos)-fr(sum*sin)) -- each inner fr() wraps a
      // multi-op raw-double sum times a trig value, case 2; the outer
      // subtract is case 1 (single op between two already-rounded
      // floats).
      double sumX14 = (double)(co->rx[j] - m->x - m->cx) + (double)co->vrx[j];
      double sumZ14 = (double)(co->rz[j] - m->z - m->cz) + (double)co->vrz[j];
      float cosXZ = medium_cos(m, m->xz + m->fxz);
      float sinXZ = medium_sin(m, m->xz + m->fxz);
      float termX14cos = (float)(sumX14 * (double)cosXZ);
      float termZ14sin = (float)(sumZ14 * (double)sinXZ);
      int32_t n14 = m->cx + jtrunc(termX14cos - termZ14sin);
      float termX14sin = (float)(sumX14 * (double)sinXZ);
      float termZ14cos = (float)(sumZ14 * (double)cosXZ);
      int32_t n15 = m->cz + jtrunc(termX14sin + termZ14cos);

      // n16/n17: rotate (ry[j]-y-cy+vry[j], n15-cz) around (cy,cz) by
      // zy. First term is the same float-sum case-2 shape as n14/n15;
      // second term is pure int (n15-cz), case 1.
      double sumY16 = (double)(co->ry[j] - m->y - m->cy) + (double)co->vry[j];
      float cosZY = medium_cos(m, m->zy + m->fzy);
      float sinZY = medium_sin(m, m->zy + m->fzy);
      float termY16cos = (float)(sumY16 * (double)cosZY);
      float termZ16sin = (float)(n15 - m->cz) * sinZY; // case 1
      int32_t n16 = m->cy + jtrunc(termY16cos - termZ16sin);
      float termY16sin = (float)(sumY16 * (double)sinZY);
      float termZ16cos = (float)(n15 - m->cz) * cosZY; // case 1
      int32_t n17 = m->cz + jtrunc(termY16sin + termZ16cos);

      int32_t xs = cont_o_xs(co, n10, n13);
      int32_t ys = cont_o_ys(co, n12, n13);
      int32_t xs2 = cont_o_xs(co, n14, n17);
      int32_t ys2 = cont_o_ys(co, n16, n17);
      if (xs < m->iw && xs2 < m->iw) co->rtg[j] = 0;
      if (xs > m->w && xs2 > m->w) co->rtg[j] = 0;
      if (ys < m->ih && ys2 < m->ih) co->rtg[j] = 0;
      if (ys > m->h && ys2 > m->h) co->rtg[j] = 0;
      if (co->ry[j] > 250) co->rtg[j] = 0;
      if (co->rtg[j] != 0) {
        int32_t r = 255;
        int32_t gg = 197 - 30 * co->rtg[j];
        int32_t b3 = 0;
        for (int32_t k = 0; k < 16; k++) {
          if (n13 > m->fade[k]) {
            r = (r * m->fogd + m->cfade[0]) / (m->fogd + 1);
            gg = (gg * m->fogd + m->cfade[1]) / (m->fogd + 1);
            b3 = (b3 * m->fogd + m->cfade[2]) / (m->fogd + 1);
          }
        }
        gfx_set_color(g, r, gg, b3);
        gfx_draw_line(g, xs, ys, xs2, ys2);
        if (!m->interpolating) {
          co->vrx[j] = co->vrx[j] * 0.8f; // fr(vrx*0.8), case 1
          co->vry[j] = co->vry[j] * 0.8f;
          co->vrz[j] = co->vrz[j] * 0.8f;
          if (co->rtg[j] == 3) co->rtg[j] = 0;
          else co->rtg[j]++;
        }
      }
    }
  }
  if (co->sprk_ != 0) co->sprk_ = 0;
}

static void cont_o_d_inner(ContO *co, struct Graphics2D *g);

void cont_o_d(ContO *co, struct Graphics2D *g) {
  Medium *m = co->m;
  m->ofxz = co->fxz, m->ofxy = co->fxy, m->ofzy = co->fzy;
  cont_o_d_inner(co, g);
  m->ofxz = m->ofxy = m->ofzy = 0.0f;
}

static void cont_o_d_inner(ContO *co, struct Graphics2D *g) {
  Medium *m = co->m;
  g->objCalls++;
  if (co->dist != 0) co->dist = 0;

  // Rotate (x-mx, z-mz) by xz around (cx,cz) -- ports the JS's two
  // "this.m.cos(this.m.xz)"/"this.m.sin(this.m.xz)" expressions, which are
  // exactly medium_rot's own formula (see native/tests/medium_test.c /
  // groundpolys for the same rotate-then-add-center idiom).
  int32_t rx[1] = { co->x - m->x };
  int32_t rz[1] = { co->z - m->z };
  medium_rot(m, rx, rz, m->cx, m->cz, m->xz + m->fxz, 1);
  int32_t n = rx[0];
  int32_t n2 = rz[0];

  // Rotate (y-my, n2) by zy around (cy,cz). This single call produces BOTH
  // n3 (used immediately below) and n8 (the JS recomputes n8 separately
  // later at its line 1255, but m/n2/zy don't change in between, so
  // hoisting both out of one rotation is the same result, not a shortcut).
  int32_t ry[1] = { co->y - m->y };
  int32_t rz2[1] = { n2 };
  medium_rot(m, ry, rz2, m->cy, m->cz, m->zy + m->fzy, 1);
  int32_t n8 = ry[0];
  int32_t n3 = rz2[0];

  int32_t n4 = cont_o_xs(co, n + co->maxR, n3) - cont_o_xs(co, n - co->maxR, n3);
  if (cont_o_xs(co, n + co->maxR * 2, n3) > m->iw &&
      cont_o_xs(co, n - co->maxR * 2, n3) < m->w &&
      n3 > -co->maxR &&
      (n3 < m->fade[co->disline] + co->maxR || m->trk != 0) &&
      (n4 > co->disp || m->trk != 0) &&
      (!co->decor || (m->resdown != 2 && m->trk != 1))) {
    g->objDrawn++;
    const GfxMark shadow_mark = gfx_mark(g);
    if (co->shadow) {
      if (!m->crs) {
        if (n3 < 2000) {
          bool b = false;
          if (co->t->ncx != 0 || co->t->ncz != 0) {
            int32_t ncx = (co->x - co->t->sx) / 3000;
            if (ncx > co->t->ncx) ncx = co->t->ncx;
            if (ncx < 0) ncx = 0;
            int32_t ncz = (co->z - co->t->sz) / 3000;
            if (ncz > co->t->ncz) ncz = co->t->ncz;
            if (ncz < 0) ncz = 0;
            for (int32_t i = co->t->sect_len[ncx][ncz] - 1; i >= 0; i--) {
              int32_t n5 = co->t->sect[ncx][ncz][i];
              if (abs(co->t->zy[n5]) != 90 && abs(co->t->xy[n5]) != 90 &&
                  abs(co->x - co->t->x[n5]) < co->t->radx[n5] + co->maxR &&
                  abs(co->z - co->t->z[n5]) < co->t->radz[n5] + co->maxR &&
                  (!co->t->decor[n5] || m->resdown != 2)) {
                b = true;
                break;
              }
            }
          }
          if (b) {
            for (int32_t j = 0; j < co->npl; j++) {
              plane_s(&co->p[j], g, co->x - m->x, co->y - m->y, co->z - m->z, co->xz, co->xy, co->zy, 0);
            }
          } else {
            int32_t gy[1] = { m->ground };
            int32_t gz[1] = { n2 };
            medium_rot(m, gy, gz, m->cy, m->cz, m->zy + m->fzy, 1);
            int32_t n6 = gy[0];
            int32_t n7 = gz[0];
            if (cont_o_ys(co, n6 + co->maxR, n7) > 0 && cont_o_ys(co, n6 - co->maxR, n7) < m->h) {
              for (int32_t k = 0; k < co->npl; k++) {
                plane_s(&co->p[k], g, co->x - m->x, co->y - m->y, co->z - m->z, co->xz, co->xy, co->zy, 1);
              }
            }
          }
          // fr(maxR*0.8), single op -- native float is exact.
          medium_addsp(m, co->x - m->x, co->z - m->z, jtrunc((float)co->maxR * 0.8f));
        } else {
          cont_o_lowshadow(co, g, n3);
        }
      } else {
        for (int32_t l = 0; l < co->npl; l++) {
          plane_s(&co->p[l], g, co->x - m->x, co->y - m->y, co->z - m->z, co->xz, co->xy, co->zy, 2);
        }
      }
    }
    if (!cont_o_shadows) gfx_rewind(g, shadow_mark);   // Settings > Shadows off

    int32_t n8_center = m->cy + n8; // JS: n8 = this.m.cy + trunc(...)
    if (cont_o_ys(co, n8_center + co->maxR, n3) > m->ih && cont_o_ys(co, n8_center - co->maxR, n3) < m->h) {
      if (co->elec && m->noelec == 0) cont_o_electrify(co, g);
      if (co->fix) cont_o_fixit(co, g);
      if (co->checkpoint != 0 && co->checkpoint - 1 == m->checkpoint) n4 = -1;

      if (co->shadow) {
        // Bytecode showed iadd for sum of squares; -fwrapv int32 arithmetic
        // matches i32(Math.imul(...)+Math.imul(...)+Math.imul(...)) exactly
        // (mod-2^32 wraparound is associative regardless of when it's
        // applied) -- same idiom as plane_spy/plane.c's p->av. No fr() at
        // all on the sqrt, genuinely double, truncated once via jtrunc_d.
        int32_t dx0 = m->x + m->cx - co->x;
        int32_t dz0 = m->z - co->z;
        int32_t dy0 = m->y + m->cy - co->y;
        int32_t sumsq0 = dx0 * dx0 + dz0 * dz0 + dy0 * dy0;
        co->dist = jtrunc_d(sqrt((double)sumsq0));
        const GfxMark dust_mark = gfx_mark(g);
        for (int32_t n9 = 0; n9 < 20; n9++) {
          if (co->stg[n9] != 0) cont_o_pdust(co, n9, g, true);
        }
        cont_o_dsprk(co, g, true);
        if (!cont_o_particles) gfx_rewind(g, dust_mark);   // Settings > Particles off
      }

      // Back-to-front face order. The source ranks every face against
      // every other (O(npl^2), ~8.5k comparisons for a 131-face car, ~8%
      // of a race frame), and that ranking is exactly a STABLE sort by av,
      // descending: a face moves behind every face with a greater av, and
      // of two equal av the lower index draws first (the inner loop always
      // has n11 < n12, so a tie credits the later index). web/ContO.js made
      // the same replacement, with a test that draws a real scene both ways
      // and compares every vertex. This is a stable bottom-up merge sort
      // computing that identical permutation in O(npl log npl).
      int32_t sort_order[CONT_O_MAX_PLANES];
      int32_t sort_tmp[CONT_O_MAX_PLANES];
      int32_t sort_av[CONT_O_MAX_PLANES];
      for (int32_t i = 0; i < co->npl; i++) {
        sort_order[i] = i;
        sort_av[i] = co->p[i].av;
      }
      {
        int32_t *src = sort_order, *dst = sort_tmp;
        for (int32_t width = 1; width < co->npl; width *= 2) {
          for (int32_t lo = 0; lo < co->npl; lo += 2 * width) {
            int32_t mid = lo + width < co->npl ? lo + width : co->npl;
            int32_t hi = lo + 2 * width < co->npl ? lo + 2 * width : co->npl;
            int32_t a = lo, b = mid, k = lo;
            // `>=` keeps the left run's face first on equal av: stability.
            while (a < mid && b < hi) dst[k++] = sort_av[src[a]] >= sort_av[src[b]] ? src[a++] : src[b++];
            while (a < mid) dst[k++] = src[a++];
            while (b < hi) dst[k++] = src[b++];
          }
          int32_t *swap = src; src = dst; dst = swap;
        }
        if (src != sort_order) memcpy(sort_order, src, sizeof(int32_t) * (size_t)co->npl);
      }
      for (int32_t n17 = 0; n17 < co->npl; n17++) {
        Plane *pl = &co->p[sort_order[n17]];
        plane_d(pl, g, co->x - m->x, co->y - m->y, co->z - m->z, co->xz, co->xy, co->zy,
                co->wxz, co->wzy, co->noline, n4);
      }

      if (co->shadow) {
        const GfxMark dust_mark = gfx_mark(g);
        for (int32_t n18 = 0; n18 < 20; n18++) {
          if (co->stg[n18] != 0) cont_o_pdust(co, n18, g, false);
        }
        cont_o_dsprk(co, g, false);
        if (!cont_o_particles) gfx_rewind(g, dust_mark);
      }

      // Unconditional on co->shadow -- the JS's second `this.dist = ...`
      // (its lines 1320-1325) sits after the `if (this.shadow) {
      // pdust/dsprk }` block closes, still inside the outer ys-visibility
      // `if`, so it always overwrites the shadow-gated dist set above.
      int32_t dx1 = m->x + m->cx - co->x;
      int32_t dz1 = m->z - co->z;
      int32_t dy1 = m->y + m->cy - co->y;
      int32_t sumsq1 = dx1 * dx1 + dz1 * dz1 + dy1 * dy1;
      int32_t d0 = jtrunc_d(sqrt((double)sumsq1));
      // "* this.grounded" is plain JS number arithmetic (int * float), no
      // fr() -- double all the way through, one jtrunc_d at the end.
      co->dist = jtrunc_d(sqrt((double)d0 * (double)co->grounded));
    }
  }

  if (co->shadow && co->dist == 0) {
    for (int32_t n19 = 0; n19 < 20; n19++) {
      if (co->stg[n19] != 0) co->stg[n19] = 0;
    }
    for (int32_t n20 = 0; n20 < 100; n20++) {
      if (co->rtg[n20] != 0) co->rtg[n20] = 0;
    }
    if (co->sprk_ != 0) co->sprk_ = 0;
  }
}

void cont_o_init_buf(ContO *co, const char *text, Medium *m, Trackers *t) {
  memset(co, 0, sizeof(*co));
  co->disline = 14;
  co->grounded = 1.0f;
  co->m = m;
  co->t = t;
  co->baseIndex = -1;
  // CONT_O_MAX_PLANES-capacity scratch buffer -- npl isn't known until
  // parsing finishes, so this over-allocates during parsing and is shrunk
  // to exactly npl with realloc() below, once it is. See cont_o.h's field
  // comment on `p` for why this matters (610 stage-placed objects at a
  // fixed 286 slots each would be ~42MB).
  co->p = malloc(CONT_O_MAX_PLANES * sizeof(Plane));

  if (m->loadnew) {
    for (int32_t j = 0; j < 4; j++) co->keyz[j] = 0;
    co->shadow = true;
  }

  int32_t n = 0, n2 = 0, n3 = 0;
  float n4 = 1.0f, n5 = 1.0f;
  float array2[3] = {1.0f, 1.0f, 1.0f};
  int32_t array3[100], array4[100], array5[100];
  int32_t array6[3] = {0, 0, 0};
  // JS: `array` (intArray(286)) -- 1 if a face had no explicit fs(), 2 if
  // it did. Only faces marked 1 get auto-oriented by the loadnew
  // post-processing pass below; consumed there, written in the parse loop.
  int32_t face_flag[CONT_O_MAX_PLANES];
  memset(face_flag, 0, sizeof(face_flag));
  bool b = false;
  Wheels wheels; wheels_init(&wheels);
  bool b2 = false;
  int32_t n6 = 0;
  int32_t getvalue1 = 1, getvalue2 = 0, getvalue3 = 0;
  int32_t n7 = 0, n8 = 0;
  bool b3 = false;
  int32_t n9 = 0;

  char *text_copy = malloc(strlen(text) + 1);
  strcpy(text_copy, text);
  char **lines;
  int32_t line_count;
  vfs_read_lines(text_copy, &lines, &line_count);
  free(text_copy);

  char line_buf[2048];
  for (int32_t li = 0; li < line_count; li++) {
    size_t src_len = strlen(lines[li]);
    size_t copy_len = src_len < sizeof(line_buf) - 1 ? src_len : sizeof(line_buf) - 1;
    memcpy(line_buf, lines[li], copy_len);
    line_buf[copy_len] = '\0';
    char *string = trim_line(line_buf);

    if (co->npl < CONT_O_MAX_POLYS) {
      if (starts_with(string, "<p>")) {
        n = 1;
        n3 = 0;
        getvalue1 = 0;
        getvalue2 = 0;
        n7 = 0;
        face_flag[co->npl] = 1;
        if (n9 == 0) b3 = false;
      }
      if (n != 0) {
        if (starts_with(string, "gr(")) {
          getvalue1 = cont_o_getvalue("gr", string, 0);
        }
        if (starts_with(string, "fs(")) {
          getvalue2 = cont_o_getvalue("fs", string, 0);
          face_flag[co->npl] = 2;
        }
        if (starts_with(string, "c(")) {
          n8 = 0;
          array6[0] = cont_o_getvalue("c", string, 0);
          array6[1] = cont_o_getvalue("c", string, 1);
          array6[2] = cont_o_getvalue("c", string, 2);
        }
        if (starts_with(string, "glass")) {
          n8 = 1;
        }
        if (starts_with(string, "gshadow")) {
          n8 = 2;
        }
        if (starts_with(string, "lightF")) {
          n7 = 1;
        }
        if (starts_with(string, "light")) {
          n7 = 1;
        }
        if (starts_with(string, "lightB")) {
          n7 = 2;
        }
        if (starts_with(string, "noOutline")) {
          b3 = true;
        }
        // Java's int[100] throws past the hundredth point and ContO's catch
        // drops the rest of the model; C would write past the stack arrays.
        if (starts_with(string, "p(") && n3 < 100) {
          // trunc(fr(fr(getvalue*n4)*n5*array2[0])) -- the OUTER fr() wraps
          // a MULTI-op expression (innerFr * n5 * array2[0], two chained
          // multiplies with no fr() splitting them) -- case 2, needs
          // double precision, NOT native float chaining.
          float v0 = (float)cont_o_getvalue("p", string, 0);
          float inner0 = v0 * n4; // fr(getvalue*n4), single op, float fine
          double outer0 = (double)inner0 * (double)n5 * (double)array2[0];
          array3[n3] = jtrunc((float)outer0);
          // trunc(fr(getvalue*n4)*array2[1]) -- fr() wraps ONLY the first
          // multiply; the second multiply by array2[1] is unwrapped, so
          // this needs double precision (case 3-like: only part of the
          // expression is float-rounded).
          float v1 = (float)cont_o_getvalue("p", string, 1);
          array4[n3] = jtrunc_d((double)(v1 * n4) * (double)array2[1]);
          float v2 = (float)cont_o_getvalue("p", string, 2);
          array5[n3] = jtrunc_d((double)(v2 * n4) * (double)array2[2]);
          int32_t maxR = jtrunc_d(sqrt((double)array3[n3] * array3[n3] +
                                       (double)array4[n3] * array4[n3] +
                                       (double)array5[n3] * array5[n3]));
          if (maxR > co->maxR) co->maxR = maxR;
          n3++;
        }
      }
      if (starts_with(string, "</p>")) {
        plane_init(&co->p[co->npl], m, t, array3, array5, array4, n3, array6,
                   n8, getvalue1, getvalue2, 0, 0, 0, co->disline, 0, b, n7, b3);
        if (array6[0] == co->fcol[0] && array6[1] == co->fcol[1] && array6[2] == co->fcol[2] && n8 == 0) {
          co->p[co->npl].colnum = 1;
        }
        if (array6[0] == co->scol[0] && array6[1] == co->scol[1] && array6[2] == co->scol[2] && n8 == 0) {
          co->p[co->npl].colnum = 2;
        }
        co->npl++;
        n = 0;
      }
    }

    if (starts_with(string, "rims(")) {
      wheels_set_rims(&wheels, cont_o_getvalue("rims", string, 0), cont_o_getvalue("rims", string, 1),
                       cont_o_getvalue("rims", string, 2), cont_o_getvalue("rims", string, 3),
                       cont_o_getvalue("rims", string, 4));
    }
    if (starts_with(string, "w(") && n6 < 4) {
      // trunc(fr(getvalue*n4)*array2[0]) -- fr() wraps only the first
      // multiply; unwrapped second multiply, case-3 double precision.
      float w0 = (float)cont_o_getvalue("w", string, 0);
      co->keyx[n6] = jtrunc_d((double)(w0 * n4) * (double)array2[0]);
      float w2 = (float)cont_o_getvalue("w", string, 2);
      co->keyz[n6] = jtrunc_d((double)(w2 * n4) * (double)array2[2]);
      // trunc(fr(fr(getvalue*n4)*n5*array2[0])) -- same multi-op-under-
      // outer-fr shape as the p() handler above: double precision, once.
      float w0b = (float)cont_o_getvalue("w", string, 0);
      float wx_inner = w0b * n4;
      double wx_outer = (double)wx_inner * (double)n5 * (double)array2[0];
      int32_t wx_arg = jtrunc((float)wx_outer);
      float w1 = (float)cont_o_getvalue("w", string, 1);
      int32_t wy_arg = jtrunc_d((double)(w1 * n4) * (double)array2[1]);
      float w2b = (float)cont_o_getvalue("w", string, 2);
      int32_t wz_arg = jtrunc_d((double)(w2b * n4) * (double)array2[2]);
      int32_t w3_arg = cont_o_getvalue("w", string, 3);
      float w4 = (float)cont_o_getvalue("w", string, 4);
      int32_t w4_arg = jtrunc((w4 * n4) * n5);
      float w5 = (float)cont_o_getvalue("w", string, 5);
      int32_t w5_arg = jtrunc_d((double)w5 * (double)n4);
      wheels_make(&wheels, m, t, co->p, co->npl, wx_arg, wy_arg, wz_arg, w3_arg, w4_arg, w5_arg, getvalue3);
      co->npl += 19;
      if (m->loadnew) {
        // Bytecode showed f2i before iadd (Case B explicit cast):
        // this.wh += (int)(this.getvalue("w", string, 5) * n4)
        float w5c = (float)cont_o_getvalue("w", string, 5);
        co->wh += jtrunc_d((double)w5c * (double)n4);
        if (wheels.ground > 140) {
          co->errd = true;
          co->keyz[n6] = 0;
          co->keyx[n6] = 0;
        }
        if (wheels.ground < -100) {
          co->errd = true;
          co->keyz[n6] = 0;
          co->keyx[n6] = 0;
        }
        if (abs(co->keyx[n6]) > 400) {
          co->errd = true;
          co->keyz[n6] = 0;
          co->keyx[n6] = 0;
        }
        if (abs(co->keyz[n6]) > 700) {
          co->errd = true;
          co->keyz[n6] = 0;
          co->keyx[n6] = 0;
        }
        float w4c = (float)cont_o_getvalue("w", string, 4);
        if (jtrunc((w4c * n4) * n5) > 300) {
          co->errd = true;
          co->keyz[n6] = 0;
          co->keyx[n6] = 0;
        }
      }
      n6++;
    }

    if (starts_with(string, "tracks")) {
      int32_t count = cont_o_getvalue("tracks", string, 0);
      co->txy = calloc((size_t)count, sizeof(int32_t));
      co->tzy = calloc((size_t)count, sizeof(int32_t));
      co->tc = calloc((size_t)count, sizeof(int32_t[3]));
      co->tradx = calloc((size_t)count, sizeof(int32_t));
      co->tradz = calloc((size_t)count, sizeof(int32_t));
      co->trady = calloc((size_t)count, sizeof(int32_t));
      co->tx = calloc((size_t)count, sizeof(int32_t));
      co->ty = calloc((size_t)count, sizeof(int32_t));
      co->tz = calloc((size_t)count, sizeof(int32_t));
      co->skd = calloc((size_t)count, sizeof(int32_t));
      co->dam = calloc((size_t)count, sizeof(int32_t));
      co->notwall = calloc((size_t)count, sizeof(bool));
      co->tnt_cap = count;
      b2 = true;
    }
    if (b2) {
      if (starts_with(string, "<track>")) {
        n2 = 1;
        co->notwall[co->tnt] = false;
        co->dam[co->tnt] = 1;
        co->skd[co->tnt] = 0;
        co->ty[co->tnt] = 0;
        co->tx[co->tnt] = 0;
        co->tz[co->tnt] = 0;
        co->txy[co->tnt] = 0;
        co->tzy[co->tnt] = 0;
        co->trady[co->tnt] = 0;
        co->tradx[co->tnt] = 0;
        co->tradz[co->tnt] = 0;
        co->tc[co->tnt][0] = 0;
        co->tc[co->tnt][1] = 0;
        co->tc[co->tnt][2] = 0;
      }
      if (n2 != 0) {
        if (starts_with(string, "c")) {
          co->tc[co->tnt][0] = cont_o_getvalue("c", string, 0);
          co->tc[co->tnt][1] = cont_o_getvalue("c", string, 1);
          co->tc[co->tnt][2] = cont_o_getvalue("c", string, 2);
        }
        if (starts_with(string, "xy")) {
          co->txy[co->tnt] = cont_o_getvalue("xy", string, 0);
        }
        if (starts_with(string, "zy")) {
          co->tzy[co->tnt] = cont_o_getvalue("zy", string, 0);
        }
        if (starts_with(string, "radx")) {
          co->tradx[co->tnt] = jtrunc_d((double)cont_o_getvalue("radx", string, 0) * (double)n4);
        }
        if (starts_with(string, "rady")) {
          co->trady[co->tnt] = jtrunc_d((double)cont_o_getvalue("rady", string, 0) * (double)n4);
        }
        if (starts_with(string, "radz")) {
          co->tradz[co->tnt] = jtrunc_d((double)cont_o_getvalue("radz", string, 0) * (double)n4);
        }
        if (starts_with(string, "ty")) {
          co->ty[co->tnt] = jtrunc_d((double)cont_o_getvalue("ty", string, 0) * (double)n4);
        }
        if (starts_with(string, "tx")) {
          co->tx[co->tnt] = jtrunc_d((double)cont_o_getvalue("tx", string, 0) * (double)n4);
        }
        if (starts_with(string, "tz")) {
          co->tz[co->tnt] = jtrunc_d((double)cont_o_getvalue("tz", string, 0) * (double)n4);
        }
        if (starts_with(string, "skid")) {
          co->skd[co->tnt] = cont_o_getvalue("skid", string, 0);
        }
        if (starts_with(string, "dam")) {
          co->dam[co->tnt] = 3;
        }
        if (starts_with(string, "notwall")) {
          co->notwall[co->tnt] = true;
        }
      }
      if (starts_with(string, "</track>")) {
        n2 = 0;
        co->tnt++;
      }
    }

    if (starts_with(string, "disp(")) co->disp = cont_o_getvalue("disp", string, 0);
    if (starts_with(string, "disline(")) co->disline = cont_o_getvalue("disline", string, 0) * 2;
    if (starts_with(string, "shadow")) co->shadow = true;
    if (starts_with(string, "stonecold")) co->noline = true;
    if (starts_with(string, "newstone")) {
      co->noline = true;
      b3 = true;
      n9 = 1;
    }
    if (starts_with(string, "decorative")) co->decor = true;
    if (starts_with(string, "road")) b = true;
    if (starts_with(string, "notroad")) b = false;
    if (starts_with(string, "grounded(")) {
      co->grounded = (float)cont_o_getvalue("grounded", string, 0) / 100.0f;
    }
    if (starts_with(string, "div(")) {
      n4 = (float)cont_o_getvalue("div", string, 0) / 10.0f;
    }
    if (starts_with(string, "idiv(")) {
      n4 = (float)cont_o_getvalue("idiv", string, 0) / 100.0f;
    }
    if (starts_with(string, "iwid(")) {
      n5 = (float)cont_o_getvalue("iwid", string, 0) / 100.0f;
    }
    if (starts_with(string, "ScaleX(")) {
      array2[0] = (float)cont_o_getvalue("ScaleX", string, 0) / 100.0f;
    }
    if (starts_with(string, "ScaleY(")) {
      array2[1] = (float)cont_o_getvalue("ScaleY", string, 0) / 100.0f;
    }
    if (starts_with(string, "ScaleZ(")) {
      array2[2] = (float)cont_o_getvalue("ScaleZ", string, 0) / 100.0f;
    }
    if (starts_with(string, "gwgr(")) {
      getvalue3 = cont_o_getvalue("gwgr", string, 0);
      if (m->loadnew) {
        if (getvalue3 > 40) getvalue3 = 40;
        if (getvalue3 < 0 && getvalue3 >= -15) getvalue3 = -16;
        if (getvalue3 < -40) getvalue3 = -40;
      }
    }
    if (starts_with(string, "1stColor(")) {
      co->fcol[0] = cont_o_getvalue("1stColor", string, 0);
      co->fcol[1] = cont_o_getvalue("1stColor", string, 1);
      co->fcol[2] = cont_o_getvalue("1stColor", string, 2);
      co->colok++;
    }
    if (starts_with(string, "2ndColor(")) {
      co->scol[0] = cont_o_getvalue("2ndColor", string, 0);
      co->scol[1] = cont_o_getvalue("2ndColor", string, 1);
      co->scol[2] = cont_o_getvalue("2ndColor", string, 2);
      co->colok++;
    }
  }

  vfs_free_lines(lines, line_count);

  co->grat = wheels.ground;
  co->sprkat = wheels.sparkat;
  if (co->shadow) {
    co->stg = calloc(20, sizeof(int32_t));
    co->rtg = calloc(100, sizeof(int32_t));
  }
  if (m->loadnew) {
    if (n6 != 0) {
      co->wh = co->wh / n6;
    }
    // Per-face bounding box, "thin axis" classification (n12: 1=x, 2=y,
    // 3=z), and -- for faces with no explicit fs() (face_flag==1) -- the
    // fs (decal/glass flip-side) flag, resolved by comparing against
    // neighbouring faces that share an edge. Ports web/ContO.js lines
    // 469-829 verbatim; every angle computation here uses Math.atan/
    // Math.abs with NO fr() at all in the JS (genuinely double, truncated
    // once via jtrunc_d), unlike the parse loop above which is mostly
    // fr()-wrapped multiplies.
    int32_t n10 = 0;
    for (int32_t n11 = 0; n11 < co->npl; n11++) {
      Plane *pl11 = &co->p[n11];
      int32_t n12 = 0;
      int32_t n13 = pl11->ox[0], n14 = pl11->ox[0];
      int32_t n15 = pl11->oy[0], n16 = pl11->oy[0];
      int32_t n17 = pl11->oz[0], n18 = pl11->oz[0];
      for (int32_t n19 = 0; n19 < pl11->n; n19++) {
        if (pl11->ox[n19] > n13) n13 = pl11->ox[n19];
        if (pl11->ox[n19] < n14) n14 = pl11->ox[n19];
        if (pl11->oy[n19] > n15) n15 = pl11->oy[n19];
        if (pl11->oy[n19] < n16) n16 = pl11->oy[n19];
        if (pl11->oz[n19] > n17) n17 = pl11->oz[n19];
        if (pl11->oz[n19] < n18) n18 = pl11->oz[n19];
      }
      if (abs(n13 - n14) <= abs(n15 - n16) && abs(n13 - n14) <= abs(n17 - n18)) n12 = 1;
      if (abs(n15 - n16) <= abs(n13 - n14) && abs(n15 - n16) <= abs(n17 - n18)) n12 = 2;
      if (abs(n17 - n18) <= abs(n13 - n14) && abs(n17 - n18) <= abs(n15 - n16)) n12 = 3;
      if (n12 == 2 && (n10 == 0 || (n15 + n16) / 2 < co->roofat)) {
        co->roofat = (n15 + n16) / 2;
        n10 = 1;
      }

      if (face_flag[n11] == 1) {
        int32_t n20 = 1000, n21 = 0;
        for (int32_t n22 = 0; n22 < pl11->n; n22++) {
          int32_t n23 = n22 + 1;
          if (n23 >= pl11->n) n23 -= pl11->n;
          int32_t n24 = n22 + 2;
          if (n24 >= pl11->n) n24 -= pl11->n;
          if (n12 == 1) {
            int32_t abs1 = abs(jtrunc_d(atan((double)(pl11->oz[n22] - pl11->oz[n23]) / (double)(pl11->oy[n22] - pl11->oy[n23])) / 0.017453292519943295));
            int32_t abs2 = abs(jtrunc_d(atan((double)(pl11->oz[n24] - pl11->oz[n23]) / (double)(pl11->oy[n24] - pl11->oy[n23])) / 0.017453292519943295));
            if (abs1 > 45) abs1 = 90 - abs1; else abs2 = 90 - abs2;
            if (abs1 + abs2 < n20) { n20 = abs1 + abs2; n21 = n22; }
          }
          if (n12 == 2) {
            int32_t abs3 = abs(jtrunc_d(atan((double)(pl11->oz[n22] - pl11->oz[n23]) / (double)(pl11->ox[n22] - pl11->ox[n23])) / 0.017453292519943295));
            int32_t abs4 = abs(jtrunc_d(atan((double)(pl11->oz[n24] - pl11->oz[n23]) / (double)(pl11->ox[n24] - pl11->ox[n23])) / 0.017453292519943295));
            if (abs3 > 45) abs3 = 90 - abs3; else abs4 = 90 - abs4;
            if (abs3 + abs4 < n20) { n20 = abs3 + abs4; n21 = n22; }
          }
          if (n12 == 3) {
            int32_t abs5 = abs(jtrunc_d(atan((double)(pl11->oy[n22] - pl11->oy[n23]) / (double)(pl11->ox[n22] - pl11->ox[n23])) / 0.017453292519943295));
            int32_t abs6 = abs(jtrunc_d(atan((double)(pl11->oy[n24] - pl11->oy[n23]) / (double)(pl11->ox[n24] - pl11->ox[n23])) / 0.017453292519943295));
            if (abs5 > 45) abs5 = 90 - abs5; else abs6 = 90 - abs6;
            if (abs5 + abs6 < n20) { n20 = abs5 + abs6; n21 = n22; }
          }
        }
        if (n21 != 0) {
          int32_t array7[100], array8[100], array9[100];
          for (int32_t n25 = 0; n25 < pl11->n; n25++) {
            array7[n25] = pl11->ox[n25];
            array8[n25] = pl11->oy[n25];
            array9[n25] = pl11->oz[n25];
          }
          for (int32_t n26 = 0; n26 < pl11->n; n26++) {
            int32_t n27 = n26 + n21;
            if (n27 >= pl11->n) n27 -= pl11->n;
            pl11->ox[n26] = array7[n27];
            pl11->oy[n26] = array8[n27];
            pl11->oz[n26] = array9[n27];
          }
        }
        if (n12 == 1) {
          if (abs(pl11->oz[0] - pl11->oz[1]) > abs(pl11->oy[0] - pl11->oy[1])) {
            if (pl11->oz[0] > pl11->oz[1]) {
              pl11->fs = (pl11->oy[1] > pl11->oy[2]) ? 1 : -1;
            } else {
              pl11->fs = (pl11->oy[1] > pl11->oy[2]) ? -1 : 1;
            }
          } else if (pl11->oy[0] > pl11->oy[1]) {
            pl11->fs = (pl11->oz[1] > pl11->oz[2]) ? -1 : 1;
          } else {
            pl11->fs = (pl11->oz[1] > pl11->oz[2]) ? 1 : -1;
          }
        }
        if (n12 == 2) {
          if (abs(pl11->oz[0] - pl11->oz[1]) > abs(pl11->ox[0] - pl11->ox[1])) {
            if (pl11->oz[0] > pl11->oz[1]) {
              pl11->fs = (pl11->ox[1] > pl11->ox[2]) ? -1 : 1;
            } else {
              pl11->fs = (pl11->ox[1] > pl11->ox[2]) ? 1 : -1;
            }
          } else if (pl11->ox[0] > pl11->ox[1]) {
            pl11->fs = (pl11->oz[1] > pl11->oz[2]) ? 1 : -1;
          } else {
            pl11->fs = (pl11->oz[1] > pl11->oz[2]) ? -1 : 1;
          }
        }
        if (n12 == 3) {
          if (abs(pl11->oy[0] - pl11->oy[1]) > abs(pl11->ox[0] - pl11->ox[1])) {
            if (pl11->oy[0] > pl11->oy[1]) {
              pl11->fs = (pl11->ox[1] > pl11->ox[2]) ? 1 : -1;
            } else {
              pl11->fs = (pl11->ox[1] > pl11->ox[2]) ? -1 : 1;
            }
          } else if (pl11->ox[0] > pl11->ox[1]) {
            pl11->fs = (pl11->oy[1] > pl11->oy[2]) ? -1 : 1;
          } else {
            pl11->fs = (pl11->oy[1] > pl11->oy[2]) ? 1 : -1;
          }
        }

        bool b4 = false, b5 = false;
        for (int32_t n28 = 0; n28 < co->npl; n28++) {
          if (n28 != n11 && face_flag[n28] != 0) {
            Plane *pl28 = &co->p[n28];
            int32_t n29 = pl28->ox[0], n30 = pl28->ox[0];
            int32_t n31 = pl28->oy[0], n32 = pl28->oy[0];
            int32_t n33 = pl28->oz[0], n34 = pl28->oz[0];
            for (int32_t n35 = 0; n35 < pl28->n; n35++) {
              if (pl28->ox[n35] > n29) n29 = pl28->ox[n35];
              if (pl28->ox[n35] < n30) n30 = pl28->ox[n35];
              if (pl28->oy[n35] > n31) n31 = pl28->oy[n35];
              if (pl28->oy[n35] < n32) n32 = pl28->oy[n35];
              if (pl28->oz[n35] > n33) n33 = pl28->oz[n35];
              if (pl28->oz[n35] < n34) n34 = pl28->oz[n35];
            }
            int32_t n36 = (n29 + n30) / 2;
            int32_t n37 = (n31 + n32) / 2;
            int32_t n38 = (n33 + n34) / 2;
            int32_t n39 = (n13 + n14) / 2;
            int32_t n40 = (n15 + n16) / 2;
            int32_t n41 = (n17 + n18) / 2;
            if (n12 == 1 && ((n37 <= n15 && n37 >= n16 && n38 <= n17 && n38 >= n18) ||
                             (n40 <= n31 && n40 >= n32 && n41 <= n33 && n41 >= n34))) {
              if (n29 < n14) b4 = true;
              if (n30 > n13) b5 = true;
            }
            if (n12 == 2 && ((n36 <= n13 && n36 >= n14 && n38 <= n17 && n38 >= n18) ||
                             (n39 <= n29 && n39 >= n30 && n41 <= n33 && n41 >= n34))) {
              if (n31 < n16) b4 = true;
              if (n32 > n15) b5 = true;
            }
            if (n12 == 3 && ((n36 <= n13 && n36 >= n14 && n37 <= n15 && n37 >= n16) ||
                             (n39 <= n29 && n39 >= n30 && n40 <= n31 && n40 >= n32))) {
              if (n33 < n18) b4 = true;
              if (n34 > n17) b5 = true;
            }
          }
          if (b4 && b5) break;
        }
        bool b6 = false;
        if (b4 && !b5) {
          b6 = true;
        }
        if (b5 && !b4) {
          pl11->fs *= -1;
          b6 = true;
        }
        if (b4 && b5) {
          pl11->fs = 0;
          pl11->gr = 40;
          b6 = true;
        }
        if (!b6) {
          int32_t n42 = 0, n43 = 0;
          if (n12 == 1) { n42 = n43 = (n13 + n14) / 2; }
          if (n12 == 2) { n42 = n43 = (n15 + n16) / 2; }
          if (n12 == 3) { n42 = n43 = (n17 + n18) / 2; }
          for (int32_t n44 = 0; n44 < co->npl; n44++) {
            if (n44 != n11) {
              Plane *pl44 = &co->p[n44];
              bool b7 = false;
              bool array10[100];
              for (int32_t n45 = 0; n45 < pl44->n; n45++) {
                array10[n45] = false;
                for (int32_t n46 = 0; n46 < pl11->n; n46++) {
                  if (pl11->ox[n46] == pl44->ox[n45] && pl11->oy[n46] == pl44->oy[n45] && pl11->oz[n46] == pl44->oz[n45]) {
                    array10[n45] = true;
                    b7 = true;
                  }
                }
              }
              if (b7) {
                for (int32_t n47 = 0; n47 < pl44->n; n47++) {
                  if (!array10[n47]) {
                    if (n12 == 1) {
                      if (pl44->ox[n47] > n42) n42 = pl44->ox[n47];
                      if (pl44->ox[n47] < n43) n43 = pl44->ox[n47];
                    }
                    if (n12 == 2) {
                      if (pl44->oy[n47] > n42) n42 = pl44->oy[n47];
                      if (pl44->oy[n47] < n43) n43 = pl44->oy[n47];
                    }
                    if (n12 == 3) {
                      if (pl44->oz[n47] > n42) n42 = pl44->oz[n47];
                      if (pl44->oz[n47] < n43) n43 = pl44->oz[n47];
                    }
                  }
                }
              }
            }
          }
          if (n12 == 1) {
            if ((n42 + n43) / 2 > (n13 + n14) / 2) {
              pl11->fs *= -1;
            } else if ((n42 + n43) / 2 == (n13 + n14) / 2 && (n13 + n14) / 2 < 0) {
              pl11->fs *= -1;
            }
          }
          if (n12 == 2) {
            if ((n42 + n43) / 2 > (n15 + n16) / 2) {
              pl11->fs *= -1;
            } else if ((n42 + n43) / 2 == (n15 + n16) / 2 && (n15 + n16) / 2 < 0) {
              pl11->fs *= -1;
            }
          }
          if (n12 == 3) {
            if ((n42 + n43) / 2 > (n17 + n18) / 2) {
              pl11->fs *= -1;
            } else if ((n42 + n43) / 2 == (n17 + n18) / 2 && (n17 + n18) / 2 < 0) {
              pl11->fs *= -1;
            }
          }
        }
        plane_deltafntyp(pl11);
      }
    }
  }

  // Shrink the CONT_O_MAX_PLANES scratch buffer down to exactly npl -- see
  // cont_o.h's field comment on `p`. Shrinking realloc() never moves data
  // out from under the pointers used above (they're all local to this
  // function and done being used by now).
  if (co->npl > 0) {
    co->p = realloc(co->p, (size_t)co->npl * sizeof(Plane));
  } else {
    free(co->p);
    co->p = NULL;
  }
}
