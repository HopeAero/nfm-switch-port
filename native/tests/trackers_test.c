// Host-buildable test for native/core/trackers.c.
//
// web/Trackers.js has no web/Trackers.test.js (unlike most ported modules)
// so there is no pre-existing verified-literal fixture to copy. Per
// native/PORT_SPEC.md §7, the already-verified JS itself is the oracle for
// a C port when no Java probe exists: the expected values below were
// captured by running the real web/Trackers.js under Node with this exact
// synthetic scene (see the commit that added this file for the script).
// This file does not re-derive Trackers.js's semantics -- it only checks
// the C port agrees with the JS.
#include <stdio.h>
#include "../core/trackers.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
  if (!(cond)) { fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); failures++; } \
} while (0)

static void check_sect(Trackers *t, int32_t i, int32_t j, const int32_t *want, int32_t want_len) {
  char label[64];
  snprintf(label, sizeof(label), "sect[%d][%d] length", i, j);
  CHECK(t->sect_len[i][j] == want_len, label);
  if (t->sect_len[i][j] != want_len) return;
  for (int32_t k = 0; k < want_len; k++) {
    char elem_label[64];
    snprintf(elem_label, sizeof(elem_label), "sect[%d][%d][%d]", i, j, k);
    CHECK(t->sect[i][j][k] == want[k], elem_label);
  }
}

int main(void) {
  Trackers t;
  trackers_init(&t);

  // Same synthetic scene as the Node oracle run: 5 trackers, one boundary
  // wall (dam=167), spread across a 9000x9000 area.
  t.nt = 5;
  int32_t xs[5] = {1000, 4000, 7000, -83000, 500};
  int32_t zs[5] = {1000, 1000, 7000, -83000, 8500};
  int32_t dams[5] = {0, 0, 0, 167, 5};
  for (int i = 0; i < 5; i++) {
    t.x[i] = xs[i];
    t.z[i] = zs[i];
    t.dam[i] = dams[i];
  }

  trackers_devidetrackers(&t, 0, 9000, 0, 9000);

  CHECK(t.ncx == 2, "ncx");
  CHECK(t.ncz == 2, "ncz");

  int32_t want_dam[5] = {0, 0, 0, 1, 5}; // dam=167 -> 1 after devidetrackers
  for (int i = 0; i < 5; i++) {
    char label[32];
    snprintf(label, sizeof(label), "dam[%d]", i);
    CHECK(t.dam[i] == want_dam[i], label);
  }

  int32_t s00[] = {0, 1, 3};       check_sect(&t, 0, 0, s00, 3);
  int32_t s01[] = {0, 1, 4, 3};    check_sect(&t, 0, 1, s01, 4);
  int32_t s02[] = {4, 3};          check_sect(&t, 0, 2, s02, 2);
  int32_t s10[] = {0, 1, 3};       check_sect(&t, 1, 0, s10, 3);
  int32_t s11[] = {1, 2};          check_sect(&t, 1, 1, s11, 2);
  int32_t s12[] = {2, 4, 3};       check_sect(&t, 1, 2, s12, 3);
  int32_t s20[] = {1, 3};          check_sect(&t, 2, 0, s20, 2);
  int32_t s21[] = {2, 3};          check_sect(&t, 2, 1, s21, 2);
  int32_t s22[] = {2, 3};          check_sect(&t, 2, 2, s22, 2);

  CHECK(trackers_py(0, 0, 0, 0) == 0, "py(0,0,0,0)");
  CHECK(trackers_py(100, 50, 200, 150) == 5000, "py(100,50,200,150)");
  CHECK(trackers_py(-300, 200, -100, 400) == 500000, "py(-300,200,-100,400)");
  // Wraps at int32 on both the multiplies and the addition -- see
  // web/TRANSPILE_SPEC.md §2b. In C this needs no explicit wrap helper:
  // -fwrapv makes native int32 arithmetic wrap the same way.
  CHECK(trackers_py(50000, 0, 50000, 0) == 705032704, "py overflow");

  trackers_free_sect(&t);

  if (failures == 0) {
    printf("all tests passed\n");
    return 0;
  }
  fprintf(stderr, "%d failure(s)\n", failures);
  return 1;
}
