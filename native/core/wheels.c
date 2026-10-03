// ports web/Wheels.js
//
// Same fr()-translation rules as native/core/medium.c/plane.c. Per the JS
// file's own header comment: `K * this.size` sites (K a literal like 8.66)
// use DOUBLE arithmetic in the real bytecode -- no fr() wraps them -- so
// those specific sites use jtrunc_d, not jtrunc. Everything else here is
// `trunc(fr(base +/- fr(K * n1x)))`, a single-op fr() nested inside another
// single-op fr(), which native float chaining reproduces exactly.
#include "wheels.h"
#include "java_compat.h"

void wheels_init(Wheels *w) {
  w->ground = 0;
  w->mast = 0;
  w->sparkat = 0;
  w->rc[0] = 120; w->rc[1] = 120; w->rc[2] = 120;
  w->size = 2.0f;
  w->depth = 3.0f;
}

void wheels_set_rims(Wheels *w, int32_t n, int32_t n2, int32_t n3, int32_t n4, int32_t n5) {
  w->rc[0] = n;
  w->rc[1] = n2;
  w->rc[2] = n3;
  w->size = (float)n4 / 10.0f;
  if (w->size < 0.0f) w->size = 0.0f;
  w->depth = (float)n5 / 10.0f;
  if (w->depth / w->size > 41.0f) w->depth = w->size * 41.0f;
  if (w->depth / w->size < -25.0f) w->depth = -(w->size * 25.0f);
}

void wheels_make(Wheels *w, Medium *m, Trackers *t, Plane *array, int32_t n,
                  int32_t n2, int32_t n3, int32_t n4, int32_t n5, int32_t n6,
                  int32_t n7, int32_t n8) {
  int32_t array2[20], array3[20], array4[20];
  int32_t array5[3] = {45, 45, 45};
  int32_t n9 = 0;
  float n10 = (float)n6 / 10.0f;
  float n11 = (float)n7 / 10.0f;
  if (n5 == 11) {
    n9 = jtrunc((float)n2 + (4.0f * n10));
  }
  w->sparkat = jtrunc(n11 * 24.0f);
  w->ground = jtrunc((float)n3 + (13.0f * n11));
  int32_t n12 = -1;
  if (n2 < 0) n12 = 1;
  for (int32_t i = 0; i < 20; i++) {
    array2[i] = jtrunc((float)n2 - (4.0f * n10));
  }
  array3[0] = jtrunc((float)n3 - (9.1923f * n11));
  array4[0] = jtrunc((float)n4 + (9.1923f * n11));
  array3[1] = jtrunc((float)n3 - (12.557f * n11));
  array4[1] = jtrunc((float)n4 + (3.3646f * n11));
  array3[2] = jtrunc((float)n3 - (12.557f * n11));
  array4[2] = jtrunc((float)n4 - (3.3646f * n11));
  array3[3] = jtrunc((float)n3 - (9.1923f * n11));
  array4[3] = jtrunc((float)n4 - (9.1923f * n11));
  array3[4] = jtrunc((float)n3 - (3.3646f * n11));
  array4[4] = jtrunc((float)n4 - (12.557f * n11));
  array3[5] = jtrunc((float)n3 + (3.3646f * n11));
  array4[5] = jtrunc((float)n4 - (12.557f * n11));
  array3[6] = jtrunc((float)n3 + (9.1923f * n11));
  array4[6] = jtrunc((float)n4 - (9.1923f * n11));
  array3[7] = jtrunc((float)n3 + (12.557f * n11));
  array4[7] = jtrunc((float)n4 - (3.3646f * n11));
  array3[8] = jtrunc((float)n3 + (12.557f * n11));
  array4[8] = jtrunc((float)n4 + (3.3646f * n11));
  array3[9] = jtrunc((float)n3 + (9.1923f * n11));
  array4[9] = jtrunc((float)n4 + (9.1923f * n11));
  array3[10] = jtrunc((float)n3 + (3.3646f * n11));
  array4[10] = jtrunc((float)n4 + (12.557f * n11));
  array3[11] = jtrunc((float)n3 - (3.3646f * n11));
  array4[11] = jtrunc((float)n4 + (12.557f * n11));
  array3[12] = n3;
  array4[12] = jtrunc((float)n4 + (10.0f * w->size));
  array3[13] = jtrunc_d((double)n3 + 8.66 * (double)w->size);
  array4[13] = jtrunc((float)n4 + (5.0f * w->size));
  array3[14] = jtrunc_d((double)n3 + 8.66 * (double)w->size);
  array4[14] = jtrunc((float)n4 - (5.0f * w->size));
  array3[15] = n3;
  array4[15] = jtrunc((float)n4 - (10.0f * w->size));
  array3[16] = jtrunc_d((double)n3 - 8.66 * (double)w->size);
  array4[16] = jtrunc((float)n4 - (5.0f * w->size));
  array3[17] = jtrunc_d((double)n3 - 8.66 * (double)w->size);
  array4[17] = jtrunc((float)n4 + (5.0f * w->size));
  array3[18] = n3;
  array4[18] = jtrunc((float)n4 + (10.0f * w->size));
  array3[19] = jtrunc((float)n3 - (3.3646f * n11));
  array4[19] = jtrunc((float)n4 + (12.557f * n11));
  plane_init(&array[n], m, t, array2, array4, array3, 20, array5, 0, n8, 0, n9, n3, n4, 7, 0, false, 0, false);
  array[n].master = 1;
  ++n;

  array2[2] = jtrunc((float)n2 - (w->depth * n10));
  array3[2] = n3;
  array4[2] = n4;
  int32_t n13 = jtrunc((float)n8 - ((w->depth / w->size) * 4.0f));
  if (n13 < -16) n13 = -16;

  array3[0] = n3;
  array4[0] = jtrunc((float)n4 + (10.0f * w->size));
  array3[1] = jtrunc_d((double)n3 + 8.66 * (double)w->size);
  array4[1] = jtrunc((float)n4 + (5.0f * w->size));
  plane_init(&array[n], m, t, array2, array4, array3, 3, w->rc, 0, n13, 0, n9, n3, n4, 7, 0, false, 0, false);
  if (w->depth / w->size < 7.0f) array[n].master = 2;
  ++n;

  array3[0] = jtrunc_d((double)n3 + 8.66 * (double)w->size);
  array4[0] = jtrunc((float)n4 + (5.0f * w->size));
  array3[1] = jtrunc_d((double)n3 + 8.66 * (double)w->size);
  array4[1] = jtrunc((float)n4 - (5.0f * w->size));
  plane_init(&array[n], m, t, array2, array4, array3, 3, w->rc, 0, n13, 0, n9, n3, n4, 7, 0, false, 0, false);
  if (w->depth / w->size < 7.0f) array[n].master = 2;
  ++n;

  array3[0] = jtrunc_d((double)n3 + 8.66 * (double)w->size);
  array4[0] = jtrunc((float)n4 - (5.0f * w->size));
  array3[1] = n3;
  array4[1] = jtrunc((float)n4 - (10.0f * w->size));
  plane_init(&array[n], m, t, array2, array4, array3, 3, w->rc, 0, n13, 0, n9, n3, n4, 7, 0, false, 0, false);
  if (w->depth / w->size < 7.0f) array[n].master = 2;
  ++n;

  array3[0] = n3;
  array4[0] = jtrunc((float)n4 - (10.0f * w->size));
  array3[1] = jtrunc_d((double)n3 - 8.66 * (double)w->size);
  array4[1] = jtrunc((float)n4 - (5.0f * w->size));
  plane_init(&array[n], m, t, array2, array4, array3, 3, w->rc, 0, n13, 0, n9, n3, n4, 7, 0, false, 0, false);
  if (w->depth / w->size < 7.0f) array[n].master = 2;
  ++n;

  array3[0] = jtrunc_d((double)n3 - 8.66 * (double)w->size);
  array4[0] = jtrunc((float)n4 - (5.0f * w->size));
  array3[1] = jtrunc_d((double)n3 - 8.66 * (double)w->size);
  array4[1] = jtrunc((float)n4 + (5.0f * w->size));
  plane_init(&array[n], m, t, array2, array4, array3, 3, w->rc, 0, n13, 0, n9, n3, n4, 7, 0, false, 0, false);
  if (w->depth / w->size < 7.0f) array[n].master = 2;
  ++n;

  array3[0] = jtrunc_d((double)n3 - 8.66 * (double)w->size);
  array4[0] = jtrunc((float)n4 + (5.0f * w->size));
  array3[1] = n3;
  array4[1] = jtrunc((float)n4 + (10.0f * w->size));
  plane_init(&array[n], m, t, array2, array4, array3, 3, w->rc, 0, n13, 0, n9, n3, n4, 7, 0, false, 0, false);
  if (w->depth / w->size < 7.0f) array[n].master = 2;
  ++n;

  array2[0] = jtrunc((float)n2 - (4.0f * n10));
  array3[0] = jtrunc((float)n3 - (12.557f * n11));
  array4[0] = jtrunc((float)n4 + (3.3646f * n11));
  array2[1] = jtrunc((float)n2 - (4.0f * n10));
  array3[1] = jtrunc((float)n3 - (12.557f * n11));
  array4[1] = jtrunc((float)n4 - (3.3646f * n11));
  array2[2] = jtrunc((float)n2 + (4.0f * n10));
  array3[2] = jtrunc((float)n3 - (12.557f * n11));
  array4[2] = jtrunc((float)n4 - (3.3646f * n11));
  array2[3] = jtrunc((float)n2 + (4.0f * n10));
  array3[3] = jtrunc((float)n3 - (12.557f * n11));
  array4[3] = jtrunc((float)n4 + (3.3646f * n11));
  plane_init(&array[n], m, t, array2, array4, array3, 4, array5, 0, n8, -1 * n12, n9, n3, n4, 7, 0, false, 0, true);
  ++n;

  array2[0] = jtrunc((float)n2 - (4.0f * n10));
  array3[0] = jtrunc((float)n3 - (9.1923f * n11));
  array4[0] = jtrunc((float)n4 - (9.1923f * n11));
  array2[1] = jtrunc((float)n2 - (4.0f * n10));
  array3[1] = jtrunc((float)n3 - (12.557f * n11));
  array4[1] = jtrunc((float)n4 - (3.3646f * n11));
  array2[2] = jtrunc((float)n2 + (4.0f * n10));
  array3[2] = jtrunc((float)n3 - (12.557f * n11));
  array4[2] = jtrunc((float)n4 - (3.3646f * n11));
  array2[3] = jtrunc((float)n2 + (4.0f * n10));
  array3[3] = jtrunc((float)n3 - (9.1923f * n11));
  array4[3] = jtrunc((float)n4 - (9.1923f * n11));
  plane_init(&array[n], m, t, array2, array4, array3, 4, array5, 0, n8, 1 * n12, n9, n3, n4, 7, 0, false, 0, true);
  ++n;

  array2[0] = jtrunc((float)n2 - (4.0f * n10));
  array3[0] = jtrunc((float)n3 - (9.1923f * n11));
  array4[0] = jtrunc((float)n4 - (9.1923f * n11));
  array2[1] = jtrunc((float)n2 - (4.0f * n10));
  array3[1] = jtrunc((float)n3 - (3.3646f * n11));
  array4[1] = jtrunc((float)n4 - (12.557f * n11));
  array2[2] = jtrunc((float)n2 + (4.0f * n10));
  array3[2] = jtrunc((float)n3 - (3.3646f * n11));
  array4[2] = jtrunc((float)n4 - (12.557f * n11));
  array2[3] = jtrunc((float)n2 + (4.0f * n10));
  array3[3] = jtrunc((float)n3 - (9.1923f * n11));
  array4[3] = jtrunc((float)n4 - (9.1923f * n11));
  plane_init(&array[n], m, t, array2, array4, array3, 4, array5, 0, n8, 1 * n12, n9, n3, n4, 7, 0, false, 0, true);
  ++n;

  array2[0] = jtrunc((float)n2 - (4.0f * n10));
  array3[0] = jtrunc((float)n3 - (3.3646f * n11));
  array4[0] = jtrunc((float)n4 - (12.557f * n11));
  array2[1] = jtrunc((float)n2 - (4.0f * n10));
  array3[1] = jtrunc((float)n3 + (3.3646f * n11));
  array4[1] = jtrunc((float)n4 - (12.557f * n11));
  array2[2] = jtrunc((float)n2 + (4.0f * n10));
  array3[2] = jtrunc((float)n3 + (3.3646f * n11));
  array4[2] = jtrunc((float)n4 - (12.557f * n11));
  array2[3] = jtrunc((float)n2 + (4.0f * n10));
  array3[3] = jtrunc((float)n3 - (3.3646f * n11));
  array4[3] = jtrunc((float)n4 - (12.557f * n11));
  plane_init(&array[n], m, t, array2, array4, array3, 4, array5, 0, n8, -1 * n12, n9, n3, n4, 7, 0, false, 0, true);
  ++n;

  array2[0] = jtrunc((float)n2 - (4.0f * n10));
  array3[0] = jtrunc((float)n3 + (9.1923f * n11));
  array4[0] = jtrunc((float)n4 - (9.1923f * n11));
  array2[1] = jtrunc((float)n2 - (4.0f * n10));
  array3[1] = jtrunc((float)n3 + (3.3646f * n11));
  array4[1] = jtrunc((float)n4 - (12.557f * n11));
  array2[2] = jtrunc((float)n2 + (4.0f * n10));
  array3[2] = jtrunc((float)n3 + (3.3646f * n11));
  array4[2] = jtrunc((float)n4 - (12.557f * n11));
  array2[3] = jtrunc((float)n2 + (4.0f * n10));
  array3[3] = jtrunc((float)n3 + (9.1923f * n11));
  array4[3] = jtrunc((float)n4 - (9.1923f * n11));
  plane_init(&array[n], m, t, array2, array4, array3, 4, array5, 0, n8, 1 * n12, n9, n3, n4, 7, 0, false, 0, true);
  ++n;

  array2[0] = jtrunc((float)n2 - (4.0f * n10));
  array3[0] = jtrunc((float)n3 + (9.1923f * n11));
  array4[0] = jtrunc((float)n4 - (9.1923f * n11));
  array2[1] = jtrunc((float)n2 - (4.0f * n10));
  array3[1] = jtrunc((float)n3 + (12.557f * n11));
  array4[1] = jtrunc((float)n4 - (3.3646f * n11));
  array2[2] = jtrunc((float)n2 + (4.0f * n10));
  array3[2] = jtrunc((float)n3 + (12.557f * n11));
  array4[2] = jtrunc((float)n4 - (3.3646f * n11));
  array2[3] = jtrunc((float)n2 + (4.0f * n10));
  array3[3] = jtrunc((float)n3 + (9.1923f * n11));
  array4[3] = jtrunc((float)n4 - (9.1923f * n11));
  plane_init(&array[n], m, t, array2, array4, array3, 4, array5, 0, n8, 1 * n12, n9, n3, n4, 7, 0, false, 0, true);
  ++n;

  array2[0] = jtrunc((float)n2 - (4.0f * n10));
  array3[0] = jtrunc((float)n3 + (12.557f * n11));
  array4[0] = jtrunc((float)n4 - (3.3646f * n11));
  array2[1] = jtrunc((float)n2 - (4.0f * n10));
  array3[1] = jtrunc((float)n3 + (12.557f * n11));
  array4[1] = jtrunc((float)n4 + (3.3646f * n11));
  array2[2] = jtrunc((float)n2 + (4.0f * n10));
  array3[2] = jtrunc((float)n3 + (12.557f * n11));
  array4[2] = jtrunc((float)n4 + (3.3646f * n11));
  array2[3] = jtrunc((float)n2 + (4.0f * n10));
  array3[3] = jtrunc((float)n3 + (12.557f * n11));
  array4[3] = jtrunc((float)n4 - (3.3646f * n11));
  plane_init(&array[n], m, t, array2, array4, array3, 4, array5, 0, n8, -1 * n12, n9, n3, n4, 7, 0, false, 0, true);
  ++n;

  array2[0] = jtrunc((float)n2 - (4.0f * n10));
  array3[0] = jtrunc((float)n3 + (9.1923f * n11));
  array4[0] = jtrunc((float)n4 + (9.1923f * n11));
  array2[1] = jtrunc((float)n2 - (4.0f * n10));
  array3[1] = jtrunc((float)n3 + (12.557f * n11));
  array4[1] = jtrunc((float)n4 + (3.3646f * n11));
  array2[2] = jtrunc((float)n2 + (4.0f * n10));
  array3[2] = jtrunc((float)n3 + (12.557f * n11));
  array4[2] = jtrunc((float)n4 + (3.3646f * n11));
  array2[3] = jtrunc((float)n2 + (4.0f * n10));
  array3[3] = jtrunc((float)n3 + (9.1923f * n11));
  array4[3] = jtrunc((float)n4 + (9.1923f * n11));
  plane_init(&array[n], m, t, array2, array4, array3, 4, array5, 0, n8, 1 * n12, n9, n3, n4, 7, 0, false, 0, true);
  ++n;

  array2[0] = jtrunc((float)n2 - (4.0f * n10));
  array3[0] = jtrunc((float)n3 + (9.1923f * n11));
  array4[0] = jtrunc((float)n4 + (9.1923f * n11));
  array2[1] = jtrunc((float)n2 - (4.0f * n10));
  array3[1] = jtrunc((float)n3 + (3.3646f * n11));
  array4[1] = jtrunc((float)n4 + (12.557f * n11));
  array2[2] = jtrunc((float)n2 + (4.0f * n10));
  array3[2] = jtrunc((float)n3 + (3.3646f * n11));
  array4[2] = jtrunc((float)n4 + (12.557f * n11));
  array2[3] = jtrunc((float)n2 + (4.0f * n10));
  array3[3] = jtrunc((float)n3 + (9.1923f * n11));
  array4[3] = jtrunc((float)n4 + (9.1923f * n11));
  plane_init(&array[n], m, t, array2, array4, array3, 4, array5, 0, n8, 1 * n12, n9, n3, n4, 7, 0, false, 0, true);
  ++n;

  array2[0] = jtrunc((float)n2 - (4.0f * n10));
  array3[0] = jtrunc((float)n3 + (3.3646f * n11));
  array4[0] = jtrunc((float)n4 + (12.557f * n11));
  array2[1] = jtrunc((float)n2 - (4.0f * n10));
  array3[1] = jtrunc((float)n3 - (3.3646f * n11));
  array4[1] = jtrunc((float)n4 + (12.557f * n11));
  array2[2] = jtrunc((float)n2 + (4.0f * n10));
  array3[2] = jtrunc((float)n3 - (3.3646f * n11));
  array4[2] = jtrunc((float)n4 + (12.557f * n11));
  array2[3] = jtrunc((float)n2 + (4.0f * n10));
  array3[3] = jtrunc((float)n3 + (3.3646f * n11));
  array4[3] = jtrunc((float)n4 + (12.557f * n11));
  plane_init(&array[n], m, t, array2, array4, array3, 4, array5, 0, n8, -1 * n12, n9, n3, n4, 7, 0, false, 0, true);
  ++n;

  array2[0] = jtrunc((float)n2 - (4.0f * n10));
  array3[0] = jtrunc((float)n3 - (9.1923f * n11));
  array4[0] = jtrunc((float)n4 + (9.1923f * n11));
  array2[1] = jtrunc((float)n2 - (4.0f * n10));
  array3[1] = jtrunc((float)n3 - (3.3646f * n11));
  array4[1] = jtrunc((float)n4 + (12.557f * n11));
  array2[2] = jtrunc((float)n2 + (4.0f * n10));
  array3[2] = jtrunc((float)n3 - (3.3646f * n11));
  array4[2] = jtrunc((float)n4 + (12.557f * n11));
  array2[3] = jtrunc((float)n2 + (4.0f * n10));
  array3[3] = jtrunc((float)n3 - (9.1923f * n11));
  array4[3] = jtrunc((float)n4 + (9.1923f * n11));
  plane_init(&array[n], m, t, array2, array4, array3, 4, array5, 0, n8, 1 * n12, n9, n3, n4, 7, 0, false, 0, true);
  ++n;

  array2[0] = jtrunc((float)n2 - (4.0f * n10));
  array3[0] = jtrunc((float)n3 - (9.1923f * n11));
  array4[0] = jtrunc((float)n4 + (9.1923f * n11));
  array2[1] = jtrunc((float)n2 - (4.0f * n10));
  array3[1] = jtrunc((float)n3 - (12.557f * n11));
  array4[1] = jtrunc((float)n4 + (3.3646f * n11));
  array2[2] = jtrunc((float)n2 + (4.0f * n10));
  array3[2] = jtrunc((float)n3 - (12.557f * n11));
  array4[2] = jtrunc((float)n4 + (3.3646f * n11));
  array2[3] = jtrunc((float)n2 + (4.0f * n10));
  array3[3] = jtrunc((float)n3 - (9.1923f * n11));
  array4[3] = jtrunc((float)n4 + (9.1923f * n11));
  plane_init(&array[n], m, t, array2, array4, array3, 4, array5, 0, n8, 1 * n12, n9, n3, n4, 7, 0, false, 0, true);
  ++n;
}
