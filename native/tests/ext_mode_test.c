// Extended's normal mode logic (ext_mode.c).
#include <stdio.h>
#include <string.h>

#include "ext_mode.h"
#include "java_compat.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
  if (!(cond)) { fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); failures++; } \
} while (0)

static void test_numbers(void) {
  CHECK(ext_car_of(0) == 23 && ext_car_of(15) == 38, "NFM 2's cars are Extended's 23-38");
  CHECK(ext_car_of(16) == 0 && ext_car_of(38) == 22, "Extended's own are this port's 16-38");
  CHECK(ext_car_of(39) == -1, "the custom car has no Extended number");
  for (int32_t c = 0; c < 39; c++) CHECK(ext_car_to_port(ext_car_of(c)) == c, "round trip");
}

static void test_sortcars(void) {
  nfm_set_seed(9001);
  for (int32_t stage = 1; stage <= 28; stage++) {
    if (stage == 26) continue;
    for (int32_t latest = 0; latest < 2; latest++) {
      const int32_t unlocked = latest ? stage : 28;
      int32_t sc[11] = {16};
      ext_sortcars(sc, 11, stage, unlocked, 0);
      char lbl[64];
      for (int32_t k = 0; k < 11; k++) {
        snprintf(lbl, sizeof(lbl), "stage %d slot %d is a car", stage, k);
        CHECK(sc[k] >= 0 && sc[k] < 39, lbl);
      }
      if (latest && stage != 28) {   // 28 is never "newest" (XT 12711)
        int32_t boss = 7 + (stage + 1) / 2;
        if (boss == 20 || boss == 21) boss = 22;
        snprintf(lbl, sizeof(lbl), "stage %d newest: the boss last", stage);
        CHECK(sc[10] == ext_car_to_port(boss), lbl);
        for (int32_t k = 1; k < 10; k++) {
          snprintf(lbl, sizeof(lbl), "stage %d newest: below the boss", stage);
          CHECK(ext_car_of(sc[k]) < boss, lbl);
        }
      }
      const bool stop = stage == 16 || stage == 12 || stage == 6 || stage == 7 || stage == 8 || stage == 11 ||
                        stage == 13 || stage == 17 || stage == 19 || stage == 23 || stage == 24;
      if ((!latest || (stage >= 5 && !stop)) && unlocked >= 5) {
        for (int32_t a = 1; a < 10; a++)
          for (int32_t b = a + 1; b < 10; b++) {
            snprintf(lbl, sizeof(lbl), "stage %d (latest %d): no repeats", stage, latest);
            CHECK(sc[a] != sc[b], lbl);
          }
      }
    }
  }
  int32_t sc[11] = {3};
  ext_sortcars(sc, 11, 26, 26, 2);
  for (int32_t k = 0; k < 11; k++) CHECK(sc[k] == ext_car_to_port(35), "PT match 2: everyone in Mighty Eight");
}

static void test_tables(void) {
  ExtMusic mu = ext_stage_music(13);
  CHECK(strcmp(mu.file, "stage13") == 0 && mu.amp == 225 && mu.rate == 7600 && mu.tempo == 137, "stage 13 music");
  char pack[16], entry[16];
  ext_stage_entry(26, 3, pack, sizeof(pack), entry, sizeof(entry));
  CHECK(strcmp(pack, "matchtracks") == 0 && strcmp(entry, "26m3.txt") == 0, "PT match 3 entry");
  ext_stage_entry(7, 0, pack, sizeof(pack), entry, sizeof(entry));
  CHECK(strcmp(pack, "tracks") == 0 && strcmp(entry, "7.txt") == 0, "stage 7 entry");
  CHECK(ext_unlock_after_win(5, 5) == 6 && ext_unlock_after_win(5, 3) == 5 && ext_unlock_after_win(28, 28) == 28,
        "unlocking");
  CHECK(ext_showcase_car(2) == ext_car_to_port(8) && ext_showcase_car(3) == -1 && ext_showcase_car(26) == ext_car_to_port(19),
        "showcase cars");
}

int main(void) {
  test_numbers();
  test_sortcars();
  test_tables();
  if (failures == 0) {
    printf("all tests passed\n");
    return 0;
  }
  fprintf(stderr, "%d failure(s)\n", failures);
  return 1;
}
