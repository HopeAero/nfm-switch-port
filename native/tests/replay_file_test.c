// replay_file: a saved replay reads back as it was written -- the header,
// the ring, the cars' dented faces -- and a foreign file is refused.
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../core/replay_file.h"

static int failures = 0;
#define CHECK(cond, msg)                         \
  do {                                           \
    if (!(cond)) {                               \
      fprintf(stderr, "FAIL: %s\n", msg);        \
      failures++;                                \
    }                                            \
  } while (0)

static Record a, b;

// A one-face model with three points, as a car's ring keyframe.
static void one_face(ContO *co, int32_t base) {
  memset(co, 0, sizeof(*co));
  co->npl = 1;
  co->p = calloc(1, sizeof(Plane));
  co->p[0].n = 3;
  co->p[0].ox = calloc(3, sizeof(int32_t));
  co->p[0].oy = calloc(3, sizeof(int32_t));
  co->p[0].oz = calloc(3, sizeof(int32_t));
  for (int32_t k = 0; k < 3; k++) co->p[0].ox[k] = co->p[0].oy[k] = co->p[0].oz[k] = base + k;
}

int main(void) {
  const char *path = "replay_file_test.nfmr";
  ReplayMeta meta = {.gmode = 3, .ext_normal = 1, .stage_num = 7, .nplayers = 3, .car_index = 20};
  meta.sc[1] = 5;
  meta.beast[2] = 1;
  snprintf(meta.stage_name, sizeof(meta.stage_name), "Van's Revenge");
  for (int32_t t = 0; t < 300; t++)
    for (int32_t i = 0; i < 3; i++) a.x[t][i] = t * 10 + i, a.wzy[t][i] = -t;
  a.checkpoint[299] = 4;
  a.dest[1] = 120;
  a.sspark[2][3][4] = 77;
  a.rcz[1][199] = 1.5f;
  a.magx[2][3][6] = 999;
  a.mtouch[0][6] = true;
  for (int32_t i = 0; i < 3; i++) one_face(&a.car[0][i], 100 * i);
  a.car[0][1].p[0].ox[2] = 4242;   // a dent
  a.car[0][1].p[0].c[1] = 33;

  CHECK(replay_save(path, &meta, &a, false), "replay_save writes");
  ReplayMeta m2;
  CHECK(replay_read_meta(path, &m2) && m2.stage_num == 7 && m2.sc[1] == 5 && m2.beast[2] == 1 &&
            strcmp(m2.stage_name, "Van's Revenge") == 0,
        "the header reads back");
  for (int32_t i = 0; i < 3; i++) one_face(&b.car[0][i], 0);   // the race set up again: fresh models
  CHECK(replay_read(path, &m2, &b), "replay_read reads");
  CHECK(b.x[150][2] == 1502 && b.wzy[299][1] == -299 && b.checkpoint[299] == 4 && b.dest[1] == 120,
        "the poses read back");
  CHECK(b.sspark[2][3][4] == 77 && b.rcz[1][199] == 1.5f && b.magx[2][3][6] == 999 && b.mtouch[0][6],
        "the sparks and dents read back");
  CHECK(b.car[0][1].p[0].ox[2] == 4242 && b.car[0][1].p[0].c[1] == 33 && b.car[0][2].p[0].oz[0] == 200,
        "each car's faces take the saved shape");
  CHECK(b.caught == 300, "the ring counts as full");

  FILE *f = fopen(path, "wb");
  fputs("not a replay at all, just some text long enough", f);
  fclose(f);
  CHECK(!replay_read_meta(path, &m2), "a foreign file is refused");
  remove(path);

  if (failures == 0) {
    printf("all tests passed\n");
    return 0;
  }
  fprintf(stderr, "%d failure(s)\n", failures);
  return 1;
}
