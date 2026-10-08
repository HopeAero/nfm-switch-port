// Tests native/core/career_stage.c (the career's per-stage scripts). A
// small fake race -- the stage's real field size, cars of NFM 2's sixteen
// (Extended 23-38, so car_define_init has their stats), blank stage pieces --
// runs career_stage_start and a few thousand ticks of
// frame/before_drive/portals/tick on every stage 1-31 and every bonus stage,
// with the cars pushed around by a fixed pattern. Then the stages with rules
// worth pinning are driven through them by hand: stage 20's falls and
// respawns, stage 13's floors and portals, stage 17's undead hunt, the
// Titan's phases on stage 23, stage 11's vans, stage 21's crumble, stage 19's
// glitches and stage 22's night. Where the original's JS was run on the same
// scripted states (scratch oracle career_stage_oracle.mjs, not in the repo),
// the values it printed are the expectations below and say so.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../core/career.h"
#include "../core/career_stage.h"
#include "../core/java_compat.h"
#include "../core/record.h"

#include "career_stage_cases.h"

static int failures = 0;
#define CHECK(cond, ...)                                       \
  do {                                                         \
    if (!(cond)) {                                             \
      failures++;                                              \
      fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__);     \
      fprintf(stderr, __VA_ARGS__);                            \
      fprintf(stderr, "\n");                                   \
    }                                                          \
  } while (0)

// ---- deterministic random sources -----------------------------------------------

static uint32_t g_r = 1, g_m = 7;
static double test_random(void) {
  g_r = g_r * 1664525u + 1013904223u;
  return (double)(g_r >> 8) / 16777216.0;
}
static float test_mrandom(Medium *m) {
  (void)m;
  g_m = g_m * 22695477u + 1u;
  return (float)((g_m >> 8) & 0xffff) / 65536.0f;
}

// ---- the fake race -----------------------------------------------------------------

#define NPIECES 900
#define WALL_R 20000
#define WALL_L -20000
#define WALL_T 60000
#define WALL_B -20000

typedef struct {
  CarDefine orig;
  CarDefine live[NFM_MAX_CARS];
  Mad mads[NFM_MAX_CARS];
  ContO cars[NFM_MAX_CARS];
  ContO pieces[NPIECES];
  CheckPoints cp;
  Control controls[NFM_MAX_CARS];
  Medium m;
  Trackers t;
  Record rpd;
  XtGraphicsStub xt;
  XtContva contva;
  CareerRace r;
  CareerSave s;
  CareerRun run;
  CareerStage cs;
  CareerStageWorld w;
  bool ghost[NFM_MAX_CARS][NFM_MAX_CARS];
} Field;

static Field *F;
static bool g_medium_ready = false;

static int32_t field_size(int32_t stage, int32_t bonus, bool hard) {
  if (bonus == 3) return 9;
  if (bonus == 4) return 5;
  if (stage == 3 || stage == 17) return 19;
  if (stage == 8) return 15;
  if (stage == 13) return 16;
  if (stage == 20) return 2;
  if (stage == 5 || stage == 18) return 7;
  if (stage == 23) return hard ? 12 : 11;
  if (stage == 14) return 6;
  return 11;
}

/** Extended car 23 + k is this port's NFM 2 car k. */
static int32_t port_of(int32_t ecar) { return ecar - 23; }

static void build(int32_t stage, int32_t bonus, bool hard) {
  Field *f = F;
  // The Medium's tables are built once; the rest is fresh per race.
  if (!g_medium_ready) {
    nfm_set_seed(12345);
    medium_init(&f->m);
    g_medium_ready = true;
  }
  memset(f->pieces, 0, sizeof(f->pieces));
  memset(f->cars, 0, sizeof(f->cars));
  car_define_init(&f->orig);
  check_points_init(&f->cp);
  trackers_init(&f->t);
  record_init(&f->rpd);
  xt_graphics_stub_init(&f->xt);
  f->xt.extended = true;
  memset(&f->contva, 0, sizeof(f->contva));
  career_reset(&f->s);
  f->s.unlocked = hard ? stage : 31;
  memset(&f->r, 0, sizeof(f->r));
  memset(&f->run, 0, sizeof(f->run));
  f->r.stage = stage;
  f->r.bonus = bonus;
  f->r.nplayers = field_size(stage, bonus, hard);
  f->r.averagelevel = 12;
  const int32_t np = f->r.nplayers;
  for (int32_t k = 0; k < np; k++) {
    f->r.sc[k] = 23 + (k * 5) % 16;
    f->r.level[k] = 10 + k;
    for (int32_t q = 0; q < CS_N; q++) f->r.sp[k][q] = 5 + ((k + q) % 7) * 3;
    f->r.beast[k] = k == 2;
    f->r.shadow[k] = k == np - 2 && np > 3;
  }
  f->cp.n = 6;
  f->cp.nsp = 6;
  f->cp.nlaps = 3;
  f->cp.pcs = 1;
  for (int32_t j = 0; j < f->cp.n; j++) {
    f->cp.x[j] = 0;
    f->cp.z[j] = j * 8000;
    f->cp.y[j] = 250;
    f->cp.typ[j] = 1;
    f->cp.rotation[j] = 0;
    f->cp.telefloor[j] = -1;
    f->cp.floor[j] = 0;
  }
  for (int32_t k = 0; k < np; k++) {
    const int32_t cn = port_of(f->r.sc[k]);
    f->live[k] = f->orig;
    career_apply_stats(&f->live[k], cn, f->r.sc[k], f->r.sp[k], f->r.level[k], f->r.shadow[k], k == 0);
    mad_init(&f->mads[k], &f->live[k], &f->m, &f->rpd, &f->xt, k);
    f->mads[k].cn = cn;
    f->mads[k].power = 98.0f;
    f->mads[k].pcleared = 1;
    f->cars[k].x = (k % 3 - 1) * 400;
    f->cars[k].z = (k / 3) * 760;
    f->cars[k].y = 250;
    f->cars[k].telechk = -1;
    control_init(&f->controls[k], &f->m);
  }
  for (int32_t p = 0; p < NPIECES; p++) {
    f->pieces[p].x = (p % 30) * 1000 - 15000;
    f->pieces[p].z = (p / 30) * 1000;
    f->pieces[p].y = 250;
    f->pieces[p].telechk = -1;
  }
  CareerStageWorld *w = &f->w;
  memset(w, 0, sizeof(*w));
  w->mads = f->mads;
  w->cars = f->cars;
  w->pieces = f->pieces;
  w->npieces = NPIECES;
  w->cp = &f->cp;
  w->controls = f->controls;
  w->m = &f->m;
  w->t = &f->t;
  w->contva = &f->contva;
  w->orig = &f->orig;
  w->nplayers = np;
  w->walls[0] = WALL_R;
  w->walls[1] = WALL_L;
  w->walls[2] = WALL_T;
  w->walls[3] = WALL_B;
  w->starcnt = 37;
  career_stage_start(&f->cs, &f->r, &f->s, w);
}

/** One tick in the game's order: day/night, (race on:) before_drive, the
 * "drive" (a fixed shove), portals, then careermode$m. */
static void step(int32_t tick) {
  Field *f = F;
  CareerStageWorld *w = &f->w;
  const int32_t np = w->nplayers;
  career_stage_frame(&f->cs);
  if (w->starcnt == 0) {
    career_stage_before_drive(&f->cs, w);
    for (int32_t k = 0; k < np; k++) {
      if (f->cs.respawning[k]) {
        career_stage_respawn_reset(&f->cs, &f->mads[k], f->mads[k].cn, &f->cars[k], &f->cp);
        continue;
      }
      f->cars[k].z += 40 + 7 * k;
      f->cars[k].x += ((tick / 50 + k) % 3 - 1) * 30;
      f->mads[k].speed = 80.0f + k;
      f->mads[k].mtouch = (tick + k) % 4 != 0;
      f->mads[k].pxy = (tick * 3 + k * 40) % 360;
      if (f->mads[k].hitmag > f->mads[k].cd->maxmag[f->mads[k].cn] && !career_phys_undead(&f->cs, k))
        f->mads[k].dest = true;
      career_stage_portal_move(&f->cs, w, k);
      career_stage_portal_detect(&f->cs, w, k);
    }
    for (int32_t k = 0; k < np; k++) f->cp.pos[k] = k;
  }
  career_stage_tick(&f->cs, &f->r, &f->s, &f->run, w);
  career_stage_ghostmode(&f->cs, &f->r, w, f->ghost);
  for (int32_t k = 0; k < np; k++) {
    float base_grip = f->live[k].grip[f->mads[k].cn];
    career_stage_stats(&f->cs, &f->r, k, &f->live[k], &f->orig, f->mads[k].cn, base_grip, &f->cp);
    f->mads[k].stunt_gain = 0.0f;
  }
  if (w->starcnt > 0) w->starcnt--;
}

// ---- every stage --------------------------------------------------------------------

static void smoke(int32_t stage, int32_t bonus, bool hard) {
  build(stage, bonus, hard);
  for (int32_t t = 0; t < 3000; t++) step(t);
  const CareerStage *cs = &F->cs;
  const int32_t np = F->w.nplayers;
  CHECK(cs->stage == stage && cs->bonus == bonus, "stage %d bonus %d: kept", stage, bonus);
  for (int32_t k = 0; k < np; k++) {
    CHECK(cs->groundlevel[k] <= 250.0f, "stage %d: groundlevel", stage);
    CHECK(F->cars[k].fade >= 0 && F->cars[k].fade <= 255, "stage %d: fade", stage);
  }
  // The undead the stage makes, and nobody else.
  int32_t nund = 0;
  for (int32_t k = 0; k < np; k++) nund += cs->undead[k];
  if (stage == 17) CHECK(cs->undead[1] && cs->undead[2] && cs->undead[3], "stage 17 undead 1-3");
  if (stage == 11 && bonus != 2) CHECK(cs->undead[1] && cs->undead[4], "stage 11 vans undead");
  if (stage == 23 && hard) CHECK(cs->undead[1], "stage 23 hard: the Titan is undead");
  if (bonus == 4) CHECK(nund == np - 2, "bonus 4: all but the player and the last undead (%d)", nund);
  if (stage < 11 && stage != 6 && stage != 3 && !bonus) CHECK(nund == 0, "stage %d: no undead (%d)", stage, nund);
  // The first 300 ticks of the race nobody can be hit on 5 and 9+.
  if (stage >= 9 || stage == 5) CHECK(!cs->invulnerable && cs->tempinv >= 300, "stage %d: start protection over", stage);
  static XtCareerAI ai;
  memset(&ai, 0, sizeof(ai));
  career_stage_export_ai(cs, &F->w, &ai);
  for (int32_t k = 0; k < np; k++)
    CHECK(ai.undead[k] == cs->undead[k] && ai.floor[k] == cs->floor[k], "stage %d: exported to the AI", stage);
  CHECK(ai.targetcar == cs->targetcar, "stage %d: targetcar exported", stage);
  career_stage_free(&F->cs);
}

// ---- stage 20: falls and lives ------------------------------------------------------

static void test_stage20(void) {
  build(20, 0, true);
  CareerStage *cs = &F->cs;
  CareerStageWorld *w = &F->w;
  w->starcnt = 0;
  F->cp.clear[0] = 2;
  F->mads[1].pcleared = 3;
  // Car 1 lies outside the arena on the ground: 30 ticks of counting, then
  // it is put back on the checkpoint before its last (pcleared - 1 = 2).
  // realwalls keeps a live car within 1000 of the walls, before the count
  // looks (GS 2481-2485): only a wasted one gets this far out.
  F->cars[1].x = WALL_R + 5000;
  F->cars[1].y = 0;
  F->mads[1].dest = true;
  int32_t t;
  for (t = 0; t < 30; t++) {
    career_stage_tick(cs, &F->r, &F->s, &F->run, w);
    CHECK(cs->lives[1] == t + 1, "stage 20: lives counts the ticks out (%d at %d)", cs->lives[1], t);
    CHECK(F->mads[1].hitmag == F->mads[1].cd->maxmag[F->mads[1].cn] + 1, "stage 20: wasted while out");
  }
  CHECK(!cs->respawning[1] && cs->timesfallen[1] == 0, "stage 20: not yet");
  F->run.losepoints = true;
  F->run.statgain = 77;
  career_stage_tick(cs, &F->r, &F->s, &F->run, w);
  CHECK(cs->lives[1] == 1000, "stage 20: respawned marks lives 1000 (%d)", cs->lives[1]);
  CHECK(cs->timesfallen[1] == 1, "stage 20: timesfallen 1");
  CHECK(cs->respawning[1], "stage 20: respawning");
  CHECK(F->mads[1].hitmag == 0 && !F->mads[1].dest, "stage 20: fixed");
  CHECK(F->cars[1].x == F->cp.x[2] && F->cars[1].z == F->cp.z[2] && F->cars[1].y == F->cp.y[2] - 250,
        "stage 20: on checkpoint 2 (%d,%d,%d)", F->cars[1].x, F->cars[1].y, F->cars[1].z);
  CHECK(F->run.statgain == 0 && !F->run.losepoints, "stage 20: the respawn costs the stat gain");
  // The reset keeps the race's progress.
  F->mads[1].clear = 9;
  F->mads[1].nlaps = 1;
  career_stage_respawn_reset(cs, &F->mads[1], F->mads[1].cn, &F->cars[1], &F->cp);
  CHECK(!cs->respawning[1] && F->mads[1].clear == 9 && F->mads[1].nlaps == 1, "stage 20: reset keeps progress");
  // Back inside: the count clears.
  career_stage_tick(cs, &F->r, &F->s, &F->run, w);
  CHECK(cs->lives[1] == 0, "stage 20: inside again");
  // A live car out there is pulled back in first.
  F->cars[1].x = WALL_L - 5000;
  career_stage_tick(cs, &F->r, &F->s, &F->run, w);
  CHECK(F->cars[1].x == WALL_L - 1000 && cs->lives[1] == 0, "stage 20: realwalls first (%d)", F->cars[1].x);
  // The second fall waits 60 ticks.
  F->cars[1].x = WALL_L - 5000;
  F->mads[1].dest = true;
  for (t = 0; t < 60; t++) career_stage_tick(cs, &F->r, &F->s, &F->run, w);
  CHECK(cs->timesfallen[1] == 1 && cs->lives[1] == 60, "stage 20: second fall waits 60 (%d)", cs->lives[1]);
  career_stage_tick(cs, &F->r, &F->s, &F->run, w);
  CHECK(cs->timesfallen[1] == 2 && cs->lives[1] == 1000, "stage 20: second respawn");
  // Falling below -1000 with no count running: nothing.
  career_stage_respawn_reset(cs, &F->mads[1], F->mads[1].cn, &F->cars[1], &F->cp);
  F->cars[1].x = WALL_L - 5000;
  F->cars[1].y = -5000;
  F->mads[1].hitmag = 0;
  F->mads[1].dest = true;
  career_stage_tick(cs, &F->r, &F->s, &F->run, w);
  CHECK(cs->lives[1] == 0 && F->mads[1].hitmag == 0, "stage 20: high in the air out of the arena counts nothing");
  // Stage 20's speed cut: top speed 400 less.
  career_stage_stats(cs, &F->r, 0, &F->live[0], &F->orig, F->mads[0].cn, F->live[0].grip[F->mads[0].cn], &F->cp);
  CHECK(F->live[0].swits[F->mads[0].cn][2] ==
            (F->orig.swits[F->mads[0].cn][2] + F->r.sp[0][CS_TS] - 400 < 20
                 ? 20
                 : F->orig.swits[F->mads[0].cn][2] + F->r.sp[0][CS_TS] - 400),
        "stage 20: speed cut");
  career_stage_free(cs);
}

// ---- stage 13: floors and portals ---------------------------------------------------------

static void test_stage13(void) {
  build(13, 0, true);
  CareerStage *cs = &F->cs;
  CareerStageWorld *w = &F->w;
  const int32_t np = w->nplayers;
  // The grid's floors (career_grid).
  for (int32_t k = 0; k < np; k++) {
    int32_t x, y, z, fl;
    career_grid(&F->r, &F->s, k, 0, &x, &y, &z, &fl);
    CHECK(cs->floor[k] == fl, "stage 13: slot %d floor %d (grid %d)", k, cs->floor[k], fl);
  }
  w->starcnt = 0;
  career_stage_tick(cs, &F->r, &F->s, &F->run, w);
  for (int32_t k = 0; k < np; k++) {
    const float gl = cs->floor[k] == 0 ? 250.0f : (float)cs->floor[k] * -10000.0f;
    CHECK(cs->groundlevel[k] == gl, "stage 13: slot %d ground %g", k, (double)cs->groundlevel[k]);
    CHECK(career_phys_groundlevel(cs, k) == gl, "stage 13: phys ground");
    if (k > 0) CHECK(cs->norender[k] == (cs->floor[k] != cs->floor[0]), "stage 13: slot %d hidden off floor", k);
  }
  // A checkpoint on another floor does not count.
  CHECK(career_phys_rightfloor(cs, 0, cs->floor[0]) && !career_phys_rightfloor(cs, 0, cs->floor[0] + 1),
        "stage 13: rightfloor");
  // The guardians: slots 2, 3, 5, 6, 8, 9.
  career_stage_before_drive(cs, w);
  for (int32_t k = 0; k < np; k++) career_stage_portal_move(cs, w, k);
  for (int32_t k = 0; k < np; k++) {
    const bool g = !(k % 3 == 1 || k == 0 || k > 9);
    CHECK(cs->floorguardian[k] == g, "stage 13: guardian %d", k);
  }
  // A portal: checkpoint 3 sends to floor 2. The player drives into it.
  F->cp.telefloor[3] = 2;
  F->cars[0].x = F->cp.x[3];
  F->cars[0].z = F->cp.z[3];
  F->cars[0].y = F->cp.y[3];
  career_stage_portal_detect(cs, w, 0);
  CHECK(cs->forcehandb[0] && cs->sendtofloor[0] == 2, "stage 13: portal caught");
  CHECK(career_phys_forcehandb(cs, 0), "stage 13: phys forcehandb");
  // Braking in: a twentieth of the entry speed a tick, 20 ticks.
  career_phys_ground_speed(cs, 0, 200.0f);   // ignored: already braking
  bool apply = false;
  const float step20 = career_phys_brake(cs, 0, 7.0f, false, &apply);
  CHECK(apply && step20 == fabsf(cs->initialspeed[0]) / 20.0f, "stage 13: portal brake step");
  for (int32_t i = 0; i < 19; i++) career_phys_brake(cs, 0, 7.0f, false, &apply);
  CHECK(cs->teletimer[0] == 20, "stage 13: teletimer 20 (%d)", cs->teletimer[0]);
  F->mads[0].mtouch = true;
  F->mads[0].capsized = false;
  // Fade out (17 ticks of 15) and settle (4 still ticks), then the move.
  int32_t ticks = 0;
  while (cs->forcehandb[0] && ticks < 100) {
    career_stage_portal_move(cs, w, 0);
    ticks++;
  }
  CHECK(!cs->forcehandb[0], "stage 13: arrived");
  CHECK(cs->floor[0] == 2, "stage 13: floor 2 (%d)", cs->floor[0]);
  CHECK(F->cars[0].x == 0 && F->cars[0].z == 0 && F->cars[0].y == -20000 - F->cars[0].grat, "stage 13: on floor 2");
  CHECK(cs->speedhack[0] == 10 && cs->teleinvul[0] == 30, "stage 13: arrival boost and invulnerability");
  CHECK(cs->portal_arrived[0] && cs->portal_whichset[0] == 3, "stage 13: bot set %d", cs->portal_whichset[0]);
  CHECK(F->controls[0].setfixfloor, "stage 13: setfixfloor");
  career_stage_tick(cs, &F->r, &F->s, &F->run, w);
  CHECK(cs->groundlevel[0] == -20000.0f, "stage 13: player's ground on floor 2");
  CHECK(cs->speedhack[0] == 9 && F->mads[0].speed == (float)F->live[0].swits[F->mads[0].cn][2],
        "stage 13: top speed after the portal");
  // While invulnerable after the portal nobody touches the player.
  career_stage_ghostmode(cs, &F->r, w, F->ghost);
  CHECK(F->ghost[0][1] && F->ghost[0][5], "stage 13: teleinvul ghosts");
  career_stage_free(cs);
}

// ---- stage 17: the undead hunt ------------------------------------------------------

static void test_stage17(void) {
  build(17, 0, false);
  CareerStage *cs = &F->cs;
  CareerStageWorld *w = &F->w;
  const int32_t np = w->nplayers;
  w->starcnt = 0;
  for (int32_t t = 0; t < 499; t++) {
    career_stage_tick(cs, &F->r, &F->s, &F->run, w);
    for (int32_t a = 1; a < 4; a++) {
      CHECK(F->cars[a].x == WALL_R + 1000000 && F->cars[a].y == -20000, "stage 17: undead %d held off (tick %d)", a, t);
      CHECK(F->cp.clear[a] == -2 && F->mads[a].power == 98.0f && cs->noarrow[a], "stage 17: undead %d out", a);
    }
  }
  CHECK(cs->undeadswitch == 499, "stage 17: switch 499 (%d)", cs->undeadswitch);
  // The leader: slot 6 in first place with 3 checkpoints.
  for (int32_t k = 0; k < np; k++) {
    F->cp.pos[k] = k == 6 ? 0 : k + 1;
    F->cp.clear[k] = k == 6 ? 5 : 1;
  }
  F->cars[6].x = 1234;
  F->cars[6].y = 250;
  F->cars[6].z = 40000;
  career_stage_tick(cs, &F->r, &F->s, &F->run, w);   // 499 -> 500
  CHECK(cs->undeadswitch == 500, "stage 17: switch 500");
  CHECK(F->cars[1].x == WALL_R + 1000000, "stage 17: still held at 500 before the hunt");
  career_stage_tick(cs, &F->r, &F->s, &F->run, w);   // the hunt
  CHECK(cs->undeadtarget == 6 && cs->sendwarning, "stage 17: target the leader (%d)", cs->undeadtarget);
  // attack() is called again for every slot after the target (6..np-1), 3 each.
  CHECK(cs->undeadswitch == 500 + 3 * (np - 6), "stage 17: switch after the hunt %d", cs->undeadswitch);
  for (int32_t a = 1; a < 4; a++)
    CHECK(F->cars[a].x == 1234 && F->cars[a].z == 40000 && F->cars[a].y == 250 - 1500, "stage 17: undead %d dropped", a);
  CHECK(strstr(cs->msg, "targeting") != NULL, "stage 17: warning '%s'", cs->msg);
  // Released: the next ticks leave them where they are.
  F->cars[1].x = 777;
  career_stage_tick(cs, &F->r, &F->s, &F->run, w);
  CHECK(F->cars[1].x == 777, "stage 17: released");
  // The undead collide only with their target.
  career_stage_ghostmode(cs, &F->r, w, F->ghost);
  CHECK(!F->ghost[1][6] && F->ghost[1][0] && F->ghost[2][5], "stage 17: undead ghost all but the target");
  // A wasted car is taken off after 80 ticks.
  F->mads[8].dest = true;
  for (int32_t t = 0; t < 82; t++) career_stage_tick(cs, &F->r, &F->s, &F->run, w);
  CHECK(cs->norender[8] && F->cars[8].y == -100000, "stage 17: wasted car gone");
  CHECK(career_stage_undeadextra(cs) == 3, "stage 17: undeadextra 3");
  career_stage_free(cs);
}

// ---- stage 23: the Titan ------------------------------------------------------------

static void test_stage23(void) {
  build(23, 0, true);
  CareerStage *cs = &F->cs;
  CareerStageWorld *w = &F->w;
  const int32_t np = w->nplayers;
  w->starcnt = 0;
  career_stage_tick(cs, &F->r, &F->s, &F->run, w);
  CHECK(cs->undead[1], "stage 23: Titan undead");
  CHECK(F->cars[1].z == -150000, "stage 23: Titan hidden");
  CHECK(career_stage_undeadextra(cs) == 1, "stage 23: undeadextra 1");
  // The player wins: the fight instead.
  CHECK(career_stage_win_is_boss(cs) && cs->bossbattle, "stage 23: win wakes the Titan");
  CHECK(career_stage_finish_end_allowed(cs) && career_stage_wasted_end_allowed(cs), "stage 23: hold allowed");
  CHECK(career_stage_hold_advance(cs, w) && cs->cstimer == 2, "stage 23: hold card -> cstimer 2 (%d)", cs->cstimer);
  for (int32_t a = 2; a < np; a++)
    CHECK(F->mads[a].hitmag == F->mads[a].cd->maxmag[F->mads[a].cn] + 1, "stage 23: car %d wrecked", a);
  CHECK(F->mads[0].hitmag == 1, "stage 23: the player must fix");
  CHECK(!career_stage_finish_end_allowed(cs) && !career_stage_wasted_end_allowed(cs), "stage 23: no end during");
  F->cp.haltall = true;
  career_stage_tick(cs, &F->r, &F->s, &F->run, w);
  CHECK(cs->release_hold && !F->cp.haltall && cs->music_event == CAREER_MUSIC_STOP, "stage 23: released, music off");
  CHECK(strcmp(cs->msg, "Fix your car at the fix hoop!") == 0, "stage 23: fix message '%s'", cs->msg);
  CHECK(cs->cstimer == 2 && cs->unlimitedlaps, "stage 23: waits for the fix");
  career_stage_tick(cs, &F->r, &F->s, &F->run, w);
  CHECK(cs->music_event == CAREER_MUSIC_NONE, "stage 23: music stop once");
  // The wrecked field is not held by realwalls (it is wasted by now).
  for (int32_t a = 2; a < np; a++) F->mads[a].dest = true;
  F->mads[0].hitmag = 0;
  F->mads[0].mtouch = true;
  // One frame runs 2 -> 3 (fixed), 3 -> 4 (the fight starts) and 4 -> 5.
  career_stage_tick(cs, &F->r, &F->s, &F->run, w);
  CHECK(cs->music_event == CAREER_MUSIC_BOSS && cs->cstimer == 5, "stage 23: boss music, cstimer 5 (%d)", cs->cstimer);
  CHECK(strcmp(cs->msg, "Damage Titan by stunting!") == 0, "stage 23: first stunting message");
  CHECK(F->cars[1].z == F->cars[0].z + 9000 || F->cars[1].z == F->cars[0].z - 9000 || F->cars[1].x == F->cars[0].x - 9000 ||
            F->cars[1].x == F->cars[0].x + 9000,
        "stage 23: Titan teleported next to the player");
  for (int32_t a = 2; a < np; a++) CHECK(F->cars[a].z == 500000 + a * 1000, "stage 23: field moved away");
  CHECK(career_phys_gravity(cs, 0) == 25.0f && career_phys_gravity(cs, 1) == 7.0f, "stage 23: player's gravity 25");
  CHECK(career_phys_nofix(cs, &F->mads[1]) && !career_phys_nofix(cs, &F->mads[0]), "stage 23: no fixes for the field");
  career_stage_ghostmode(cs, &F->r, w, F->ghost);
  CHECK(F->ghost[1][2] && !F->ghost[0][1] && !F->ghost[1][0], "stage 23: the fight's gating");
  // 176 ticks of "Damage Titan by stunting!".
  int32_t msgs = 0;
  while (cs->cstimer < 180) {
    career_stage_tick(cs, &F->r, &F->s, &F->run, w);
    msgs += strcmp(cs->msg, "Damage Titan by stunting!") == 0;
    CHECK(cs->boss_bar, "stage 23: bar shown");
  }
  CHECK(msgs == 175, "stage 23: stunting message %d", msgs);
  // Stunts hurt it: 1200 of 2400 shown, 1200 of 2500 counted.
  F->mads[0].stunt_gain = 1200.9f;
  career_stage_tick(cs, &F->r, &F->s, &F->run, w);
  F->mads[0].stunt_gain = 0.0f;
  CHECK(cs->stunthealth == 1200, "stage 23: stunthealth %d", cs->stunthealth);
  CHECK(fabs(cs->boss_fill - 0.5) < 1e-9 && strcmp(cs->boss_pct, "50.0 %") == 0, "stage 23: bar %s", cs->boss_pct);
  CHECK(F->mads[1].hitmag == 0, "stage 23: Titan's damage held at 0");
  // At 60% the field's np-5 comes back.
  F->mads[np - 5].dest = true;
  F->mads[0].stunt_gain = 400.0f;
  career_stage_tick(cs, &F->r, &F->s, &F->run, w);
  F->mads[0].stunt_gain = 0.0f;
  CHECK(cs->revive[np - 5] && !F->mads[np - 5].dest, "stage 23: revive at > 60");
  // Down.
  F->mads[0].stunt_gain = 2000.0f;
  career_stage_tick(cs, &F->r, &F->s, &F->run, w);
  CHECK(cs->cstimer == 10000 && !cs->bossbattle, "stage 23: Titan down (%d)", cs->cstimer);
  for (int32_t a = 1; a < np; a++)
    CHECK(F->mads[a].hitmag == F->mads[a].cd->maxmag[F->mads[a].cn] + 1, "stage 23: car %d wrecked at the end", a);
  // The beaten Titan stays undead and does not count (this port's fix, career_stage_undeadextra).
  CHECK(career_stage_wasted_end_allowed(cs) && career_stage_undeadextra(cs) == 1, "stage 23: the end may come");
  CHECK(!career_stage_win_is_boss(cs), "stage 23: no second fight");
  career_stage_free(cs);
}

// ---- stage 11: the vans ------------------------------------------------------------------

static void test_stage11(void) {
  build(11, 0, true);
  CareerStage *cs = &F->cs;
  CareerStageWorld *w = &F->w;
  w->starcnt = 0;
  career_stage_tick(cs, &F->r, &F->s, &F->run, w);
  // Once undead the vans are out of realwalls' reach and their yards hold them.
  F->cars[2].x = 100000;
  F->cars[2].z = 100000;
  career_stage_tick(cs, &F->r, &F->s, &F->run, w);
  for (int32_t a = 1; a < 5; a++) CHECK(cs->undead[a] && cs->newflame[a] && cs->noarrow[a], "stage 11: van %d", a);
  CHECK(F->cars[2].x == 26800 - 700 && F->cars[2].z == 24400 - 700, "stage 11: van 2 in its yard (%d,%d)", F->cars[2].x,
        F->cars[2].z);
  CHECK(cs->statdrain[0] == 2 && cs->statdrain[5] == 2, "stage 11: drain counting");
  CHECK(career_stage_undeadextra(cs) == 4, "stage 11: undeadextra 4");
  CHECK(career_phys_no_trackers(cs, 3) && !career_phys_no_trackers(cs, 1) && !career_phys_no_trackers(cs, 5),
        "stage 11: vans 2-4 ignore the trackers");
  // A wrecked opponent turns undead and counts as wasted.
  CHECK(career_phys_fakedest_stage(cs, 6) && !career_phys_fakedest_stage(cs, 0), "stage 11: fakedest slots");
  career_phys_wreck_done(cs, &F->mads[6]);
  career_stage_tick(cs, &F->r, &F->s, &F->run, w);
  CHECK(cs->undead[6] && cs->fakedest[6] && F->cp.wasted == 1, "stage 11: fakedest counts wasted (%d)", F->cp.wasted);
  CHECK(career_phys_nofix(cs, &F->mads[6]), "stage 11: undead can not fix");
  // The drain lowers the stats once the car has gone long unfixed.
  for (int32_t t = 0; t < 3000; t++) career_stage_tick(cs, &F->r, &F->s, &F->run, w);
  CarDefine before = F->orig;
  const int32_t cn = F->mads[0].cn;
  career_apply_stats(&before, cn, F->r.sc[0], F->r.sp[0], F->r.level[0], F->r.shadow[0], true);
  career_stage_stats(cs, &F->r, 0, &F->live[0], &F->orig, cn, 10.0f, &F->cp);
  CHECK(F->live[0].grip[cn] < before.grip[cn] && F->live[0].airs[cn] < before.airs[cn], "stage 11: drained %g < %g",
        (double)F->live[0].grip[cn], (double)before.grip[cn]);
  CHECK(cs->nuclearmod[0] < 1.0f, "stage 11: nuclearmod");
  career_stage_free(cs);
}

// ---- stage 21: the crumbling entry ------------------------------------------------------

static void test_stage21(void) {
  build(21, 0, false);
  CareerStage *cs = &F->cs;
  CareerStageWorld *w = &F->w;
  const int32_t np = w->nplayers;
  w->starcnt = 0;
  F->t.nt = 100;
  for (int32_t k = 0; k < np; k++) F->cars[k].z = 0;   // all on the entry road
  career_stage_tick(cs, &F->r, &F->s, &F->run, w);
  CHECK(!cs->crumble && !cs->entered[1], "stage 21: nobody in");
  CHECK(career_phys_gravity(cs, 1) == 50.0f, "stage 21: heavy before the entry");
  for (int32_t k = 1; k < np; k++) F->cars[k].z = WALL_B - 2000;
  career_stage_tick(cs, &F->r, &F->s, &F->run, w);
  CHECK(cs->crumble && cs->startfalling == np - 1, "stage 21: crumbles once all are in");
  CHECK(career_phys_gravity(cs, 1) == 7.0f && career_phys_noslow(cs, 1), "stage 21: in");
  for (int32_t t = 0; t < 20; t++) career_stage_tick(cs, &F->r, &F->s, &F->run, w);
  CHECK(F->pieces[0].x == -300000 && F->t.x[60] == -300000 && F->t.x[61] != -300000, "stage 21: the road falls");
  CHECK(F->pieces[0].fade == 255, "stage 21: faded out");
  career_stage_ghostmode(cs, &F->r, w, F->ghost);
  CHECK(F->ghost[1][2], "stage 21: entered cars do not collide");
  career_stage_free(cs);
}

// ---- stage 19: glitches -------------------------------------------------------------------

static void test_stage19(void) {
  build(19, 0, false);
  CareerStage *cs = &F->cs;
  CareerStageWorld *w = &F->w;
  w->starcnt = 0;
  // A car with grip well short of the field's: glitches every 800 ticks.
  for (int32_t k = 0; k < w->nplayers; k++) F->live[k].grip[F->mads[k].cn] = 10.0f;
  int32_t tele = 0, lastz = F->cars[0].z;
  for (int32_t t = 0; t < 2500; t++) {
    career_stage_tick(cs, &F->r, &F->s, &F->run, w);
    if (F->cars[0].y == -350 || F->cars[0].y == -7000) tele++;
    F->cars[0].y = 250;
    lastz = F->cars[0].z;
  }
  (void)lastz;
  CHECK(tele == 3, "stage 19: player glitched %d times in 2500 ticks", tele);
  CHECK(cs->glitchtimer[0] > 2000, "stage 19: glitchtimer %d", cs->glitchtimer[0]);
  career_stage_free(cs);
}

// ---- stage 22: night ---------------------------------------------------------------

static void test_stage22(void) {
  build(22, 0, false);
  CareerStage *cs = &F->cs;
  for (int32_t t = 0; t < 1001; t++) career_stage_frame(cs);
  CHECK(!cs->dn_on && !cs->dn_verydark, "stage 22: day");
  career_stage_frame(cs);
  CHECK(cs->dn_on && cs->dn_changingsnap && cs->dn_dim == 2, "stage 22: dusk");
  for (int32_t t = 0; t < 10; t++) career_stage_frame(cs);
  CHECK(cs->dn_verydark && cs->dn_dim == 20, "stage 22: night");
  for (int32_t t = 0; t < 489; t++) career_stage_frame(cs);
  CHECK(cs->dn_verydark && cs->dn_effecttime == 500, "stage 22: night lasts (%d)", cs->dn_effecttime);
  for (int32_t t = 0; t < 10; t++) career_stage_frame(cs);
  CHECK(cs->dn_dim == 0 && cs->dn_verydark, "stage 22: dawn");
  career_stage_frame(cs);
  CHECK(!cs->dn_verydark && !cs->dn_on && cs->dn_makefase == 0, "stage 22: day again");
  career_stage_free(cs);
}

// ---- the JS oracle's scenarios -----------------------------------------------------
//
// career_stage_cases.h is the real careermode$m (its stage part, XT
// 7584-9074, with the real realwalls / teleport / attack / randtele /
// Madness.respawn) run under node on stubs of the same scripted race as
// below, digested after every tick. Math.random() is java.js's xorshift32
// (seeded per scenario), Medium.random() the LCG test_mrandom. Everything
// here mirrors the oracle script: change one, change the other.

static uint32_t xs_seed;
static double xs_random(void) {
  uint32_t x = xs_seed;
  x ^= x << 13;
  x ^= x >> 17;
  x ^= x << 5;
  xs_seed = x;
  return (double)x / 4294967296.0;
}

static uint32_t g_h;
static int32_t g_dump_i = -1;
static void put(int32_t v) {
  if (g_dump_i >= 0) fprintf(stderr, "%d %d\n", (int)g_dump_i++, (int)v);
  for (int32_t b = 0; b < 4; b++) {
    g_h ^= ((uint32_t)v >> (b * 8)) & 0xffu;
    g_h *= 16777619u;
  }
}
static void putb(bool b) { put(b ? 1 : 0); }

static uint32_t oracle_digest(void) {
  Field *f = F;
  const CareerStage *cs = &f->cs;
  const int32_t np = f->w.nplayers;
  g_h = 0x811c9dc5u;
  for (int32_t k = 0; k < np; k++) {
    const ContO *c = &f->cars[k];
    const Mad *md = &f->mads[k];
    put(c->x); put(c->y); put(c->z); put(c->xz);
    put(md->hitmag); put(jtrunc_d((double)md->power * 100.0)); put(md->clear); put(f->cp.clear[k]);
    put(jtrunc_d((double)md->speed * 100.0));
    putb(md->dest); put(md->nlaps); put(jtrunc_d((double)md->spatk * 100.0)); put(cs->car_inv[k]);
    putb(cs->undead[k]); putb(cs->entered[k]); put(cs->lives[k]); put(cs->timesfallen[k]); put(cs->floor[k]);
    put(cs->speedhack[k]); put(cs->glitchtimer[k]); putb(cs->noarrow[k]); putb(cs->newflame[k]); putb(cs->norender[k]);
    putb(cs->nohit[k]); put(cs->statdrain[k]); putb(cs->safezone[k]); put(cs->undeadlock[k]); put(cs->sp_ts[k]);
    put(cs->xzadjust[k]); putb(cs->respawning[k]); put(cs->destimer[k]); putb(cs->revive[k]); putb(cs->crumblefail[k]);
  }
  const int32_t gl[] = {cs->undeadswitch, cs->undeadtarget, cs->ghosttimer, cs->ghostfade, cs->ghostattempt,
                        cs->ghostattack, cs->ghostfar, cs->ghostflashtimer, cs->ghostteletimer, cs->whatghostdo,
                        cs->randomtimes, cs->scareflashtime, cs->wallimmunity, cs->pieceglitch, cs->cstimer,
                        cs->stunthealth, cs->teledelay, cs->telecooldown, cs->telewait, cs->targetcar, cs->tempinv,
                        cs->startfalling, f->cp.wasted, cs->polyoutline};
  for (size_t i = 0; i < sizeof(gl) / sizeof(gl[0]); i++) put(gl[i]);
  const bool bl[] = {cs->sendwarning, cs->ghostflash[0], cs->ghostflash[1], cs->ghostflash[2], cs->ghosthit,
                     cs->ghosttele, cs->shownghost, cs->scareflash, cs->wallcountdown, cs->bossbattle, cs->verydark,
                     cs->invulnerable, cs->crumble, f->controls[0].mutem, f->cp.haltall, cs->unlimitedlaps};
  for (size_t i = 0; i < sizeof(bl) / sizeof(bl[0]); i++) putb(bl[i]);
  for (int32_t p = 0; p < 160; p++) {
    const ContO *c = &f->pieces[p];
    put(c->x); put(c->y); put(c->z); put(c->xz); put(255 - c->fade); putb(c->glowlines);
  }
  for (int32_t i = 0; i < 64; i++) put(f->t.x[i]);
  for (const char *q = cs->msg; *q; q++) put((unsigned char)*q);
  put(cs->msg[0] ? cs->msg_r : 0);
  return g_h;
}

static void oracle_scenario(int32_t si, int32_t dump_tick) {
  const CsScenario *sc = &cs_scen[si];
  const int32_t stage = sc->stage, bonus = sc->bonus;
  build(stage, bonus, sc->hard != 0);
  xs_seed = (uint32_t)(4242 + stage * 31 + bonus);
  if (!xs_seed) xs_seed = 1;
  g_m = (uint32_t)(7 + stage);
  Field *f = F;
  CareerStage *cs = &f->cs;
  CareerStageWorld *w = &f->w;
  const int32_t np = w->nplayers;
  for (int32_t k = 0; k < np; k++) {
    const int32_t cn = f->mads[k].cn;
    f->r.sp[k][CS_TS] = 5 + k;
    f->r.sp[k][CS_STR] = 10 + k;
    f->live[k].grip[cn] = 18.0f + 1.5f * (float)k;
    f->live[k].swits[cn][2] = 240 + 12 * k;
    f->live[k].moment[cn] = 1.2f + 0.05f * (float)k;
    f->live[k].revpush[cn] = 1.0f;
    f->live[k].maxmag[cn] = 4000 + 250 * k;
  }
  for (int32_t cn = 0; cn < 16; cn++) {
    f->orig.swits[cn][2] = 200 + 7 * cn;
    f->orig.maxmag[cn] = 5000 + 300 * cn;
  }
  for (int32_t i = 0; i < 64; i++) f->t.x[i] = i * 10;
  career_stage_start(cs, &f->r, &f->s, w);
  for (int32_t k = 0; k < np; k++) cs->floor[k] = stage == 13 ? k % 4 : 3;
  const uint32_t *want = cs_digests[si];
  int32_t bad = 0;
  for (int32_t t = 0; t < sc->ticks; t++) {
    for (int32_t k = 0; k < np; k++) {
      f->contva.completed[k] = t / 30 + k < 100 ? t / 30 + k : 100;
      if (f->cp.clear[k] != -2) {
        f->cp.clear[k] = t / sc->rate + k % 4;
        f->mads[k].clear = f->cp.clear[k];
      }
    }
    if (stage == 22) cs->dn_verydark = (t % 400) >= 200;
    for (int32_t e = 0; e < CS_N_EVENTS; e++) {
      const CsEvent *ev = &cs_events[e];
      if (ev->stage != stage || ev->tick != t) continue;
      const int32_t k = ev->slot;
      if (!strcmp(ev->what, "boss")) {
        cs->bossbattle = true;
        cs->cstimer = 2;
        for (int32_t a = 2; a < np; a++) f->mads[a].hitmag = f->live[a].maxmag[f->mads[a].cn] + 1;
        f->mads[0].hitmag = 1;
      } else if (!strcmp(ev->what, "dest")) {
        f->mads[k].dest = ev->value != 0;
      } else if (!strcmp(ev->what, "hitmag")) {
        f->mads[k].hitmag = ev->value;
      } else if (!strcmp(ev->what, "x")) {
        f->cars[k].x = ev->value;
      } else if (!strcmp(ev->what, "y")) {
        f->cars[k].y = ev->value;
      } else if (!strcmp(ev->what, "z")) {
        f->cars[k].z = ev->value;
      }
    }
    if (w->starcnt == 0) {
      career_stage_before_drive(cs, w);
      for (int32_t k = 0; k < np; k++) {
        Mad *md = &f->mads[k];
        if (cs->respawning[k]) {
          career_stage_respawn_reset(cs, md, md->cn, &f->cars[k], &f->cp);
          continue;
        }
        f->cars[k].z += 40 + 7 * k;
        f->cars[k].x += ((t / 50 + k) % 3 - 1) * 30;
        md->speed = (float)(80 + k);
        md->mtouch = (t + k) % 4 != 0;
        md->pxy = (t * 3 + k * 40) % 360;
        if (md->hitmag > md->cd->maxmag[md->cn] && !cs->undead[k]) md->dest = true;
      }
      if (stage == 23 && cs->bossbattle && cs->cstimer >= 4 && t % 40 == 0) f->mads[0].stunt_gain = 130.0f;
    }
    int32_t wasted = 0;
    for (int32_t k = 1; k < np; k++)
      if (f->mads[k].dest || cs->fakedest[k]) wasted++;
    f->cp.wasted = wasted;
    for (int32_t k = 0; k < np; k++) f->cp.pos[k] = k;
    w->holdcnt = f->mads[0].dest ? w->holdcnt + 1 : 0;
    career_stage_tick(cs, &f->r, &f->s, &f->run, w);
    f->mads[0].stunt_gain = 0.0f;
    if (t == dump_tick) g_dump_i = 0;
    const uint32_t got = oracle_digest();
    g_dump_i = -1;
    if (got != want[t] && bad++ < 3)
      CHECK(false, "oracle stage %d bonus %d: tick %d digest %08x, JS %08x", stage, bonus, t, got, want[t]);
    if (w->starcnt > 0) w->starcnt--;
  }
  career_stage_free(cs);
}

static uint32_t fbits(float v) {
  uint32_t u;
  memcpy(&u, &v, sizeof(u));
  return u;
}

/** nitroandspecials' career block (XT 6918-7064) against career_stage_stats. */
static void test_stats_oracle(void) {
  for (int32_t i = 0; i < CS_N_STATS; i++) {
    const int32_t *in = cs_stats[i].in;
    const int32_t stage = in[0], bonus = in[1], slot = in[2];
    build(stage, bonus, false);
    Field *f = F;
    const int32_t np = f->w.nplayers;
    const int32_t cn = port_of(CS_STATS_CAR);
    for (int32_t k = 0; k < 3; k++) {
      f->orig.swits[cn][k] = cs_stats_orig_swits[k];
      f->orig.acelf[cn][k] = cs_stats_orig_acelf[k];
    }
    f->orig.grip[cn] = cs_stats_orig_grip;
    f->orig.airs[cn] = cs_stats_orig_airs;
    f->orig.airc[cn] = cs_stats_orig_airc;
    f->orig.moment[cn] = cs_stats_orig_moment;
    f->orig.maxmag[cn] = cs_stats_orig_maxmag;
    for (int32_t k = 0; k < np; k++) {
      f->r.sc[k] = CS_STATS_CAR;
      f->r.level[k] = k == np - 1 ? in[3] : 8;
      for (int32_t q = 0; q < CS_N; q++) f->r.sp[k][q] = cs_stats_sp[q];
      f->r.beast[k] = false;
      f->r.shadow[k] = false;
    }
    career_stage_start(&f->cs, &f->r, &f->s, &f->w);
    f->cs.statdrain[slot] = in[5];
    f->cp.clear[0] = in[6];
    f->cp.nlaps = in[7];
    f->cp.nsp = in[8];
    CarDefine *base = &f->live[slot];
    career_stage_stats(&f->cs, &f->r, slot, base, &f->orig, cn, (float)in[4], &f->cp);
    const uint32_t got[12] = {(uint32_t)base->swits[cn][0], (uint32_t)base->swits[cn][1], (uint32_t)base->swits[cn][2],
                              fbits(base->acelf[cn][0]),  fbits(base->acelf[cn][1]),  fbits(base->acelf[cn][2]),
                              fbits(base->grip[cn]),      fbits(base->airs[cn]),      (uint32_t)base->airc[cn],
                              (uint32_t)base->maxmag[cn], fbits(f->cs.powermulti[slot]), fbits(f->cs.nuclearmod[slot])};
    for (int32_t q = 0; q < 12; q++)
      CHECK(got[q] == cs_stats[i].out[q], "stats case %d (stage %d bonus %d slot %d grip %d drain %d): field %d %u, JS %u",
            i, stage, bonus, slot, in[4], in[5], q, got[q], cs_stats[i].out[q]);
    career_stage_free(&f->cs);
  }
}

static void test_oracle(void) {
  test_stats_oracle();
  double (*saved)(void) = career_random;
  career_random = xs_random;
  // CS_DUMP=stage,bonus,tick prints the digested words to stderr (the
  // oracle's --dump stage bonus tick prints the JS side's).
  const char *dump = getenv("CS_DUMP");
  int ds = -1, db = -1, dt = -1;
  if (dump) sscanf(dump, "%d,%d,%d", &ds, &db, &dt);
  for (int32_t si = 0; si < CS_N_SCEN; si++)
    oracle_scenario(si, cs_scen[si].stage == ds && cs_scen[si].bonus == db ? dt : -1);
  career_random = saved;
}

// ---- physics helpers ------------------------------------------------------------------

static void test_physics(void) {
  CHECK(career_phys_gravity(NULL, 0) == 7.0f && career_phys_groundlevel(NULL, 0) == 250.0f, "no career: neutral");
  CHECK(!career_phys_nofix(NULL, NULL) && career_phys_wall_damage(NULL, 0, 30.0f) == 1.0f, "no career: neutral 2");
  build(24, 0, false);
  CareerWater wv = career_phys_water(&F->cs, 5, 28.5f);
  CHECK(career_phys_water(&F->cs, 5, 128.5f).accelmod == 0.8f, "stage 24: full grip water");
  CHECK(wv.bouncemod == 1.5f && wv.waterdrag == 0.2f && wv.accelmod == 0.25f, "stage 24: low grip water");
  wv = career_phys_water(&F->cs, 19, 28.5f);
  CHECK(wv.bouncemod == 1.0f && wv.waterdrag == 1.0f, "stage 24: car 19 swims");
  float f7 = 40.0f;
  career_phys_grip(&F->cs, 0, 5, 128.5f, 0, &f7);
  CHECK(F->cs.speedmulti[0] == 1.0f && f7 == 40.0f, "stage 24: full grip speedmulti 1 (%g)", (double)F->cs.speedmulti[0]);
  career_phys_grip(&F->cs, 0, 5, 90.0f, 0, &f7);
  CHECK(F->cs.speedmulti[0] == 0.8f + ((((90.0f - 28.5f) / 100.0f) - 0.55f) / 0.45f) * 0.2f, "stage 24: speedmulti");
  career_stage_free(&F->cs);
  build(16, 0, false);
  f7 = 40.0f;
  career_phys_grip(&F->cs, 0, 5, 31.0f, 1, &f7);
  CHECK(f7 == 40.0f * 0.6f && F->cs.speedmulti[0] == 0.25f, "stage 16: ice on the road %g", (double)f7);
  f7 = 40.0f;
  career_phys_grip(&F->cs, 0, 3, 31.0f, 1, &f7);
  CHECK(f7 == 40.0f, "stage 16: car 3 has chains");
  career_stage_free(&F->cs);
  build(18, 0, false);
  f7 = 40.0f;
  career_phys_grip(&F->cs, 1, 5, 33.5f, 0, &f7);
  CHECK(F->cs.powermulti[1] == 2.5f && F->cs.speedmulti[1] == 0.45f, "stage 18: sand");
  career_stage_free(&F->cs);
  build(15, 0, false);
  F->w.starcnt = 0;
  // Stage 15: give piece 0 a face, put car 0 on it.
  static int32_t ox[4] = {100, 200, 300, 400}, oz[4] = {100, 200, 300, 400}, oy[4] = {0, 0, 0, 0};
  static Plane pl;
  memset(&pl, 0, sizeof(pl));
  pl.n = 4;
  pl.ox = ox;
  pl.oz = oz;
  pl.oy = oy;
  career_stage_free(&F->cs);
  F->pieces[0].npl = 1;
  F->pieces[0].p = &pl;
  F->pieces[0].x = 0;
  F->pieces[0].z = 0;
  career_stage_start(&F->cs, &F->r, &F->s, &F->w);
  // The sort-as-you-copy keeps a 0 in (and drops 100 and 300): 0 .. 400.
  CHECK(F->cs.piece_ext[0] == 0 && F->cs.piece_ext[1] == 400 && F->cs.piece_ext[2] == 0 && F->cs.piece_ext[3] == 400,
        "stage 15: footprint %d %d %d %d", F->cs.piece_ext[0], F->cs.piece_ext[1], F->cs.piece_ext[2],
        F->cs.piece_ext[3]);
  F->cars[0].x = 100;
  F->cars[0].z = 100;
  F->cars[1].x = 100;
  F->cars[1].z = -100;
  career_stage_tick(&F->cs, &F->r, &F->s, &F->run, &F->w);
  CHECK(!F->cs.outoftrack[0] && F->cs.outoftrack[1], "stage 15: on / off the track");
  CHECK(career_phys_gravity(&F->cs, 0) == 10.0f && career_phys_gravity(&F->cs, 1) == 2.0f, "stage 15: gravity");
  CHECK(!career_phys_outoftrack(&F->cs, 0) && career_phys_outoftrack(&F->cs, 1), "stage 15: dive along");
  career_stage_free(&F->cs);
  F->pieces[0].npl = 0;
  F->pieces[0].p = NULL;
}

int main(void) {
  career_random = test_random;
  career_stage_mrandom_hook = test_mrandom;
  F = calloc(1, sizeof(Field));
  if (!F) return 1;
  for (int32_t st = 1; st <= CAREER_STAGES; st++) {
    smoke(st, 0, false);
    smoke(st, 0, true);
  }
  static const int32_t bonus_stage[5] = {0, 5, 11, 15, 18};
  for (int32_t b = 1; b <= 4; b++) smoke(bonus_stage[b], b, false);
  test_stage20();
  test_stage13();
  test_stage17();
  test_stage23();
  test_stage11();
  test_stage21();
  test_stage19();
  test_stage22();
  test_physics();
  test_oracle();
  if (failures) {
    fprintf(stderr, "career_stage_test: %d failure(s)\n", failures);
    return 1;
  }
  printf("career_stage_test: ok\n");
  return 0;
}
