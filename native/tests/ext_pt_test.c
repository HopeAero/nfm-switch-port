// Extended's Premier Tournament logic (ext_pt.c), with this port's mends.
#include <stdio.h>
#include <string.h>

#include "ext_pt.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
  if (!(cond)) { fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); failures++; } \
} while (0)

static void clear_cars(ExtPTCar *cars) {
  memset(cars, 0, sizeof(ExtPTCar) * EXT_PT_CARS);
  for (int32_t a = 0; a < EXT_PT_CARS; a++) cars[a].killer = -1;
}

static void test_wasting_match(void) {
  ExtPT pt;
  ext_pt_start_tournament(&pt);
  ExtPTCar cars[EXT_PT_CARS];
  int32_t pos[EXT_PT_CARS] = {0}, clear[EXT_PT_CARS] = {0};
  // Car 3 wastes car 5 seven times; car 5 comes back after 40 ticks each time.
  for (int32_t kill = 0; kill < 7; kill++) {
    clear_cars(cars);
    cars[5].dest = true;
    cars[5].killer = 3;
    for (int32_t t = 0; t < 41; t++) ext_pt_tick(&pt, cars, pos, clear, 10, 0, true);
    if (!pt.over) CHECK(cars[5].revive_now, "a wasted car comes back after 40 ticks");
    clear_cars(cars);
    ext_pt_tick(&pt, cars, pos, clear, 10, 0, true);
  }
  CHECK(pt.score[3] == 7 && pt.score[5] == -7, "kills score, deaths cost");
  CHECK(pt.over && pt.winner == 3 && pt.position[3] == 0, "first to 7 wins match 1");
  ext_pt_award(&pt);
  CHECK(pt.points[3] == 10 && pt.points[5] == 0, "10 - place points");
}

static void test_elimination_and_final(void) {
  ExtPT pt;
  ext_pt_start_tournament(&pt);
  pt.match = 5;
  ext_pt_start_match(&pt);
  ExtPTCar cars[EXT_PT_CARS];
  int32_t pos[EXT_PT_CARS] = {0}, clear[EXT_PT_CARS] = {0};
  clear_cars(cars);
  cars[0].hitmag = 0;
  for (int32_t a = 1; a < EXT_PT_CARS; a++) cars[a].hitmag = a * 100;   // car 10 most damaged: last
  for (int32_t t = 0; t < 1002; t++) ext_pt_tick(&pt, cars, pos, clear, 10, 0, true);
  CHECK(pt.eliminated[10] && !pt.eliminated[0], "match 5's clock eliminates the last-placed car, not the player");
  pt.points[0] = 40;
  pt.points[7] = 55;
  CHECK(ext_pt_champion(&pt) == 7, "most points takes the tournament");
  CHECK(ext_pt_rules(3)[0] && strcmp(ext_pt_rules(3)[0], "MATCH 3") == 0, "rules text");
}

int main(void) {
  test_wasting_match();
  test_elimination_and_final();
  if (failures == 0) {
    printf("all tests passed\n");
    return 0;
  }
  fprintf(stderr, "%d failure(s)\n", failures);
  return 1;
}
