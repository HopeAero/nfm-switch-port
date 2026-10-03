// Host-buildable test for native/core/bots.c's sortcars() port.
//
// No web/*.js oracle exists for this one -- xtGraphics.java itself is
// out of the web port's scope (see bots.h's own note), so this is a
// straight Java->C translation with no differential JS fixture to test
// against. Verified instead by: (1) structural invariants that MUST
// hold for any valid roll (car indices in range, no duplicate cars
// across slots, sc[0] never touched); (2) the milestone/boss-car
// placement rules, which are deterministic GIVEN the RNG draw sequence
// -- forced into a specific slot whenever the player isn't already
// driving that exact car, independent of what the random rolls
// produced elsewhere; (3) the unlocked[]-gated reroll rules never
// letting a locked car appear as an opponent, run across many seeds to
// catch an off-by-one in the threshold comparisons.
#include <stdio.h>
#include <string.h>
#include "../core/bots.h"
#include "../core/java_compat.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
  if (!(cond)) { fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); failures++; } \
} while (0)

static void check_no_duplicates_and_range(const int32_t sc[7], int32_t n2, const char *label) {
  for (int32_t j = 1; j < n2; j++) {
    char msg[96];
    snprintf(msg, sizeof(msg), "%s: sc[%d] in range 0..15", label, j);
    CHECK(sc[j] >= 0 && sc[j] <= 15, msg);
    for (int32_t k = j + 1; k < n2; k++) {
      snprintf(msg, sizeof(msg), "%s: sc[%d] != sc[%d] (no duplicate car)", label, j, k);
      CHECK(sc[j] != sc[k], msg);
    }
  }
}

static void test_zero_stage_is_noop(void) {
  nfm_set_seed(1);
  int32_t sc[7] = {3, 9, 9, 9, 9, 9, 9}; // deliberately pre-filled with garbage/dupes
  GameProgress p; game_progress_reset(&p);
  bots_sortcars(sc, GMODE_FREE_PLAY, &p, 0);
  CHECK(sc[0] == 3 && sc[1] == 9 && sc[2] == 9, "stage_num==0 leaves sc[] untouched");
}

static void test_free_play_basic_roll(void) {
  nfm_set_seed(42);
  GameProgress p; game_progress_reset(&p);
  p.unlocked[0] = 11; p.unlocked[1] = 17; // fully unlocked, so no gate rejects a roll
  int32_t sc[7] = {0, 0, 0, 0, 0, 0, 0};
  bots_sortcars(sc, GMODE_FREE_PLAY, &p, -1); // Free Play -> internally becomes stage 27
  check_no_duplicates_and_range(sc, 7, "free play stage 27");
  CHECK(sc[0] == 0, "sc[0] (player car) never overwritten");
}

static void test_nfm1_milestone_placement(void) {
  // Java :7071 -- stage 1 or 2, slot 4 (NFM1's n3), forced to car 5
  // UNLESS the player is already driving car 5.
  nfm_set_seed(7);
  GameProgress p; game_progress_reset(&p); // unlocked[0]=1 -- most cars locked
  int32_t sc[7] = {0, 0, 0, 0, 0, 0, 0}; // player driving car 0
  bots_sortcars(sc, GMODE_NFM1, &p, 1);
  CHECK(sc[4] == 5, "NFM1 stage 1: slot 4 forced to car 5 (player not driving it)");

  // Same stage, but player IS driving car 5 -- placement should NOT fire.
  nfm_set_seed(7);
  int32_t sc2[7] = {5, 0, 0, 0, 0, 0, 0};
  bots_sortcars(sc2, GMODE_NFM1, &p, 1);
  CHECK(sc2[4] != 5, "NFM1 stage 1: no forced placement when player already drives car 5");
}

static void test_nfm2_boss_placement(void) {
  // Java :7095-7098 -- NFM2 stage 21 (n=21-10=11): guaranteed rival
  // 7+(11+1)/2 = 7+6 = 13 forced into slot 6, unless already the
  // player's own car.
  nfm_set_seed(3);
  GameProgress p; game_progress_reset(&p);
  p.unlocked[0] = 11; p.unlocked[1] = 17;
  int32_t sc[7] = {0, 0, 0, 0, 0, 0, 0};
  bots_sortcars(sc, GMODE_NFM2, &p, 21);
  CHECK(sc[6] == 13, "NFM2 stage 21: slot 6 forced to car 13 (7+(11+1)/2)");
}

static void test_nfm1_unlock_gate_never_violated(void) {
  // With NFM1 barely started (unlocked[0]=1), cars >5 must never appear
  // as rolled opponents (Java :7142 -- sc[j]>5 && unlocked[0]<=2 rerolls;
  // by extension every higher gate is also active). Run across many
  // seeds/stages to stress the reroll loop's termination and the gate
  // comparisons.
  //
  // Every NFM1 stage 1..10 triggers SOME milestone placement (Java
  // :7071-7090 covers all ten with no gaps) -- that placement is
  // DELIBERATELY exempt from the unlock gate (it's what previews an
  // upcoming rival before their car is generally selectable), so
  // sc[0] is set to stage 3's own milestone car (6) here specifically
  // to suppress it (`sc[0] != 6` becomes false) and let slot 4 fall
  // through to the normal gated reroll path this test actually wants
  // to stress.
  GameProgress p; game_progress_reset(&p); // unlocked = {1,1}
  for (int32_t seed = 1; seed <= 30; seed++) {
    nfm_set_seed((uint32_t)seed);
    int32_t sc[7] = {6, 0, 0, 0, 0, 0, 0};
    bots_sortcars(sc, GMODE_NFM1, &p, 3);
    for (int32_t j = 1; j < 7; j++) {
      if (sc[j] < 0) continue; // slot never filled (n2 boundary) -- fine
      char msg[64];
      snprintf(msg, sizeof(msg), "seed %d: sc[%d]=%d respects unlocked[0]<=2 gate (<=5)", seed, j, sc[j]);
      CHECK(sc[j] <= 5, msg);
    }
  }
}

static void test_nfm2_unlock_gate_never_violated(void) {
  GameProgress p; game_progress_reset(&p); // unlocked[1] = 1
  for (int32_t seed = 1; seed <= 30; seed++) {
    nfm_set_seed((uint32_t)seed);
    int32_t sc[7] = {8, 0, 0, 0, 0, 0, 0}; // player driving car 8 so the stage-11 guaranteed-rival check (7+(1+1)/2=8) is skipped
    bots_sortcars(sc, GMODE_NFM2, &p, 11);
    for (int32_t j = 1; j < 7; j++) {
      if (sc[j] < 0) continue;
      // Gate: (sc[j]-7)*2 > unlocked[1] rerolls -- unlocked[1]=1 means
      // only sc[j]<=7 (since (8-7)*2=2>1) can ever pass.
      char msg[64];
      snprintf(msg, sizeof(msg), "seed %d: sc[%d]=%d respects NFM2 unlock gate", seed, j, sc[j]);
      CHECK(sc[j] <= 7, msg);
    }
  }
}

// Regression test for a real user-reported crash: stages 28-32 are
// multiplayer-only in the original Java (single-player stage select
// clamps at 27, xtGraphics.java:1899,2597), so sortcars() there never
// sees stage_num>27. This port's stage select lets a solo player reach
// 28-32 anyway (matching web/XtGraphics.js's own sortcars(), which
// documents the identical "PORT DIVERGENCE" and clamps n to 27 both
// below AND above for exactly this reason). Without the upper clamp,
// sc[j] can land outside 0..15 (e.g. stage 30 forces sc[6]=17), which
// later crashes cont_o_init_copy() on a null/garbage base model --
// reproduced live via NFM_STAGE_NUM=30 before this fix.
static void test_stages_beyond_27_stay_in_range(void) {
  GameProgress p; game_progress_reset(&p);
  p.unlocked[0] = 11; p.unlocked[1] = 17; // fully unlocked, so no gate rejects a roll
  for (int32_t stage = 28; stage <= 32; stage++) {
    for (int32_t seed = 1; seed <= 10; seed++) {
      nfm_set_seed((uint32_t)seed);
      int32_t sc[7] = {0, 0, 0, 0, 0, 0, 0};
      bots_sortcars(sc, GMODE_FREE_PLAY, &p, stage);
      char label[64];
      snprintf(label, sizeof(label), "stage %d seed %d", stage, seed);
      check_no_duplicates_and_range(sc, 7, label);
    }
  }
}

int main(void) {
  test_zero_stage_is_noop();
  test_free_play_basic_roll();
  test_nfm1_milestone_placement();
  test_nfm2_boss_placement();
  test_nfm1_unlock_gate_never_violated();
  test_nfm2_unlock_gate_never_violated();
  test_stages_beyond_27_stay_in_range();

  if (failures == 0) { printf("all tests passed\n"); return 0; }
  fprintf(stderr, "%d failure(s)\n", failures); return 1;
}
