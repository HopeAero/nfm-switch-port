#include "bots.h"
#include "java_compat.h"

// Java's own `Math.random() > Math.random()` evaluates its two operands
// left-to-right (JS/Java guarantee); C makes no such promise for `>`'s
// operands, so each call is sequenced through a named temporary here --
// same pitfall medium.c's own medium_random() already documents and
// works around for the identical Java idiom.
static bool rand_gt_rand(void) {
  double a = nfm_random();
  double b = nfm_random();
  return a > b;
}

void bots_sortcars(int32_t sc[BOTS_MAX_PLAYERS], GameMode gmode,
                    const GameProgress *progress, int32_t stage_num) {
  int32_t n = stage_num;
  if (n == 0) return; // Java :7053 `if (n != 0)`

  for (int32_t i = 1; i < 7; i++) sc[i] = -1;
  bool settled[7] = {false, false, false, false, false, false, false};

  if (n < 0) n = 27; // Free Play -- Java :7058-7060

  // PORT DIVERGENCE (matches web/XtGraphics.js:2374-2380's own comment on
  // this exact clamp, itself explaining why the real Java never needed
  // it): stages 28-32 are multiplayer-only in the original -- single-
  // player stage select clamps at 27 (xtGraphics.java:1899,2597) -- so
  // sortcars() there never sees n>27. This port's stage select lets a
  // solo player reach 28-32 anyway (see native/docs/MENU_FLOW.md), and
  // without this clamp the car-index formulas below run off the 16-car
  // roster (e.g. n=30 forces opponent index 17, out of base_models[]'s
  // 0-16 range), so a later cont_o_init_copy() reads a null/garbage
  // model and segfaults -- reproduced via NFM_STAGE_NUM=28..32. Clamping
  // to 27 (top NFM2 difficulty) instead is exactly the JS oracle's own
  // fix for the identical divergence.
  if (n > 27) n = 27;

  int32_t n2 = 7;
  if (gmode == GMODE_NFM1) n2 = 5;
  bool b = false; // NFM2 ("high tier") mode

  if (n <= 10) {
    // Java :7066-7091 -- milestone-car placement in NFM1 (or an
    // equivalent early stage in Free Play): the same 5 scaffold cars
    // progress.c's own unlock table advances (5/6/11/14/15), each
    // guaranteed to appear as a rival for a pair of stages UNLESS the
    // player is already driving that exact car.
    int32_t n3 = 6;
    if (gmode == GMODE_NFM1) n3 = 4;
    if ((n == 1 || n == 2) && sc[0] != 5)  { sc[n3] = 5;  n2 = n3; }
    if ((n == 3 || n == 4) && sc[0] != 6)  { sc[n3] = 6;  n2 = n3; }
    if ((n == 5 || n == 6) && sc[0] != 11) { sc[n3] = 11; n2 = n3; }
    if ((n == 7 || n == 8) && sc[0] != 14) { sc[n3] = 14; n2 = n3; }
    if ((n == 9 || n == 10) && sc[0] != 15) { sc[n3] = 15; n2 = n3; }
  } else {
    // Java :7092-7099 -- NFM2: n becomes "stage - 10" for the rest of
    // this function. The guaranteed-rival formula 7+(n+1)/2 walks
    // 7,7,8,8,9,9,...15,15 across NFM2's 17 stages (integer division),
    // skipping adjusted n==17 (actual stage 27, the finale).
    n -= 10;
    b = true;
    if (sc[0] != 7 + (n + 1) / 2 && n != 17) {
      sc[6] = 7 + (n + 1) / 2;
      n2 = 6;
    }
  }

  int32_t n4 = 16; // smallest car index assigned so far
  int32_t n5 = 1;  // slot holding the smallest
  int32_t n6 = 2;  // slot holding the second-smallest

  for (int32_t j = 1; j < n2; j++) {
    settled[j] = false;
    while (!settled[j]) {
      float n7 = b ? 17.0f : 10.0f;
      // Java (int)(Math.random() * (24.0f + 8.0f*(n/n7))) -- n/n7 is a
      // float division (n7 is a Java float literal), matching C's
      // ordinary float arithmetic here (n auto-promotes).
      sc[j] = (int32_t)(nfm_random() * (24.0f + 8.0f * ((float)n / n7)));
      if (sc[j] >= 16) sc[j] -= 16;
      settled[j] = true;
      for (int32_t k = 0; k < 7; k++) {
        if (j != k && sc[j] == sc[k]) settled[j] = false;
      }
      if (b) n7 = 16.0f;
      float n9 = (float)(15 - sc[j]) / 15.0f * ((float)n / n7);
      if (n9 > 0.8f) n9 = 0.8f;
      if (n == 17 && n9 > 0.5f) n9 = 0.5f;
      if (n9 > nfm_random()) settled[j] = false;

      if (gmode == GMODE_NFM1) {
        if (sc[j] >= 7 && sc[j] <= 10) settled[j] = false;
        if (sc[j] == 12 || sc[j] == 13) settled[j] = false;
        if (sc[j] > 5 && progress->unlocked[0] <= 2) settled[j] = false;
        if (sc[j] > 6 && progress->unlocked[0] <= 4) settled[j] = false;
        if (sc[j] > 11 && progress->unlocked[0] <= 6) settled[j] = false;
        if (sc[j] > 14 && progress->unlocked[0] <= 8) settled[j] = false;
      }
      if (gmode == GMODE_NFM2) {
        if ((sc[j] - 7) * 2 > progress->unlocked[1]) settled[j] = false;
        // Java :7159-7163 -- `continue` inside the while loop skips the
        // unconditional `array[j]=false` below UNLESS this IS the
        // special case (adjusted n==16 i.e. actual stage 26, NFM2 fully
        // unlocked, AND this is a low-tier NFM2 car) -- see bots.h's
        // own note on this being deliberately convoluted in the source.
        if (n != 16 || progress->unlocked[1] != 16 || sc[j] >= 9) {
          continue;
        }
        settled[j] = false;
      }
    }
    if (sc[j] < n4) {
      n4 = sc[j];
      if (n5 != j) { n6 = n5; n5 = j; }
    }
  }

  // Java :7173-7263 -- guaranteed "boss" placements for specific NFM2
  // milestone stages, each checking whether the car is ALREADY present
  // among the rolled opponents before forcing it into slot n5 or n6
  // (the two lowest-car-index slots found above).
  if (!b && n == 10) {
    bool has11 = false;
    for (int32_t l = 0; l < 7; l++) if (sc[l] == 11) has11 = true;
    if (!has11 && (rand_gt_rand() || gmode != GMODE_FREE_PLAY)) sc[n5] = 11;
    bool has14 = false;
    for (int32_t l = 0; l < 7; l++) if (sc[l] == 14) has14 = true;
    if (!has14 && (rand_gt_rand() || gmode != GMODE_FREE_PLAY)) sc[n6] = 14;
  }
  if (n == 12) {
    bool has11 = false;
    for (int32_t l = 0; l < 7; l++) if (sc[l] == 11) has11 = true;
    if (!has11) sc[n5] = 11;
  }
  if (n == 14) {
    bool has12 = false;
    for (int32_t l = 0; l < 7; l++) if (sc[l] == 12) has12 = true;
    if (!has12 && (rand_gt_rand() || gmode != GMODE_FREE_PLAY)) sc[n5] = 12;
    bool has10 = false;
    for (int32_t l = 0; l < 7; l++) if (sc[l] == 10) has10 = true;
    if (!has10 && (rand_gt_rand() || gmode != GMODE_FREE_PLAY)) sc[n6] = 10;
  }
  if (n == 15) {
    bool has11 = false;
    for (int32_t l = 0; l < 7; l++) if (sc[l] == 11) has11 = true;
    if (!has11 && (rand_gt_rand() || gmode != GMODE_FREE_PLAY)) sc[n5] = 11;
    bool has13 = false;
    for (int32_t l = 0; l < 7; l++) if (sc[l] == 13) has13 = true;
    if (!has13 && (rand_gt_rand() || gmode != GMODE_FREE_PLAY)) sc[n6] = 13;
  }
  if (n == 16) {
    bool has13 = false;
    for (int32_t l = 0; l < 7; l++) if (sc[l] == 13) has13 = true;
    if (!has13 && (rand_gt_rand() || gmode != GMODE_FREE_PLAY)) sc[n5] = 13;
    bool has12 = false;
    for (int32_t l = 0; l < 7; l++) if (sc[l] == 12) has12 = true;
    if (!has12 && (rand_gt_rand() || gmode != GMODE_FREE_PLAY)) sc[n6] = 12;
  }

  // Java :7264-7301 (cd.lastload==1/2 custom-car-pool injection) --
  // intentionally NOT ported, see bots.h's own scope note.
}
