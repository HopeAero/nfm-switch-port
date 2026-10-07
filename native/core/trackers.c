// ports web/Trackers.js
//
// Requires -fwrapv (see native/CMakeLists.txt and native/tests/CMakeLists.txt):
// trackers_py relies on signed int32 arithmetic wrapping like Java's.
#include "trackers.h"
#include <stdlib.h>
#include <string.h>

void trackers_init(Trackers *t) {
  memset(t, 0, sizeof(*t));
}

void trackers_free_sect(Trackers *t) {
  if (!t->sect) return;
  // sect/sect_len were sized to the grid BEFORE the post-build decrement
  // (see the header note), so the live extent is ncx+1 by ncz+1.
  int32_t ncx = t->ncx + 1;
  int32_t ncz = t->ncz + 1;
  for (int32_t i = 0; i < ncx; i++) {
    for (int32_t j = 0; j < ncz; j++) {
      free(t->sect[i][j]);
    }
    free(t->sect[i]);
    free(t->sect_len[i]);
  }
  free(t->sect);
  free(t->sect_len);
  t->sect = NULL;
  t->sect_len = NULL;
}

void trackers_devidetrackers_cover(Trackers *t, int32_t sx, int32_t n, int32_t sz, int32_t n2) {
  trackers_free_sect(t);
  t->sx = sx;
  t->sz = sz;
  t->ncx = n / 3000;
  if (t->ncx <= 0) t->ncx = 1;
  t->ncz = n2 / 3000;
  if (t->ncz <= 0) t->ncz = 1;
  const int32_t ncx = t->ncx, ncz = t->ncz, pad = 1500;

  // Two passes over the trackers: count each cell's list, then fill it.
  t->sect = malloc(sizeof(int32_t **) * (size_t)ncx);
  t->sect_len = malloc(sizeof(int32_t *) * (size_t)ncx);
  for (int32_t i = 0; i < ncx; i++) {
    t->sect[i] = malloc(sizeof(int32_t *) * (size_t)ncz);
    t->sect_len[i] = calloc((size_t)ncz, sizeof(int32_t));
  }
  for (int32_t pass = 0; pass < 2; pass++) {
    if (pass == 1) {
      for (int32_t i = 0; i < ncx; i++) {
        for (int32_t j = 0; j < ncz; j++) {
          // An empty cell lists tracker 0, as devidetrackers' do.
          const int32_t len = t->sect_len[i][j] ? t->sect_len[i][j] : 1;
          t->sect[i][j] = malloc(sizeof(int32_t) * (size_t)len);
          t->sect[i][j][0] = 0;
          if (t->sect_len[i][j] == 0) t->sect_len[i][j] = -1;   // filled below as 1
          else t->sect_len[i][j] = 0;
        }
      }
    }
    for (int32_t k = 0; k < t->nt; k++) {
      int32_t i0 = (int32_t)(((int64_t)t->x[k] - t->radx[k] - pad - sx) / 3000);
      int32_t i1 = (int32_t)(((int64_t)t->x[k] + t->radx[k] + pad - sx) / 3000);
      int32_t j0 = (int32_t)(((int64_t)t->z[k] - t->radz[k] - pad - sz) / 3000);
      int32_t j1 = (int32_t)(((int64_t)t->z[k] + t->radz[k] + pad - sz) / 3000);
      // Truncating division rounds negatives up; that only widens the
      // range by a cell at the low edge, which clamping absorbs.
      if (i0 < 0) i0 = 0;
      if (j0 < 0) j0 = 0;
      if (i1 > ncx - 1) i1 = ncx - 1;
      if (j1 > ncz - 1) j1 = ncz - 1;
      if (i0 > ncx - 1) i0 = ncx - 1;
      if (j0 > ncz - 1) j0 = ncz - 1;
      if (i1 < 0) i1 = 0;
      if (j1 < 0) j1 = 0;
      for (int32_t i = i0; i <= i1; i++) {
        for (int32_t j = j0; j <= j1; j++) {
          if (pass == 0) {
            t->sect_len[i][j]++;
          } else if (t->sect_len[i][j] >= 0) {
            t->sect[i][j][t->sect_len[i][j]++] = k;
          }
        }
      }
    }
  }
  for (int32_t i = 0; i < ncx; i++) {
    for (int32_t j = 0; j < ncz; j++) {
      if (t->sect_len[i][j] < 0) t->sect_len[i][j] = 1;
    }
  }
  --t->ncx;
  --t->ncz;
}

int32_t trackers_py(int32_t n, int32_t n2, int32_t n3, int32_t n4) {
  int32_t a = n - n2;
  int32_t b = n3 - n4;
  return a * a + b * b;
}

void trackers_devidetrackers(Trackers *t, int32_t sx, int32_t n, int32_t sz, int32_t n2) {
  trackers_free_sect(t);
  t->sx = sx;
  t->sz = sz;
  t->ncx = n / 3000;
  if (t->ncx <= 0) t->ncx = 1;
  t->ncz = n2 / 3000;
  if (t->ncz <= 0) t->ncz = 1;

  int32_t ncx = t->ncx, ncz = t->ncz;
  t->sect = malloc(sizeof(int32_t **) * (size_t)ncx);
  t->sect_len = malloc(sizeof(int32_t *) * (size_t)ncx);
  for (int32_t i = 0; i < ncx; i++) {
    t->sect[i] = malloc(sizeof(int32_t *) * (size_t)ncz);
    t->sect_len[i] = malloc(sizeof(int32_t) * (size_t)ncz);
  }

  int32_t *scratch = malloc(sizeof(int32_t) * TRACKERS_MAX);
  for (int32_t i = 0; i < ncx; i++) {
    for (int32_t j = 0; j < ncz; j++) {
      int32_t n3 = t->sx + i * 3000 + 1500;
      int32_t n4 = t->sz + j * 3000 + 1500;
      int32_t n5 = 0;
      for (int32_t k = 0; k < t->nt; k++) {
        int32_t py = trackers_py(n3, t->x[k], n4, t->z[k]);
        if (py < 20250000 && py > 0 && t->dam[k] != 167) {
          scratch[n5++] = k;
        }
      }
      // dam == 167 marks the always-present boundary walls; they go into
      // the edge cells only.
      if (i == 0 || j == 0 || i == ncx - 1 || j == ncz - 1) {
        for (int32_t l = 0; l < t->nt; l++) {
          if (t->dam[l] == 167) scratch[n5++] = l;
        }
      }
      if (n5 == 0) scratch[n5++] = 0;
      t->sect[i][j] = malloc(sizeof(int32_t) * (size_t)n5);
      t->sect_len[i][j] = n5;
      for (int32_t k = 0; k < n5; k++) t->sect[i][j][k] = scratch[k];
    }
  }
  free(scratch);

  for (int32_t n7 = 0; n7 < t->nt; n7++) {
    if (t->dam[n7] == 167) t->dam[n7] = 1;
  }
  --t->ncx;
  --t->ncz;
}
