// See ext_mode.h; line numbers are Extended's xtGraphics.java.
#include "ext_mode.h"

#include <stdio.h>
#include <string.h>

#include "java_compat.h"

int32_t ext_car_of(int32_t cn) {
  if (cn >= 0 && cn < 16) return cn + 23;
  if (cn >= 16 && cn < 39) return cn - 16;
  return -1;
}

int32_t ext_car_to_port(int32_t ecar) {
  if (ecar >= 0 && ecar < 23) return ecar + 16;
  if (ecar >= 23 && ecar < 39) return ecar - 23;
  return -1;
}

// xtGraphics.proba (XT 14428), by Extended car number.
static const float kProba[39] = {.5f, .5f, .4f, .3f, .3f, .4f, .3f, .3f, .3f, .1f, .1f, .5f, .1f, 0, 0, 0, 0, .1f, .1f, .5f,
                                 .85f, .85f, 0, .5f, .5f, .4f, .3f, .5f, .4f, .3f, .3f, .3f, .1f, .1f, .5f, .1f, 0, 0, 0};

void ext_sortcars(int32_t *sc, int32_t nplayers, int32_t stage, int32_t unlocked, int32_t ptmatch) {
  const int32_t i = stage;
  int32_t e[32];
  if (nplayers > 32) nplayers = 32;
  for (int32_t k = 0; k < nplayers; k++) e[k] = ext_car_of(sc[k]);
  bool ok[32] = {false};

  const bool lateststage = unlocked == i && unlocked != 28;
  if (lateststage) {
    // XT 12718-12775: the boss last; the rest below it, no repeats from
    // stage 5 on (with 11 cars) except on a few stages, Stampede only in
    // the first slots or on 11/12.
    int32_t bestcar = 7 + (i + 1) / 2;
    if (bestcar == 20 || bestcar == 21) bestcar = 22;
    e[nplayers - 1] = bestcar;
    const bool stopception = i == 16 || i == 12 || i == 6 || i == 7 || i == 8 || i == 11 || i == 13 || i == 17 ||
                             i == 19 || i == 23 || i == 24;
    const bool allowrepeats = !(nplayers <= (i + 17) / 2 && nplayers <= 18);
    for (int32_t k = 1; k < nplayers - 1;) {
      e[k] = (int32_t)(nfm_random() * (double)bestcar);
      ok[k] = true;
      for (int32_t l = 0; l < nplayers; l++) {
        if (k != l && e[k] == e[l] && !allowrepeats && !stopception) ok[k] = false;
      }
      nfm_random();   // XT 12769's unused Math.random()
      if (i != 11 && i != 12 && k != 1 && k != 2 && e[k] == 13) ok[k] = false;
      if (ok[k]) k++;
    }
  } else {
    // XT 13079-13240: the boss unless the player drives it; the rest from
    // everything progress has reached, weighted against weak old cars.
    int32_t byte0 = nplayers;
    const int32_t bestcar2 = 7 + (i + 1) / 2;
    if (e[0] != bestcar2) {
      e[nplayers - 1] = bestcar2 > 38 ? 38 : bestcar2;
      byte0 = nplayers - 1;
    }
    const int32_t bestunlocked = 7 + (unlocked + 1) / 2;
    for (int32_t k2 = 1; k2 < byte0; k2++) {
      ok[k2] = false;
      while (!ok[k2]) {
        int32_t pick = (int32_t)(nfm_random() * (double)(bestunlocked + 1));
        if (pick > 38) pick = 38;
        e[k2] = pick;
        ok[k2] = true;
        for (int32_t g = 0; g < nplayers; g++) {
          if (k2 != g && e[k2] == e[g] && unlocked >= 5 && nplayers <= 11) ok[k2] = false;
        }
        float f = kProba[e[k2]];
        if (i - e[k2] > 4 && i != 28) {
          f += (float)(i - e[k2] - 4) / 10.0f;
          if (f > 0.9f) f = 0.9f;
        }
        if (i == 16 && f < 0.9f) f = 0.9f;
        if (nfm_random() < f) ok[k2] = false;
      }
    }
  }
  // The Premier Tournament (XT 13254-13280): one car for the whole field.
  if (i == 26 && ptmatch >= 1 && ptmatch <= 5) {
    static const int32_t kMatchCar[5] = {16, 35, 27, 15, 11};
    for (int32_t a = 0; a < nplayers; a++) e[a] = kMatchCar[ptmatch - 1];
  }
  for (int32_t k = 0; k < nplayers; k++) {
    const int32_t c = ext_car_to_port(e[k]);
    sc[k] = c < 0 ? 0 : c;
  }
}

ExtMusic ext_stage_music(int32_t stage) {
  static const int16_t kParams[28][3] = {
      {320, 8000, 125}, {260, 7200, 125}, {230, 8000, 125}, {240, 8000, 125}, {282, 7800, 125}, {320, 7600, 125},
      {300, 7500, 125}, {270, 7900, 125}, {330, 7900, 125}, {352, 7300, 125}, {480, 7900, 125}, {290, 7900, 125},
      {225, 7600, 137}, {400, 8000, 125}, {220, 8000, 125}, {261, 8000, 125}, {310, 7600, 125}, {310, 7600, 125},
      {400, 7600, 125}, {310, 7600, 125}, {230, 7600, 125}, {280, 8000, 125}, {375, 7600, 125}, {310, 7600, 125},
      {300, 7600, 125}, {300, 7600, 125}, {305, 7600, 136}, {250, 7600, 135}};
  ExtMusic mu;
  memset(&mu, 0, sizeof(mu));
  if (stage < 1 || stage > EXT_NORMAL_STAGES) stage = 1;
  snprintf(mu.file, sizeof(mu.file), "stage%d", stage);
  mu.amp = kParams[stage - 1][0];
  mu.rate = kParams[stage - 1][1];
  mu.tempo = kParams[stage - 1][2];
  return mu;
}

ExtMusic ext_menu_music(const char *which) {
  ExtMusic mu;
  memset(&mu, 0, sizeof(mu));
  snprintf(mu.file, sizeof(mu.file), "%s", which);
  mu.tempo = 125;
  if (strcmp(which, "menu") == 0) { mu.amp = 345; mu.rate = 7900; }
  else if (strcmp(which, "cars") == 0) { mu.amp = 200; mu.rate = 7900; }
  else { mu.amp = 135; mu.rate = 7800; }
  return mu;
}

void ext_stage_entry(int32_t stage, int32_t ptmatch, char *pack, int32_t pack_len, char *entry, int32_t entry_len) {
  if (stage == EXT_PT_STAGE) {
    snprintf(pack, (size_t)pack_len, "matchtracks");
    snprintf(entry, (size_t)entry_len, "26m%d.txt", ptmatch < 1 ? 1 : ptmatch);
  } else {
    snprintf(pack, (size_t)pack_len, "tracks");
    snprintf(entry, (size_t)entry_len, "%d.txt", stage);
  }
}

int32_t ext_unlock_after_win(int32_t unlocked, int32_t stage) {
  if (stage == unlocked && unlocked < EXT_NORMAL_STAGES) return unlocked + 1;
  return unlocked;
}

int32_t ext_showcase_car(int32_t stage) {
  // XT 12251-12310: stage won -> Extended car shown.
  static const int8_t kShow[29] = {-1, -1, 8, -1, 9, -1, 10, -1, 11, -1, 12, -1, 13, -1, 14, -1, 15,
                                   -1, 16, -1, 17, -1, -1, 18, -1, -1, 19, -1, -1};
  if (stage < 0 || stage > 28 || kShow[stage] < 0) return -1;
  return ext_car_to_port(kShow[stage]);
}

static void ext_progress_path(const char *progress_path, char *out, size_t out_len) {
  const char *slash = strrchr(progress_path, '/');
  size_t dir = slash ? (size_t)(slash - progress_path) + 1 : 0;
  if (dir + sizeof("ext_progress.txt") > out_len) dir = 0;
  memcpy(out, progress_path, dir);
  memcpy(out + dir, "ext_progress.txt", sizeof("ext_progress.txt"));
}

void ext_progress_reset(ExtProgress *p) { p->normal_unlocked = 1; }

void ext_progress_load(const char *progress_path, ExtProgress *p) {
  ext_progress_reset(p);
  char path[512];
  ext_progress_path(progress_path, path, sizeof(path));
  FILE *f = fopen(path, "r");
  if (!f) return;
  char line[128];
  while (fgets(line, sizeof(line), f)) {
    int v;
    if (sscanf(line, "normal_unlocked=%d", &v) == 1 && v >= 1 && v <= EXT_NORMAL_STAGES) p->normal_unlocked = v;
  }
  fclose(f);
}

bool ext_progress_save(const char *progress_path, const ExtProgress *p) {
  char path[512];
  ext_progress_path(progress_path, path, sizeof(path));
  FILE *f = fopen(path, "w");
  if (!f) return false;
  fprintf(f, "normal_unlocked=%d\n", (int)p->normal_unlocked);
  return fclose(f) == 0;
}
