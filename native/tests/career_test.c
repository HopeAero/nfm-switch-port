// career.c: the save round trip, the car locks, experience and level-ups.
#include <stdio.h>
#include <string.h>

#include "../core/career.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
  if (!(cond)) { fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); failures++; } \
} while (0)

int main(void) {
  CareerSave s, t;
  career_reset(&s);
  CHECK(s.unlocked == 1 && s.level[5] == 1, "reset");

  // Locks (XT 15274-15309).
  CHECK(career_car_lock(&s, 0) == 0 && career_car_lock(&s, 7) == 0, "first eight open");
  CHECK(career_car_lock(&s, 8) == 2 && career_car_lock(&s, 17) == 20, "cars by stage");
  CHECK(career_car_lock(&s, 18) == 23 && career_car_lock(&s, 19) == 26, "Titan, Deity");
  CHECK(career_car_lock(&s, 23) == 0 && career_car_lock(&s, 30) == 0, "NFM 2's first eight open");
  CHECK(career_car_lock(&s, 31) == -1 && career_car_lock(&s, 37) == -3 && career_car_lock(&s, 20) == 99, "bonus cars");
  s.unlocked = 3;
  CHECK(career_car_lock(&s, 8) == 0 && career_car_lock(&s, 9) == 4, "stage 2 won opens car 8");

  // Round trip.
  s.level[3] = 12;
  s.exp[3] = 345;
  s.statpoints[3] = 7;
  s.sp[3][CS_END] = 9;
  s.boncomp[1] = 2;
  s.lastcar = 3;
  s.carpoints = 9;
  s.statchangers[0] = 1;
  s.extpoints[3] = 14;
  s.perk[3][2] = 7;
  s.perk[38][5] = 20;
  s.rebsp[5] = 1.0 / 3.0 * 2.25;   // a level transfer's ratio, kept to the last bit
  s.xbsp[6] = 0.1;
  const char *path = "career_test_save.txt";
  CHECK(career_save(path, &s), "save");
  CHECK(career_load(path, &t), "load");
  CHECK(memcmp(&s, &t, sizeof(s)) == 0, "round trip");
  remove(path);

  // A save from before perks and level transfers still loads, with none.
  {
    FILE *f = fopen(path, "w");
    fprintf(f, "unlocked=4\nlaststage=2\nlastcar=1\nkills=3\nwins=5\ncarpoints=0\nchangers=1,1\n"
               "bonus=0,0,0,0,0,0\ncar=1,6,10,2,5,5,5,5,5,5,1,2,3\n");
    fclose(f);
    CHECK(career_load(path, &t), "old save loads");
    CHECK(t.unlocked == 4 && t.level[1] == 6 && t.extpoints[1] == 3, "old save's fields");
    CHECK(t.perk[1][0] == 0 && t.rebsp[1] == 1.0 && t.xbsp[1] == 0.0 && t.rebsp[30] == 1.0, "old save: no perks, ratios 1");
    remove(path);
  }

  // Experience: a checkpoint pays, enough of it levels the car up.
  CareerRace r;
  memset(&r, 0, sizeof(r));
  r.stage = 1;
  r.nplayers = 11;
  r.sc[0] = 0;
  for (int32_t i = 0; i < r.nplayers; i++) r.level[i] = 3;
  r.softlevelcap = 9;
  career_reset(&s);
  CareerRun run;
  career_run_start(&run, &r, &s);
  CHECK(run.expneeded == career_reqneed(1, 0) && run.expneeded > 0, "expneeded");
  career_xp_checkpoint(&run, &r, &s, 1);
  CHECK(s.exp[0] == run.last_gain && run.last_gain == 67 + 7 + 6, "checkpoint experience (base 67+7*1+6*1)");
  for (int32_t k = 0; k < 40; k++) career_xp_checkpoint(&run, &r, &s, 1);
  career_tick(&run, &r, &s, true, false);
  CHECK(s.level[0] == 2 && s.statpoints[0] == 4 && s.sp[0][CS_TS] == 1, "level up: +4 points, +1 every stat");
  // Losing the newest stage takes the race back (stage 1 is exempt).
  r.stage = 2;
  s.unlocked = 2;
  career_run_start(&run, &r, &s);
  career_tick(&run, &r, &s, false, false);
  const int32_t before = s.exp[0];
  career_xp_checkpoint(&run, &r, &s, 1);
  career_xp_lose(&run, &r, &s);
  CHECK(s.exp[0] == before, "loss gives it back");

  if (failures == 0) {
    printf("all tests passed\n");
    return 0;
  }
  fprintf(stderr, "%d failure(s)\n", failures);
  return 1;
}
