// Host-buildable test for native/core/progress.c.
//
// progress.c has no counterpart in web/ (the web port skips progression,
// see PORT_SPEC.md §M5) so there's no JS oracle to defer to. The
// expected values below come directly from reading xtGraphics.java: the
// initial state at :422-423, the car gates at :5307-5323, the stage
// gates at :2029, the finish() unlock advance at :7002-7017, and the
// scaffold-car table at :6697-6776.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "../core/progress.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
  if (!(cond)) { fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); failures++; } \
} while (0)

static void test_reset(void) {
  GameProgress p;
  memset(&p, 0xaa, sizeof(p));
  game_progress_reset(&p);
  CHECK(p.unlocked[0] == 1 && p.unlocked[1] == 1, "reset unlocked = {1,1}");
  CHECK(p.scm[0] == 0 && p.scm[1] == 0, "reset scm = {0,0}");
  CHECK(!p.justwon1 && !p.justwon2, "reset justwon = false");
}

static void test_car_gates_free_play(void) {
  GameProgress p; game_progress_reset(&p);
  // Free Play: every built-in car index is pickable.
  for (int32_t i = 0; i <= 15; i++) {
    CHECK(game_progress_can_pick_car(&p, GMODE_FREE_PLAY, i), "free play any car");
  }
  // Custom car (>=16) also pickable in Free Play.
  CHECK(game_progress_can_pick_car(&p, GMODE_FREE_PLAY, 16), "free play custom car");
}

static void test_car_gates_nfm1_initial(void) {
  GameProgress p; game_progress_reset(&p); // unlocked[0] == 1
  // Java :5307-5320: initial NFM1 gates block 5, 6, 11, 14, 15.
  CHECK(!game_progress_can_pick_car(&p, GMODE_NFM1, 5), "nfm1 initial locks car 5");
  CHECK(!game_progress_can_pick_car(&p, GMODE_NFM1, 6), "nfm1 initial locks car 6");
  CHECK(!game_progress_can_pick_car(&p, GMODE_NFM1, 11), "nfm1 initial locks car 11");
  CHECK(!game_progress_can_pick_car(&p, GMODE_NFM1, 14), "nfm1 initial locks car 14");
  CHECK(!game_progress_can_pick_car(&p, GMODE_NFM1, 15), "nfm1 initial locks car 15");
  // Everything else unlocked from the start.
  CHECK(game_progress_can_pick_car(&p, GMODE_NFM1, 0), "nfm1 car 0 always");
  CHECK(game_progress_can_pick_car(&p, GMODE_NFM1, 4), "nfm1 car 4 always");
  CHECK(game_progress_can_pick_car(&p, GMODE_NFM1, 7), "nfm1 car 7 always");
  CHECK(game_progress_can_pick_car(&p, GMODE_NFM1, 12), "nfm1 car 12 always");
  // Custom car never in campaign mode.
  CHECK(!game_progress_can_pick_car(&p, GMODE_NFM1, 16), "nfm1 no custom car");
}

static void test_car_gates_nfm1_progressive(void) {
  GameProgress p; game_progress_reset(&p);
  // Java gate is `unlocked[0] <= N` -> locked. So unlocked>N unlocks. Car 5
  // gate is `<=2`, so unlock at unlocked[0]==3.
  p.unlocked[0] = 3;
  CHECK(game_progress_can_pick_car(&p, GMODE_NFM1, 5), "car 5 unlocks at u[0]=3");
  CHECK(!game_progress_can_pick_car(&p, GMODE_NFM1, 6), "car 6 still locked at u[0]=3");

  p.unlocked[0] = 5; CHECK(game_progress_can_pick_car(&p, GMODE_NFM1, 6), "car 6 unlocks at u[0]=5");
  p.unlocked[0] = 7; CHECK(game_progress_can_pick_car(&p, GMODE_NFM1, 11), "car 11 unlocks at u[0]=7");
  p.unlocked[0] = 9; CHECK(game_progress_can_pick_car(&p, GMODE_NFM1, 14), "car 14 unlocks at u[0]=9");
  p.unlocked[0] = 11; CHECK(game_progress_can_pick_car(&p, GMODE_NFM1, 15), "car 15 unlocks at u[0]=11");
}

static void test_car_gates_nfm2(void) {
  GameProgress p; game_progress_reset(&p); // unlocked[1] == 1
  // Java :5323 -- cars 8..15 gated on unlocked[1] <= (car-7)*2.
  // Car 8 gate: unlocked[1] <= 2. Cars 0..7 always pickable.
  for (int32_t i = 0; i <= 7; i++) {
    CHECK(game_progress_can_pick_car(&p, GMODE_NFM2, i), "nfm2 car 0..7 always");
  }
  for (int32_t i = 8; i <= 15; i++) {
    CHECK(!game_progress_can_pick_car(&p, GMODE_NFM2, i), "nfm2 initial locks car 8..15");
  }
  // Unlock car 8 at unlocked[1]==3.
  p.unlocked[1] = 3;
  CHECK(game_progress_can_pick_car(&p, GMODE_NFM2, 8), "car 8 at u[1]=3");
  CHECK(!game_progress_can_pick_car(&p, GMODE_NFM2, 9), "car 9 still locked at u[1]=3");
  // Car 15 gate: <=(15-7)*2=16, unlock at 17.
  p.unlocked[1] = 17;
  CHECK(game_progress_can_pick_car(&p, GMODE_NFM2, 15), "car 15 at u[1]=17");
}

static void test_stage_gates(void) {
  GameProgress p; game_progress_reset(&p);

  // Free Play: 1..27 always pickable, out-of-range not.
  for (int32_t s = 1; s <= 27; s++) {
    CHECK(game_progress_can_pick_stage(&p, GMODE_FREE_PLAY, s), "free play stage 1..27");
  }
  CHECK(!game_progress_can_pick_stage(&p, GMODE_FREE_PLAY, 0), "free play stage 0 no");
  CHECK(!game_progress_can_pick_stage(&p, GMODE_FREE_PLAY, 28), "free play stage 28 no");

  // NFM1 initial: only stage 1 directly playable; 2 shows cantgo.
  CHECK(game_progress_can_pick_stage(&p, GMODE_NFM1, 1), "nfm1 stage 1 ok");
  CHECK(!game_progress_can_pick_stage(&p, GMODE_NFM1, 2), "nfm1 stage 2 locked");
  CHECK(game_progress_stage_in_range(GMODE_NFM1, 2), "nfm1 stage 2 in range");
  CHECK(!game_progress_stage_in_range(GMODE_NFM1, 12), "nfm1 stage 12 out of range");

  // NFM2 initial: only stage 11 directly playable.
  CHECK(!game_progress_can_pick_stage(&p, GMODE_NFM2, 10), "nfm2 stage 10 no");
  CHECK(game_progress_can_pick_stage(&p, GMODE_NFM2, 11), "nfm2 stage 11 ok");
  CHECK(!game_progress_can_pick_stage(&p, GMODE_NFM2, 12), "nfm2 stage 12 locked");

  // After beating stage 3 of NFM1, stages 1..4 are playable.
  p.unlocked[0] = 4;
  for (int32_t s = 1; s <= 4; s++) {
    CHECK(game_progress_can_pick_stage(&p, GMODE_NFM1, s), "nfm1 1..u[0] playable");
  }
  CHECK(!game_progress_can_pick_stage(&p, GMODE_NFM1, 5), "nfm1 u[0]+1 locked");
}

static void test_first_and_default_stage(void) {
  CHECK(game_progress_first_stage(GMODE_FREE_PLAY) == 1, "free play first = 1");
  CHECK(game_progress_first_stage(GMODE_NFM1) == 1, "nfm1 first = 1");
  CHECK(game_progress_first_stage(GMODE_NFM2) == 11, "nfm2 first = 11");

  GameProgress p; game_progress_reset(&p);
  // Java :1914 -- default stage on entering picker: unlocked[gmode-1] (for
  // NFM1) unless campaign complete without justwon1.
  CHECK(game_progress_default_stage(&p, GMODE_NFM1) == 1, "nfm1 default = u[0]");
  p.unlocked[0] = 5;
  CHECK(game_progress_default_stage(&p, GMODE_NFM1) == 5, "nfm1 default advances");
  // Campaign complete + justwon takes the unlocked[0] path, which yields
  // 11 -- and Java :1921 then remaps exactly that value to 27, since NFM1
  // owns stages 1..10 and a bare 11 is an NFM2 stage number. This CHECK
  // asserted 11 before, encoding the missing remap as if it were correct.
  p.unlocked[0] = 11; p.justwon1 = true;
  CHECK(game_progress_default_stage(&p, GMODE_NFM1) == 27, "nfm1 complete + justwon remaps 11 -> 27");
  p.justwon1 = false;
  CHECK(game_progress_default_stage(&p, GMODE_NFM1) == 10, "nfm1 complete no justwon = 10");
}

static void test_finish_progression_nfm1(void) {
  GameProgress p; game_progress_reset(&p);

  // Loss on threshold stage: nothing advances, justwon stays false.
  game_progress_finish_stage(&p, GMODE_NFM1, 1, false);
  CHECK(p.unlocked[0] == 1, "loss no advance");
  CHECK(!p.justwon1, "loss no justwon");

  // Win on threshold stage (unlocked[0]==1 => threshold is 1). Advances.
  game_progress_finish_stage(&p, GMODE_NFM1, 1, true);
  CHECK(p.unlocked[0] == 2, "win advances");
  CHECK(p.justwon1, "win latches justwon1");
  CHECK(p.scm[0] == 0, "stage 1 grants no bonus car");

  // Now threshold is 2. Winning stage 2 unlocks car 5.
  game_progress_finish_stage(&p, GMODE_NFM1, 2, true);
  CHECK(p.unlocked[0] == 3, "advance to 3");
  CHECK(p.scm[0] == 5, "stage 2 grants car 5");

  // Replaying stage 1 (below threshold) does not re-advance.
  int32_t prev_u = p.unlocked[0];
  game_progress_finish_stage(&p, GMODE_NFM1, 1, true);
  CHECK(p.unlocked[0] == prev_u, "replay doesn't advance");
  CHECK(!p.justwon1, "replay clears justwon");

  // Rest of the bonus-car table.
  p.unlocked[0] = 4;
  game_progress_finish_stage(&p, GMODE_NFM1, 4, true);
  CHECK(p.scm[0] == 6, "stage 4 -> car 6");
  p.unlocked[0] = 6;
  game_progress_finish_stage(&p, GMODE_NFM1, 6, true);
  CHECK(p.scm[0] == 11, "stage 6 -> car 11");
  p.unlocked[0] = 8;
  game_progress_finish_stage(&p, GMODE_NFM1, 8, true);
  CHECK(p.scm[0] == 14, "stage 8 -> car 14");
  p.unlocked[0] = 10;
  game_progress_finish_stage(&p, GMODE_NFM1, 10, true);
  CHECK(p.scm[0] == 15, "stage 10 -> car 15");
  CHECK(p.unlocked[0] == 11, "campaign complete");
}

static void test_finish_progression_nfm2(void) {
  GameProgress p; game_progress_reset(&p);

  // NFM2 threshold is `unlocked[1] + 10`, so initial threshold is stage 11.
  game_progress_finish_stage(&p, GMODE_NFM2, 11, true);
  CHECK(p.unlocked[1] == 2, "nfm2 win advances");
  CHECK(p.justwon2, "nfm2 justwon2 latches");

  // Threshold now 12; grants car 8.
  game_progress_finish_stage(&p, GMODE_NFM2, 12, true);
  CHECK(p.scm[1] == 8, "stage 12 -> car 8");

  // Stage 27 does NOT advance (Java :6692 excludes it).
  p.unlocked[1] = 17;
  game_progress_finish_stage(&p, GMODE_NFM2, 27, true);
  CHECK(p.unlocked[1] == 17, "stage 27 doesn't advance");
}

static void test_finish_free_play_noop(void) {
  GameProgress p; game_progress_reset(&p);
  game_progress_finish_stage(&p, GMODE_FREE_PLAY, 1, true);
  CHECK(p.unlocked[0] == 1 && p.unlocked[1] == 1, "free play never advances");
  CHECK(!p.justwon1 && !p.justwon2, "free play never sets justwon");
}

static void test_persistence(void) {
  // game_progress_save_to_disk/load_from_disk now take an explicit path
  // (Part 18 -- see native/TASKS_NATIVE.md: path resolution moved out to
  // platform/common/platform.h's platform_progress_path() so this file
  // stays fully platform-agnostic, no getenv/XDG involved here at all),
  // so this test just points straight at a scratch dir -- hermetic, no
  // env var redirection needed.
  char tmpl[] = "/tmp/nfm_progress_XXXXXX";
  char *tmpdir = mkdtemp(tmpl);
  CHECK(tmpdir != NULL, "mkdtemp");
  if (!tmpdir) return;
  char path[1024];
  snprintf(path, sizeof(path), "%s/nfm-psivta/progress.bin", tmpdir);

  GameProgress p; game_progress_reset(&p);
  p.unlocked[0] = 7;
  p.unlocked[1] = 3;
  p.scm[0] = 11;
  p.scm[1] = 8;
  p.justwon1 = true; // per-race latch, should NOT survive round-trip

  CHECK(game_progress_save_to_disk(&p, path), "save ok");

  GameProgress q; memset(&q, 0, sizeof(q));
  CHECK(game_progress_load_from_disk(&q, path), "load ok");
  CHECK(q.unlocked[0] == 7, "u[0] round-trips");
  CHECK(q.unlocked[1] == 3, "u[1] round-trips");
  CHECK(q.scm[0] == 11, "scm[0] round-trips");
  CHECK(q.scm[1] == 8, "scm[1] round-trips");
  CHECK(!q.justwon1 && !q.justwon2, "justwon reset on load");

  // Corrupt: overwrite the file with junk -> load resets to defaults.
  FILE *f = fopen(path, "wb");
  CHECK(f != NULL, "reopen for junk");
  if (f) { fwrite("garbage!", 1, 8, f); fclose(f); }
  GameProgress r;
  CHECK(!game_progress_load_from_disk(&r, path), "junk load fails");
  CHECK(r.unlocked[0] == 1 && r.unlocked[1] == 1, "junk load resets");

  // Missing file -> load resets to defaults.
  unlink(path);
  GameProgress s;
  CHECK(!game_progress_load_from_disk(&s, path), "missing load fails");
  CHECK(s.unlocked[0] == 1, "missing load resets");

  // Cleanup: rm scratch dir.
  char nfm_dir[1024];
  snprintf(nfm_dir, sizeof(nfm_dir), "%s/nfm-psivta", tmpdir);
  rmdir(nfm_dir);
  rmdir(tmpdir);
}

// Regression coverage for game_progress_bonus_car_for() being exported
// (native/platform/common/game.c's post-race unlock-celebration card
// calls it directly, re-deriving n4 fresh each draw rather than reading
// back GameProgress::scm[] -- see progress.h's own doc comment on why
// scm[] alone would be stale on non-bonus-car unlock stages). Checked
// against every real (gmode,stage) pair from xtGraphics.java:6697-6776.
static void test_bonus_car_for(void) {
  CHECK(game_progress_bonus_car_for(GMODE_NFM1, 1) == 0, "bonus: NFM1 stage 1 -> none");
  CHECK(game_progress_bonus_car_for(GMODE_NFM1, 2) == 5, "bonus: NFM1 stage 2 -> car 5");
  CHECK(game_progress_bonus_car_for(GMODE_NFM1, 4) == 6, "bonus: NFM1 stage 4 -> car 6");
  CHECK(game_progress_bonus_car_for(GMODE_NFM1, 6) == 11, "bonus: NFM1 stage 6 -> car 11");
  CHECK(game_progress_bonus_car_for(GMODE_NFM1, 8) == 14, "bonus: NFM1 stage 8 -> car 14");
  CHECK(game_progress_bonus_car_for(GMODE_NFM1, 10) == 15, "bonus: NFM1 stage 10 -> car 15");
  CHECK(game_progress_bonus_car_for(GMODE_NFM2, 12) == 8, "bonus: NFM2 stage 12 -> car 8");
  CHECK(game_progress_bonus_car_for(GMODE_NFM2, 14) == 9, "bonus: NFM2 stage 14 -> car 9");
  CHECK(game_progress_bonus_car_for(GMODE_NFM2, 16) == 10, "bonus: NFM2 stage 16 -> car 10");
  CHECK(game_progress_bonus_car_for(GMODE_NFM2, 18) == 11, "bonus: NFM2 stage 18 -> car 11");
  CHECK(game_progress_bonus_car_for(GMODE_NFM2, 20) == 12, "bonus: NFM2 stage 20 -> car 12");
  CHECK(game_progress_bonus_car_for(GMODE_NFM2, 22) == 13, "bonus: NFM2 stage 22 -> car 13");
  CHECK(game_progress_bonus_car_for(GMODE_NFM2, 24) == 14, "bonus: NFM2 stage 24 -> car 14");
  CHECK(game_progress_bonus_car_for(GMODE_NFM2, 26) == 15, "bonus: NFM2 stage 26 -> car 15");
  CHECK(game_progress_bonus_car_for(GMODE_NFM2, 13) == 0, "bonus: NFM2 non-threshold stage -> none");
  CHECK(game_progress_bonus_car_for(GMODE_FREE_PLAY, 2) == 0, "bonus: Free Play -> none");
}

// Regression coverage for game_progress_car_unlock_stage() -- the locked-
// car gate overlay's "This car unlocks when stage K is completed..."
// message value, distinct from game_progress_can_pick_car()'s boolean
// gate. Checked against xtGraphics.java:5306-5325 directly.
static void test_car_unlock_stage(void) {
  GameProgress p;
  game_progress_reset(&p); // unlocked = {1, 1}

  CHECK(game_progress_car_unlock_stage(&p, GMODE_NFM1, 5) == 2, "unlock-stage: NFM1 car 5 -> 2");
  CHECK(game_progress_car_unlock_stage(&p, GMODE_NFM1, 6) == 4, "unlock-stage: NFM1 car 6 -> 4");
  CHECK(game_progress_car_unlock_stage(&p, GMODE_NFM1, 11) == 6, "unlock-stage: NFM1 car 11 -> 6");
  CHECK(game_progress_car_unlock_stage(&p, GMODE_NFM1, 14) == 8, "unlock-stage: NFM1 car 14 -> 8");
  CHECK(game_progress_car_unlock_stage(&p, GMODE_NFM1, 15) == 10, "unlock-stage: NFM1 car 15 -> 10");
  CHECK(game_progress_car_unlock_stage(&p, GMODE_NFM1, 0) == 0, "unlock-stage: NFM1 car 0 (never gated) -> 0");
  // NFM2's k is the campaign-RELATIVE index (car-7)*2, not the absolute
  // stage number -- preserved exactly, see this function's own doc comment.
  CHECK(game_progress_car_unlock_stage(&p, GMODE_NFM2, 8) == 2, "unlock-stage: NFM2 car 8 -> 2 (relative, not stage 12)");
  CHECK(game_progress_car_unlock_stage(&p, GMODE_NFM2, 15) == 16, "unlock-stage: NFM2 car 15 -> 16 (relative, not stage 26)");

  p.unlocked[0] = 11; // NFM1 fully unlocked
  CHECK(game_progress_car_unlock_stage(&p, GMODE_NFM1, 5) == 0, "unlock-stage: NFM1 car 5 unlocked once threshold passed -> 0");

  CHECK(game_progress_car_unlock_stage(&p, GMODE_FREE_PLAY, 5) == 0, "unlock-stage: Free Play -> always 0");
}

static void test_settings_blur(void) {
  const char *prog = "/tmp/nfm_settings_test/progress.bin";
  remove("/tmp/nfm_settings_test/settings.txt");
  GameSettings s = game_settings_defaults(GFX_HD);
  game_settings_load(prog, &s);
  CHECK(s.blur == GAME_SETTINGS_BLUR_DEFAULT && s.graphics == GFX_HD && s.shake && s.rumble,
        "no settings file -> defaults");
  s.blur = 40; s.graphics = GFX_SMOOTH; s.smooth = 0; s.shake = false; s.rumble = false;
  CHECK(game_settings_save(prog, &s), "settings save");
  GameSettings r = game_settings_defaults(GFX_ORIGINAL);
  game_settings_load(prog, &r);
  CHECK(r.blur == 40 && r.graphics == GFX_SMOOTH && !r.smooth && !r.shake && !r.rumble, "settings roundtrip");
  FILE *f = fopen("/tmp/nfm_settings_test/settings.txt", "w");
  if (f) { fputs("motion_blur=57\ngraphics=9\n", f); fclose(f); }
  r = game_settings_defaults(GFX_ORIGINAL);
  game_settings_load(prog, &r);
  CHECK(r.blur == 60, "off-grid value snaps to 60");
  CHECK(r.graphics == GFX_ORIGINAL, "out-of-range graphics keeps the default");
  // A settings.txt from before Graphics / Shake / Vibration existed.
  f = fopen("/tmp/nfm_settings_test/settings.txt", "w");
  if (f) { fputs("motion_blur=20\n", f); fclose(f); }
  r = game_settings_defaults(GFX_HD);
  game_settings_load(prog, &r);
  CHECK(r.blur == 20 && r.graphics == GFX_HD && r.smooth && r.shake && r.rumble, "old file: new settings default");
  // The Graphics / Audio / Interface options.
  s = game_settings_defaults(GFX_HD);
  CHECK(s.draw_dist == 0 && s.detail == 0 && s.shadows && s.particles && s.music_vol == 100 &&
        s.sfx_vol == 100 && s.show_fps == 0, "new options default to the original");
  s.draw_dist = 2; s.detail = 1; s.shadows = 0; s.particles = 0; s.music_vol = 30; s.sfx_vol = 70; s.show_fps = 2;
  CHECK(game_settings_save(prog, &s), "settings save (all options)");
  r = game_settings_defaults(GFX_ORIGINAL);
  game_settings_load(prog, &r);
  CHECK(memcmp(&r, &s, sizeof(r)) == 0, "every option roundtrips");
  f = fopen("/tmp/nfm_settings_test/settings.txt", "w");
  if (f) { fputs("draw_distance=7\nmusic_volume=44\n", f); fclose(f); }
  r = game_settings_defaults(GFX_HD);
  game_settings_load(prog, &r);
  CHECK(r.draw_dist == 0, "a choice off its list keeps the default");
  CHECK(r.music_vol == 40, "a volume snaps to its step of 10");
  remove("/tmp/nfm_settings_test/settings.txt");
}

int main(void) {
  test_settings_blur();
  test_reset();
  test_car_gates_free_play();
  test_car_gates_nfm1_initial();
  test_car_gates_nfm1_progressive();
  test_car_gates_nfm2();
  test_stage_gates();
  test_first_and_default_stage();
  test_finish_progression_nfm1();
  test_finish_progression_nfm2();
  test_finish_free_play_noop();
  test_persistence();
  test_bonus_car_for();
  test_car_unlock_stage();

  if (failures == 0) { printf("all tests passed\n"); return 0; }
  fprintf(stderr, "%d failure(s)\n", failures); return 1;
}
