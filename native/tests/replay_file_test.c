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

  // A stage piece the script moves and lights on tick 3 of 5, after 295 still ones.
  ObjLook now[2];
  ContO piece;
  memset(&piece, 0, sizeof(piece));
  now[0] = look_of(&piece, 0), now[1] = look_of(&piece, 0);
  LookRing la;
  memset(&la, 0, sizeof(la));
  looks_reset(&la, now, 2);
  for (int32_t t = 0; t < 300; t++) {
    if (t == 297) {
      piece.y = -500, piece.glowlines = true, piece.glowc[0] = 255;
      now[1] = look_of(&piece, 40);
    }
    looks_rec(&la, now, t == 299 ? 90 : 0);
  }
  CHECK(la.n == 1 && la.idx[0] == 1, "only the changed object is kept");
  CHECK(looks_at(&la, 0, 296)->y == 0 && looks_at(&la, 0, 297)->y == -500, "its past is how it started");
  CHECK(replay_save(path, &meta, &a, false, &la), "replay_save writes");
  ReplayMeta m2;
  CHECK(replay_read_meta(path, &m2) && m2.stage_num == 7 && m2.sc[1] == 5 && m2.beast[2] == 1 &&
            strcmp(m2.stage_name, "Van's Revenge") == 0,
        "the header reads back");
  for (int32_t i = 0; i < 3; i++) one_face(&b.car[0][i], 0);   // the race set up again: fresh models
  LookRing lb;
  memset(&lb, 0, sizeof(lb));
  looks_reset(&lb, now, 2);   // the race set up again: its objects
  CHECK(replay_read(path, &m2, &b, &lb), "replay_read reads");
  CHECK(lb.n == 1 && lb.idx[0] == 1 && looks_at(&lb, 0, 299)->y == -500 && looks_at(&lb, 0, 299)->fade == 40 &&
            (looks_at(&lb, 0, 299)->flags & LOOK_GLOW) && looks_at(&lb, 0, 10)->y == 0 &&
            looks_outline(&lb, 299) == 90 && looks_outline(&lb, 298) == 0,
        "the stage script's looks read back in order");
  ContO back;
  memset(&back, 0, sizeof(back));
  look_apply(&back, looks_at(&lb, 0, 299), true);
  CHECK(back.y == -500 && back.fade == 40 && back.glowlines && back.glowc[0] == 255, "a look goes back onto its object");
  looks_free(&la);
  looks_free(&lb);
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
