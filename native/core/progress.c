#include "progress.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

// Custom "Simple_Car.rad" slot -- kept in sync with main.c's own
// CUSTOM_CAR_INDEX. Not exported through the header because progress.h
// is deliberately GL/UI-free; the only place that consults this is the
// car-select gate below and any car_index >= 16 is treated the same way.
#define PROGRESS_CUSTOM_CAR_INDEX 16

void game_progress_reset(GameProgress *p) {
  p->unlocked[0] = 1;
  p->unlocked[1] = 1;
  p->scm[0] = 0;
  p->scm[1] = 0;
  p->justwon1 = false;
  p->justwon2 = false;
}

bool game_progress_can_pick_car(const GameProgress *p, GameMode gmode, int32_t car_index) {
  if (car_index < 0) return false;
  // Custom car (Simple_Car.rad slot) -- only pickable in Free Play.
  if (car_index >= PROGRESS_CUSTOM_CAR_INDEX) return gmode == GMODE_FREE_PLAY;
  // Java wraps the whole gate in `if (this.gmode != 0)` at :5306.
  if (gmode == GMODE_FREE_PLAY) return true;
  if (gmode == GMODE_NFM1) {
    // :5307-5320
    if (car_index == 5  && p->unlocked[0] <= 2)  return false;
    if (car_index == 6  && p->unlocked[0] <= 4)  return false;
    if (car_index == 11 && p->unlocked[0] <= 6)  return false;
    if (car_index == 14 && p->unlocked[0] <= 8)  return false;
    if (car_index == 15 && p->unlocked[0] <= 10) return false;
    return true;
  }
  if (gmode == GMODE_NFM2) {
    // :5323 -- cars 8..15 gated on unlocked[1] > (car-7)*2. 0..7 always pickable.
    if (car_index >= 8 && p->unlocked[1] <= (car_index - 7) * 2) return false;
    return true;
  }
  return true;
}

int32_t game_progress_car_unlock_stage(const GameProgress *p, GameMode gmode, int32_t car_index) {
  if (gmode == GMODE_NFM1) {
    if (car_index == 5  && p->unlocked[0] <= 2)  return 2;
    if (car_index == 6  && p->unlocked[0] <= 4)  return 4;
    if (car_index == 11 && p->unlocked[0] <= 6)  return 6;
    if (car_index == 14 && p->unlocked[0] <= 8)  return 8;
    if (car_index == 15 && p->unlocked[0] <= 10) return 10;
  } else if (gmode == GMODE_NFM2) {
    if (car_index >= 8 && p->unlocked[1] <= (car_index - 7) * 2) return (car_index - 7) * 2;
  }
  return 0;
}

bool game_progress_can_pick_stage(const GameProgress *p, GameMode gmode, int32_t stage_num) {
  if (gmode == GMODE_FREE_PLAY) return stage_num >= 1 && stage_num <= 27;
  if (gmode == GMODE_NFM1) return stage_num >= 1 && stage_num <= p->unlocked[0];
  if (gmode == GMODE_NFM2) return stage_num >= 11 && stage_num <= p->unlocked[1] + 10;
  return false;
}

bool game_progress_stage_in_range(GameMode gmode, int32_t stage_num) {
  if (gmode == GMODE_FREE_PLAY) return stage_num >= 1 && stage_num <= 27;
  if (gmode == GMODE_NFM1) return stage_num >= 1 && stage_num <= 11;
  if (gmode == GMODE_NFM2) return stage_num >= 11 && stage_num <= 27;
  return false;
}

int32_t game_progress_first_stage(GameMode gmode) {
  if (gmode == GMODE_NFM2) return 11;
  return 1;
}

int32_t game_progress_default_stage(const GameProgress *p, GameMode gmode) {
  // Java :1913-1922 -- NFM1: if unlocked[0] != 11 || justwon1, stage =
  // unlocked[0]. Else (campaign complete + no fresh win) Java rolls a
  // random 1..11; we simplify to 10 there since we don't remember the
  // prior pick.
  //
  // THEN, on either path, :1921 remaps `stage == 11` to 27. That step was
  // missing here, and it matters: NFM1 owns stages 1..10, so a bare 11 is
  // an NFM2 stage number. Finishing NFM1 (unlocked[0] reaching 11) used
  // to leave the picker defaulting to stage 11 -- outside NFM1's own
  // range -- instead of the stage-27 finale the original sends you to.
  if (gmode == GMODE_NFM1) {
    int32_t s = (p->unlocked[0] != 11 || p->justwon1) ? p->unlocked[0] : 10;
    if (s == 11) s = 27;
    return s;
  }
  // Java :1925 -- NFM2: same pattern with the +10 offset.
  if (gmode == GMODE_NFM2) {
    if (p->unlocked[0] != 17 || p->justwon2) return p->unlocked[1] + 10;
    return 26;
  }
  return 1;
}

int32_t game_progress_bonus_car_for(GameMode gmode, int32_t stage_num) {
  if (gmode == GMODE_NFM1) {
    if (stage_num == 2)  return 5;
    if (stage_num == 4)  return 6;
    if (stage_num == 6)  return 11;
    if (stage_num == 8)  return 14;
    if (stage_num == 10) return 15;
  } else if (gmode == GMODE_NFM2) {
    if (stage_num == 12) return 8;
    if (stage_num == 14) return 9;
    if (stage_num == 16) return 10;
    if (stage_num == 18) return 11;
    if (stage_num == 20) return 12;
    if (stage_num == 22) return 13;
    if (stage_num == 24) return 14;
    if (stage_num == 26) return 15;
  }
  return 0;
}

void game_progress_finish_stage(GameProgress *p, GameMode gmode, int32_t stage_num, bool won) {
  // Java :6692 -- Free Play never advances progression.
  if (gmode == GMODE_FREE_PLAY) return;

  int32_t gm = (int32_t)gmode;
  bool at_threshold = (stage_num == p->unlocked[gm - 1] + (gm - 1) * 10);
  // Java also excludes stage 27 from progression advancement (:6692 / :7002 / :7014).
  bool progression_stage = (stage_num != 27);

  if (won && at_threshold && progression_stage) {
    p->unlocked[gm - 1]++;
    int32_t bonus = game_progress_bonus_car_for(gmode, stage_num);
    if (bonus != 0) p->scm[gm - 1] = bonus;
    if (gmode == GMODE_NFM1) p->justwon1 = true;
    if (gmode == GMODE_NFM2) p->justwon2 = true;
  } else {
    if (gmode == GMODE_NFM1) p->justwon1 = false;
    if (gmode == GMODE_NFM2) p->justwon2 = false;
  }
}

// --- persistence ---
// Fixed binary layout:
//   [0..3]   magic "NFMP" (0x50 0x4D 0x46 0x4E little-endian doesn't matter, we memcmp bytes)
//   [4..7]   uint32 version (1)
//   [8..15]  int32 unlocked[0], unlocked[1]
//   [16..23] int32 scm[0], scm[1]
// Total 24 bytes. justwon flags are per-race latches, deliberately not saved.

#define PROGRESS_MAGIC "NFMP"
#define PROGRESS_VERSION 1u

// Best-effort mkdir -p for the directory portion of `path`. Ignores
// EEXIST; other errors are silently swallowed since the subsequent
// fopen() will surface any real problem.
static void ensure_parent_dir(const char *path) {
  char copy[1024];
  size_t n = strlen(path);
  if (n >= sizeof(copy)) return;
  memcpy(copy, path, n + 1);
  // Trim last path component.
  char *slash = strrchr(copy, '/');
  if (!slash) return;
  *slash = '\0';
  if (copy[0] == '\0') return;
  // Walk up creating each intermediate directory.
  for (char *p = copy + 1; *p; p++) {
    if (*p == '/') {
      *p = '\0';
      mkdir(copy, 0755);
      *p = '/';
    }
  }
  mkdir(copy, 0755);
}

bool game_progress_save_to_disk(const GameProgress *p, const char *path) {
  ensure_parent_dir(path);
  FILE *f = fopen(path, "wb");
  if (!f) return false;
  uint8_t buf[24];
  memcpy(buf, PROGRESS_MAGIC, 4);
  uint32_t ver = PROGRESS_VERSION;
  memcpy(buf + 4, &ver, 4);
  int32_t u0 = p->unlocked[0], u1 = p->unlocked[1];
  int32_t s0 = p->scm[0], s1 = p->scm[1];
  memcpy(buf + 8, &u0, 4);
  memcpy(buf + 12, &u1, 4);
  memcpy(buf + 16, &s0, 4);
  memcpy(buf + 20, &s1, 4);
  size_t written = fwrite(buf, 1, sizeof(buf), f);
  int close_rc = fclose(f);
  return written == sizeof(buf) && close_rc == 0;
}

// settings.txt in the progress file's directory.
static void settings_path_for(const char *progress_path, char *out, size_t out_len) {
  const char *slash = strrchr(progress_path, '/');
  size_t dir_len = slash ? (size_t)(slash - progress_path) + 1 : 0;
  if (dir_len + sizeof("settings.txt") > out_len) dir_len = 0;
  memcpy(out, progress_path, dir_len);
  memcpy(out + dir_len, "settings.txt", sizeof("settings.txt"));
}

GameSettings game_settings_defaults(int32_t graphics) {
  GameSettings s;
  memset(&s, 0, sizeof(s));
  s.graphics = graphics;
  s.draw_dist = 0;
  s.detail = 0;
  s.shadows = 1;
  s.particles = 1;
  s.blur = GAME_SETTINGS_BLUR_DEFAULT;
  s.smooth = 1;
  s.music_vol = 100;
  s.sfx_vol = 100;
  s.show_fps = 0;
  s.board_names = 0;
  s.language = 0;   // English
  s.shake = 0;   // the extended build starts without it
  s.rumble = 1;
  s.cam_orbit = s.cam_watch = s.cam_far = 1;
  s.fp_opponents = 5;   // six rivals, NFM 2's field
  s.fp_pick = 0;
  s.fp_tier = 1;        // NFM 2's own draw, as Free Play always raced
  s.fp_win = 0;
  s.fp_laps = 0;
  s.fp_arrow = 1;
  s.fp_specials = 1;
  for (int32_t i = 0; i < 19; i++) s.fp_rival[i] = i + 1;
  s.steer_dpad = 0;
  // Indices into game.c's Switch button list: ZR, ZL, B, X, D-Pad Up,
  // D-Pad Down, Plus, Y, Minus -- the scheme v1.0 shipped with.
  static const int32_t kBindDefaults[BIND_COUNT] = {7, 6, 1, 2, 12, 13, 9, 3, 8, 4, 15};   // ..., Special L, list bars D-Pad Right
  memcpy(s.bind, kBindDefaults, sizeof(s.bind));
  return s;
}

// settings.txt's keys, each with its field and allowed range; values off the
// range keep what is there, and `step` snaps to the slider's grid.
typedef struct { const char *key; size_t off; int32_t lo, hi, step; } SettingKey;
static const SettingKey kSettingKeys[] = {
  {"graphics", offsetof(GameSettings, graphics), 0, GFX_QUALITY_COUNT - 1, 1},
  {"draw_distance", offsetof(GameSettings, draw_dist), 0, 2, 1},
  {"scenery_detail", offsetof(GameSettings, detail), 0, 1, 1},
  {"shadows", offsetof(GameSettings, shadows), 0, 1, 1},
  {"particles", offsetof(GameSettings, particles), 0, 1, 1},
  {"motion_blur", offsetof(GameSettings, blur), 0, 100, 20},
  {"smooth_frames", offsetof(GameSettings, smooth), 0, 1, 1},
  {"music_volume", offsetof(GameSettings, music_vol), 0, 100, 10},
  {"effects_volume", offsetof(GameSettings, sfx_vol), 0, 100, 10},
  {"show_fps", offsetof(GameSettings, show_fps), 0, 2, 1},
  {"board_names", offsetof(GameSettings, board_names), 0, 1, 1},
  {"language", offsetof(GameSettings, language), 0, 1, 1},
  {"screen_shake", offsetof(GameSettings, shake), 0, 1, 1},
  {"vibration", offsetof(GameSettings, rumble), 0, 1, 1},
  {"camera_orbit", offsetof(GameSettings, cam_orbit), 0, 1, 1},
  {"camera_tripod", offsetof(GameSettings, cam_watch), 0, 1, 1},
  {"camera_far", offsetof(GameSettings, cam_far), 0, 1, 1},
  {"fp_rivals", offsetof(GameSettings, fp_opponents), 0, 18, 1},
  {"fp_pick", offsetof(GameSettings, fp_pick), 0, 1, 1},
  {"fp_tier", offsetof(GameSettings, fp_tier), 0, 4, 1},
  {"fp_win", offsetof(GameSettings, fp_win), 0, 2, 1},
  {"fp_laps", offsetof(GameSettings, fp_laps), 0, 10, 1},
  {"fp_arrow", offsetof(GameSettings, fp_arrow), 0, 1, 1},
  {"fp_specials", offsetof(GameSettings, fp_specials), 0, 1, 1},
#define FP_RIVAL_KEY(i) {"fp_rival" #i, offsetof(GameSettings, fp_rival[(i) - 1]), 0, 255, 1}
  FP_RIVAL_KEY(1),  FP_RIVAL_KEY(2),  FP_RIVAL_KEY(3),  FP_RIVAL_KEY(4),  FP_RIVAL_KEY(5),
  FP_RIVAL_KEY(6),  FP_RIVAL_KEY(7),  FP_RIVAL_KEY(8),  FP_RIVAL_KEY(9),  FP_RIVAL_KEY(10),
  FP_RIVAL_KEY(11), FP_RIVAL_KEY(12), FP_RIVAL_KEY(13), FP_RIVAL_KEY(14), FP_RIVAL_KEY(15),
  FP_RIVAL_KEY(16), FP_RIVAL_KEY(17), FP_RIVAL_KEY(18), FP_RIVAL_KEY(19),
#undef FP_RIVAL_KEY
  {"steer_dpad", offsetof(GameSettings, steer_dpad), 0, 1, 1},
  {"bind_accelerate", offsetof(GameSettings, bind[BIND_ACCEL]), 0, 19, 1},
  {"bind_brake", offsetof(GameSettings, bind[BIND_BRAKE]), 0, 19, 1},
  {"bind_handbrake", offsetof(GameSettings, bind[BIND_HANDB]), 0, 19, 1},
  {"bind_view", offsetof(GameSettings, bind[BIND_VIEW]), 0, 19, 1},
  {"bind_arrow", offsetof(GameSettings, bind[BIND_ARRACE]), 0, 19, 1},
  {"bind_map", offsetof(GameSettings, bind[BIND_RADAR]), 0, 19, 1},
  {"bind_pause", offsetof(GameSettings, bind[BIND_PAUSE]), 0, 19, 1},
  {"bind_mute_music", offsetof(GameSettings, bind[BIND_MUSIC]), 0, 19, 1},
  {"bind_mute_effects", offsetof(GameSettings, bind[BIND_SFX]), 0, 19, 1},
  {"bind_special", offsetof(GameSettings, bind[BIND_SPECIAL]), 0, 19, 1},
  {"bind_list_bars", offsetof(GameSettings, bind[BIND_LISTBARS]), 0, 19, 1},
};

void game_settings_load(const char *progress_path, GameSettings *s) {
  char path[1024];
  settings_path_for(progress_path, path, sizeof(path));
  FILE *f = fopen(path, "r");
  if (!f) return;
  char line[96];
  while (fgets(line, sizeof(line), f)) {
    char key[48];
    int v;
    if (sscanf(line, "%47[^=]=%d", key, &v) != 2) continue;
    for (size_t k = 0; k < sizeof(kSettingKeys) / sizeof(kSettingKeys[0]); k++) {
      const SettingKey *sk = &kSettingKeys[k];
      if (strcmp(key, sk->key) != 0) continue;
      if (sk->step > 1) {
        // A slider: clamped, then snapped to its grid.
        if (v < sk->lo) v = sk->lo;
        if (v > sk->hi) v = sk->hi;
        v = ((v + sk->step / 2) / sk->step) * sk->step;
      } else if (v < sk->lo || v > sk->hi) {
        break;   // a choice off its list keeps the default
      }
      *(int32_t *)((char *)s + sk->off) = v;
      break;
    }
  }
  fclose(f);
}

bool game_settings_save(const char *progress_path, const GameSettings *s) {
  char path[1024];
  settings_path_for(progress_path, path, sizeof(path));
  ensure_parent_dir(path);
  FILE *f = fopen(path, "w");
  if (!f) return false;
  for (size_t k = 0; k < sizeof(kSettingKeys) / sizeof(kSettingKeys[0]); k++) {
    fprintf(f, "%s=%d\n", kSettingKeys[k].key, (int)*(const int32_t *)((const char *)s + kSettingKeys[k].off));
  }
  return fclose(f) == 0;
}

bool game_progress_load_from_disk(GameProgress *p, const char *path) {
  game_progress_reset(p);
  FILE *f = fopen(path, "rb");
  if (!f) return false;
  uint8_t buf[24];
  size_t r = fread(buf, 1, sizeof(buf), f);
  fclose(f);
  if (r != sizeof(buf)) return false;
  if (memcmp(buf, PROGRESS_MAGIC, 4) != 0) return false;
  uint32_t ver;
  memcpy(&ver, buf + 4, 4);
  if (ver != PROGRESS_VERSION) return false;
  int32_t u0, u1, s0, s1;
  memcpy(&u0, buf + 8, 4);
  memcpy(&u1, buf + 12, 4);
  memcpy(&s0, buf + 16, 4);
  memcpy(&s1, buf + 20, 4);
  // Basic sanity clamp -- a corrupt save shouldn't unlock everything or
  // set nonsensical scaffold indices.
  if (u0 < 1 || u0 > 11) return false;
  if (u1 < 1 || u1 > 17) return false;
  if (s0 < 0 || s0 > 15) return false;
  if (s1 < 0 || s1 > 15) return false;
  p->unlocked[0] = u0;
  p->unlocked[1] = u1;
  p->scm[0] = s0;
  p->scm[1] = s1;
  return true;
}
