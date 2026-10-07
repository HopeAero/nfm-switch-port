// See ext_pt.h. XT = Extended's xtGraphics.java.
#include "ext_pt.h"

#include <string.h>

const char *const kExtPTOpponents[EXT_PT_CARS] = {"You",          "Motion",      "Redline",    "Crash",
                                                  "Swift",        "Omega",       "Olsie820",   "RadicalRacer",
                                                  "Kaffeinated",  "InsanElite",  "Grimjow"};
const char *const kExtPTPlayerNames[4] = {"DragShot", "ToaZuka", "Velocity", "KRC"};

void ext_pt_start_tournament(ExtPT *pt) {
  memset(pt, 0, sizeof(*pt));
  pt->match = 1;
  ext_pt_start_match(pt);
}

void ext_pt_start_match(ExtPT *pt) {
  const int32_t match = pt->match;
  int32_t points[EXT_PT_CARS];
  memcpy(points, pt->points, sizeof(points));
  memset(pt, 0, sizeof(*pt));
  pt->match = match;
  memcpy(pt->points, points, sizeof(points));
  for (int32_t a = 0; a < EXT_PT_CARS; a++) pt->position[a] = a;
  pt->ptimer = 1000;
  pt->winner = -1;
}

/** Places by score, then damage, then index (XT's orderscore). Eliminated
 * cars keep the place they went out on. */
static void rank_by_score(ExtPT *pt, const ExtPTCar *cars) {
  int64_t key[EXT_PT_CARS];
  for (int32_t a = 0; a < EXT_PT_CARS; a++) {
    key[a] = pt->eliminated[a] ? -1000000000LL - pt->position[a]
                               : (int64_t)(pt->score[a] + 1) * 1000000 + 100000 - cars[a].hitmag + a;
  }
  for (int32_t a = 0; a < EXT_PT_CARS; a++) {
    if (pt->eliminated[a]) continue;
    int32_t place = 0;
    for (int32_t b = 0; b < EXT_PT_CARS; b++) {
      if (b != a && key[b] > key[a]) place++;
    }
    pt->position[a] = place;
  }
}

void ext_pt_tick(ExtPT *pt, ExtPTCar *cars, const int32_t *pos, const int32_t *clear, int32_t nlaps_nsp,
                 int32_t wasted, bool racing) {
  for (int32_t a = 0; a < EXT_PT_CARS; a++) {
    cars[a].revive_now = cars[a].force_wreck = cars[a].no_damage = cars[a].no_special = false;
  }
  if (pt->over) {
    for (int32_t a = 0; a < EXT_PT_CARS; a++) cars[a].force_wreck = true;   // XT: hitmag = 100000 for all
    return;
  }
  const int32_t m = pt->match;
  if (m == 1 || m == 4 || m == 5) {
    // XT 5637-5669: a wasted car loses a point once and comes back after 40
    // ticks; the car that wasted it scores (the part v2.8 never wired up).
    for (int32_t a = 0; a < EXT_PT_CARS; a++) {
      if (cars[a].dest && !pt->eliminated[a]) {
        if (!pt->counted_death[a]) {
          pt->score[a]--;
          const int32_t k = cars[a].killer;
          if (k >= 0 && k < EXT_PT_CARS && k != a) pt->score[k]++;
          pt->counted_death[a] = true;
        }
        if (++pt->revive[a] > 40) cars[a].revive_now = true;
      } else {
        pt->revive[a] = 0;
        pt->counted_death[a] = false;
      }
      if (m == 1) cars[a].no_special = true;
    }
    if (m == 5 && racing) {
      // XT 5852-5875, mended: when the clock runs out, the last-placed car
      // still in is eliminated.
      if (--pt->ptimer < 0) {
        int32_t last = -1;
        for (int32_t a = 0; a < EXT_PT_CARS; a++) {
          if (!pt->eliminated[a] && (last < 0 || pt->position[a] > pt->position[last])) last = a;
        }
        if (last >= 0) {
          pt->eliminated[last] = true;
          pt->position[last] = 10 - pt->neliminated;
          pt->neliminated++;
        }
        pt->ptimer = 1000;
      }
      for (int32_t a = 0; a < EXT_PT_CARS; a++) {
        if (pt->eliminated[a]) cars[a].force_wreck = true;
      }
    }
    rank_by_score(pt, cars);
    const int32_t target = m == 1 ? 7 : 8;
    for (int32_t a = 0; a < EXT_PT_CARS; a++) {
      if ((m == 1 || m == 4) && pt->score[a] >= target) pt->over = true;
    }
    if (m == 5 && (pt->neliminated >= 10 || pt->eliminated[0])) pt->over = true;
  } else if (m == 2) {
    // XT 5765-5817: nobody takes damage; every second checkpoint the leader
    // clears, the last-placed car still in is eliminated.
    for (int32_t a = 0; a < EXT_PT_CARS; a++) {
      cars[a].no_special = true;
      if (!pt->eliminated[a]) {
        cars[a].no_damage = true;
        pt->position[a] = pos[a];
      } else {
        cars[a].force_wreck = true;
      }
      if (pos[a] == 0) {
        if (clear[a] % 2 == 0 && clear[a] >= 2) {
          pt->eliminate = !pt->eliminateonce;
        } else {
          pt->eliminateonce = false;
        }
        if (pt->eliminate) {
          for (int32_t f = 0; f < EXT_PT_CARS; f++) {
            if (a != f && pos[f] == 10 - wasted) {
              pt->position[f] = 10 - wasted;
              pt->eliminated[f] = true;
            }
          }
          pt->eliminateonce = true;
          pt->eliminate = false;
        }
      }
    }
    if (pt->eliminated[0] || wasted >= 10) pt->over = true;
  } else if (m == 3) {
    // XT 5818-5850: a plain race, nobody takes damage.
    for (int32_t a = 0; a < EXT_PT_CARS; a++) {
      cars[a].no_damage = true;
      cars[a].no_special = true;
      pt->position[a] = pos[a];
      if (clear[a] == nlaps_nsp && pos[a] == 0) pt->over = true;
    }
  }
  if (pt->over) {
    for (int32_t a = 0; a < EXT_PT_CARS; a++) {
      if (pt->position[a] == 0) pt->winner = a;
    }
  }
}

void ext_pt_award(ExtPT *pt) {
  for (int32_t a = 0; a < EXT_PT_CARS; a++) {
    const int32_t place = pt->position[a];
    pt->gained[a] = (pt->match >= 4) ? 20 - 2 * place : 10 - place;
    if (pt->gained[a] < 0) pt->gained[a] = 0;
    pt->points[a] += pt->gained[a];
  }
}

int32_t ext_pt_champion(const ExtPT *pt) {
  int32_t best = 0;
  for (int32_t a = 1; a < EXT_PT_CARS; a++) {
    if (pt->points[a] > pt->points[best]) best = a;
  }
  return best;
}

const char *const *ext_pt_rules(int32_t match) {
  static const char *const kRules[5][10] = {
      {"MATCH 1", "VENUE - Centrifugal Rush, Under Water?", "TYPE - Wasting, Bounty Hunter", "- No fixing.",
       "- Every time you waste a car, you get a point.", "- You lose a point if you're wasted.",
       "- The first person to 7 points wins!", "- No special attacks!",
       "Winner gets 10 points, 2nd place gets 9 points, 3rd place gets 8, etc.", NULL},
      {"MATCH 2", "VENUE - The Fast and The Furious + The Radical", "TYPE - Elimination, Mighty Eight",
       "- Every two checkpoints, the person in last is eliminated.",
       "- The race continues until there's only one person left or if you're wasted.",
       "- You can't waste here - no one takes damage unless they're eliminated!", "- No special attacks allowed.",
       "Winner gets 10 points, 2nd place gets 9 points, 3rd place gets 8, etc.", NULL},
      {"MATCH 3", "VENUE - Rolling with the Big Boys (2 laps)", "TYPE - Racing, Nimi",
       "- Racing only! No one takes damage here.", "- Specials aren't allowed either.",
       "Winner gets 10 points, 2nd place gets 9 points, 3rd place gets 8, etc.", NULL},
      {"MATCH 4", "VENUE - The Mad Party", "TYPE - Wasting, DR Chaos", "- Wasting someone grants you a point.",
       "- Getting wasted loses you a point.", "- First to 8 points wins!",
       "Winner gets 20 points, 2nd place gets 18 points, 3rd place gets 16, etc.", NULL},
      {"MATCH 5", "VENUE - The Garden of the Van", "TYPE - Wasting Elimination, Old Van",
       "- Wasting scores a point, getting wasted loses one.",
       "- There's a timer: when it reaches 0, the car in last place is eliminated.",
       "- The game ends if you're eliminated or if only one car remains.",
       "Winner gets 20 points, 2nd place gets 18 points, 3rd place gets 16, etc.", NULL},
  };
  if (match < 1 || match > 5) match = 1;
  return kRules[match - 1];
}
