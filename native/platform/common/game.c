// The whole game: menu state machine, asset loading, and the fixed-
// timestep physics/race loop -- shared verbatim by every platform target
// (platform/linux/main.c, platform/vita/main.c). This is NOT a port of
// any web/*.js file -- see ../../PORT_SPEC.md §5, same category as those
// thin per-platform entry points. Its only job is to give the shared
// native/core/ modules somewhere to actually run.
//
// Split out of what used to be platform/linux/main.c (Part 18 -- see
// ../../TASKS_NATIVE.md) once that file's genuinely platform-specific
// surface turned out to be small and isolated (window/GL-context creation,
// the event pump, buffer swap, wall-clock ticks -- all now behind
// platform.h) compared to the ~3000 lines of menu/asset/physics-loop code
// that has nothing to do with SDL or vitaGL specifically. Duplicating that
// bulk per platform would have meant every future fix (see the Part 18
// entry's own example: the stages 28-32 crash) risked landing in only one
// copy. Real device access this file itself never touches directly goes
// through a small per-platform contract instead: platform.h (window/
// event/clock), input.h (driving controls), audio.h (SFX/music playback,
// duplicated per platform/<name>/ directory since its struct fields are
// genuinely platform-specific -- see that header's own comment).
//
// M1+M2 (see ../../TASKS_NATIVE.md): loads a real car through
// cont_o_init_buf + cont_o_init_copy -- any of the 16 built-in cars, or
// Simple_Car.rad with its OWN gameplay stat table (slot CUSTOM_CAR_INDEX)
// computed by car_define_loadcar/loadstat from its own stat()/physics()/
// handling() lines, not a built-in car's hardcoded stats -- see the M3
// menu below for how that choice gets made. Loads a real stage (any of
// the 32 stages/N.txt files) through game_sparker_loadbase/loadstage (the
// lean subset -- see game_sparker.h for exactly what that skips and why),
// and drives it with REAL mad_drive() physics at a fixed 53ms/tick (see
// the "Fixed-timestep physics" comment below) against the stage's real
// Trackers collision geometry, with real keyboard/pad input (see
// platform/<name>/input.h).
//
// M3: a real menu (car select -> stage select -> race), with the
// original's Arial text through core/font.c.
// Not a port of web/Smenu.js (written against the browser's own DOM/
// image-asset loading, out of this port's scope) -- a native menu screen
// flow instead, same category as this whole file.
//
// The per-frame object sort/draw below (search "ORDERING") is ported from
// GameSparker.js's own #draw method -- not itself a web/*.js port target
// (see PORT_SPEC.md §5), but its algorithm IS the game's, reproduced
// faithfully rather than improvised, same as the JS's own banner comment
// insists on for anyone touching #draw.
#include "game.h"
#include "platform.h"
#include "gl_include.h"
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "java_compat.h"
#include "trig.h"
#include "gfx.h"
#include "gfx_gl.h"
#include "font.h"
#include "i18n.h"
#include "gif_decode.h"
#include "jpeg_decode.h"
#include "png_decode.h"
#include "hud_recolor.h"
#include <math.h>
#include "vfs.h"
#include "medium.h"
#include "trackers.h"
#include "plane.h"
#include "wheels.h"
#include "cont_o.h"
#include "game_sparker.h"
#include "ext_stage.h"
#include "car_define.h"
#include "record.h"
#include "check_points.h"
#include "xt_graphics.h"
#include "control.h"
#include "mad.h"
#include "input.h"
#include "progress.h"
#include "audio.h"
#include "wav_decode.h"
#include "radical_mod.h"
#include "bots.h"
#include "nfm_limits.h"
#include "specials.h"
#include "ext_mode.h"
#include "ext_pt.h"
#include "new_cars.h"
#include "career.h"
#include "career_perks.h"
#include "career_bots.h"
#include "career_stage.h"
#include "diag.h"

// GameSparker.js's own ContO[610] held NFM 2's stages; Extended's reach 1106
// (its ContO[15000]), so room for those.
#define STAGE_OBJECT_CAPACITY 1600
#define NUM_STAGES 32              // stages/1.txt .. stages/32.txt
#define CUSTOM_CAR_INDEX 39        // menu entry for the loadcar()/loadstat() Simple_Car.rad flow (after Extended's cars)

// Display names for the 16 built-in car slots -- transcribed verbatim
// from web/CarDefine.js's own `this.names` literal (menu-display only,
// never read by physics/drawing code, so not part of core/car_define.c's
// own ported field set -- see that file's header comment on
// `getSvalue`/name tables being out of scope there).
static const char *CAR_DISPLAY_NAMES[39] = {
    "Tornado Shark", "Formula 7", "Wow Caninaro", "La Vita Crab", "Nimi",
    "MAX Revenge", "Lead Oxide", "Kool Kat", "Drifter X", "Sword of Justice",
    "High Rider", "EL KING", "Mighty Eight", "M A S H E E N", "Radical One",
    "DR Monstaa",
    // Extended's own 23 (its xtGraphics.names 0-22), this port's 16-38.
    "Remington", "Speedy 7", "Damn Van", "Blizzard Rush", "Twingoor", "Steel Falcon", "Oldskool",
    "Revonater", "Comet", "Das Cop", "Hellfire", "Old Van", "Redspeed", "Stampede", "Skyrider",
    "DR Chaos", "Bounty Hunter", "Radical Racer", "Titan", "Deity", "Agent Waster", "Agent Racer",
    "Tesco Lorry",
};
#define CAR_COUNT 39   // NFM 2's 16 and Extended's 23; CUSTOM_CAR_INDEX follows

/** Free Play's last car: the new cars' last, else the custom car (Extended's
 * normal mode has no custom car: its last is 38). */
static int32_t car_last_index(bool ext_normal) {
  if (g_new_car_count > 0) return NEW_CAR_FIRST + g_new_car_count - 1;
  return ext_normal ? CAR_COUNT - 1 : CUSTOM_CAR_INDEX;
}

// Car select's filters (Up / Down): Free Play's by origin, the career's by
// how the car is won.
enum { CF_ALL, CF_CLASSIC, CF_EXTENDED, CF_RR, CF_CUSTOM, CF_CAREER, CF_BONUS };
static const char *const kCarFilterName[] = {"All Cars",    "Classic Cars", "Extended Cars", "R&R Cars",
                                             "Custom Cars", "Career Cars",  "Bonus Cars"};

static bool car_in_filter(int32_t cn, int32_t filter) {
  const bool new_car = cn >= NEW_CAR_FIRST && cn < NEW_CAR_FIRST + g_new_car_count;
  switch (filter) {
    case CF_CLASSIC: return cn < 16;
    case CF_EXTENDED: return cn >= 16 && cn < CAR_COUNT;
    case CF_RR: return new_car && !g_new_cars[cn - NEW_CAR_FIRST].from_sd;
    case CF_CUSTOM: return cn == CUSTOM_CAR_INDEX || (new_car && g_new_cars[cn - NEW_CAR_FIRST].from_sd);
    case CF_CAREER: return cn < CAR_COUNT && ext_car_of(cn) <= 30;   // Extended's 31-38 are the bonus stages' prizes
    case CF_BONUS: return cn < CAR_COUNT && ext_car_of(cn) >= 31;
    default: return true;
  }
}

/** The filters car select cycles through in this mode; returns how many. */
static int32_t car_filters(bool career, int32_t gmode, int32_t *out) {
  int32_t n = 0;
  out[n++] = CF_ALL;
  if (career) {
    out[n++] = CF_CAREER;
    out[n++] = CF_BONUS;
  } else if (gmode == GMODE_FREE_PLAY) {
    out[n++] = CF_CLASSIC;
    out[n++] = CF_EXTENDED;
    for (int32_t i = 0; i < g_new_car_count; i++)
      if (!g_new_cars[i].from_sd) {
        out[n++] = CF_RR;
        break;
      }
    out[n++] = CF_CUSTOM;
  }
  return n;
}

/** The next car from `cn` toward `dir` (+1 / -1) car select can land on, -1 at the end. */
static int32_t car_step(int32_t cn, int32_t dir, int32_t maxsl, int32_t filter, bool ext_normal) {
  for (int32_t c = cn + dir; c >= 0 && c <= maxsl; c += dir)
    if (car_in_filter(c, filter) && !(ext_normal && c == CUSTOM_CAR_INDEX)) return c;
  return -1;
}

/** Free Play's Race Setup picks the rivals (sc[1..]): by hand, or at random
 * from a group of cars (fp_tier, car select's filters) without repeats while
 * it has cars left. "Classic" keeps NFM 2's own draw (bots_sortcars). */
static void free_play_rivals(int32_t *sc, int32_t nplayers, const GameSettings *s) {
  const int32_t last = car_last_index(false);
  if (s->fp_pick == 1) {
    for (int32_t i = 1; i < nplayers; i++) {
      const int32_t c = s->fp_rival[i - 1];
      if (c >= 0 && c <= last && c != CUSTOM_CAR_INDEX) sc[i] = c;
    }
    return;
  }
  if (s->fp_tier == CF_CLASSIC) return;
  int32_t pool[NEW_CAR_FIRST + NEW_CARS_MAX], n = 0;
  for (int32_t c = 0; c <= last; c++)
    if (c != CUSTOM_CAR_INDEX && car_in_filter(c, s->fp_tier)) pool[n++] = c;
  if (n == 0) return;
  bool used[NEW_CAR_FIRST + NEW_CARS_MAX] = {false};
  int32_t left = n;
  for (int32_t i = 1; i < nplayers; i++) {
    if (left == 0) {   // every car of the group is out: again from the top
      memset(used, 0, sizeof(used));
      left = n;
    }
    int32_t k = (int32_t)(nfm_random() * (double)left);
    if (k >= left) k = left - 1;
    for (int32_t j = 0; j < n; j++) {
      if (used[j]) continue;
      if (k-- == 0) {
        used[j] = true;
        sc[i] = pool[j];
        break;
      }
    }
    left--;
  }
}

/** Who made a car, as Extended's car select credits it (XT 15341-15398,
 * "Created by ..."): Extended's own cars by its table, a new car by its
 * .rad's carmaker(name) line (the web Car Maker's Author field); NULL for
 * NFM 2's sixteen and a new car without one. */
static const char *car_author(int32_t cn) {
  if (cn >= NEW_CAR_FIRST && cn < NEW_CAR_FIRST + g_new_car_count)
    return g_new_cars[cn - NEW_CAR_FIRST].author[0] ? g_new_cars[cn - NEW_CAR_FIRST].author : NULL;
  if (cn < EXT_FIRST_CAR || cn > EXT_FIRST_CAR + 22) return NULL;
  static const char *const kBy[23] = {
      "aliff01",      "Afterburn/ToaZuka", "ACVoong", "Rulue",   "Tunari",     "Trelivision", "GX",
      "Chaotic",      "projectDUB",        "Tunari",  "DJ Miker", "Phyrexian", "KingOfSpeed", "Excalibur",
      "Tunari",       "Ultimato",          "Rulue",   "RAD1",    "aliff01/Phyrexian", "Vitalogy", "Tunari",
      "DJ Miker",     "ACVoong"};
  return kBy[cn - EXT_FIRST_CAR];
}

/** A car's name for the HUD; the custom car's, or "Car" off the end. */
static const char *car_name(int32_t cn) {
  if (cn >= 0 && cn < CAR_COUNT) return CAR_DISPLAY_NAMES[cn];
  if (cn >= NEW_CAR_FIRST && cn < NEW_CAR_FIRST + g_new_car_count) return g_new_cars[cn - NEW_CAR_FIRST].name;
  return cn == CUSTOM_CAR_INDEX ? "Simple Car" : "Car";
}

// Locked-car gate overlay's fixed 9-segment fence layout --
// xtGraphics.java:599-600.
static const int32_t kCarGatePgatx[9] = {211, 240, 280, 332, 399, 466, 517, 558, 586};
static const int32_t kCarGatePgaty[9] = {193, 213, 226, 237, 244, 239, 228, 214, 196};

// Menu-only helper (not a port -- see below): reads just the `name(...)`
// line from a stage file's opening few lines, without running the full
// game_sparker_loadstage geometry parse, so the stage-select screen can
// list all 32 stages' real display names cheaply. Falls back to "Stage
// N" if the file is missing or has no name() line -- never blocks the
// menu on a malformed/missing stage file.
// Free Play's Extended stages, after NFM's 27 (stage 28 on): Extended's
// normal-mode tracks the career does not race -- 20 and 27 are career stages
// too, 26 is the Premier Tournament.
static const int8_t kFreePlayExt[] = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13,
                                      14, 15, 16, 17, 18, 19, 21, 22, 23, 24, 25, 28};
#define FREE_PLAY_EXT_COUNT ((int32_t)(sizeof(kFreePlayExt) / sizeof(kFreePlayExt[0])))
#define FREE_PLAY_LAST_STAGE (27 + FREE_PLAY_EXT_COUNT)

/** The Extended normal-mode stage Free Play's `stage_num` races, 0 for NFM's own. */
static int32_t free_play_ext_stage(int32_t stage_num) {
  return stage_num > 27 && stage_num <= FREE_PLAY_LAST_STAGE ? kFreePlayExt[stage_num - 28] : 0;
}

static void ext_read_name(int32_t stage, char *out, size_t outsz);

static void stage_read_name(int32_t stage_num, char *out, size_t outsz) {
  if (free_play_ext_stage(stage_num)) {
    ext_read_name(free_play_ext_stage(stage_num), out, outsz);
    return;
  }
  char path[64];
  snprintf(path, sizeof(path), "stages/%d.txt", stage_num);
  char *text = vfs_read_text(path);
  if (text) {
    char *start = strstr(text, "name(");
    if (start) {
      start += 5;
      char *end = strchr(start, ')');
      if (end && (size_t)(end - start) < outsz) {
        memcpy(out, start, (size_t)(end - start));
        out[end - start] = '\0';
        free(text);
        return;
      }
    }
    free(text);
  }
  snprintf(out, outsz, "Stage %d", stage_num);
}

/**
 * (Re)loads stage `stage_num`'s real geometry into `*objects_ptr`/
 * `*count_ptr`, freeing whatever was there before. Shared by the stage-
 * select screen's live preview (reloaded every time the browsed stage
 * changes) and the STATE_RACING transition further down (which reloads
 * once more for whichever stage was finally confirmed, even if the
 * preview already loaded the same one -- redundant but cheap, ~100
 * objects, and simpler than threading an "already loaded" fast path
 * through both call sites). `m`/`t`/`cp` are the SAME shared instances
 * racing itself uses -- safe to reuse for preview purposes: every field
 * a previewed stage's snap/sky/ground commands touch gets overwritten
 * again once an actual race starts (game_sparker_loadstage runs again
 * there too), and trackers_devidetrackers() (called from inside
 * game_sparker_loadstage) already frees its own previous sect arrays
 * before rebuilding, so `t` never leaks across repeated calls.
 *
 * `out_center_x`/`out_center_z` (nullable) pass straight through to
 * game_sparker_loadstage's own out-params -- see that function's doc
 * comment. Only the stage-select call site needs them (to arm the 3D
 * preview's camera); the racing transition passes NULL/NULL.
 */
// Settings > Graphics > Scenery Detail as Medium.resdown: 0 is the original's
// full detail, 2 its own low-detail mode (no decoration objects, half the
// clouds and mountains, no extra sky bands). 1 is only a step of the
// original's automatic drop and looks like 0, so it is not offered.
static int32_t g_resdown_setting = 0;

static bool load_stage_objects(ContO **objects_ptr, int32_t *count_ptr, int32_t previous_count,
                                ContO *base_models, Medium *m, Trackers *t, CheckPoints *cp,
                                int32_t stage_num, int32_t *out_center_x, int32_t *out_center_z) {
  char stage_path[64];
  snprintf(stage_path, sizeof(stage_path), "stages/%d.txt", stage_num);
  char *stage_text = vfs_read_text(stage_path);
  if (!stage_text) return false;

  if (*objects_ptr) {
    for (int32_t i = 0; i < previous_count; i++) cont_o_free(&(*objects_ptr)[i]);
    free(*objects_ptr);
  }
  *objects_ptr = calloc(STAGE_OBJECT_CAPACITY, sizeof(ContO));
  check_points_init(cp);
  // CheckPoints.stage is the stage being raced (the original's stage list
  // writes it, GameSparker.loadstagePreview): init leaves a random one, which
  // made the AI's per-stage rules and stage 10's no-arrow rule a dice roll.
  cp->stage = stage_num;
  bool ok = game_sparker_loadstage(*objects_ptr, STAGE_OBJECT_CAPACITY, count_ptr,
                                    base_models, m, t, cp, stage_text, out_center_x, out_center_z);
  free(stage_text);
  check_points_calprox(cp);
  // loadstage resets resdown to the original's full detail; Settings >
  // Scenery Detail puts it back (see apply_settings()).
  m->resdown = g_resdown_setting;
  return ok;
}

// The career (career.c) runs on normal mode's flow with its own stages:
// careertracks.radq 1-31, no tournament.
static bool g_ext_career = false;
// The career's bonus stage being raced (1-4, careertracks bonus/K.txt), 0 none.
static int32_t g_career_bonus = 0;

// Extended's normal-mode stage `stage` as a "pack:entry" spec (26 is the
// Premier Tournament's match `ptmatch`).
static void ext_stage_spec(int32_t stage, int32_t ptmatch, char *out, size_t outsz) {
  char pack[16], entry[16];
  if (g_ext_career) {
    if (g_career_bonus) snprintf(out, outsz, "careertracks:bonus/%d.txt", (int)g_career_bonus);
    else snprintf(out, outsz, "careertracks:%d.txt", (int)stage);
    return;
  }
  ext_stage_entry(stage, ptmatch, pack, sizeof(pack), entry, sizeof(entry));
  snprintf(out, outsz, "%s:%s", pack, entry);
}

/** A normal-mode stage's name, from its file's name(...) line. */
static void ext_read_name(int32_t stage, char *out, size_t outsz) {
  if (stage == EXT_PT_STAGE && !g_ext_career) {
    snprintf(out, outsz, "The Premier Tournament");
    return;
  }
  char pack[16], entry[16];
  ext_stage_entry(stage, 1, pack, sizeof(pack), entry, sizeof(entry));
  if (g_ext_career) {
    snprintf(pack, sizeof(pack), "careertracks");
    if (g_career_bonus) snprintf(entry, sizeof(entry), "bonus/%d.txt", (int)g_career_bonus);
    else snprintf(entry, sizeof(entry), "%d.txt", (int)stage);
  }
  char *text = ext_stage_text(pack, entry);
  snprintf(out, outsz, "Stage %d", stage);
  if (!text) return;
  char *start = strstr(text, "name(");
  if (start) {
    start += 5;
    char *end = strchr(start, ')');
    if (end && (size_t)(end - start) < outsz) {
      memcpy(out, start, (size_t)(end - start));
      out[end - start] = '\0';
      for (char *c = out; *c; c++) if (*c == '|') *c = ',';
    }
  }
  free(text);
}

// ---- One of Extended's stages, `spec` "pack:entry" ("tracks:1" is
// ext/data/Files/tracks.radq's 1.txt), through ext_loadstage from Extended's
// model table; `player_ext_car` is the player's car in Extended's numbering.
static bool load_ext_stage_objects(ContO **objects_ptr, int32_t *count_ptr, int32_t previous_count, ContO *ext_models,
                                   Medium *m, Trackers *t, CheckPoints *cp, const char *spec, int32_t player_ext_car,
                                   ExtStageInfo *out_info) {
  char pack[32] = "tracks", entry[48] = "1.txt";
  const char *colon = strchr(spec, ':');
  if (colon) {
    snprintf(pack, sizeof(pack), "%.*s", (int)(colon - spec), spec);
    if (strchr(colon + 1, '.')) snprintf(entry, sizeof(entry), "%s", colon + 1);
    else snprintf(entry, sizeof(entry), "%s.txt", colon + 1);
  }
  char *text = ext_stage_text(pack, entry);
  if (!text) return false;
  if (*objects_ptr) {
    for (int32_t i = 0; i < previous_count; i++) cont_o_free(&(*objects_ptr)[i]);
    free(*objects_ptr);
  }
  *objects_ptr = calloc(STAGE_OBJECT_CAPACITY, sizeof(ContO));
  check_points_init(cp);
  ExtStageInfo local;
  ExtStageInfo *info = out_info ? out_info : &local;
  const bool ok = ext_loadstage(*objects_ptr, STAGE_OBJECT_CAPACITY, count_ptr, ext_models, m, t, cp, text,
                                player_ext_car, info);
  free(text);
  check_points_calprox(cp);
  m->resdown = g_resdown_setting;
  return ok;
}
// ----

// Depth-sorted painter's-algorithm render for a flat ContO array -- the
// SAME selection-sort-by-dist ranking as the racing loop's own object draw
// below (see its "ORDERING" comment for why it's exactly this shape and
// not a cleaner sort), reused here for the stage-select 3D preview
// (GameSparker.java:466-497's fase==1 object loop). Objects whose dist is
// 0 draw immediately unsorted, same as racing; everything else sorts by
// last frame's dist, farthest first.
static void render_sorted_objects(ContO *objects, int32_t count, Graphics2D *g) {
  static int32_t visible_idx[STAGE_OBJECT_CAPACITY];
  static int32_t rank[STAGE_OBJECT_CAPACITY];
  static int32_t order[STAGE_OBJECT_CAPACITY];
  int32_t nvis = 0;
  for (int32_t i = 0; i < count; i++) {
    if (objects[i].dist != 0) {
      visible_idx[nvis++] = i;
    } else {
      cont_o_d(&objects[i], g);
    }
  }
  for (int32_t i = 0; i < nvis; i++) rank[i] = 0;
  for (int32_t i = 0; i < nvis; i++) {
    for (int32_t j = i + 1; j < nvis; j++) {
      if (objects[visible_idx[i]].dist < objects[visible_idx[j]].dist) rank[i]++;
      else rank[j]++;
    }
    order[rank[i]] = i;
  }
  for (int32_t i = 0; i < nvis; i++) {
    cont_o_d(&objects[visible_idx[order[i]]], g);
  }
}

// STATE_MAIN_MENU corresponds to Java's fase == 10 (maini(), xtGraphics.java:4309).
// The full Java fase graph is documented in native/docs/MENU_FLOW.md; each state
// here maps to one fase we actually implement (single-player subset, no netplay).
// Order matches the natural progression at the same time so the enum values
// happen to double as menu depth. STATE_INSTRUCTIONS and STATE_CREDITS are
// leaves off the main menu (Java fase 11 and 8 respectively); STATE_GAMEMODE_MENU
// is Java fase 102, entered from Play Game.
typedef enum {
  STATE_MAIN_MENU,       // fase 10 -- maini()
  STATE_GAMEMODE_MENU,   // fase 102 -- maini2()
  STATE_INSTRUCTIONS,    // fase 11 -- inst()
  STATE_CREDITS,         // fase 8 -- credits()
  STATE_CAR_SELECT,      // fase 7 -- carselect()
  STATE_STAGE_SELECT,    // fase 6 -- stageselect()
  STATE_STAGE_LOCKED,    // fase 6 briefly with lockcnt>0 -- cantgo() locked-stage overlay (xtGraphics.java:1993)
  STATE_STAGE_LOADING,   // fase 2 -- loadingstage() (xtGraphics.java:1971), the animated scrolling
                         //   track backdrop shown between stage-select confirm and race start
  STATE_RACING,          // fase 0 -- racing loop
  STATE_REPLAY,          // fase -3 -- the post-race highlight-reel camera choreography
  STATE_POST_RACE,       // fase -5 -- finish()
  STATE_PAUSED,          // fase -7 -- pausedgame() (xtGraphics.java:4695), the in-race pause menu
  STATE_PAUSE_REPLAY,    // fase -1 -- on-demand replay of the raw 300-tick ring (GameSparker.java:1258)
  STATE_CANTREPLY,       // fase -8 -- cantreply() (xtGraphics.java:4820), "not enough replay data" banner
  STATE_BOOT_CLICK,      // fase 111 -- clicknow() (xtGraphics.java:1443) over the loading() screen
  STATE_BOOT_RAD,        // fase 9 -- rad() (xtGraphics.java:1570), the Radicalplay intro
  STATE_SETTINGS,        // this port's own Settings screen, reached from the pause menu
  STATE_STAGE_INTRO,     // fase 5 + 6 -- loadmusic()/musicomp() (xtGraphics.java:2935, :3105), the
                         //   stage's presentation card: hint, music playing, "Press Start to begin"
  STATE_BENCH_RESULT,    // this port's Performance Test summary (Settings > Performance Test)
  STATE_PT_INFO,         // Extended's ptstart (fase 49): the Premier Tournament's next match and its rules
  STATE_PT_SCORES,       // Extended's scoreshow (fase 51): points after a match, the champion after five
} GameState;

// On-screen names for the physical controls, so the help text can say
// what the player of THIS build actually has to press. Only the strings
// differ; no logic anywhere is conditional on the target.
//
// The desktop side is the original's own wording, verbatim. The Vita side
// exists because the original text is not merely stylistically off there,
// it is wrong: a Vita has no spacebar and no A key, so "PRESS [ A ]" asks
// for something impossible. A deliberate, narrow departure from 1:1,
// confined to help text on the one target where the source's own wording
// cannot be followed. See native/CMakeLists.txt for where the define
// comes from.
// KEY_STUNT is what the stunt combo pairs with the handbrake: the same
// arrow keys on desktop, the left stick on the Vita (snapped to four
// directions, platform/vita/input.c), where driving is triggers + stick.
#ifdef NFM_TARGET_VITA
#define KEY_STEER    tr("L/R and Left Stick")
#define KEY_STUNT    tr("Left Stick")
#define KEY_HANDB    tr("Cross")
#define KEY_ARRACE   tr("press Up on the D-pad")
#define KEY_CONTINUE "Cross"
#define KEY_BACK     "Circle"
// clicknow()'s prompt; the original asks for a mouse click.
#define KEY_START_PROMPT "Press Cross to Start"
#define KEY_ARRACE_HINT  tr("Press Up on the D-pad")
#define KEY_SPECIAL "R"
#define KEY_VIEW tr("Triangle")
#define KEY_LISTBARS "L"
#elif defined(NFM_TARGET_SWITCH)
// The Switch's buttons as platform/switch/{platform,input}.c bind them: the
// same names and places on the Joy-Cons (handheld or paired) and on the Pro
// Controller, which libnx all reads as player 1. Settings > Controls can move
// the race ones, so the help text shows wherever they are now (key_*()), as
// button icons (pad_icon()).
#define KEY_STEER    key_steer()
#define KEY_STUNT    (key_settings()->steer_dpad ? ICON_DPAD : ICON_LSTICK)
#define KEY_HANDB    kPadIcons[key_settings()->bind[BIND_HANDB]]
#define KEY_ARRACE   key_arrace("press")
#define KEY_CONTINUE PAD_ICON(a)
#define KEY_BACK     PAD_ICON(b)
#define KEY_START_PROMPT "Press " PAD_ICON(a) " to Start"
#define KEY_ARRACE_HINT  key_arrace("Press")
#define KEY_SPECIAL  kPadIcons[key_settings()->bind[BIND_SPECIAL]]
#define KEY_VIEW     kPadIcons[key_settings()->bind[BIND_VIEW]]
#define KEY_LISTBARS kPadIcons[key_settings()->bind[BIND_LISTBARS]]
#else
#define KEY_STEER    tr("Arrow Keys")
#define KEY_STUNT    tr("Arrow Keys")
#define KEY_HANDB    tr("Spacebar")
#define KEY_ARRACE   tr("press [ A ]")
#define KEY_CONTINUE "Enter"
#define KEY_BACK     "Esc"
#define KEY_START_PROMPT "Click here to Start"
// The stage cards' hint (stages 3 and 14), in their own sentence case.
#define KEY_ARRACE_HINT  tr("Press [ A ]")
#define KEY_SPECIAL "Q"
#define KEY_VIEW "V"
#define KEY_LISTBARS "D"
#endif

// The original's wide SPACEBAR key carries its label across its own face.
// The Vita's Cross button is a small round glyph in the same slot, so that
// text would land on top of it -- these nudge it clear, below the button,
// keeping the wording rather than dropping it (the glyph says WHICH button,
// the word says what it DOES). Zero on desktop, where the wide key is
// still there to write on.
// Settings > Controls' buttons: an index into these is what GameSettings.bind
// holds; kPadBits is the same button's libnx HidNpadButton bit. D-Pad and
// L Stick directions are both in Up, Down, Left, Right order, four apart
// (settings_swap_steer relies on it).
static const char *const kPadNames[] = {
    "A", "B", "X", "Y", "L", "R", "ZL", "ZR", "Minus", "Plus", "L Stick Press", "R Stick Press",
    "D-Pad Up", "D-Pad Down", "D-Pad Left", "D-Pad Right",
    "L Stick Up", "L Stick Down", "L Stick Left", "L Stick Right"};
#define PAD_COUNT 20
#define PAD_DPAD_UP 12
#define PAD_STICK_UP 16

// The same buttons as inline icons in text (font.h's FONT_ICON), drawn by
// pad_icon(); two more for the D-pad and the left stick as a whole.
#define PAD_ICON(c) "\x1b" #c
#define ICON_DPAD PAD_ICON(u)
#define ICON_LSTICK PAD_ICON(v)
static const char *const kPadIcons[PAD_COUNT] = {
    PAD_ICON(a), PAD_ICON(b), PAD_ICON(c), PAD_ICON(d), PAD_ICON(e), PAD_ICON(f), PAD_ICON(g),
    PAD_ICON(h), PAD_ICON(i), PAD_ICON(j), PAD_ICON(k), PAD_ICON(l), PAD_ICON(m), PAD_ICON(n),
    PAD_ICON(o), PAD_ICON(p), PAD_ICON(q), PAD_ICON(r), PAD_ICON(s), PAD_ICON(t)};

/** FontIconFn: a Switch button in the text's own colour -- round face buttons
 * and +/-, pill-shaped shoulders, ringed sticks (an arrow beside for a
 * direction), and a cross for the D-pad with the pressed arm cut out. */
static int32_t pad_icon(Graphics2D *g, int32_t icon, int32_t x, int32_t y, int32_t size) {
  static const char *const kLabel[] = {"A", "B", "X", "Y", "L", "R", "ZL", "ZR", "-", "+", "L", "R"};
  const int32_t d = (size * 13 + 5) / 10;                      // the button's height
  const int32_t cy = y - (size * 36 + 50) / 100;               // a capital letter's middle
  const int32_t top = cy - d / 2, lsize = (size * 3 + 2) / 4;
  const bool shoulder = icon >= 4 && icon <= 7;
  const bool stick = icon == 10 || icon == 11 || (icon >= 16 && icon <= 19) || icon == 21;
  const bool dpad = (icon >= 12 && icon <= 15) || icon == 20;
  int32_t w = d;
  font_set(FONT_BOLD, lsize);
  if (shoulder) {
    w = font_width(kLabel[icon]) + d / 2;
    if (w < d) w = d;
  }
  if (icon >= 16 && icon <= 19) w = d + d / 2 + 1;
  if (!g) return w + 3;

  const int32_t ir = (int32_t)(g->r * 255.0f + 0.5f), ig = (int32_t)(g->g * 255.0f + 0.5f),
                ib = (int32_t)(g->b * 255.0f + 0.5f);
  const int32_t paper = (ir * 299 + ig * 587 + ib * 114 < 128000) ? 255 : 24;
  const int32_t x0 = x + 1;
  const char *label = icon < 12 ? kLabel[icon] : (stick ? "L" : NULL);
  if (dpad) {
    const int32_t t = d / 3 > 3 ? d / 3 : 3, a = (d - t) / 2;
    gfx_fill_rect(g, x0 + a, top, t, d);
    gfx_fill_rect(g, x0, top + a, d, t);
    if (icon != 20) {
      static const int32_t kArm[4][2] = {{1, 0}, {1, 2}, {0, 1}, {2, 1}};  // up, down, left, right
      const int32_t *arm = kArm[icon - 12];
      const int32_t ax = arm[0] == 0 ? x0 + 1 : (arm[0] == 1 ? x0 + a + 1 : x0 + d - a + 1);
      const int32_t ay = arm[1] == 0 ? top + 1 : (arm[1] == 1 ? top + a + 1 : top + d - a + 1);
      gfx_set_color(g, paper, paper, paper);
      gfx_fill_rect(g, ax, ay, (arm[0] == 1 ? t : a) - 2, (arm[1] == 1 ? t : a) - 2);
    }
  } else if (stick) {
    gfx_fill_oval(g, x0, top, d, d);
    gfx_set_color(g, paper, paper, paper);
    gfx_fill_oval(g, x0 + 2, top + 2, d - 4, d - 4);
    gfx_set_color(g, ir, ig, ib);
    if (icon == 11) label = "R";
    if (icon >= 16 && icon <= 19) {
      // The arrow beside the stick, pointing its way.
      const int32_t h = d / 2, ax = x0 + d + 1, ac = ax + h / 2;
      int32_t xs[3], ys[3];
      if (icon == 16 || icon == 17) {
        const int32_t s = icon == 16 ? -1 : 1;
        xs[0] = ax; xs[1] = ax + h; xs[2] = ac;
        ys[0] = ys[1] = cy - s * h / 2; ys[2] = cy + s * h / 2;
      } else {
        const int32_t s = icon == 18 ? -1 : 1;
        ys[0] = cy - h / 2; ys[1] = cy + h / 2; ys[2] = cy;
        xs[0] = xs[1] = ac - s * h / 2; xs[2] = ac + s * h / 2;
      }
      gfx_fill_polygon(g, xs, ys, 3);
    }
  } else if (shoulder) {
    gfx_fill_round_rect(g, x0, top, w, d, d / 2, d / 2);
  } else {
    gfx_fill_oval(g, x0, top, d, d);
  }
  if (label) {
    if (!stick) gfx_set_color(g, paper, paper, paper);
    const int32_t lw = stick ? d : w;
    font_draw(g, label, x0 + (lw - font_width(label) + 1) / 2, cy + (lsize * 36 + 50) / 100);
  }
  gfx_set_color(g, ir, ig, ib);
  return w + 3;
}
#ifdef NFM_TARGET_SWITCH
static const uint8_t kPadBits[PAD_COUNT] = {0, 1, 2, 3, 6, 7, 8, 9, 11, 10, 4, 5,
                                            13, 15, 12, 14, 17, 19, 16, 18};
// The settings in effect (apply_settings), for the help text's key names.
static const GameSettings *g_key_settings;
static const GameSettings *key_settings(void) {
  static GameSettings defaults;
  if (!g_key_settings) {
    defaults = game_settings_defaults(0);
    g_key_settings = &defaults;
  }
  return g_key_settings;
}
static const char *key_steer(void) {
  static char buf[64];
  const GameSettings *s = key_settings();
  snprintf(buf, sizeof(buf), tr("%s/%s and %s"), kPadIcons[s->bind[BIND_ACCEL]], kPadIcons[s->bind[BIND_BRAKE]],
           s->steer_dpad ? ICON_DPAD : ICON_LSTICK);
  return buf;
}
static const char *key_arrace(const char *verb) {
  static char buf[2][48];
  char *b = buf[verb[0] == 'P'];
  snprintf(b, sizeof(buf[0]), "%s %s", tr(verb), kPadIcons[key_settings()->bind[BIND_ARRACE]]);
  return b;
}
#endif

/** font_draw of a printf-formatted line (the help text's key names). The
 * format is translated before it is filled in (i18n_port_es.c has these
 * lines as formats), so the key names land where the language puts them. */
static void font_drawf(Graphics2D *g, int32_t x, int32_t y, const char *fmt, ...) {
  char buf[160];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), tr(fmt), ap);
  va_end(ap);
  font_draw(g, buf, x, y);
}

#if defined(NFM_TARGET_VITA) || defined(NFM_TARGET_SWITCH)
#define KEYLBL_DX 4
#define KEYLBL_DY 24
#else
#define KEYLBL_DX 0
#define KEYLBL_DY 0
#endif

// drawcs's `drawString(s, 400 - ftm.stringWidth(s) / 2, y)`, in the current
// font (font_set), `y` a baseline as in Java.
static void draw_centered(Graphics2D *g, const char *s, int32_t cx, int32_t y) {
  font_draw(g, s, cx - font_width(s) / 2, y);
}

// The real HUD panel graphics (data/images.zip), decoded+recoloured+
// uploaded once when a race starts (see the STATE_RACING transition
// block below) -- see native/TASKS_NATIVE.md's "1:1 original assets"
// section for the gif_decode.c/hud_recolor.c/gfx_draw_image pipeline
// this pulls together. `tex < 0` means "failed to load" (missing/
// corrupt asset) -- HUD drawing skips a panel with tex < 0 rather than
// crashing, matching web/XtGraphics.js's own `if (this.dmg) ...` null
// guards at every one of these draw call sites.
typedef struct { int32_t tex, w, h; } HudImg;
typedef struct {
  HudImg dmg, pwr, lap, was, pos, sped;
  HudImg spec;    // Extended's Special bar (data/port/special.png, tools/bake_special.py)
  HudImg rank[8]; // checkPoints.pos[im] indexes this directly, 0-7
  HudImg cntdn[4]; // 0=GO (gc.gif), 1=1c.gif, 2=2c.gif, 3=3c.gif -- xtGraphics.java:268-269
  HudImg youwon, youlost; // xtGraphics.java:153-154, loadsnap()'d at :9515-9516
  // Elimination-ending cards -- xtGraphics.java:129,134 (raw load :809-816),
  // loadsnap()'d at :9514,9517. Distinct assets from youwon/youlost above:
  // youwastedem.gif for "all other cars wasted" (win), yourwasted.gif for
  // "you were wasted" (lose). web/XtGraphics.js never loads these (its own
  // "TODO not ported: asset not loaded" fallback at those drawhi() call
  // sites) since the JS oracle's asset-loading pass was scoped to only what
  // Part 9-16 needed -- ported here straight from the Java asset names,
  // same recolor treatment as every other loadsnap()'d HUD glyph.
  HudImg youwastedem, yourwasted;
} HudImages;

// Every race re-tints these for its own stage, so the texture the previous
// race uploaded is refilled in place rather than replaced: a fresh upload
// per race leaked ~25 textures each time (there is no texture-free call).
static int32_t upload_or_refill(HudImg prev, const uint8_t *rgba, int32_t w, int32_t h) {
  if (prev.tex >= 0 && prev.w == w && prev.h == h) {
    gfx_gl_update_texture(prev.tex, rgba, w, h);
    return prev.tex;
  }
  return gfx_gl_upload_texture(rgba, w, h);
}

// Dark-sky HUD (hud_recolor.h): the race's sky when Medium.darksky, set before
// its HUD sprites load. The Java's answer was boxes behind the HUD; the port
// recolours the sprites, counters and announcements to read against the sky,
// as the web port's default does.
static bool g_hud_dark = false;
static int32_t g_hud_sky[3];
// ponytail: the Java's boxes, kept off; a Settings row if anyone wants them.
static const bool kJavaDarkSkyBoxes = false;

/** gfx_set_color for HUD text: on a dark sky, moved to read against it. */
static void hud_set_ink(Graphics2D *g, int32_t r, int32_t gg, int32_t b) {
  if (g_hud_dark) {
    const int32_t c[3] = {r, gg, b};
    int32_t o[3];
    hud_readable(c, g_hud_sky, o);
    r = o[0], gg = o[1], b = o[2];
  }
  gfx_set_color(g, r, gg, b);
}

/** In Spanish, the twin of images.zip's `name` (data/port/es/<name>.png, the
 * web port's ui-sprites-es.js lettering: see that folder's MANIFEST.txt)
 * decoded into *img; false in English or when there is none, and the caller
 * decodes the original. The PNG keeps the GIF's own background and key
 * pixels, so the caller's recolouring treats it as it would the GIF. */
static bool load_es_png(const char *name, PngImage *img) {
  if (i18n_lang() != I18N_LANG_ES) return false;
  const char *dot = strrchr(name, '.');
  char path[96];
  snprintf(path, sizeof(path), "data/port/es/%.*s.png", dot ? (int)(dot - name) : (int)strlen(name), name);
  int32_t len = 0;
  uint8_t *bytes = vfs_read_bytes(path, &len);
  if (!bytes) return false;
  const bool ok = png_decode(bytes, (size_t)len, img);
  vfs_free_bytes(bytes);
  return ok;
}

static HudImg load_hud_gif(VfsZip *zip, const char *name, const int32_t snap[3], HudImg prev) {
  HudImg result = {-1, 0, 0};
  PngImage es;
  if (load_es_png(name, &es)) {
    hud_recolor(es.rgba, es.width, es.height, snap);
    if (g_hud_dark) hud_adapt_ink(es.rgba, es.width, es.height, g_hud_sky);
    result.tex = upload_or_refill(prev, es.rgba, es.width, es.height);
    result.w = es.width;
    result.h = es.height;
    png_free(&es);
    return result;
  }
  for (int32_t i = 0; i < zip->count; i++) {
    if (strcmp(zip->entries[i].name, name) != 0) continue;
    GifImage img;
    if (gif_decode(zip->entries[i].data, (size_t)zip->entries[i].len, &img)) {
      hud_recolor(img.rgba, img.width, img.height, snap);
      if (g_hud_dark) hud_adapt_ink(img.rgba, img.width, img.height, g_hud_sky);
      result.tex = upload_or_refill(prev, img.rgba, img.width, img.height);
      result.w = img.width;
      result.h = img.height;
      gif_free(&img);
    } else {
      fprintf(stderr, "data/images.zip: %s failed to decode\n", name);
    }
    break;
  }
  return result;
}

// xtGraphics.loadopsnap() (:9641-9728): the hipnoload() screen's own tint,
// different from loadsnap()'s. Every pixel that is not the key pixel
// (index `key`, which is left exactly as it was -- for these GIFs it is the
// transparent one) is darkened by m.snap and made opaque; for float.gif
// (key 1) the bubble's fill colour (the pixel at 61993) is instead set to
// 237-237*snap/150, or plain 250 on stage 11. The snap triple is first
// lifted until it sums to at least -30, as hipnoload() itself does.
static void opsnap_lift(const int32_t snap[3], int32_t out[3]) {
  out[0] = snap[0]; out[1] = snap[1]; out[2] = snap[2];
  while (out[0] + out[1] + out[2] < -30) {
    for (int32_t i = 0; i < 3; i++) {
      if (out[i] < 50) out[i]++;
    }
  }
}

static int32_t clamp255(int32_t v) { return v > 255 ? 255 : (v < 0 ? 0 : v); }

static HudImg load_opsnap_gif(VfsZip *zip, const char *name, int32_t stage, int32_t key,
                              const int32_t snap[3], HudImg prev) {
  HudImg r = {-1, 0, 0};
  for (int32_t i = 0; i < zip->count; i++) {
    if (strcmp(zip->entries[i].name, name) != 0) continue;
    // The Spanish twin has the key pixel where the GIF has it (transparent).
    PngImage es;
    const bool spanish = load_es_png(name, &es);
    GifImage img;
    if (spanish) {
      img.rgba = es.rgba;
      img.width = es.width;
      img.height = es.height;
    } else if (!gif_decode(zip->entries[i].data, (size_t)zip->entries[i].len, &img)) {
      fprintf(stderr, "data/images.zip: %s failed to decode (gif)\n", name);
      break;
    }
    int32_t n = img.width * img.height;
    int32_t a[3];
    opsnap_lift(snap, a);
    uint8_t *px = img.rgba;
    uint8_t keyc[4] = {px[key * 4], px[key * 4 + 1], px[key * 4 + 2], px[key * 4 + 3]};
    uint8_t fill[4] = {0, 0, 0, 0};
    if (key == 1 && n > 61993) memcpy(fill, px + 61993 * 4, 4);
    for (int32_t j = 0; j < n; j++) {
      uint8_t *c = px + j * 4;
      if (memcmp(c, keyc, 4) == 0) continue;
      int32_t cr, cg, cb;
      if (key == 1 && memcmp(c, fill, 4) == 0) {
        cr = clamp255((int32_t)(237.0f - 237.0f * ((float)a[0] / 150.0f)));
        cg = clamp255((int32_t)(237.0f - 237.0f * ((float)a[1] / 150.0f)));
        cb = clamp255((int32_t)(237.0f - 237.0f * ((float)a[2] / 150.0f)));
        if (stage == 11) cr = cg = cb = 250;
      } else {
        cr = clamp255((int32_t)((float)c[0] - (float)c[0] * ((float)a[0] / 100.0f)));
        cg = clamp255((int32_t)((float)c[1] - (float)c[1] * ((float)a[1] / 100.0f)));
        cb = clamp255((int32_t)((float)c[2] - (float)c[2] * ((float)a[2] / 100.0f)));
      }
      c[0] = (uint8_t)cr; c[1] = (uint8_t)cg; c[2] = (uint8_t)cb; c[3] = 255;
    }
    r.tex = upload_or_refill(prev, img.rgba, img.width, img.height);
    r.w = img.width; r.h = img.height;
    if (spanish) png_free(&es);
    else gif_free(&img);
    break;
  }
  return r;
}

// Menu asset loaders: NO hud_recolor pass (that's the racing-HUD-specific
// grey-to-alpha + snap[]-tint step -- see hud_recolor.h) since the menu's
// backdrop photos/logos are already the intended finished visuals in the
// zip. Returns {tex=-1,...} on missing entry / decode failure -- same
// convention as load_hud_gif above, callers guard with `if (tex < 0)`.
static HudImg load_menu_jpeg(VfsZip *zip, const char *name) {
  HudImg r = {-1, 0, 0};
  for (int32_t i = 0; i < zip->count; i++) {
    if (strcmp(zip->entries[i].name, name) != 0) continue;
    JpegImage img;
    if (jpeg_decode(zip->entries[i].data, (size_t)zip->entries[i].len, &img)) {
      r.tex = gfx_gl_upload_texture(img.rgba, img.width, img.height);
      r.w = img.width; r.h = img.height;
      jpeg_free(&img);
    } else {
      fprintf(stderr, "data/images.zip: %s failed to decode (jpeg)\n", name);
    }
    break;
  }
  return r;
}

// PNG loader for menu (no snap tint pass, RGBA kept as-decoded).
static HudImg load_menu_png(VfsZip *zip, const char *name) {
  HudImg r = {-1, 0, 0};
  for (int32_t i = 0; i < zip->count; i++) {
    if (strcmp(zip->entries[i].name, name) != 0) continue;
    PngImage img;
    if (png_decode(zip->entries[i].data, (size_t)zip->entries[i].len, &img)) {
      r.tex = gfx_gl_upload_texture(img.rgba, img.width, img.height);
      r.w = img.width; r.h = img.height;
      png_free(&img);
    } else {
      fprintf(stderr, "data/images.zip: %s failed to decode (png)\n", name);
    }
    break;
  }
  return r;
}

// Port of xtGraphics.java:9981's dodgen(): brightens each RGB channel
// (unchanged alpha) via `channel = clamp(channel*4 + 90)`. Applied to a
// copy of track.jpg's decoded RGBA so trackbg[1] can be uploaded as its
// own texture -- see load_track_pair() below for how the pair gets used.
static void apply_dodgen_in_place(uint8_t *rgba, int32_t width, int32_t height) {
  int32_t n = width * height;
  for (int32_t i = 0; i < n; i++) {
    for (int32_t c = 0; c < 3; c++) {
      int32_t v = rgba[i * 4 + c] * 4 + 90;
      if (v > 255) v = 255;
      rgba[i * 4 + c] = (uint8_t)v;
    }
  }
}

// Car-select's "smoke warp" entrance transition -- xtGraphics.java's
// drawSmokeCarsbg()/carsbginflex() (:10146-10232) plus the flatrstart
// dispatch inside carselect() itself (:5086-5102) and the one-time
// smokey.gif mask prep in smokeypix() (:10018-10040). Every time car-
// select is (re-)entered (matching Java's own inishcarselect(), called
// from GameSparker.java's fase==-9 transition on EVERY path into car-
// select, including a "back" from stage-select -- there's no OTHER
// `fase = 7` assignment anywhere in xtGraphics.java) the cars.gif backdrop
// briefly warps/swirls into place through a smoke-shaped mask before
// settling, rather than appearing instantly.
//
// carsbg_rgba and the smoke-mask table are built once at startup and never
// mutated again (the mask gets its one-time hue/saturation retint first,
// matching smokeypix()); flexpix is the actual per-frame working buffer,
// re-seeded from the clean carsbg_rgba each time the screen is entered
// (car_smoke_warp_enter) and then progressively warped in place by
// car_smoke_warp_step, exactly like Java's own `this.flexpix` field.
// One non-background pixel of the tinted smoke mask, with the per-channel
// factors drawSmokeCarsbg() recomputes from the mask every frame computed
// once instead: n = (255-mask)/255, om = 1-n, mm = mask*om -- the very
// same float operations, so every frame's output is bit-identical to the
// per-frame version (checked over a whole animation on the host). Only
// ~23k of the mask's 94k pixels are smoke; the rest were skipped by a
// per-pixel compare every frame.
typedef struct {
  int16_t i, j;
  float n[3], om[3], mm[3];
} SmokeMaskPx;

typedef struct {
  uint8_t *carsbg_rgba;   // 670*400*4, clean copy, RE-SEEDS flexpix on enter
  SmokeMaskPx *mask_px;   // the tinted smokey.gif's smoke pixels, column-major
  int32_t mask_count;     //   like the Java loop (see car_smoke_warp_step)
  uint8_t *flexpix;       // 670*400*4, the actual working/warped buffer
  int32_t flexpix_tex;    // dynamic GL texture, glTexSubImage2D'd every step
  bool full_upload;       // flexpix was re-seeded: next upload sends all rows
  int32_t flatr, flyr, flyrdest, flang, flatrstart;
} CarSmokeWarp;

// Loads cars.gif/smokey.gif's raw pixels (NOT via load_menu_gif -- that
// uploads-then-frees, but this needs the raw bytes kept around for the
// whole session) and applies smokeypix()'s one-time retint to the mask.
// Leaves every buffer NULL and flatrstart forced to 6 (permanently
// "settled", see the STATE_CAR_SELECT dispatch) on any failure, so a
// missing/corrupt smokey.gif degrades to the plain static backdrop
// instead of crashing or drawing garbage.
static void car_smoke_warp_load(VfsZip *images_zip, CarSmokeWarp *w) {
  memset(w, 0, sizeof(*w));
  w->flexpix_tex = -1;
  w->flatrstart = 6;

  GifImage carsbg_img = {0}, smokey_img = {0};
  bool have_carsbg = false, have_smokey = false;
  for (int32_t i = 0; i < images_zip->count; i++) {
    if (strcmp(images_zip->entries[i].name, "cars.gif") == 0) {
      have_carsbg = gif_decode(images_zip->entries[i].data, (size_t)images_zip->entries[i].len, &carsbg_img);
    } else if (strcmp(images_zip->entries[i].name, "smokey.gif") == 0) {
      have_smokey = gif_decode(images_zip->entries[i].data, (size_t)images_zip->entries[i].len, &smokey_img);
    }
  }
  if (!have_carsbg || carsbg_img.width != 670 || carsbg_img.height != 400 ||
      !have_smokey || smokey_img.width != 466 || smokey_img.height != 202) {
    fprintf(stderr, "data/images.zip: cars.gif/smokey.gif missing or unexpected size -- "
                     "car-select smoke-warp intro disabled, using plain backdrop\n");
    if (have_carsbg) gif_free(&carsbg_img);
    if (have_smokey) gif_free(&smokey_img);
    return;
  }

  size_t carsbg_bytes = (size_t)670 * 400 * 4;
  w->carsbg_rgba = malloc(carsbg_bytes);
  w->flexpix = malloc(carsbg_bytes);
  memcpy(w->carsbg_rgba, carsbg_img.rgba, carsbg_bytes);
  memcpy(w->flexpix, carsbg_img.rgba, carsbg_bytes);
  gif_free(&carsbg_img);
  uint8_t *smokey_rgba = smokey_img.rgba; // tinted in place, then tabulated

  // smokeypix() (xtGraphics.java:10018-10040): every mask pixel that
  // isn't the same colour as pixel 0 (the sentinel "background" colour --
  // the mask's own corners/margins outside the smoke shape) gets its hue
  // forced to 0.11 and saturation to 0.45, keeping its original
  // brightness -- turns the raw smokey.gif art into a uniform smoky-
  // orange tint whose ALPHA (via brightness) still traces the original
  // shape.
  uint8_t bg_r = smokey_rgba[0], bg_g = smokey_rgba[1], bg_b = smokey_rgba[2];
  int32_t smokey_pixels = 466 * 202;
  for (int32_t i = 0; i < smokey_pixels; i++) {
    uint8_t *px = &smokey_rgba[i * 4];
    if (px[0] == bg_r && px[1] == bg_g && px[2] == bg_b) continue;
    float hsb[3];
    rgb_to_hsb(px[0], px[1], px[2], hsb);
    int32_t rgb = hsb_to_rgb(0.11f, 0.45f, hsb[2]);
    px[0] = (uint8_t)((rgb >> 16) & 0xff);
    px[1] = (uint8_t)((rgb >> 8) & 0xff);
    px[2] = (uint8_t)(rgb & 0xff);
  }

  // Tabulate the smoke pixels in the Java loop's own order (i outer, j
  // inner). The order matters: two mask pixels can land on the same
  // flexpix pixel in one frame, and the second blends over the first.
  w->mask_px = malloc(sizeof(SmokeMaskPx) * (size_t)smokey_pixels);
  w->mask_count = 0;
  const uint8_t *mask0 = &smokey_rgba[0];
  for (int32_t i = 0; i < 466; i++) {
    for (int32_t j = 0; j < 202; j++) {
      const uint8_t *mask = &smokey_rgba[(i + j * 466) * 4];
      if (mask[0] == mask0[0] && mask[1] == mask0[1] && mask[2] == mask0[2]) continue;
      SmokeMaskPx *p = &w->mask_px[w->mask_count++];
      p->i = (int16_t)i;
      p->j = (int16_t)j;
      for (int32_t c = 0; c < 3; c++) {
        p->n[c] = (255.0f - (float)mask[c]) / 255.0f;
        p->om[c] = 1.0f - p->n[c];
        p->mm[c] = (float)mask[c] * p->om[c];
      }
    }
  }
  gif_free(&smokey_img);

  w->flexpix_tex = gfx_gl_upload_texture(w->flexpix, 670, 400);
}

static void car_smoke_warp_free(CarSmokeWarp *w) {
  free(w->carsbg_rgba);
  free(w->mask_px);
  free(w->flexpix);
}

// carsbginflex() (xtGraphics.java:10146-10159) -- called every time car-
// select is (re-)entered (see this struct's own doc comment for why every
// entry path counts). Re-seeds flexpix from the clean cars.gif pixels and
// resets the warp's own random-walk state; does NOT touch flatrstart --
// callers decide that separately, matching Java exactly (the initial
// entry sets it to 0 right after calling this; the internal "flatrstart
// 2..5" transitional frame calls this again but then forces it straight
// to 6).
static void car_smoke_warp_enter(CarSmokeWarp *w, Medium *m) {
  if (!w->flexpix) return;
  memcpy(w->flexpix, w->carsbg_rgba, (size_t)670 * 400 * 4);
  w->full_upload = true;
  w->flatr = 0;
  w->flyr = jtrunc(medium_random(m) * 160.0f - 80.0f);
  w->flyrdest = jtrunc((float)w->flyr + medium_random(m) * 160.0f - 80.0f);
  w->flang = 1;
}

// drawSmokeCarsbg() (xtGraphics.java:10161-10232), minus the `badmac`
// legacy-browser fallback branch (irrelevant to a native port -- always
// takes the real warp path, same as this project dropping every other
// browser-compat hack). Advances the warp's random-walk radius/center,
// re-warps `flexpix` in place by blending each smoke-mask pixel's
// tinted colour into a radially-displaced sample of the CURRENT (already
// warped from prior frames) buffer, then uploads+draws the result.
static void car_smoke_warp_step(CarSmokeWarp *w, Medium *m, Graphics2D *g) {
  if (!w->flexpix) return;

  if (abs(w->flyr - w->flyrdest) > 20) {
    if (w->flyr > w->flyrdest) w->flyr -= 20; else w->flyr += 20;
  } else {
    w->flyr = w->flyrdest;
    w->flyrdest = jtrunc((float)w->flyr + medium_random(m) * 160.0f - 80.0f);
  }
  if (w->flyr > 160) w->flyr = 160;
  if (w->flatr > 170) {
    w->flatrstart++;
    w->flatr = w->flatrstart * 3;
    w->flyr = jtrunc(medium_random(m) * 160.0f - 80.0f);
    w->flyrdest = jtrunc((float)w->flyr + medium_random(m) * 160.0f - 80.0f);
    w->flang = 1;
  }

  // Same arithmetic as the Java's per-pixel loop, over the precomputed
  // smoke pixels (see SmokeMaskPx). This is the expensive part of car
  // select's entrance on the Vita: a sqrt and five float divisions per
  // smoke pixel, every frame for ~33 frames, plus re-uploading the
  // texture -- the reported frame drop while the smoke swirls in.
  const float flang = (float)w->flang;
  const float flatr = (float)w->flatr;
  const int32_t flyr = w->flyr;
  int32_t dirty_top = 400, dirty_bottom = -1;
  for (int32_t k = 0; k < w->mask_count; k++) {
    const SmokeMaskPx *p = &w->mask_px[k];
    int32_t i = p->i, j = p->j;
    float pys = sqrtf((float)((i - 233) * (i - 233) + (j - flyr) * (j - flyr)));
    int32_t n = jtrunc((float)(i - 233) / pys * flatr);
    int32_t n2 = jtrunc((float)(j - flyr) / pys * flatr);
    int32_t px = i + n + 100, py2 = j + n2 + 110;
    int32_t n3 = px + py2 * 670;
    if (px >= 670 || px <= 0 || py2 >= 400 || py2 <= 0 || n3 >= 268000 || n3 < 0) continue;
    if (py2 < dirty_top) dirty_top = py2;
    if (py2 > dirty_bottom) dirty_bottom = py2;

    uint8_t *dst = &w->flexpix[n3 * 4];
    for (int32_t c = 0; c < 3; c++) {
      float kc = flang * p->n[c];
      int32_t v = jtrunc(((float)dst[c] * kc + p->mm[c]) / (kc + p->om[c]));
      if (v > 255) v = 255;
      if (v < 0) v = 0;
      dst[c] = (uint8_t)v;
    }
  }

  w->flang += 2;
  w->flatr += 10 + w->flatrstart * 2;
  // Only the rows this step touched (~58% on average) -- unless flexpix
  // was just re-seeded, in which case the texture is stale everywhere.
  if (w->full_upload) {
    gfx_gl_update_texture(w->flexpix_tex, w->flexpix, 670, 400);
    w->full_upload = false;
  } else if (dirty_bottom >= dirty_top) {
    gfx_gl_update_texture_rows(w->flexpix_tex, w->flexpix, 670, dirty_top,
                               dirty_bottom - dirty_top + 1);
  }
  gfx_draw_image(g, w->flexpix_tex, 65, 25, 670, 400);
}

// Loads track.jpg from images.zip and uploads BOTH the raw image (trackbg[0])
// AND its dodgen'd bright variant (trackbg[1]) as separate textures --
// matches xtGraphics.java:899-902's own pair. On failure either slot may be
// tex<0 and callers must fall back to a plain-fill backdrop.
static void load_track_pair(VfsZip *zip, HudImg *out_normal, HudImg *out_dodged) {
  *out_normal = (HudImg){-1, 0, 0};
  *out_dodged = (HudImg){-1, 0, 0};
  for (int32_t i = 0; i < zip->count; i++) {
    if (strcmp(zip->entries[i].name, "track.jpg") != 0) continue;
    JpegImage img;
    if (!jpeg_decode(zip->entries[i].data, (size_t)zip->entries[i].len, &img)) {
      fprintf(stderr, "data/images.zip: track.jpg failed to decode (jpeg)\n");
      return;
    }
    out_normal->tex = gfx_gl_upload_texture(img.rgba, img.width, img.height);
    out_normal->w = img.width; out_normal->h = img.height;
    apply_dodgen_in_place(img.rgba, img.width, img.height);
    out_dodged->tex = gfx_gl_upload_texture(img.rgba, img.width, img.height);
    out_dodged->w = img.width; out_dodged->h = img.height;
    jpeg_free(&img);
    return;
  }
}

// Persistent per-frame state for trackbg() (xtGraphics.java:339-341,
// initialised at :558-559: trkx starts at {65, 735}, trkl at 0). Held
// here as a struct so the animation state survives across state
// transitions -- the same two tiles keep sliding whether we're in
// STATE_STAGE_LOCKED, STATE_STAGE_LOADING, or the racing splash.
typedef struct {
  int32_t trkx[2];
  int32_t trkl;
  int32_t trklim;
} TrackBgState;

static void trackbg_init(TrackBgState *s) {
  s->trkx[0] = 65; s->trkx[1] = 735;
  s->trkl = 0; s->trklim = 0;
}

// 1:1 port of xtGraphics.java:1707's trackbg(final boolean b). `force_normal`
// is Java's `b` parameter: true = always draw the un-dodged trackbg[0] this
// frame (loadingstage/cantgo pass true so the transition doesn't strobe);
// false = let the internal counter (trkl>trklim) occasionally flip to the
// dodged variant for a single frame, matching the JS's soft flicker.
// Border rects use setColor(0,0,0) exactly as Java :1727-1731.
static void draw_trackbg(Graphics2D *gr, TrackBgState *s,
                          HudImg trackbg_normal, HudImg trackbg_dodged, bool force_normal) {
  int32_t which = 0;
  s->trkl++;
  if (s->trkl > s->trklim) {
    which = 1;
    s->trklim = (int32_t)(nfm_random() * 40.0);
    s->trkl = 0;
  }
  if (force_normal) which = 0;
  HudImg t = (which == 1 && trackbg_dodged.tex >= 0) ? trackbg_dodged : trackbg_normal;
  for (int32_t i = 0; i < 2; i++) {
    if (t.tex >= 0) {
      gfx_draw_image(gr, t.tex, s->trkx[i], 25, t.w, t.h);
    }
    s->trkx[i] -= 10;
    if (s->trkx[i] <= -605) s->trkx[i] = 735;
  }
  gfx_set_color(gr, 0, 0, 0);
  gfx_fill_rect(gr, 0, 0, 65, 450);
  gfx_fill_rect(gr, 735, 0, 65, 450);
  gfx_fill_rect(gr, 65, 0, 670, 25);
  gfx_fill_rect(gr, 65, 425, 670, 25);
}

// GIF loader for menu (no snap tint, opaque pixels kept opaque, transparent
// index kept transparent -- so decorative GIFs like the button/label assets
// render as their author intended, not through the HUD-specific
// grey-to-alpha crush).
/**
 * Loads a PNG from a plain file path rather than from inside images.zip --
 * the zip is the original's own untouched archive, so art this port ADDS
 * (currently the Vita control glyphs under data/vita/) lives beside it as
 * loose files instead. Same upload path and same {-1,0,0} "missing" result
 * as the zip loaders above, so callers can treat all three alike.
 *
 * Compiled only into the Vita build, since that is the only caller today --
 * lift the guard the moment the desktop side needs a loose-file image too.
 */
static HudImg load_menu_png_file(const char *path) {
  HudImg r = {-1, 0, 0};
  int32_t len = 0;
  uint8_t *bytes = vfs_read_bytes(path, &len);
  if (!bytes) {
    fprintf(stderr, "%s: not found\n", path);
    return r;
  }
  PngImage img;
  if (png_decode(bytes, (size_t)len, &img)) {
    r.tex = gfx_gl_upload_texture(img.rgba, img.width, img.height);
    r.w = img.width; r.h = img.height;
    png_free(&img);
  } else {
    fprintf(stderr, "%s: failed to decode (png)\n", path);
  }
  vfs_free_bytes(bytes);
  return r;
}

static HudImg load_menu_gif(VfsZip *zip, const char *name) {
  HudImg r = {-1, 0, 0};
  for (int32_t i = 0; i < zip->count; i++) {
    if (strcmp(zip->entries[i].name, name) != 0) continue;
    GifImage img;
    if (gif_decode(zip->entries[i].data, (size_t)zip->entries[i].len, &img)) {
      r.tex = gfx_gl_upload_texture(img.rgba, img.width, img.height);
      r.w = img.width; r.h = img.height;
      gif_free(&img);
    } else {
      fprintf(stderr, "data/images.zip: %s failed to decode (gif)\n", name);
    }
    break;
  }
  return r;
}

// The menu images with a Spanish twin under data/port/es/. Both are loaded at
// boot (there is no texture-free call, and they are small) and
// lang_imgs_sync() points each slot at the current language's, so a change
// of the Language setting shows on the next frame.
typedef struct { HudImg *slot; HudImg en, es; } LangImg;
// The menus' lettered rows in Spanish, drawn instead of options.png's rows
// (Play Game, Game Instructions, Credits) and options2.png's Free Play.
static HudImg g_es_opti_rows[3] = {{-1, 0, 0}, {-1, 0, 0}, {-1, 0, 0}};
static HudImg g_es_freeplay = {-1, 0, 0};
static LangImg g_lang_imgs[32];
static int32_t g_lang_img_count = 0, g_lang_imgs_lang = -1;

/** Registers *slot (already loaded, the English) with its twin data/port/es/<es_name>.png. */
static void lang_img_add(HudImg *slot, const char *es_name) {
  if (g_lang_img_count >= (int32_t)(sizeof(g_lang_imgs) / sizeof(g_lang_imgs[0]))) return;
  char path[96];
  snprintf(path, sizeof(path), "data/port/es/%s.png", es_name);
  g_lang_imgs_lang = -1;
  LangImg *l = &g_lang_imgs[g_lang_img_count++];
  l->slot = slot;
  l->en = *slot;
  for (int32_t i = 0; i < g_lang_img_count - 1; i++) {
    if (g_lang_imgs[i].en.tex == slot->tex && slot->tex >= 0) {   // a copy of one already paired
      l->es = g_lang_imgs[i].es;
      return;
    }
  }
  l->es = load_menu_png_file(path);
  if (l->es.tex < 0) l->es = l->en;
}

static void lang_imgs_sync(void) {
  if (g_lang_imgs_lang == i18n_lang()) return;
  g_lang_imgs_lang = i18n_lang();
  for (int32_t i = 0; i < g_lang_img_count; i++)
    *g_lang_imgs[i].slot = g_lang_imgs_lang == I18N_LANG_ES ? g_lang_imgs[i].es : g_lang_imgs[i].en;
}

// A loose GIF from data/ (the loading screen's sign/hello/loadbar sit
// beside images.zip rather than inside it, xtGraphics.java:630-632).
static HudImg load_data_gif(const char *path) {
  HudImg r = {-1, 0, 0};
  int32_t len = 0;
  uint8_t *bytes = vfs_read_bytes(path, &len);
  if (!bytes) return r;
  GifImage img;
  if (gif_decode(bytes, (size_t)len, &img)) {
    r.tex = gfx_gl_upload_texture(img.rgba, img.width, img.height);
    r.w = img.width; r.h = img.height;
    gif_free(&img);
  }
  vfs_free_bytes(bytes);
  return r;
}

typedef struct { HudImg sign, hello, loadbar; } BootImages;

// xtGraphics.loading() (:1410-1435) without the box's bottom half, which
// is either the progress readout (boot_loading_frame) or clicknow()'s
// prompt (the STATE_BOOT_CLICK draw).
static void draw_boot_backdrop(Graphics2D *g, const BootImages *bi) {
  gfx_set_color(g, 0, 0, 0);
  gfx_fill_rect(g, 0, 0, 800, 450);
  if (bi->sign.tex >= 0) gfx_draw_image(g, bi->sign.tex, 362, 35, bi->sign.w, bi->sign.h);
  if (bi->hello.tex >= 0) gfx_draw_image(g, bi->hello.tex, 125, 105, bi->hello.w, bi->hello.h);
  gfx_set_color(g, 198, 214, 255);
  gfx_fill_round_rect(g, 250, 340, 300, 80, 30, 70);
  gfx_set_color(g, 128, 167, 255);
  gfx_draw_round_rect(g, 250, 340, 300, 80, 30, 70);
}

// One frame of loading() while the game's assets load: the progress bar
// and "N % loaded | N KB remaining" readout, presented immediately (this
// runs before the main loop, between the blocking loads). `done_kb` of
// `total_kb` -- the original counted downloaded KB; here it is the size
// of the archives read so far.
static void boot_loading_frame(Graphics2D *g, const BootImages *bi, int32_t done_kb, int32_t total_kb) {
  int32_t disp_w, disp_h;
  platform_display_size(&disp_w, &disp_h);
  glViewport(0, 0, disp_w, disp_h);
  glClear(GL_COLOR_BUFFER_BIT);
  gfx_begin(g);
  draw_boot_backdrop(g, bi);
  if (bi->loadbar.tex >= 0) gfx_draw_image(g, bi->loadbar.tex, 281, 365, bi->loadbar.w, bi->loadbar.h);
  gfx_set_color(g, 0, 0, 0);
  font_set(FONT_BOLD, 11);
  draw_centered(g, "Loading game, please wait.", 400, 358);
  gfx_set_color(g, 255, 255, 255);
  gfx_fill_rect(g, 295, 398, 210, 17);
  if (total_kb < 1) total_kb = 1;
  float frac = (float)done_kb / (float)total_kb;
  if (frac > 1.0f) frac = 1.0f;
  char line[64];
  snprintf(line, sizeof(line), "%d %% loaded    |    %d KB remaining",
           (int32_t)((26.0f + frac * 200.0f) / 226.0f * 100.0f), total_kb - done_kb);
  gfx_set_color(g, 32, 64, 128);
  draw_centered(g, line, 400, 410);
  gfx_fill_rect(g, 287, 371, 26 + (int32_t)(frac * 200.0f), 10);
  gfx_submit_gl(g);
  platform_swap_buffers();
}

// The music for a race: xtGraphics.loadstrack() (:2987-3101) picks the
// track and its RadicalMod(file, vol, rate, bpm) arguments per stage, and
// stage 27 in the NFM2 campaign plays music/party.zip instead of its own.
// Returned as an id so a re-race on the same track skips the re-render:
// 1..32 are music/stageN.zip, 33 is party.zip.
static int32_t stage_music_id(int32_t stage_num, int32_t gmode) {
  if (gmode == 4) return g_career_bonus ? 250 + g_career_bonus : 200 + stage_num;   // Extended's career (callers pass 4)
  if (gmode == 3) return 100 + stage_num;   // Extended's normal mode (callers pass 3), ext_stage_music
  if (stage_num == 27 && gmode == 2) return 33;
  if (gmode == 0 && free_play_ext_stage(stage_num)) return 100 + free_play_ext_stage(stage_num);
  return stage_num;
}

// The career's music (XT loadmusic 2633): stage N's stageNa.ogg once, then
// stageNb.ogg looping (ogg_music.c). Held here while it plays.
static uint8_t *g_career_ogg[2];
static int32_t g_career_ogg_len[2];

static void career_ogg_free(void) {
  for (int32_t k = 0; k < 2; k++) {
    free(g_career_ogg[k]);
    g_career_ogg[k] = NULL;
    g_career_ogg_len[k] = 0;
  }
}

// ModuleLoader.loadMod() reads the zip's FIRST entry, whatever its name.
static bool load_music_track(int32_t id, RadicalTrack *out) {
  // {vol, rate, bpm}, loadstrack()'s constructor arguments, index = id.
  static const int16_t kArgs[34][3] = {
      {0, 0, 0},         {240, 8400, 135}, {190, 9000, 145}, {170, 8500, 145}, {205, 7500, 125},
      {170, 7900, 125}, {370, 7900, 125}, {205, 7500, 125}, {230, 7900, 125}, {180, 7900, 125},
      {280, 8100, 145}, {120, 8000, 125}, {260, 7200, 125}, {270, 8000, 125}, {190, 8000, 125},
      {162, 7800, 125}, {220, 7600, 125}, {300, 7500, 125}, {200, 7900, 125}, {200, 7900, 125},
      {232, 7300, 125}, {370, 7900, 125}, {290, 7900, 125}, {222, 7600, 125}, {230, 8000, 125},
      {220, 8000, 125}, {261, 8000, 125}, {276, 8800, 145}, {182, 8000, 125}, {220, 8000, 125},
      {200, 8000, 125}, {350, 7900, 125}, {310, 8000, 125}, {400, 7600, 125}};
  memset(out, 0, sizeof(*out));
  if (id > 250) {
    // The bonus stages (XT 2728-2755, 2988-2994): modules b1-b3, b4 an Ogg pair.
    career_ogg_free();
    const int32_t k = id - 250;
    char path[96];
    if (k == 4) {
      for (int32_t p = 0; p < 2; p++) {
        snprintf(path, sizeof(path), "ext/data/Files/bonusmusic/b4%c.ogg", p ? 'b' : 'a');
        g_career_ogg[p] = vfs_read_bytes(path, &g_career_ogg_len[p]);
      }
      return g_career_ogg[1] != NULL;
    }
    static const int16_t kBonus[3][3] = {{250, 8500, 145}, {190, 8500, 200}, {300, 8500, 220}};
    snprintf(path, sizeof(path), "ext/data/Files/bonusmusic/b%d.radq", (int)k);
    VfsZip zip;
    if (k < 1 || k > 3 || !vfs_read_zip(path, &zip)) return false;
    bool ok = zip.count > 0 && radical_render_stage(zip.entries[0].data, (size_t)zip.entries[0].len, kBonus[k - 1][0],
                                                    kBonus[k - 1][1], kBonus[k - 1][2], out);
    vfs_free_zip(&zip);
    return ok;
  }
  if (id > 200) {
    // Stages 1-25: an Ogg pair (played by audio_start_ogg, `out` stays
    // empty); 26-28 a module in careermusic; 29-31 nothing.
    career_ogg_free();
    const int32_t st = id - 200;
    if (st <= 25) {
      char path[96];
      for (int32_t k = 0; k < 2; k++) {
        snprintf(path, sizeof(path), "ext/data/Files/careermusic/stage%d%c.ogg", (int)st, k ? 'b' : 'a');
        g_career_ogg[k] = vfs_read_bytes(path, &g_career_ogg_len[k]);
      }
      return g_career_ogg[1] != NULL;
    }
    if (st > 28) return false;
    char path[96];
    snprintf(path, sizeof(path), "ext/data/Files/careermusic/stage%d.radq", (int)st);
    VfsZip zip;
    if (!vfs_read_zip(path, &zip)) return false;
    // loadmusic's numbers for stracks 53-55 (XT 2910-2916).
    static const int16_t kLate[3][3] = {{300, 7600, 125}, {305, 7600, 136}, {250, 7600, 135}};
    bool ok = zip.count > 0 && radical_render_stage(zip.entries[0].data, (size_t)zip.entries[0].len, kLate[st - 26][0],
                                                    kLate[st - 26][1], kLate[st - 26][2], out);
    vfs_free_zip(&zip);
    return ok;
  }
  if (id > 100 && id <= 100 + EXT_NORMAL_STAGES) {
    // Extended's normal mode: ext/data/Files/music/stageN.radq, its own
    // loadMod(amp, rate, tempo) (ext_mode.c).
    const ExtMusic mu = ext_stage_music(id - 100);
    char path[96];
    snprintf(path, sizeof(path), "ext/data/Files/music/%s.radq", mu.file);
    VfsZip zip;
    if (!vfs_read_zip(path, &zip)) {
      fprintf(stderr, "could not load %s -- no music this stage\n", path);
      return false;
    }
    bool ok = zip.count > 0 &&
              radical_render_stage(zip.entries[0].data, (size_t)zip.entries[0].len, mu.amp, mu.rate, mu.tempo, out);
    vfs_free_zip(&zip);
    return ok;
  }
  if (id < 1 || id > 33) return false;
  char zip_path[64];
  if (id == 33) snprintf(zip_path, sizeof(zip_path), "music/party.zip");
  else snprintf(zip_path, sizeof(zip_path), "music/stage%d.zip", id);
  VfsZip zip;
  if (!vfs_read_zip(zip_path, &zip)) {
    fprintf(stderr, "could not load %s -- no music this stage\n", zip_path);
    return false;
  }
  bool ok = zip.count > 0 &&
            radical_render_stage(zip.entries[0].data, (size_t)zip.entries[0].len, kArgs[id][0], kArgs[id][1],
                                 kArgs[id][2], out);
  if (!ok) fprintf(stderr, "%s: failed to render\n", zip_path);
  vfs_free_zip(&zip);
  return ok;
}

// new RadicalMod("music/interface.zip") + loadimod(false) -- the menu
// track, rendered once at boot and kept (the original re-renders it each
// time car select opens; the bytes are the same every time).
static bool load_interface_track(RadicalTrack *out) {
  memset(out, 0, sizeof(*out));
  VfsZip zip;
  if (!vfs_read_zip("music/interface.zip", &zip)) {
    fprintf(stderr, "could not load music/interface.zip -- no menu music\n");
    return false;
  }
  bool ok = zip.count > 0 && radical_render_interface(zip.entries[0].data, (size_t)zip.entries[0].len, out);
  vfs_free_zip(&zip);
  return ok;
}

// The post-race unlock-celebration card's fixed Y placement for the
// newly-unlocked car's live 3D render -- xtGraphics.java:6697-6776's
// per-(gmode,stage) `y` literal. Keyed by car index rather than
// (gmode,stage) like the Java: every car index that can be unlocked
// this way gets the SAME y in both gamemodes' tables (car 11 -> 326 and
// car 14 -> 350 in both NFM1's and NFM2's branches, checked directly
// against the Java source), so one car-indexed table covers both without
// needing the gamemode as a second key.
static int32_t post_race_unlock_car_y(int32_t car_index) {
  switch (car_index) {
    case 5: return 365;
    case 6: return 320;
    case 8: return 365;
    case 9: return 320;
    case 10: return 370;
    case 11: return 326;
    case 12: return 310;
    case 13: return 310;
    case 14: return 350;
    case 15: return 370;
    default: return 350;
  }
}

/** trunc(fr(base + fr(base*fr(snap_pct/100.0)))), clamped 0-255 -- the
 * exact tint formula XtGraphics.js's arrow()/drawcs()/drawstat() all use
 * (each of the 3 channels calls this with the same shape, only the base
 * literal and snap[] channel differ). Case 2 in this port's fr()/trunc()
 * convention: a single outer trunc(fr(...)) around a multi-op expression
 * -> compute in double, round once. */
static int32_t hud_tint(double base, int32_t snap_pct) {
  int32_t v = jtrunc_d(base + base * ((double)snap_pct / 100.0));
  if (v > 255) v = 255;
  if (v < 0) v = 0;
  return v;
}

// Forward declaration -- the arrow's arrace branch labels its locked
// target with drawcs(), whose port lives further down this file.
static void hud_say_draw(Graphics2D *g, Medium *m, int32_t y, const char *str,
                          int32_t r, int32_t gg, int32_t b, int32_t mode);

// The career's arrow skips the cars careermode$m takes out of it (noarrow,
// norender: the undead, stage 13's other floors). NULL outside the career.
static const bool *g_arrow_skip = NULL;
// ...and the standings the undead (stat$m 4274, noarrow). NULL outside the career.
static const bool *g_board_skip = NULL;
// The career's levels by slot, for the car the arrow points at. NULL outside it.
static const int32_t *g_arrow_levels = NULL;
// Extended's cars, specials and list-bars toggle (control.swap), for the
// locked car's bars beside its name (XT 1534-1627). NULL outside Extended.
static const Mad *g_arrow_mads = NULL;
static const Specials *g_arrow_specials = NULL;
static const bool *g_arrow_swap = NULL;

/**
 * Ports the !arrace branch of XtGraphics.js's arrow(n, n2, checkPoints, b)
 * (lines 1967-2159) -- a 7-vertex world-space polygon (an arrowhead lying
 * flat at y=-90, base z=700, drawn as if painted on the road ahead of the
 * car) rotated to bear toward checkpoint `point` and projected through the
 * SAME perspective this file's 3D scene uses (medium_xs/ys). The arrace
 * (radar-lock, car-vs-car targeting) branch is genuinely out of scope: see
 * xt_graphics.h's own comment on why arrace stays false permanently here.
 *
 * Reads cp->opx[0]/opz[0] (the player's own current x/z, refreshed every
 * tick by the already-ported check_points_checkstat -- JS's own
 * `checkPoints.opx[this.im]`), and mutates xt->ana/flk persistently across
 * frames, matching the JS's own `this.ana`/`this.flk` instance fields.
 */
static void draw_checkpoint_arrow(Graphics2D *g, Medium *m, XtGraphicsStub *xt,
                                   CheckPoints *cp, int32_t point, int32_t missedcp,
                                   bool arrace, int32_t nplayers, const int32_t *sc) {
  // Defensive guard on `point`, not something the original has -- and only
  // meaningful for the checkpoint-bearing branch, since the arrace branch
  // never reads `point` at all.
  if (!arrace && (point < 0 || point >= cp->n)) return;

  int32_t ax[7], az[7];
  const int32_t ay = -90;
  ax[0] = 400; az[0] = 810;
  ax[1] = 365; az[1] = 750;
  ax[2] = 385; az[2] = 750;
  ax[3] = 385; az[3] = 650;
  ax[4] = 415; az[4] = 650;
  ax[5] = 415; az[5] = 750;
  ax[6] = 435; az[6] = 750;

  int32_t n7;
  int32_t target = 0; // the rival the arrace branch settled on, for the label below
  if (!arrace) {
    // :8643-8648 -- bearing toward the next checkpoint.
    double n6 = (cp->x[point] - cp->opx[0] >= 0) ? 180.0 : 0.0;
    double ratio = (double)(cp->z[point] - cp->opz[0]) / (double)(cp->x[point] - cp->opx[0]);
    // trunc() directly on raw double arithmetic, no fr() anywhere -- case 3.
    n7 = jtrunc_d(90.0 + n6 + atan(ratio) / 0.017453292519943295);
  } else {
    // :8650-8676 -- bearing toward a RIVAL instead. Java picks the target
    // under `multion == 0 || alocked == -1`; both hold here (single-player,
    // and alocked is only ever set by the unported mouse-hover branch), so
    // the auto-search always runs and `this.alocked` is never consulted.
    //
    // The search keeps the nearest live rival by SQUARED distance in
    // hundredths of a world unit (py(), :8656 -- the /100 is Java integer
    // division, matching C's own truncation), with one twist: `seen_on`
    // latches as soon as any on-screen candidate is accepted, and from
    // then on the `(seen_on == 0 || onscreen[j] != 0)` term rejects every
    // off-screen car. So a visible rival always beats an invisible closer
    // one -- but only if the visible one is examined while the latch is
    // still down, which is exactly the order-dependent behaviour the
    // original has.
    int32_t best = -1; // py == -1 sentinel: nothing accepted yet
    int32_t seen_on = 0;
    for (int32_t j = 0; j < nplayers; j++) {
      if (j == 0) continue;
      if (g_arrow_skip && g_arrow_skip[j]) continue;
      int32_t dx = cp->opx[0] / 100 - cp->opx[j] / 100;
      int32_t dz = cp->opz[0] / 100 - cp->opz[j] / 100;
      int32_t d2 = dx * dx + dz * dz;
      if ((d2 < best || best == -1) && (seen_on == 0 || cp->onscreen[j] != 0) &&
          cp->dested[j] == 0) {
        target = j;
        best = d2;
        if (cp->onscreen[j] != 0) seen_on = 1;
      }
    }
    double n9 = (cp->opx[target] - cp->opx[0] >= 0) ? 180.0 : 0.0;
    double ratio = (double)(cp->opz[target] - cp->opz[0]) / (double)(cp->opx[target] - cp->opx[0]);
    n7 = jtrunc_d(90.0 + n9 + atan(ratio) / 0.017453292519943295);
  }

  int32_t k = n7 + m->xz;
  while (k < 0) k += 360;
  while (k > 180) k -= 360;
  // :8694-8709 -- the arrace branch is allowed to swing further round
  // (+-100) than the checkpoint one (+-130).
  int32_t lim = arrace ? 100 : 130;
  if (k > lim) k = lim;
  if (k < -lim) k = -lim;

  if (abs(xt->ana - k) < 180) {
    if (abs(xt->ana - k) < 10) xt->ana = k;
    else if (xt->ana < k) xt->ana += 10;
    else xt->ana -= 10;
  } else {
    if (k < 0) { xt->ana += 15; if (xt->ana > 180) xt->ana -= 360; }
    if (k > 0) { xt->ana -= 15; if (xt->ana < -180) xt->ana += 360; }
  }

  medium_rot(m, ax, az, 400, 700, xt->ana, 7);
  int32_t abs_ana = abs(xt->ana);

  int32_t sx[7], sy[7];

  if (arrace) {
    // :8844-8898 -- the car-hunting arrow. No visibility gate at all
    // (unlike the checkpoint branch's own `abs > 7 || ...` test below):
    // once A is on, this arrow is always drawn. `n11` is the +8px y nudge
    // multiplayer applies to make room for the player-name line above the
    // car name; multion is 0 here, so it stays 0 and the projection is
    // identical to the other branch's.
    for (int32_t l = 0; l < 7; l++) {
      sx[l] = medium_xs(m, ax[l], az[l]);
      sy[l] = medium_ys(m, ay, az[l]);
    }
    gfx_set_color(g, hud_tint(159.0, m->snap[0]), hud_tint(207.0, m->snap[1]),
                  hud_tint(255.0, m->snap[2]));
    gfx_fill_polygon(g, sx, sy, 7);
    gfx_set_color(g, hud_tint(120.0, m->snap[0]), hud_tint(114.0, m->snap[1]),
                  hud_tint(255.0, m->snap[2]));
    gfx_draw_polygon(g, sx, sy, 7);
    // :8673-8676 -- name the locked car, framed by a bracket pair, in Arial
    // bold 11 (Extended's XT 1469-1471: "[" + 36 spaces + "]"); without its
    // own font it took whatever the frame drew last.
    font_set(FONT_BOLD, 11);
    if (g_arrow_mads) {
      // Round the name at 435, the bars clear of it on the left.
      static const char kBracket[] = "[                                    ]";
      gfx_set_color(g, hud_tint(76.0, m->snap[0]), hud_tint(67.0, m->snap[1]), hud_tint(240.0, m->snap[2]));
      font_draw(g, kBracket, 435 - font_width(kBracket) / 2, 13);
    } else {
      hud_say_draw(g, m, 13, "[                                    ]", 76, 67, 240, 0);
    }
    const bool valid = target >= 0 && target < NFM_MAX_CARS && sc[target] >= 0 && sc[target] < CAR_COUNT;
    if (valid && g_arrow_mads && target > 0) {
      // Extended (XT 1471-1508): the name at x 435, coloured by what is on
      // the car (a running special red, frozen blue, weakened orange,
      // leeched brown, swapped green).
      const Mad *t = &g_arrow_mads[target];
      const bool fixsp = g_arrow_specials && g_arrow_specials->fixspecials[target];
      int32_t nr = 0, ng = 0, nb = 0;
      if (t->strswap) ng = 200;
      else if (t->leech) nr = 140, ng = 105;
      else if (t->redstr) nr = 220, ng = 110;
      else if (t->frozen) nb = 220;
      else if (fixsp) nr = 200;
      const char *nm = car_name(sc[target]);
      gfx_set_color(g, nr, ng, nb);
      font_draw(g, nm, 435 - font_width(nm) / 2, 13);
      // Its damage bar (or, with the list bars on specials, its special's
      // charge), then its power, 60 wide at x 300 (XT 1534-1627).
      int32_t fill, r, gg, b;
      if (!(g_arrow_swap && *g_arrow_swap)) {
        fill = (int32_t)(60.0f * ((float)t->hitmag / (float)t->cd->maxmag[t->cn]));
        r = 244; gg = 244; b = 11;
        if (fill > 20) gg = (int32_t)(244.0f - 233.0f * ((float)(fill - 20) / 40.0f));
        if (fill > 60) fill = 60;
        r = hud_tint((double)r, m->snap[0]);
        gg = hud_tint((double)gg, m->snap[1]);
        b = hud_tint((double)b, m->snap[2]);
      } else {
        fill = (int32_t)(60.0f * ((fixsp ? t->speclast : t->spatk) / 120.0f));
        r = hud_tint(fixsp ? 85.0 : 220.0, m->snap[0]);
        gg = b = 0;
      }
      if (fill < 0) fill = 0;
      gfx_set_color(g, r, gg, b);
      gfx_fill_rect(g, 300, 5, fill, 6);
      gfx_set_color(g, 0, 0, 0);
      gfx_draw_rect(g, 300, 5, 60, 6);
      fill = (int32_t)(60.0f * (t->power / 98.0f));
      if (fill > 60) fill = 60;
      if (fill < 0) fill = 0;
      gfx_set_color(g, hud_tint(t->power == 98.0f ? 64.0 : 128.0, m->snap[0]), hud_tint(244.0, m->snap[1]),
                    hud_tint(244.0, m->snap[2]));
      gfx_fill_rect(g, 300, 16, fill, 6);
      gfx_set_color(g, 0, 0, 0);
      gfx_draw_rect(g, 300, 16, 60, 6);
    } else if (valid) {
      hud_say_draw(g, m, 13, car_name(sc[target]), 0, 0, 0, 0);
    }
    if (g_arrow_levels && target > 0) {
      // The career: its level beside it, red when more than 5 above yours
      // (XT 1901-1912).
      char lv[24];
      snprintf(lv, sizeof(lv), "Level %d", (int)g_arrow_levels[target]);
      font_set(FONT_BOLD, 15);
      if (g_arrow_levels[target] > g_arrow_levels[0] + 5) gfx_set_color(g, 150, 0, 0);
      else gfx_set_color(g, 0, 60, 0);
      font_draw(g, lv, g_arrow_mads ? 523 : 488, 15);   // Extended: beside the name at 435
    }
    return;
  }

  if (!(abs_ana > 7 || missedcp > 0 || missedcp == -2 || xt->cntan != 0)) return;

  for (int32_t l = 0; l < 7; l++) {
    sx[l] = medium_xs(m, ax[l], az[l]);
    sy[l] = medium_ys(m, ay, az[l]);
  }

  int32_t r = hud_tint(190.0, m->snap[0]);
  int32_t gc = hud_tint(255.0, m->snap[1]);
  int32_t b = 0;
  if (missedcp <= 0) {
    if (abs_ana <= 45 && missedcp != -2 && xt->cntan == 0) {
      r = (r * abs_ana + m->csky[0] * (45 - abs_ana)) / 45;
      gc = (gc * abs_ana + m->csky[1] * (45 - abs_ana)) / 45;
      b = (b * abs_ana + m->csky[2] * (45 - abs_ana)) / 45;
    }
    if (abs_ana >= 90) {
      int32_t n10 = hud_tint(255.0, m->snap[0]);
      r = (r * (140 - abs_ana) + n10 * (abs_ana - 90)) / 50;
      if (r > 255) r = 255;
    }
  } else if (xt->flk) {
    r = hud_tint(255.0, m->snap[0]);
    xt->flk = false;
  } else {
    r = hud_tint(255.0, m->snap[0]);
    gc = hud_tint(220.0, m->snap[1]);
    xt->flk = true;
  }
  gfx_set_color(g, r, gc, b);
  gfx_fill_polygon(g, sx, sy, 7);

  // Outline colour: reads xt->flk AFTER the fill-colour block above may
  // have just toggled it -- a real ordering quirk of the original (both
  // blocks share the one flag), not a transcription slip.
  int32_t r2 = hud_tint(115.0, m->snap[0]);
  int32_t g2 = hud_tint(170.0, m->snap[1]);
  int32_t b2 = 0;
  if (missedcp <= 0) {
    if (abs_ana <= 45 && missedcp != -2 && xt->cntan == 0) {
      r2 = (r2 * abs_ana + m->csky[0] * (45 - abs_ana)) / 45;
      g2 = (g2 * abs_ana + m->csky[1] * (45 - abs_ana)) / 45;
      b2 = (b2 * abs_ana + m->csky[2] * (45 - abs_ana)) / 45;
    }
  } else if (xt->flk) {
    r2 = hud_tint(255.0, m->snap[0]);
    g2 = 0;
  }
  gfx_set_color(g, r2, g2, b2);
  gfx_draw_polygon(g, sx, sy, 7);
}

/**
 * Ports XtGraphics.js's drawcs(y, str, r, g, b, mode) (lines 1917-1965) --
 * ONLY modes 0 (plain snap[]-tint) and 2 (tint then blend 2:1 with csky),
 * the only two this port's scoped message set (Checkpoint!/Checkpoint
 * Missed!/Wrong Way!/Car Fixed, see the message-trigger code further
 * down) ever passes. Modes 1/3/4/5 (drop-shadow copy, untinted, negative
 * tint, half-csky-solid -- used by menus/announcer-stunt text/multiplayer
 * banners this port doesn't have) are genuinely out of scope, not an
 * oversight. Centers `str` at x=400 via the existing draw_centered
 * helper -- matches drawcs's own `400 - stringWidth(str)/2`, since this
 * window is 800 wide same as the original's canvas.
 */
static void hud_say_draw(Graphics2D *g, Medium *m, int32_t y, const char *str,
                          int32_t r, int32_t gg, int32_t b, int32_t mode) {
  r = hud_tint((double)r, m->snap[0]);
  gg = hud_tint((double)gg, m->snap[1]);
  b = hud_tint((double)b, m->snap[2]);
  if (mode == 2) {
    r = (r * 2 + m->csky[0]) / 3;
    if (r > 255) r = 255;
    if (r < 0) r = 0;
    gg = (gg * 2 + m->csky[1]) / 3;
    if (gg > 255) gg = 255;
    if (gg < 0) gg = 0;
    b = (b * 2 + m->csky[2]) / 3;
    if (b > 255) b = 255;
    if (b < 0) b = 0;
  }
  if (mode == 1) {
    // drawcs's drop shadow (xtGraphics.java drawcs, `n2 == 1`): the same
    // string in black one pixel down-right, under the coloured copy.
    gfx_set_color(g, 0, 0, 0);
    draw_centered(g, str, 401, y + 1);
  }
  hud_set_ink(g, r, gg, b);
  draw_centered(g, str, 400, y);
}

/**
 * Ports XtGraphics.js's tickMissedCp(mad, checkPoints) (lines 846-862) --
 * already simplified for single-player by the JS port itself (see its own
 * comment there): advances mad->missedcp toward the "Checkpoint Missed!"
 * display window and wraps it to -2 (waiting-to-reset) at 70.
 *
 * In the Java this advance is not a standalone helper at all -- it's the
 * `++mad.missedcp; if (mad.missedcp == 70) mad.missedcp = -2;` tail of the
 * very block hud_wrongway_tick() ports (xtGraphics.java:7930-7933), so it
 * lives under that block's `!holdit && starcnt == 0 && stage != 10` gate
 * too, not merely under `capcnt == 0`. Its call site in the tick loop
 * repeats that guard for exactly that reason: without it the missed-
 * checkpoint timer keeps ticking (and can wrap to -2, cancelling a
 * pending banner) during the countdown and behind the win/lose hold card.
 */
static void tick_missed_cp(Mad *mad) {
  if (mad->capcnt != 0) return;
  if (mad->missedcp > 0) {
    mad->missedcp++;
    if (mad->missedcp == 70) mad->missedcp = -2;
  }
}

// XtGraphics.js's own adj[]/exlm[] literal tables (lines 296-303) -- the
// random adjective/exclamation a landed stunt's announcement gets
// prefixed/suffixed with, sized by how big mad->powerup was.
static const char *kStuntAdj[5][3] = {
    {"Cool", "Alright", "Nice"},
    {"Wicked", "Amazing", "Super"},
    {"Awesome", "Ripping", "Radical"},
    {"What the...?", "You're a super star!!!!", "Who are you again...?"},
    {"surf style", "off the lip", "bounce back"},
};
static const char *kStuntExlm[3] = {"!", "!!", "!!!"};

// Bounded string-append helper -- truncates rather than overflows if a
// caller's own size accounting is ever off, defense-in-depth on top of
// the generous buffer sizes xt_graphics.h's own comment already reasons
// through for the worst-case assembled announcement.
static void xt_append(char *dst, size_t dstsz, const char *suffix) {
  size_t len = strlen(dst);
  if (len < dstsz - 1) snprintf(dst + len, dstsz - len, "%s", suffix);
}

/**
 * Ports the mad->trcnt===10 stunt-detection/naming block of XtGraphics.js's
 * stat() (lines 1549-1722) -- builds xt->loop/xt->spin/xt->asay's text
 * from how the just-landed trick's travxy/travzy/travxz rotation
 * accumulated (see mad.c's own loop state machine for where those are
 * filled in), decrements xt->auscnt to open the asay display window, and
 * on auscnt<45 finalizes the announcement (adj[]/exlm[] decoration,
 * "Power Up X%"/"Power To The MAX" say/tcnt trigger, powerup.wav).
 *
 * PHYSICS-TICK-AUTHORITATIVE, NOT frame-authoritative, unlike the rest of
 * this file's message/HUD helpers: `mad->trcnt===10` is a one-shot
 * instant (the tick trcnt becomes exactly 10 on, not a level condition
 * like mad->clear or mad->newcar that stays changed until explicitly
 * un-set), so checking it from the once-per-RENDERED-FRAME draw call
 * would re-fire this whole block -- rebuilding the announcement and
 * replaying powerup.wav -- on every frame drawn while a tick hasn't
 * advanced trcnt past 10 yet (frame rate frequently exceeds this port's
 * fixed ~18.9 ticks/sec). Call once per TICK, matching how the
 * checkpoint/carfixed sound triggers are already tick-scoped in the main
 * loop for the identical reason.
 */
static void hud_stunt_detect(Mad *mad, XtGraphicsStub *xt, Medium *m, Audio *audio, WavClip *snd_powerup) {
  if (mad->trcnt != 10) return;

  xt->loop[0] = '\0';
  xt->spin[0] = '\0';
  xt->asay[0] = '\0';

  int32_t n5 = 0;
  while (mad->travzy > 225) { mad->travzy -= 360; n5++; }
  while (mad->travzy < -225) { mad->travzy += 360; n5--; }
  if (n5 == 1) strncpy(xt->loop, "Forward loop", sizeof(xt->loop) - 1);
  if (n5 == 2) strncpy(xt->loop, "double Forward", sizeof(xt->loop) - 1);
  if (n5 == 3) strncpy(xt->loop, "triple Forward", sizeof(xt->loop) - 1);
  if (n5 >= 4) strncpy(xt->loop, "massive Forward looping", sizeof(xt->loop) - 1);
  if (n5 == -1) strncpy(xt->loop, "Backloop", sizeof(xt->loop) - 1);
  if (n5 == -2) strncpy(xt->loop, "double Back", sizeof(xt->loop) - 1);
  if (n5 == -3) strncpy(xt->loop, "triple Back", sizeof(xt->loop) - 1);
  if (n5 <= -4) strncpy(xt->loop, "massive Back looping", sizeof(xt->loop) - 1);
  if (n5 == 0) {
    if (mad->ftab && mad->btab) strncpy(xt->loop, "Tabletop and reversed Tabletop", sizeof(xt->loop) - 1);
    else if (mad->ftab || mad->btab) strncpy(xt->loop, "Tabletop", sizeof(xt->loop) - 1);
  }
  // snprintf rather than strncpy for the copy back: strncpy(dst, src,
  // sizeof(dst) - 1) leaves dst[sizeof - 1] untouched, so it only
  // terminates by relying on that byte already being NUL -- true here
  // today, but only because nothing ever writes the last byte of
  // xt->loop, which is not a property any of this code states or
  // enforces. gcc's -Wstringop-truncation flags exactly that (it cannot
  // see that `tmp` never actually reaches 39 chars: the longest real
  // announcement is "Hanged Tabletop and reversed Tabletop", 37). The
  // snprintf form always terminates, so the guarantee stops depending on
  // a neighbouring byte, and the warning goes away with it. Same
  // truncation point either way, so no behaviour change.
  // Prefixing "Hanged " onto xt->loop, written so every step is provably
  // in-bounds to the compiler rather than merely true in practice. `tmp`
  // is sized for the worst case the types allow (the prefix plus a
  // completely full xt->loop), so the snprintf can never truncate; the
  // copy back is then a fixed-size memcpy between compile-time-known
  // sizes, with the last byte forced to NUL since a memcpy carries no
  // terminator guarantee of its own.
  //
  // The original form -- snprintf into a char[64] scratch, then
  // strncpy(xt->loop, tmp, sizeof(xt->loop) - 1) -- drew both
  // -Wstringop-truncation and, once reworked naively, -Wformat-
  // truncation. Both were fair: gcc cannot see that the scratch never
  // actually fills (the longest real announcement is "Hanged Tabletop
  // and reversed Tabletop", 37 chars, inside xt->loop's 40), and
  // strncpy with `sizeof - 1` would genuinely have left the result
  // unterminated if it ever did, relying on the last byte of xt->loop
  // happening to already be zero. Same visible text either way.
  enum { kHangedPrefixLen = sizeof("Hanged ") - 1 };
  if (n5 > 0 && mad->btab) {
    char tmp[kHangedPrefixLen + sizeof(xt->loop)];
    snprintf(tmp, sizeof(tmp), "Hanged %s", xt->loop);
    memcpy(xt->loop, tmp, sizeof(xt->loop));
    xt->loop[sizeof(xt->loop) - 1] = '\0';
  }
  if (n5 < 0 && mad->ftab) {
    char tmp[kHangedPrefixLen + sizeof(xt->loop)];
    snprintf(tmp, sizeof(tmp), "Hanged %s", xt->loop);
    memcpy(xt->loop, tmp, sizeof(xt->loop));
    xt->loop[sizeof(xt->loop) - 1] = '\0';
  }
  if (xt->loop[0] != '\0') { xt_append(xt->asay, sizeof(xt->asay), " "); xt_append(xt->asay, sizeof(xt->asay), xt->loop); }

  // Local copies, not the live mad->travxy/travxz -- matches
  // web/XtGraphics.js's own comment on why (a netplay-desync concern
  // that doesn't apply here, but preserved for exact fidelity: unlike
  // travzy above, these two are read again by mad.c's physics before
  // their next reset, so the announcer must not mutate them live).
  int32_t n6 = 0;
  int32_t travxy = abs(mad->travxy);
  while (travxy > 270) { travxy -= 360; n6++; }
  if (n6 == 0 && mad->rtab) {
    if (xt->loop[0] == '\0') strncpy(xt->spin, "Tabletop", sizeof(xt->spin) - 1);
    else strncpy(xt->spin, "Flipside", sizeof(xt->spin) - 1);
  }
  if (n6 == 1) strncpy(xt->spin, "Rollspin", sizeof(xt->spin) - 1);
  if (n6 == 2) strncpy(xt->spin, "double Rollspin", sizeof(xt->spin) - 1);
  if (n6 == 3) strncpy(xt->spin, "triple Rollspin", sizeof(xt->spin) - 1);
  if (n6 >= 4) strncpy(xt->spin, "massive Roll spinning", sizeof(xt->spin) - 1);

  int32_t n7 = 0;
  bool b5 = false;
  int32_t travxz = abs(mad->travxz);
  while (travxz > 90) {
    travxz -= 180;
    n7 += 180;
    if (n7 > 900) { n7 = 900; b5 = true; }
  }
  if (n7 != 0) {
    if (xt->loop[0] == '\0' && xt->spin[0] == '\0') {
      char num[16];
      snprintf(num, sizeof(num), " %d", n7);
      xt_append(xt->asay, sizeof(xt->asay), num);
      if (b5) xt_append(xt->asay, sizeof(xt->asay), " and beyond");
    } else {
      if (xt->spin[0] != '\0') {
        if (xt->loop[0] == '\0') xt_append(xt->asay, sizeof(xt->asay), " ");
        else xt_append(xt->asay, sizeof(xt->asay), " with ");
        xt_append(xt->asay, sizeof(xt->asay), xt->spin);
      }
      char byN[16];
      snprintf(byN, sizeof(byN), " by %d", n7);
      xt_append(xt->asay, sizeof(xt->asay), byN);
      if (b5) xt_append(xt->asay, sizeof(xt->asay), " and beyond");
    }
  } else if (xt->spin[0] != '\0') {
    if (xt->loop[0] == '\0') xt_append(xt->asay, sizeof(xt->asay), " ");
    else xt_append(xt->asay, sizeof(xt->asay), " by ");
    xt_append(xt->asay, sizeof(xt->asay), xt->spin);
  }

  if (xt->asay[0] != '\0') xt->auscnt -= 15;
  if (xt->loop[0] != '\0') xt->auscnt -= 25;
  if (xt->spin[0] != '\0') xt->auscnt -= 25;
  if (n7 != 0) xt->auscnt -= 25;

  if (xt->auscnt < 45) {
    if (!xt->mutes && snd_powerup->samples) {
      audio_play(audio, snd_powerup->samples, snd_powerup->frame_count, snd_powerup->sample_rate, 1.0f, false);
    }
    if (xt->auscnt < -20) xt->auscnt = -20;

    int32_t n8 = 0;
    if (mad->powerup > 20.0f) n8 = 1;
    if (mad->powerup > 40.0f) n8 = 2;
    if (mad->powerup > 150.0f) n8 = 3;

    if (mad->surfer) {
      int32_t pick = jtrunc(medium_random(m) * 3.0f);
      char tmp[256]; // headroom over " " + longest adj + asay[192] so gcc's format-truncation check can prove this fits
      snprintf(tmp, sizeof(tmp), " %s%s", kStuntAdj[4][pick], xt->asay);
      // memcpy of a fixed, compile-time-known size rather than
      // strncpy(..., sizeof - 1): identical result (the explicit
      // terminator below already made this safe), but nothing left for
      // -Wstringop-truncation to flag, so the warning does not sit here
      // competing for attention with a real one.
      memcpy(xt->asay, tmp, sizeof(xt->asay));
      xt->asay[sizeof(xt->asay) - 1] = '\0';
    }
    int32_t pick2 = jtrunc(medium_random(m) * 3.0f);
    if (n8 != 3) {
      char tmp[256]; // headroom over longest adj + asay[192] + longest exlm so gcc's format-truncation check can prove this fits
      snprintf(tmp, sizeof(tmp), "%s%s%s", kStuntAdj[n8][pick2], xt->asay, kStuntExlm[n8]);
      // Fixed-size memcpy, same reasoning as the surfer branch above.
      memcpy(xt->asay, tmp, sizeof(xt->asay));
      xt->asay[sizeof(xt->asay) - 1] = '\0';
    } else {
      strncpy(xt->asay, kStuntAdj[n8][pick2], sizeof(xt->asay) - 1);
      xt->asay[sizeof(xt->asay) - 1] = '\0';
    }

    if (!xt->wasay) {
      xt->tcnt = xt->auscnt;
      if (mad->power != 98.0f) {
        snprintf(xt->say, sizeof(xt->say), "Power Up %d%%", jtrunc_d(100.0 * (double)mad->powerup / 98.0));
      } else {
        strncpy(xt->say, "Power To The MAX", sizeof(xt->say) - 1);
      }
      xt->skidup = !xt->skidup;
    }
  }
}

// Medium's two career colour effects, which careermode$m only steers:
// stage 22's night (Medium.js 1615-1665: the colours drawn as if snap were
// lowered by CareerStage.dn_dim) and stage 23's greying ground (Medium.js
// 1450-1483: the ground drained towards grey by CareerStage.greystage). The
// stage's own colours are kept here; the fog's and the patches' are taken
// back off their snapped values (the Medium keeps no copy of those).
typedef struct {
  bool on;
  int32_t stage;
  int32_t snap[3];
  int32_t sky[3], fog[3], grnd[3], poly[3];   // unsnapped
  int32_t dim;
} CareerMedium;

static int32_t unsnap(int32_t c, int32_t snap) {
  const double f = 1.0 + snap / 100.0;
  return f > 0.0 ? (int32_t)floor(c / f + 0.5) : c;
}

static void career_medium_init(CareerMedium *cm, const Medium *m) {
  memset(cm, 0, sizeof(*cm));
  for (int32_t a = 0; a < 3; a++) {
    cm->snap[a] = m->snap[a];
    cm->sky[a] = m->osky[a];
    cm->grnd[a] = m->ogrnd[a];
    cm->fog[a] = unsnap(m->cfade[a], m->snap[a]);
    cm->poly[a] = unsnap(m->cpol[a], m->snap[a]);
  }
  cm->on = true;
}

static void career_medium_tick(CareerMedium *cm, Medium *m, const CareerStage *cs) {
  if (!cm->on) return;
  if (cs->stage == 22 && cs->dn_dim != cm->dim) {
    cm->dim = cs->dn_dim;
    for (int32_t a = 0; a < 3; a++) {
      m->snap[a] = cm->snap[a] - cm->dim;
      m->csky[a] = medium_snapped(cm->sky[a], m->snap[a]);
      m->cfade[a] = medium_snapped(cm->fog[a], m->snap[a]);
      m->cgrnd[a] = medium_snapped(cm->grnd[a], m->snap[a]);
      m->cpol[a] = medium_snapped(cm->poly[a], m->snap[a]);
      m->crgrnd[a] = jtrunc_d(((double)m->cpol[a] * 0.99 + (double)m->cgrnd[a]) / 2.0);
    }
  }
  if (cs->stage == 23 && cs->greystage >= 2 && cs->greystage <= 5) {
    const int32_t a = cs->greystage - 2;
    bool changed = false;
    if (cm->grnd[0] > 99 - a * 7) { cm->grnd[0]--; cm->poly[0]--; changed = true; }
    if (cm->grnd[1] > 186 - a * 36) { cm->grnd[1] -= 3; cm->poly[1] -= 3; changed = true; }
    if (cm->grnd[2] > 102 - a * 8) { cm->grnd[2]--; cm->poly[2]--; changed = true; }
    if (changed) {
      for (int32_t q = 0; q < 3; q++) {
        m->cgrnd[q] = medium_snapped(cm->grnd[q], m->snap[q]);
        m->cpol[q] = medium_snapped(cm->poly[q], m->snap[q]);
        m->crgrnd[q] = jtrunc_d(((double)m->cpol[q] * 0.99 + (double)m->cgrnd[q]) / 2.0);
      }
    }
  }
}

// Extended's own sounds (ext/data/Files/sounds) are 16-bit stereo, which
// wav_decode leaves out: the two channels are averaged into one, in place,
// and the fmt/data chunks rewritten to say so, then decoded as usual.
static bool wav_decode_downmix(uint8_t *b, size_t len, WavClip *out) {
  if (wav_decode(b, len, out)) return true;
  if (len < 12 || memcmp(b, "RIFF", 4) != 0 || memcmp(b + 8, "WAVE", 4) != 0) return false;
  uint8_t *fmt = NULL, *data = NULL;
  uint32_t data_size = 0;
  for (size_t off = 12; off + 8 <= len;) {
    const uint32_t sz = (uint32_t)b[off + 4] | (uint32_t)b[off + 5] << 8 | (uint32_t)b[off + 6] << 16 | (uint32_t)b[off + 7] << 24;
    if (off + 8 + sz > len) break;
    if (memcmp(b + off, "fmt ", 4) == 0 && sz >= 16) fmt = b + off + 8;
    if (memcmp(b + off, "data", 4) == 0) { data = b + off + 8; data_size = sz; }
    off += 8 + sz + (sz & 1);
  }
  if (!fmt || !data || fmt[0] != 1 || fmt[1] != 0 || fmt[2] != 2 || fmt[14] != 16) return false;
  const uint32_t frames = data_size / 4;
  for (uint32_t i = 0; i < frames; i++) {
    const int16_t l = (int16_t)(data[i * 4] | data[i * 4 + 1] << 8), r = (int16_t)(data[i * 4 + 2] | data[i * 4 + 3] << 8);
    const int16_t mono = (int16_t)(((int32_t)l + (int32_t)r) / 2);
    data[i * 2] = (uint8_t)(mono & 0xff);
    data[i * 2 + 1] = (uint8_t)((uint16_t)mono >> 8);
  }
  fmt[2] = 1;      // channels
  fmt[12] = 2;     // block align
  fmt[13] = 0;
  const uint32_t nsz = frames * 2;
  data[-4] = (uint8_t)nsz;
  data[-3] = (uint8_t)(nsz >> 8);
  data[-2] = (uint8_t)(nsz >> 16);
  data[-1] = (uint8_t)(nsz >> 24);
  return wav_decode(b, len, out);
}

static void play_wav_oneshot(Audio *audio, const WavClip *clip) {
  if (clip->samples) audio_play(audio, clip->samples, clip->frame_count, clip->sample_rate, 1.0f, false);
}

/**
 * Stops every physics-SFX LOOP (5 engine revs + the one active air
 * whoosh + the wasted rumble) and resets this port's own channel-
 * tracking state, so the next race starts clean and nothing keeps
 * looping into the post-race screen or menu. web/audio.js's own
 * Audio class has a `stopAllLoops()` for exactly this ("Cut every loop
 * -- used when the race ends or the car is destroyed"); this is that,
 * scoped to the channel-index bookkeeping this native backend needs
 * that the JS's name-keyed Map doesn't.
 */
static void stop_all_sfx_loops(Audio *audio, int32_t engine_channel[5], int32_t *last_engine_bank,
                                int32_t *air_channel, int32_t *wasted_channel) {
  for (int32_t j = 0; j < 5; j++) {
    if (engine_channel[j] != -1) { audio_stop(audio, engine_channel[j]); engine_channel[j] = -1; }
  }
  *last_engine_bank = -1;
  if (*air_channel != -1) { audio_stop(audio, *air_channel); *air_channel = -1; }
  if (*wasted_channel != -1) { audio_stop(audio, *wasted_channel); *wasted_channel = -1; }
}

/**
 * Ports the message/announcer pieces of XtGraphics.js's stat() this port
 * scopes to (see xt_graphics.h's own field comment): the "Checkpoint
 * Missed!"/"Wrong Way!" banners (lines 1317-1345, driven live off
 * mad->missedcp/mad->mtouch, now correctly gated on `auscnt===45` --
 * i.e. suppressed while a stunt name is showing, matching the original
 * exactly now that auscnt actually varies -- see below), the "Power low,
 * perform stunt!"/"Please read the Game Instructions!" nag (1466-1504),
 * the generic "say" display loop and its "Bad Landing!" alternate (lines
 * 1505-1548), the stunt-name "asay" display loop (1528-1539), and the
 * "Checkpoint!"/"Car Fixed" say/tcnt triggers (1780-1793 and 1723-1727).
 * exitm (multiplayer exit-message state) is always 0 in this single-
 * player stub, so the JS's own `... && this.exitm === 0` gates are
 * simplified away. Call once per rendered frame, AFTER mad_drive() has
 * run for this frame (reads mad->missedcp/mtouch/clear/newcar/power) and
 * BEFORE draw_checkpoint_arrow() is expected to have already run (shares
 * xt->flk with it). The stunt DETECTION itself (building xt->loop/spin/
 * asay's text and decrementing auscnt when a trick lands) is physics-
 * tick-authoritative, not frame-authoritative -- see hud_stunt_detect()
 * below and its own call site in the tick loop for why.
 *
 * Split across TWO functions because the original splits them across two
 * differently-gated blocks. This first one is xtGraphics.java:7920-7955,
 * which sits inside `if (!this.holdit && this.fase != -6 && this.starcnt
 * == 0 && this.multion < 2 && checkPoints.stage != 10)` (:7917) -- the
 * SAME guard the checkpoint arrow one line above it (:7918) is under, so
 * the two are called together at one shared call site. hud_messages_tick()
 * below carries the rest, under the wider `if (!this.holdit)` of :8009.
 */
static void hud_wrongway_tick(Graphics2D *g, Medium *m, XtGraphicsStub *xt, Mad *mad) {
  font_set(FONT_BOLD, 11);   // the original's own font for every HUD message (Arial bold 11)
  if (xt->auscnt == 45 && mad->capcnt == 0) {
    if (mad->missedcp > 0) {
      if (mad->missedcp > 15 && mad->missedcp < 50) {
        if (xt->flk) hud_say_draw(g, m, 70, "Checkpoint Missed!", 255, 0, 0, 0);
        else hud_say_draw(g, m, 70, "Checkpoint Missed!", 255, 150, 0, 2);
      }
    } else if (mad->mtouch && xt->cntovn < 70) {
      if (abs(xt->ana) > 100) xt->cntan++;
      else if (xt->cntan != 0) xt->cntan--;
      if (xt->cntan > 40) {
        xt->cntovn++;
        xt->cntan = 40;
        if (xt->flk) { hud_say_draw(g, m, 70, "Wrong Way!", 255, 150, 0, 0); xt->flk = false; }
        else { hud_say_draw(g, m, 70, "Wrong Way!", 255, 0, 0, 2); xt->flk = true; }
      }
    }
  }
}

/**
 * The SECOND, separately-gated half (see hud_wrongway_tick() above for the
 * first): xtGraphics.java:8057-8153 -- the `looped` reset, the low-power
 * nag and its pwcnt escalation, the generic say/tcnt display loop, the
 * asay stunt-banner loop, and the "Bad Landing!" alternate -- followed by
 * this port's own tick-scoped say/tcnt TRIGGERS (Car Fixed / Checkpoint!).
 * Java wraps all of :8010-8403 in a single `if (!this.holdit)` (:8009), so
 * this whole function is suppressed while the win/lose hold card is up --
 * a wider gate than hud_wrongway_tick()'s, which additionally requires
 * starcnt==0 and stage!=10. Kept as two functions rather than one with
 * flags precisely because the two gates genuinely differ.
 */
static void hud_messages_tick(Graphics2D *g, Medium *m, XtGraphicsStub *xt, Mad *mad,
                              const CheckPoints *cp, int32_t nplayers, const int32_t *sc) {
  // The original's font for every one of these lines (Arial bold 11). Set
  // here: before, they took whatever the frame drew last -- the career
  // arrow's level label made them larger with the arrow on the cars.
  font_set(FONT_BOLD, 11);
  // 1463-1465 -- looped resets the instant a fresh trick attempt starts
  // (mad->loop reaching 2, the "armed" state -- see mad.c's own loop
  // state machine), so the "Please read the Game Instructions!" escalated
  // nag tier doesn't carry over into the next stunt attempt.
  if (xt->looped != 0 && mad->loop == 2) xt->looped = 0;

  // 1466-1504 -- low-power nag, gated on the say-display loop having just
  // gone idle this exact instant (tcnt===30/auscnt===45, i.e. no message
  // OR stunt banner currently showing) plus being grounded and undamaged.
  if (mad->power < 45.0f) {
    if (xt->tcnt == 30 && xt->auscnt == 45 && mad->mtouch && mad->capcnt == 0) {
      if (xt->looped != 2) {
        if (xt->pwcnt < 70 || (xt->pwcnt < 100 && xt->looped != 0)) {
          if (xt->pwflk) { hud_say_draw(g, m, 110, "Power low, perform stunt!", 0, 0, 200, 0); xt->pwflk = false; }
          else { hud_say_draw(g, m, 110, "Power low, perform stunt!", 255, 100, 0, 0); xt->pwflk = true; }
        }
      } else if (xt->pwcnt < 100) {
        // multion===0 always in this single-player stub, so the JS's own
        // s2 suffix (empty in multiplayer) always resolves to "(Press Enter)".
        if (xt->pwflk) { hud_say_draw(g, m, 110, "Please read the Game Instructions!  (Press " KEY_CONTINUE ")", 0, 0, 200, 0); xt->pwflk = false; }
        else { hud_say_draw(g, m, 110, "Please read the Game Instructions!  (Press " KEY_CONTINUE ")", 255, 100, 0, 0); xt->pwflk = true; }
      }
      xt->pwcnt++;
      if (xt->pwcnt == 300) {
        xt->pwcnt = 0;
        if (xt->looped != 0) {
          xt->looped++;
          if (xt->looped == 4) xt->looped = 2;
        }
      }
    }
  } else if (xt->pwcnt != 0) {
    xt->pwcnt = 0;
  }

  // Generic say/tcnt display loop runs BEFORE the trigger-setting code
  // below, matching the original's own line order (1505-1527 before
  // 1723-1793) -- a freshly-set message this tick displays starting NEXT
  // tick, not this one. Not worth reordering for a one-tick (~53ms)
  // difference that would only ever be visible on the exact tick a
  // message starts. The asay (stunt-name) display loop is a sibling of
  // this one, both inside the same `capcnt===0` branch (1528-1539).
  if (mad->capcnt == 0) {
    if (xt->tcnt < 30) {
      if (xt->tflk) {
        hud_say_draw(g, m, 105, xt->say, 0, 0, 0, 0);
        xt->tflk = false;
      } else {
        if (!xt->wasay) hud_say_draw(g, m, 105, xt->say, 0, 128, 255, 0);
        else hud_say_draw(g, m, 105, xt->say, 255, 128, 0, 0);
        xt->tflk = true;
      }
      xt->tcnt++;
    } else if (xt->wasay) {
      xt->wasay = false;
    }
    if (xt->auscnt < 45) {
      if (xt->aflk) { hud_say_draw(g, m, 85, xt->asay, 98, 176, 255, 0); xt->aflk = false; }
      else { hud_say_draw(g, m, 85, xt->asay, 0, 128, 255, 0); xt->aflk = true; }
      xt->auscnt++;
    }
  } else {
    // 1540-1548 -- "Bad Landing!" (mad->capcnt != 0, i.e. currently
    // capsized/crashed) replaces both the say and asay banners.
    if (xt->tflk) { hud_say_draw(g, m, 110, "Bad Landing!", 0, 0, 200, 0); xt->tflk = false; }
    else { hud_say_draw(g, m, 110, "Bad Landing!", 255, 100, 0, 0); xt->tflk = true; }
  }

  if (mad->newcar) {
    if (!xt->wasay) { strncpy(xt->say, "Car Fixed", sizeof(xt->say) - 1); xt->tcnt = 0; }
    // crash()'s own turn-rotation latch -- see xt_graphics.c's doc
    // comment on crashup, the crash-sound sibling of the stunt
    // announcer's skidup (Part 15), both toggled by this SAME trigger.
    xt->crashup = !xt->crashup;
  }
  // :8341-8356 -- a car just wasted: by another car (dested 1) or by you (2).
  for (int32_t n9 = 0; n9 < nplayers; n9++) {
    if (xt->hud_dested[n9] == cp->dested[n9] || n9 == xt->im) continue;
    xt->hud_dested[n9] = cp->dested[n9];
    const char *name = car_name(sc[n9]);
    if (xt->hud_dested[n9] == 1) snprintf(xt->say, sizeof(xt->say), "%s has been wasted!", name);
    if (xt->hud_dested[n9] == 2) snprintf(xt->say, sizeof(xt->say), "You wasted %s!", name);
    if (xt->hud_dested[n9] == 1 || xt->hud_dested[n9] == 2) {
      xt->wasay = true;
      xt->tcnt = -15;
    }
  }
  if (xt->hud_clear != mad->clear && mad->clear != 0) {
    if (!xt->wasay) { strncpy(xt->say, "Checkpoint!", sizeof(xt->say) - 1); xt->tcnt = 15; }
    xt->hud_clear = mad->clear;
    xt->cntovn = 0;
    if (xt->cntan != 0) xt->cntan = 0;
  }
}

// The game-space projection: game coordinates (left..right, top..bottom)
// across whatever viewport is current, y down, no depth. The normal one is
// (0, 800, 450, 0); a letterboxed menu drawn straight to the display uses
// (65, 735, 425, 25) so the original's 670x400 interior fills the screen.
static void set_game_projection(double left, double right, double bottom, double top) {
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(left, right, bottom, top, -1, 1);
  glMatrixMode(GL_MODELVIEW);
}

static void draw_hud_img(Graphics2D *g, HudImg img, int32_t x, int32_t y) {
  if (img.tex < 0) return;
  gfx_draw_image(g, img.tex, x, y, img.w, img.h);
}

/**
 * xtGraphics.pauseimage() (xtGraphics.java:9765-9806): turns the frame the
 * race was paused on into the pause menu's backdrop, `fleximg`. Each row is
 * greyscaled through a running average -- the first pixel of a row is its
 * own (r+g+b)/3, every later one (r+g+b + prev*30)/33, which smears the
 * picture sideways -- and the 237x188 panel under paused.gif at (281,8) is
 * tinted blue from that same grey. Transcribed loop for loop, including
 * the `i > 800*(8+n2)+281` test that starts each panel row at x=282.
 *
 * `rgba_bottom_up` is the 800x450 frame as glReadPixels returns it (row 0
 * at the bottom); `out` receives top-down RGBA ready to upload.
 */
static void pause_image(const uint8_t *rgba_bottom_up, uint8_t *out) {
  int32_t n = 0, n2 = 0, n3 = 0, n4 = 0;
  for (int32_t i = 0; i < 360000; i++) {
    int32_t x = i % 800, y = i / 800;
    const uint8_t *src = &rgba_bottom_up[((449 - y) * 800 + x) * 4];
    int32_t n5;
    if (n4 == 0) {
      n5 = n3 = (src[0] + src[1] + src[2]) / 3;
    } else {
      n5 = n3 = (src[0] + src[1] + src[2] + n3 * 30) / 33;
    }
    if (++n4 == 800) n4 = 0;
    uint8_t *dst = &out[i * 4];
    if (i > 800 * (8 + n2) + 281 && n2 < 188) {
      dst[0] = (uint8_t)((n5 + 60) / 3);
      dst[1] = (uint8_t)((n5 + 135) / 3);
      dst[2] = (uint8_t)((n5 + 220) / 3);
      if (++n == 237) {
        ++n2;
        n = 0;
      }
    } else {
      dst[0] = dst[1] = dst[2] = (uint8_t)n5;
    }
    dst[3] = 255;
  }
}

// The pause menu's look for the parts this port adds to it (the Settings
// row under paused.gif, and the Settings screen): paused.gif itself is an
// orange frame over a transparent interior, the blue being pauseimage()'s
// tint of the backdrop, so these plates use that tint's colour and that
// frame's orange.
static void draw_pause_plate(Graphics2D *g, int32_t x, int32_t y, int32_t w, int32_t h) {
  gfx_set_color(g, 86, 112, 140);
  gfx_fill_round_rect(g, x, y, w, h, 7, 20);
  gfx_set_color(g, 222, 132, 57);
  gfx_draw_round_rect(g, x, y, w, h, 7, 20);
}

// A selected row's highlight -- pausedgame()'s own colours and arcs.
static void draw_pause_highlight(Graphics2D *g, int32_t x, int32_t y, int32_t w, int32_t h) {
  gfx_set_color(g, 64, 143, 223);
  gfx_fill_round_rect(g, x, y, w, h, 7, 20);
  gfx_set_color(g, 0, 89, 223);
  gfx_draw_round_rect(g, x, y, w, h, 7, 20);
}

// Settings > Graphics > Smooth Frames -- ports web/main.js's INTERPOLATE.
// The race ticks at 18.9Hz; to move at the display's rate, the cars and the
// camera are captured before and after each tick, and every display frame
// draws them blended between the two, then puts the tick's values back.
// Nothing in the simulation ever sees a blended value.
typedef struct { int32_t x, y, z, xz, xy, zy; } SmoothPose;
typedef struct {
  SmoothPose car[NFM_MAX_CARS];
  int32_t x, y, z, xz, zy; // the camera
} SmoothSnap;

static void smooth_capture(SmoothSnap *s, const ContO *co, int32_t n, const Medium *m) {
  for (int32_t i = 0; i < n; i++) {
    s->car[i] = (SmoothPose){co[i].x, co[i].y, co[i].z, co[i].xz, co[i].xy, co[i].zy};
  }
  s->x = m->x; s->y = m->y; s->z = m->z; s->xz = m->xz; s->zy = m->zy;
}

/** Shortest-path angle blend, in degrees. */
static float smooth_angle(int32_t a, int32_t b, float t) {
  int32_t d = b - a;
  while (d > 180) d -= 360;
  while (d < -180) d += 360;
  return (float)a + (float)d * t;
}

static int32_t smooth_lerp(int32_t a, int32_t b, float t) {
  return (int32_t)lroundf((float)a + (float)(b - a) * t);
}

// Cars and camera both keep the blended angle's fraction (ContO.fxz/fxy/fzy,
// Medium.fxz/fzy), so a turn or a flip moves between ticks instead of in
// whole-degree steps.
static void smooth_apply(const SmoothSnap *a, const SmoothSnap *b, float t, ContO *co, int32_t n, Medium *m) {
  for (int32_t i = 0; i < n; i++) {
    const SmoothPose *p = &a->car[i], *c = &b->car[i];
    co[i].x = smooth_lerp(p->x, c->x, t);
    co[i].y = smooth_lerp(p->y, c->y, t);
    co[i].z = smooth_lerp(p->z, c->z, t);
    // Whole degrees in xz/xy/zy, the fraction in fxz/fxy/fzy for the draw.
    const float xz = smooth_angle(p->xz, c->xz, t), xy = smooth_angle(p->xy, c->xy, t),
                zy = smooth_angle(p->zy, c->zy, t);
    co[i].xz = (int32_t)floorf(xz); co[i].fxz = xz - (float)co[i].xz;
    co[i].xy = (int32_t)floorf(xy); co[i].fxy = xy - (float)co[i].xy;
    co[i].zy = (int32_t)floorf(zy); co[i].fzy = zy - (float)co[i].zy;
  }
  m->x = smooth_lerp(a->x, b->x, t);
  m->y = smooth_lerp(a->y, b->y, t);
  m->z = smooth_lerp(a->z, b->z, t);
  const float xz = smooth_angle(a->xz, b->xz, t), zy = smooth_angle(a->zy, b->zy, t);
  m->xz = (int32_t)floorf(xz); m->fxz = xz - (float)m->xz;
  m->zy = (int32_t)floorf(zy); m->fzy = zy - (float)m->zy;
}

static void smooth_restore(const SmoothSnap *s, ContO *co, int32_t n, Medium *m) {
  for (int32_t i = 0; i < n; i++) {
    const SmoothPose *c = &s->car[i];
    co[i].x = c->x; co[i].y = c->y; co[i].z = c->z;
    co[i].xz = c->xz; co[i].xy = c->xy; co[i].zy = c->zy;
    co[i].fxz = co[i].fxy = co[i].fzy = 0.0f;
  }
  m->x = s->x; m->y = s->y; m->z = s->z; m->xz = s->xz; m->zy = s->zy;
  m->fxz = m->fzy = 0.0f;
}

/**
 * The scene draw both replay fases share with racing: medium_d() for the
 * sky/ground, then every ContO painted back-to-front. `dist == 0` objects
 * are drawn immediately (the source's own "not depth-sorted" class), the
 * rest ranked by descending dist with ties broken by ascending index.
 *
 * Factored out because three states now need exactly this -- fase -3's
 * highlight reel, fase -1's pause replay, and the frozen backdrop the
 * pause menu itself sits on -- and the ranking loop is fiddly enough that
 * three hand-copies would be three chances to diverge.
 */
static void draw_race_scene(Graphics2D *g, Medium *m, ContO **all_objs, int32_t total_objs,
                            int32_t *visible_idx, int32_t *rank, int32_t *order) {
  nfm_set_draw_phase(true);
  medium_d(m, g);
  int32_t nvis = 0;
  for (int32_t i = 0; i < total_objs; i++) {
    if (all_objs[i]->dist != 0) visible_idx[nvis++] = i;
    else cont_o_d(all_objs[i], g);
  }
  for (int32_t i = 0; i < nvis; i++) rank[i] = 0;
  for (int32_t i = 0; i < nvis; i++) {
    for (int32_t j = i + 1; j < nvis; j++) {
      int32_t di = all_objs[visible_idx[i]]->dist;
      int32_t dj = all_objs[visible_idx[j]]->dist;
      if (di != dj) {
        if (di < dj) rank[i]++;
        else rank[j]++;
      } else {
        rank[i]++;
      }
    }
  }
  for (int32_t i = 0; i < nvis; i++) order[rank[i]] = i;
  for (int32_t i = 0; i < nvis; i++) cont_o_d(all_objs[visible_idx[order[i]]], g);
  nfm_set_draw_phase(false);
}

/**
 * Ports the single-player half of xtGraphics.java's arrace leaderboard
 * panel (:3688-3922) -- the right-hand column the A key brings up, one
 * row per finishing position, showing that position's ordinal and a live
 * damage bar for whichever car currently holds it.
 *
 * The outer loop walks positions 0..nplayers-1; the inner one finds the
 * first car reported at that position which is still alive, draws its
 * row, and latches `found` (the source's own n31) so a second car can
 * never double up on one row.
 *
 * Deliberately omitted, all structurally unreachable here rather than
 * skipped for convenience: the `multion >= 2` spectator-follow block
 * (:3693-3725), every `clangame != 0` recolour arm (no clan mode), the
 * mouse hover/click lock (:3823-3842 and :3869-3921 -- this port has no
 * mouse), and the `alocked == n32` highlight rects (:3843-3868), which
 * only that mouse path could ever satisfy.
 *
 * The name above each bar (:3785) is `plnames[]`, which only the multiplayer
 * lobby fills -- blank in the original's single player. Settings > Interface
 * > Names in Standings (`names`) fills that slot with the car's name, drawn
 * exactly as the multiplayer one is: black, centred on x=730.
 */
static void draw_arrace_board(Graphics2D *g, Medium *m, CheckPoints *cp, int32_t nplayers,
                              const int32_t *sc, bool names) {
  int32_t label_b = hud_tint(100.0, m->snap[2]);
  const int32_t rows = nplayers < 12 ? nplayers : 12;   // 76 + 30 * 11 is the last row that fits
  for (int32_t place = 0; place < rows; place++) {
    int32_t found = 0;
    for (int32_t j = 0; j < nplayers && found == 0; j++) {
      if (cp->pos[j] != place || cp->dested[j] != 0) continue;
      if (g_board_skip && g_board_skip[j]) continue;
      // :3730-3749 -- ordinal. "1st" sits one pixel right of the rest.
      gfx_set_color(g, 0, 0, label_b);
      // 16, not 8: `place` is a race position and never exceeds the 7-car
      // field, so "%dth" below is at most 4 chars, but gcc reasons from
      // int's full range (up to "-2147483648th") and warns. snprintf
      // would have truncated safely either way -- this just sizes the
      // buffer so the bound is provable and the warning goes.
      char ord[16];
      if (place == 0) snprintf(ord, sizeof(ord), "1st");
      else if (place == 1) snprintf(ord, sizeof(ord), "2nd");
      else if (place == 2) snprintf(ord, sizeof(ord), "3rd");
      else snprintf(ord, sizeof(ord), "%dth", place + 1);
      font_draw(g, ord, (place == 0) ? 673 : 671, 76 + 30 * place);
      if (names) {
        const char *name = car_name(sc[j]);
        hud_set_ink(g, 0, 0, 0);
        // Centred like Java's, but never left of the bar: the longest stock
        // name would otherwise run into the ordinal beside it.
        int32_t nx = 730 - font_width(name) / 2;
        if (nx < 700) nx = 700;
        font_draw(g, name, nx, 70 + 30 * place);
      }

      // :3790-3821 -- damage bar. Full width is 60px; the fill tracks
      // magperc and its green channel ramps 244 -> 11 once the bar passes
      // a third, turning it from yellow through orange to red.
      int32_t len = jtrunc(60.0f * cp->magperc[j]);
      int32_t bg = 244;
      if (len > 20) bg = jtrunc(244.0f - 233.0f * ((float)(len - 20) / 40.0f));
      gfx_set_color(g, hud_tint(244.0, m->snap[0]), hud_tint((double)bg, m->snap[1]),
                    hud_tint(11.0, m->snap[2]));
      gfx_fill_rect(g, 700, 74 + 30 * place, len, 5);
      gfx_set_color(g, 0, 0, 0);
      gfx_draw_rect(g, 700, 74 + 30 * place, 60, 5);
      found = 1;
    }
  }
}

/**
 * Ports xtGraphics.java's radarstat(mad, contO, checkPoints) (:8903-9079) --
 * the top-left map overlay the S key toggles: a translucent sky-coloured
 * plate, the track drawn as one line per consecutive checkpoint pair, the
 * rival blips (only while arrace is on), the player's own centre cross,
 * and THEN the speedometer.
 *
 * That last part is not a stray: `this.sped` is drawn in exactly ONE place
 * in the whole original, line 9067, right here inside radarstat(). The
 * base HUD block at :7994-8007 draws dmg/pwr/lap/was/pos/rank and calls
 * drawstat(), but no speedo -- so in the real game the speed readout is
 * part of the radar overlay and is INVISIBLE until the player presses S.
 * This port used to draw it unconditionally every frame, which is why it
 * moved in here rather than staying up in the always-on HUD.
 *
 * Everything the `clangame != 0` arms recolour is skipped: clangame is
 * structurally 0 in this single-player port (no clan mode), so those
 * branches can never be taken and their `pclan`/`gaclan` lookups have no
 * counterpart here. Likewise `multion > 1` in the :8928 blip gate, leaving
 * plain `arrace`.
 *
 * The map is drawn in a fixed 172x172 box whose centre (96, 141) is the
 * player; everything else is placed relative to the player's own opx/opz
 * and scaled by checkPoints.prox (check_points_calprox's stage-bounds
 * divisor), then counter-rotated by mad->cxz so the map turns with the
 * car rather than staying north-up.
 */
static void radar_stat(Graphics2D *g, Medium *m, XtGraphicsStub *xt, Mad *mad,
                       ContO *co, CheckPoints *cp, bool arrace, int32_t nplayers,
                       HudImg sped_img) {
  // :8904-8906 -- half-alpha sky-coloured backing. Java uses
  // fillRoundRect(10,55,172,172,30,30); gfx.h has no rounded-rect
  // primitive, so this is the same square-cornered approximation the
  // hold card's own backing plate already makes (see its comment).
  gfx_set_composite(g, 0.5f);
  gfx_set_color(g, m->csky[0], m->csky[1], m->csky[2]);
  gfx_fill_rect(g, 10, 55, 172, 172);
  gfx_set_composite(g, 1.0f);

  // :8909-8927 -- the track outline. Each iteration joins checkpoint i to
  // its successor, wrapping the last one back to 0; a successor of type
  // -3 (the original's own end-of-circuit marker) closes the loop to 0 and
  // then breaks, so an open course does not draw a spurious closing leg.
  gfx_set_color(g, m->csky[0] / 2, m->csky[1] / 2, m->csky[2] / 2);
  for (int32_t i = 0; i < cp->n; i++) {
    int32_t nx = i + 1;
    if (i == cp->n - 1) nx = 0;
    bool last = false;
    if (cp->typ[nx] == -3) { nx = 0; last = true; }
    int32_t rx[2], ry[2];
    rx[0] = jtrunc(96.0f - (float)(cp->opx[0] - cp->x[i]) / cp->prox);
    rx[1] = jtrunc(96.0f - (float)(cp->opx[0] - cp->x[nx]) / cp->prox);
    ry[0] = jtrunc(141.0f - (float)(cp->z[i] - cp->opz[0]) / cp->prox);
    ry[1] = jtrunc(141.0f - (float)(cp->z[nx] - cp->opz[0]) / cp->prox);
    medium_rot(m, rx, ry, 96, 141, mad->cxz, 2);
    gfx_draw_line(g, rx[0], ry[0], rx[1], ry[1]);
    if (last) break;
  }

  // :8928-9003 -- rival blips, drawn ONLY when the arrow is in car-hunting
  // mode. Each live rival gets a small cross plus a 3x3 dot; the cross
  // grows by a pixel and drops the sky-blend when that car is the manually
  // locked one (never true here -- see xt_graphics.h on alocked).
  if (arrace) {
    int32_t bx[NFM_MAX_CARS], by[NFM_MAX_CARS];
    for (int32_t j = 0; j < nplayers; j++) {
      bx[j] = jtrunc(96.0f - (float)(cp->opx[0] - cp->opx[j]) / cp->prox);
      by[j] = jtrunc(141.0f - (float)(cp->opz[j] - cp->opz[0]) / cp->prox);
    }
    medium_rot(m, bx, by, 96, 141, mad->cxz, nplayers);
    int32_t br = 0;
    int32_t bg = hud_tint(80.0, m->snap[1]);
    int32_t bb = hud_tint(159.0, m->snap[2]);
    for (int32_t k = 0; k < nplayers; k++) {
      if (k == 0 || cp->dested[k] != 0) continue;
      int32_t arm = 2;
      if (xt->alocked == k) {
        arm = 3;
        gfx_set_color(g, br, bg, bb);
      } else {
        gfx_set_color(g, (br + m->csky[0]) / 2, (m->csky[1] + bg) / 2, (bb + m->csky[2]) / 2);
      }
      gfx_draw_line(g, bx[k] - arm, by[k], bx[k] + arm, by[k]);
      gfx_draw_line(g, bx[k], by[k] + arm, bx[k], by[k] - arm);
      gfx_set_color(g, br, bg, bb);
      gfx_fill_rect(g, bx[k] - 1, by[k] - 1, 3, 3);
    }
  }

  // :9004-9053 -- the player's own marker, permanently at the centre. Red
  // (159-ish after the snap tint) with green and blue left at 0, since the
  // only thing that would change them is the clangame arm.
  int32_t pr = hud_tint(159.0, m->snap[0]);
  int32_t pg = 0, pb = 0;
  gfx_set_color(g, (pr + m->csky[0]) / 2, (m->csky[1] + pg) / 2, (pb + m->csky[2]) / 2);
  gfx_draw_line(g, 96, 139, 96, 143);
  gfx_draw_line(g, 94, 141, 98, 141);
  gfx_set_color(g, pr, pg, pb);
  gfx_fill_rect(g, 95, 140, 3, 3);

  // :9055-9066 -- the speedo's OWN dark-sky backing plate, a fifth one
  // beyond the four the always-on HUD panels get (see this file's own
  // darksky block up in the HUD draw). Measured unreachable with the
  // shipped stages, same as its siblings; kept for the same reason.
  if (m->darksky && kJavaDarkSkyBoxes) {
    float hsb[3];
    rgb_to_hsb(m->csky[0], m->csky[1], m->csky[2], hsb);
    hsb[2] = 0.6f;
    int32_t rgb = hsb_to_rgb(hsb[0], hsb[1], hsb[2]);
    gfx_set_color(g, (rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff);
    gfx_fill_rect(g, 5, 232, 181, 17);
    gfx_draw_line(g, 4, 233, 4, 247);
    gfx_draw_line(g, 3, 235, 3, 245);
    gfx_draw_line(g, 186, 233, 186, 247);
    gfx_draw_line(g, 187, 235, 187, 245);
  }

  // :9067-9078 -- the speedometer itself. n13 (the vertical delta) is
  // computed and then genuinely never used by the original's own formula,
  // only X and Z feed the speed; preserved as a real quirk rather than
  // trimmed, exactly as the previous always-on copy of this block did.
  draw_hud_img(g, sped_img, 7, 234);
  int32_t n12 = co->x - xt->lcarx;
  xt->lcarx = co->x;
  int32_t n13 = co->y - xt->lcary;
  xt->lcary = co->y;
  (void)n13;
  int32_t n14 = co->z - xt->lcarz;
  xt->lcarz = co->z;
  int32_t sq = n12 * n12 + n14 * n14;
  double step = sqrt((double)sq) * 1.4;
  float a = (float)step;
  float b_ = a * 21.0f;
  float c = b_ * 60.0f;
  float d = c * 60.0f;
  float n15 = d / 100000.0f;
  float n16 = n15 * 0.621371f;
  char buf[32];
  hud_set_ink(g, 0, 0, 100);
  snprintf(buf, sizeof(buf), "%d", jtrunc(n15));
  font_draw(g, buf, 62, 245);
  snprintf(buf, sizeof(buf), "%d", jtrunc(n16));
  font_draw(g, buf, 132, 245);

}

/**
 * Ports web/preview.js's drawCar() -- the real 3D spinning car the car-
 * select screen shows in the original (both the actual Java applet's
 * in-game screen, per preview.js's own comment citing `xtGraphics.
 * java:5058`'s camera constants, and the JS port's separate launcher
 * page, which copies those same constants exactly). NOT a static image:
 * `car` is drawn through the SAME cont_o_d() the racing scene itself
 * uses, spinning in place, matching the original instead of a flat
 * placeholder or a pre-rendered thumbnail.
 *
 * Mutates `m`'s camera fields and `car`'s position/rotation/shadow in
 * place -- safe because (a) this only ever runs during the menu, never
 * concurrently with racing, and (b) cont_o_init_copy() (used once the
 * player actually starts a race) re-derives x/y/z/xz/xy/zy from its own
 * explicit parameters and never inherits them from the base model, so
 * spin state left on `car` here can't leak into the driven car -- see
 * cont_o.c's own cont_o_init_copy for why.
 *
 * `y_offset`/`zy_value`/`freeze_spin` implement xtGraphics.java:6335-6425's
 * car-switch transition: while `flipo != 0` the CALLER (the CAR_SELECT
 * draw block) drives `y_offset` (the car falling away/rising back in) and
 * `zy_value` (its tumble as it falls) directly instead of the idle
 * resting height/zy=0, and freezes the idle xz (yaw)/wzy (wheel) spin --
 * both only advance inside Java's own `flipo == 0` branch, never during
 * the transition.
 */
static void draw_car_preview(Graphics2D *g, Medium *m, ContO *car,
                              int32_t *xz_state, int32_t *wzy_state,
                              int32_t y_offset, int32_t zy_value, bool freeze_spin) {
  m->trk = 0;
  m->crs = true;
  // Camera from the car-SELECT screen (xtGraphics.java:5058) -- looks
  // slightly down at the car, distinct from the car-MAKER preview's level
  // camera (java:6806), which shows the underside and reads badly here.
  m->x = -400;
  m->y = -525;
  m->z = -50;
  m->xz = 0;
  m->zy = 10;
  m->ground = 495;
  m->ih = 0;
  m->iw = 0;
  m->w = 800;
  m->h = 450;
  m->focus_point = 400;
  m->cx = 400;
  m->cy = 225;
  m->cz = 50;

  // Car placement -- from Java's own carselect() lines 5273-5297, NOT
  // preview.js's launcher-page version. Two real differences:
  //   - z=950 (Java) vs z=1000 (preview.js). The launcher page moved it
  //     ~50u back because its viewport was cropped tighter; the in-game
  //     screen uses z=950 to place the car at the intended screen scale.
  //     drmonster (car index 13) is an exception -- Java sets z=1000
  //     for it specifically, since it's larger and would clip at z=950.
  //   - y=-34-grat (Java) vs y=0 (preview.js). Same reason: sits the
  //     car on the intended y-baseline given the -525 camera y.
  //     `grat` is the car's ground-attachment offset (already computed
  //     during ContO init) so this seats the wheels on the invisible
  //     y=-34 plane.
  car->x = 0;
  car->z = (car->baseIndex == 13) ? 1000 : 950;
  car->y = -34 - (int32_t)car->grat + y_offset;
  if (!freeze_spin) *xz_state = (*xz_state + 5) % 360;
  car->xz = *xz_state;
  car->zy = zy_value;
  car->xy = 0;
  // :5292-5297 -- the wheel spin wraps at -30, NOT at -360. Java does
  // `wzy -= 10; if (wzy < -30) wzy += 30;`, which after the first few
  // frames settles into a tight 3-value cycle (-10, -20, -30, -10, ...)
  // rather than sweeping the whole circle. This port used `% 360`, which
  // walks all 36 angles instead and reads as a slow full rotation where
  // the original shows a fast, shallow flicker. (The post-race unlock
  // celebration's own copy of this animation genuinely has NO wrap at all
  // -- :6818-6819 -- and `% 360` IS equivalent there, since the angle is
  // only ever consumed modulo a full turn; that call site is left alone.)
  if (!freeze_spin) {
    *wzy_state -= 10;
    if (*wzy_state < -30) *wzy_state += 30;
  }
  car->wzy = *wzy_state;

  // No shadow -- floating rather than seated on a track, a shadow would
  // land across the model instead of beneath it (see preview.js's own
  // comment). Restored afterward since `car` is a shared base model/
  // template, not a disposable per-draw copy.
  bool had_shadow = car->shadow;
  car->shadow = false;
  nfm_set_draw_phase(true); // cont_o_d may draw dust/sparks -- see main()'s racing-draw comment
  cont_o_d(car, g);
  nfm_set_draw_phase(false);
  car->shadow = had_shadow;
}

/** A car side on, scaled to about 140 px long, centred on screen (sx, sy):
 * the Race Setup's rival list (its rows, lobby style). The camera fields it
 * sets are draw_car_preview's, moved: the projection centre is (sx, sy). */
static void draw_car_thumb(Graphics2D *g, Medium *m, ContO *car, int32_t sx, int32_t sy) {
  m->trk = 0;
  m->crs = true;
  m->x = -sx;
  m->y = -sy;
  m->z = -50;
  m->xz = 0;
  m->zy = 0;
  m->ground = 495;
  m->ih = 0;
  m->iw = 0;
  m->w = 800;
  m->h = 450;
  m->focus_point = 400;
  m->cx = sx;
  m->cy = sy;
  m->cz = 50;
  const int32_t r = car->maxR > 0 ? car->maxR : 300;
  car->x = 0;
  car->y = -(int32_t)car->grat / 2;
  car->z = r * 400 / 70;   // its radius projects to ~70 px
  car->xz = -90;   // facing right, as the lobby shows them
  car->zy = 0;
  car->xy = 0;
  car->wzy = 0;
  const bool had_shadow = car->shadow;
  car->shadow = false;
  nfm_set_draw_phase(true);
  cont_o_d(car, g);
  nfm_set_draw_phase(false);
  car->shadow = had_shadow;
}

/** The career's stat bars (XT carselect 16573-16716): the car's stats with
 * its points in, numbered, each bar's colour stepping every 100 (green,
 * red, teal, ...), top speed in MPH. Laid out for this screen: two columns
 * of four rows from y0, under the stat panel. */
static void draw_ext_stat_bars(Graphics2D *g, const CarDefine *cd, int32_t cn, const int32_t sp[CS_N], int32_t y0) {
  float e[7];
  const float sw2 = (float)cd->swits[cn][2] + (float)sp[CS_TS];
  e[1] = (sw2 - 220.0f) / 90.0f;
  const float a0 = cd->acelf[cn][0] + (float)sp[CS_ACC] * 0.1f;
  const float a1 = cd->acelf[cn][1] * a0 / cd->acelf[cn][0] - 3.0f;
  const float a2 = cd->acelf[cn][2] * a0 / cd->acelf[cn][0] - 2.0f;
  e[2] = ((a0 - 6.0f) * 21.0f + a1 * 6.0f + a2 * 3.0f) / 201.0f;
  e[3] = (cd->grip[cn] + (float)sp[CS_GRIP] * 0.2f - 10.0f) / 20.0f;
  e[4] = ((float)cd->airc[cn] + (float)sp[CS_STU] + (cd->airs[cn] + (float)sp[CS_STU] * 0.025f) * 10.0f) / 125.0f;
  e[5] = (cd->moment[cn] + (float)sp[CS_STR] * 0.025f) / 2.1f;
  e[6] = cd->outdam[cn] + (float)sp[CS_END] * 0.01f;
  e[0] = (e[1] + e[2] + e[3] + e[4] + e[5] + e[6]) / 6.0f;
  static const char *const kLabels[7] = {"OVERALL:", "TOP SPEED:", "ACCELERATION:", "CONTROL:",
                                         "STUNTING:", "STRENGTH:", "DEFENCE:"};
  static const uint8_t kCol[8][3] = {{0, 130, 0},   {130, 0, 0},  {0, 95, 95},   {130, 0, 130},
                                     {70, 70, 0},   {0, 0, 130},  {140, 80, 140}, {80, 80, 80}};
  const int32_t bw = 140, bh = 12, step = 14;
  for (int32_t i = 0; i < 7; i++) {
    // Left: overall, top speed, acceleration, control; right (rows 2-4):
    // stunting, strength, defence.
    const bool left = i < 4;
    const int32_t row = left ? i : i - 3;
    const int32_t lx = left ? 175 : 575, bx = lx + 8, by = y0 + row * step;
    int32_t fbar = (int32_t)(e[i] * (float)bw);
    if (i == 1 && fbar < 15 * bw / 163) fbar = 15 * bw / 163;
    if (fbar < 0) fbar = 0;
    int32_t code = fbar / (bw + 1), fill = fbar;
    if (code >= 7) { code = 7; fill = bw; }
    else fill -= code * bw;
    font_set(FONT_BOLD, 11);
    gfx_set_color(g, 235, 235, 235);
    font_draw(g, kLabels[i], lx - font_width(kLabels[i]), by + 10);
    gfx_set_color(g, 0, 0, 0);
    gfx_fill_rect(g, bx, by, bw, bh);
    gfx_set_color(g, kCol[code][0], kCol[code][1], kCol[code][2]);
    gfx_fill_rect(g, bx, by, fill, bh);
    gfx_set_color(g, 245, 245, 245);
    gfx_draw_rect(g, bx - 1, by - 1, bw + 1, bh + 1);
    char v[24];
    if (i == 1) snprintf(v, sizeof(v), "%d MPH", (int)(sw2 / 2.0f));
    else snprintf(v, sizeof(v), "%d", (int)(e[i] * 100.0f));
    gfx_set_color(g, 252, 206, 0);
    font_draw(g, v, bx + (bw - font_width(v)) / 2, by + 10);
  }
}

// draw_stage_preview (overhead 3D stage render) removed in Part 6 --
// the Java stage-select screen doesn't have a 3D preview, and its
// br.png torn-paper backdrop is the intended empty visual there.
// The code lived here through Parts 4-5 as an intermediate; kept in
// git history for the launcher-page look if it's ever wanted again.

/**
 * Ports xtGraphics.java:1734's mainbg(1), the main-menu scrolling
 * backdrop -- constant orange (255,176,67) fill on the central content
 * area (65,25 .. 735,425 == the 670x400 letterbox interior), with
 * bgmain.jpg (also 670x400) tiled vertically in two slots that scroll
 * down 8px/frame and wrap seamlessly. Java's `bgmy[0]=0, bgmy[1]=-400`
 * initial offsets create the seamless tile: at t=0, tile 0 covers
 * y=25..425 (fully on-screen) and tile 1 covers y=-375..25 (fully
 * off-screen above). Each frame both offsets ADVANCE by +8 (moving the
 * tiles visually downward) and wrap back to -400 once they reach +400 --
 * so one tile is always in view.
 *
 * `n2 == 8` picks the scroll speed for main-menu mode (mode 1 in the
 * Java's own dispatch). Other modes use different colors/speeds -- we
 * only need mode 1 for now.
 *
 * `bgmy_ptr` is caller-owned persistent state (2 ints), so the scroll
 * continues frame-to-frame -- this function is stateless otherwise.
 * The 65px black letterbox borders (drawn last so the tiles never bleed
 * into them) match Java xtGraphics.java:1886-1891 exactly.
 */
// The shared body of every scrolling-tile mainbg mode: a flat base fill
// in this mode's own colour, the two wrapping bgmain tiles at this mode's
// own speed, then the black letterbox. Modes differ ONLY in those two
// values (Java xtGraphics.java:1734-1891 sets `n2` and the fill colour
// per mode and then falls through to one shared tail), so they share one
// implementation here rather than a copy per mode.
static void draw_mainbg_tiles(Graphics2D *g, HudImg bgmain, int32_t *bgmy_ptr,
                               int32_t fr, int32_t fg, int32_t fb, int32_t speed) {
  const int32_t height = 450;
  gfx_set_color(g, fr, fg, fb);
  gfx_fill_rect(g, 65, 25, 670, 400);

  // The two scrolling tiles. If bgmain never loaded (tex < 0) we skip
  // the tiles and the flat base colour stays -- graceful fallback,
  // matching how the HUD panel loaders behave.
  if (bgmain.tex >= 0) {
    for (int32_t k = 0; k < 2; k++) {
      gfx_draw_image(g, bgmain.tex, 65, 25 + bgmy_ptr[k], bgmain.w, bgmain.h);
      bgmy_ptr[k] += speed;
      if (bgmy_ptr[k] >= 400) bgmy_ptr[k] = -400;
    }
  }

  // Black letterbox borders. These MUST paint over any tile bleed at
  // the seams (they don't quite in practice because bgmy wraps at 400
  // exactly matching the 400px tile height, but the Java always draws
  // them defensively -- and we do the same, cheap and correct).
  gfx_set_color(g, 0, 0, 0);
  gfx_fill_rect(g, 0, 0, 65, height);
  gfx_fill_rect(g, 735, 0, 65, height);
  gfx_fill_rect(g, 65, 0, 670, 25);
  gfx_fill_rect(g, 65, 425, 670, 25);
}

// mainbg(1) -- the MAIN MENU / GAMEMODE submenu backdrop: bright orange
// (255,176,67) at the fast n2=8 scroll. xtGraphics.java:1776-1784.
static void draw_mainbg_1(Graphics2D *g, HudImg bgmain, int32_t *bgmy_ptr) {
  draw_mainbg_tiles(g, bgmain, bgmy_ptr, 255, 176, 67, 8);
}

// mainbg(-1) -- the CREDITS backdrop: a distinctly GREEN (144,222,9) base
// at the same n2=8 scroll. xtGraphics.java:1737-1747. This port used to
// reuse mode 1 here and call the difference "a subtle background hue";
// orange vs green is not subtle, so credits now gets its real colour.
// (Java's mode -1 also resets bgup/bgf, which only matter to the modes
// that animate their own fill -- this one does not.)
static void draw_mainbg_neg1(Graphics2D *g, HudImg bgmain, int32_t *bgmy_ptr) {
  draw_mainbg_tiles(g, bgmain, bgmy_ptr, 144, 222, 9, 8);
}

/**
 * Ports xtGraphics.java:1785-1891's mainbg(2) -- the INSTRUCTIONS screen's
 * own backdrop mode. Differs from mainbg(1) (draw_mainbg_1 above) in
 * three ways, all of them visible: the base fill is a muted beige
 * (188,170,122) rather than bright orange; the tiles scroll at n2=2
 * instead of 8 (a much slower drift); and on the FINAL page (Java's
 * `flipo == 16`) the bgmain tiles stop being drawn entirely while the
 * base fill lerps from beige toward a pale blue (176,202,255) as `bgf`
 * ramps 0.2 -> 0.85 at +0.025/frame -- the "wrap-up" fade the original
 * plays under its closing recap page. Every other page pins bgf back to
 * 0.2, so re-entering page 16 always restarts that fade from the start.
 *
 * `bgmy_ptr` (2 ints) and `bgf_ptr` are caller-owned persistent state,
 * same convention as draw_mainbg_1. Java's own `lmode` bookkeeping (which
 * resets bgmy/bgf when the background MODE changes) is folded into the
 * caller here: game.c resets them on entering the Instructions state,
 * which is the only transition that can change mode into this one.
 */
static void draw_mainbg_2(Graphics2D *g, HudImg bgmain, int32_t *bgmy_ptr,
                           float *bgf_ptr, int32_t flipo) {
  int32_t r = 188, gg = 170, b = 122;
  if (flipo == 16) {
    // :1793-1799 -- lerp beige -> pale blue as bgf climbs.
    r  = (int32_t)(176.0f * (*bgf_ptr) + 191.0f * (1.0f - *bgf_ptr));
    gg = (int32_t)(202.0f * (*bgf_ptr) + 184.0f * (1.0f - *bgf_ptr));
    b  = (int32_t)(255.0f * (*bgf_ptr) + 124.0f * (1.0f - *bgf_ptr));
    *bgf_ptr += 0.025f;
    if (*bgf_ptr > 0.85f) *bgf_ptr = 0.85f;
  } else {
    *bgf_ptr = 0.2f; // :1801 -- every non-final page re-arms the fade
  }
  gfx_set_color(g, r, gg, b);
  gfx_fill_rect(g, 65, 25, 670, 400);

  // :1873-1883 -- the tiles are SKIPPED on page 16 (that's what makes the
  // colour fade above visible at all), but bgmy still advances either way.
  for (int32_t k = 0; k < 2; k++) {
    if (bgmain.tex >= 0 && flipo != 16) {
      gfx_draw_image(g, bgmain.tex, 65, 25 + bgmy_ptr[k], bgmain.w, bgmain.h);
    }
    bgmy_ptr[k] += 2; // n2 == 2 for this mode
    if (bgmy_ptr[k] >= 400) bgmy_ptr[k] = -400;
  }

  gfx_set_color(g, 0, 0, 0);
  gfx_fill_rect(g, 0, 0, 65, 450);
  gfx_fill_rect(g, 735, 0, 65, 450);
  gfx_fill_rect(g, 65, 0, 670, 25);
  gfx_fill_rect(g, 65, 425, 670, 25);
}

/**
 * Ports xtGraphics.java:4309's maini() -- the MAIN MENU (fase 10), the
 * screen the game boots into after the Radicalplay splash. See
 * native/docs/MENU_FLOW.md §3.2 for the pixel-exact spec.
 *
 * Deliberately hides the Multiplayer option (user decision -- no
 * netplay in this port). Java's original 4 options at y=261/291/321/351
 * become our 3 at 261/291/321 -- option index 1 (Play Multiplayer) is
 * dropped and the two rows BELOW it slide up by 30 each, so the layout
 * closes up instead of leaving a gap where Multiplayer used to sit.
 * options.png is cropped to only draw the 3 rows we keep:
 * Java's options.png is 211x105 with 4 evenly-spaced rows of ~26px, so
 * we do 3 sub-image draws (row 0, row 2, row 3) at the visible y's.
 *
 * All animation state (bgmy, flkat, gxdu, gydu, movly, aflk) is
 * caller-owned so this function is stateless -- callers pass in
 * pointers to persistent ints they hold across frames.
 */
/**
 * The shared background pipeline that both the main menu (Java maini(),
 * fase 10) and the gamemode submenu (Java maini2(), fase 102) draw
 * IDENTICALLY before their own option-specific overlays. Both Java
 * methods run these steps in the same order with the exact same
 * arguments (see xtGraphics.java:4314-4345 vs :4505-4536 -- textually
 * the same 30 lines). Factored out here so the two menu screens can
 * share their bg without duplicating the ~50 lines below.
 *
 * All animation state (bgmy, flkat, gxdu, gydu, movly) is caller-owned
 * -- the two menus SHARE the SAME persistent state variables in main(),
 * matching Java (both methods mutate `this.flkat`/`this.bgmy[]` on the
 * same `this`), so switching between fase 10 and 102 doesn't restart
 * the animations.
 */
static void draw_menu_common_bg(Graphics2D *g,
                                 HudImg bgmain, HudImg logomadbg, HudImg logomadnes,
                                 HudImg dude, HudImg logocars, HudImg opback,
                                 int32_t *bgmy_ptr, int32_t *flkat_ptr,
                                 int32_t *gxdu_ptr, int32_t *gydu_ptr, int32_t *movly_ptr) {
  // 1. Background scrolling tiles + orange fill + letterbox.
  draw_mainbg_1(g, bgmain, bgmy_ptr);

  // 2. logomadbg watermark at 60% alpha. Java line 4315-4317.
  if (logomadbg.tex >= 0) {
    gfx_set_composite(g, 0.6f);
    gfx_draw_image(g, logomadbg.tex, 65, 25, logomadbg.w, logomadbg.h);
    gfx_set_composite(g, 1.0f);
  }

  // 3. logomadnes ("MADNESS" gold wordmark) at (233, 186). Java 4318.
  if (logomadnes.tex >= 0) {
    gfx_draw_image(g, logomadnes.tex, 233, 186, logomadnes.w, logomadnes.h);
  }

  // 4. Face-blink animation for dude[0] at (351+gxdu, 28+gydu). Java 4319-4342.
  // Alpha curve:
  //   flkat 0..200:   alpha = flkat/800  (max at 200 = 0.25, capped to 0.2)
  //   flkat 200..400: alpha = (400-flkat)/1000 (fades back to 0)
  //   flkat wraps to 0 at 400.
  // Jitter (gxdu, gydu): reset every 2 frames. Java is
  // `(int)(5.0 - 11.0 * Math.random())` -- Math.random() is [0,1), so the
  // expression spans (-6, 5] and Java's cast truncates TOWARD ZERO, so
  // the real range is -5..+5, not -6..+4.
  float alpha = (float)(*flkat_ptr) / 800.0f;
  if (alpha > 0.2f) alpha = 0.2f;
  if (*flkat_ptr > 200) {
    alpha = (float)(400 - *flkat_ptr) / 1000.0f;
    if (alpha < 0.0f) alpha = 0.0f;
  }
  (*flkat_ptr)++;
  if (*flkat_ptr == 400) *flkat_ptr = 0;
  if (dude.tex >= 0) {
    gfx_set_composite(g, alpha);
    gfx_draw_image(g, dude.tex, 351 + *gxdu_ptr, 28 + *gydu_ptr, dude.w, dude.h);
    gfx_set_composite(g, 1.0f);
  }
  if (*movly_ptr == 0) {
    *gxdu_ptr = (int32_t)(5.0 - 11.0 * ((double)nfm_random() / (double)0xffffffffu));
    *gydu_ptr = (int32_t)(5.0 - 11.0 * ((double)nfm_random() / (double)0xffffffffu));
  }
  (*movly_ptr)++;
  if (*movly_ptr == 2) *movly_ptr = 0;

  // 5. logocars ("CARS" logo strip) at (66, 33). Java 4344.
  if (logocars.tex >= 0) {
    gfx_draw_image(g, logocars.tex, 66, 33, logocars.w, logocars.h);
  }

  // 6. opback (brown pill) at (247, 237). Java 4345.
  if (opback.tex >= 0) {
    gfx_draw_image(g, opback.tex, 247, 237, opback.w, opback.h);
  }
}

/**
 * Small helper: draw one option rectangle with aflk pulse. Shared by
 * main menu and gamemode submenu -- Java's own rectangle-drawing
 * pattern is copy-pasted 4 times per method (once per option) with
 * identical shape but different coordinates/colors, so extracting the
 * shape once here matches "port the shape, not the copy-paste".
 * Toggles *aflk_ptr each call so the color alternates frame-to-frame,
 * exactly what the Java's aflk does.
 */
static void draw_menu_option_rect(Graphics2D *g, int32_t x, int32_t y, int32_t w, int32_t h,
                                    bool selected, int32_t ra, int32_t ga, int32_t ba,
                                    int32_t rs, int32_t gs, int32_t bs, bool *aflk_ptr) {
  if (selected) {
    if (*aflk_ptr) { gfx_set_color(g, ra, ga, ba); *aflk_ptr = false; }
    else            { gfx_set_color(g, rs, gs, bs); *aflk_ptr = true; }
  } else {
    gfx_set_color(g, 0, 0, 0);
  }
  // Java uses drawRoundRect(x,y,w,h,7,20). We don't have a rounded-rect
  // primitive; drawRect is visually close at this size (3-4px corners).
  gfx_draw_rect(g, x, y, w, h);
}

static void draw_main_menu(Graphics2D *g,
                            HudImg bgmain, HudImg logomadbg, HudImg logomadnes,
                            HudImg dude, HudImg logocars, HudImg opback, HudImg opti,
                            HudImg byrd, HudImg nfmcoms, HudImg opsettings,
                            int32_t *bgmy_ptr, int32_t *flkat_ptr,
                            int32_t *gxdu_ptr, int32_t *gydu_ptr, int32_t *movly_ptr,
                            int32_t opselect, bool *aflk_ptr) {
  draw_menu_common_bg(g, bgmain, logomadbg, logomadnes, dude, logocars, opback,
                       bgmy_ptr, flkat_ptr, gxdu_ptr, gydu_ptr, movly_ptr);

  // 7. Option rectangles: 3 visible options (Play Game / Instructions /
  // Credits), matching Java's opselect 0 / 2 / 3 -- see this function's
  // own header comment on why option 1 (Multiplayer) is skipped.
  //
  // Each entry: {y, w, aflk color, !aflk color}. x/h are the same for
  // all rows (h=22, x varies). Java xtGraphics.java:4368-4447 laid out
  // 4 selectors; we drop selector index 1 (Multiplayer) and re-index
  // our own opselect 0..2 -> Java's opselect 0/2/3.
  // Java's 4 rows sat at y=261, 291, 321, 351 (30px apart) -- we compact
  // ours to 3 contiguous rows at 261, 291, 321 so the visual doesn't
  // have a Multiplayer-shaped hole in it. Widths/x-centers match the
  // Java's own per-label metrics (each option has its own snug width
  // because the labels themselves have different lengths).
  struct MenuOpt { int32_t x, y, w; int32_t r_aflk, g_aflk, b_aflk; int32_t r_solid, g_solid, b_solid; };
  // A fourth row, this port's Settings, takes the slot the original's own
  // fourth row used (y=351), which the brown pill already makes room for.
  const struct MenuOpt opts[4] = {
    { 343, 261, 110, 200, 200,   0, 255, 128, 0 },  // Play Game (Java opselect=0)
    { 301, 291, 196, 200, 128,   0, 255, 128, 0 },  // Game Instructions (Java opselect=2, moved from y=321)
    { 357, 321,  85, 200,   0,   0, 255, 128, 0 },  // Credits (Java opselect=3, moved from y=351)
    { 353, 351,  93, 200, 200,   0, 255, 128, 0 },  // Settings (this port's)
  };

  // In Spanish the rows are their own labels (g_es_opti_rows), centred, and
  // each rect is as wide as its label plus the originals' margin.
  const bool es = i18n_lang() == I18N_LANG_ES && g_es_opti_rows[0].tex >= 0;
  const HudImg es_rows[4] = {g_es_opti_rows[0], g_es_opti_rows[1], g_es_opti_rows[2], opsettings};
  for (int32_t i = 0; i < 4; i++) {
    const struct MenuOpt *o = &opts[i];
    int32_t x = o->x, w = o->w;
    if (es && es_rows[i].tex >= 0) {
      w = es_rows[i].w + 22;
      x = 399 - w / 2;
    }
    draw_menu_option_rect(g, x, o->y, w, 22, i == opselect,
                           o->r_aflk, o->g_aflk, o->b_aflk,
                           o->r_solid, o->g_solid, o->b_solid, aflk_ptr);
  }

  // 8. options.png (opti, 211x105) label sprites. Java's own options.png
  // has 4 rows laid out at ~30px pitch (each label centered vertically
  // within its 30px slot). We crop the 3 rows we KEEP into contiguous
  // dst positions matching the opts[] rects above:
  //   row 0 (Play Game):    src_y 0..30  -> dst (294, 265) height 30
  //   row 2 (Instructions): src_y 60..90 -> dst (294, 295) height 30  (was 325 with the gap; now contiguous)
  //   row 3 (Credits):      src_y 90..105 -> dst (294, 325) height 15 (partial: image is only 105 tall)
  // Skip row 1 (Multiplayer, src_y 30..60) entirely.
  if (es) {
    for (int32_t i = 0; i < 3; i++) {
      const HudImg r = es_rows[i];
      // Bottom-aligned to the 15 px row: an accent makes a label taller.
      if (r.tex >= 0) gfx_draw_image(g, r.tex, 400 - r.w / 2, 265 + 30 * i + 15 - r.h, r.w, r.h);
    }
  } else if (opti.tex >= 0) {
    gfx_draw_image_sub(g, opti.tex, 294, 265, opti.w, 30,
                        0, 0, opti.w, 30, opti.w, opti.h);
    gfx_draw_image_sub(g, opti.tex, 294, 295, opti.w, 30,
                        0, 60, opti.w, 30, opti.w, opti.h);
    gfx_draw_image_sub(g, opti.tex, 294, 325, opti.w, 15,
                        0, 90, opti.w, 15, opti.w, opti.h);
  }
  if (opsettings.tex >= 0) {
    gfx_draw_image(g, opsettings.tex, 400 - opsettings.w / 2, 355, opsettings.w, opsettings.h);
  }

  // 9. byrd at (72, 410). Java 4493.
  if (byrd.tex >= 0) {
    gfx_draw_image(g, byrd.tex, 72, 410, byrd.w, byrd.h);
  }

  // 10. nfmcoms at (567, 410). Java 4494.
  if (nfmcoms.tex >= 0) {
    gfx_draw_image(g, nfmcoms.tex, 567, 410, nfmcoms.w, nfmcoms.h);
  }
}

/**
 * Ports xtGraphics.java:4504's maini2() -- the GAMEMODE SUBMENU (fase
 * 102), reached from main menu's "Play Game" option. See
 * native/docs/MENU_FLOW.md §3.3 for the pixel-exact spec.
 *
 * Same shared background as the main menu (bgmain scroll, logomadbg
 * 0.6-alpha watermark, logomadnes wordmark, dude blink, logocars,
 * opback pill) -- see draw_menu_common_bg above.
 *
 * Deliberately hides the Multiplayer option (user decision, matching
 * main menu). Java's 4 options at y=262/290/318/346 become our 3 at
 * 262/290/318 (option index 2 = Multiplayer skipped), and options2.png
 * is cropped to only draw the 3 rows we keep: NFM 1, NFM 2, Free Play.
 * All 3 modes are always available -- which matches the original, since
 * its `dropf` gate around Free Play is dead code there too (see the
 * layout table's own comment below for the evidence).
 *
 * `opselect` here maps to Java's own opselect: 0 (NFM 1), 1 (NFM 2),
 * 2 (Free Play). Java's opselect=2 (Multiplayer) doesn't exist here.
 * Same-shape aflk_ptr as draw_main_menu (both share the same field on
 * `this` in Java, so ours share the same variable in main() too).
 */
static void draw_gamemode_menu(Graphics2D *g,
                                 HudImg bgmain, HudImg logomadbg, HudImg logomadnes,
                                 HudImg dude, HudImg logocars, HudImg opback, HudImg opti2,
                                 HudImg byrd, HudImg nfmcoms, HudImg extlabel, HudImg careerlabel,
                                 int32_t *bgmy_ptr, int32_t *flkat_ptr,
                                 int32_t *gxdu_ptr, int32_t *gydu_ptr, int32_t *movly_ptr,
                                 int32_t opselect, bool *aflk_ptr) {
  draw_menu_common_bg(g, bgmain, logomadbg, logomadnes, dude, logocars, opback,
                       bgmy_ptr, flkat_ptr, gxdu_ptr, gydu_ptr, movly_ptr);

  // 3 visible option rects. Java xtGraphics.java:4551-4632 laid out 4
  // selectors at y=262+dropf, 290+dropf, 318+dropf, 346. We drop
  // selector index 2 (Multiplayer, at y=318 in Java) and re-index our
  // opselect 0..2 -> Java's opselect 0/1/3. Free Play moves up from
  // y=346 to y=318 so the 3 rows are contiguous (28px apart, matching
  // Java's own gap between rects 0..2), no Multiplayer-shaped hole.
  //
  // Java's rects here are all written as `262 + dropf` etc., and its
  // Free Play row + the 4-option wrap range are both gated on
  // `dropf == 0`. That reads like a progression lock, and this port's
  // comments used to describe skipping it as a deliberate deviation --
  // it is not. `dropf` is DEAD in the original: xtGraphics.java assigns
  // it exactly once, `dropf = 0` in the constructor (:414), and nothing
  // in ANY of the decompiled classes ever writes it again. So every
  // `+ dropf` is `+ 0`, the `dropf != 0` label mask (:4634) never runs,
  // and Free Play is always present with the wrap range always 0..3.
  // Drawing all rows unconditionally is therefore the FAITHFUL
  // behaviour, not a simplification of one.
  struct MenuOpt { int32_t x, y, w; int32_t r_aflk, g_aflk, b_aflk; int32_t r_solid, g_solid, b_solid; };
  const struct MenuOpt opts[5] = {
    { 358, 262,  82, 200,  64,   0, 255, 128,  0 },  // NFM 1     (Java opselect=0)
    { 358, 290,  82, 200,  64,   0, 255,  95,  0 },  // NFM 2     (Java opselect=1)
    { 348, 318, 102, 200,  64,   0, 255, 128,  0 },  // Free Play (Java opselect=3, moved from y=346)
    { 348, 346, 102, 200,  64,   0, 255, 128,  0 },  // RPG Mode, Extended's career, the original's 4th slot
    { 348, 374, 102, 200,  64,   0, 255, 128,  0 },  // Tournament: Extended's Premier Tournament
  };

  // In Spanish, Free Play is its own label; a longer label widens its rect.
  const bool es = i18n_lang() == I18N_LANG_ES && g_es_freeplay.tex >= 0;
  const HudImg labels[5] = {{-1, 0, 0}, {-1, 0, 0}, g_es_freeplay, careerlabel, extlabel};
  for (int32_t i = 0; i < 5; i++) {
    const struct MenuOpt *o = &opts[i];
    int32_t x = o->x, w = o->w;
    if (es && labels[i].tex >= 0 && labels[i].w + 12 > w) {
      w = labels[i].w + 12;
      x = 399 - w / 2;
    }
    draw_menu_option_rect(g, x, o->y, w, 22, i == opselect,
                           o->r_aflk, o->g_aflk, o->b_aflk,
                           o->r_solid, o->g_solid, o->b_solid, aflk_ptr);
  }

  // options2.png (opti2, 107x100) label sprites. Java draws the whole
  // image at (346, 265) in ONE shot and never crops it; this port has to
  // crop, because it drops the Multiplayer row and closes the gap.
  //
  // The rows are NOT the even 25px slices the first cut of this assumed.
  // Measured from the actual file, the four labels occupy y 0..15, 28..43,
  // 56..71 and 85..99 -- a 28px pitch with the last band sitting flush
  // against the bottom edge. Slicing at 0/25/50/75 therefore caught each
  // label at a different offset inside its slice, and Free Play, whose
  // band starts 10px into its slice, was drawn about nine pixels low --
  // hanging off the bottom of its 22px-tall rect. That is the reported
  // misalignment.
  //
  // Cropping each band exactly and placing it where the original puts it
  // fixes it. In Java the label tops land at 265/293/321/350 against rects
  // at 262/290/318/346, i.e. a constant 3-4px inset. Rows 0 and 1 keep
  // their screen y; the Free Play row moves up 28 with its own rect
  // (346 -> 318), so its label goes 350 -> 322. x and width are untouched
  // (the full 107px strip at x=346), which preserves each label's
  // horizontal placement exactly as the source has it.
  if (opti2.tex >= 0) {
    gfx_draw_image_sub(g, opti2.tex, 346, 265, opti2.w, 16,
                        0,  0, opti2.w, 16, opti2.w, opti2.h);
    gfx_draw_image_sub(g, opti2.tex, 346, 293, opti2.w, 16,
                        0, 28, opti2.w, 16, opti2.w, opti2.h);
    if (es) gfx_draw_image(g, g_es_freeplay.tex, 400 - g_es_freeplay.w / 2, 321, g_es_freeplay.w, g_es_freeplay.h);
    else gfx_draw_image_sub(g, opti2.tex, 346, 322, opti2.w, 15,
                             0, 85, opti2.w, 15, opti2.w, opti2.h);
  }
  if (careerlabel.tex >= 0)
    gfx_draw_image(g, careerlabel.tex, 400 - careerlabel.w / 2, 349, careerlabel.w, careerlabel.h);
  if (extlabel.tex >= 0) gfx_draw_image(g, extlabel.tex, 400 - extlabel.w / 2, 377, extlabel.w, extlabel.h);

  // Footer bylines, same positions as main menu.
  if (byrd.tex >= 0) {
    gfx_draw_image(g, byrd.tex, 72, 410, byrd.w, byrd.h);
  }
  if (nfmcoms.tex >= 0) {
    gfx_draw_image(g, nfmcoms.tex, 567, 410, nfmcoms.w, nfmcoms.h);
  }
}

// Every image xtGraphics.java's inst() draws, bundled so the page
// renderer below takes one parameter instead of eighteen. Populated once
// at boot from data/images.zip (see the load block in game_run).
typedef struct {
  HudImg dude[3], oflaot, nfm, racing, wasting, ory, chil, opwr, fixhoop,
         sarrow, space, arrows, plus, stunts, back, next, contin, bggo, bgmain;
  HudImg stunt_arrows; // what the stunt combo pairs with the handbrake: arrows.gif, or the Vita's stick
  HudImg kz, kx, kv, kenter, km, kn, ks; // page-15 "other controls" key caps
} InstAssets;

/**
 * Ports xtGraphics.java:4048's inst() -- the INSTRUCTIONS screen (fase
 * 11), in full. Java drives it as a 9-screen flipbook off its shared
 * `flipo` counter: pages 1, 3, 5, 7, 9, 11, 13, 15 and 16, where the
 * EVEN values (2/4/6/8/10/12/14) exist for exactly one frame as a
 * transient -- inst()'s own top-of-method fixups (:4049-4078) bump each
 * even value to the next odd one while re-arming `dudo`, so an even
 * value never actually renders. Layouts are shared in pairs (3/5, 7/9,
 * 11/13) with only the body text and its colour differing, which is why
 * nine screens need only five layouts.
 *
 * All the art is in data/images.zip, including the three whose Java FIELD
 * name disagrees with their filename (see the loader).
 *
 * Text is the original's, in its own fonts (Arial bold 13 for the body, 11
 * for the labels), with the platform's button names spliced in.
 *
 * `aflk`/`duds`/`dudo` are caller-owned (Java keeps them on `this`):
 * `aflk` is the shared frame-flip toggle, `duds` picks Coach Insano's
 * current mouth frame, and `dudo` counts down how long he keeps talking
 * on this page.
 */
static void draw_instructions(Graphics2D *g, const InstAssets *ia,
                               int32_t flipo, bool aflk, int32_t duds, int32_t dudo,
                               int32_t *bgmy, float *bgf) {
  draw_mainbg_2(g, ia->bgmain, bgmy, bgf, flipo);

  // :4081-4083 -- bggo photo washed over the backdrop at 30%.
  if (ia->bggo.tex >= 0) {
    gfx_set_composite(g, 0.3f);
    gfx_draw_image(g, ia->bggo.tex, 65, 25, ia->bggo.w, ia->bggo.h);
    gfx_set_composite(g, 1.0f);
  }
  // :4085-4086 -- right and bottom letterbox repainted AFTER bggo (which
  // is 800 wide and would otherwise bleed over them).
  gfx_set_color(g, 0, 0, 0);
  gfx_fill_rect(g, 735, 0, 65, 450);
  gfx_fill_rect(g, 65, 425, 670, 25);

  // :4093-4112 -- Coach Insano's talking head, on every page except the
  // two "main controls" ones (1 and 16), which have no narration.
  if (flipo != 1 && flipo != 16) {
    if (ia->dude[duds].tex >= 0) {
      gfx_set_composite(g, 0.4f);
      gfx_draw_image(g, ia->dude[duds].tex, 95, 15, ia->dude[duds].w, ia->dude[duds].h);
      gfx_set_composite(g, 1.0f);
    }
    if (ia->oflaot.tex >= 0) {
      gfx_draw_image(g, ia->oflaot.tex, 192, 42, ia->oflaot.w, ia->oflaot.h);
    }
  }

  gfx_set_color(g, 0, 64, 128);
  font_set(FONT_BOLD, 13);

  // ---- Pages 3 / 5: what completing a stage means. :4114-4147 ----
  if (flipo == 3 || flipo == 5) {
    if (flipo == 3) {
      font_draw(g, "Hello!  Welcome to the world of", 262, 67);
      font_draw(g, "!", 657, 67);
      if (ia->nfm.tex >= 0) gfx_draw_image(g, ia->nfm.tex, 469, 55, ia->nfm.w, ia->nfm.h);
      font_draw(g, "In this game there are two ways to complete a stage.", 262, 107);
      font_draw(g, "One is by racing and finishing in first place, the other is by", 262, 127);
      font_draw(g, "wasting and crashing all the other cars in the stage!", 262, 147);
    } else {
      gfx_set_color(g, 0, 128, 255);
      font_draw(g, "While racing, you will need to focus on going fast and passing", 262, 67);
      font_draw(g, "through all the checkpoints in the track. To complete a lap, you", 262, 87);
      font_draw(g, "must not miss a checkpoint.", 262, 107);
      font_draw(g, "While wasting, you will just need to chase the other cars and", 262, 127);
      font_draw(g, "crash into them (without worrying about track and checkpoints).", 262, 147);
    }
    gfx_set_color(g, 0, 0, 0);
    if (ia->racing.tex >= 0)  gfx_draw_image(g, ia->racing.tex, 165, 185, ia->racing.w, ia->racing.h);
    if (ia->ory.tex >= 0)     gfx_draw_image(g, ia->ory.tex, 429, 235, ia->ory.w, ia->ory.h);
    if (ia->wasting.tex >= 0) gfx_draw_image(g, ia->wasting.tex, 492, 185, ia->wasting.w, ia->wasting.h);
    font_set(FONT_BOLD, 11);
    font_draw(g, "Checkpoint", 392, 189);
    font_set(FONT_BOLD, 13);
    font_drawf(g, 125, 320, "Drive your car using the %s and %s", KEY_STEER, KEY_HANDB);
    if (ia->space.tex >= 0)  gfx_draw_image(g, ia->space.tex, 171, 355, ia->space.w, ia->space.h);
    if (ia->arrows.tex >= 0) gfx_draw_image(g, ia->arrows.tex, 505, 323, ia->arrows.w, ia->arrows.h);
    font_set(FONT_BOLD, 11);
    font_drawf(g, 125, 341, "(When your car is on the ground %s is for Handbrake)", KEY_HANDB);
    font_draw(g, "Accelerate", 515, 319);
    font_draw(g, "Brake/Reverse", 506, 397);
    font_draw(g, "Turn left", 454, 375);
    font_draw(g, "Turn right", 590, 375);
    font_draw(g, "Handbrake", 247 + KEYLBL_DX, 374 + KEYLBL_DY);
  }

  // ---- Pages 7 / 9: power and stunts. :4149-4188 ----
  if (flipo == 7 || flipo == 9) {
    if (flipo == 7) {
      font_draw(g, "Whether you are racing or wasting the other cars you will need", 262, 67);
      font_draw(g, "to power up your car.", 262, 87);
      font_draw(g, "=> More 'Power' makes your car become faster and stronger!", 262, 107);
      font_draw(g, "To power up your car (and keep it powered up) you will need to", 262, 127);
      font_draw(g, "perform stunts!", 262, 147);
      if (ia->chil.tex >= 0) gfx_draw_image(g, ia->chil.tex, 167, 295, ia->chil.w, ia->chil.h);
    } else {
      font_draw(g, "The better the stunt the more power you get!", 262, 67);
      gfx_set_color(g, 0, 128, 255);
      font_draw(g, "Forward looping pushes your car forwards in the air and helps", 262, 87);
      font_draw(g, "when racing. Backward looping pushes your car upwards giving it", 262, 107);
      font_draw(g, "more hang time in the air making it easier to control its landing.", 262, 127);
      font_draw(g, "Left and right rolls shift your car in the air left and right slightly.", 262, 147);
      // :4166 -- on this page the illustration BLINKS while Coach Insano
      // is still talking (dudo >= 150), then goes solid once he stops.
      if ((aflk || dudo < 150) && ia->chil.tex >= 0) {
        gfx_draw_image(g, ia->chil.tex, 167, 295, ia->chil.w, ia->chil.h);
      }
    }
    gfx_set_color(g, 0, 0, 0);
    if (ia->stunts.tex >= 0) gfx_draw_image(g, ia->stunts.tex, 105, 175, ia->stunts.w, ia->stunts.h);
    if (ia->opwr.tex >= 0)   gfx_draw_image(g, ia->opwr.tex, 540, 253, ia->opwr.w, ia->opwr.h);
    font_set(FONT_BOLD, 13);
    font_draw(g, "To perform stunts. When your car is in the AIR:", 125, 310);
    font_drawf(g, 125, 330, "Press combo %s + %s", KEY_HANDB, KEY_STUNT);
    if (ia->space.tex >= 0)  gfx_draw_image(g, ia->space.tex, 185, 355, ia->space.w, ia->space.h);
    if (ia->plus.tex >= 0)   gfx_draw_image(g, ia->plus.tex, 405, 358, ia->plus.w, ia->plus.h);
    if (ia->stunt_arrows.tex >= 0)
      gfx_draw_image(g, ia->stunt_arrows.tex, 491, 323, ia->stunt_arrows.w, ia->stunt_arrows.h);
    font_set(FONT_BOLD, 11);
    font_draw(g, "Forward Loop", 492, 319);
    font_draw(g, "Backward Loop", 490, 397);
    font_draw(g, "Left Roll", 443, 375);
    font_draw(g, "Right Roll", 576, 375);
    font_draw(g, KEY_HANDB, 266 + KEYLBL_DX, 374 + KEYLBL_DY);
    // :4187 -- the filled segment inside the power-meter art.
    gfx_set_color(g, 140, 243, 244);
    gfx_fill_rect(g, 602, 257, 76, 9);
  }

  // ---- Pages 11 / 13: guidance arrow and the repair hoop. :4189-4211 ----
  if (flipo == 11 || flipo == 13) {
    if (flipo == 11) {
      font_draw(g, "When wasting cars, to help you find the other cars in the stage,", 262, 67);
      font_drawf(g, 262, 87, "%s to toggle the guidance arrow from pointing to the track", KEY_ARRACE);
      font_draw(g, "to pointing to the cars.", 262, 107);
      font_draw(g, "When your car is damaged. You fix it (and reset its 'Damage') by", 262, 127);
      font_draw(g, "jumping through the electrified hoop.", 262, 147);
    } else {
      gfx_set_color(g, 0, 128, 255);
      font_draw(g, "You will find that in some stages it's easier to waste the other cars", 262, 67);
      font_draw(g, "and in some others it's easier to race and finish in first place.", 262, 87);
      font_draw(g, "It is up to you to decide when to waste and when to race.", 262, 107);
      font_draw(g, "And remember, 'Power' is an important factor in the game. You", 262, 127);
      font_draw(g, "will need it whether you are racing or wasting!", 262, 147);
    }
    gfx_set_color(g, 0, 0, 0);
    if (ia->fixhoop.tex >= 0) gfx_draw_image(g, ia->fixhoop.tex, 185, 218, ia->fixhoop.w, ia->fixhoop.h);
    if (ia->sarrow.tex >= 0)  gfx_draw_image(g, ia->sarrow.tex, 385, 228, ia->sarrow.w, ia->sarrow.h);
    font_set(FONT_BOLD, 11);
    font_draw(g, "The Electrified Hoop", 192, 216);
    font_draw(g, "Jumping through it fixes your car.", 158, 338);
    font_draw(g, "Make guidance arrow point to cars.", 385, 216);
  }

  // ---- Page 15: sign-off + the "other controls" reference. :4212-4235 ----
  // (The original's apostrophes here are U+2019; the atlas is ASCII.)
  if (flipo == 15) {
    font_draw(g, "And if you don't know who I am,", 262, 67);
    font_draw(g, "I am Coach Insano, I am the coach and narrator of this game!", 262, 87);
    font_draw(g, "I recommended starting with NFM 1 if it's your first time to play.", 262, 127);
    font_draw(g, "Good Luck & Have Fun!", 262, 147);
    gfx_set_color(g, 0, 0, 0);
    font_draw(g, "Other Controls :", 155, 205);
    font_set(FONT_BOLD, 11);
    if (ia->kz.tex >= 0) gfx_draw_image(g, ia->kz.tex, 169, 229, ia->kz.w, ia->kz.h);
    font_draw(g, "OR", 206, 251);
    if (ia->kx.tex >= 0) gfx_draw_image(g, ia->kx.tex, 229, 229, ia->kx.w, ia->kx.h);
    font_draw(g, "To look behind you while driving.", 267, 251);
    if (ia->kv.tex >= 0) gfx_draw_image(g, ia->kv.tex, 169, 279, ia->kv.w, ia->kv.h);
    font_draw(g, "Change Views", 207, 301);
    if (ia->kenter.tex >= 0) gfx_draw_image(g, ia->kenter.tex, 169, 329, ia->kenter.w, ia->kenter.h);
    font_draw(g, "Navigate & Pause Game", 275, 351);
    if (ia->km.tex >= 0) gfx_draw_image(g, ia->km.tex, 489, 229, ia->km.w, ia->km.h);
    font_draw(g, "Mute Music", 527, 251);
    if (ia->kn.tex >= 0) gfx_draw_image(g, ia->kn.tex, 489, 279, ia->kn.w, ia->kn.h);
    font_draw(g, "Mute Sound Effects", 527, 301);
    if (ia->ks.tex >= 0) gfx_draw_image(g, ia->ks.tex, 489, 329, ia->ks.w, ia->ks.h);
    font_draw(g, "Toggle radar / map", 527, 351);
  }

  // ---- Pages 1 / 16: the main-controls recap. :4236-4275 ----
  if (flipo == 1 || flipo == 16) {
    font_set(FONT_BOLD, 13);
    gfx_set_color(g, 0, 0, 0);
    draw_centered(g, (flipo == 16) ? "M A I N    C O N T R O L S   -   once again!"
                                   : "M A I N    C O N T R O L S", 400, 49);
    font_drawf(g, 125, 80, "Drive your car using the %s:", KEY_STEER);
    font_drawf(g, 125, 101, "On the GROUND %s is for Handbrake", KEY_HANDB);
    if (ia->space.tex >= 0)  gfx_draw_image(g, ia->space.tex, 171, 115, ia->space.w, ia->space.h);
    if (ia->arrows.tex >= 0) gfx_draw_image(g, ia->arrows.tex, 505, 83, ia->arrows.w, ia->arrows.h);
    font_set(FONT_BOLD, 11);
    font_draw(g, "Accelerate", 515, 79);
    font_draw(g, "Brake/Reverse", 506, 157);
    font_draw(g, "Turn left", 454, 135);
    font_draw(g, "Turn right", 590, 135);
    font_draw(g, "Handbrake", 247 + KEYLBL_DX, 134 + KEYLBL_DY);
    // :4257 -- the rule across the page is a run of dashes through drawcs.
    gfx_set_color(g, 0, 64, 128);
    draw_centered(g, "----------------------------------------------------------------------------------------------------------------------------------------------------", 400, 175);
    gfx_set_color(g, 0, 0, 0);
    font_set(FONT_BOLD, 13);
    font_draw(g, "To perform STUNTS:", 125, 200);
    font_drawf(g, 125, 220, "In the AIR press combo %s + %s", KEY_HANDB, KEY_STUNT);
    if (ia->space.tex >= 0)  gfx_draw_image(g, ia->space.tex, 185, 245, ia->space.w, ia->space.h);
    if (ia->plus.tex >= 0)   gfx_draw_image(g, ia->plus.tex, 405, 248, ia->plus.w, ia->plus.h);
    if (ia->stunt_arrows.tex >= 0)
      gfx_draw_image(g, ia->stunt_arrows.tex, 491, 213, ia->stunt_arrows.w, ia->stunt_arrows.h);
    font_set(FONT_BOLD, 11);
    font_draw(g, "Forward Loop", 492, 209);
    font_draw(g, "Backward Loop", 490, 287);
    font_draw(g, "Left Roll", 443, 265);
    font_draw(g, "Right Roll", 576, 265);
    font_draw(g, KEY_HANDB, 266 + KEYLBL_DX, 264 + KEYLBL_DY);
    if (ia->stunts.tex >= 0) gfx_draw_image(g, ia->stunts.tex, 125, 285, ia->stunts.w, ia->stunts.h);
  }

  // :4276-4284 -- paging affordances. Java indexes next[]/back[] by a
  // mouse-hover frame; with no mouse there is only ever frame 0.
  if (flipo >= 1 && flipo <= 15 && ia->next.tex >= 0) {
    gfx_draw_image(g, ia->next.tex, 665, 395, ia->next.w, ia->next.h);
  }
  if (flipo >= 3 && flipo <= 16 && ia->back.tex >= 0) {
    gfx_draw_image(g, ia->back.tex, 75, 395, ia->back.w, ia->back.h);
  }
  if (flipo == 16 && ia->contin.tex >= 0) {
    gfx_draw_image(g, ia->contin.tex, 565, 395, ia->contin.w, ia->contin.h);
  }
}

// ---- This port's extra credits and Extended guide -----------------------------

// Credits after the original's page: Need for Madness 2 Extended's own
// (its carselect credits, XT 15286-15340, word for word), then this port's.
#define CREDITS_PAGES 5

static void draw_extra_credits(Graphics2D *g, int32_t page, HudImg next, HudImg back, HudImg contin) {
  typedef struct { int32_t kind; const char *text; } CreditLine;   // kind 0 heading, 1 line, 2 gap
  static const CreditLine kPages[CREDITS_PAGES - 1][16] = {
      {{0, "Need for Madness 2 Extended"},
       {1, "This game was modded by Ten Graves, with"},
       {1, "guidance from DragShot, rafa1231518, and Kaffeinated."},
       {2, NULL},
       {0, "Car creators"},
       {1, "Tails, DJ Miker, Mezzelo, Phyrexian, Trelivision, Chaotic,"},
       {1, "Afterburn, Excalibur, Vitalogy, Ultimato, Rulue, Tunari,"},
       {1, "Toazuka, GX, projectDUB, RAD1 and KingOfSpeed."},
       {1, "All Classic mode cars by Omar Waly."},
       {2, NULL},
       {0, "Stage creators"},
       {1, "All Classic mode stages by Omar Waly."},
       {1, "All other stages by Ten Graves."}},
      {{0, "Need for Madness 2 Extended - Music"},
       {1, "RPG mode stages 18 and 23 have been produced by SEGA."},
       {1, "Some other music tracks are remixes of music by SEGA."},
       {2, NULL},
       {1, "RPG mode stages 16, 17, 19, 20, 21, 22 and 23b are remixes"},
       {1, "by NicoCW, teckworks, bridgecapper227, Kamex and"},
       {1, "DJ FACT.50 of Vernian Process"},
       {1, "(check out their youtube accounts!)."},
       {2, NULL},
       {1, "All other music is from modarchive.org."},
       {2, NULL},
       {1, "The original game and concept of Need for Madness"},
       {1, "is by Omar Waly of radicalplay.com."}},
      {{0, "New Cars"},
       {1, "Ten cars from Need for Madness: Revised & Recharged,"},
       {1, "by Ryan Albano."},
       {1, "Your own cars from the web Car Maker, by their authors"},
       {1, "(\"Created by\" in Car Select)."},
       {2, NULL},
       {0, "Libraries"},
       {1, "stb_vorbis (Ogg music) by Sean Barrett."},
       {1, "devkitPro and libnx, the Nintendo Switch homebrew toolchain."}},
      {{2, NULL},
       {2, NULL},
       {0, "Nintendo Switch Port"},
       {2, NULL},
       {1, "Need for Madness and Need for Madness 2 Extended,"},
       {1, "ported to the Nintendo Switch by"},
       {2, NULL},
       {3, "HopeAero"},
       {2, NULL},
       {1, "The original game rebuilt in C: its 3D engine, physics,"},
       {1, "AI and menus, with Extended's specials, cars, stages"},
       {1, "and RPG mode, made to play on the console."},
       {2, NULL},
       {1, "Thanks for playing!"}},
  };
  if (page < 1 || page >= CREDITS_PAGES) return;
  int32_t y = 108;
  for (int32_t i = 0; i < 16; i++) {
    const CreditLine *l = &kPages[page - 1][i];
    if (l->kind == 0 && !l->text) break;
    if (l->kind == 2) {
      y += 10;
      continue;
    }
    if (l->kind == 0) {
      font_set(FONT_BOLD, 15);
      gfx_set_color(g, 0, 0, 0);
      draw_centered(g, l->text, 400, y);
      y += 22;
    } else if (l->kind == 3) {
      font_set(FONT_BOLD, 22);
      gfx_set_color(g, 40, 60, 0);
      draw_centered(g, l->text, 400, y + 8);
      y += 30;
    } else {
      font_set(FONT_BOLD, 13);
      gfx_set_color(g, 33, 49, 0);
      draw_centered(g, l->text, 400, y);
      y += 18;
    }
  }
  if (back.tex >= 0) gfx_draw_image(g, back.tex, 75, 395, back.w, back.h);
  if (page < CREDITS_PAGES - 1) {
    if (next.tex >= 0) gfx_draw_image(g, next.tex, 665, 395, next.w, next.h);
  } else if (contin.tex >= 0) {
    gfx_draw_image(g, contin.tex, 565, 395, contin.w, contin.h);
  }
}

// The Extended guide: pages after the original's instructions (its last
// page's Continue leads here), in their look -- Coach Insano and his speech
// box, blue text -- explaining what this build adds.
#define GUIDE_PAGES 6
#define GUIDE_FLIPO 99   // inst_flipo while the guide shows: draw_instructions draws only the backdrop

/** One line of the guide: its format translated (i18n_port_es.c), then filled in. */
static void guide_line(char *dst, size_t n, const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(dst, n, tr(fmt), ap);
  va_end(ap);
}

static void draw_ext_guide(Graphics2D *g, const InstAssets *ia, int32_t page) {
  char l[12][96];
  int32_t n = 0;
  const char *title = "";
#define GL(...) guide_line(l[n++], sizeof(l[0]), __VA_ARGS__)
  switch (page) {
    case 0:
      title = "Extended:  special attacks";
      GL("Every car now has its own special attack!");
      GL("The SPECIAL bar (top right) charges as you do stunts.");
      GL("When it is full, press  %s  to fire it.", KEY_SPECIAL);
      GL("A special boosts your car for a while, and many also");
      GL("weaken a random rival: lower speed, swapped strength,");
      GL("drained health or reduced defence.");
      GL("Car Select shows each car's special at the top left.");
      GL("In a race,  %s  switches the car list on the right", KEY_LISTBARS);
      GL("between damage and special bars.");
      GL("NFM 1 and NFM 2 race the original way, without specials.");
      break;
    case 1:
      title = "Extended:  cameras, cars and stages";
      GL("%s  cycles the cameras, Extended's far camera included.", KEY_VIEW);
      GL("Turn off the ones you don't use in Settings > Gameplay.");
      GL("%s  points the arrow at the cars instead of the", KEY_ARRACE_HINT);
      GL("track, and names the car it is after.");
      GL("Free Play: NFM's 27 stages, then 25 of Extended's own.");
      GL("Up to 20 cars can race at once.");
      GL("Car Select:  Up / Down  filters the cars (Classic,");
      GL("Extended, R&R, Custom).");
      GL("Settings > Stress Test races 20 cars on the biggest stage.");
      break;
    case 2:
      title = "RPG Mode";
      GL("Extended's career: 31 stages, and your cars level up.");
      GL("Cars gain experience from checkpoints, wasting cars,");
      GL("stunts and winning (the bar at the bottom left).");
      GL("Every level up gives 4 stat points. In Car Select press");
      GL("%s  to open the upgrade panel:  %s  adds a point,", KEY_SPECIAL, KEY_CONTINUE);
      GL("%s  takes back one you added before closing it.", KEY_VIEW);
      GL("Clearing a checkpoint or wasting a car can give bonus");
      GL("stat points. Cars unlocked later level up more slowly.");
      break;
    case 3:
      title = "RPG Mode:  your rivals";
      GL("From stage 4 you meet beasts: bigger, stronger cars.");
      GL("From stage 15, shadow cars: faster, agile and tough.");
      GL("Some stages bring undead cars that won't stay wasted,");
      GL("and a boss waits at the end...");
      GL("Before a race, scouting shows each rival's car and level.");
      GL("The arrow tells a rival's level: red when it is more");
      GL("than 5 levels above yours.");
      GL("Beaten stages can be raced hard, with scaled levels or");
      GL("with no levels (Up / Down in Stage Select).");
      break;
    case 4:
      title = "RPG Mode:  bonus stages and perks";
      GL("Stages 5, 11, 15 and 18 hide a bonus stage: press  %s", KEY_SPECIAL);
      GL("in Stage Select. Win it for a prize car.");
      GL("Winning bonus stage 4 gives car points. Spend them on");
      GL("your car's six perks (the PERKS page of the panel).");
      GL("Each perk takes up to 20 points, and the panel says");
      GL("what it does: less power drain, more experience,");
      GL("more strength when badly damaged, and much more.");
      break;
    default:
      title = "RPG Mode:  your cars";
      GL("The CAR page of the upgrade panel can:");
      GL("- reshuffle a car's stat points (free once per new stage),");
      GL("- transfer its level and points to another car,");
      GL("- sell it for car points (it goes back to level 1).");
      GL("Your career saves itself after every race.");
      GL("That's all!  Have fun playing!");
      break;
  }
#undef GL
  // The speech box (oflaot, white x 248-710) runs on down under the longer pages.
  const int32_t bottom = 100 + 22 * (n - 1) + 16;
  if (bottom > 150) {
    gfx_set_color(g, 255, 255, 255);
    gfx_fill_rect(g, 248, 140, 463, bottom - 140);
    gfx_set_color(g, 160, 160, 160);
    gfx_draw_line(g, 247, 150, 247, bottom);
    gfx_draw_line(g, 711, 150, 711, bottom);
    gfx_draw_line(g, 247, bottom, 711, bottom);
  }
  gfx_set_color(g, 0, 64, 128);
  font_set(FONT_BOLD, 15);
  font_draw(g, title, 262, 67);
  font_set(FONT_BOLD, 13);
  for (int32_t i = 0; i < n; i++) font_draw(g, l[i], 262, 100 + 22 * i);
  if (ia->back.tex >= 0) gfx_draw_image(g, ia->back.tex, 75, 395, ia->back.w, ia->back.h);
  if (page < GUIDE_PAGES - 1) {
    if (ia->next.tex >= 0) gfx_draw_image(g, ia->next.tex, 665, 395, ia->next.w, ia->next.h);
  } else if (ia->contin.tex >= 0) {
    gfx_draw_image(g, ia->contin.tex, 565, 395, ia->contin.w, ia->contin.h);
  }
}

// Settings > Graphics' default: the Switch's screen is far past 800x450, so
// it starts in HD; the Vita (960x544) and desktop keep the original look.
#ifdef NFM_TARGET_SWITCH
#define NFM_DEFAULT_GRAPHICS GFX_HD
#else
#define NFM_DEFAULT_GRAPHICS GFX_ORIGINAL
#endif

/**
 * (Re)creates the two targets the frame is drawn into and the trail blends
 * between (see the comment where game_run() first calls this), sized for
 * Settings > Graphics: the 800x450 game space in an 800x450 texture
 * (Original, NEAREST; Smooth, LINEAR), or in a texture the display's own size
 * (HD) -- same game-space coordinates, just more pixels per unit. `had`
 * frees the previous pair first. Returns false (both freed) if either fails,
 * and the frame then draws straight to the display.
 */
static bool scene_targets_build(GfxGlRenderTarget *scene, GfxGlRenderTarget *accum, bool had,
                                int32_t width, int32_t height, int32_t quality) {
  if (had) {
    gfx_gl_render_target_free(scene);
    gfx_gl_render_target_free(accum);
  }
  int32_t px_w = width, px_h = height;
  if (quality == GFX_HD) platform_display_size(&px_w, &px_h);
  const bool linear = quality != GFX_ORIGINAL;
  if (!gfx_gl_render_target_init_scaled(scene, width, height, px_w, px_h, linear)) return false;
  if (!gfx_gl_render_target_init_scaled(accum, width, height, px_w, px_h, linear)) {
    gfx_gl_render_target_free(scene);
    return false;
  }
  // glTexImage2D with a NULL pixel pointer leaves contents undefined, and the
  // trail reads this one before it is ever fully written.
  gfx_gl_render_target_bind(accum);
  glClear(GL_COLOR_BUFFER_BIT);
  gfx_gl_render_target_bind(NULL);
  return true;
}

// ---- Settings screen ---------------------------------------------------------
//
// Reached from the main menu and from the pause menu. Pages of rows, laid out
// like the launcher's own menus (beige stripes, black-bordered rows, the
// selected one black with yellow text and a hazard stripe). Each row is data:
// a page to open, a GameSettings field with its list of values, Reset or
// Back -- adding an option is one line in kSettingsPages.

typedef enum {
  SET_MAIN, SET_GRAPHICS, SET_AUDIO, SET_INTERFACE, SET_GAMEPLAY, SET_CONTROLS,
  SET_RACE, SET_RIVALS,   // Free Play's Race Setup, opened from its stage list
  SET_PAGE_COUNT
} SettingsPageId;
// ROW_CAR: a car (its `page` is the rival's number, 0-18); ROW_START: start the race.
typedef enum { ROW_OPEN, ROW_CHOICE, ROW_RESET, ROW_BACK, ROW_BENCH, ROW_CAREER_RESET, ROW_CAR, ROW_START } SettingsRowKind;

typedef struct {
  SettingsRowKind kind;
  const char *label;
  int32_t page;              // ROW_OPEN: the page it opens
  size_t off;                // ROW_CHOICE: the GameSettings field...
  int32_t count, step;       // ...which takes 0, step, ..., (count - 1) * step
  const char *const *names;  // the values' names (NULL: the number itself)
  int32_t needs;             // NEED_*: shown only where the platform has it
} SettingsRow;

// NEED_RANDOM / NEED_MANUAL: shown only with the rivals picked that way.
enum { NEED_RUMBLE = 1, NEED_REMAP = 2, NEED_RANDOM = 4, NEED_MANUAL = 8 };
#ifdef NFM_TARGET_SWITCH
#define HAS_REMAP true
#else
#define HAS_REMAP false
#endif

#define SETTINGS_MAX_ROWS 21
typedef struct { const char *title; int32_t nrows; SettingsRow rows[SETTINGS_MAX_ROWS]; } SettingsPage;

static const char *const kOnOff[] = {"Off", "On"};
static const char *const kLanguageNames[] = {"English", "Español"};   // I18N_LANG_EN, I18N_LANG_ES
static const char *const kQualityNames[] = {"Original", "Smooth", "HD"};
static const char *const kDistNames[] = {"Original", "Far", "Max"};
static const char *const kDetailNames[] = {"High", "Low"};
static const char *const kBlurNames[] = {"Off", "20", "40", "60", "80", "100"};
static const char *const kFpsNames[] = {"Off", "FPS", "Detailed"};
static const char *const kSteerNames[] = {ICON_LSTICK " Left Stick", ICON_DPAD " D-Pad"};
static const char *const kRivalCount[] = {"1",  "2",  "3",  "4",  "5",  "6",  "7",  "8",  "9", "10",
                                          "11", "12", "13", "14", "15", "16", "17", "18", "19"};
static const char *const kPickNames[] = {"Random", "Manual"};
static const char *const kTierNames[] = {"Any", "Classic", "Extended", "R&R", "Custom"};
static const char *const kWinNames[] = {"Race or Waste", "Wasting Only", "Racing Only"};
static const char *const kLapNames[] = {"Stage", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10"};

#define SET_FIELD(f) offsetof(GameSettings, f)
static const SettingsPage kSettingsPages[SET_PAGE_COUNT] = {
  [SET_MAIN] = {"SETTINGS", 11, {
    {ROW_CHOICE, "Language", 0, SET_FIELD(language), 2, 1, kLanguageNames, false},
    {ROW_OPEN, "Graphics", SET_GRAPHICS, 0, 0, 0, NULL, false},
    {ROW_OPEN, "Audio", SET_AUDIO, 0, 0, 0, NULL, false},
    {ROW_OPEN, "Interface", SET_INTERFACE, 0, 0, 0, NULL, false},
    {ROW_OPEN, "Gameplay", SET_GAMEPLAY, 0, 0, 0, NULL, false},
    {ROW_OPEN, "Controls", SET_CONTROLS, 0, 0, 0, NULL, NEED_REMAP},
    {ROW_BENCH, "Performance Test", 0, 0, 0, 0, NULL, false},
    {ROW_BENCH, "Stress Test", 1, 0, 0, 0, NULL, false},
    {ROW_CAREER_RESET, "Reset RPG Mode", 0, 0, 0, 0, NULL, false},
    {ROW_RESET, "Reset to Defaults", 0, 0, 0, 0, NULL, false},
    {ROW_BACK, "Back", 0, 0, 0, 0, NULL, false}}},
  [SET_GRAPHICS] = {"SETTINGS - GRAPHICS", 8, {
    {ROW_CHOICE, "Image Quality", 0, SET_FIELD(graphics), 3, 1, kQualityNames, false},
    {ROW_CHOICE, "Draw Distance", 0, SET_FIELD(draw_dist), 3, 1, kDistNames, false},
    {ROW_CHOICE, "Scenery Detail", 0, SET_FIELD(detail), 2, 1, kDetailNames, false},
    {ROW_CHOICE, "Shadows", 0, SET_FIELD(shadows), 2, 1, kOnOff, false},
    {ROW_CHOICE, "Particles", 0, SET_FIELD(particles), 2, 1, kOnOff, false},
    {ROW_CHOICE, "Motion Blur", 0, SET_FIELD(blur), 6, 20, kBlurNames, false},
    {ROW_CHOICE, "Smooth Frames", 0, SET_FIELD(smooth), 2, 1, kOnOff, false},
    {ROW_BACK, "Back", 0, 0, 0, 0, NULL, false}}},
  [SET_AUDIO] = {"SETTINGS - AUDIO", 3, {
    {ROW_CHOICE, "Music", 0, SET_FIELD(music_vol), 11, 10, NULL, false},
    {ROW_CHOICE, "Effects", 0, SET_FIELD(sfx_vol), 11, 10, NULL, false},
    {ROW_BACK, "Back", 0, 0, 0, 0, NULL, false}}},
  [SET_INTERFACE] = {"SETTINGS - INTERFACE", 4, {
    {ROW_CHOICE, "Show FPS", 0, SET_FIELD(show_fps), 3, 1, kFpsNames, false},
    {ROW_CHOICE, "Names in Standings", 0, SET_FIELD(board_names), 2, 1, kOnOff, false},
    {ROW_CHOICE, "Career Counters", 0, SET_FIELD(career_counts), 2, 1, kOnOff, false},
    {ROW_BACK, "Back", 0, 0, 0, 0, NULL, false}}},
  [SET_GAMEPLAY] = {"SETTINGS - GAMEPLAY", 6, {
    {ROW_CHOICE, "Screen Shake", 0, SET_FIELD(shake), 2, 1, kOnOff, false},
    {ROW_CHOICE, "Vibration", 0, SET_FIELD(rumble), 2, 1, kOnOff, NEED_RUMBLE},
    {ROW_CHOICE, "Orbit Camera", 0, SET_FIELD(cam_orbit), 2, 1, kOnOff, false},
    {ROW_CHOICE, "Tripod Camera", 0, SET_FIELD(cam_watch), 2, 1, kOnOff, false},
    {ROW_CHOICE, "Far Camera", 0, SET_FIELD(cam_far), 2, 1, kOnOff, false},
    {ROW_BACK, "Back", 0, 0, 0, 0, NULL, false}}},
  [SET_RACE] = {"RACE SETUP", 10, {
    {ROW_CHOICE, "Rivals", 0, SET_FIELD(fp_opponents), 19, 1, kRivalCount, 0},
    {ROW_CHOICE, "Choose Rivals", 0, SET_FIELD(fp_pick), 2, 1, kPickNames, 0},
    {ROW_CHOICE, "Rival Cars", 0, SET_FIELD(fp_tier), 5, 1, kTierNames, NEED_RANDOM},
    {ROW_OPEN, "Pick Rivals", SET_RIVALS, 0, 0, 0, NULL, NEED_MANUAL},
    {ROW_CHOICE, "Win By", 0, SET_FIELD(fp_win), 3, 1, kWinNames, 0},
    {ROW_CHOICE, "Laps", 0, SET_FIELD(fp_laps), 11, 1, kLapNames, 0},
    {ROW_CHOICE, "Arrow", 0, SET_FIELD(fp_arrow), 2, 1, kOnOff, 0},
    {ROW_CHOICE, "Specials", 0, SET_FIELD(fp_specials), 2, 1, kOnOff, 0},
    {ROW_START, "Start Race", 0, 0, 0, 0, NULL, 0},
    {ROW_BACK, "Back", 0, 0, 0, 0, NULL, false}}},
  [SET_RIVALS] = {"RACE SETUP - RIVALS", 20, {
    {ROW_CAR, "Rival 1", 0, SET_FIELD(fp_rival[0]), 0, 0, NULL, 0},
    {ROW_CAR, "Rival 2", 1, SET_FIELD(fp_rival[1]), 0, 0, NULL, 0},
    {ROW_CAR, "Rival 3", 2, SET_FIELD(fp_rival[2]), 0, 0, NULL, 0},
    {ROW_CAR, "Rival 4", 3, SET_FIELD(fp_rival[3]), 0, 0, NULL, 0},
    {ROW_CAR, "Rival 5", 4, SET_FIELD(fp_rival[4]), 0, 0, NULL, 0},
    {ROW_CAR, "Rival 6", 5, SET_FIELD(fp_rival[5]), 0, 0, NULL, 0},
    {ROW_CAR, "Rival 7", 6, SET_FIELD(fp_rival[6]), 0, 0, NULL, 0},
    {ROW_CAR, "Rival 8", 7, SET_FIELD(fp_rival[7]), 0, 0, NULL, 0},
    {ROW_CAR, "Rival 9", 8, SET_FIELD(fp_rival[8]), 0, 0, NULL, 0},
    {ROW_CAR, "Rival 10", 9, SET_FIELD(fp_rival[9]), 0, 0, NULL, 0},
    {ROW_CAR, "Rival 11", 10, SET_FIELD(fp_rival[10]), 0, 0, NULL, 0},
    {ROW_CAR, "Rival 12", 11, SET_FIELD(fp_rival[11]), 0, 0, NULL, 0},
    {ROW_CAR, "Rival 13", 12, SET_FIELD(fp_rival[12]), 0, 0, NULL, 0},
    {ROW_CAR, "Rival 14", 13, SET_FIELD(fp_rival[13]), 0, 0, NULL, 0},
    {ROW_CAR, "Rival 15", 14, SET_FIELD(fp_rival[14]), 0, 0, NULL, 0},
    {ROW_CAR, "Rival 16", 15, SET_FIELD(fp_rival[15]), 0, 0, NULL, 0},
    {ROW_CAR, "Rival 17", 16, SET_FIELD(fp_rival[16]), 0, 0, NULL, 0},
    {ROW_CAR, "Rival 18", 17, SET_FIELD(fp_rival[17]), 0, 0, NULL, 0},
    {ROW_CAR, "Rival 19", 18, SET_FIELD(fp_rival[18]), 0, 0, NULL, 0},
    {ROW_BACK, "Back", 0, 0, 0, 0, NULL, false}}},
  [SET_CONTROLS] = {"SETTINGS - CONTROLS", 13, {
    {ROW_CHOICE, "Steer and Stunt With", 0, SET_FIELD(steer_dpad), 2, 1, kSteerNames, false},
    {ROW_CHOICE, "Accelerate", 0, SET_FIELD(bind[BIND_ACCEL]), PAD_COUNT, 1, kPadNames, false},
    {ROW_CHOICE, "Brake / Reverse", 0, SET_FIELD(bind[BIND_BRAKE]), PAD_COUNT, 1, kPadNames, false},
    {ROW_CHOICE, "Handbrake / Stunt", 0, SET_FIELD(bind[BIND_HANDB]), PAD_COUNT, 1, kPadNames, false},
    {ROW_CHOICE, "Change View", 0, SET_FIELD(bind[BIND_VIEW]), PAD_COUNT, 1, kPadNames, false},
    {ROW_CHOICE, "Guidance Arrow", 0, SET_FIELD(bind[BIND_ARRACE]), PAD_COUNT, 1, kPadNames, false},
    {ROW_CHOICE, "Map", 0, SET_FIELD(bind[BIND_RADAR]), PAD_COUNT, 1, kPadNames, false},
    {ROW_CHOICE, "Pause", 0, SET_FIELD(bind[BIND_PAUSE]), PAD_COUNT, 1, kPadNames, false},
    {ROW_CHOICE, "Mute Music", 0, SET_FIELD(bind[BIND_MUSIC]), PAD_COUNT, 1, kPadNames, false},
    {ROW_CHOICE, "Mute Effects", 0, SET_FIELD(bind[BIND_SFX]), PAD_COUNT, 1, kPadNames, false},
    {ROW_CHOICE, "Special", 0, SET_FIELD(bind[BIND_SPECIAL]), PAD_COUNT, 1, kPadNames, false},
    {ROW_CHOICE, "Car List Bars", 0, SET_FIELD(bind[BIND_LISTBARS]), PAD_COUNT, 1, kPadNames, false},
    {ROW_BACK, "Back", 0, 0, 0, 0, NULL, false}}},
};
#undef SET_FIELD

typedef struct {
  int32_t page, row;
  bool armed;       // Reset RPG Mode pressed once: the next press erases it
  int32_t notice;   // frames left of "RPG Mode reset." in the hint line
} SettingsUi;

// A touch's target: what it selects (sel = sel_value) and the button it
// presses on release (BTN_COUNT: none).
typedef struct { int32_t *sel; int32_t sel_value; Button btn; } TouchHit;
typedef enum {
  SETTINGS_STAY, SETTINGS_EXIT, SETTINGS_BENCH, SETTINGS_STRESS, SETTINGS_CAREER_RESET, SETTINGS_START
} SettingsAction;

static int32_t *settings_field(GameSettings *s, const SettingsRow *r) {
  return (int32_t *)((char *)s + r->off);
}

// Settings opened from the pause menu: the Performance Test (a race of its
// own) is not offered there.
static bool g_settings_in_race;

static bool settings_row_shown(const SettingsRow *r, const GameSettings *s, bool has_rumble) {
  if ((r->kind == ROW_BENCH || r->kind == ROW_CAREER_RESET) && g_settings_in_race) return false;
  if ((r->needs & NEED_RUMBLE) && !has_rumble) return false;
  if ((r->needs & NEED_RANDOM) && s->fp_pick != 0) return false;
  if ((r->needs & NEED_MANUAL) && s->fp_pick != 1) return false;
  if (r->kind == ROW_CAR && r->page > s->fp_opponents) return false;   // past the rivals raced
  return !(r->needs & NEED_REMAP) || HAS_REMAP;
}

// A page shows at most this many rows; a longer one (the rivals) scrolls.
#define SETTINGS_VISIBLE 13
#define SETTINGS_VISIBLE_RIVALS 5   // the rivals' rows are tall: each shows its car

/** A page's row height, gap, text offset and font size for `nvis` rows. */
static void settings_metrics(int32_t page, int32_t nvis, int32_t *h, int32_t *gap, int32_t *ty, int32_t *font) {
  if (page == SET_RIVALS) {
    *h = 58; *gap = 6; *ty = 7; *font = 18;
    return;
  }
  const bool tight = nvis > 8, tighter = nvis > 11;
  *h = tighter ? 22 : (tight ? 28 : 34);
  *gap = tighter ? 4 : (tight ? 4 : 6);
  *ty = tighter ? 5 : (tight ? 6 : 7);
  *font = tight ? (tighter ? 14 : 16) : 18;
}

/** The rows the page draws, top to bottom, scrolled to keep the selected
 * one in view; how many. */
static int32_t settings_window(const SettingsUi *ui, const GameSettings *s, bool has_rumble, int32_t *rows) {
  const SettingsPage *pg = &kSettingsPages[ui->page];
  int32_t all[SETTINGS_MAX_ROWS], n = 0, at = 0;
  for (int32_t r = 0; r < pg->nrows; r++) {
    if (!settings_row_shown(&pg->rows[r], s, has_rumble)) continue;
    if (r == ui->row) at = n;
    all[n++] = r;
  }
  int32_t first = 0;
  const int32_t most = ui->page == SET_RIVALS ? SETTINGS_VISIBLE_RIVALS : SETTINGS_VISIBLE;
  if (n > most) {
    first = at - most / 2;
    if (first < 0) first = 0;
    if (first > n - most) first = n - most;
    n = most;
  }
  for (int32_t i = 0; i < n; i++) rows[i] = all[first + i];
  return n;
}

/** The row of the current page drawn across game-space `y`, or -1 (the
 * layout of settings_screen_draw, for touch). */
static int32_t settings_row_at(const SettingsUi *ui, const GameSettings *s, bool has_rumble, int32_t y) {
  int32_t rows[SETTINGS_MAX_ROWS];
  const int32_t n = settings_window(ui, s, has_rumble, rows);
  int32_t h, gap, ty, fs;
  settings_metrics(ui->page, n, &h, &gap, &ty, &fs);
  int32_t top = 68;
  for (int32_t i = 0; i < n; i++) {
    if (y >= top && y < top + h + gap) return rows[i];
    top += h + gap;
  }
  return -1;
}

/** A car a rival can race: any but the custom car's slot (the player's own). */
static int32_t settings_step_car(int32_t c, int32_t dir) {
  const int32_t last = car_last_index(false);
  int32_t n = c + dir;
  if (n == CUSTOM_CAR_INDEX) n += dir;
  return (n < 0 || n > last) ? c : n;
}

/** Whether a Controls row's button is also another action's. */
static bool settings_bind_clash(const GameSettings *s, const SettingsRow *r) {
  if (r->names != kPadNames) return false;
  const int32_t *v = (const int32_t *)((const char *)s + r->off);
  for (int32_t i = 0; i < BIND_COUNT; i++) {
    if (&s->bind[i] != v && s->bind[i] == *v) return true;
  }
  return false;
}

/** Steering moved between the left stick and the D-pad: whatever was on the
 * other one's directions (the arrow and the map, by default) moves with it,
 * so nothing ends up under the thumb that steers. */
static void settings_swap_steer(GameSettings *s) {
  for (int32_t i = 0; i < BIND_COUNT; i++) {
    int32_t *b = &s->bind[i];
    if (*b >= PAD_DPAD_UP && *b < PAD_STICK_UP) *b += PAD_STICK_UP - PAD_DPAD_UP;
    else if (*b >= PAD_STICK_UP && *b < PAD_COUNT) *b -= PAD_STICK_UP - PAD_DPAD_UP;
  }
}

/** One frame of Settings input. Changes go straight into `s`; the caller
 * applies them (apply_settings) and saves on SETTINGS_EXIT. */
static SettingsAction settings_screen_input(SettingsUi *ui, GameSettings *s, int32_t default_graphics,
                                            bool up, bool down, bool left, bool right, bool ok, bool back,
                                            bool has_rumble) {
  const SettingsPage *pg = &kSettingsPages[ui->page];
  const int32_t n = pg->nrows;
  if (ui->notice > 0) ui->notice--;
  if (down || up || left || right || back) ui->armed = false;
  if (down || up) {
    int32_t r = ui->row;
    do { r = (r + (down ? 1 : n - 1)) % n; } while (!settings_row_shown(&pg->rows[r], s, has_rumble));
    ui->row = r;
  }
  const SettingsRow *row = &pg->rows[ui->row];
  if (row->kind == ROW_CAR && (left || right)) {
    int32_t *v = settings_field(s, row);
    *v = settings_step_car(*v, right ? 1 : -1);
  }
  if (row->kind == ROW_CHOICE && (left || right)) {
    int32_t *v = settings_field(s, row);
    int32_t i = *v / row->step + (right ? 1 : -1);
    if (i >= 0 && i < row->count) {
      *v = i * row->step;
      if (v == &s->steer_dpad) settings_swap_steer(s);
    }
  }
  if (ok) {
    if (row->kind == ROW_OPEN) {
      ui->page = row->page;
      ui->row = 0;
    } else if (row->kind == ROW_RESET) {
      const int32_t language = s->language;   // the defaults are not a language change
      *s = game_settings_defaults(default_graphics);
      s->language = language;
    } else if (row->kind == ROW_BACK) {
      back = true;
    } else if (row->kind == ROW_START) {
      return SETTINGS_START;
    } else if (row->kind == ROW_BENCH) {
      return row->page ? SETTINGS_STRESS : SETTINGS_BENCH;
    } else if (row->kind == ROW_CAREER_RESET) {
      // Twice to erase: the first press asks.
      if (!ui->armed) {
        ui->armed = true;
      } else {
        ui->armed = false;
        ui->notice = 120;
        return SETTINGS_CAREER_RESET;
      }
    }
  }
  if (back) {
    if (ui->page == SET_MAIN || ui->page == SET_RACE) return SETTINGS_EXIT;
    // Back to the page that opened this one, on the row that opened it.
    const int32_t parent = ui->page == SET_RIVALS ? SET_RACE : SET_MAIN;
    const SettingsPage *main_pg = &kSettingsPages[parent];
    for (int32_t r = 0; r < main_pg->nrows; r++) {
      if (main_pg->rows[r].kind == ROW_OPEN && main_pg->rows[r].page == ui->page) ui->row = r;
    }
    ui->page = parent;
  }
  return SETTINGS_STAY;
}

// The launcher's palette.
#define SET_BG       232, 228, 216
#define SET_BG_DARK  217, 213, 201
#define SET_INK      40, 40, 40
#define SET_YELLOW   245, 208, 0

/** `poly` (n points) clipped to xmin <= x <= xmax; writes to out, returns its size. */
static int32_t clip_poly_x(const int32_t *px, const int32_t *py, int32_t n, int32_t xmin, int32_t xmax,
                           int32_t *ox, int32_t *oy) {
  int32_t ax[16], ay[16], an = 0;
  for (int32_t pass = 0; pass < 2; pass++) {
    const int32_t *ix = pass ? ax : px, *iy = pass ? ay : py;
    int32_t in = pass ? an : n, cnt = 0;
    int32_t *wx = pass ? ox : ax, *wy = pass ? oy : ay;
    for (int32_t i = 0; i < in; i++) {
      const int32_t x0 = ix[i], y0 = iy[i], x1 = ix[(i + 1) % in], y1 = iy[(i + 1) % in];
      const bool in0 = pass ? x0 <= xmax : x0 >= xmin, in1 = pass ? x1 <= xmax : x1 >= xmin;
      const int32_t edge = pass ? xmax : xmin;
      if (in0) { wx[cnt] = x0; wy[cnt] = y0; cnt++; }
      if (in0 != in1 && x1 != x0) {
        wx[cnt] = edge;
        wy[cnt] = y0 + (y1 - y0) * (edge - x0) / (x1 - x0);
        cnt++;
      }
    }
    if (pass) return cnt;
    an = cnt;
  }
  return 0;
}

/** The yellow-and-black hazard stripe in (x, y, w, h). */
static void draw_hazard(Graphics2D *g, int32_t x, int32_t y, int32_t w, int32_t h) {
  gfx_set_color(g, SET_YELLOW);
  gfx_fill_rect(g, x, y, w, h);
  gfx_set_color(g, 17, 17, 17);
  for (int32_t k = -h; k < w; k += 12) {
    const int32_t px[4] = {x + k, x + k + 6, x + k + 6 + h, x + k + h};
    const int32_t py[4] = {y + h, y + h, y, y};
    int32_t cx[16], cy[16];
    const int32_t cn = clip_poly_x(px, py, 4, x, x + w, cx, cy);
    if (cn >= 3) gfx_fill_polygon(g, cx, cy, cn);
  }
}

static void draw_settings_arrow(Graphics2D *g, int32_t x, int32_t cy, bool right, bool lit, bool selected) {
  if (!lit) gfx_set_color(g, 150, 146, 136);
  else if (selected) gfx_set_color(g, SET_YELLOW);
  else gfx_set_color(g, SET_INK);
  const int32_t xs[3] = {x, right ? x + 9 : x - 9, x};
  const int32_t ys[3] = {cy - 6, cy, cy + 6};
  gfx_fill_polygon(g, xs, ys, 3);
}

static void settings_screen_draw(Graphics2D *g, const SettingsUi *ui, const GameSettings *s, bool has_rumble) {
  // Beige with darker vertical stripes, over the whole 800x450.
  gfx_set_color(g, SET_BG);
  gfx_fill_rect(g, 0, 0, 800, 450);
  gfx_set_color(g, SET_BG_DARK);
  for (int32_t x = 60; x < 800; x += 120) gfx_fill_rect(g, x, 0, 60, 450);

  const SettingsPage *pg = &kSettingsPages[ui->page];
  gfx_set_color(g, 17, 17, 17);
  gfx_fill_rect(g, 120, 22, 560, 32);
  gfx_set_color(g, SET_YELLOW);
  font_set(FONT_BOLD, 22);
  draw_centered(g, pg->title, 400, 46);
  font_set(FONT_BOLD, 18);

  // A page longer than eight rows (Controls) packs them tighter, and
  // tighter still past eleven; past SETTINGS_VISIBLE it scrolls.
  int32_t rows[SETTINGS_MAX_ROWS];
  const int32_t nvis = settings_window(ui, s, has_rumble, rows);
  int32_t h, gap, ty, fs;
  settings_metrics(ui->page, nvis, &h, &gap, &ty, &fs);
  const int32_t x0 = 120, w = 560;
  font_set(FONT_BOLD, fs);
  int32_t y = 68;
  for (int32_t i = 0; i < nvis; i++) {
    const int32_t r = rows[i];
    const SettingsRow *row = &pg->rows[r];
    const bool sel = r == ui->row;
    gfx_set_color(g, 17, 17, 17);
    gfx_fill_rect(g, x0, y, w, h);                       // the 3px black border...
    if (!sel) {
      gfx_set_color(g, SET_BG);
      gfx_fill_rect(g, x0 + 3, y + 3, w - 6, h - 6);     // ...around a beige face
    } else {
      draw_hazard(g, x0 + 3, y + 3, 18, h - 6);
    }
    const int32_t cy = y + h / 2;
    if (sel) gfx_set_color(g, SET_YELLOW); else gfx_set_color(g, SET_INK);
    if (row->kind == ROW_CAR) {
      // A rival, lobby style: who over which car, the car itself drawn on
      // the right by the caller (draw_car_thumb), between the arrows.
      const int32_t v = *(const int32_t *)((const char *)s + row->off);
      font_set(FONT_BOLD, 18);
      font_draw(g, row->label, x0 + 34, y + 24);
      font_set(FONT_BOLD, 14);
      font_draw(g, car_name(v), x0 + 34, y + 46);
      font_set(FONT_BOLD, fs);
      draw_settings_arrow(g, x0 + 290, cy, false, settings_step_car(v, -1) != v, sel);
      draw_settings_arrow(g, x0 + w - 26, cy, true, settings_step_car(v, 1) != v, sel);
      y += h + gap;
      continue;
    }
    if (row->kind == ROW_CAREER_RESET && sel && ui->armed) {
      gfx_set_color(g, 255, 80, 60);
      font_draw(g, "Press again: erase all RPG progress", x0 + 34, cy + ty);
    } else {
      font_draw(g, row->label, x0 + 34, cy + ty);
    }
    if (row->kind == ROW_OPEN || row->kind == ROW_BENCH || row->kind == ROW_START) {
      draw_settings_arrow(g, x0 + w - 26, cy, true, true, sel);
    } else if (row->kind == ROW_CHOICE) {
      const int32_t v = *(const int32_t *)((const char *)s + row->off);
      const int32_t i = v / row->step;
      char num[16];
      const char *name = row->names ? row->names[i] : (snprintf(num, sizeof(num), "%d", (int)v), num);
      char with_icon[40];
      if (row->names == kPadNames) {
        snprintf(with_icon, sizeof(with_icon), "%s %s", kPadIcons[i], tr(name));
        name = with_icon;
      }
      const int32_t tw = font_width(name);
      const int32_t right_x = x0 + w - 26;
      if (settings_bind_clash(s, row)) gfx_set_color(g, 220, 40, 30);
      else if (sel) gfx_set_color(g, SET_YELLOW);
      else gfx_set_color(g, SET_INK);
      font_draw(g, name, right_x - 14 - tw, cy + ty);
      draw_settings_arrow(g, right_x - 22 - tw, cy, false, i > 0, sel);
      draw_settings_arrow(g, right_x, cy, true, i < row->count - 1, sel);
    }
    y += h + gap;
  }

  gfx_set_color(g, 90, 88, 80);
  char hint[96];
  if (ui->notice > 0) snprintf(hint, sizeof(hint), "RPG Mode reset: back to stage 1.");
  else snprintf(hint, sizeof(hint), "Left/Right: Change    %s: Select    %s: Back", KEY_CONTINUE, KEY_BACK);
  font_set(FONT_BOLD, 12);
  draw_centered(g, hint, 400, 428);
}

// --- Settings > Performance Test ---------------------------------------
// A fixed race the AI drives for you (same stage, car and seed every run,
// so two reports compare), measured for BENCH_SECONDS from the green
// light: every frame's length and where its time went. The summary is
// shown at the end and the full report written beside the save, to be
// read on a PC.
#define BENCH_STAGE 9          // a big stage; the whole field is in view at the start
#define BENCH_CAR 15           // Dr Monstaa: hard to wreck, so the race runs the full time
#define BENCH_SEED 9001
#define BENCH_SECONDS 60
#define BENCH_MAX_FRAMES 8192  // 60 s at 120 fps

typedef struct {
  uint32_t interval_us;                      // this frame start to the next
  uint32_t update_us, draw_us, submit_us, swap_us;
  int32_t objs, faces, verts;
} BenchFrame;

typedef struct {
  bool active;      // the test race is on (loading, intro, racing)
  bool measuring;   // past the green light
  uint64_t start_us, last_us;
  int32_t n;
  BenchFrame *f;
  bool ended_early; // the race ended (wasted / finished) before the time was up
  bool stress;      // the Stress Test: Extended's biggest stage, a full field (NFM_MAX_CARS)
  char lines[14][80];
  int32_t nlines;
  char path[1024];
  bool saved;
} Bench;

static int cmp_u32(const void *a, const void *b) {
  const uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
  return x < y ? -1 : x > y;
}

static void bench_line(FILE *f, const char *fmt, ...) {
  char buf[160];
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);
  if (f) fprintf(f, "%s\n", buf);
}

/** The numbers, to the report file and (the headline ones) to b->lines. */
static void bench_report(Bench *b, const GameSettings *s, int32_t disp_w, int32_t disp_h,
                         int32_t px_w, int32_t px_h, const char *stage_name, const char *progress_path) {
  b->nlines = 0;
  b->saved = false;
  const int32_t n = b->n;
  if (n < 2) {
    snprintf(b->lines[b->nlines++], 80, "NOT ENOUGH FRAMES MEASURED");
    return;
  }
  uint32_t *iv = malloc(sizeof(uint32_t) * (size_t)n);
  double sum = 0, su = 0, sd = 0, ss = 0, sw = 0, so = 0, sf = 0, sv = 0;
  int32_t over17 = 0, over33 = 0, maxv = 0;
  for (int32_t i = 0; i < n; i++) {
    const BenchFrame *fr = &b->f[i];
    iv[i] = fr->interval_us;
    sum += fr->interval_us;
    su += fr->update_us; sd += fr->draw_us; ss += fr->submit_us; sw += fr->swap_us;
    so += fr->objs; sf += fr->faces; sv += fr->verts;
    if (fr->verts > maxv) maxv = fr->verts;
    if (fr->interval_us > 20000) over17++;
    if (fr->interval_us > 33400) over33++;
  }
  qsort(iv, (size_t)n, sizeof(uint32_t), cmp_u32);
  const double avg_fps = (double)n * 1e6 / sum;
  // 1% low: the frame rate of the slowest 1% of frames.
  const int32_t k = n / 100 > 0 ? n / 100 : 1;
  double worst_sum = 0;
  for (int32_t i = n - k; i < n; i++) worst_sum += iv[i];
  const double low1 = (double)k * 1e6 / worst_sum;
  const double p50 = iv[n / 2] / 1000.0, p95 = iv[n * 95 / 100] / 1000.0, p99 = iv[n * 99 / 100] / 1000.0,
               pmax = iv[n - 1] / 1000.0;
  const double work = (su + sd + ss + sw) / n / 1000.0;

  // The report file, beside the save.
  snprintf(b->path, sizeof(b->path), "%s", progress_path ? progress_path : "benchmark.txt");
  char *slash = strrchr(b->path, '/');
  if (slash) snprintf(slash + 1, sizeof(b->path) - (size_t)(slash + 1 - b->path), "benchmark.txt");
  else snprintf(b->path, sizeof(b->path), "benchmark.txt");
  if (progress_path) game_settings_save(progress_path, s); // creates the directory if it is missing
  FILE *f = progress_path ? fopen(b->path, "w") : NULL;
  b->saved = f != NULL;

  static const char *const kOnOffR[] = {"off", "on"};
  bench_line(f, "NFM performance report (build %s %s)", __DATE__, __TIME__);
  bench_line(f, "display %dx%d, drawn at %dx%d", disp_w, disp_h, px_w, px_h);
  bench_line(f, "settings: image %s, draw distance %s, scenery %s, shadows %s, particles %s, blur %d, smooth frames %s",
             kQualityNames[s->graphics], kDistNames[s->draw_dist], kDetailNames[s->detail], kOnOffR[s->shadows],
             kOnOffR[s->particles], s->blur, kOnOffR[s->smooth]);
  bench_line(f, "stage %d (%s), car %d, seed %d", BENCH_STAGE, stage_name, BENCH_CAR, BENCH_SEED);
  bench_line(f, "measured %.1f s, %d frames%s", sum / 1e6, n, b->ended_early ? " (race ended early)" : "");
  bench_line(f, "");
  bench_line(f, "fps: average %.1f, 1%% low %.1f", avg_fps, low1);
  bench_line(f, "frame ms: median %.2f, p95 %.2f, p99 %.2f, worst %.2f", p50, p95, p99, pmax);
  bench_line(f, "late frames (over 20 ms): %d (%.1f%%), over 33.4 ms (a 30 fps frame): %d", over17, 100.0 * over17 / n, over33);
  bench_line(f, "work ms per frame (avg): logic %.2f, draw %.2f, gl %.2f, swap %.2f = %.2f",
             su / n / 1000.0, sd / n / 1000.0, ss / n / 1000.0, sw / n / 1000.0, work);
  bench_line(f, "scene per frame (avg): %.0f objects, %.0f faces, %.0f vertices (max %d)", so / n, sf / n, sv / n, maxv);
  bench_line(f, "");
  bench_line(f, "per second (fps/worst ms):");
  {
    char row[256] = "";
    double t = 0;
    int32_t frames = 0, sec = 0;
    uint32_t worst = 0;
    for (int32_t i = 0; i < n; i++) {
      t += b->f[i].interval_us;
      frames++;
      if (b->f[i].interval_us > worst) worst = b->f[i].interval_us;
      if (t >= (sec + 1) * 1e6 || i == n - 1) {
        char cell[24];
        snprintf(cell, sizeof(cell), " %d/%.0f", frames, worst / 1000.0);
        strncat(row, cell, sizeof(row) - strlen(row) - 1);
        frames = 0;
        worst = 0;
        if (++sec % 10 == 0 || i == n - 1) {
          bench_line(f, " %3ds:%s", (sec - 1) / 10 * 10, row);
          row[0] = 0;
        }
      }
    }
  }
  bench_line(f, "");
  bench_line(f, "slowest frames (total = logic + draw + gl + swap ms, faces / vertices, when):");
  {
    bool *used = calloc((size_t)n, sizeof(bool));
    for (int32_t w = 0; w < 8 && w < n; w++) {
      int32_t wi = -1;
      for (int32_t i = 0; i < n; i++) {
        if (!used[i] && (wi < 0 || b->f[i].interval_us > b->f[wi].interval_us)) wi = i;
      }
      used[wi] = true;
      double t = 0;
      for (int32_t i = 0; i < wi; i++) t += b->f[i].interval_us;
      const BenchFrame *fr = &b->f[wi];
      bench_line(f, "  %.2f = %.2f + %.2f + %.2f + %.2f, %d / %d, at %.1f s", fr->interval_us / 1000.0,
                 fr->update_us / 1000.0, fr->draw_us / 1000.0, fr->submit_us / 1000.0, fr->swap_us / 1000.0,
                 fr->faces, fr->verts, t / 1e6);
    }
    free(used);
  }
  if (f) fclose(f);

  snprintf(b->lines[b->nlines++], 80, "AVERAGE %.1f FPS   1%% LOW %.1f FPS", avg_fps, low1);
  snprintf(b->lines[b->nlines++], 80, "FRAME MS  MEDIAN %.1f  P99 %.1f  WORST %.1f", p50, p99, pmax);
  snprintf(b->lines[b->nlines++], 80, "LATE FRAMES  OVER 20 MS %d (%.1f%%)  OVER 33 MS %d", over17, 100.0 * over17 / n, over33);
  snprintf(b->lines[b->nlines++], 80, "WORK MS  LOGIC %.1f  DRAW %.1f  GL %.1f  SWAP %.1f", su / n / 1000.0, sd / n / 1000.0,
           ss / n / 1000.0, sw / n / 1000.0);
  snprintf(b->lines[b->nlines++], 80, "SCENE  %.0f FACES  %.0f VERTICES", sf / n, sv / n);
  snprintf(b->lines[b->nlines++], 80, "%dX%d  %.0f S  %d FRAMES%s", px_w, px_h, sum / 1e6, n, b->ended_early ? "  (RACE ENDED)" : "");
  free(iv);
}

static void bench_result_draw(Graphics2D *g, const Bench *b) {
  gfx_set_color(g, SET_BG);
  gfx_fill_rect(g, 0, 0, 800, 450);
  gfx_set_color(g, SET_BG_DARK);
  for (int32_t x = 60; x < 800; x += 120) gfx_fill_rect(g, x, 0, 60, 450);
  gfx_set_color(g, 17, 17, 17);
  gfx_fill_rect(g, 80, 22, 640, 32);
  gfx_set_color(g, SET_YELLOW);
  font_set(FONT_BOLD, 22);
  draw_centered(g, b->stress ? "STRESS TEST" : "PERFORMANCE TEST", 400, 46);
  font_set(FONT_BOLD, 18);
  gfx_set_color(g, 17, 17, 17);
  gfx_fill_rect(g, 80, 70, 640, 34 * b->nlines + 20);
  gfx_set_color(g, SET_BG);
  gfx_fill_rect(g, 83, 73, 634, 34 * b->nlines + 14);
  for (int32_t i = 0; i < b->nlines; i++) {
    gfx_set_color(g, SET_INK);
    draw_centered(g, b->lines[i], 400, 96 + 34 * i);
  }
  gfx_set_color(g, 90, 88, 80);
  char hint[1100];
  if (b->saved) snprintf(hint, sizeof(hint), "FULL REPORT: %s", b->path);
  else snprintf(hint, sizeof(hint), "THE REPORT FILE COULD NOT BE WRITTEN");
  font_set(FONT_BOLD, 12);
  draw_centered(g, hint, 400, 386);
  draw_centered(g, KEY_CONTINUE ": Back to the menu", 400, 424);
}

/** Puts the Settings that act on the engine into effect (Image Quality is
 * done by the caller: it rebuilds the render targets). */
static void apply_settings(const GameSettings *s, Medium *m) {
  static const int32_t kDistPercent[3] = {100, 150, 200};
  medium_draw_distance(m, kDistPercent[s->draw_dist]);
  g_resdown_setting = s->detail ? 2 : 0;
  m->resdown = g_resdown_setting;
  cont_o_shadows = s->shadows != 0;
  cont_o_particles = s->particles != 0;
  i18n_set_lang(s->language);
  audio_sfx_gain = (float)s->sfx_vol / 100.0f;
  radical_music_gain = (float)s->music_vol / 100.0f;
#ifdef NFM_TARGET_SWITCH
  uint64_t mask[BIND_COUNT];
  for (int32_t i = 0; i < BIND_COUNT; i++) mask[i] = 1ULL << kPadBits[s->bind[i]];
  input_configure(mask, s->steer_dpad != 0);
  g_key_settings = s;
#endif
}

/** 800x450 RGBA from a `w`x`h` glReadPixels frame (both bottom-up), nearest. */
static void downsample_to_game(const uint8_t *src, int32_t w, int32_t h, uint8_t *dst) {
  for (int32_t y = 0; y < 450; y++) {
    const uint8_t *row = src + (size_t)(y * h / 450) * (size_t)w * 4;
    for (int32_t x = 0; x < 800; x++) {
      memcpy(dst + ((size_t)y * 800 + (size_t)x) * 4, row + (size_t)(x * w / 800) * 4, 4);
    }
  }
}

// Extended's specials on the race HUD (its xtGraphics.java:4271-4302,
// 6393-6480, 7149-7213), moved from its 870-wide screen to this 800 one:
// the Special bar under Power, the status lines down the left, and the
// player's boosts and cuts as tabs above the bottom-left corner.
static void draw_specials_hud(Graphics2D *g, Medium *m, const Specials *sp, const Mad *mads, int32_t nplayers,
                              const int32_t *sc, const HudImg *bar) {
  draw_hud_img(g, *bar, 600, 47);
  const bool running = sp->fixspecials[0];
  int32_t red = (int32_t)((running ? 85.0f : 220.0f) * (1.0f + (float)m->snap[0] / 100.0f));
  if (red > 255) red = 255;
  if (red < 0) red = 0;
  const float fill = running ? mads[0].speclast : mads[0].spatk;
  gfx_set_color(g, red, 0, 0);
  gfx_fill_rect(g, 662, 51, (int32_t)(98.0f * (fill / 120.0f)), 8);

  // "X activated its special!" and the conditions it put on cars.
  font_set(FONT_BOLD, 11);
  gfx_set_composite(g, (float)sp->xfade / 255.0f);
  for (int32_t a = 0; a < nplayers; a++) {
    if (mads[a].dest) continue;
    const char *name = car_name(sc[a]);
    char line[96];
    if (sp->over[0][a] && sp->xm[0][a] < 199) {
      if (a == 0) snprintf(line, sizeof(line), "You activated your special!");
      else snprintf(line, sizeof(line), "%s activated its special!", name);
      gfx_set_color(g, 100, 0, 0);
      font_draw(g, line, 12, sp->xm[0][a]);
    }
    if (sp->over[1][a] && sp->xm[1][a] < 199) {
      char by[16] = "";
      if (sp->fixspecials[0] && sp->randomcar[0] == a)
        snprintf(by, sizeof(by), " (-%d%%)", (int32_t)round(100.0 - sp->statreduce[a][0] * 100.0));
      if (a == 0) snprintf(line, sizeof(line), "You have reduced speed!%s", by);
      else snprintf(line, sizeof(line), "%s has reduced speed!%s", name, by);
      gfx_set_color(g, 0, 0, 100);
      font_draw(g, line, 12, sp->xm[1][a]);
    }
    if (sp->over[2][a] && sp->xm[2][a] < 199) {
      const int32_t o = sp->strswapee[a];
      const char *object = o == 0 ? "you" : (car_name(sc[o]));
      snprintf(line, sizeof(line), "%s swapped strength with %s!", a == 0 ? "You have" : name, object);
      gfx_set_color(g, 0, 50, 0);
      font_draw(g, line, 12, sp->xm[2][a]);
    }
    if (sp->over[3][a] && sp->xm[3][a] < 199) {
      char by[16] = "";
      if (sp->fixspecials[0] && sp->randomcar[0] == a)
        snprintf(by, sizeof(by), " (-%d%%)", (int32_t)round(sp->statreduce[a][5] * 100.0));
      if (a == 0) snprintf(line, sizeof(line), "You have reduced defence!%s", by);
      else snprintf(line, sizeof(line), "%s has reduced defence!%s", name, by);
      gfx_set_color(g, 180, 90, 0);
      font_draw(g, line, 12, sp->xm[3][a]);
    }
    if (sp->over[4][a] && sp->xm[4][a] < 199) {
      if (a == 0) snprintf(line, sizeof(line), "Your health is being drained!");
      else snprintf(line, sizeof(line), "%s's health is being drained!", name);
      gfx_set_color(g, 100, 75, 0);
      font_draw(g, line, 12, sp->xm[4][a]);
    }
  }
  gfx_set_composite(g, 1.0f);

  // The player's stats against its car's own: green tabs up, red down.
  if (mads[0].dest) return;
  static const char *const kStat[6] = {"spd", "acc", "con", "stu", "str", "def"};
  int32_t row = 0;
  for (int32_t k = 0; k < 6; k++) {
    const double mod = sp->statmod[0][k];
    if (mod == 100.0 || mod == 0.0) continue;
    const int32_t diff = abs((int32_t)mod - 100);
    const int32_t ext = diff >= 1000 ? 2 : (diff >= 100 ? 1 : 0);
    int32_t c = (int32_t)(120.0f + 120.0f * ((float)(mod < 100.0 ? m->snap[0] : m->snap[1]) / 100.0f));
    if (c > 255) c = 255;
    if (c < 0) c = 0;
    const int32_t y = 293 + row * 21;
    const int32_t xs[6] = {24, 97 + ext * 6, 101 + ext * 6, 97 + ext * 6, 24, 20};
    const int32_t ys[6] = {y, y, y + 9, y + 18, y + 18, y + 9};
    gfx_set_composite(g, 175.0f / 255.0f);
    if (mod < 100.0) gfx_set_color(g, c, 0, 0);
    else gfx_set_color(g, 0, c, 0);
    gfx_fill_polygon(g, xs, ys, 6);
    gfx_set_composite(g, 1.0f);
    char tab[32];
    snprintf(tab, sizeof(tab), "%s %d%% %s", mod < 100.0 ? "-" : "+", diff, tr(kStat[k]));
    gfx_set_color(g, 255, 255, 255);
    font_set(FONT_BOLD, 11);
    font_draw(g, tab, 28, y + 13);
    row++;
  }
}

// Extended's car list (its xtGraphics.stat, :3964-4229), moved 70 left from
// its 870-wide screen: under the Special bar, every car in race order with
// its name -- glowing in its condition's colour -- and a bar: damage while
// the arrow points at cars, power while it points at the track, and the
// special's charge after the List Bars button (its D).
// The car select's career questions.
enum { CC_RESHUFFLE = 1, CC_TRANSFER = 2, CC_SELL = 3, CC_TRANSFER_TO = 4 };

/** The car select's career panel (the Special button): three pages --
 * STATS (stat points into the six stats), PERKS (car points into the car's
 * six perks, after bonus stage 4) and CAR (reshuffle, level transfer,
 * sell) -- switched with Left / Right, rows with Up / Down. The original's
 * mouse screen (carselect's +-buttons, "extra stats", "change stats",
 * "sell car") laid out for a pad. */
static void draw_career_panel(Graphics2D *g, const CareerSave *s, int32_t ec, int32_t page, int32_t row,
                              const int32_t *pend) {
  static const char *const kStatNames[CS_N] = {"Top Speed", "Acceleration", "Handling", "Stunts", "Strength", "Endurance"};
  static const char *const kPages[3] = {"STATS", "PERKS", "CAR"};
  const int32_t x0 = 200, y0 = 100, w = 400, h = 244;   // clear of the special's box and the stat bars
  gfx_set_composite(g, 0.9f);
  gfx_set_color(g, 10, 14, 30);
  gfx_fill_rect(g, x0, y0, w, h);
  gfx_set_composite(g, 1.0f);
  gfx_set_color(g, 255, 196, 0);
  gfx_draw_rect(g, x0, y0, w, h);
  // The tabs.
  font_set(FONT_BOLD, 13);
  for (int32_t p = 0; p < 3; p++) {
    const int32_t tx = x0 + 10 + p * 128;
    if (p == page) {
      gfx_set_color(g, 255, 196, 0);
      gfx_fill_rect(g, tx, y0 + 8, 124, 22);
      gfx_set_color(g, 10, 14, 30);
    } else {
      gfx_set_color(g, 90, 90, 110);
      gfx_draw_rect(g, tx, y0 + 8, 124, 22);
      gfx_set_color(g, 170, 170, 190);
    }
    draw_centered(g, kPages[p], tx + 62, y0 + 24);
  }
  char line[96];
  const int32_t ry = y0 + 70;   // the first row's baseline
  // The car's level and experience on the left of every page.
  snprintf(line, sizeof(line), "Level %d   XP %d / %d", (int)s->level[ec], (int)s->exp[ec],
           (int)career_reqneed(s->level[ec], ec));
  font_set(FONT_BOLD, 12);
  gfx_set_color(g, 255, 196, 0);
  font_draw(g, line, x0 + 20, y0 + 50);
  font_set(FONT_BOLD, 13);
  if (page == 0) {
    snprintf(line, sizeof(line), "STAT POINTS:   %d", (int)s->statpoints[ec]);
    gfx_set_color(g, 255, 50, 0);
    font_draw(g, line, x0 + w - 20 - font_width(line), y0 + 50);
    font_set(FONT_BOLD, 12);
    for (int32_t k = 0; k < CS_N; k++) {
      const bool sel = k == row;
      gfx_set_color(g, sel ? 255 : 210, sel ? 196 : 210, sel ? 0 : 210);
      font_draw(g, kStatNames[k], x0 + 30, ry + k * 20);
      snprintf(line, sizeof(line), "%s%d", sel ? "> " : "", (int)s->sp[ec][k]);
      font_draw(g, line, x0 + 300, ry + k * 20);
      if (pend && pend[k] > 0) {
        snprintf(line, sizeof(line), "+%d", (int)pend[k]);
        gfx_set_color(g, 90, 220, 90);
        font_draw(g, line, x0 + 345, ry + k * 20);
      }
    }
    // What a point in the chosen stat does (career_apply_stats,
    // career_power_factor, specials' grip-scaled debuffs).
    static const char *const kStatHelp[CS_N][3] = {
        {"+1 top speed per point (+0.5 MPH).", "The MPH under the bars shows the new top speed.", ""},
        {"+0.1 acceleration per point, in every gear.", "Points above your level - 1 also add engine", "power (full power at 75 above)."},
        {"+0.2 grip per point: hold corners at speed.", "Points above your level - 1 sharpen steering", "and handbrake. Your debuffs hit harder too."},
        {"+1 air control and more flip power per point:", "faster spins and flips for bigger stunts.", ""},
        {"More weight per point: you deal more damage", "and push other cars around more.", ""},
        {"More health per point: you survive more hits", "before you are wasted.", ""},
    };
    font_set(FONT_BOLD, 11);
    gfx_set_color(g, 190, 200, 230);
    for (int32_t l = 0; l < 3; l++) font_draw(g, kStatHelp[row][l], x0 + 20, ry + 6 * 20 - 4 + l * 15);
  } else if (page == 1) {
    if (s->boncomp[3] > 0) {
      snprintf(line, sizeof(line), "CAR POINTS:   %d", (int)s->carpoints);
      gfx_set_color(g, 255, 50, 0);
    } else {
      snprintf(line, sizeof(line), "Win bonus stage 4 for car points");
      gfx_set_color(g, 170, 170, 170);
      font_set(FONT_BOLD, 11);
    }
    font_draw(g, line, x0 + w - 20 - font_width(line), y0 + 50);
    font_set(FONT_BOLD, 12);
    // Only the perks that work (career_perk_rows): v2.8's described-only
    // ones are left out.
    int32_t slots[CAREER_PERK_SLOTS];
    const int32_t nperk = career_perk_rows(ec, slots);
    if (nperk == 0) {
      gfx_set_color(g, 170, 170, 170);
      draw_centered(g, "This car has no perks.", 400, ry + 40);
    }
    for (int32_t r = 0; r < nperk; r++) {
      const int32_t k = slots[r];
      const int32_t p = career_statsalc[ec][k], v = s->perk[ec][k];
      const bool sel = r == row;
      const int32_t yy = ry + r * 20;
      gfx_set_color(g, sel ? 255 : 210, sel ? 196 : 210, sel ? 0 : 210);
      snprintf(line, sizeof(line), "%s%s", sel ? "> " : "", tr(career_perk_name[p]));
      font_draw(g, line, x0 + 30, yy);
      // Its points, 0-20, as a bar.
      gfx_set_color(g, 60, 60, 80);
      gfx_fill_rect(g, x0 + 200, yy - 9, 120, 9);
      gfx_set_color(g, 255, 150, 0);
      gfx_fill_rect(g, x0 + 200, yy - 9, 120 * v / CAREER_PERK_MAX, 9);
      gfx_set_color(g, 210, 210, 210);
      snprintf(line, sizeof(line), "%d / %d", (int)v, CAREER_PERK_MAX);
      font_draw(g, line, x0 + 330, yy);
      if (pend && pend[k] > 0) {
        snprintf(line, sizeof(line), "+%d", (int)pend[k]);
        gfx_set_color(g, 90, 220, 90);
        font_draw(g, line, x0 + 372, yy);
      }
    }
    // writeboosts' text for the chosen one.
    if (nperk > 0) {
      const int32_t p = career_statsalc[ec][slots[row < nperk ? row : 0]];
      const CareerPerkText *t = &career_perk_text[p];
      font_set(FONT_BOLD, 11);
      int32_t ty = ry + 6 * 20 - 2;
      char what[160];
      // A line at a time, as the dictionary has them.
      snprintf(what, sizeof(what), "%s %s %s", tr(career_perk_name[p]), tr(t->what[0]), t->what[1] ? tr(t->what[1]) : "");
      gfx_set_color(g, 235, 235, 235);
      if (font_width(what) > w - 16 && t->what[1]) {
        // Too wide for the panel (a translation): its two halves on two lines.
        snprintf(what, sizeof(what), "%s %s", tr(career_perk_name[p]), tr(t->what[0]));
        draw_centered(g, what, 400, ty);
        ty += 14;
        draw_centered(g, tr(t->what[1]), 400, ty);
      } else {
        draw_centered(g, what, 400, ty);
      }
      ty += 14;
      snprintf(what, sizeof(what), "%s %s", tr(t->max[0]), t->max[1] ? tr(t->max[1]) : "");
      gfx_set_color(g, 150, 240, 150);
      draw_centered(g, what, 400, ty);
    }
  } else {
    snprintf(line, sizeof(line), "Reshuffles: %d   Transfers: %d", (int)s->statchangers[0], (int)s->statchangers[1]);
    gfx_set_color(g, 200, 200, 200);
    font_set(FONT_BOLD, 12);
    font_draw(g, line, x0 + w - 20 - font_width(line), y0 + 50);
    const int32_t cost = career_reshuffle_cost(s, ec);
    char right[3][32];
    if (cost == 0) snprintf(right[0], sizeof(right[0]), "free");
    else snprintf(right[0], sizeof(right[0]), "costs %d stat points", (int)cost);
    if (s->statchangers[1] > 0) snprintf(right[1], sizeof(right[1]), "%d left", (int)s->statchangers[1]);
    else snprintf(right[1], sizeof(right[1]), "none left");
    if (s->boncomp[3] > 0) snprintf(right[2], sizeof(right[2]), "+%d car points", (int)career_sell_price(s, ec));
    else snprintf(right[2], sizeof(right[2]), "back to level 1");
    const char *names[3] = {"RESHUFFLE STATS", "LEVEL TRANSFER", s->boncomp[3] > 0 ? "SELL CAR" : "RESET CAR"};
    font_set(FONT_BOLD, 14);
    for (int32_t k = 0; k < 3; k++) {
      const bool sel = k == row, off = k == 1 && s->statchangers[1] == 0;
      const int32_t yy = ry + 4 + k * 30;
      if (sel) {
        gfx_set_color(g, 255, 196, 0);
        gfx_draw_rect(g, x0 + 20, yy - 18, w - 40, 26);
      }
      if (off) gfx_set_color(g, 120, 120, 120);
      else gfx_set_color(g, sel ? 255 : 220, sel ? 196 : 220, sel ? 0 : 220);
      font_draw(g, names[k], x0 + 32, yy);
      font_set(FONT_BOLD, 12);
      gfx_set_color(g, off ? 120 : 200, off ? 120 : 200, off ? 120 : 200);
      font_draw(g, right[k], x0 + 372 - font_width(right[k]), yy);
      font_set(FONT_BOLD, 14);
    }
    // carqhover's info boxes (XT 16300-16352).
    static const char *const kInfo[3][2] = {
        {"Unlocking a new stage gives you a free stat", "reshuffle, but costs stat points otherwise."},
        {"Unlocking a new stage lets you transfer your", "level and stat points to another car once."},
        {"The car goes back to level 1; its perks stay.", NULL}};
    font_set(FONT_BOLD, 11);
    gfx_set_color(g, 235, 235, 235);
    const int32_t r3 = row < 3 ? row : 0;
    draw_centered(g, kInfo[r3][0], 400, ry + 104);
    if (kInfo[r3][1]) draw_centered(g, kInfo[r3][1], 400, ry + 117);
    // The running totals (killscn / winscn, kills / wins) and what this
    // car's add to its XP (at full XP; an outgrown stage caps them lower).
    font_set(FONT_BOLD, 11);
    gfx_set_color(g, 255, 196, 0);
    snprintf(line, sizeof(line), "This car: %d wasted, %d cleared", (int)s->killscn[ec], (int)s->winscn[ec]);
    draw_centered(g, line, 400, ry + 130);
    snprintf(line, sizeof(line), "All cars: %d wasted, %d cleared", (int)s->kills, (int)s->wins);
    draw_centered(g, line, 400, ry + 142);
    int32_t wpct, cpct;
    career_count_bonus(s, ec, 1.0, &wpct, &cpct);
    gfx_set_color(g, 90, 220, 90);
    snprintf(line, sizeof(line), "XP bonus: +%d%% per waste, +%d%% per checkpoint", (int)wpct, (int)cpct);
    draw_centered(g, line, 400, ry + 154);
  }
  char foot[128];
  if (page < 2)
    snprintf(foot, sizeof(foot), "Left / Right page   %s add   %s take back   %s done", KEY_CONTINUE, KEY_VIEW,
             KEY_BACK);
  else
    snprintf(foot, sizeof(foot), "Left / Right page    %s choose    %s done", KEY_CONTINUE, KEY_BACK);
  gfx_set_color(g, 170, 170, 170);
  font_set(FONT_BOLD, 11);
  draw_centered(g, foot, 400, y0 + h - 10);
}

/** A career question with YES / NO (carselect's shufflefase 2, 3 and 6 and
 * the sell's savefase 1), in the original's words. */
static void draw_career_confirm(Graphics2D *g, const CareerSave *s, int32_t ec, int32_t from, int32_t which, bool yes) {
  char l[6][96];
  int32_t n = 0, red_from = 99;
  if (which == CC_RESHUFFLE) {
    snprintf(l[n++], 96, "you'll get your stat points");
    snprintf(l[n++], 96, "back but at the cost of %d.", (int)career_reshuffle_cost(s, ec));
    snprintf(l[n++], 96, "do you want to continue?");
  } else if (which == CC_TRANSFER) {
    snprintf(l[n++], 96, "you will reset this car and");
    snprintf(l[n++], 96, "transfer its level and stat");
    snprintf(l[n++], 96, "points. Continue?");
  } else if (which == CC_SELL) {
    if (s->boncomp[3] == 0) {
      snprintf(l[n++], 96, "are you sure? You get no");
      snprintf(l[n++], 96, "stat points from this!");
    } else {
      snprintf(l[n++], 96, "you will get %d car", (int)career_sell_price(s, ec));
      snprintf(l[n++], 96, "points. Continue?");
    }
  } else {
    const int32_t pf = ext_car_to_port(from >= 0 ? from : 0), pt = ext_car_to_port(ec);
    snprintf(l[n++], 96, "You will reset your:");
    snprintf(l[n++], 96, "%s, level %d", car_name(pf), (int)s->level[from >= 0 ? from : 0]);
    snprintf(l[n++], 96, "And transfer its level and stat points to:");
    snprintf(l[n++], 96, "%s, level %d", car_name(pt), (int)s->level[ec]);
    red_from = n;
    snprintf(l[n++], 96, "This cannot be undone and will save the");
    snprintf(l[n++], 96, "game. Continue?");
  }
  const int32_t w = 360, h = 70 + n * 18, x0 = 400 - w / 2, y0 = 225 - h / 2;
  gfx_set_composite(g, 0.92f);
  gfx_set_color(g, 0, 0, 0);
  gfx_fill_rect(g, x0, y0, w, h);
  gfx_set_composite(g, 1.0f);
  gfx_set_color(g, 255, 196, 0);
  gfx_draw_rect(g, x0, y0, w, h);
  font_set(FONT_BOLD, 13);
  for (int32_t i = 0; i < n; i++) {
    if (i >= red_from) gfx_set_color(g, 255, 60, 60);
    else if (which == CC_TRANSFER_TO && (i == 1 || i == 3)) gfx_set_color(g, 0, 200, 0);
    else gfx_set_color(g, 235, 235, 235);
    draw_centered(g, l[i], 400, y0 + 26 + i * 18);
  }
  // YES / NO.
  font_set(FONT_BOLD, 16);
  for (int32_t b = 0; b < 2; b++) {
    const bool sel = (b == 0) == yes;
    const int32_t bx = 400 - 120 + b * 130, by = y0 + h - 38;
    gfx_set_color(g, sel ? 255 : 70, sel ? 196 : 70, sel ? 0 : 70);
    gfx_fill_rect(g, bx, by, 110, 26);
    gfx_set_color(g, sel ? 0 : 200, sel ? 0 : 200, sel ? 0 : 200);
    draw_centered(g, b == 0 ? "YES" : "NO", bx + 55, by + 19);
  }
}

/** Scouting's first page: the Titan's slot is skipped on its fight (XT 9739-9742). */
static int32_t career_scout_first(const CareerRace *r, const CareerSave *s) {
  return (r->stage == 23 && (s->unlocked == 23 || r->hardstage)) ? 2 : 1;
}

/** xtGraphics.scouting (fase 205, XT 9723-9982): one opponent a page --
 * its name and level (red when more than 5 above yours), its six stats
 * beside yours and the difference; an undead's are mostly hidden. Drawn
 * over the stage's start card; Left / Right page, confirm goes on. */
static void draw_career_scout(Graphics2D *g, const CareerRace *r, const CareerSave *s, const CarDefine *cd, int32_t page,
                              int32_t flash) {
  if (r->nplayers < 2 || page < 1 || page >= r->nplayers) return;
  static const char *const kRows[CS_N] = {"Top Speed:", "Acceleration:", "Control:", "Stunting:", "Strength:", "Defence:"};
  const int32_t skipone = career_scout_first(r, s) - 1;
  gfx_set_color(g, 16, 18, 28);
  gfx_fill_rect(g, 65, 25, 670, 400);
  gfx_set_color(g, 0, 0, 0);
  gfx_fill_rect(g, 0, 0, 65, 450);
  gfx_fill_rect(g, 735, 0, 65, 450);
  gfx_fill_rect(g, 65, 0, 670, 25);
  gfx_fill_rect(g, 65, 425, 670, 25);
  // Who is undead (11's vans, 17's hunters, bonus 4's field), or a
  // guardian (13's floors).
  bool undead = false, partundead = false;
  if (r->stage == 17 && page >= 1 && page <= 3) undead = true;
  if (r->stage == 11 && r->bonus != 2 && page >= 1 && page <= 4) undead = true;
  if (r->stage == 13 && (page == 2 || page == 3 || page == 5 || page == 6 || page == 8 || page == 9))
    undead = partundead = true;
  if (r->bonus == 4 && page < r->nplayers - 1) undead = true;
  const char *banner = NULL;
  if (undead) banner = partundead ? "GUARDIAN!" : "UNDEAD OPPONENT!";
  else if (r->shadow[page]) banner = "SHADOW OPPONENT!";
  else if (r->beast[page]) banner = "BEAST OPPONENT!";
  if (banner) {
    const int32_t a = flash < 0 ? 0 : (flash > 255 ? 255 : flash);
    gfx_set_composite(g, (float)a / 255.0f);
    gfx_set_color(g, 255, 255, 255);
    font_set(FONT_BOLD, 26);
    draw_centered(g, banner, 400, 60);
    gfx_set_composite(g, 1.0f);
  }
  char line[96];
  const int32_t pcn = ext_car_to_port(r->sc[page]);
  int32_t number = page - skipone;
  bool titled = true;
  if (r->stage == 17 || (r->stage == 11 && r->bonus != 2)) {
    const int32_t caroffset = r->stage == 17 ? 3 : 4;
    number = page - caroffset;
    titled = page > caroffset;
  }
  font_set(FONT_BOLD, 22);
  gfx_set_color(g, 255, 255, 255);
  if (titled) {
    snprintf(line, sizeof(line), "OPPONENT %d: %s", (int)number, car_name(pcn));
    draw_centered(g, line, 400, 95);
  } else {
    draw_centered(g, car_name(pcn), 400, 95);
  }
  font_set(FONT_BOLD, 18);
  snprintf(line, sizeof(line), "Level %d", (int)r->level[page]);
  if (r->level[page] > r->level[0] + 5) gfx_set_color(g, 220, 40, 40);
  else gfx_set_color(g, 60, 200, 60);
  draw_centered(g, line, 400, 120);
  // The columns: theirs, yours, the difference.
  int32_t theirs[CS_N], mine[CS_N];
  career_scout_stats(cd, pcn, r->sc[page], r->sp[page], theirs);
  career_scout_stats(cd, ext_car_to_port(r->sc[0]), r->sc[0], r->sp[0], mine);
  gfx_set_composite(g, 0.86f);
  gfx_set_color(g, 255, 255, 255);
  gfx_fill_rect(g, 268, 136, 82, 25);
  gfx_fill_rect(g, 384, 136, 128, 25);
  gfx_set_composite(g, 1.0f);
  font_set(FONT_BOLD, 15);
  gfx_set_color(g, 0, 0, 0);
  draw_centered(g, "STATS:", 309, 154);
  draw_centered(g, "YOUR STATS:", 448, 154);
  for (int32_t k = 0; k < CS_N; k++) {
    const int32_t y = 194 + 30 * k;
    gfx_set_color(g, 255, 255, 255);
    font_set(FONT_BOLD, 15);
    font_draw(g, kRows[k], 130, y);
    for (int32_t b = 0; b < 2; b++) {
      gfx_set_color(g, 0, 100, 0);
      gfx_fill_rect(g, 285 + b * 138, y - 19, 56, 25);
    }
    font_set(FONT_BOLD, 18);
    gfx_set_color(g, 230, 230, 230);
    // An undead shows its speed and strength only (XT 9861-9868).
    const bool hidden = undead && !partundead && (k == 1 || k == 2 || k == 3 || k == 5);
    if (hidden) snprintf(line, sizeof(line), "-");
    else snprintf(line, sizeof(line), "%d", (int)theirs[k]);
    draw_centered(g, line, 313, y);
    snprintf(line, sizeof(line), "%d", (int)mine[k]);
    draw_centered(g, line, 451, y);
    if (!undead || partundead) {
      const int32_t d = mine[k] - theirs[k];
      if (d > 0) {
        gfx_set_color(g, 60, 200, 60);
        snprintf(line, sizeof(line), "+%d", (int)d);
      } else if (d < 0) {
        gfx_set_color(g, 220, 40, 40);
        snprintf(line, sizeof(line), "%d", (int)d);
      } else {
        gfx_set_color(g, 220, 220, 220);
        snprintf(line, sizeof(line), "-");
      }
      draw_centered(g, line, 586, y);
    }
  }
  if (r->stage >= 16) {
    snprintf(line, sizeof(line), "Bonus stat points: %d (%d)", (int)r->bonuspoints[page], (int)s->extpoints[r->sc[0]]);
    font_set(FONT_BOLD, 15);
    gfx_set_color(g, 230, 230, 230);
    font_draw(g, line, 130, 380);
  }
  // Paging and the way on.
  font_set(FONT_BOLD, 13);
  gfx_set_color(g, 255, 196, 0);
  if (page > career_scout_first(r, s)) font_draw(g, "< Left", 90, 120);
  if (page < r->nplayers - 1) font_draw(g, "Right >", 710 - font_width("Right >"), 120);
  snprintf(line, sizeof(line), "Opponent %d of %d      %s continue", (int)(page - skipone),
           (int)(r->nplayers - 1 - skipone), KEY_CONTINUE);
  gfx_set_color(g, 200, 200, 200);
  draw_centered(g, line, 400, 412);
}

static void draw_ext_board(Graphics2D *g, const Medium *m, const CheckPoints *cp, const Mad *mads, int32_t nplayers,
                           const int32_t *sc, const Specials *sp, bool arrace, bool listbars, bool step,
                           const char *const *driver_names) {
  static int32_t glowg[SPECIALS_MAX], glowg2[SPECIALS_MAX];
  static bool glowphase[SPECIALS_MAX], glowphase2[SPECIALS_MAX];
  static bool glow_ready;
  if (!glow_ready) {
    for (int32_t i = 0; i < SPECIALS_MAX; i++) { glowg[i] = 65; glowg2[i] = 150; }
    glow_ready = true;
  }
  font_set(FONT_BOLD, 11);
  for (int32_t place = 0; place < 7; place++) {
    for (int32_t a = 0; a < nplayers && a < SPECIALS_MAX; a++) {
      if (cp->pos[a] != place || cp->dested[a] != 0) continue;
      const int32_t y = 30 * place;
      gfx_set_color(g, 0, 0, 0);
      char ord[8];
      if (place == 0) snprintf(ord, sizeof(ord), "1st");
      else if (place == 1) snprintf(ord, sizeof(ord), "2nd");
      else if (place == 2) snprintf(ord, sizeof(ord), "3rd");
      else snprintf(ord, sizeof(ord), "%dth", place + 1);
      font_draw(g, ord, place == 0 ? 688 : 686, 142 + y);

      // The name: black, or glowing while a condition is on the car.
      const bool glow = sp->fixspecials[a] || mads[a].frozen || mads[a].redstr || mads[a].leech;
      if (glow && step) {
        glowg[a] += glowphase[a] ? -12 : 12;
        if (glowg[a] <= 65) glowphase[a] = false;
        if (glowg[a] >= 225) glowphase[a] = true;
        if (!glowphase2[a]) { if (glowg2[a] < 245) glowg2[a] += 10; }
        else if (glowg2[a] > 75) glowg2[a] -= 10;
        if (glowg2[a] <= 150) glowphase2[a] = false;
        if (glowg2[a] >= 230) glowphase2[a] = true;
        if (glowg[a] > 255) glowg[a] = 255;
        if (glowg[a] < 0) glowg[a] = 0;
        if (glowg2[a] > 255) glowg2[a] = 255;
        if (glowg2[a] < 65) glowg2[a] = 65;
      }
      gfx_set_color(g, 0, 0, 0);
      if (glow) {
        if (sp->fixspecials[a] && !mads[a].frozen && !mads[a].redstr && !mads[a].leech && !mads[a].strswap)
          gfx_set_color(g, glowg[a], 0, 0);
        if (mads[a].frozen && !mads[a].redstr && !mads[a].leech && !mads[a].strswap) gfx_set_color(g, 0, 0, glowg[a]);
        if (mads[a].redstr && !mads[a].leech && !mads[a].strswap) gfx_set_color(g, glowg2[a], glowg2[a] / 2, 0);
        if (mads[a].leech && !mads[a].strswap)
          gfx_set_color(g, glowg2[a] - 60, (glowg2[a] - 60) * 3 / 4, 0);
        if (mads[a].strswap) gfx_set_color(g, 0, glowg[a], 0);
      }
      const int32_t c = sc[a];
      const char *name = c == 9 ? "SoJ" : (c == 2 ? "Wow C." : (car_name(c)));
      if (driver_names) name = driver_names[a];   // the tournament's drivers (XT 6421)
      font_draw(g, name, 750 - font_width(name) / 2, 131 + y);

      // The bar.
      int32_t fill, r, gg, b;
      if (!listbars || !arrace) {
        if (arrace) {
          fill = (int32_t)(60.0f * ((float)mads[a].hitmag / (float)mads[a].cd->maxmag[mads[a].cn]));
          r = 244; gg = 244; b = 11;
          if (fill > 20) gg = (int32_t)(244.0f - 233.0f * ((float)(fill - 20) / 40.0f));
          if (fill > 60) fill = 60;
          r += (int32_t)((float)r * ((float)m->snap[0] / 100.0f));
          gg += (int32_t)((float)gg * ((float)m->snap[1] / 100.0f));
          b += (int32_t)((float)b * ((float)m->snap[2] / 100.0f));
        } else {
          fill = (int32_t)(60.0f * (mads[a].power / 98.0f));
          r = (int32_t)((mads[a].power == 98.0f ? 64.0f : 128.0f) * (1.0f + (float)m->snap[0] / 100.0f));
          gg = (int32_t)(244.0f * (1.0f + (float)m->snap[1] / 100.0f));
          b = (int32_t)(244.0f * (1.0f + (float)m->snap[2] / 100.0f));
        }
      } else {
        const bool running = sp->fixspecials[a];
        fill = (int32_t)(60.0f * ((running ? mads[a].speclast : mads[a].spatk) / 120.0f));
        r = (int32_t)((running ? 85.0f : 220.0f) * (1.0f + (float)m->snap[0] / 100.0f));
        gg = 0;
        b = 0;
      }
      r = r > 255 ? 255 : (r < 0 ? 0 : r);
      gg = gg > 255 ? 255 : (gg < 0 ? 0 : gg);
      b = b > 255 ? 255 : (b < 0 ? 0 : b);
      gfx_set_color(g, r, gg, b);
      gfx_fill_rect(g, 715, 135 + y, fill, 5);
      gfx_set_color(g, 0, 0, 0);
      gfx_draw_rect(g, 715, 135 + y, 60, 5);
      break;
    }
  }
}

int game_run(void) {
  // See platform_asset_prefix()'s own doc comment (platform.h) -- "" on
  // desktop (repo-root-relative paths, matching AGENTS.md's convention
  // for the JS dev server; run this binary from the repo root), "app0:"
  // on Vita.
  vfs_set_fpath(platform_asset_prefix());
  // The display comes up FIRST, before any asset loads, so the original's
  // loading() screen (xtGraphics.java:1410) can show while they do.
  const int width = 800, height = 450;
  if (!platform_init(width, height)) {
    return 1;
  }

  // Audio device + mixer -- see platform/<name>/audio.h's own doc comment.
  // A failed open (no audio hardware, e.g. under this sandbox's Xvfb)
  // degrades to silent play (audio_play returns -1, callers don't need to
  // check), same fallback philosophy as a missing data/images.zip.
  Audio audio;
  audio_init(&audio, 44100);

  // The race's music (Java's `strack`, see load_music_track()). Owned here
  // so it stays alive while the audio backend's player reads it; replaced
  // when a race needs a different track, and freed at exit.
  RadicalTrack stage_music;
  memset(&stage_music, 0, sizeof(stage_music));
  int32_t stage_music_loaded_for = -1; // stage_music_id() the above holds, -1 = none yet
  // The menu track (Java's `intertrack`), and which of the two is the one
  // currently playing -- see the reconcile step after the state logic.
  RadicalTrack interface_music;
  memset(&interface_music, 0, sizeof(interface_music));
  bool interface_playing = false;

  glDisable(GL_DEPTH_TEST); // no depth buffer, by design -- see gfx.h
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glClearColor(0.05f, 0.05f, 0.08f, 1.0f);

  // Game-space projection: 800x450, y-down, origin top-left -- matches
  // web/graphics.js's vertex shader mapping (see its VERT_SRC comment).
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, width, height, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();

  Graphics2D g;
  gfx_init(&g, width, height);

  // loading() -- sign.gif, hello.gif (the Radicalplay logo) and loadbar.gif
  // live loose in data/, and the bar fills by the size of what has loaded.
  // The loading screen already speaks the saved language (the Settings
  // load in full, and apply, further down).
  {
    char path[1024];
    if (platform_progress_path(path, sizeof(path))) {
      GameSettings early = game_settings_defaults(NFM_DEFAULT_GRAPHICS);
      game_settings_load(path, &early);
      i18n_set_lang(early.language);
    }
  }
  BootImages boot_images;
  boot_images.sign = load_data_gif("data/sign.gif");
  boot_images.hello = load_data_gif("data/hello.gif");
  boot_images.loadbar = load_data_gif("data/loadbar.gif");
  const int32_t boot_kb_models = 105, boot_kb_images = 715, boot_kb_sounds = 331;
  const int32_t boot_kb_total = boot_kb_models + boot_kb_images + boot_kb_sounds;
  boot_loading_frame(&g, &boot_images, 0, boot_kb_total);

  char *car_text = vfs_read_text("mycars/Simple_Car.rad");
  if (!car_text) {
    fprintf(stderr, "could not read mycars/Simple_Car.rad (run this from the repo root)\n");
    return 1;
  }

  nfm_set_seed(9001);
  Medium m;
  medium_init(&m);
  // `static` purely to keep it OFF the stack: Trackers is ~353KB on its
  // own, and this function's frame has to fit the PS Vita main thread's
  // fixed stack (platform/vita/main.c), which it was overflowing on real
  // hardware -- see ../../TASKS_NATIVE.md's Vita hardware-crash entry.
  // Safe here because game_run() is the whole program: it is called
  // exactly once, from main(), and never recursively or from a second
  // thread, so there is no instance for a single shared copy to collide
  // with. trackers_init() below still initialises it explicitly, so the
  // zero-init `static` adds changes nothing about its starting state.
  static Trackers t;
  trackers_init(&t);

  // ContO(bytes, m, t) with m.loadnew=true, matching CarDefine.loadcar()'s
  // real usage exactly (see native/tests/cont_o_test.c). Loaded eagerly
  // regardless of what the menu ends up selecting -- it's the
  // CUSTOM_CAR_INDEX menu entry, and it's a small file, so there's no
  // reason to defer parsing it until/unless the player actually picks it.
  m.loadnew = true;
  ContO car_base;
  cont_o_init_buf(&car_base, car_text, &m, &t);
  m.loadnew = false;
  if (car_base.errd || car_base.npl <= 60) {
    fprintf(stderr, "car failed to load (errd=%d npl=%d)\n", car_base.errd, car_base.npl);
    return 1;
  }
  // The rest of CarDefine.loadcar()'s field setup (loadstat()'s gameplay
  // stat tables are skipped entirely -- see TASKS_NATIVE.md).
  car_base.shadow = true;
  car_base.noline = false;
  car_base.decor = false;
  car_base.tnt = 0;
  car_base.disp = 0;
  car_base.disline = 7;
  car_base.grounded = 1.0f;

  // Base models (16 cars + 68 track/decoration pieces), loaded through the
  // lean game_sparker subset -- see game_sparker.h. Also eager: ALL 16
  // built-in cars' geometry is needed for the car-select menu regardless
  // of which one gets picked, and the same zip pass also loads every
  // track/decoration piece a stage might reference, so there's no
  // "load only what's selected" shortcut available here anyway.
  ContO *base_models = calloc(GAME_SPARKER_NUM_BASE_MODELS, sizeof(ContO));
  if (!base_models || !game_sparker_loadbase(base_models, &m, &t, "data/models.zip")) {
    fprintf(stderr, "could not load data/models.zip\n");
    return 1;
  }
  // ---- Extended's model table (ext_stage.c): its 129 models in its own
  // numbering -- 0-22 its cars (our 16-38, EXT_FIRST_CAR), 23-38 NFM 2's,
  // 39-77 track pieces, 78-116 the beasts, 117-128 scenery.
  ContO *ext_models = calloc(EXT_NUM_MODELS, sizeof(ContO));
  // A car's model: NFM 2's 16 from models.zip, Extended's own 23 (this
  // port's 16-38) from its models.radq.
#define CAR_MODEL(cn) ((cn) >= NEW_CAR_FIRST ? &new_models[(cn) - NEW_CAR_FIRST] \
                      : (cn) < EXT_FIRST_CAR ? &base_models[(cn)] : &ext_models[(cn) - EXT_FIRST_CAR])
  // A racer's model, for its start and every rebuild (a fix, the replays):
  // the custom car's, a career beast's "B" model (GameSparker.loadstage,
  // Extended's model sc + 78), else its car's. RACE_LOOK puts back what the
  // copy resets: a shadow car is see-through (Plane.d, shadowtrans 80).
#define RACE_MODEL(i, cn) ((i) == 0 && car_index == CUSTOM_CAR_INDEX ? &car_base                            : (ext_career && crace.beast[i]) ? &ext_models[EXT_MODEL_BEAST + crace.sc[i]] : CAR_MODEL(cn))
#define RACE_LOOK(i) do {                                                                \
    if (ext_career && crace.shadow[i]) co[i].fade = 255 - 80;                              \
    /* and the stage's see-through on it (career_stage.c set_inv) */                       \
    if (ext_career && cstage.started && 255 - cstage.car_inv[i] > co[i].fade)              \
      co[i].fade = 255 - cstage.car_inv[i];                                                \
  } while (0)
  if (!ext_models || !ext_loadbase(ext_models, &m, &t)) {
    fprintf(stderr, "could not load ext/data/models.radq\n");
  }
  // ----
  boot_loading_frame(&g, &boot_images, boot_kb_models, boot_kb_total);

  CarDefine cd;
  car_define_init(&cd);
  car_define_extended(&cd);   // the cars as Extended tuned them
  // Simple_Car.rad's own interpolated stats, computed once (independent
  // of menu navigation) into slot CUSTOM_CAR_INDEX -- matching
  // CarDefine.js's real loadcar() call (see car_define.h). If this
  // fails, the custom-car menu entry silently drives with slot 0's
  // built-in stats instead (car_slot_for below never selects
  // CUSTOM_CAR_INDEX as a cn in that case).
  bool custom_car_ok = car_define_loadcar(&cd, car_text, &car_base, car_base.maxR, car_base.roofat,
                                           car_base.wh, CUSTOM_CAR_INDEX);
  if (!custom_car_ok) {
    fprintf(stderr, "Simple_Car.rad: loadcar/loadstat failed, custom car entry will use built-in slot 0 stats\n");
  }
  // The new cars (new_cars.c): Revised and Recharged's, then the player's
  // own .rad files in the save folder's cars/ (sdmc:/switch/nfm-extended/
  // cars on the Switch).
  ContO *new_models = calloc(NEW_CARS_MAX, sizeof(ContO));
  {
    char sd_cars[600] = "";
    char save_path[512];
    if (platform_progress_path(save_path, sizeof(save_path))) {
      char *slash = strrchr(save_path, '/');
      if (slash) {
        *slash = '\0';
        snprintf(sd_cars, sizeof(sd_cars), "%s/cars", save_path);
      }
    }
    new_cars_load(new_models, &cd, &m, &t, sd_cars[0] ? sd_cars : NULL);
    fprintf(stderr, "new cars: %d\n", (int)g_new_car_count);
  }
  free(car_text);


  // Offscreen target the whole frame renders into, then gets blitted back
  // from at less-than-full alpha -- see gfx_gl_render_target_blit's own
  // doc comment (gfx_gl.h) for the Java `paint()` mechanism this ports.
  // A failed init (unlikely -- FBOs are core since GL 3.0/nearly every
  // GLES2 device, but see gl_include.h's own note on this port
  // deliberately staying off the GL1.1 subset just for this) degrades to
  // drawing straight to the default framebuffer every frame, same
  // fail-soft convention as a missing HUD asset elsewhere in this file --
  // just without the trailing effect, not a crash.
  GfxGlRenderTarget scene_rt = {0};

  // The trail's HISTORY, held in a texture this code owns rather than in
  // the window's back buffer. The composite used to blend this frame's
  // scene straight onto the default framebuffer without clearing it, so
  // "the previous frame" meant whatever the windowing system happened to
  // leave in the buffer being drawn into. Desktop GL made that look
  // right; vitaGL cycles through several display buffers
  // (DISPLAY_MAX_BUFFER_COUNT is 5), so on the Vita "the previous frame"
  // was actually two or three frames old and the effect showed discrete
  // older frames instead of a trail -- reported from hardware as being
  // able to see previous frames.
  //
  // Neither API ever promised that content survives a swap, so the fix is
  // to stop asking. accum_rt is written and read only here: each frame
  // blends the new scene into it at the same alpha as before, then the
  // result is what reaches the screen. Same exponential falloff, now
  // identical on both platforms because nothing outside this file touches
  // the buffer between frames.
  //
  // Both are built by scene_targets_build(), at the size Settings > Graphics
  // asks for (rebuilt once the settings are read, and when they change).
  GfxGlRenderTarget accum_rt = {0};
  bool motion_blur_ok = scene_targets_build(&scene_rt, &accum_rt, false, width, height, GFX_ORIGINAL);

  // Menu asset textures -- loaded ONCE at startup (they don't change
  // between menu states, and holding them across the whole session
  // avoids a re-decode every time the player returns to a menu). Names
  // pulled directly from data/images.zip; picked out of the 139 real
  // assets by matching which sit visually on the JS's own menu screens.
  //   bggo.jpg / bgmain.jpg -- 800x450 photo backdrops (the JS's own
  //     game-over / main-menu backgrounds respectively; bggo covers the
  //     whole viewport at native size).
  //   logocars.png -- 638x243 "CARS" heading art, the JS's own car-
  //     select-screen title.
  //   nfm.gif / madness.gif / racing.gif -- the "NEED FOR MADNESS RACING"
  //     wordmark, split into three separately-typeset pieces in the
  //     original.
  //   ready.gif / back.gif / next.gif / play.gif / cancel.gif / exit.gif
  //     -- pre-rendered button glyphs; used for menu affordances.
  //   selectcar.gif -- "SELECT CAR" caption for the car-picker screen.
  //   sdets.gif -- "SELECT DETAILS" (used on the stage-picker screen).
  // Every one may resolve to tex<0 if data/images.zip is missing or the
  // named entry isn't there -- callers guard with `if (tex >= 0)` and
  // draw without that art, same convention as the HUD's load_hud_gif
  // failure path.
  // Menu assets -- see native/docs/MENU_FLOW.md §1 for the full
  // Java-field -> images.zip filename map. Grouped by which screen uses
  // them (main menu / gamemode submenu / car select / stage select),
  // matching the layout xtGraphics.java's loadimages() enforces (all
  // loaded once at boot, held forever).
  HudImg menu_bg = {-1, 0, 0};                             // bggo.jpg -- game-over background (post-race)
  HudImg menu_carsbg = {-1, 0, 0};                         // cars.gif -- CAR SELECT stormy-sky backdrop (670x400)
  HudImg menu_statb = {-1, 0, 0};                          // statb.gif -- 156x7 stat-bar colored background
  HudImg menu_statbo = {-1, 0, 0};                         // statbo.gif -- 156x7 stat-bar outline/border overlay
  HudImg menu_br = {-1, 0, 0};                             // br.png -- 670x400 STAGE SELECT torn-paper backdrop
  HudImg menu_select = {-1, 0, 0};                         // select.gif -- 125x18 "SELECT" caption (stage picker)
  HudImg menu_stunts = {-1, 0, 0};                         // stunts.png -- 464x110 4-car stunt visualization
  HudImg menu_arrows = {-1, 0, 0};                         // arrows.gif -- arrow keys illustration
  HudImg menu_stunt_arrows = {-1, 0, 0};                   // the stunt diagrams' copy of it (the Vita's stick)
  HudImg menu_space = {-1, 0, 0};                          // space.gif -- spacebar key icon
  HudImg menu_plus = {-1, 0, 0};                           // plus.gif -- 25x25 "+" glyph between space and arrows
  HudImg menu_madness = {-1, 0, 0};                        // madness.gif -- 231x46 "MADNESS!" wordmark (credits page)
  HudImg menu_congrd = {-1, 0, 0};                         // congrad.gif -- "CONGRATULATIONS" banner (post-race win)
  HudImg menu_gameov = {-1, 0, 0};                         // gameov.gif -- "GAME OVER" banner (post-race lose)
  HudImg menu_bgmain = {-1, 0, 0};                          // bgmain.jpg -- main menu scrolling backdrop
  HudImg menu_logomadbg = {-1, 0, 0};                       // logomadbg.jpg -- main menu 60% alpha watermark
  HudImg menu_logomadnes = {-1, 0, 0};                      // logomad.png -- gold MADNESS wordmark
  HudImg menu_logocars = {-1, 0, 0};                        // logocars.png -- CARS logo strip
  // d1/d2/d3.png -- Java's own `dude[3]` array (xtGraphics.java:4110).
  // Index 0 is the only one the main menu / gamemode submenu ever draw
  // (`dude[0]`, :4335); the Instructions screen cycles all three as Coach
  // Insano's talking-head mouth positions (`dude[duds]`, duds 0..2).
  HudImg menu_dude[3] = {{-1, 0, 0}, {-1, 0, 0}, {-1, 0, 0}};
  // Instructions-screen art (Java inst(), :4048-4307). All present in
  // data/images.zip, three of them under a DIFFERENT filename than the
  // Java field name -- see each load call below.
  HudImg menu_nfm = {-1, 0, 0};        // nfm.gif -- "NFM" wordmark inline in the page-3 sentence
  HudImg menu_racing = {-1, 0, 0};     // racing.gif / wasting.gif / ory.gif -- the
  HudImg menu_wasting = {-1, 0, 0};    //   "racing OR wasting" illustration triptych
  HudImg menu_ory = {-1, 0, 0};
  HudImg menu_chil = {-1, 0, 0};       // chil.gif -- powered-up car illustration
  HudImg menu_opwr = {-1, 0, 0};       // power.gif  (Java field `opwr`)   -- power meter art
  HudImg menu_fixhoop = {-1, 0, 0};    // fixhoop.png -- the electrified repair hoop
  HudImg menu_sarrow = {-1, 0, 0};     // arrow.gif  (Java field `sarrow`) -- guidance arrow
  HudImg menu_oflaot = {-1, 0, 0};     // float.gif  (Java field `oflaot`) -- Coach Insano speech bubble
  HudImg menu_kz = {-1, 0, 0}, menu_kx = {-1, 0, 0}, menu_kv = {-1, 0, 0};
  HudImg menu_kenter = {-1, 0, 0}, menu_km = {-1, 0, 0}, menu_kn = {-1, 0, 0}, menu_ks = {-1, 0, 0};
  HudImg menu_opback = {-1, 0, 0};                          // opback.png -- brown pill
  HudImg menu_opti = {-1, 0, 0};                            // options.png -- main menu 4-option label block
  HudImg menu_extlabel = {-1, 0, 0};                        // data/port/extended_label.png -- the Extended row
  HudImg menu_careerlabel = {-1, 0, 0};                     // data/port/career_label.png -- the RPG Mode row
  HudImg menu_opti2 = {-1, 0, 0};                           // options2.png -- gamemode submenu 4-option label block
  HudImg menu_opsettings = {-1, 0, 0};                      // data/port/opsettings.png -- this port's Settings row
  HudImg menu_byrd = {-1, 0, 0};                            // byrd.png -- byline
  HudImg menu_nfmcoms_asset = {-1, 0, 0};                   // nfmcoms.png -- www.NFM.com footer
  HudImg menu_back = {-1, 0, 0}, menu_next = {-1, 0, 0};
  HudImg menu_play = {-1, 0, 0};                            // play.gif -- 81x18 (unused in Java's actual carselect but kept as fallback)
  HudImg menu_contin = {-1, 0, 0};                          // continue.gif -- 90x23 (Java carselect's real "continue" button at 355,385)
  HudImg menu_selectcar = {-1, 0, 0};
  HudImg menu_trackbg_normal = {-1, 0, 0};                  // track.jpg -- xtGraphics.java:899 trackbg[0], 670x400 scrolling landscape
  HudImg menu_trackbg_dodged = {-1, 0, 0};                  // dodgen(track.jpg) -- xtGraphics.java:901 trackbg[1], variant swap
  HudImg menu_pgate = {-1, 0, 0};                           // pgate.gif -- cantgo() padlock glyph (drawn 9x, xtGraphics.java:2001-2003)
  HudImg menu_gameh = {-1, 0, 0};                           // gameh.gif -- levelhigh()'s replay-caption panel (xtGraphics.java:4004-4046)
  HudImg menu_rpro = {-1, 0, 0};                            // rpro.gif -- rad()'s "A Radicalplay Production" (xtGraphics.java:1618)
  HudImg menu_radicalplay = {-1, 0, 0};                     // radicalplay.gif -- finish()'s stage-27 campaign-completion logo (xtGraphics.java:6876)
  HudImg menu_paused = {-1, 0, 0};                          // paused.gif -- pausedgame()'s own panel at (281,8) (xtGraphics.java:4761)
  CarSmokeWarp car_smoke_warp;                              // cars.gif/smokey.gif -- car-select's smoke-warp entrance, see its own doc comment
  memset(&car_smoke_warp, 0, sizeof(car_smoke_warp));
  car_smoke_warp.flexpix_tex = -1;
  car_smoke_warp.flatrstart = 6; // settled/no-op until car_smoke_warp_load succeeds below
  {
    VfsZip images_zip;
    {
      // The text font (tools/bake_font.py): every drawString goes through it.
      int32_t len = 0;
      uint8_t *bytes = vfs_read_bytes("data/port/font.png", &len);
      PngImage img;
      if (bytes && png_decode(bytes, (size_t)len, &img)) {
        font_set_texture(gfx_gl_upload_texture_mipmapped(img.rgba, img.width, img.height));
        font_set_icon_fn(pad_icon);
        png_free(&img);
      } else {
        fprintf(stderr, "data/port/font.png: missing or undecodable -- no text\n");
      }
      if (bytes) vfs_free_bytes(bytes);
    }
    if (vfs_read_zip("data/images.zip", &images_zip)) {
      menu_bg = load_menu_jpeg(&images_zip, "bggo.jpg");
      menu_carsbg = load_menu_gif(&images_zip, "cars.gif");
      car_smoke_warp_load(&images_zip, &car_smoke_warp);
      menu_statb = load_menu_gif(&images_zip, "statb.gif");
      menu_statbo = load_menu_gif(&images_zip, "statbo.gif");
      menu_br = load_menu_png(&images_zip, "br.png");
      menu_select = load_menu_gif(&images_zip, "select.gif");
      menu_stunts = load_menu_png(&images_zip, "stunts.png");
      menu_arrows = load_menu_gif(&images_zip, "arrows.gif");
      menu_space = load_menu_gif(&images_zip, "space.gif");
      menu_plus = load_menu_gif(&images_zip, "plus.gif");
      menu_madness = load_menu_gif(&images_zip, "madness.gif");
      menu_congrd = load_menu_gif(&images_zip, "congrad.gif");
      menu_gameov = load_menu_gif(&images_zip, "gameov.gif");
      menu_bgmain = load_menu_jpeg(&images_zip, "bgmain.jpg");
      menu_logomadbg = load_menu_jpeg(&images_zip, "logomadbg.jpg");
      menu_logomadnes = load_menu_png(&images_zip, "logomad.png");
      menu_logocars = load_menu_png(&images_zip, "logocars.png");
      menu_dude[0] = load_menu_png(&images_zip, "d1.png");
      menu_dude[1] = load_menu_png(&images_zip, "d2.png");
      menu_dude[2] = load_menu_png(&images_zip, "d3.png");
      // Instructions-screen art. NOTE the three renamed ones: Java's
      // loadimages() binds field `oflaot` to float.gif (:837), `opwr` to
      // power.gif (:916) and `sarrow` to arrow.gif (:950) -- the field
      // names and the filenames genuinely disagree in the original, so
      // searching the zip for "oflaot"/"opwr"/"sarrow" finds nothing.
      menu_nfm = load_menu_gif(&images_zip, "nfm.gif");
      menu_racing = load_menu_gif(&images_zip, "racing.gif");
      menu_wasting = load_menu_gif(&images_zip, "wasting.gif");
      menu_ory = load_menu_gif(&images_zip, "ory.gif");
      menu_chil = load_menu_gif(&images_zip, "chil.gif");
      menu_opwr = load_menu_gif(&images_zip, "power.gif");
      menu_fixhoop = load_menu_png(&images_zip, "fixhoop.png");
      menu_sarrow = load_menu_gif(&images_zip, "arrow.gif");
      menu_oflaot = load_menu_gif(&images_zip, "float.gif");
      menu_kz = load_menu_gif(&images_zip, "kz.gif");
      menu_kx = load_menu_gif(&images_zip, "kx.gif");
      menu_kv = load_menu_gif(&images_zip, "kv.gif");
      menu_kenter = load_menu_gif(&images_zip, "kenter.gif");
      menu_km = load_menu_gif(&images_zip, "km.gif");
      menu_kn = load_menu_gif(&images_zip, "kn.gif");
      menu_ks = load_menu_gif(&images_zip, "ks.gif");
#if defined(NFM_TARGET_VITA) || defined(NFM_TARGET_SWITCH)
      // The Switch build swaps in its own set by the same names
      // (tools/gen_switch_assets.py -> data/switch/sw_*.png): ZR/ZL and the
      // stick for the drive cluster, B for the handbrake, A and Plus for
      // ENTER, X / Y / Minus / D-pad down / the right stick for the rest.
#ifdef NFM_TARGET_VITA
#define PAD_ART(n) "data/vita/vita_" n ".png"
#else
#define PAD_ART(n) "data/switch/sw_" n ".png"
#endif
      // Vita control art, drawn to the same footprints so every label the
      // Instructions pages place around these keeps its position: the
      // arrow-key cluster becomes a d-pad (81x62) and the SPACEBAR bar
      // becomes a Cross button centred in the bar's 212x30 slot. Both are
      // regenerable from tools/gen_vita_assets.py, which matches the
      // original art's own grey ramp, gradient face, inner top line and
      // attached right/bottom side wall. Loaded from loose files rather
      // than images.zip because that archive is the original's, untouched.
      // A missing file leaves the keyboard art in place rather than
      // blanking the page, since load_menu_png_file returns {-1,0,0}.
      {
        HudImg vita_arrows = load_menu_png_file(PAD_ART("arrows"));
        if (vita_arrows.tex >= 0) menu_arrows = vita_arrows;
        // Stunts are CROSS + the left stick here, so their diagrams show
        // the stick pushed four ways instead of the D-pad.
        menu_stunt_arrows = load_menu_png_file(PAD_ART("stick"));
        HudImg vita_space = load_menu_png_file(PAD_ART("space"));
        if (vita_space.tex >= 0) menu_space = vita_space;
        // The "OTHER CONTROLS" page's seven individual key caps, each drawn
        // to its original's exact footprint (29x33, and 97x33 for ENTER).
        // The mapping is whatever platform/vita/platform.c actually binds:
        // Z/X become the L/R triggers, V becomes Triangle, M and N become
        // Square and Select, S becomes D-pad down, and ENTER -- which
        // this page labels "navigate and pause" -- carries both Cross and
        // START, since those two took its two jobs.
        struct { HudImg *dst; const char *path; } vita_keys[] = {
          { &menu_kz,     PAD_ART("kz") },
          { &menu_kx,     PAD_ART("kx") },
          { &menu_kv,     PAD_ART("kv") },
          { &menu_km,     PAD_ART("km") },
          { &menu_kn,     PAD_ART("kn") },
          { &menu_ks,     PAD_ART("ks") },
          { &menu_kenter, PAD_ART("kenter") },
        };
        for (size_t vi = 0; vi < sizeof(vita_keys)/sizeof(vita_keys[0]); vi++) {
          HudImg img = load_menu_png_file(vita_keys[vi].path);
          if (img.tex >= 0) *vita_keys[vi].dst = img;
        }
      }
#endif
      menu_opback = load_menu_png(&images_zip, "opback.png");
      menu_opti = load_menu_png(&images_zip, "options.png");
      menu_opti2 = load_menu_png(&images_zip, "options2.png");
      menu_extlabel = load_menu_png_file("data/port/tournament_label.png");   // the Tournament row
      menu_careerlabel = load_menu_png_file("data/port/career_label.png");
      // This port's fourth main-menu row. Not an original asset: options.png's
      // style redrawn (tools/gen_menu_label.py), a loose file beside data/vita/.
      menu_opsettings = load_menu_png_file("data/port/opsettings.png");
      menu_byrd = load_menu_png(&images_zip, "byrd.png");
      menu_nfmcoms_asset = load_menu_png(&images_zip, "nfmcoms.png");
      menu_back = load_menu_gif(&images_zip, "back.gif");
      menu_next = load_menu_gif(&images_zip, "next.gif");
      menu_play = load_menu_gif(&images_zip, "play.gif");
      menu_contin = load_menu_gif(&images_zip, "continue.gif");
      menu_selectcar = load_menu_gif(&images_zip, "selectcar.gif");
      load_track_pair(&images_zip, &menu_trackbg_normal, &menu_trackbg_dodged);
      menu_pgate = load_menu_gif(&images_zip, "pgate.gif");
      menu_gameh = load_menu_gif(&images_zip, "gameh.gif");
      menu_radicalplay = load_menu_gif(&images_zip, "radicalplay.gif");
      menu_rpro = load_menu_gif(&images_zip, "rpro.gif");
      menu_paused = load_menu_gif(&images_zip, "paused.gif");
      vfs_free_zip(&images_zip);
      // Their Spanish lettering (data/port/es/MANIFEST.txt).
      lang_img_add(&menu_selectcar, "selectcar");
      lang_img_add(&menu_select, "select");
      lang_img_add(&menu_back, "back");
      lang_img_add(&menu_next, "next");
      lang_img_add(&menu_contin, "continue");
      lang_img_add(&menu_congrd, "congrad");
      lang_img_add(&menu_gameov, "gameov");
      lang_img_add(&menu_gameh, "gameh");
      lang_img_add(&menu_paused, "paused");
      lang_img_add(&menu_opwr, "power");
      lang_img_add(&menu_opsettings, "opsettings");
      lang_img_add(&menu_careerlabel, "career_label");
      lang_img_add(&menu_extlabel, "tournament_label");
      lang_img_add(&menu_stunts, "stunts");   // tools/bake_inst_es.py
      lang_img_add(&menu_ory, "ory");
      for (int32_t i = 0; i < 3; i++) {
        static const char *const kRows[3] = {"data/port/es/op_play.png", "data/port/es/op_inst.png",
                                             "data/port/es/op_credits.png"};
        g_es_opti_rows[i] = load_menu_png_file(kRows[i]);
      }
      g_es_freeplay = load_menu_png_file("data/port/es/freeplay_label.png");
    } else {
      fprintf(stderr, "could not load data/images.zip -- menus will draw without their art\n");
    }
  }
  boot_loading_frame(&g, &boot_images, boot_kb_models + boot_kb_images, boot_kb_total);

  // Bundle the Instructions screen's art into one value so its page
  // renderer takes a single parameter (see draw_instructions). Built
  // AFTER the load block above so every field is either a real texture or
  // the {-1,0,0} "failed to load" sentinel every draw site already
  // guards on.
  InstAssets inst_assets;
  memset(&inst_assets, 0, sizeof(inst_assets));
  inst_assets.dude[0] = menu_dude[0];
  inst_assets.dude[1] = menu_dude[1];
  inst_assets.dude[2] = menu_dude[2];
  inst_assets.oflaot = menu_oflaot;
  inst_assets.nfm = menu_nfm;
  inst_assets.racing = menu_racing;
  inst_assets.wasting = menu_wasting;
  inst_assets.ory = menu_ory;
  inst_assets.chil = menu_chil;
  inst_assets.opwr = menu_opwr;
  inst_assets.fixhoop = menu_fixhoop;
  inst_assets.sarrow = menu_sarrow;
  inst_assets.space = menu_space;
  inst_assets.arrows = menu_arrows;
  inst_assets.stunt_arrows = menu_stunt_arrows.tex >= 0 ? menu_stunt_arrows : menu_arrows;
  inst_assets.plus = menu_plus;
  inst_assets.stunts = menu_stunts;
  inst_assets.back = menu_back;
  inst_assets.next = menu_next;
  inst_assets.contin = menu_contin;
  inst_assets.bggo = menu_bg;
  inst_assets.bgmain = menu_bgmain;
  inst_assets.kz = menu_kz;
  inst_assets.kx = menu_kx;
  inst_assets.kv = menu_kv;
  inst_assets.kenter = menu_kenter;
  inst_assets.km = menu_km;
  inst_assets.kn = menu_kn;
  inst_assets.ks = menu_ks;
  lang_img_add(&inst_assets.opwr, "power");
  lang_img_add(&inst_assets.back, "back");
  lang_img_add(&inst_assets.next, "next");
  lang_img_add(&inst_assets.contin, "continue");
  lang_img_add(&inst_assets.stunts, "stunts");
  lang_img_add(&inst_assets.ory, "ory");

  // Sound-effect assets (data/sounds.zip) -- loaded once, held for the
  // whole session. WavClip.samples==NULL (frame_count 0) on a missing/
  // failed entry; audio_play()/reconciliation code below treats
  // that as silence, same fail-soft convention as a missing images.zip
  // asset. Part 16 adds the full physics-SFX set (engine/air/crash/skid/
  // scrape/wasted) alongside the pre-existing countdown/checkpoint/
  // carfixed/powerup handful -- table-driven since it's ~60 files
  // (real sounds.zip has 56; a couple of entries below share names with
  // engine slots, e.g. "one"/"two" ARE countdown clips not engine ones,
  // Java just numbers both sets independently).
  WavClip snd_checkpoint = {0}, snd_carfixed = {0}, snd_powerup = {0};
  WavClip snd_three = {0}, snd_two = {0}, snd_one = {0}, snd_go = {0};
  WavClip snd_engine[5][5] = {{{0}}};   // [enginsignature][rev] -- "00.wav".."44.wav"
  WavClip snd_air[6] = {{0}};           // "air0.wav".."air5.wav"
  WavClip snd_crash[3] = {{0}}, snd_lowcrash[3] = {{0}};
  WavClip snd_skid[3] = {{0}}, snd_dustskid[3] = {{0}};
  WavClip snd_scrape[3] = {{0}};        // scrape3 doubles as gscrape()'s "scrape3b" -- see xt_graphics.c
  WavClip snd_tires = {0}, snd_wasted = {0}, snd_firewasted = {0};
  {
    // {want_name, dst} table -- built once, matched against every real
    // entry in the zip by name. 7 (pre-existing) + 25 (engine) + 6 (air)
    // + 3+3 (crash/lowcrash) + 3+3 (skid/dustskid) + 3 (scrape) + 3
    // (tires/wasted/firewasted) = 56.
    struct { char name[16]; WavClip *dst; } table[56];
    int32_t nt = 0;
#define SND_WANT(nm, ptr) do { snprintf(table[nt].name, sizeof(table[nt].name), "%s", (nm)); table[nt].dst = (ptr); nt++; } while (0)
    SND_WANT("checkpoint.wav", &snd_checkpoint);
    SND_WANT("carfixed.wav", &snd_carfixed);
    SND_WANT("powerup.wav", &snd_powerup);
    SND_WANT("three.wav", &snd_three);
    SND_WANT("two.wav", &snd_two);
    SND_WANT("one.wav", &snd_one);
    SND_WANT("go.wav", &snd_go);
    for (int32_t sig = 0; sig < 5; sig++) {
      for (int32_t rev = 0; rev < 5; rev++) {
        char nm[16];
        snprintf(nm, sizeof(nm), "%d%d.wav", sig, rev);
        SND_WANT(nm, &snd_engine[sig][rev]);
      }
    }
    for (int32_t i = 0; i < 6; i++) {
      char nm[16];
      snprintf(nm, sizeof(nm), "air%d.wav", i);
      SND_WANT(nm, &snd_air[i]);
    }
    for (int32_t i = 0; i < 3; i++) {
      char nm[16];
      snprintf(nm, sizeof(nm), "crash%d.wav", i + 1);
      SND_WANT(nm, &snd_crash[i]);
      snprintf(nm, sizeof(nm), "lowcrash%d.wav", i + 1);
      SND_WANT(nm, &snd_lowcrash[i]);
      snprintf(nm, sizeof(nm), "skid%d.wav", i + 1);
      SND_WANT(nm, &snd_skid[i]);
      snprintf(nm, sizeof(nm), "dustskid%d.wav", i + 1);
      SND_WANT(nm, &snd_dustskid[i]);
      snprintf(nm, sizeof(nm), "scrape%d.wav", i + 1);
      SND_WANT(nm, &snd_scrape[i]);
    }
    SND_WANT("tires.wav", &snd_tires);
    SND_WANT("wasted.wav", &snd_wasted);
    SND_WANT("firewasted.wav", &snd_firewasted);
#undef SND_WANT

    VfsZip sounds_zip;
    if (vfs_read_zip("data/sounds.zip", &sounds_zip)) {
      for (int32_t i = 0; i < sounds_zip.count; i++) {
        VfsZipEntry *e = &sounds_zip.entries[i];
        for (int32_t j = 0; j < nt; j++) {
          if (strcmp(e->name, table[j].name) == 0) {
            if (!wav_decode(e->data, (size_t)e->len, table[j].dst)) {
              fprintf(stderr, "data/sounds.zip: %s failed to decode (wav)\n", e->name);
            }
            break;
          }
        }
      }
      vfs_free_zip(&sounds_zip);
    } else {
      fprintf(stderr, "could not load data/sounds.zip -- sound effects disabled\n");
    }
  }
  load_interface_track(&interface_music);
  boot_loading_frame(&g, &boot_images, boot_kb_total, boot_kb_total);

  // Headless verification hook: NFM_SCREENSHOT_PPM=/path/out.ppm dumps the
  // framebuffer after N frames and exits, so this can be checked from a
  // script/CI without an interactive display. Not part of the game loop
  // proper -- no way to eyeball a GL window in this sandbox. Since there's
  // also no way to simulate menu input in that same sandbox (see
  // TASKS_NATIVE.md's notes on xdotool not working under the headless
  // Xvfb this runs under), setting it also skips straight to racing with
  // the default car/stage (index 0 / stage 1) so existing screenshot-based
  // regression checks keep working unmodified.
  const char *screenshot_path = getenv("NFM_SCREENSHOT_PPM");
  const char *screenshot_frame_env = getenv("NFM_SCREENSHOT_FRAME");
  int32_t screenshot_frame = screenshot_frame_env ? atoi(screenshot_frame_env) : 0;
  // NFM_SCREENSHOT_COUNT=n dumps n consecutive frames (path.0, path.1, ...),
  // to see what happens between two race ticks.
  const char *screenshot_count_env = getenv("NFM_SCREENSHOT_COUNT");
  int32_t screenshot_count = screenshot_count_env ? atoi(screenshot_count_env) : 1;
  // NFM_SCREENSHOT_MENU=pausereplay picks Instant Replay at this frame
  // (default: 60 frames before the dump), so a run can dump any point of it.
  // NFM_SIM_TICKS_PER_FRAME=n: the race runs exactly n ticks every frame,
  // whatever the clock says -- a headless run then simulates a known number
  // of ticks by a given frame, and two runs can be compared.
  const char *sim_ticks_env = getenv("NFM_SIM_TICKS_PER_FRAME");
  const int32_t sim_ticks_per_frame = sim_ticks_env ? atoi(sim_ticks_env) : 0;
  const char *hook_replay_env = getenv("NFM_HOOK_REPLAY_FRAME");
  int32_t hook_replay_frame = hook_replay_env ? atoi(hook_replay_env) : screenshot_frame - 60;

  // --- Menu state (main -> gamemode -> car -> stage -> race) ---
  // See native/docs/MENU_FLOW.md §2 for the full Java fase-transition
  // graph this mirrors (single-player subset only -- no netplay).
  // NFM_SCREENSHOT_MENU={main|gamemode|car|stage} keeps the screenshot
  // hook on the named menu screen instead of skipping straight to racing
  // -- lets a headless test dump the menu without needing to simulate the
  // ENTER press (xdotool doesn't work under this sandbox's Xvfb, see the
  // Menu comment above).
  const char *screenshot_menu = getenv("NFM_SCREENSHOT_MENU");
  GameState state;
  bool preview_race_lose = false;
  if (screenshot_menu && strcmp(screenshot_menu, "main") == 0) state = STATE_MAIN_MENU;
  else if (screenshot_menu && strcmp(screenshot_menu, "mainsettings") == 0) state = STATE_MAIN_MENU;
  else if (screenshot_menu && strcmp(screenshot_menu, "bench") == 0) state = STATE_MAIN_MENU;
  else if (screenshot_menu && strcmp(screenshot_menu, "gamemode") == 0) state = STATE_GAMEMODE_MENU;
  else if (screenshot_menu && strcmp(screenshot_menu, "instructions") == 0) state = STATE_INSTRUCTIONS;
  else if (screenshot_menu && strcmp(screenshot_menu, "credits") == 0) state = STATE_CREDITS;
  else if (screenshot_menu && strcmp(screenshot_menu, "stage") == 0) state = STATE_STAGE_SELECT;
  else if (screenshot_menu && strcmp(screenshot_menu, "car") == 0) state = STATE_CAR_SELECT;
  else if (screenshot_menu && strcmp(screenshot_menu, "postwin") == 0) state = STATE_POST_RACE;
  else if (screenshot_menu && strcmp(screenshot_menu, "postlose") == 0) { state = STATE_POST_RACE; preview_race_lose = true; }
  else if (screenshot_menu && strcmp(screenshot_menu, "loading") == 0) state = STATE_STAGE_LOADING;
  else if (screenshot_menu && strcmp(screenshot_menu, "intro") == 0) state = STATE_STAGE_INTRO;
  else if (screenshot_menu && strcmp(screenshot_menu, "locked") == 0) state = STATE_STAGE_LOCKED;
  else if (screenshot_menu && strcmp(screenshot_menu, "holdcard") == 0) state = STATE_RACING;
  else if (screenshot_menu && strcmp(screenshot_menu, "boot") == 0) state = STATE_BOOT_CLICK;
  else if (screenshot_menu && strcmp(screenshot_menu, "rad") == 0) state = STATE_BOOT_RAD;
  // A real start goes through the original's boot sequence: loading()
  // (drawn while the assets loaded, above), then fase 111's clicknow()
  // prompt and fase 9's Radicalplay intro, then the main menu
  // (GameSparker.java:263-296).
  else state = screenshot_path ? STATE_RACING : STATE_BOOT_CLICK;
  // Boot sequence state: boot_n is GameSparker's n8 (iterations spent in
  // the current boot fase); radpx/pin are xtGraphics.rad()'s own logo
  // slide state.
  int32_t boot_n = 0, boot_radpx = 212, boot_pin = 0;
  bool boot_aflk = false;
  // NFM_CAR_INDEX/NFM_STAGE_NUM -- diagnostic overrides for the screenshot
  // hook's default car/stage, so a headless run can sweep combinations
  // other than car 0 / stage 1 without needing real menu-navigation input
  // (this sandbox's Xvfb can't simulate keyboard input, see the Menu
  // comment above). Not read at all once state != STATE_RACING-via-hook.
  int32_t car_index = 0;   // 0-15 NFM 2, 16-38 Extended, CUSTOM_CAR_INDEX (39) = Simple_Car.rad
  int32_t stage_num = 1;   // 1-based, matches stages/N.txt
  {
    const char *car_env = getenv("NFM_CAR_INDEX");
    if (car_env) car_index = atoi(car_env);
    const char *stage_env = getenv("NFM_STAGE_NUM");
    if (stage_env) stage_num = atoi(stage_env);
  }
  char stage_name_buf[64];
  stage_read_name(stage_num, stage_name_buf, sizeof(stage_name_buf));

  // Car-select screen's live 3D preview spin state -- see
  // draw_car_preview()'s own doc comment. Persistent across frames so the
  // spin is continuous, not reset every draw.
  int32_t car_preview_xz = 0;
  int32_t car_preview_wzy = 0;
  // Car-switch transition -- xtGraphics.java:6335-6425's `flipo`/`nextc`
  // state machine (carselect() only runs its normal idle-spin/input code
  // inside `if (flipo == 0)`; a LEFT/RIGHT press sets flipo=20 and drives
  // a 20-frame fall-away/rise-in animation instead of an instant swap --
  // see draw_car_preview()'s own doc comment for the exact per-frame
  // math). 0 = idle (not transitioning).
  int32_t car_flipo = 0;
  int32_t car_nextc = 0; // +1 (next/RIGHT) or -1 (prev/LEFT) for the in-flight transition
  int32_t car_transition_y = 0;  // accumulated y offset while car_flipo != 0
  int32_t car_transition_zy = 0; // accumulated tumble angle while car_flipo != 0
  // Locked-car gate overlay -- xtGraphics.java:598/5327-5364/6364. `gatey`
  // slides the whole 9-segment fence arch down from +300 to 0 over 3
  // frames (-100/frame) once settled on a locked car; pgady[]/pgas[] drive
  // a left-to-right "wave" bounce across the 9 segments while gatey==0.
  // Reset to 300 whenever the car-switch transition is active (matches
  // Java's own :6364, which sets this EVERY frame flipo!=0, not just once)
  // so the gate always re-slides-in fresh after switching cars.
  int32_t car_gatey = 300;
  int32_t car_pgady[9] = {0};
  bool car_pgas[9] = {false};
  // inishcarselect() (xtGraphics.java:4844, called from GameSparker.java's
  // fase==-9 transition on every single path into car-select -- see
  // CarSmokeWarp's own doc comment) -- set true at every
  // `state = STATE_CAR_SELECT` transition below, consumed at the top of
  // the STATE_CAR_SELECT draw block to (re-)arm the smoke-warp intro.
  bool car_select_needs_intro = true;
  int32_t stage_preview_loaded_num = -1; // -1 = nothing loaded yet, see the STAGE_SELECT draw block below
  bool stage_preview_ok = true;          // that load passed game_sparker_loadstage's checks

  // Post-race unlock-celebration card's live 3D car spin state --
  // xtGraphics.java:6816/6819 (`contO.xz += 5`, `contO2.wzy -= 10` every
  // draw, unconditionally -- no idle/frozen state machine like the car-
  // select preview has, just a continuous spin for as long as the card
  // shows). Persistent so the spin doesn't reset every frame.
  int32_t post_unlock_car_xz = 0;
  int32_t post_unlock_car_wzy = 0;

  // Stage-27 campaign-completion card's scrolling-logo state --
  // xtGraphics.java:337/556 (`radpx`, initialised to 212) and the shared
  // `flipo` counter :6883-6889 uses here. radpx == 212 is the SETTLED
  // position: the logo sits centred and the "A Game by Radicalplay.com"
  // caption under it is drawn (:6890-6899). Every 70 draws flipo hits 40
  // and kicks radpx to 213, which un-settles it; from there it slides
  // +40/draw, wraps -468 once past 800, and lands back on exactly 212
  // after 17 more steps (212 - (-468) == 680 == 40*17), re-settling and
  // bringing the caption back. Persistent across frames, same as the
  // sibling spin state above.
  int32_t post_radpx = 212;
  int32_t post_flipo = 0;

  // Main menu (fase 10) animation state -- ported 1:1 from xtGraphics.java's
  // own instance fields. See native/docs/MENU_FLOW.md §3.2 for the exact
  // shape of each animation.
  // - mainbg_bgmy[]: bgmain.jpg vertical scroll offsets (two tiles, so the
  //   scroll wraps seamlessly). Java xtGraphics.java:1739-1740: bgmy[0]=0,
  //   bgmy[1]=-400. Advance by n2=8 per frame for mode 1 (main menu).
  // - flkat/gxdu/gydu/movly: face-blink animation (dude[0] alpha + jitter).
  //   Java xtGraphics.java:4319-4342.
  // - opselect: currently-highlighted option index (0..N-1).
  // - aflk: alternates every draw to make the highlighted option's outline
  //   flicker between two colours -- same aflk field the drawcs() text
  //   messages use, real shared state in the original, not a coincidence.
  int32_t mainbg_bgmy[2] = {0, -400};
  int32_t mainmenu_flkat = 0;
  int32_t mainmenu_gxdu = 0, mainmenu_gydu = 0;
  int32_t mainmenu_movly = 0;
  int32_t mainmenu_opselect = 0;
  int32_t gamemode_opselect = 0;
  // Extended's normal mode (the game-mode menu's fourth row): Free Play's
  // flow with Extended's stages, 11 cars and its own unlocking (ext_mode.c).
  bool ext_normal = false;
  bool ext_justwon = false;      // this race unlocked the next normal-mode stage
  int32_t ptmatch = 0;           // the Premier Tournament's match, 1..5 (stage 26)
  ExtProgress ext_prog;
  ext_progress_reset(&ext_prog);
  // NFM_EXT_NORMAL=1 (with NFM_STAGE_NUM, NFM_PTMATCH): race Extended's
  // normal mode headless.
  if (getenv("NFM_EXT_NORMAL")) {
    ext_normal = true;
    if (getenv("NFM_PTMATCH")) ptmatch = atoi(getenv("NFM_PTMATCH"));
  }
  static ExtPT pt;               // the Premier Tournament (ext_pt.c)
  // Extended's career ("RPG Mode", the game-mode menu's fifth row): normal
  // mode's flow (ext_normal stays true) on careertracks.radq, with levels,
  // experience and stat points per car (career.c). Saved to career.txt.
  bool ext_career = false;
  static CareerSave csave;
  // The save as the race started: quitting or restarting before the race
  // ends puts it back, so an unfinished race pays nothing (this port's
  // own -- the original kept the experience, levels and bonus points).
  static CareerSave csave_race;
  static CareerRace crace;
  static CareerRun crun;          // this race's experience (career.c)
  int32_t career_seen_clear = 0;  // the player's checkpoints already paid for
  int32_t career_seen_dested[NFM_MAX_CARS] = {0};
  static CareerBots cbots;        // this race's recorded bots (career_bots.c)
  // The stage's own scripts (career_stage.c, careermode$m): the undead, the
  // ghost, the tower's floors, the Titan... and the race they act on.
  static CareerStage cstage;
  static CareerStageWorld cworld;
  static bool cstage_ghost[NFM_MAX_CARS][NFM_MAX_CARS];
  static bool cstage_arrow_skip[NFM_MAX_CARS];
  static bool cstage_board_skip[NFM_MAX_CARS];
  static CareerMedium cstage_medium;   // stages 22 and 23's colours
  int32_t career_walls[4] = {0, 0, 0, 0};
  bool cstage_boss_music = false;   // the fight's music is loaded in g_career_ogg
  WavClip snd_redflash = {0}, snd_caught = {0};
  bool cstage_snd_loaded = false;
  // The car list's names in the career: "Beast ", "Shadow ", "Undead "
  // before the car's (stat$m's namestouse, XT 5200-5213).
  static char career_names_buf[NFM_MAX_CARS][48];
  static const char *career_names[NFM_MAX_CARS];
  // Cars the last win opened (finish()'s congratulations, XT 12116-12167).
  int32_t career_new_cars[4] = {-1, -1, -1, -1};
  // A beaten stage's options (XT stageselect 13482-13523): 0 as it is,
  // 1 hard mode (the newest stage's field), 2 scale levels (to yours),
  // 3 no levels (everyone level 1, no experience).
  int32_t career_mode = 0;
  int32_t cbots_last_clear[NFM_MAX_CARS] = {0}, cbots_since[NFM_MAX_CARS] = {0};
  bool career_settled = false;    // the race's end already paid or taken back
  // The car select's stat panel: the player's points into the six stats
  // (XT carselect 15705-15760, statincrease), one row at a time.
  bool career_panel = false;
  int32_t career_panel_row = 0;
  // Points put in since the panel opened: the view button takes them back
  // (this port's own; the original's points stick the moment they go in).
  int32_t career_pend[2][CS_N > CAREER_PERK_SLOTS ? CS_N : CAREER_PERK_SLOTS] = {{0}};
  // The panel's pages: the stat points, the perks (car points), and the
  // car's options -- carselect's reshuffle, level transfer and sell
  // (shufflefase 1-11, savefase 1), each behind a YES / NO question.
  int32_t career_panel_page = 0;
  int32_t career_confirm = 0;          // the question up (CC_*), 0 none
  bool career_confirm_yes = false;     // NO is the default answer
  int32_t career_transfer_from = -1;   // a level transfer looking for its car (shufflefase 5), Extended number
  if (getenv("NFM_CAREER_PANEL")) career_panel = true;   // headless: the panel open
  // Headless: its page and row, a question, a transfer's source car.
  if (getenv("NFM_CAREER_PANEL_PAGE")) career_panel_page = atoi(getenv("NFM_CAREER_PANEL_PAGE")) % 3;
  if (getenv("NFM_CAREER_PANEL_ROW")) career_panel_row = atoi(getenv("NFM_CAREER_PANEL_ROW"));
  if (getenv("NFM_CAREER_CONFIRM")) career_confirm = atoi(getenv("NFM_CAREER_CONFIRM"));
  if (getenv("NFM_CAREER_TRANSFER")) career_transfer_from = atoi(getenv("NFM_CAREER_TRANSFER"));
  // Scouting (fase 205): the field, one opponent a page, before the start.
  bool career_scout = false;
  int32_t career_scout_page = 1;
  int32_t career_scout_flash = 0;
  bool career_scout_up = true;
  career_reset(&csave);
  char career_path[1100] = "";
  // NFM_CAREER=1 (with NFM_STAGE_NUM, NFM_CAR_INDEX): race the career headless.
  if (getenv("NFM_CAREER")) {
    ext_normal = true;
    ext_career = true;
    g_ext_career = true;
    if (getenv("NFM_CAREER_BONUS")) g_career_bonus = atoi(getenv("NFM_CAREER_BONUS"));   // 1-4
  }
  if (state == STATE_STAGE_INTRO && ext_career && getenv("NFM_CAREER_SCOUT")) {
    career_scout = true;   // headless: scouting at that page
    career_scout_page = atoi(getenv("NFM_CAREER_SCOUT"));
  }
  // The tournament is on: normal mode's stage 26 with a match picked.
#define PT_ACTIVE (ext_normal && !ext_career && stage_num == EXT_PT_STAGE && ptmatch > 0)
  // The stage reached and the last one, normal mode's or the career's.
#define EXT_UNLOCKED (ext_career ? csave.unlocked : ext_prog.normal_unlocked)
#define EXT_LAST_STAGE (ext_career ? CAREER_STAGES : EXT_NORMAL_STAGES)
#define MUSIC_MODE (ext_career ? 4 : (ext_normal ? 3 : gmode))
  bool mainmenu_aflk = false;
  // Java xtGraphics.java:416 (`firstime = true`) + :44 (`oldfase`) -- the
  // first-run Instructions gate. The VERY first time the player picks
  // "Play Game" from the main menu, Java does NOT go straight to the
  // gamemode submenu: it detours through the Instructions screen
  // (:4480-4484, `oldfase=102; fase=11`), and Instructions' own exit
  // (:4288, `fase = oldfase`) then lands on the gamemode submenu rather
  // than back at the main menu. Picking Instructions from the menu
  // directly instead sets `oldfase=10` (:4464) so it returns to the main
  // menu -- and ALSO consumes the flag (:4466), so the Play Game detour
  // only ever happens if the player didn't already read them. Both flags
  // live here for the same reason Java keeps them on `this`: they
  // outlive any single screen.
  bool menu_firstime = true;
  GameState instructions_return_to = STATE_MAIN_MENU;
  // Instructions flipbook state -- Java's own `flipo`/`dudo`/`duds` on
  // `this` (see draw_instructions' doc comment for the page numbering).
  // `inst_bgf` is mainbg(2)'s colour-fade accumulator, separate from the
  // shared bgmy scroll offsets it rides alongside.
  int32_t inst_flipo = 0;
  int32_t guide_page = 0;    // the Extended guide's page, while inst_flipo == GUIDE_FLIPO
  int32_t credits_page = 0;  // 0 the original's credits, then draw_extra_credits'
  // Headless: NFM_GUIDE_PAGE=n opens the instructions on the guide's page n,
  // NFM_CREDITS_PAGE=n the credits on page n.
  if (getenv("NFM_GUIDE_PAGE")) inst_flipo = GUIDE_FLIPO, guide_page = atoi(getenv("NFM_GUIDE_PAGE"));
  if (getenv("NFM_INST_PAGE")) inst_flipo = atoi(getenv("NFM_INST_PAGE"));   // headless: an instructions page (1, 3 .. 15, 16)
  if (getenv("NFM_CREDITS_PAGE")) credits_page = atoi(getenv("NFM_CREDITS_PAGE"));
  int32_t inst_dudo = 0;
  int32_t inst_duds = 0;
  float inst_bgf = 0.2f;
  // Post-race outcome. Set when transitioning STATE_RACING -> STATE_POST_RACE
  // by check_win_condition below. `true` = won (finished all laps first),
  // `false` = lost (destroyed / wasted / would-be-eliminated). Read by the
  // POST_RACE draw block to pick congrd/gameov + text.
  bool race_winner = !preview_race_lose;
  // Name of whichever bot finished first when the player didn't -- Java
  // xtGraphics.java:7757 (`this.cd.names[this.sc[n2]] + " finished first,
  // race over!"`). Only read/formatted at the moment a bot wins; empty
  // otherwise.
  char race_lost_car_name[32] = "";
  // Which of the three hold-card endings fired -- xtGraphics.java:7683-7761,
  // checked in this exact priority order every tick (mirrored in the tick
  // loop's end-of-race scan below): all-other-cars-wasted (win) beats
  // player-wasted (lose) beats the normal cross-the-finish-line scan, since
  // the real code's `!this.holdit` guard on each successive `if` means only
  // the FIRST one to see its own condition true in a given tick can latch.
  // Picks both the hold-card image (youwastedem/yourwasted vs youwon/
  // youlost) and whether a blinking "You Won..." message draws under it
  // (RACE_END_PLAYER_WASTED draws only the card, no blink line -- see
  // :7708-7736, which has no `drawcs(120, ...)` call at all, unlike the
  // other two endings).
  typedef enum { RACE_END_FINISH, RACE_END_ALL_WASTED, RACE_END_PLAYER_WASTED } RaceEndKind;
  RaceEndKind race_end_kind = RACE_END_FINISH;
  // This race follows Free Play's Race Setup (settings.fp_*), and whether
  // its specials run (NFM 1 and 2 race without them).
  bool fp_race = false, race_specials = true;
  // NFM's stage 10 races without the arrow, the radar or their messages; on
  // Extended's own stages (the career's 10, Free Play's 28 on) that rule is
  // gone, as it is in Extended.
#define NFM_NO_ARROW_STAGE (cp.stage == 10 && xt.classicmode)
  // gmode is Java xtGraphics.gmode: 0 = Free Play (any car, any stage,
  // no progression); 1 = NFM 1 campaign (stages 1..10 sequential);
  // 2 = NFM 2 campaign (stages 11..27 sequential). Set by the gamemode
  // submenu on ENTER (Java maini2 line 4640-4674). Currently only the
  // value is registered -- Parts 5/6 will read it to gate the stage
  // picker and set the initial stage. Default 0 = Free Play so any
  // pre-Part-3 launch path (screenshot hooks, direct STATE_RACING) keeps
  // its current behaviour.
  int32_t gmode = 0;
  {
    // NFM_GAMEMODE -- diagnostic override alongside NFM_CAR_INDEX/
    // NFM_STAGE_NUM above, so a headless run can preview car/stage-select
    // gating (locked-car gate overlay, cantgo screen) in a specific
    // campaign without real menu-navigation input. 0=Free Play (default),
    // 1=NFM1, 2=NFM2.
    const char *gmode_env = getenv("NFM_GAMEMODE");
    if (gmode_env) gmode = atoi(gmode_env);
  }

  // Persistent progression (unlocked stages/cars per campaign + justwon
  // latches) -- loaded from platform_progress_path() (see platform.h --
  // $XDG_DATA_HOME/nfm-psivta/progress.bin on desktop, ux0:data/.../
  // progress.bin on Vita) at startup so beating stages sticks across
  // sessions. Missing/corrupt save resets to Java's initial state
  // (unlocked={1,1}, scm={0,0}); a platform with no usable save location
  // at all (progress_path_ok false) just never persists progress this
  // session, same fail-soft philosophy as a missing data/images.zip.
  GameProgress progress;
  char progress_path[1024];
  bool progress_path_ok = platform_progress_path(progress_path, sizeof(progress_path));
  if (progress_path_ok) diag_init(progress_path);
  // Settings screen (pause menu -> Settings): motion-blur intensity 0..100
  // in steps of 20, persisted beside the progress file. the screen state is
  // the highlighted row (0 = Motion Blur slider, 1 = Back).
  // Also Graphics (see scene_targets_build), Screen Shake and Vibration.
  GameSettings settings = game_settings_defaults(NFM_DEFAULT_GRAPHICS);
  if (progress_path_ok) game_settings_load(progress_path, &settings);
  if (settings.graphics != GFX_ORIGINAL) {
    motion_blur_ok = scene_targets_build(&scene_rt, &accum_rt, motion_blur_ok, width, height, settings.graphics);
  }
  apply_settings(&settings, &m);
  SettingsUi settings_ui = {.page = SET_MAIN};
  // Where Back leaves Settings: the pause menu or the main menu.
  GameState settings_return = STATE_PAUSED;
  // NFM_SCREENSHOT_MENU=racesetup (NFM_HOOK_RACESETUP_PAGE=1: its rivals): Free Play's Race Setup, headless.
  if (getenv("NFM_SCREENSHOT_MENU") && strcmp(getenv("NFM_SCREENSHOT_MENU"), "racesetup") == 0) {
    settings_ui = (SettingsUi){.page = getenv("NFM_HOOK_RACESETUP_PAGE") ? SET_RIVALS : SET_RACE, .row = 0};
    settings_return = STATE_STAGE_SELECT;
    state = STATE_SETTINGS;
  }
  if (progress_path_ok) {
    game_progress_load_from_disk(&progress, progress_path);
    ext_progress_load(progress_path, &ext_prog);
    {
      const char *slash = strrchr(progress_path, '/');
      const int dir = slash ? (int)(slash - progress_path) + 1 : 0;
      snprintf(career_path, sizeof(career_path), "%.*scareer.txt", dir, progress_path);
      career_load(career_path, &csave);
    }
  } else {
    game_progress_reset(&progress);
  }

  // Track backdrop scroll state -- see draw_trackbg() above. Persistent
  // across state transitions so the two tiles keep sliding smoothly
  // whether we're in STATE_STAGE_LOCKED (cantgo overlay), STATE_STAGE_LOADING
  // (post-stage-select transition), or any other consumer.
  TrackBgState trackbg_state;
  trackbg_init(&trackbg_state);

  // STATE_STAGE_LOCKED countdown -- Java's `lockcnt` decremented every
  // cantgo() frame; screen returns to stage-select when it hits zero or
  // the player presses any nav key. Set on entering STATE_STAGE_LOCKED.
  // Screenshot hook seeds with a huge value so the frame the ppm dump
  // grabs is still on the locked screen, not the auto-transition back.
  int32_t stage_lockcnt = (screenshot_menu && strcmp(screenshot_menu, "locked") == 0) ? 100000 : 0;

  // STATE_STAGE_LOADING countdown -- Java's loadingstage() runs while
  // the stage geometry is actually being fetched from the network; our
  // stage loader is synchronous but we still hold the animated
  // transition for a short beat so the trackbg scroll is visible.
  int32_t stage_loadcnt = (screenshot_menu && strcmp(screenshot_menu, "loading") == 0) ? 100000 : 0;

  // STATE_STAGE_INTRO -- hipnoload()'s own state (xtGraphics.java:2685).
  // intro_frame counts the screen's paced draws: draw 0 is loadmusic()'s
  // "N KB / Please Wait..." card, every later one musicomp()'s "Loading
  // complete!" card. The images are loadopsnap()'d per stage at setup.
  int32_t intro_frame = 0;
  int32_t intro_dudo = 150, intro_duds = 0, intro_pstar = 0;
  bool intro_aflk = false, intro_tflk = false;
  bool stage_music_pending = false; // loaded, waiting for loadmusic()'s strack.play()
  char stage_asay[96] = "";
  HudImg intro_loadingmusic = {-1, 0, 0}, intro_flaot = {-1, 0, 0};
  HudImg intro_star[2] = {{-1, 0, 0}, {-1, 0, 0}};
  // When previewing the locked state headlessly, park stage_num on the
  // slot that would actually be locked given the initial progression
  // (unlocked[0]==1 -> stage 2 is the cantgo target for NFM1).
  if (screenshot_menu && strcmp(screenshot_menu, "locked") == 0) {
    gmode = 1;
    stage_num = 2;
    stage_read_name(stage_num, stage_name_buf, sizeof(stage_name_buf));
  }

  // Menu navigation reads raw logical-button edges (rising edge only, so
  // holding a key doesn't scroll the list every frame) -- deliberately
  // NOT going through Control (input_poll's own struct is a
  // level-triggered continuous drive-input mapping, the wrong shape for
  // discrete menu paging). previous_held is refreshed every frame after
  // use (see the KEY_EDGE macro right where the loop reads platform_poll()
  // below).
  bool previous_held[BTN_COUNT] = {0};

  // Racing-only state -- left uninitialised until the STATE_STAGE_SELECT
  // -> STATE_RACING transition below actually populates it exactly once.
  // Room for NFM_MAX_CARS racers, index 0 always the human; NFM 2 races 7
  // (xtGraphics.java's inishcarselect(), `this.nplayers = 7`), Extended's
  // normal mode 11 (race_nplayers below). Slots 1.. are AI-controlled via
  // control_preform(), see the STATE_RACING tick loop below. Static: with
  // twenty cars the Record and these arrays are too big for the stack.
  // Zeroed, not left uninitialised: every (re)build of a car goes through
  // cont_o_recopy(), which frees what the slot held before.
  static ContO co[NFM_MAX_CARS];
  ContO *stage_objects = NULL;
  int32_t stage_count = 0;
  static Record rpd;
  memset(&rpd, 0, sizeof(rpd)); // record_free() before each race's record_init() needs it valid
  static CheckPoints cp;   // ~70KB with Extended's room (check_points.h): off the stack
  XtGraphicsStub xt;
  static Control control[NFM_MAX_CARS];
  static Mad mad[NFM_MAX_CARS];
  static CarDefine live_cd[NFM_MAX_CARS]; // each racing car's own stats (see mad_init below)
  static CarDefine race_base[NFM_MAX_CARS]; // what specials rebuild each live_cd from (the career's points in)
  static Specials specials;                    // Extended's specials (specials.c)
  bool ext_listbars = false;
  int32_t car_filter = CF_ALL;                 // car select's filter (car_filters)                   // Extended's control.swap: list bars show specials
  // Which car (0-15) each slot drives -- ports xtGraphics.java's own
  // persistent `sc[]` field (xtGraphics.java:94,460 -- `new int[]{0,0,...}`,
  // never reset between races). sc[0] (the player's own car) is refreshed
  // every time this block runs, from car_index; sc[1..6] are ONLY ever
  // written by bots_sortcars(), so between races within this session they
  // legitimately hold whichever cars the LAST race's opponents drove until
  // sortcars() rerolls them a moment later -- see the STATE_RACING setup
  // block for why that stale-read-before-reroll ordering is deliberate,
  // not a bug (GameSparker.java:2723-2726: the u[].reset() loop runs
  // BEFORE resetstat()'s own sortcars() call).
  int32_t sc[NFM_MAX_CARS] = {0};
  // xtGraphics.java:2354-2358 (loadstage()) -- NFM1 races only 5 cars,
  // not 7; every per-race loop below (construction, collision, checkstat,
  // AI, cleanup) is bounded by this instead of BOTS_MAX_PLAYERS, matching
  // Java's own xtGraphics.nplayers exactly. bots_sortcars() already knows
  // this (it leaves sc[5]/sc[6] at their -1 reset value for GMODE_NFM1,
  // see bots.c) -- reading base_models[sc[i]] for an unbuilt slot i>=
  // nplayers is what used to segfault (reproduced via NFM1 stage 1).
  int32_t nplayers = BOTS_MAX_PLAYERS;
  // The next race's car count when a mode wants other than NFM 2's 7 (and
  // NFM 1's 5): Extended's normal mode sets 11. 0 = the mode's own.
  // NFM_PLAYERS=n sets it headless.
  int32_t race_nplayers = 0;
  if (getenv("NFM_PLAYERS")) {
    race_nplayers = atoi(getenv("NFM_PLAYERS"));
    if (race_nplayers < 1) race_nplayers = 0;
    if (race_nplayers > NFM_MAX_CARS) race_nplayers = NFM_MAX_CARS;
  }
  ContO **all_objs = NULL;
  int32_t *visible_idx = NULL;
  int32_t *rank = NULL;
  int32_t *order = NULL;
  int32_t total_objs = 0;

  // Countdown state (xtGraphics.java:45,270 -- fields; :1501-1502 -- reset
  // to 130 and 3 at fase 1 entry). starcnt decrements every tick from 130;
  // the visible 3/2/1/GO overlay draws only when starcnt is in (0, 35].
  // gocnt is which glyph to draw right now: 3 (init) -> 2 at starcnt==24 ->
  // 1 at starcnt==13 -> 0 (GO) at starcnt==2. See draw block below for the
  // exact x-offsets Java uses (:8051 vs :8054 -- GO glyph is wider so it
  // shifts left 22 px). No car can move before GO fires -- the tick loop
  // below skips colide/drive/checkstat/preform entirely while starcnt !=
  // 0, matching GameSparker.java:939's own gate exactly (every car sits
  // frozen at its starting-grid position, not just the player's input).
  int32_t starcnt = 130;
  // The fly-around before the countdown can be skipped with handbrake/
  // enter, but not by the press that confirmed the stage card: musicomp()
  // consumes it (`control.handb = false`, xtGraphics.java:3150), and here
  // the pad is polled by level, so a button still held from that confirm
  // read as a skip on the race's first frame. Armed once it is released.
  bool flyby_skip_armed = true;
  // The loop iteration whose confirm started the race: Return is both
  // confirm and pause here, so that same press must not also pause the
  // race it just started (musicomp() sets `control.enter = false`).
  int32_t intro_confirm_frame = -1;
  int32_t gocnt = 3;

  // Win hold-card state -- Java xtGraphics.java:7737-7751 (holdit set true
  // + youwon/youlost.gif card starts drawing the instant ANY car crosses
  // the finish line first) and :7554-7571 (stat()'s own holdcnt++ /
  // advance rule: ENTER or holdcnt > 250 ticks moves on). We reuse this
  // for the genuine Java-modeled win/lose path (see the tick loop's own
  // finish-line scan across every car); the F10 manual-abandon hotkey is
  // our own port's convenience, not a real Java trigger, so it skips
  // straight to STATE_POST_RACE with no hold card -- see the end-of-race
  // detection block below.
  bool race_holdit = false;
  int32_t race_holdcnt = 0;
  bool race_hold_aflk = false; // local blink toggle for the hold-card message, mirrors Java's shared aflk

  // Countdown mascot -- xtGraphics.java:8032-8049. The face is drawn at
  // 30% alpha behind the 3/2/1/GO glyphs, mouth open (dude[1]) during the
  // window around each number and dude[2] once GO lands. `race_dudo` is
  // its x, chosen once per race in musicomp() (:3144-3149) as one of two
  // fixed positions, so the face does not always appear on the same side.
  int32_t race_dudo = 250;

  // Pause-menu state -- xtGraphics.java's pausedgame() (:4695) and the two
  // fases it can hand off to. `pause_opselect` is the highlighted row
  // (0 Resume / 1 Watch Replay / 2 Game Instructions / 3 Quit); Java uses
  // its one shared `opselect` field, but this port keeps per-screen copies
  // so the pause menu and the gamemode menu cannot clobber each other.
  // `pause_replay_tick` is fase -1's own n7 cursor over the 300-tick ring,
  // and `cantreply_cnt` counts fase -8's 150-frame auto-dismiss (:1679).
  int32_t pause_opselect = 0;
  bool restart_race = false;   // Restart Race picked: free the world and load the stage again
  int32_t pause_replay_tick = 0;
  int32_t cantreply_cnt = 0;
  // fleximg: fase -6's one-frame pauseimage() of the frame underneath,
  // taken from the previous frame's render target at the top of the next
  // frame (before it is reused), on the way into the pause menu and again after its replay
  // (GameSparker.java:1340 goes back through fase -6). `pause_flex_tex`
  // stays -1 without render targets, and the menu then falls back to
  // redrawing the frozen scene.
  bool pause_snapshot_pending = false;
  int32_t pause_flex_tex = -1;
  uint8_t *pause_flex_read = NULL, *pause_flex_rgba = NULL;

  // Replay state -- GameSparker.java's own n7/n8/n9/n10/n11 locals (declared
  // once at the top of its outer render loop, :256-260, so they persist
  // tick-to-tick across the whole fase==-3 camera choreography exactly like
  // starcnt/gocnt above). See the STATE_REPLAY block for what each drives:
  // replay_tick (n7) is the 0-299 index into record's frozen ring;
  // replay_cam (n8) is the sub-state within whichever camera dance is
  // playing; replay_loops (n9) counts full 300-tick passes (forces an
  // advance after 2, :1478); replay_variant (n10) picks one of the three
  // random "crash reel" flavors (67/69/30, :1397-1404); replay_flicker
  // (n11) debounces the n10==30 variant's random strobe (:1611-1618).
  int32_t replay_tick = 0;
  int32_t replay_cam = 0;
  int32_t replay_loops = 0;
  int32_t replay_variant = 0;
  int32_t replay_flicker = 0;

  // "Motion blur" trail state -- GameSparker.java:80,152 (`mvect`, default
  // 100) and :1817 (`shaka`, the screen-shake-from-crash countdown --
  // distinct from `contO.shaka`/`shakedam`, which is the per-tick damage
  // magnitude THIS reads from). NOT ported to web/*.js at all (see this
  // block's own call site further down for the full explanation) --
  // translated straight from the decompiled Java. `mvect` is this port's
  // own name match for Java's field: how opaque (0-100, /100 for the
  // real alpha) each frame's blit onto the previous one is -- LOWER
  // during a sharp turn means MORE of the previous frame's pixels show
  // through, i.e. more visible trailing. `lmxz` is last TICK's camera
  // heading (`medium.xz`), needed to measure how much it moved THIS
  // tick. `shaka` decays once per FRAME (see the draw-side call site),
  // armed once per TICK whenever the local player just took damage.
  int32_t mvect = 100;
  int32_t shaka = 0;
  int32_t lmxz = 0;
  // GameSparker.java's own `view` field, cycled 0->1->2->0 by the V key
  // (:3634-3639) and dispatched at :971-1000: 0 = chase/follow,
  // 1 = orbit/around, 2 = fixed "watch" tripod. Previously the port had
  // no such variable and always ran view 0.
  int32_t race_view = 0;

  // Checkpoint-sound edge tracker -- mirrors xt_graphics.c's own
  // `xt->hud_clear != mad->clear` check (used there to trigger the
  // "Checkpoint!" text) but tracked independently here so audio fires
  // once per tick-loop pass through check_points_checkstat rather than
  // once per DRAWN frame -- see the tick-loop trigger site's own comment.
  int32_t audio_last_clear = 0;

  // Physics-SFX loop channel tracking (Part 16) -- audio_mixer is
  // channel-indexed, not name-indexed like web/audio.js's own Map-based
  // Audio class, so this is the native-audio-backend bookkeeping that
  // reconciles xt_graphics.c's pure-state pengs[]/air_*/pwastd decisions
  // (see xt_graphics_stub_playsounds()'s own doc comment) against real
  // mixer channels once per tick. -1 = not currently playing.
  int32_t engine_channel[5] = {-1, -1, -1, -1, -1}; // one per rev slot of the CURRENT bank
  int32_t last_engine_bank = -1; // detects xt.lcn changing -- see sparkeng()'s own doc comment on why that forces a full stop
  int32_t air_channel = -1;      // at most one air[] clip is ever meant to be audible at once, see xt_graphics.h
  int32_t wasted_channel = -1;
  bool last_mutem = false;       // edge-detects control->mutem via xt->mutem, see xt_graphics_stub_playsounds()'s own doc comment
  HudImages hud_images;
  memset(&hud_images, 0xFF, sizeof(hud_images)); // every tex field starts at -1 ("not loaded")

  // Fixed-timestep physics, decoupled from the render/vsync rate -- ports
  // web/main.js's own accumulator (see its "---- pacing ----" comment):
  // every velocity/acceleration/rotation constant in Mad.drive() is
  // PER-TICK, tuned to GameSparker.run()'s original self-tuning sleep,
  // which settled at 53ms/tick (~18.9 ticks/sec). Calling mad_drive()
  // once per rendered frame (the M2 placeholder this replaces) ran the
  // simulation at the display's refresh rate instead -- 60Hz on a typical
  // monitor is ~3.2x too fast, matching exactly the "car crosses the map
  // way too quickly" symptom this fixes. TICK_MS/MAX_CATCHUP match
  // web/main.js's own defaults. Rendering happens once per real frame
  // regardless of how many ticks that frame consumed (0 to MAX_CATCHUP) --
  // i.e. tick-rate rendering, matching the JS's own `?interp=0` mode
  // rather than its default blended-frame interpolation (INTERPOLATE's
  // snapshot/blend machinery is real work belonging with Record.js's
  // rec/play/playh, deferred for the same reason -- see TASKS_NATIVE.md).
  const double TICK_MS = 53.0;
  const double MAX_ACCUMULATOR_MS = TICK_MS * 3.0; // MAX_CATCHUP=3, clamped directly
  uint32_t last_ticks_ms = platform_ticks_ms();
  double accumulator_ms = 0.0;
  int32_t last_racing_frame = -2; // the loop's `frame` when the race last ran
  // Smooth frames (see smooth_apply): the cars and camera before and after
  // the latest tick, the HUD that tick drew (replayed on the frames between
  // ticks -- its drawing advances per-draw timers), and each object's dist
  // from that tick's draw (simulation reads it: checkstat's onscreen[]).
  // smooth_ready: a tick has filled all of it with the setting on.
  SmoothSnap smooth_prev = {0}, smooth_curr = {0};
  GfxClip hud_clip = {0};
  int32_t *smooth_dist = NULL;
  bool smooth_ready = false;
  bool smooth_captured = false; // smooth_curr holds a tick of THIS race
  Bench bench = {0};            // Settings > Performance Test
  bool bench_start = false;
  const char *bench_seconds_env = getenv("NFM_BENCH_SECONDS");
  const int32_t bench_seconds = bench_seconds_env ? atoi(bench_seconds_env) : BENCH_SECONDS;
  double race_frame_ms = TICK_MS; // this frame's length, for the trail
  bool race_confirm_latch = false;

  int32_t frame = 0;
  // Whether the OTHER ping-pong target holds the previous frame's finished
  // picture. False after any frame drawn without a trail (or straight to
  // the display), so the first trail frame after it takes its scene as is
  // instead of blending in a stale, unrelated picture.
  bool accum_valid = false;
  // scene_rt and accum_rt are used as a PING-PONG pair: each frame renders
  // into rt_pair[rt_cur], folds the previous frame's finished picture
  // (rt_pair[rt_cur ^ 1]) over it for the trail, presents it, and flips.
  // See the composite at the end of the loop.
  GfxGlRenderTarget *rt_pair[2] = {&scene_rt, &accum_rt};
  int32_t rt_cur = 0;
  // Racing and the two replays are only DRAWN on a frame that advances
  // them by a tick; frames in between present the last finished picture
  // again (see reuse_frame). picture_state is the state whose finished
  // picture rt_pair[rt_cur ^ 1] holds (-1: none usable); the offsets are
  // the shake it was presented with.
  int32_t picture_state = -1;
  float race_present_dx = 0.0f, race_present_dy = 0.0f;
  // Pacing for the screens that are drawn once per Java game-loop
  // iteration rather than per display frame: the two replays at the race's
  // 53ms tick, and car select / stage select at the menus' ~40ms (the Java
  // loop's own menu rate -- it adapts its sleep until 10 iterations take
  // 400ms). This port used to draw them every display frame, so they ran
  // 2.4-3x the original speed and did that much more work. The confirm
  // latch keeps a replay skip pressed on a frame that doesn't advance from
  // being lost (the menus read their input before the draw, every frame).
  double paced_acc_ms = 0.0;
  uint32_t paced_last_ms = 0;
  int32_t paced_state_now = -1;
  bool replay_confirm_latch = false;
  bool running = true;
  // Frame-phase breakdown for the FPS overlay (microseconds, summed over
  // the current one-second window): logic = input, menu state and the
  // physics ticks; build = assembling this frame's geometry on the CPU;
  // gl = handing it to the GL plus the composite; swap = waiting in
  // platform_swap_buffers, i.e. for the GPU to finish and for vsync.
  uint64_t prof_frame_start = 0, prof_render_start = 0, prof_submit_start = 0, prof_swap_start = 0;
  uint64_t prof_sum[4] = {0, 0, 0, 0};
  int32_t prof_avg_tenths[4] = {0, 0, 0, 0}; // last window's per-frame mean, 0.1ms units
  while (running) {
    prof_frame_start = platform_ticks_us();
    lang_imgs_sync();
    // Breadcrumbs for the freeze watchdog / crash handler (diag.h).
    if (frame == 0) diag_start_watchdog();
    g_diag.heartbeat++;
    g_diag.frame = frame;
    g_diag.state = (int32_t)state;
    g_diag.stage = stage_num;
    g_diag.gmode = gmode;
    g_diag.car = car_index;
    g_diag.nplayers = nplayers;
    DIAG_PHASE("frame: input and menus");
    // NFM_DEBUG_FREEZE_FRAME=n: hang on purpose at frame n, to check the
    // freeze watchdog writes its report.
    if (getenv("NFM_DEBUG_FREEZE_FRAME") && frame == atoi(getenv("NFM_DEBUG_FREEZE_FRAME"))) {
      DIAG_PHASE("debug: deliberate freeze (NFM_DEBUG_FREEZE_FRAME)");
      for (volatile int spin = 1; spin;) {}
    }
    bool held[BTN_COUNT];
    bool race_ticked = false; // this frame consumed at least one physics tick
    running = platform_poll(held);

    // ---- Touch (the Switch's screen, the mouse on desktop) ----------------
    // ctachm() (xtGraphics.java:7305-7552), the original's mouse handler,
    // with its hit boxes: pressing a row or button selects it (Java's
    // n3 == 1), lifting the finger on the same one presses it (n3 == 2),
    // here as a one-frame press of the matching button so each screen's own
    // input below handles it. Screens the port added (Settings, the pause
    // menu's two plates) get boxes of their own.
    {
      static bool was_down;
      static int32_t tx, ty;
      static TouchHit pressed;
      int32_t px, py;
      bool down = platform_pointer(&px, &py);
      // NFM_HOOK_TAP=frame,x,y: a headless tap (down two frames, then up).
      static int32_t tap_f = -1, tap_x, tap_y;
      if (tap_f == -1) {
        const char *e = getenv("NFM_HOOK_TAP");
        tap_f = -2;
        if (e && sscanf(e, "%d,%d,%d", &tap_f, &tap_x, &tap_y) != 3) tap_f = -2;
      }
      if (tap_f >= 0 && frame >= tap_f && frame < tap_f + 2) { down = true; px = tap_x; py = tap_y; }
      if (down) { tx = px; ty = py; }
      if (down || was_down) {
        TouchHit hit = {NULL, 0, BTN_COUNT};
#define OVER(img, x0, y0) ((img).tex >= 0 && tx > (x0) - 5 && tx < (x0) + (img).w + 5 && ty > (y0) - 5 && ty < (y0) + (img).h + 5)
#define OVERON(x0, y0, w0, h0) (tx > (x0) && tx < (x0) + (w0) && ty > (y0) && ty < (y0) + (h0))
#define HIT(s, v, b) (hit = (TouchHit){(s), (v), (b)})
        if (state == STATE_BOOT_CLICK || state == STATE_CANTREPLY || state == STATE_BENCH_RESULT ||
            state == STATE_REPLAY || state == STATE_PAUSE_REPLAY) {
          HIT(NULL, 0, BTN_CONFIRM);   // a click anywhere
        } else if (state == STATE_MAIN_MENU) {
          // draw_main_menu's rows.
          static const int32_t kRows[4][3] = {{343, 261, 110}, {301, 291, 196}, {357, 321, 85}, {353, 351, 93}};
          for (int32_t i = 0; i < 4; i++)
            if (OVERON(kRows[i][0], kRows[i][1], kRows[i][2], 22)) HIT(&mainmenu_opselect, i, BTN_CONFIRM);
        } else if (state == STATE_GAMEMODE_MENU) {
          // draw_gamemode_menu's rows.
          static const int32_t kRows[5][3] = {
              {358, 262, 82}, {358, 290, 82}, {348, 318, 102}, {348, 346, 102}, {348, 374, 102}};
          for (int32_t i = 0; i < 5; i++)
            if (OVERON(kRows[i][0], kRows[i][1], kRows[i][2], 22)) HIT(&gamemode_opselect, i, BTN_CONFIRM);
        } else if (state == STATE_INSTRUCTIONS) {
          if (inst_flipo >= 1 && inst_flipo <= 15 && OVER(menu_next, 665, 395)) HIT(NULL, 0, BTN_RIGHT);
          if (inst_flipo >= 3 && inst_flipo <= 16 && OVER(menu_back, 75, 395)) HIT(NULL, 0, BTN_LEFT);
          if (inst_flipo == 16 && OVER(menu_contin, 565, 395)) HIT(NULL, 0, BTN_CONFIRM);
          if (inst_flipo == GUIDE_FLIPO) {
            if (guide_page < GUIDE_PAGES - 1 && OVER(menu_next, 665, 395)) HIT(NULL, 0, BTN_RIGHT);
            if (OVER(menu_back, 75, 395)) HIT(NULL, 0, BTN_LEFT);
            if (guide_page == GUIDE_PAGES - 1 && OVER(menu_contin, 565, 395)) HIT(NULL, 0, BTN_CONFIRM);
          }
        } else if (state == STATE_CREDITS) {
          if (credits_page < CREDITS_PAGES - 1 && OVER(menu_next, 665, 395)) HIT(NULL, 0, BTN_CONFIRM);
          if (credits_page > 0 && OVER(menu_back, 75, 395)) HIT(NULL, 0, BTN_LEFT);
          if (credits_page == CREDITS_PAGES - 1 && OVER(menu_contin, 565, 395)) HIT(NULL, 0, BTN_CONFIRM);
        } else if (state == STATE_CAR_SELECT) {
          if (OVER(menu_next, 645, 275)) HIT(NULL, 1, BTN_RIGHT);
          if (OVER(menu_back, 95, 275)) HIT(NULL, 2, BTN_LEFT);
          if (OVER(menu_contin, 355, 385)) HIT(NULL, 3, BTN_CONFIRM);
        } else if (state == STATE_STAGE_SELECT) {
          if (OVER(menu_next, 625, 135)) HIT(NULL, 1, BTN_RIGHT);
          if (OVER(menu_back, 115, 135)) HIT(NULL, 2, BTN_LEFT);
          if (OVER(menu_contin, 355, 385)) HIT(NULL, 3, BTN_CONFIRM);
          if (gmode == GMODE_FREE_PLAY && !ext_normal && OVERON(180, 326, 440, 40)) HIT(NULL, 4, BTN_VIEW);
        } else if (state == STATE_STAGE_LOCKED) {
          if (OVER(menu_back, 370, 345)) HIT(NULL, 0, BTN_CONFIRM);
        } else if (state == STATE_STAGE_INTRO) {
          if (OVER(intro_star[0], 359, 385) || OVER(intro_star[0], 359, 295)) HIT(NULL, 0, BTN_CONFIRM);
        } else if (state == STATE_POST_RACE) {
          if (OVER(menu_contin, 355, 380)) HIT(NULL, 0, BTN_CONFIRM);
        } else if (state == STATE_PAUSED) {
          static const int32_t kRows[6][4] = {{329, 45, 137, 22}, {320, 73, 155, 22}, {303, 99, 190, 22},
                                              {341, 125, 109, 22}, {320, 202, 160, 30}, {320, 236, 160, 30}};
          for (int32_t i = 0; i < 6; i++)
            if (OVERON(kRows[i][0], kRows[i][1], kRows[i][2], kRows[i][3])) HIT(&pause_opselect, i, BTN_CONFIRM);
        } else if (state == STATE_SETTINGS) {
          const int32_t r = (tx > 120 && tx < 680) ? settings_row_at(&settings_ui, &settings, platform_has_rumble(), ty) : -1;
          if (r >= 0) {
            const SettingsRow *row = &kSettingsPages[settings_ui.page].rows[r];
            Button b = BTN_CONFIRM;
            // A value row: its right end steps up, the rest of its value
            // half steps down, its label only selects it.
            if (row->kind == ROW_CHOICE || row->kind == ROW_CAR) b = tx >= 640 ? BTN_RIGHT : (tx >= 400 ? BTN_LEFT : BTN_COUNT);
            HIT(&settings_ui.row, r, b);
          }
        }
#undef OVER
#undef OVERON
#undef HIT
        if (down && !was_down) pressed = hit;
        // Held: the row under the finger is the selected one (Java's
        // pressed rows and n3 == 0 drags).
        if (down && hit.sel && hit.sel == pressed.sel) *hit.sel = hit.sel_value;
        // Lifted where it was pressed: that button, for one frame.
        if (!down && hit.btn != BTN_COUNT && hit.btn == pressed.btn && hit.sel == pressed.sel &&
            hit.sel_value == pressed.sel_value) {
          if (hit.sel) *hit.sel = hit.sel_value;
          held[hit.btn] = true;
        }
      }
      was_down = down;
    }
    // Left for the HOME menu or sleep: a race in progress pauses.
    const bool focus_pause = platform_take_focus_lost();
#define KEY_EDGE(b) (held[(b)] && !previous_held[(b)])

    if (state == STATE_BOOT_CLICK) {
      // fase 111 (GameSparker.java:263-279): a click -- here the confirm
      // button -- ends the prompt; so do 800 iterations (the draw counts).
      if (KEY_EDGE(BTN_CONFIRM)) {
        boot_n = 0;
        state = STATE_BOOT_RAD;
      }
    } else if (state == STATE_MAIN_MENU) {
      // 3 selectable options (Play Game / Instructions / Credits) --
      // Multiplayer is deliberately hidden in this single-player-only
      // port (user decision). `mainmenu_opselect` cycles 0..2 with wrap.
      // NFM_SCREENSHOT_MENU=mainsettings: open Settings from here, headless.
      if (screenshot_menu && strcmp(screenshot_menu, "mainsettings") == 0 && frame == screenshot_frame - 3) {
        mainmenu_opselect = 3;
        settings_ui = (SettingsUi){.page = SET_MAIN};
        // NFM_SETTINGS_PAGE=n (and NFM_SETTINGS_ROW=n) open a section directly.
        const char *page_env = getenv("NFM_SETTINGS_PAGE"), *row_env = getenv("NFM_SETTINGS_ROW");
        if (page_env && atoi(page_env) >= 0 && atoi(page_env) < SET_PAGE_COUNT) settings_ui.page = atoi(page_env);
        if (row_env) settings_ui.row = atoi(row_env);
        settings_return = STATE_MAIN_MENU;
        state = STATE_SETTINGS;
      }
      // A fourth, Settings, opens the same screen the pause menu does.
      if (KEY_EDGE(BTN_DOWN)) mainmenu_opselect = (mainmenu_opselect + 1) % 4;
      if (KEY_EDGE(BTN_UP)) mainmenu_opselect = (mainmenu_opselect + 3) % 4;
      if (KEY_EDGE(BTN_CONFIRM)) {
        // Java's fase transitions from maini() -- see xtGraphics.java:4449.
        // Play Game -> fase 102 (gamemode submenu).
        // Multiplayer would be opselect=1 -> fase -9 (netplay), skipped.
        // Instructions was opselect=2 -> fase 11 (inst()).
        // Credits was opselect=3 -> fase 8 (credits()).
        // Since we skip Multiplayer here, our opselect 0/1/2 map to
        // Java's opselect 0/2/3.
        switch (mainmenu_opselect) {
          case 0:
            // :4475-4479 -- pre-position the GAMEMODE submenu's cursor
            // from campaign completion before entering it. Java can do
            // this by assigning its single shared `opselect` field
            // (maini and maini2 both read the same one); this port keeps
            // a separate cursor per screen, so the equivalent is writing
            // gamemode_opselect here. unlocked[0]==11 means NFM1 is
            // fully cleared -> start on NFM2; unlocked[1]==17 means NFM2
            // is cleared too, and Java then picks ITS opselect=2, which
            // in maini2 is the Multiplayer row -- a row this port
            // deliberately hides. Free Play is the substitute landing
            // spot for "you've finished everything" (and happens to sit
            // at index 2 here as well, but that's coincidence, not a
            // 1:1 mapping -- Java's own target genuinely does not exist
            // in this build).
            if (progress.unlocked[0] == 11) {
              gamemode_opselect = (progress.unlocked[1] != 17) ? 1 : 2;
            }
            // :4480-4484 -- first-run Instructions detour (see
            // menu_firstime's own declaration comment above).
            if (menu_firstime) {
              instructions_return_to = STATE_GAMEMODE_MENU;
              state = STATE_INSTRUCTIONS;
              menu_firstime = false;
            } else {
              state = STATE_GAMEMODE_MENU;
            }
            break;
          case 1:
            // :4463-4466 -- reading them deliberately returns to the MAIN
            // menu, and consumes the first-run flag so Play Game won't
            // detour afterwards.
            instructions_return_to = STATE_MAIN_MENU;
            menu_firstime = false;
            state = STATE_INSTRUCTIONS;
            break;
          case 2: state = STATE_CREDITS; break;
          case 3:
            settings_ui = (SettingsUi){.page = SET_MAIN};
            settings_return = STATE_MAIN_MENU;
            state = STATE_SETTINGS;
            break;
        }
        // :4489 -- maini's ENTER handler ends by zeroing the shared
        // `flipo` for whichever screen it just dispatched to; the
        // Instructions flipbook's own top-of-screen fixup then turns
        // that 0 into page 1. Also re-arms mainbg(2)'s scroll/fade the
        // way Java's own `lmode`-change branch (:1786-1791) does, since
        // entering Instructions is the only transition that switches
        // background mode into it.
        inst_flipo = 0;
        inst_dudo = 0;
        inst_duds = 0;
        inst_bgf = 0.2f;
        mainbg_bgmy[0] = 0;
        mainbg_bgmy[1] = -400;
      }
    } else if (state == STATE_GAMEMODE_MENU) {
      // 3 selectable options (NFM 1 / NFM 2 / Free Play) -- Multiplayer
      // hidden same as main menu.
      // A fourth row, Extended (its normal mode), where the original's own
      // fourth row sat.
      if (KEY_EDGE(BTN_DOWN)) gamemode_opselect = (gamemode_opselect + 1) % 5;
      if (KEY_EDGE(BTN_UP)) gamemode_opselect = (gamemode_opselect + 4) % 5;
      if (KEY_EDGE(BTN_CANCEL)) {
        state = STATE_MAIN_MENU;
      }
      if (KEY_EDGE(BTN_CONFIRM)) {
        // Java xtGraphics.java:4640-4674 maini2 ENTER dispatch: our
        // opselect 0/1/2 map to Java's opselect 0/1/3 (skipping
        // Multiplayer at Java opselect=2). Each sets `gmode` to
        // 1/2/0 respectively, then goes to fase=-9 which cleans up
        // and transitions to fase=7 (car select).
        switch (gamemode_opselect) {
          case 0: gmode = 1; break;  // NFM 1
          case 1: gmode = 2; break;  // NFM 2
          case 2: gmode = 0; break;  // Free Play
          case 3: gmode = 0; break;  // RPG Mode: Extended's career (its normal mode's flow, ext_career)
          case 4: gmode = 0; break;  // Tournament: Extended's Premier Tournament (its normal mode's stage 26)
        }
        const bool tourney = gamemode_opselect == 4;
        ext_normal = gamemode_opselect >= 3;
        ext_career = gamemode_opselect == 3;
        g_ext_career = ext_career;
        car_filter = CF_ALL;
        // :4646-4659 -- Java resets its cursor to 0 when confirming NFM2
        // or Free Play (its own opselect 1 and 3); the NFM1 branch does
        // not, but it is already 0 there so all three paths leave the
        // cursor at 0 either way. Without this the port's cursor stayed
        // where the player left it, so backing out of car-select
        // reopened this menu on the previously-chosen row instead of the
        // top one. (The post-race return path sets the cursor from
        // `gmode` separately -- see its own block below -- so this is
        // only observable when the player backs out before racing.)
        gamemode_opselect = 0;
        // Java maini2 -> fase=-9 clears then goes to fase=7 (carselect)
        // with sc[0] = scm[gmode-1] (:4868). We reset car_index to a
        // pickable default: scm if it's nonzero (a scaffold car the
        // player has unlocked), else 0 which is always pickable.
        if (gmode != 0 && progress.scm[gmode - 1] != 0) {
          car_index = progress.scm[gmode - 1];
        } else {
          car_index = ext_normal ? EXT_FIRST_CAR : 0;   // Extended starts on its car 0, Remington
        }
        if (ext_normal) stage_num = ext_prog.normal_unlocked;
        if (ext_career) {
          // RPG MODE (XT 14573): the car and stage the player left off on.
          car_index = ext_car_to_port(csave.lastcar);
          stage_num = csave.laststage <= csave.unlocked ? csave.laststage : csave.unlocked;
        }
        // Java stageselect :1914/:1925 -- entering the picker starts at
        // the next-to-unlock stage for the gmode. Free Play stays at
        // whatever stage_num already was so screenshot hooks keep working.
        if (gmode != 0) {
          stage_num = game_progress_default_stage(&progress, (GameMode)gmode);
        }
        // The stage list opens on this mode's stage: its name and preview,
        // not the last mode's.
        if (gmode == GMODE_FREE_PLAY && !ext_normal && stage_num > FREE_PLAY_LAST_STAGE) stage_num = 1;
        if (ext_normal) ext_read_name(stage_num, stage_name_buf, sizeof(stage_name_buf));
        else stage_read_name(stage_num, stage_name_buf, sizeof(stage_name_buf));
        stage_preview_loaded_num = -1;
        state = STATE_CAR_SELECT;
        car_select_needs_intro = true;
        if (tourney) {
          // Every match gives every car, the player's too: straight to the
          // first match's rules.
          stage_num = EXT_PT_STAGE;
          ext_read_name(stage_num, stage_name_buf, sizeof(stage_name_buf));
          ext_pt_start_tournament(&pt);
          ptmatch = 1;
          state = STATE_PT_INFO;
        }
      }
    } else if (state == STATE_INSTRUCTIONS) {
      // :4285-4306 -- the flipbook's paging, ported exactly. Forward is
      // ENTER or RIGHT; only ENTER on the LAST page (16) exits, and it
      // exits to `oldfase` rather than unconditionally to the main menu
      // (see menu_firstime's declaration comment for why that matters).
      // Note the ordering quirk this preserves: the exit check zeroes
      // flipo BEFORE the increment below runs, so the `1..15` guard then
      // fails and the freshly-zeroed counter is left alone for the next
      // visit.
      //
      // Java has NO escape/cancel out of this screen -- the only way out
      // is to page through to 16 and confirm. Kept as-is rather than
      // leaving this port's earlier CANCEL shortcut in place: the screen
      // is fully navigable in both directions, so the original's flow is
      // not a trap, and honouring it is the point of this review.
      if (inst_flipo == GUIDE_FLIPO) {
        // The Extended guide (draw_ext_guide): Left back (off its first page,
        // to the original's last), Right / confirm on, Cancel out.
        if (KEY_EDGE(BTN_CONFIRM) || KEY_EDGE(BTN_RIGHT)) {
          if (guide_page < GUIDE_PAGES - 1) {
            guide_page++;
            inst_dudo = 200;
          } else if (KEY_EDGE(BTN_CONFIRM)) {
            inst_flipo = 0;
            state = instructions_return_to;
          }
        } else if (KEY_EDGE(BTN_LEFT)) {
          if (guide_page > 0) {
            guide_page--;
            inst_dudo = 200;
          } else {
            inst_flipo = 16;
          }
        } else if (KEY_EDGE(BTN_CANCEL)) {
          inst_flipo = 0;
          state = instructions_return_to;
        }
      } else {
      if (KEY_EDGE(BTN_CONFIRM) || KEY_EDGE(BTN_RIGHT)) {
        if (KEY_EDGE(BTN_CONFIRM) && inst_flipo == 16) {
          // The original's last page leads on to the Extended guide.
          inst_flipo = GUIDE_FLIPO;
          guide_page = 0;
          inst_dudo = 200;
        }
        if (inst_flipo >= 1 && inst_flipo <= 15) inst_flipo++;
      }
      if (KEY_EDGE(BTN_LEFT)) {
        // :4298-4304 -- back steps by THREE, not one: from an odd page N
        // that lands on the even value N-3, which the next frame's
        // top-of-screen fixup bumps back up to the previous odd page
        // (e.g. 9 -> 6 -> 7). Page 16 is the exception, stepping back by
        // one onto 15.
        if (inst_flipo >= 3 && inst_flipo <= 15) inst_flipo -= 3;
        if (inst_flipo == 16) inst_flipo--;
      }
      }   // !guide
    } else if (state == STATE_CREDITS) {
      // :4684 -- credits' own exit is always back to the main menu
      // (fase 10), with no oldfase indirection.
      // This port pages on through Extended's credits and its own.
      if (KEY_EDGE(BTN_CANCEL) || (KEY_EDGE(BTN_CONFIRM) && credits_page == CREDITS_PAGES - 1)) {
        state = STATE_MAIN_MENU;
        credits_page = 0;
      } else if (KEY_EDGE(BTN_CONFIRM) || KEY_EDGE(BTN_RIGHT)) {
        if (credits_page < CREDITS_PAGES - 1) credits_page++;
      } else if (KEY_EDGE(BTN_LEFT) && credits_page > 0) {
        credits_page--;
      }
    } else if (state == STATE_CAR_SELECT) {
      // Java carselect :6445-6461 -- LEFT/RIGHT only start a new
      // transition while `flipo == 0` (not already mid-transition); the
      // actual car_index change is deferred to the crossover frame in the
      // per-frame update below (draw_car_preview's own doc comment), not
      // applied instantly here.
      // :6445-6464 -- RIGHT/LEFT only ARM a transition when the cursor is
      // not already at the end of the range: Java gates each on
      // `sc[0] != maxsl` / `sc[0] != minsl`, so the list CLAMPS at both
      // ends rather than wrapping. This port used to wrap
      // (`% (CUSTOM_CAR_INDEX + 1)` at the crossover), which let the
      // player run off either end straight into the far side of the list.
      // minsl/maxsl are 0/15 for single-player (:4969-4970 -- the other
      // values Java computes are all inside its `multion != 0` netplay
      // branch); this port's extra custom-car slot extends maxsl by one,
      // but only in Free Play, which is the only mode that can select it.
      const bool career_car_open = ext_career && career_car_lock(&csave, ext_car_of(car_index)) == 0;
      // A bonus car shown with no points gets its own (XT 15885-15887).
      if (career_car_open) career_bonus_car_points(&csave, ext_car_of(car_index));
      if (career_confirm != 0) {
        // A YES / NO question: any direction flips the answer.
        const int32_t ec = ext_car_of(car_index);
        if (KEY_EDGE(BTN_LEFT) || KEY_EDGE(BTN_RIGHT) || KEY_EDGE(BTN_UP) || KEY_EDGE(BTN_DOWN))
          career_confirm_yes = !career_confirm_yes;
        if (KEY_EDGE(BTN_CANCEL)) {
          if (career_confirm == CC_TRANSFER_TO) career_transfer_from = -1;   // NO ends the transfer (shufflefase 0)
          career_confirm = 0;
        } else if (KEY_EDGE(BTN_CONFIRM)) {
          if (career_confirm_yes) {
            if (career_confirm == CC_RESHUFFLE) career_reshuffle(&csave, ec);
            if (career_confirm == CC_SELL) career_sell(&csave, ec);
            if (career_confirm == CC_TRANSFER) {
              career_transfer_from = ec;   // now pick the car to take it
              career_panel = false;
            }
            if (career_confirm == CC_TRANSFER_TO) {
              career_transfer(&csave, career_transfer_from, ec);
              career_transfer_from = -1;
            }
            if (career_path[0]) career_save(career_path, &csave);   // the original saves on a transfer, at its next save otherwise
          } else if (career_confirm == CC_TRANSFER_TO) {
            career_transfer_from = -1;
          }
          career_confirm = 0;
        }
      } else if (career_panel) {
        const int32_t ec = ext_car_of(car_index);
        int32_t perk_slots[CAREER_PERK_SLOTS];
        const int32_t nperk = career_perk_rows(ec, perk_slots);   // only the perks that work
        int32_t nrows = career_panel_page == 2 ? 3 : (career_panel_page == 1 ? nperk : CS_N);
        if (nrows < 1) nrows = 1;
        if (career_panel_row >= nrows) career_panel_row = 0;
        if (KEY_EDGE(BTN_DOWN)) career_panel_row = (career_panel_row + 1) % nrows;
        if (KEY_EDGE(BTN_UP)) career_panel_row = (career_panel_row + nrows - 1) % nrows;
        if (KEY_EDGE(BTN_RIGHT) || KEY_EDGE(BTN_LEFT)) {
          career_panel_page = (career_panel_page + (KEY_EDGE(BTN_RIGHT) ? 1 : 2)) % 3;
          career_panel_row = 0;
        } else if (KEY_EDGE(BTN_CONFIRM)) {
          if (career_panel_page == 0 && csave.statpoints[ec] > 0) {
            csave.sp[ec][career_panel_row]++;
            csave.statpoints[ec]--;
            career_pend[0][career_panel_row]++;
          }
          // Car points work once bonus stage 4 is won (its "extra stats" button).
          if (career_panel_page == 1 && career_panel_row < nperk && csave.boncomp[3] > 0 &&
              career_spend_perk(&csave, ec, perk_slots[career_panel_row]))
            career_pend[1][perk_slots[career_panel_row]]++;
          if (career_panel_page == 2) {
            if (career_panel_row == 0) career_confirm = CC_RESHUFFLE;
            if (career_panel_row == 1 && csave.statchangers[1] > 0) career_confirm = CC_TRANSFER;
            if (career_panel_row == 2) career_confirm = CC_SELL;
            career_confirm_yes = false;
          }
        }
        const int32_t back_k = career_panel_page == 1 ? (career_panel_row < nperk ? perk_slots[career_panel_row] : -1)
                                                       : career_panel_row;
        if (KEY_EDGE(BTN_VIEW) && career_panel_page < 2 && back_k >= 0 && career_pend[career_panel_page][back_k] > 0) {
          career_pend[career_panel_page][back_k]--;
          if (career_panel_page == 0) {
            csave.sp[ec][back_k]--;
            csave.statpoints[ec]++;
          } else {
            csave.perk[ec][back_k]--;
            csave.carpoints++;
          }
        }
        if (KEY_EDGE(BTN_CANCEL) || KEY_EDGE(BTN_SPECIAL)) {
          career_panel = false;
          if (career_path[0]) career_save(career_path, &csave);
        }
      } else {
      if (ext_career && career_car_open && car_flipo == 0 && career_transfer_from < 0 && KEY_EDGE(BTN_SPECIAL)) {
        career_panel = true;
        career_panel_row = 0;
        memset(career_pend, 0, sizeof(career_pend));
      }
      if (car_flipo == 0) {
        int32_t car_maxsl = (gmode == GMODE_FREE_PLAY) ? (ext_career ? CAR_COUNT - 1 : car_last_index(ext_normal)) : 15;
        if (KEY_EDGE(BTN_RIGHT) && car_step(car_index, 1, car_maxsl, car_filter, ext_normal) >= 0) { car_nextc = 1; car_flipo = 20; }
        if (KEY_EDGE(BTN_LEFT) && car_step(car_index, -1, car_maxsl, car_filter, ext_normal) >= 0) { car_nextc = -1; car_flipo = 20; }
        int32_t filters[8];
        const int32_t nf = car_filters(ext_career, gmode, filters);
        if (nf > 1 && (KEY_EDGE(BTN_DOWN) || KEY_EDGE(BTN_UP))) {
          int32_t k = 0;
          while (k < nf - 1 && filters[k] != car_filter) k++;
          car_filter = filters[(k + (KEY_EDGE(BTN_DOWN) ? 1 : nf - 1)) % nf];
          if (!car_in_filter(car_index, car_filter)) {
            const int32_t first = car_step(-1, 1, car_maxsl, car_filter, ext_normal);
            if (first >= 0) car_index = first;
          }
        }
      }
      if (KEY_EDGE(BTN_CANCEL)) {
        if (career_transfer_from >= 0) career_transfer_from = -1;   // the transfer called off
        else state = STATE_GAMEMODE_MENU;
      }
      // :6465 -- confirm is gated on `k == 0 && flipo < 10`, i.e. the car
      // must be unlocked AND the swap animation must be past its
      // half-way crossover. The flipo gate matters: while flipo is 20..11
      // the index still names the OLD car (it only changes AT flipo==10),
      // so without it a confirm during the first half of a swap commits
      // the car that is visibly falling off the screen rather than the
      // one arriving. Locked cars land fine, they just can't be confirmed
      // -- game_progress_can_pick_car() is the same gate the locked-car
      // overlay's own visibility uses below.
      if (KEY_EDGE(BTN_CONFIRM) && car_flipo < 10 && career_transfer_from >= 0) {
        // shufflefase 5: an open car other than the source takes the transfer.
        if (career_car_open && ext_car_of(car_index) != career_transfer_from) {
          career_confirm = CC_TRANSFER_TO;
          career_confirm_yes = false;
        }
      } else if (KEY_EDGE(BTN_CONFIRM) && car_flipo < 10) {
        const bool career_open = !ext_career || career_car_lock(&csave, ext_car_of(car_index)) == 0;
        if (career_open &&
            (car_index == CUSTOM_CAR_INDEX || game_progress_can_pick_car(&progress, (GameMode)gmode, car_index))) {
          // :6466 -- leaving car select clears the preview camera's `crs`
          // (draw_car_preview sets it). See the race setup for why it
          // matters.
          m.crs = false;
          // :6490-6496 -- remember this campaign's chosen car. `scm[]` is
          // the SAME field the bonus-car unlock writes (:6701-6775): it
          // means "the car this campaign should default to", set either
          // by winning a milestone stage or, here, by the player picking
          // one. The port already READ it back when entering car select
          // (see the gamemode-submenu confirm) but never wrote this half,
          // so a player's choice was silently forgotten every time.
          if (gmode == GMODE_NFM1) progress.scm[0] = car_index;
          else if (gmode == GMODE_NFM2) progress.scm[1] = car_index;
          if (progress_path_ok) game_progress_save_to_disk(&progress, progress_path);
          if (ext_career) {
            csave.lastcar = ext_car_of(car_index);
            if (career_path[0]) career_save(career_path, &csave);
          }
          career_panel = false;
          if (ext_normal) {
            if (stage_num < 1 || stage_num > EXT_UNLOCKED) stage_num = EXT_UNLOCKED;
            ext_read_name(stage_num, stage_name_buf, sizeof(stage_name_buf));
            stage_preview_loaded_num = -1;
          }
          state = STATE_STAGE_SELECT;
        }
      }
      }   // !career_panel
    } else if (state == STATE_STAGE_SELECT) {
      // Stage navigation -- :2594-2636, ported exactly. Three things this
      // port had wrong:
      //
      //  a) It WRAPPED at the range ends. Java clamps: LEFT is gated on
      //     `stage != 1 && (stage != 11 || gmode != 2)` and RIGHT can
      //     never leave 27.
      //  b) NFM1's list is NOT 1..11 -- its eleventh slot is stage 27.
      //     Java remaps on both edges: stepping up off 10 gives 11 which
      //     becomes 27 (:2600-2602), and stepping down off 27 gives 26
      //     which becomes 10 (:2623-2625). Treating NFM1 as a flat 1..11
      //     offered stage 11, which belongs to NFM2, and never offered
      //     the stage-27 finale at all. (Same root cause as the default-
      //     stage remap fixed in progress.c this pass.)
      //  c) Pressing RIGHT while sitting ON the unlock frontier does not
      //     move at all in Java -- it opens cantgo() right there
      //     (:2614-2616). The port only reached cantgo by confirming a
      //     locked stage, which its own wrapping nav could seldom land on.
      GameMode gm = (GameMode)gmode;
      // The frontier stage: the highest one actually playable right now.
      // Free Play has none (gmode==0 short-circuits Java's whole check).
      int32_t frontier = (gm == GMODE_NFM1) ? progress.unlocked[0]
                       : (gm == GMODE_NFM2) ? progress.unlocked[1] + 10
                       : -1;
      // Extended's normal mode: stages 1..28, up to the one reached.
      const int32_t last_stage = ext_normal ? EXT_LAST_STAGE : (gm == GMODE_FREE_PLAY ? FREE_PLAY_LAST_STAGE : 27);
      if (ext_normal) frontier = EXT_UNLOCKED;
      if (KEY_EDGE(BTN_RIGHT)) {
        if (frontier >= 0 && stage_num == frontier && stage_num != last_stage) {
          // :2614-2616 -- at the frontier, RIGHT shows the locked card.
          state = STATE_STAGE_LOCKED;
          stage_lockcnt = 100;
        } else if (stage_num != last_stage) {
          stage_num++;
          if (gm == GMODE_NFM1 && stage_num == 11) stage_num = 27; // :2600-2602
          if (ext_normal) ext_read_name(stage_num, stage_name_buf, sizeof(stage_name_buf));
          else stage_read_name(stage_num, stage_name_buf, sizeof(stage_name_buf));
        }
      }
      if (KEY_EDGE(BTN_LEFT)) {
        if (stage_num != 1 && (stage_num != 11 || gm != GMODE_NFM2 || ext_normal)) {
          stage_num--;
          if (gm == GMODE_NFM1 && stage_num == 26) stage_num = 10; // :2623-2625
          if (ext_normal) ext_read_name(stage_num, stage_name_buf, sizeof(stage_name_buf));
          else stage_read_name(stage_num, stage_name_buf, sizeof(stage_name_buf));
        }
      }
      if (KEY_EDGE(BTN_CANCEL)) {
        state = STATE_CAR_SELECT;
        car_select_needs_intro = true;
      }
      if (gm == GMODE_FREE_PLAY && !ext_normal && KEY_EDGE(BTN_VIEW)) {
        // Free Play's Race Setup, from the stage list (its Start Race or
        // Back come back here or race).
        settings_ui = (SettingsUi){.page = SET_RACE};
        settings_return = STATE_STAGE_SELECT;
        state = STATE_SETTINGS;
      }
      g_career_bonus = 0;
      const bool career_modes_ok = ext_career && stage_num < csave.unlocked && csave.unlocked >= 3;
      if (!career_modes_ok || KEY_EDGE(BTN_LEFT) || KEY_EDGE(BTN_RIGHT)) career_mode = 0;
      if (career_modes_ok && KEY_EDGE(BTN_DOWN)) career_mode = (career_mode + 1) % 4;
      if (career_modes_ok && KEY_EDGE(BTN_UP)) career_mode = (career_mode + 3) % 4;
      if (ext_career && KEY_EDGE(BTN_SPECIAL) && stage_preview_ok && stage_num <= EXT_UNLOCKED &&
          (stage_num == 5 || stage_num == 11 || stage_num == 15 || stage_num == 18)) {
        // BONUS STAGE! (XT 13433, 16938-16960): the bonus hanging off this
        // stage, its own track and field.
        g_career_bonus = stage_num == 5 ? 1 : stage_num == 11 ? 2 : stage_num == 15 ? 3 : 4;
        stage_preview_loaded_num = -1;
        state = STATE_STAGE_LOADING;
        stage_loadcnt = 30;
      }
      if (KEY_EDGE(BTN_CONFIRM)) {
        if (ext_normal ? stage_num > EXT_UNLOCKED
                       : !free_play_ext_stage(stage_num) && !game_progress_can_pick_stage(&progress, gm, stage_num)) {
          // Java cantgo() -- xtGraphics.java:1993, armed at :2615-2616.
          // The countdown is 100 frames, not the 40 this port used (its
          // comment asserted Java used 40; :2616 is `lockcnt = 100`).
          state = STATE_STAGE_LOCKED;
          stage_lockcnt = 100;
        } else if (!stage_preview_ok) {
          // A stage that failed loadstage's checks (Java's stage == -3) is
          // not raced: its draw block says so and confirm does nothing.
        } else {
          // Java loadingstage() -- fase 2. We hold the animated transition
          // for ~30 frames (~1.6s at our 18.9 FPS tick rate; feels close
          // to the original applet's actual asset-fetch time on the JS's
          // own frame rate). Then flip to STATE_RACING.
          if (ext_career) {
            csave.laststage = stage_num;
            if (career_path[0]) career_save(career_path, &csave);
          }
          if (ext_normal && !ext_career && stage_num == EXT_PT_STAGE) {
            // Extended's Premier Tournament (ext_pt.c): five matches, each
            // shown with its rules first.
            ext_pt_start_tournament(&pt);
            ptmatch = 1;
            state = STATE_PT_INFO;
          } else {
            state = STATE_STAGE_LOADING;
            stage_loadcnt = 30;
          }
        }
      }
    } else if (state == STATE_PT_INFO) {
      if (KEY_EDGE(BTN_CONFIRM)) {
        stage_preview_loaded_num = -1;   // the match's own track
        state = STATE_STAGE_LOADING;
        stage_loadcnt = 30;
      }
      if (KEY_EDGE(BTN_CANCEL) && pt.match == 1) {
        ptmatch = 0;
        gamemode_opselect = 4;
        state = STATE_GAMEMODE_MENU;
      }
    } else if (state == STATE_PT_SCORES) {
      if (KEY_EDGE(BTN_CONFIRM)) {
        audio_stop_music(&audio);
        stage_music_loaded_for = -1;
        if (pt.match < EXT_PT_MATCHES) {
          pt.match++;
          ext_pt_start_match(&pt);
          ptmatch = pt.match;
          state = STATE_PT_INFO;
        } else {
          // The tournament is over: taking it counts as winning stage 26.
          if (ext_pt_champion(&pt) == 0 && ext_prog.normal_unlocked < EXT_PT_STAGE + 1) {
            ext_prog.normal_unlocked = EXT_PT_STAGE + 1;
            if (progress_path_ok) ext_progress_save(progress_path, &ext_prog);
          }
          ptmatch = 0;
          gamemode_opselect = 4;
          state = STATE_GAMEMODE_MENU;
        }
      }
    } else if (state == STATE_STAGE_LOCKED) {
      // :2015-2022 -- the countdown expiring, ENTER, handbrake or LEFT
      // dismiss the overlay. Deliberately NOT right/cancel: Java's list
      // is exactly `lockcnt == 0 || enter || handb || left`, and this
      // port used to accept RIGHT and CANCEL as well. BTN_CONFIRM covers
      // Java's enter AND handb here, since this port's desktop mapping
      // binds Space (its handbrake key) to that same logical button.
      stage_lockcnt--;
      if (stage_lockcnt <= 0 || KEY_EDGE(BTN_CONFIRM) || KEY_EDGE(BTN_LEFT)) {
        state = STATE_STAGE_SELECT;
      }
    } else if (state == STATE_STAGE_LOADING) {
      stage_loadcnt--;
      if (stage_loadcnt <= 0) {
        // loadstage() ends in fase 5 (GameSparker.java:2753), and the
        // stage-select confirm that led here armed dudo = 150 (:2587).
        state = STATE_STAGE_INTRO;
        intro_frame = 0;
        intro_dudo = 150;
        // getstats (XT 9697-9709): a career race opens on scouting, at
        // the first opponent (the Titan's slot skipped on its fight).
        career_scout = ext_career && !bench.active;
        career_scout_page = 0;   // set once the field is known
      }
    } else if (state == STATE_STAGE_INTRO) {
      // loadmusic()'s tail (:2971-2983): the track starts once it has
      // loaded, i.e. as the "Please Wait" card gives way to musicomp().
      if (intro_frame >= 1 && stage_music_pending) {
        stage_music_pending = false;
        if (stage_music_loaded_for == stage_music_id(stage_num, MUSIC_MODE)) {
          if (g_career_ogg[1] && ext_career)
            audio_start_ogg(&audio, g_career_ogg[0], g_career_ogg_len[0], g_career_ogg[1], g_career_ogg_len[1]);
          else
            audio_start_music(&audio, &stage_music);
          audio_set_music_muted(&audio, control[0].mutem);
          interface_playing = false;
        }
      }
      if (career_scout && ext_career && intro_frame >= 1) {
        // Scouting's pages (XT 9944-9963): Left / Right through the field,
        // confirm (or back) to the start card.
        const int32_t first = career_scout_first(&crace, &csave);
        if (career_scout_page < first) career_scout_page = first;
        if (KEY_EDGE(BTN_RIGHT) && career_scout_page < crace.nplayers - 1) career_scout_page++;
        if (KEY_EDGE(BTN_LEFT) && career_scout_page > first) career_scout_page--;
        if (KEY_EDGE(BTN_CONFIRM) || KEY_EDGE(BTN_CANCEL)) career_scout = false;
      } else
      // musicomp() (:3105-3155): handbrake or enter starts the race, with
      // the full-canvas viewport and the default camera projection.
      if (intro_frame >= 1 && (KEY_EDGE(BTN_CONFIRM) || (bench.active && intro_frame >= 40))) {
        m.trk = 0;
        m.crs = false;
        m.ih = 0;
        m.iw = 0;
        m.h = 450;
        m.w = 800;
        m.focus_point = 400;
        m.cx = 400;
        m.cy = 225;
        m.cz = 50;
        state = STATE_RACING;
        flyby_skip_armed = false;
        intro_confirm_frame = frame;
        // The race's accumulator starts now, not when the stage loaded.
        last_ticks_ms = platform_ticks_ms();
        accumulator_ms = 0.0;
      }
    } else if (state == STATE_POST_RACE && PT_ACTIVE) {
      // A tournament match: its points (scoreshow), not finish().
      ext_pt_award(&pt);
      state = STATE_PT_SCORES;
    } else if (state == STATE_POST_RACE) {
      // Java's finish() screen (fase -5), :6994-7027: ENTER or handbrake
      // (our SPACE binding -- Java checks `control.enter || control.handb`)
      // returns to the gamemode submenu (fase 102), landing on the option
      // matching the campaign just played (:6999-7023 -- Java's own
      // opselect numbering has Multiplayer at slot 2, so its "default 3"
      // for Free Play is OUR gamemode_opselect=2 since we hide that slot;
      // gmode==1/2 map straight to our 0/1).
      if (KEY_EDGE(BTN_CONFIRM)) {
        if (gmode == 1) gamemode_opselect = 0;
        else if (gmode == 2) gamemode_opselect = 1;
        else gamemode_opselect = ext_normal ? (ext_career ? 3 : 4) : 2;
        // Java :6995-6997 -- `if (this.loadedt) { this.strack.unload(); }`
        // stops the stage's music on the way back to the menu.
        audio_stop_music(&audio);
        stage_music_loaded_for = -1; // forces a fresh load if the player races again
        state = STATE_GAMEMODE_MENU;
      }
    } else if (state == STATE_PAUSED) {
      // pausedgame()'s own input half -- xtGraphics.java:4703-4806. Four
      // rows, wrapping both ways; Java consumes control.up/down by setting
      // them false, which is its own edge-detection, so KEY_EDGE here is
      // the direct equivalent. Confirm is `enter || handb` in the source,
      // and BTN_CONFIRM already covers both of this port's bindings for
      // that (Return and Space).
      // Six rows: the original's four, plus this port's Settings (4) and
      // Restart Race (5).
      if (KEY_EDGE(BTN_UP)) {
        pause_opselect--;
        if (pause_opselect == -1) pause_opselect = 5;
      }
      if (KEY_EDGE(BTN_DOWN)) {
        pause_opselect++;
        if (pause_opselect == 6) pause_opselect = 0;
      }
      // NFM_SCREENSHOT_MENU=pausereplay: headless stand-in for picking
      // Instant Replay (see the matching pause trigger in the race block).
      bool hook_replay = screenshot_menu && strcmp(screenshot_menu, "pausereplay") == 0 &&
                         frame == hook_replay_frame;
      // A real press, edge and all, so the replay sees what a player sends.
      // NFM_HOOK_PAUSE_ROW picks another row (5: Restart Race).
      if (hook_replay) {
        pause_opselect = getenv("NFM_HOOK_PAUSE_ROW") ? atoi(getenv("NFM_HOOK_PAUSE_ROW")) : 1;
        held[BTN_CONFIRM] = true;
      }
      // NFM_SCREENSHOT_MENU=settings: open the Settings screen headless.
      if (screenshot_menu && strcmp(screenshot_menu, "settings") == 0 && frame == screenshot_frame - 3) {
        settings_ui = (SettingsUi){.page = SET_MAIN};
        settings_return = STATE_PAUSED;
        state = STATE_SETTINGS;
      }
      if (KEY_EDGE(BTN_CONFIRM) || hook_replay) {
        if (pause_opselect == 0) {
          // :4763-4768 -- resume. Java restarts the music track it stopped
          // on the way in; this port pauses/resumes the same stream via
          // the mute flag, so honour the player's own M setting rather
          // than unconditionally unmuting.
          audio_set_music_muted(&audio, control[0].mutem);
          state = STATE_RACING;
        } else if (pause_opselect == 1) {
          // :4769-4779 -- Watch Replay, but only once the ring actually
          // holds a full 300 ticks; below that the source shows the
          // fase -8 "not enough replay data" banner instead.
          if (rpd.caught >= 300) {
            audio_set_music_muted(&audio, control[0].mutem);
            pause_replay_tick = 0;
            state = STATE_PAUSE_REPLAY;
          } else {
            cantreply_cnt = 0;
            state = STATE_CANTREPLY;
          }
        } else if (pause_opselect == 4) {
          settings_ui = (SettingsUi){.page = SET_MAIN};
          settings_return = STATE_PAUSED;
          state = STATE_SETTINGS;
        } else if (pause_opselect == 5) {
          // Restart Race: the same car and stage from the loading card, as
          // if picked again in the stage list.
          if (ext_career && !career_settled) csave = csave_race;   // an unfinished race pays nothing
          audio_stop_music(&audio);
          stage_music_loaded_for = -1;
          restart_race = true;
          state = STATE_STAGE_LOADING;
          stage_loadcnt = 30;
        } else if (pause_opselect == 2) {
          // :4780-4786 -- Game Instructions, with oldfase = -7 so it comes
          // back here. instructions_return_to is this port's own oldfase.
          audio_stop_music(&audio);
          stage_music_loaded_for = -1;
          instructions_return_to = STATE_PAUSED;
          state = STATE_INSTRUCTIONS;
        } else {
          // :4787-4800 -- Quit to the gamemode submenu, landing on the row
          // for the campaign just abandoned. Same opselect mapping the
          // post-race screen above already explains (Multiplayer is hidden
          // here, so Java's "3" for Free Play is our 2).
          if (ext_career && !career_settled) csave = csave_race;   // an unfinished race pays nothing
          audio_stop_music(&audio);
          stage_music_loaded_for = -1;
          if (gmode == 1) gamemode_opselect = 0;
          else if (gmode == 2) gamemode_opselect = 1;
          else gamemode_opselect = ext_normal ? (ext_career ? 3 : 4) : 2;
          state = STATE_GAMEMODE_MENU;
        }
      }
    } else if (state == STATE_CANTREPLY) {
      // fase -8 (GameSparker.java:1677-1685) -- the banner dismisses on
      // any confirm or after 150 frames, and always returns to the pause
      // menu. The counter advances in the draw block below, matching
      // Java's own `++n7` sitting inside the fase -8 body.
      if (KEY_EDGE(BTN_CONFIRM)) {
        cantreply_cnt = 0;
        state = STATE_PAUSED;
      }
    } else if (state == STATE_SETTINGS) {
      g_settings_in_race = settings_return != STATE_MAIN_MENU;
      const GameSettings before = settings;
      const SettingsAction act = settings_screen_input(
          &settings_ui, &settings, NFM_DEFAULT_GRAPHICS, KEY_EDGE(BTN_UP), KEY_EDGE(BTN_DOWN),
          KEY_EDGE(BTN_LEFT), KEY_EDGE(BTN_RIGHT), KEY_EDGE(BTN_CONFIRM), KEY_EDGE(BTN_CANCEL),
          platform_has_rumble());
      if (memcmp(&settings, &before, sizeof(settings)) != 0) {
        // Everything applies at once. Image Quality rebuilds the targets the
        // frame is drawn into (the trail restarts).
        if (settings.graphics != before.graphics) {
          motion_blur_ok = scene_targets_build(&scene_rt, &accum_rt, motion_blur_ok, width, height,
                                               settings.graphics);
          accum_valid = false;
        }
        apply_settings(&settings, &m);
        if (settings.rumble && !before.rumble) platform_rumble(0.6f, 200);   // a taste of it
      }
      if (act == SETTINGS_EXIT) {
        if (progress_path_ok) game_settings_save(progress_path, &settings);
        state = settings_return;
      } else if (act == SETTINGS_START) {
        // Race Setup's Start Race: kept for next time, then the stage loads
        // (one that failed to load stays in the list, as its confirm does).
        if (progress_path_ok) game_settings_save(progress_path, &settings);
        if (stage_preview_ok) {
          state = STATE_STAGE_LOADING;
          stage_loadcnt = 30;
        } else {
          state = STATE_STAGE_SELECT;
        }
      } else if (act == SETTINGS_CAREER_RESET) {
        // Settings > Reset RPG Mode: the career from scratch, saved.
        career_reset(&csave);
        csave_race = csave;   // a reset from the pause menu stays reset on quitting
        if (career_path[0]) career_save(career_path, &csave);
      } else if (act == SETTINGS_BENCH || act == SETTINGS_STRESS) {
        if (progress_path_ok) game_settings_save(progress_path, &settings);
        bench_start = true;
        bench.stress = act == SETTINGS_STRESS;
      }
    } else if (state == STATE_BENCH_RESULT) {
      if (KEY_EDGE(BTN_CONFIRM) || KEY_EDGE(BTN_CANCEL)) state = STATE_MAIN_MENU;
    }

    // The Performance Test: Free Play on a fixed stage and car, from a fixed
    // seed, through the normal loading and intro. NFM_SCREENSHOT_MENU=bench
    // starts it headless (NFM_BENCH_SECONDS shortens it).
    if (screenshot_menu && strcmp(screenshot_menu, "bench") == 0 && frame == 1) bench_start = true;
    if (bench_start) {
      bench_start = false;
      if (!bench.f) bench.f = malloc(sizeof(BenchFrame) * BENCH_MAX_FRAMES);
      if (bench.f) {
        bench.active = true;
        bench.measuring = false;
        bench.n = 0;
        bench.ended_early = false;
        gmode = GMODE_FREE_PLAY;
        ext_normal = ext_career = g_ext_career = false;
        if (getenv("NFM_BENCH_STRESS")) bench.stress = true;
        // The Stress Test races Free Play's last stage, Extended's biggest
        // (28, "A Competitive Ending", 189 pieces).
        stage_num = getenv("NFM_BENCH_STAGE") ? atoi(getenv("NFM_BENCH_STAGE"))
                    : bench.stress            ? FREE_PLAY_LAST_STAGE
                                              : BENCH_STAGE;
        car_index = BENCH_CAR;
        stage_read_name(stage_num, stage_name_buf, sizeof(stage_name_buf));
        nfm_set_seed(BENCH_SEED);
        state = STATE_STAGE_LOADING;
        stage_loadcnt = 30;
      }
    }

    // The menu track (Java's intertrack): inishcarselect() starts it as car
    // select opens (xtGraphics.java:5074-5077) and it plays on through the
    // stage list, until a stage is confirmed (:2591, :2655) or the list is
    // left with Exit (:2674). play() always starts from the top.
    {
      const bool want_interface =
          state == STATE_CAR_SELECT || state == STATE_STAGE_SELECT || state == STATE_STAGE_LOCKED ||
          (state == STATE_SETTINGS && settings_return == STATE_STAGE_SELECT);
      if (want_interface && !interface_playing && interface_music.bytes) {
        audio_start_music(&audio, &interface_music);
        audio_set_music_muted(&audio, false);
        interface_playing = true;
      } else if (!want_interface && interface_playing) {
        audio_stop_music(&audio);
        interface_playing = false;
      }
    }

    // Tear the finished race's world down once we are back on a menu that
    // can only be reached by ENDING a race, so the setup below runs again
    // for the next one. Without this, `!all_objs` stayed false for the
    // rest of the process after the very first race: every later start
    // skipped the whole block -- stage load, car slot, bot draw,
    // xt_graphics_stub_init, record_reset, starcnt, race_holdit -- and
    // dropped the player back into the previous race's leftover state,
    // hold card and all. Picking a different car changed nothing, and
    // the hold card sent you straight back to the menu, which is the
    // "it loops back to the match I just lost" report.
    //
    // Only MAIN_MENU and GAMEMODE_MENU qualify. Every other non-racing
    // state either draws the race world (the replays, the pause menu and
    // its cantreply banner) or can return to something that does --
    // STATE_INSTRUCTIONS is reachable FROM the pause menu and goes back
    // to it, so freeing there would pull the world out from under it.
    // Both qualifying states are reached only at boot, when this is
    // already NULL, or after a race is genuinely over.
    // Restart Race (the pause menu) frees it too, so the race is set up anew.
    if (all_objs && (state == STATE_MAIN_MENU || state == STATE_GAMEMODE_MENU || restart_race)) {
      restart_race = false;
      free(all_objs); all_objs = NULL;
      free(visible_idx); visible_idx = NULL;
      free(rank); rank = NULL;
      free(order); order = NULL;
      free(smooth_dist); smooth_dist = NULL;
      total_objs = 0;
      bench.active = false; // a test quit from the pause menu ends here
    }

    if ((state == STATE_RACING || state == STATE_STAGE_INTRO) && !all_objs) {
      // One-time transition into racing: build everything the physics/
      // draw loop below needs, parameterised by whatever the menu (or
      // the screenshot hook's defaults) selected. Mirrors this file's
      // own pre-menu M1/M2 setup exactly, just deferred and parameterised
      // by car_index/stage_num instead of hardcoded.
      // Reloads unconditionally, even if the stage-select screen's own
      // live 3D preview (see load_stage_objects's own doc comment)
      // already loaded this exact stage_num -- correctly frees whatever
      // was there first either way, so this is a redundant-but-harmless
      // reparse in the common case, not a leak.
      // xtGraphics.java's musicomp() (fase 6, right before racing's own
      // fase 0 begins) resets `m.ih=0`/`m.iw=0`/`m.h=450`/`m.w=800` -- the
      // full-canvas viewport racing needs. Without this, the stage-select
      // screen's OWN narrower viewport (Part 29's `m.iw=65`/`ih=25`/
      // `h=425`/`w=735`, armed for its letterboxed 3D preview window)
      // would still be sitting on the shared `m` instance here, and
      // medium_d()/cont_o_d() would keep clipping the racing scene's
      // backdrop to that smaller rectangle -- exactly the black-bordered
      // render a user reported after Part 29 shipped without this reset.
      m.ih = 0;
      m.iw = 0;
      m.h = 450;
      m.w = 800;

      DIAG_PHASE("race setup: loading the stage");
      // ---- NFM_EXT_STAGE=pack:entry (e.g. tracks:1, matchtracks:26m3.txt):
      // race one of Extended's stages, headless (ext_stage.c).
      // Extended's normal mode races its own stages the same way.
      const char *ext_stage_env = getenv("NFM_EXT_STAGE");
      char ext_spec[48] = "";
      if (ext_stage_env) snprintf(ext_spec, sizeof(ext_spec), "%s", ext_stage_env);
      else if (ext_normal) ext_stage_spec(stage_num, ptmatch, ext_spec, sizeof(ext_spec));
      else if (gmode == GMODE_FREE_PLAY && free_play_ext_stage(stage_num))
        ext_stage_spec(free_play_ext_stage(stage_num), 1, ext_spec, sizeof(ext_spec));
      ExtStageInfo ext_info;
      memset(&ext_info, 0, sizeof(ext_info));
      bool stage_ok = ext_spec[0]
          ? load_ext_stage_objects(&stage_objects, &stage_count, stage_count, ext_models, &m, &t, &cp, ext_spec,
                                   ext_car_of(car_index), &ext_info)
          : load_stage_objects(&stage_objects, &stage_count, stage_count,
                               base_models, &m, &t, &cp, stage_num, NULL, NULL);
      // Extended's stages by its own numbers: the career's, its normal
      // mode's (Free Play's 28 on are those). load_stage_objects sets NFM's.
      if (ext_spec[0] && !ext_stage_env)
        cp.stage = (gmode == GMODE_FREE_PLAY && !ext_normal) ? free_play_ext_stage(stage_num) : stage_num;
      // xtGraphics.wallcode's walls (careermode$m, realwalls).
      career_walls[0] = ext_info.wallr;
      career_walls[1] = ext_info.walll;
      career_walls[2] = ext_info.wallt;
      career_walls[3] = ext_info.wallb;
      // ----
      if (!stage_ok) {
        // Java's stage == -3: no race on a stage that failed its checks
        // (missing file, bad model id, too many objects or trackers, under
        // two checkpoints). Racing it anyway hung mad_drive.
        fprintf(stderr, "could not load stages/%d.txt -- back to the menu\n", stage_num);
        stage_preview_loaded_num = -1;
        state = STATE_MAIN_MENU;
        goto race_setup_done;
      }
      // The race reloads the stage and drives the camera, so the stage
      // list's preview no longer holds what it loaded: make it load and
      // re-arm its dive-in camera again when the list is next shown.
      // Without this, quitting back to the list showed no preview until
      // the player moved off the stage and back.
      stage_preview_loaded_num = -1;
      fprintf(stderr, "loaded stage: %d objects, %d trackers, %d checkpoints (%d laps)\n",
              stage_count, t.nt, cp.nsp, cp.nlaps);

      // Stage music -- Java xtGraphics.java:2989 `loadstrack` loads
      // music/stage{N}.zip and plays it on a loop for the whole stage.
      // Only reload when the stage actually changed (re-entering the
      // SAME stage, e.g. after a post-race "Continue" back into the same
      // campaign slot, shouldn't restart the track from 0 -- though in
      // practice this whole setup block only runs once per STATE_RACING
      // entry anyway, so this guard mainly documents intent).
      const int32_t music_id = stage_music_id(stage_num, MUSIC_MODE);
      if (stage_music_loaded_for != music_id) {
        audio_stop_music(&audio); // stop referencing the OLD track before freeing it
        interface_playing = false;
        radical_track_free(&stage_music);
        if (load_music_track(music_id, &stage_music)) {
          stage_music_loaded_for = music_id;
        } else {
          stage_music_loaded_for = -1;
        }
      }
      // Entering from the stage card, the track waits for the card's own
      // loadmusic() step; anything that drops straight into the race (the
      // headless hooks) starts it here.
      stage_music_pending = state == STATE_STAGE_INTRO && stage_music_loaded_for == music_id;
      if (!stage_music_pending && stage_music_loaded_for == music_id) {
        if (g_career_ogg[1] && ext_career)
          audio_start_ogg(&audio, g_career_ogg[0], g_career_ogg_len[0], g_career_ogg[1], g_career_ogg_len[1]);
        else
          audio_start_music(&audio, &stage_music);
        interface_playing = false;
      }
      // GameSparker.java:2755-2767 -- the card's title line.
      {
        int32_t shown = stage_num;
        if (ext_normal) shown = stage_num;
        else if (shown > 27) shown -= 27;
        else if (shown > 10) shown -= 10;
        snprintf(stage_asay, sizeof(stage_asay), "Stage %d:  %s ", shown, stage_name_buf);
      }

      // Real HUD panel images -- AFTER stage load, matching web/main.js's
      // own ordering ("HUD assets. After loadstage, because loadsnap()
      // tints with medium.snap"): m.snap only gets its real per-stage
      // values from the stage file's own `snap(...)` command inside
      // game_sparker_loadstage, just called above.
      g_hud_dark = m.darksky;
      for (int32_t c = 0; c < 3; c++) g_hud_sky[c] = m.csky[c];
      {
        VfsZip images_zip;
        if (vfs_read_zip("data/images.zip", &images_zip)) {
          const int32_t snap[3] = {m.snap[0], m.snap[1], m.snap[2]};
          // The bars keep their own colours on a dark sky, so their fill
          // reads against their frame; the plates behind their labels (the
          // Java's boxes, drawn before them) do the rest.
          const bool hud_dark = g_hud_dark;
          g_hud_dark = false;
          hud_images.dmg = load_hud_gif(&images_zip, "damage.gif", snap, hud_images.dmg);
          hud_images.pwr = load_hud_gif(&images_zip, "power.gif", snap, hud_images.pwr);
          g_hud_dark = hud_dark;
          {
            // Extended's Special bar: a PNG of this port's, already in its
            // colours and transparent -- hud_recolor() keys on the GIFs' grey
            // background, which this has not -- used as it is.
            int32_t len = 0;
            uint8_t *bytes = vfs_read_bytes(i18n_lang() == I18N_LANG_ES ? "data/port/es/special.png"
                                                                        : "data/port/special.png", &len);
            PngImage img;
            if (bytes && png_decode(bytes, (size_t)len, &img)) {
              hud_images.spec.tex = upload_or_refill(hud_images.spec, img.rgba, img.width, img.height);
              hud_images.spec.w = img.width;
              hud_images.spec.h = img.height;
              png_free(&img);
            }
            if (bytes) vfs_free_bytes(bytes);
          }
          hud_images.lap = load_hud_gif(&images_zip, "lap.gif", snap, hud_images.lap);
          hud_images.was = load_hud_gif(&images_zip, "wasted.gif", snap, hud_images.was);
          hud_images.pos = load_hud_gif(&images_zip, "position.gif", snap, hud_images.pos);
          hud_images.sped = load_hud_gif(&images_zip, "speed.gif", snap, hud_images.sped);
          const char *rank_names[8] = {"1.gif", "2.gif", "3.gif", "4.gif", "5.gif", "6.gif", "7.gif", "8.gif"};
          for (int32_t i = 0; i < 8; i++) hud_images.rank[i] = load_hud_gif(&images_zip, rank_names[i], snap, hud_images.rank[i]);
          // Countdown glyphs (Java xtGraphics.java:840-846, :909-910). loadsnap()
          // is what recolors them per stage in the original -- load_hud_gif
          // does the same recolor+alpha pass here. Java's array uses:
          //   ocntdn[0] = "0c.gif" (the GO glyph -- misnamed for historical reasons)
          //   ocntdn[1] = "1c.gif", ocntdn[2] = "2c.gif", ocntdn[3] = "3c.gif"
          const char *cntdn_names[4] = {"0c.gif", "1c.gif", "2c.gif", "3c.gif"};
          for (int32_t i = 0; i < 4; i++) hud_images.cntdn[i] = load_hud_gif(&images_zip, cntdn_names[i], snap, hud_images.cntdn[i]);
          // Win/loss hold-card glyphs -- xtGraphics.java:903-907 (raw load),
          // :9515-9516 (loadsnap() recolor, same treatment as the countdown).
          hud_images.youwon = load_hud_gif(&images_zip, "youwon.gif", snap, hud_images.youwon);
          hud_images.youlost = load_hud_gif(&images_zip, "youlost.gif", snap, hud_images.youlost);
          hud_images.youwastedem = load_hud_gif(&images_zip, "youwastedem.gif", snap, hud_images.youwastedem);
          hud_images.yourwasted = load_hud_gif(&images_zip, "yourwasted.gif", snap, hud_images.yourwasted);
          // The stage card's images -- snap()'s loadopsnap() trio, :9519-9522.
          intro_loadingmusic = load_opsnap_gif(&images_zip, "loadingmusic.gif", stage_num, 76, snap, intro_loadingmusic);
          intro_star[0] = load_opsnap_gif(&images_zip, "start1.gif", stage_num, 0, snap, intro_star[0]);
          intro_star[1] = load_opsnap_gif(&images_zip, "start2.gif", stage_num, 0, snap, intro_star[1]);
          intro_flaot = load_opsnap_gif(&images_zip, "float.gif", stage_num, 1, snap, intro_flaot);
          vfs_free_zip(&images_zip);
        } else {
          fprintf(stderr, "could not load data/images.zip -- HUD panels will be blank\n");
        }
      }

      // The actually-driven instance is always an #initCopy clone, never
      // the raw #initBuf object -- matching how the real game drives a
      // car (GameSparker.js's own tick rebuilds it via #initCopy every
      // "newcar" reset). This matters: #initBuf never allocates the
      // shadow/dust particle arrays (sx/sy/sz/osmag/scx/scz) even with
      // shadow=true (see ContO.js), so driving the raw base model
      // crashes the instant a skid rolls a dust() call with nonzero
      // lateral speed -- see mad.c's VERIFICATION STATUS comment for
      // where this was actually caught.
      //
      // Player's own car slot -- CUSTOM_CAR_INDEX falls back to built-in
      // slot 0's stats if the custom car failed to load, matching this
      // port's existing single-car fallback.
      int32_t car_slot;
      if (car_index == CUSTOM_CAR_INDEX) {
        car_slot = custom_car_ok ? CUSTOM_CAR_INDEX : 0;
      } else {
        car_slot = car_index;
      }
      sc[0] = car_slot;

      // xtGraphics.java:3116 -- `crs` off before the race starts. It is the
      // flag the car-select preview and the finish screen's unlocked-car
      // card turn on for their own fixed camera, and nothing in the race
      // ever cleared it: with it on, cont_o_d takes its `m.crs` branch and
      // every car shadow is projected flat onto m.ground, under ramps and
      // all, instead of onto the track piece the car is on (and plane_d's
      // distant-face simplification is disabled).
      m.crs = false;
      record_free(&rpd); // last race's replay ring -- record_init() just zeroes it
      record_init(&rpd);
      // :3144-3149 -- musicomp()'s own coin flip for which side the
      // countdown face appears on. Java writes `Math.random() >
      // Math.random()`, which it evaluates left to right; C does not
      // guarantee operand order, so the two draws are sequenced through
      // named temporaries, the same way medium.c already handles this.
      {
        double dudo_a = nfm_random();
        double dudo_b = nfm_random();
        race_dudo = (dudo_a > dudo_b) ? 250 : 428;
      }
      xt_graphics_stub_init(&xt);
      xt.extended = true;      // Extended's gameplay (mad.c)
      // Classic Mode is NFM 2's stages; Extended's own (its normal mode, or a
      // headless NFM_EXT_STAGE other than classictracks) are not.
      xt.classicmode = !ext_spec[0] || strncmp(ext_spec, "classictracks", 13) == 0;
      xt.ptmatch = PT_ACTIVE ? ptmatch : 0;
      xt.wasteonly = false;   // set below for Free Play's Wasting Only
      xt.nfmpower = gmode == GMODE_NFM1 || gmode == GMODE_NFM2;   // NFM 2's own power rule there
      xt.career.careermode = false;   // set again below when the career races
      xt.im = 0;

      // xtGraphics.java:2354-2358 (loadstage(), top of the function) --
      // NFM1 races only 5 cars, not 7; slot 4 (the 5th car) moves to the
      // position slot 6 normally occupies in the full 7-car grid. This is
      // set BEFORE the u[].reset() loop below, matching Java exactly, so
      // every per-race loop from here on (reset, sortcars, construction,
      // collision, checkstat, AI, cleanup) sees the right car count.
      nplayers = race_nplayers > 0 ? race_nplayers : (ext_normal ? EXT_NORMAL_PLAYERS : BOTS_MAX_PLAYERS);
      if (bench.active && bench.stress) nplayers = NFM_MAX_CARS;
      fp_race = gmode == GMODE_FREE_PLAY && !ext_normal && !bench.active && race_nplayers == 0;
      if (fp_race) nplayers = settings.fp_opponents + 2;
      if (fp_race && settings.fp_laps > 0) cp.nlaps = settings.fp_laps;   // Race Setup's laps (the stage is loaded)
      xt.wasteonly = fp_race && settings.fp_win == 1;
      race_specials = gmode != GMODE_NFM1 && gmode != GMODE_NFM2 && !(fp_race && !settings.fp_specials);
      if (ext_career) {
        // randomno (XT 18144): the career's field size for the stage.
        memset(&crace, 0, sizeof(crace));
        crace.stage = stage_num;
        crace.bonus = g_career_bonus;
        crace.hardstage = !g_career_bonus && career_mode == 1;
        crace.scalelevels = !g_career_bonus && career_mode == 2;
        crace.nolevels = !g_career_bonus && career_mode == 3;
        crace.sc[0] = ext_car_of(car_index);
        career_randomno(&crace, &csave);
        nplayers = crace.nplayers;
        if (race_nplayers > 0) nplayers = crace.nplayers = race_nplayers;
      }
      // The starting grid, rows of three: Extended's (GameSparker.java:
      // 1555-1574), which for the first seven is exactly NFM 2's
      // inishcarselect() grid (xtGraphics.java:4847-4860): x 0/-350/350,
      // z -760/-380/-380 then 760 further back per row.
      int32_t kXstart[NFM_MAX_CARS], kZstart[NFM_MAX_CARS];
      for (int32_t j = 0; j < NFM_MAX_CARS; j++) {
        static const int32_t kRowX[3] = {0, -350, 350};
        kXstart[j] = kRowX[j % 3];
        kZstart[j] = (j % 3 == 0 ? -760 : -380) + (j / 3) * 760;
      }
      if (gmode == GMODE_NFM1 && race_nplayers == 0) {
        nplayers = 5;
        kXstart[4] = 0;
        kZstart[4] = 760;
      }

      // GameSparker.java:2723-2734 (loadstage(), called from fase==2):
      // EVERY slot's Control resets BEFORE sortcars() re-rolls sc[1..6]
      // for this race (so those calls read whichever cars last raced
      // that slot -- a real, harmless one-race-stale quirk in the
      // original, see this file's sc[] field comment above), THEN
      // sortcars() picks this race's opponents, THEN every car is
      // actually constructed/reseto'd against the FRESH sc[].
      for (int32_t i = 0; i < nplayers; i++) {
        control_init(&control[i], &m);
        if (xt.extended) control_reset_ext(&control[i], &cp, sc[i], xt.classicmode);   // Extended's AI
        else control_reset(&control[i], &cp, sc[i]);
      }
      // Extended's AI repair target: its classictracks mark a route point
      // with `setpoint` (Contva.fixpoint, Control.java:9785 fpnt[0]); the
      // same point is in NFM 2's file, so find it by its place. 13 of the
      // 17 stages have one; the others keep NFM 2's nearest-hoop target.
      if (ext_spec[0]) {
        // Extended's own stages carry it themselves (ext_loadstage).
        if (ext_info.numfixes > 0)
          for (int32_t i = 1; i < nplayers; i++) control[i].fpnt[0] = ext_info.fixpoint[0];
      } else if (xt.extended && stage_num >= 11 && stage_num <= 27) {
        VfsZip tracks;
        if (vfs_read_zip("ext/data/Files/classictracks.radq", &tracks)) {
          char want[16];
          snprintf(want, sizeof(want), "%d.txt", stage_num - 10);
          for (int32_t e = 0; e < tracks.count; e++) {
            if (strcmp(tracks.entries[e].name, want) != 0) continue;
            char *text = vfs_entry_text(&tracks.entries[e]);
            const char *sp = text ? strstr(text, "setpoint(") : NULL;
            int32_t id, x, z;
            if (sp && sscanf(sp, "setpoint(%d,%d,%d", &id, &x, &z) == 3) {
              for (int32_t k = 0; k < cp.n; k++) {
                if (cp.x[k] == x && cp.z[k] == z) {
                  for (int32_t i = 1; i < nplayers; i++) control[i].fpnt[0] = k;
                  break;
                }
              }
            }
            free(text);
            break;
          }
          vfs_free_zip(&tracks);
        }
      }
      control_falseo(&control[0], 0);

      bots_sortcars(sc, (GameMode)gmode, &progress, stage_num);
      // Past NFM 2's seven: random NFM 2 cars, no repeats while any are
      // left (the modes that race more cars pick their own afterwards).
      for (int32_t i = BOTS_MAX_PLAYERS; i < nplayers; i++) {
        int32_t pick = 0;
        for (int32_t tries = 0; tries < 64; tries++) {
          pick = (int32_t)(nfm_random() * 16.0);
          bool used = false;
          for (int32_t k = 0; k < i; k++) used = used || sc[k] == pick;
          if (!used || i >= 16) break;
        }
        sc[i] = pick;
      }
      if (fp_race) free_play_rivals(sc, nplayers, &settings);
      // Extended's normal mode: its own sortcars (ext_mode.c), 11 cars; on
      // stage 26 every car, the player's too, is the match's.
      if (ext_normal && !ext_career) {
        ext_sortcars(sc, nplayers, stage_num, ext_prog.normal_unlocked, stage_num == EXT_PT_STAGE ? ptmatch : 0);
      }
      if (ext_career) {
        // The career's field (career_lineup.c), every opponent's level and
        // stat points (career_stats.c); the player's from the save.
        const int32_t me = crace.sc[0];
        career_sortcars(&crace, &csave);
        // A fresh Madness holds level 1 everywhere: what stage 31 keeps (its
        // levels are never set, career_stats.c).
        for (int32_t i = 1; i < CAREER_MAX_PLAYERS; i++) crace.level[i] = 1;
        crace.level[0] = csave.level[me];
        for (int32_t k = 0; k < CS_N; k++) crace.sp[0][k] = csave.sp[me][k];
        float gripreset[CAREER_CARS];
        for (int32_t e = 0; e < CAREER_CARS; e++) gripreset[e] = cd.grip[ext_car_to_port(e)];
        career_airpgstats(&crace, &csave, gripreset);
        for (int32_t i = 0; i < nplayers; i++) sc[i] = ext_car_to_port(crace.sc[i]);
      }
      // A tournament match reached straight from a hook (NFM_PTMATCH).
      if (PT_ACTIVE && pt.match != ptmatch) {
        ext_pt_start_tournament(&pt);
        pt.match = ptmatch;
        ext_pt_start_match(&pt);
      }

      for (int32_t i = 0; i < nplayers; i++) {
        ContO *base = RACE_MODEL(i, sc[i]);
        int32_t gx = kXstart[i], gy = 250 - base->grat, gz = kZstart[i];
        if (ext_career) {
          int32_t gfloor;
          career_grid(&crace, &csave, i, base->grat, &gx, &gy, &gz, &gfloor);
        } else if (ext_normal && i == nplayers - 1 && nplayers % 3 != 1) {
          // Extended's grid puts a last car that would start a new row of
          // three centred behind (GameSparker.java 1555-1574): normal
          // mode's 11th.
          gx = 0;
          gz = (i / 3) * 760;
        }
        cont_o_recopy(&co[i], base, gx, gy, gz, 0);
        RACE_LOOK(i);
        // Keyboard/pad input drives slot 0 -- see platform/<name>/input.h
        // for the exact keymap. Polled once per frame in the loop below,
        // after platform_poll() reads this frame's hardware state. Slots
        // 1-6 are driven by control_preform() instead, see the tick loop.
        // Each car races on its own copy of the stat tables, as Extended's
        // Madness instances do: specials (and later tourney and career
        // stats) rewrite a car's values every frame without touching the
        // others or the car-select screen's `cd`.
        race_base[i] = cd;
        if (ext_career) {
          career_apply_stats(&race_base[i], sc[i], crace.sc[i], crace.sp[i], crace.level[i], crace.shadow[i], i == 0);
          if (crace.beast[i]) {
            // nitroandspecials (XT 6390-6397): a beast loses power three
            // times as fast and hits from twice as far (Madness.multiplier).
            race_base[i].powerloss[sc[i]] *= 3;
            race_base[i].clrad[sc[i]] *= 2;
          }
        }
        live_cd[i] = race_base[i];
        mad_init(&mad[i], &live_cd[i], &m, &rpd, &xt, i);
        mad_reseto(&mad[i], sc[i], &co[i], &cp);
        // mad_reseto just set M A S H E E N's dent radius (Madness.java
        // 1080-1087); a beast's multiplier 2 still applies on top of it
        // (Madness.java 595: clrad * multiplier, XT 6194).
        if (ext_career && crace.beast[i] && sc[i] == 13) {
          live_cd[i].clrad[13] *= 2;
          race_base[i].clrad[13] = live_cd[i].clrad[13];
        }
        if (ext_career && i == 0) mad[i].powfactor = career_power_factor(crace.sc[0], crace.sp[0], crace.level[0]);
        career_seen_dested[i] = 0;
        mad[i].career = NULL;   // career_stage_start below, in the career
      }
      specials_reset(&specials);
      if (ext_career) {
        career_run_start(&crun, &crace, &csave);
        // The player's perks, read by every car's physics and the specials.
        for (int32_t i = 0; i < nplayers; i++) mad[i].perks = &crun.perks;
        specials.perks = &crun.perks;
        // The targets' career levels, for sortpower's diffmod (XT 7395).
        for (int32_t i = 0; i < nplayers; i++) specials.level[i] = crace.level[i];
        // Headless: both bonus stat point popups up from the start (they pay).
        if (getenv("NFM_CAREER_POPUPS")) {
          crun.winchance[1] = 40;
          crun.killchance[1] = 500;
        }
        career_seen_clear = 0;
        career_settled = false;
        csave_race = csave;
        // The stage's recorded bots (GameSparker.java 1749-1790).
        career_bots_free(&cbots);
        int32_t bot_slots[NFM_MAX_CARS];
        const bool hard = csave.unlocked == stage_num || crace.hardstage;
        const int32_t nbots = career_bots_slots(stage_num, nplayers, hard, crace.bonus, bot_slots);
        if (nbots > 0) {
          char path[64];
          snprintf(path, sizeof(path), "ext/data/Files/Bots/stage%d.radq", (int)stage_num);
          VfsZip zip;
          if (vfs_read_zip(path, &zip)) {
            for (int32_t b = 0; b < nbots; b++) {
              char want[16];
              snprintf(want, sizeof(want), "%d.txt", (int)bot_slots[b]);
              for (int32_t e = 0; e < zip.count; e++) {
                if (strcmp(zip.entries[e].name, want) != 0) continue;
                char *text = vfs_entry_text(&zip.entries[e]);
                if (text && text[0]) career_bots_parse(&cbots, bot_slots[b], text);
                free(text);
              }
            }
            vfs_free_zip(&zip);
          }
        }
        for (int32_t i = 0; i < nplayers; i++) {
          const char *c = sc[i] == 9 ? "SoJ" : (sc[i] == 2 ? "Wow C." : car_name(sc[i]));
          const char *pre = crace.shadow[i] ? "Shadow " : crace.undead[i] ? "Undead " : crace.beast[i] ? "Beast " : "";
          snprintf(career_names_buf[i], sizeof(career_names_buf[i]), "%s%s", pre, c);
          career_names[i] = career_names_buf[i];
        }
        // The career AI (control.c, control_career.inc): its context from
        // the field, the race's Contva with the stage's fix points, and
        // Extended's whole reset for every slot (GameSparker.js 1264).
        {
          XtCareerAI *cx = &xt.career;
          memset(cx, 0, sizeof(*cx));
          cx->targetcar = 100;   // none
          cx->careermode = true;
          cx->nplayers = nplayers;
          cx->bonus = crace.bonus;
          cx->hardstage = crace.hardstage;
          cx->unlocked = csave.unlocked;
          for (int32_t e = 0; e < CAREER_CARS; e++) cx->extpoints[e] = csave.extpoints[e];
          for (int32_t k = 0; k < nplayers; k++) {
            cx->sc[k] = crace.sc[k];
            cx->beastopponent[k] = cx->beast[k] = crace.beast[k];
            cx->shadowcar[k] = crace.shadow[k];
            cx->undead[k] = crace.undead[k];
            cx->level[k] = crace.level[k];
            cx->aistrsp[k] = crace.sp[k][CS_STR];
            cx->endsp[k] = crace.sp[k][CS_END];
          }
          cx->usermad = &mad[0];
          xt_contva_reset(&cx->contva);
          xt_contva_resetfp(&cx->contva);
          for (int32_t f = 0; f < ext_info.numfixes && f < XT_CONTVA_FIX; f++) cx->contva.fixpoint[f] = ext_info.fixpoint[f];
          cx->contva.numfixes = ext_info.numfixes;
          for (int32_t k = 0; k < nplayers; k++) control_reset_career(&control[k], &cp, sc[k], &xt, &mad[k]);
        }
        for (int32_t i = 0; i < nplayers; i++) {
          mad[i].isabot = false;   // set each tick once the race is on
          cbots_last_clear[i] = 0;
          cbots_since[i] = 0;
          mad[i].bot_hit = false;
        }
        // The stage's scripts (careermode$m), on the race just set up.
        memset(&cworld, 0, sizeof(cworld));
        cworld.mads = mad;
        cworld.cars = co;
        cworld.pieces = stage_objects;
        cworld.npieces = stage_count;
        cworld.cp = &cp;
        cworld.controls = control;
        cworld.m = &m;
        cworld.t = &t;
        cworld.contva = &xt.career.contva;
        cworld.orig = &cd;
        cworld.botbreak = cbots.brk;
        cworld.names = career_names;
        cworld.nplayers = nplayers;
        for (int32_t k = 0; k < 4; k++) cworld.walls[k] = career_walls[k];
        cworld.starcnt = 130;
        career_stage_start(&cstage, &crace, &csave, &cworld);
        for (int32_t i = 0; i < nplayers; i++) mad[i].career = &cstage;
        if (cstage_boss_music) {
          career_ogg_free();
          cstage_boss_music = false;
        }
        if (!cstage_snd_loaded) {
          // careermode$m's two sounds (XT 14945-14947): the Titan's and the
          // Desert Night shadow's teleport warning, Ghost Planet's scare.
          static const char *const kSnd[2] = {"ext/data/Files/sounds/redflash.wav", "ext/data/Files/sounds/caught.wav"};
          WavClip *const dst[2] = {&snd_redflash, &snd_caught};
          for (int32_t q = 0; q < 2; q++) {
            int32_t len = 0;
            uint8_t *bytes = vfs_read_bytes(kSnd[q], &len);
            if (bytes && !wav_decode_downmix(bytes, (size_t)len, dst[q])) fprintf(stderr, "%s failed to decode (wav)\n", kSnd[q]);
            free(bytes);
          }
          cstage_snd_loaded = true;
        }
      }
      g_arrow_skip = ext_career ? cstage_arrow_skip : NULL;
      g_arrow_levels = ext_career ? crace.level : NULL;
      g_arrow_mads = xt.extended ? mad : NULL;
      g_arrow_specials = xt.extended ? &specials : NULL;
      g_arrow_swap = &ext_listbars;
      g_board_skip = ext_career ? cstage_board_skip : NULL;
      for (int32_t k = 0; k < NFM_MAX_CARS; k++) cstage_arrow_skip[k] = cstage_board_skip[k] = false;
      m.polyoutline_on = false;
      for (int32_t k = 0; k < nplayers; k++) co[k].flame_on = false;
      if (ext_career) career_medium_init(&cstage_medium, &m);
      // NFM_HOOK_SPECIALS=1: every bar starts full, to see specials headless.
      if (getenv("NFM_HOOK_SPECIALS")) for (int32_t i = 0; i < nplayers; i++) mad[i].spatk = mad[i].speclast = mad[i].speclast2 = 120.0f;
      // NFM_HOOK_VIEW=n and NFM_HOOK_LISTBARS=1: a camera and the list bars, headless.
      if (getenv("NFM_HOOK_VIEW")) race_view = atoi(getenv("NFM_HOOK_VIEW"));
      if (getenv("NFM_HOOK_LISTBARS")) ext_listbars = true;
      if (getenv("NFM_HOOK_ARRACE")) control[0].arrace = true;   // the arrow on the cars
      // GameSparker.java:2768 -- record.reset(array) at the tail of
      // loadstage(), AFTER every car's ContO is (re)constructed for this
      // race but BEFORE the first tick -- clears the whole replay ring
      // (positions/sparks/dents/hcaught) so a previous race's highlight
      // reel never bleeds into this one.
      {
        ContO *record_car_ptrs[NFM_MAX_CARS];
        for (int32_t i = 0; i < nplayers; i++) record_car_ptrs[i] = &co[i];
        record_reset(&rpd, record_car_ptrs, nplayers);
      }
      // Reset countdown -- Java xtGraphics.java:1501-1502 (fase 1 entry).
      starcnt = 130;
      gocnt = 3;
      // GameSparker.java:958-963 -- arms the fast/"b=true" pre-race flyby
      // camera at the exact moment racing (re)starts, matching the real
      // Java's own `xtGraphics.starcnt == 130` one-shot setup inside the
      // fase==0 tick (this port hits that "just started" condition here
      // instead, once, rather than re-checking `starcnt==130` every
      // tick). See medium_around()'s own doc comment and the per-frame
      // camera dispatch below (around STATE_RACING's medium_follow call)
      // for the rest of this feature -- a cinematic orbit around the
      // player's own car shown BEFORE the 3-2-1-GO countdown appears.
      m.adv = 1900;
      m.zy = 40;
      m.vxz = 70;
      race_holdit = false;
      race_holdcnt = 0;
      audio_last_clear = 0;
      // Headless preview hook -- NFM_SCREENSHOT_MENU=holdcard forces the
      // win hold-card on immediately instead of waiting for a real lap
      // count (input can't be simulated under this sandbox's Xvfb, see
      // the screenshot_menu comment above), so a regression check can
      // still capture what the card looks like.
      if (screenshot_menu && strcmp(screenshot_menu, "holdcard") == 0) {
        race_holdit = true;
        race_winner = true;
      }
      // Headless preview hook -- NFM_SCREENSHOT_MENU=replay{0,1,2,crash}
      // forces STATE_REPLAY on immediately with a synthetic frozen
      // snapshot (a real close-finish/crash can't be scripted under this
      // sandbox's Xvfb either, same reasoning as the holdcard hook above)
      // so the highlight-reel camera choreography can still be screenshot-
      // regression-tested. Suffix picks record.closefinish (0/1/2) or the
      // "local player caught" crash-reel path.
      if (screenshot_menu && strncmp(screenshot_menu, "replay", 6) == 0) {
        for (int32_t i = 0; i < nplayers; i++) cont_o_recopy(&rpd.starcar[i], &co[i], 0, 0, 0, 0);
        rpd.hcaught = true;
        if (strcmp(screenshot_menu, "replaycrash") == 0) {
          rpd.wasted = 0;
          rpd.whenwasted = 229;
        } else {
          rpd.wasted = 1;
          rpd.whenwasted = 165 + 1;
          rpd.closefinish = (strcmp(screenshot_menu, "replay1") == 0) ? 1 :
                             (strcmp(screenshot_menu, "replay2") == 0) ? 2 : 0;
        }
        replay_tick = 0;
        replay_cam = 0;
        replay_loops = 0;
        state = STATE_REPLAY;
      }

      // Every object the painter's-algorithm sort below draws, all
      // racers first -- see the ORDERING comment further down. Bounded
      // by nplayers, not BOTS_MAX_PLAYERS: co[nplayers..6] were never
      // constructed above (NFM1's 5-car grid) and are still garbage
      // stack memory, so including them here would draw/dereference junk.
      total_objs = nplayers + stage_count;
      all_objs = malloc(sizeof(ContO *) * (size_t)total_objs);
      for (int32_t i = 0; i < nplayers; i++) all_objs[i] = &co[i];
      for (int32_t i = 0; i < stage_count; i++) all_objs[nplayers + i] = &stage_objects[i];
      visible_idx = malloc(sizeof(int32_t) * (size_t)total_objs);
      rank = malloc(sizeof(int32_t) * (size_t)total_objs);
      order = malloc(sizeof(int32_t) * (size_t)total_objs);
      smooth_dist = malloc(sizeof(int32_t) * (size_t)total_objs);
      smooth_ready = false;
      smooth_captured = false;
      race_confirm_latch = false;

      last_ticks_ms = platform_ticks_ms();
      accumulator_ms = 0.0;
    }
  race_setup_done:

    if (state == STATE_RACING) {
      // The Performance Test's car drives itself (control_preform below).
      if (!bench.active) input_poll(&control[0]);
      // Headless replay hook: hold the throttle so there is motion to replay.
      if (screenshot_menu && strcmp(screenshot_menu, "pausereplay") == 0) control[0].up = true;

      uint32_t now_ms = platform_ticks_ms();
      // Back from the pause menu, Settings or the pause replay: the time spent
      // there is not race time. Without this the first frame back ran the
      // capped 3 catch-up ticks (~159 ms of physics) in one burst.
      if (last_racing_frame != frame - 1) last_ticks_ms = now_ms;
      last_racing_frame = frame;
      race_frame_ms = (double)(now_ms - last_ticks_ms);
      accumulator_ms += race_frame_ms;
      last_ticks_ms = now_ms;
      if (accumulator_ms > MAX_ACCUMULATOR_MS) accumulator_ms = MAX_ACCUMULATOR_MS;
      if (sim_ticks_per_frame > 0) accumulator_ms = TICK_MS * sim_ticks_per_frame;

      // Real drive() physics against the real loaded stage's Trackers, at a
      // fixed 53ms/tick regardless of how often this loop iterates.
      // colide() (every ordered pair) -> drive() (every car) ->
      // check_points_checkstat() -> cont_o_step_fix() (every car) ->
      // control_preform() (bots only, using what checkstat just computed)
      // matches GameSparker.java's own simulate() ordering exactly
      // (:940-956) -- see cont_o.h's doc comment on the drive()/stepFix()
      // half of that ordering. preform() running AFTER drive()/checkstat()
      // in the SAME tick is deliberate, not a bug: it computes each bot's
      // input for the NEXT tick, matching Java's own one-tick-delayed AI
      // reaction exactly (a fresh bot's control flags all start false from
      // control_init/control_falseo, so it coasts for exactly one tick
      // before its first preform() call fires).
      Mad *mad_ptrs[NFM_MAX_CARS];
      ContO *co_ptrs[NFM_MAX_CARS];
      for (int32_t i = 0; i < nplayers; i++) { mad_ptrs[i] = &mad[i]; co_ptrs[i] = &co[i]; }
      // Every draw-phase EFFECT guards its own advance on m.interpolating
      // (16 sites across medium.c, cont_o.c and plane.c, all spelled
      // `if (!m->interpolating)`), and medium_d() already does the whole
      // per-pass bookkeeping around it -- rewinding the PRNG replay cursor
      // on an interpolated pass, starting a fresh recording otherwise.
      // The one thing nothing ever did was SET the flag, so it sat at its
      // zero-initialised false and every one of those guards passed on
      // every rendered frame.
      //
      // That is the same class of bug as calling mad_drive() per frame
      // instead of per tick (see TICK_MS's own comment above): the port
      // renders at the display's refresh rate but ticks at 18.9Hz, so at
      // 60Hz each effect advanced ~3.2x too fast. Visible on the repair
      // ring, which spins via `xy += 11` per electrify() call, but it was
      // never only that one -- sparks, chips, dust and the backdrop fades
      // all ran at display rate too.
      //
      // A frame that consumes no tick is exactly the JS's "interpolated
      // pass": the same simulation state drawn again. Marking it as such
      // makes all 16 guards behave as designed, without touching any of
      // them. Only states that actually run this accumulator get the
      // treatment -- menus never tick, so leaving it false there keeps
      // their own animations running at frame rate, as they always have.
      bool ticked_this_frame = false;
      // Confirm is read inside the tick (the hold card's continue, the
      // fly-by skip); latched so a press on a frame without a tick -- two
      // frames in three at 60Hz -- is not lost.
      if (KEY_EDGE(BTN_CONFIRM)) race_confirm_latch = true;
      while (accumulator_ms >= TICK_MS) {
        ticked_this_frame = true;
        smooth_capture(&smooth_prev, co, nplayers, &m);
        // Countdown tick -- Java xtGraphics.java:8010-8031. Decrement
        // starcnt every tick and update gocnt at the same thresholds.
        // Once starcnt hits 0 the countdown is done and never runs again.
        if (ext_career) career_stage_frame(&cstage);
        if (starcnt > 0) {
          starcnt--;
          // Sound triggers at the exact same ticks Java's own gocnt
          // updates fire (xtGraphics.java:8011-8030: three.play() at
          // starcnt==35 -- one tick before the "3" glyph even appears,
          // i.e. count-in -- then two/one/go.play() alongside each
          // glyph change).
          if (starcnt == 35 && snd_three.samples) audio_play(&audio, snd_three.samples, snd_three.frame_count, snd_three.sample_rate, 1.0f, false);
          if (starcnt == 24) { gocnt = 2; if (snd_two.samples) audio_play(&audio, snd_two.samples, snd_two.frame_count, snd_two.sample_rate, 1.0f, false); }
          else if (starcnt == 13) { gocnt = 1; if (snd_one.samples) audio_play(&audio, snd_one.samples, snd_one.frame_count, snd_one.sample_rate, 1.0f, false); }
          else if (starcnt == 2) { gocnt = 0; if (snd_go.samples) audio_play(&audio, snd_go.samples, snd_go.frame_count, snd_go.sample_rate, 1.0f, false); }
        }
        // GameSparker.java:939 -- the ENTIRE simulate step (collision,
        // physics, checkpoints, AI) is skipped while starcnt != 0, not
        // just the player's own input: every car sits frozen exactly
        // where the starting grid placed it (no gravity/suspension
        // settling) until the countdown reaches GO, matching the original
        // exactly rather than approximating "frozen" as "zeroed input."
        if (starcnt == 0) {
          // Physics-SFX one-shot triggers (crash/skid/scrape/gscrape) get
          // set by mad_colide()/mad_drive() below, at their own JS call
          // sites deep inside collision/wheel-contact code -- reset here,
          // BEFORE those run, not after xt_graphics_stub_playsounds()
          // (which doesn't touch these fields at all -- see xt_graphics.h's
          // own comment on why these four get their own slots instead of
          // sharing one with playsounds()'s air/engine/wasted decisions).
          xt.pending_crash = XT_CRASH_NONE;
          xt.pending_skid = XT_SKID_NONE;
          xt.pending_scrape = XT_SCRAPE_NONE;
          xt.pending_gscrape = false;
          g_diag.ticks++;
          DIAG_PHASE("race tick: collisions");
          if (ext_career) {
            // GameSparker.js 2134-2227: which cars may touch this tick, and
            // what Madness.drive reads off the car before it moves.
            cworld.starcnt = starcnt;
            career_stage_before_drive(&cstage, &cworld);
            career_stage_ghostmode(&cstage, &crace, &cworld, cstage_ghost);
          }
          for (int32_t i = 0; i < nplayers; i++) {
            for (int32_t j = 0; j < nplayers; j++) {
              if (i == j) continue;
              if (ext_career && (cstage_ghost[i][j] || cstage_ghost[j][i])) continue;
              const int32_t prevhitmag = mad[j].hitmag;
              mad_colide(&mad[i], &co[i], &mad[j], &co[j]);
              // The player's hits pay experience (Madness.java 996-1030):
              // the damage they did, up to the health the car had left;
              // none on a wreck (regx's hitgain, 304-309) or the undead (850).
              if (ext_career && i == 0 && !mad[j].dest && !crace.undead[j] && !cstage.undead[j]) {
                const int32_t maxmag = live_cd[j].maxmag[sc[j]];
                int32_t dmg = mad[j].hitmag - prevhitmag;
                const int32_t healthleft = maxmag - prevhitmag > 0 ? maxmag - prevhitmag : 0;
                if (dmg > healthleft) dmg = healthleft;
                career_xp_hit(&crun, &crace, &csave, j, dmg, maxmag, cd.maxmag[sc[j]]);
              }
            }
          }
          // A stunt armed in the air (loop 2, or the handbrake going down
          // off the ground, which arms it inside this very drive() call)
          // turns every direction into a stunt until landing. Tell the
          // input layer, so on the Vita only CROSS + stick reach the car
          // then and the throttle/brake triggers cannot loop it.
          // The second case is drive()'s own arming test, all of it: a
          // handbrake already held on the ground (`pushed`, a drift) arms
          // nothing when the wheels skip off a bump -- without that term the
          // throttle went dead for those ticks mid-drift.
          // Not on the landing tick (wheels down, loop 2 -> -1 inside drive()):
          // there drive() latches pu/pd/pl/pr from the keys held, so they must
          // be the driving ones -- with the stunt set a ZR held through the
          // landing latched nothing, and the next tick read it as "up" in the
          // post-stunt tilt (loop -1), flipping the car forward on its own.
          if (!bench.active)
            input_set_stunting(&control[0], (mad[0].loop == 2 && !mad[0].wtouch) ||
                                                (control[0].handb && !mad[0].wtouch && !mad[0].pushed &&
                                                 mad[0].loop == 0));
          DIAG_PHASE("race tick: driving");
          for (int32_t i = 0; i < nplayers; i++) {
            g_diag.aux[0] = i;
            if (ext_career && cstage.respawning[i]) {
              // GameSparker.js 2368-2373: a respawned car resets, keeping its laps.
              const int32_t clrad = live_cd[i].clrad[sc[i]];   // a beast M A S H E E N's, past reseto's
              career_stage_respawn_reset(&cstage, &mad[i], sc[i], &co[i], &cp);
              live_cd[i].clrad[sc[i]] = clrad;
              continue;
            }
            mad_drive(&mad[i], &control[i], &co[i], &t, &cp);
            if (ext_career) {
              // Stage 13's portals ran inside mad_drive (Madness.drive
              // 3252-3361); the recorded bot's side of an arrival.
              if (cstage.portal_unbreak[i]) cbots.brk[i] = false;
              if (cstage.portal_arrived[i]) {
                const int32_t ws = cstage.portal_whichset[i];
                if (ws >= 0 && ws < 10) cbots.specialtimer[i] = cbots.botoffset[ws][i];
              }
            }
          }
          DIAG_PHASE("race tick: checkpoints and repairs");
          // GameSparker.java:950-952 -- one record.rec() per car, BETWEEN
          // the drive() loop and checkstat(), every tick. Advances the
          // 300-tick position/spark/skid ring, ticks fix[]/dest[] down,
          // and (via dest[n]==230) can itself freeze the replay snapshot
          // for a "just got wasted" reel -- independent of checkstat's own
          // close-finish freeze trigger a few lines below.
          for (int32_t i = 0; i < nplayers; i++) {
            record_rec(&rpd, &co[i], i, mad[i].squash, mad[i].lastcolido, mad[i].cntdest, 0);
          }
          check_points_checkstat(&cp, mad_ptrs, co_ptrs, &rpd, nplayers, 0, 0);
          // The career's count (CheckPoints 149-155): stage 11's undead wrecks count as wasted.
          if (ext_career) cp.wasted = career_stage_wasted(&cstage, mad, nplayers);
          for (int32_t i = 0; i < nplayers; i++) cont_o_step_fix(&co[i]);
          // XtGraphics.js's own per-tick missedcp advance, under the same
          // guard xtGraphics.java:7917 puts it behind -- see the helper's
          // own doc comment (fase != -6 and multion < 2 are always true in
          // this single-player port, so only these three terms remain).
          if (!race_holdit && starcnt == 0 && !NFM_NO_ARROW_STAGE) {
            tick_missed_cp(&mad[0]);
          }
          // Stunt naming/scoring's ANNOUNCER half -- see hud_stunt_detect()'s
          // own doc comment for why this must be tick-scoped, not called
          // from the once-per-frame HUD draw code below. Player-only, same
          // reasoning as the checkpoint/carfixed sound triggers just below
          // (bots never narrate to the local listener). Suppressed while
          // the win/lose hold card is up: the `mad.trcnt == 10` block this
          // ports (xtGraphics.java:8154) sits inside the single
          // `if (!this.holdit)` opened at :8009, so a trick landed on the
          // coast-down after the flag never gets announced or scored.
          if (!race_holdit) {
            hud_stunt_detect(&mad[0], &xt, &m, &audio, &snd_powerup);
          }

          // Physics-SFX pump (Part 16) -- GameSparker.js:569's own
          // per-tick `xtGraphics.playsounds(array3[im], this.u[0],
          // checkPoints.stage)`. Player-only, same reasoning as every
          // other xt_graphics_stub_* trigger. Decides engine rev/air
          // loop/wasted loop and decrements crash/skid/scrape/gscrape
          // debounces; the drain below turns its pure-state outputs
          // (plus whatever mad_colide()/mad_drive() already set on
          // pending_crash/skid/scrape/gscrape above) into real
          // audio_play()/channel-stop calls.
          xt_graphics_stub_playsounds(&xt, &mad[0], &control[0], &m, starcnt);

          // One-shot triggers -- crash()/skid()/scrape()/gscrape() each
          // decided at most one outcome this tick (see xt_graphics.h's
          // own doc comment on why one slot per debounce is safe).
          switch (xt.pending_crash) {
            case XT_CRASH_CRASH1: play_wav_oneshot(&audio, &snd_crash[0]); break;
            case XT_CRASH_CRASH2: play_wav_oneshot(&audio, &snd_crash[1]); break;
            case XT_CRASH_CRASH3: play_wav_oneshot(&audio, &snd_crash[2]); break;
            case XT_CRASH_LOWCRASH1: play_wav_oneshot(&audio, &snd_lowcrash[0]); break;
            case XT_CRASH_LOWCRASH2: play_wav_oneshot(&audio, &snd_lowcrash[1]); break;
            case XT_CRASH_LOWCRASH3: play_wav_oneshot(&audio, &snd_lowcrash[2]); break;
            case XT_CRASH_TIRES: play_wav_oneshot(&audio, &snd_tires); break;
            case XT_CRASH_NONE: default: break;
          }
          switch (xt.pending_skid) {
            case XT_SKID_SKID1: play_wav_oneshot(&audio, &snd_skid[0]); break;
            case XT_SKID_SKID2: play_wav_oneshot(&audio, &snd_skid[1]); break;
            case XT_SKID_SKID3: play_wav_oneshot(&audio, &snd_skid[2]); break;
            case XT_SKID_DUSTSKID1: play_wav_oneshot(&audio, &snd_dustskid[0]); break;
            case XT_SKID_DUSTSKID2: play_wav_oneshot(&audio, &snd_dustskid[1]); break;
            case XT_SKID_DUSTSKID3: play_wav_oneshot(&audio, &snd_dustskid[2]); break;
            case XT_SKID_NONE: default: break;
          }
          switch (xt.pending_scrape) {
            case XT_SCRAPE_SCRAPE1: play_wav_oneshot(&audio, &snd_scrape[0]); break;
            case XT_SCRAPE_SCRAPE2: play_wav_oneshot(&audio, &snd_scrape[1]); break;
            case XT_SCRAPE_SCRAPE3: play_wav_oneshot(&audio, &snd_scrape[2]); break;
            case XT_SCRAPE_NONE: default: break;
          }
          if (xt.pending_gscrape) play_wav_oneshot(&audio, &snd_scrape[2]); // scrape3/"scrape3b" both play scrape3.wav, see xt_graphics.c
          if (xt.pending_firewasted) play_wav_oneshot(&audio, &snd_firewasted);

          // Engine reconciliation -- xt.pengs[j] says which of the
          // current bank's 5 revs SHOULD be looping; force a full stop
          // first if the bank itself changed (xt.lcn), matching
          // sparkeng()'s own doc comment on why that's not optional.
          if (xt.lcn != last_engine_bank) {
            for (int32_t j = 0; j < 5; j++) {
              if (engine_channel[j] != -1) { audio_stop(&audio, engine_channel[j]); engine_channel[j] = -1; }
            }
            last_engine_bank = xt.lcn;
          }
          {
            int32_t bank = mad[0].cd->enginsignature[xt.lcn];
            for (int32_t j = 0; j < 5; j++) {
              if (xt.pengs[j] && engine_channel[j] == -1) {
                WavClip *clip = &snd_engine[bank][j];
                if (clip->samples) {
                  engine_channel[j] = audio_play(&audio, clip->samples, clip->frame_count, clip->sample_rate, 1.0f, true);
                }
              } else if (!xt.pengs[j] && engine_channel[j] != -1) {
                audio_stop(&audio, engine_channel[j]);
                engine_channel[j] = -1;
              }
            }
          }

          // Air (whoosh) reconciliation -- at most one slot is ever
          // meant to be audible at once, see xt_graphics.h's own doc
          // comment on air_stop_all/air_start_slot.
          if (xt.air_stop_all && air_channel != -1) {
            audio_stop(&audio, air_channel);
            air_channel = -1;
          }
          if (xt.air_start_slot >= 0) {
            WavClip *clip = &snd_air[xt.air_start_slot];
            if (clip->samples) {
              if (air_channel != -1) { audio_stop(&audio, air_channel); air_channel = -1; }
              air_channel = audio_play(&audio, clip->samples, clip->frame_count, clip->sample_rate, 1.0f, true);
            }
          }

          // "About to be wasted" loop reconciliation -- xt.pwastd IS the
          // desired end state (see xt_graphics.h), so this is a direct
          // level check, no edge-detection needed.
          if (xt.pwastd && wasted_channel == -1) {
            if (snd_wasted.samples) {
              wasted_channel = audio_play(&audio, snd_wasted.samples, snd_wasted.frame_count, snd_wasted.sample_rate, 1.0f, true);
            }
          } else if (!xt.pwastd && wasted_channel != -1) {
            audio_stop(&audio, wasted_channel);
            wasted_channel = -1;
          }

          // Music mute edge -- xt.mutem synced from control[0].mutem
          // inside xt_graphics_stub_playsounds() (dormant until a key
          // binds control->mutem, see that function's own doc comment).
          if (xt.mutem != last_mutem) {
            // :9228-9237 -- strack.stop() / strack.resume(): the track
            // picks up where it was, it does not restart.
            audio_set_music_muted(&audio, xt.mutem);
            last_mutem = xt.mutem;
          }

          // GameSparker.java:954-956 -- bots react to what checkstat just
          // computed, setting up their input for the NEXT tick's drive().
          // Skips index 0 (the human).
          DIAG_PHASE("race tick: AI (aux: car, point, clear, laps, route points, gates)");
          if (ext_career) {
            for (int32_t k = 0; k < nplayers; k++) {
              xt.career.endsp[k] = crace.sp[k][CS_END];   // xtGraphics 9018
              xt.career.botbreak[k] = cbots.brk[k];
              xt.career.undead[k] = cstage.undead[k];
              xt.career.beast[k] = xt.career.beastopponent[k] = crace.beast[k];
            }
          }
          // GameSparker.js 2396-2401: on stage 13 a car braking into a portal
          // or just out of one takes no input and no AI.
          bool cstage_noai[NFM_MAX_CARS] = {false};
          if (ext_career && stage_num == 13) {
            for (int32_t k = 0; k < nplayers; k++) {
              if (!cstage.forcehandb[k] && cstage.speedhack[k] <= 0) continue;
              cstage_noai[k] = true;
              if (k == 0) continue;
              control[k].down = control[k].left = control[k].right = control[k].handb = false;
            }
          }
          for (int32_t i = bench.active ? 0 : 1; i < nplayers; i++) {
            if (ext_career && mad[i].isabot) continue;   // a recorded bot drives it
            if (cstage_noai[i]) continue;
            g_diag.aux[0] = i;
            g_diag.aux[1] = mad[i].point;
            g_diag.aux[2] = mad[i].clear;
            g_diag.aux[3] = mad[i].nlaps;
            g_diag.aux[4] = cp.n;
            g_diag.aux[5] = cp.nsp;
            control_preform(&control[i], &mad[i], &co[i], &cp, &t);
          }
          if (ext_career) {
            // Contva.sortvariables after the AI (GameSparker.js 2390-2417).
            for (int32_t k = 0; k < nplayers; k++) {
              if (mad[k].isabot || cstage_noai[k]) continue;
              contva_sortvariables(&xt.career.contva, &mad[k], &cp, control, true, nplayers, &xt.career);
            }
          }
          if (ext_career && starcnt == 0) {
            // GameSparker.java 2228-2356: a bot breaks off for good when it
            // was hit, races the wrong car, or sits far ahead of a player
            // close by who could take it; until then it replays its keys.
            const bool hard = csave.unlocked == stage_num || crace.hardstage;
            const bool cantbot = crace.scalelevels || crace.nolevels;
            const int32_t st = stage_num;
            for (int32_t s = 1; s < nplayers; s++) {
              if (!cbots.keys[s]) continue;
              if (mad[s].bot_hit) cbots.brk[s] = true;
              const int32_t dx = co[s].x / 100 - co[0].x / 100, dz = co[s].z / 100 - co[0].z / 100;
              const int32_t dist = dx * dx + dz * dz;
              const int32_t lead = cp.clear[s] - cp.clear[0];
              const int32_t car = crace.sc[s];
              const float mine = live_cd[0].moment[sc[0]], its = live_cd[s].moment[sc[s]];
              bool brk = cantbot || mad[s].frozen;
              if (st == 5 || st == 9 || st == 10 || st == 14) {
                int32_t whichcar = st == 14 ? 14 : (st == 5 ? 10 : 12), threshold = 2;
                if (st == 9) {
                  if (cp.clear[0] >= 2 && cp.clear[0] <= 4) threshold = 4;
                  if (cp.clear[0] >= 9 && cp.clear[0] <= 15) threshold = 7;
                  if (cp.clear[0] >= 15 && cp.clear[0] <= 19) threshold = 5;
                }
                brk = brk || (dist < 10000 && lead >= threshold) || car != whichcar;
              }
              if (st == 13) {
                const int32_t rightcar = s == nplayers - 1 ? 14 : car;
                brk = brk || (dist < 8000 && lead >= 5 && mine > its + 1.0f) || car != rightcar || !hard;
              }
              if (st == 18) brk = brk || (dist < 6500 && lead >= 4 && mine > its) || car != 16;
              if ((st == 11 || st == 21) && hard)
                brk = brk || (dist < 10000 && lead >= 4 && mine > its + 1.0f) || car != (st == 21 ? 17 : 12);
              // This port's own guard: its physics are not Extended's bit for
              // bit, so a recording can drift off its line; a bot that goes
              // 600 ticks without a checkpoint hands over to the AI.
              if (cp.clear[s] != cbots_last_clear[s]) {
                cbots_last_clear[s] = cp.clear[s];
                cbots_since[s] = 0;
              } else if (++cbots_since[s] > 600) {
                brk = true;
              }
              if ((brk || mad[s].bot_hit) && !cbots.brk[s])
                fprintf(stderr, "career bot %d breaks off at tick %d (hit %d, lead %d)\n", (int)s, (int)cbots.timer,
                        (int)mad[s].bot_hit, (int)lead);
              if (brk) cbots.brk[s] = true;
              mad[s].isabot = !cbots.brk[s];
            }
            if (st == 5 || st == 9 || st == 10 || st == 13 || st == 14 || st == 18 || st == 20 ||
                ((st == 11 || st == 21) && hard)) {
              cbots.timer++;
              for (int32_t s = 0; s < nplayers; s++) cbots.specialtimer[s]++;
            }
            const bool special = st == 13 && hard;
            for (int32_t s = 1; s < nplayers; s++) {
              if (!cbots.keys[s] || cbots.brk[s]) continue;
              const uint8_t k = career_bots_keys(&cbots, s, special);
              control[s].up = (k & BOT_UP) != 0;
              control[s].down = (k & BOT_DOWN) != 0;
              control[s].left = (k & BOT_LEFT) != 0;
              control[s].right = (k & BOT_RIGHT) != 0;
              control[s].handb = (k & BOT_HANDB) != 0;
            }
          }
          // Extended's specials: charged bars fire, boosts and attacks
          // rewrite each car's live stats (xtGraphics.nitroandspecials).
          if (xt.extended) {
            if (ext_career && career_stage_has_stat_effects(&cstage)) {
              // XT 6925-7051: stage 9's fire, 11's drain, 20's and bonus 4's speed cut.
              for (int32_t i = 0; i < nplayers; i++)
                career_stage_stats(&cstage, &crace, i, &race_base[i], &cd, sc[i], live_cd[i].grip[sc[i]], &cp);
            }
            if (race_specials)   // NFM 1 / 2, and Race Setup's Specials Off, race without them
              specials_tick(&specials, mad, control, nplayers, &cp, &cd, race_base);
            if (ext_career) {
              // Experience (career.c): checkpoints, wastes, stunts, full
              // power, level-ups -- stat$m and careermode$m's bookkeeping.
              if (crace.bonus == 1 || crace.bonus == 3) mad[0].nlaps = 0;   // unlimitedlaps (XT 4668)
              if (mad[0].clear != career_seen_clear && mad[0].clear != 0 && !race_holdit &&
                  crace.bonus != 1 && crace.bonus != 3) {
                career_seen_clear = mad[0].clear;
                career_xp_checkpoint(&crun, &crace, &csave, cp.clear[0]);
                // RECOVERY (this port's): up to 4% of the car's health back.
                const int32_t rv = career_perk_points(&crun.perks, PERK_RECOVERY);
                if (rv > 0) {
                  const int32_t heal = (int32_t)((float)mad[0].cd->maxmag[mad[0].cn] * 0.04f * (float)rv / 20.0f);
                  mad[0].hitmag = mad[0].hitmag > heal ? mad[0].hitmag - heal : 0;
                }
              }
              for (int32_t k = 1; k < nplayers; k++) {
                if (cp.dested[k] == 2 && career_seen_dested[k] != 2) career_xp_waste(&crun, &crace, &csave, k);
                career_seen_dested[k] = cp.dested[k];
              }
              if (mad[0].stunt_gain > 0.0f) {
                career_xp_stunt(&crun, &crace, &csave, mad[0].stunt_gain,
                                career_stunt_stat(&cd, sc[0], csave.sp[crace.sc[0]]));
              }
              const bool full = mad[0].power == 98.0f && starcnt == 0 && !mad[0].dest && !race_holdit &&
                                fabsf(mad[0].speed) > 0.0f;
              career_tick(&crun, &crace, &csave, starcnt == 0, full);
              career_popups_tick(&crun, &crace, &csave);   // the bonus stat points pay as they show
              // (The undead, careermode$m's, are career_stage_tick's below.)
              crace.level[0] = csave.level[crace.sc[0]];
            }
            // The career reads the stunts once more (the Titan's damage,
            // career_stage_tick below) and clears them there.
            if (!ext_career) for (int32_t k = 0; k < nplayers; k++) mad[k].stunt_gain = 0.0f;
            if (PT_ACTIVE) {
              // tourney() (ext_pt.c): wasting matches revive cars and score
              // kills, the racing ones hold damage at 0, eliminated cars go.
              ExtPTCar ptc[EXT_PT_CARS];
              memset(ptc, 0, sizeof(ptc));
              for (int32_t i = 0; i < EXT_PT_CARS && i < nplayers; i++) {
                ptc[i].dest = mad[i].dest;
                ptc[i].hitmag = mad[i].hitmag;
                ptc[i].killer = mad[i].lastcolider;
              }
              ext_pt_tick(&pt, ptc, cp.pos, cp.clear, cp.nlaps * cp.nsp, cp.wasted, starcnt == 0);
              for (int32_t i = 0; i < EXT_PT_CARS && i < nplayers; i++) {
                if (ptc[i].revive_now) {
                  mad[i].hitmag = 0;
                  mad[i].cntdest = 0;
                  mad[i].squash = 0;
                  mad[i].dest = false;
                  mad[i].newcar = true;
                  cp.dested[i] = 0;
                }
                if (ptc[i].force_wreck) mad[i].hitmag = 100000;
                if (ptc[i].no_damage) { mad[i].hitmag = 0; mad[i].squash = 0; }
                if (ptc[i].no_special) mad[i].spatk = 0.0f;
              }
            }
            for (int32_t i = 0; i < nplayers; i++) {
              co[i].spec_on = specials.spec_on[i];
              memcpy(co[i].spec, specials.spec[i], sizeof(co[i].spec));
            }
          }

          // Checkpoint/Car-Fixed sound triggers -- Java xtGraphics.java:8397
          // (checkpoint.play()) and :9445's carfixed.checkopen() (armed by
          // the fcnt==7||8 transition mad_drive() already ported into
          // mad->just_fixed -- see mad.h's own doc comment on that field).
          // Player-only (xtGraphics.playsounds() is always called with
          // array3[xtGraphics.im], GameSparker.java:1705 -- bots never
          // trigger audio for the local listener). Checkpoint uses the
          // exact same edge condition as the "Checkpoint!" text trigger in
          // xt_graphics.c's hud_say_draw (`hud_clear != clear && clear !=
          // 0`), tracked independently here so it fires once per TICK
          // (physics-authoritative) rather than once per drawn frame.
          if (mad[0].clear != audio_last_clear && mad[0].clear != 0) {
            if (snd_checkpoint.samples) audio_play(&audio, snd_checkpoint.samples, snd_checkpoint.frame_count, snd_checkpoint.sample_rate, 1.0f, false);
            audio_last_clear = mad[0].clear;
          }
          if (mad[0].just_fixed && snd_carfixed.samples) {
            audio_play(&audio, snd_carfixed.samples, snd_carfixed.frame_count, snd_carfixed.sample_rate, 1.0f, false);
          }

        }
        if (race_holdit && state == STATE_RACING) {
          // Java stat():7566-7570 -- holdcnt increments every tick; ENTER
          // or holdcnt > 250 (~13.2s at our 18.9 ticks/sec) advances past
          // the hold card. Java then runs the instant-replay (fase -3)
          // and Madness-logo flash (fase -4) before finish() (fase -5) --
          // both are Record.js-dependent and not ported (see record.h's
          // own scope note), so we go straight to STATE_POST_RACE once
          // the hold finishes, same as every other now-unreachable fase
          // in between.
          // control.enter is never populated by input_poll (see
          // platform/<name>/input.c -- only left/right/up/down/handb are
          // wired), so this reads the raw logical-button edge directly,
          // same convention every other menu-nav key in this file
          // already uses.
          race_holdcnt++;
          if ((race_confirm_latch || race_holdcnt > 250) && ext_career && cstage.bossbattle) {
            // stat$m 3935-3947: the card gives way to the fight
            // (career_stage_tick releases the hold this same tick).
            career_stage_hold_advance(&cstage, &cworld);
            race_confirm_latch = false;
          } else if (race_confirm_latch || race_holdcnt > 250) {
            stop_all_sfx_loops(&audio, engine_channel, &last_engine_bank, &air_channel, &wasted_channel);
            game_progress_finish_stage(&progress, (GameMode)gmode, stage_num, race_winner);
            if (progress_path_ok) game_progress_save_to_disk(&progress, progress_path);
            // Extended's finish() (XT 12648-12672): winning the newest
            // normal-mode stage opens the next.
            ext_justwon = false;
            for (int32_t k = 0; k < 4; k++) career_new_cars[k] = -1;
            bool was_locked[CAREER_CARS];
            for (int32_t e = 0; e < CAREER_CARS; e++) was_locked[e] = ext_career && career_car_lock(&csave, e) != 0;
            if (ext_career && race_winner) {
              // finish() (XT 12499-12510): winning the newest stage opens
              // the next, up to 31.
              if (crace.bonus) {
                // The bonus stages' prizes (XT 12511-12532): bonus 2 by
                // racing (1), by wasting (2), both (3).
                const int32_t a = crace.bonus - 1;
                const bool racing = race_end_kind == RACE_END_FINISH, wasting = !racing;
                if (a != 1) {
                  if (csave.boncomp[a] == 0) csave.boncomp[a] = 1;
                } else {
                  if (csave.boncomp[a] == 0) csave.boncomp[a] = racing ? 1 : 2;
                  if ((csave.boncomp[a] == 1 && wasting) || (csave.boncomp[a] == 2 && racing)) csave.boncomp[a] = 3;
                }
              } else if (stage_num == csave.unlocked && csave.unlocked < CAREER_STAGES) {
                csave.unlocked++;
                csave.statchangers[0] = csave.statchangers[1] = 1;
                ext_justwon = true;
              }
              if (career_path[0]) career_save(career_path, &csave);
              int32_t nn = 0;
              for (int32_t e = 0; e < CAREER_CARS && nn < 4; e++)
                if (was_locked[e] && career_car_lock(&csave, e) == 0) career_new_cars[nn++] = e;
            } else if (ext_normal && !ext_career && race_winner && stage_num != EXT_PT_STAGE) {
              const int32_t nu = ext_unlock_after_win(ext_prog.normal_unlocked, stage_num);
              ext_justwon = nu != ext_prog.normal_unlocked;
              ext_prog.normal_unlocked = nu;
              if (progress_path_ok) ext_progress_save(progress_path, &ext_prog);
            }
            // GameSparker.java stat():7565-7570 advances holdit into
            // fase==-2 (xtGraphics.java:3179/7568/7602), whose own body
            // (:1347-1386) decides between the fase==-3 highlight reel and
            // skipping straight to fase==-4. Single-player-only here, so
            // the `multion>=2` reset never applies; `u[0].falseo(3)`
            // becomes control_falseo(&control[0], 3) to match.
            control_falseo(&control[0], 3);
            if (rpd.hcaught && rpd.wasted == 0 && rpd.whenwasted != 229 &&
                (cp.stage == 1 || cp.stage == 2) && xt.looped != 0) {
              rpd.hcaught = false;
            }
            if (rpd.hcaught) {
              // :1370-1377 -- randomizes the camera's starting orbit state
              // before the reel's first frame (replay_tick==0 branch below
              // re-derives medium.adv/vxz/vert the SAME way :1390-1409
              // does, but this seeds the pre-around() state exactly once,
              // matching the Java's own two-step setup).
              m.vert = medium_random(&m) <= 0.45f;
              m.adv = jtrunc(900.0f * medium_random(&m));
              m.vxz = jtrunc(360.0f * medium_random(&m));
              replay_tick = 0;
              replay_cam = 0;
              replay_loops = 0;
              state = STATE_REPLAY;
            } else {
              // :1383-1386 -- fase=-4 (Madness-logo pixel-melt transition)
              // then fase=-5 (finish()). The logo flash is purely
              // decorative (finish()/STATE_POST_RACE already shows the
              // real win/lose card); the ACTUAL gameplay-critical bit fase
              // -4's first frame does -- xtGraphics.sendwin()'s
              // unlocked[]++ stage-unlock -- is already handled above by
              // game_progress_finish_stage(), so skipping straight to
              // STATE_POST_RACE here loses no functionality, matching
              // this port's established pattern of skipping purely-
              // cosmetic transitions (see the pre-race flyby/menu-blur
              // doc comments for the same call elsewhere in this file).
              state = STATE_POST_RACE;
            }
          }
        }
        // End-of-race detection -- Java xtGraphics.java:7683-7803 (stat()'s
        // own win/lose scan, called once per rendered frame in Java's
        // unified render+simulate loop, hence once per TICK here -- see
        // the hold-card advance rule's own comment just above for why
        // that correspondence holds). Checked in the SAME priority order
        // as the source's three successive `!this.holdit` guards (see
        // race_end_kind's own doc comment above): all-other-cars-wasted,
        // then player-wasted, then the normal finish-line scan.
        //
        // Deliberately sits AFTER the advance block above and OUTSIDE the
        // `if (starcnt == 0)` physics gate, mirroring stat()'s own shape:
        // Java advances holdcnt at the top of stat() (:7554-7570) and only
        // then, under `if (this.fase != -2)` (:7605), runs this scan --
        // neither half gated on starcnt. Running the scan first (as this
        // did before) started holdcnt at 1 on the very tick the card
        // appears instead of 0, and let a CONFIRM edge on that same tick
        // skip a card the player had not yet seen a single frame of.
        if (!race_holdit && state == STATE_RACING && PT_ACTIVE) {
          if (pt.over) {
            // "MATCH OVER / X WON!" (tourney()); the points come next.
            race_winner = pt.winner == 0;
            race_end_kind = RACE_END_FINISH;
            race_holdit = true;
            race_holdcnt = 0;
            cp.haltall = true;
            if (!race_winner && pt.winner > 0)
              snprintf(race_lost_car_name, sizeof(race_lost_car_name), "%s", kExtPTOpponents[pt.winner]);
          }
        } else if (!race_holdit && state == STATE_RACING) {
          // stat$m 3992-4004: the career's undead are not there to waste,
          // and during the Titan's fight nothing ends the race.
          const int32_t undeadextra = ext_career ? career_stage_undeadextra(&cstage) : 0;
          // Race Setup's Win By: wasting only (no finish, endless laps) or
          // racing only (wasting everyone does not end it).
          const bool wasted_end = (!ext_career || career_stage_wasted_end_allowed(&cstage)) && !(fp_race && settings.fp_win == 2);
          const bool finish_end = (!ext_career || career_stage_finish_end_allowed(&cstage)) && !(fp_race && settings.fp_win == 1);
          if (fp_race && settings.fp_win == 1) mad[0].nlaps = 0;
          if (cp.wasted == nplayers - 1 - undeadextra && nplayers != 1 && wasted_end) {
            // :7683 -- every other car destroyed. Single-player-only
            // port, so this.multion<2 and !b2 (clan mode) always hold --
            // matches the "You Won, all cars have been wasted!" branch.
            race_winner = true;
            race_end_kind = RACE_END_ALL_WASTED;
            race_holdit = true;
            race_holdcnt = 0;
            cp.haltall = true; // :1077 -- freezes all cars (mad.c:1098), matching the finish-line ending
            if (ext_career) career_stage_win_is_boss(&cstage);   // stat$m 4020: the Titan wakes
          } else if (mad[0].dest && xt.cntwis == 8) {
            // :7708 -- the local player's own car destroyed and its
            // "about to be wasted" ramp (xt.cntwis, Part 16) has fully
            // played out. discon/exitm/lan branches are multiplayer-only
            // (discon never becomes 240 here) -- skipped, matching how
            // this port already treats every other netplay-only path.
            // Deliberately does NOT set cp.haltall (JS :1081-1116 never
            // does either, unlike its two sibling endings above/below) --
            // other cars keep racing/finishing around the wrecked player
            // during this hold card, matching the original exactly.
            race_winner = false;
            race_end_kind = RACE_END_PLAYER_WASTED;
            race_holdit = true;
            race_holdcnt = 0;
            if (ext_career) career_stage_player_wasted(&cstage);   // stat$m 4123
          } else if (finish_end) {
            // :7737 -- normal finish-line scan: whoever JUST finished
            // (cleared every checkpoint of every required lap AND
            // currently in 1st). Player (index 0) -> youwon.gif; a bot
            // -> youlost.gif naming which car beat us (:7757).
            for (int32_t i = 0; i < nplayers; i++) {
              if (cp.clear[i] == cp.nlaps * cp.nsp && cp.pos[i] == 0) {
                race_winner = (i == 0);
                race_end_kind = RACE_END_FINISH;
                race_holdit = true;
                race_holdcnt = 0;
                cp.haltall = true; // :1189 -- freezes all cars' throttle to a coast-down (mad.c:1098)
                if (ext_career && race_winner) career_stage_win_is_boss(&cstage);   // stat$m 4142-4146
                if (!race_winner) {
                  snprintf(race_lost_car_name, sizeof(race_lost_car_name), "%s", car_name(sc[i]));
                }
                break;
              }
            }
          }
        }
        if (ext_career && race_holdit && !career_settled && !cstage.bossbattle && state == STATE_RACING) {
          // stat$m: a win pays (by racing or by wasting), a loss on the
          // newest stage takes the race's experience back. Saved now.
          if (race_winner) career_xp_win(&crun, &crace, &csave, cp.nsp, cp.nlaps, race_end_kind == RACE_END_FINISH);
          else career_xp_lose(&crun, &crace, &csave);
          career_settled = true;
          if (career_path[0]) career_save(career_path, &csave);
        }
        if (ext_career && state == STATE_RACING && starcnt < 38) {
          // careermode$m (GameSparker.js 2485): every frame from the
          // countdown's last 37 ticks on, after the physics, the specials
          // and stat$m (career_stage.c).
          cworld.starcnt = starcnt;
          cworld.holdcnt = race_holdcnt;
          cworld.winner = race_holdit && race_winner;
          cworld.mutes = control[0].mutes;
          career_stage_tick(&cstage, &crace, &csave, &crun, &cworld);
          career_stage_export_ai(&cstage, &cworld, &xt.career);
          for (int32_t k = 0; k < NFM_MAX_CARS; k++) {
            cstage_arrow_skip[k] = k < nplayers && (cstage.noarrow[k] || cstage.norender[k]);
            cstage_board_skip[k] = k < nplayers && cstage.noarrow[k];
          }
          career_medium_tick(&cstage_medium, &m, &cstage);
          if (cstage.release_hold) {
            // cstimer 2: the hold card goes, the field is let go, the fight is on.
            race_holdit = false;
            race_holdcnt = 0;
            race_winner = false;
            cp.haltall = false;
          }
          // (hold_by_boss -- the player wasted at the fix stage -- is left to
          // the player-wasted ending above, which ends the fight.)
          if (cstage.unlimitedlaps) mad[0].nlaps = 0;
          if (cstage.music_event == CAREER_MUSIC_STOP) audio_stop_music(&audio);
          if (cstage.music_event == CAREER_MUSIC_BOSS) {
            // The fight's music (XT 2680-2716): bossbattlea.ogg, then b looping.
            career_ogg_free();
            for (int32_t q = 0; q < 2; q++) {
              char path[64];
              snprintf(path, sizeof(path), "ext/data/Files/careermusic/bossbattle%c.ogg", q ? 'b' : 'a');
              g_career_ogg[q] = vfs_read_bytes(path, &g_career_ogg_len[q]);
            }
            cstage_boss_music = true;
            if (g_career_ogg[1])
              audio_start_ogg(&audio, g_career_ogg[0], g_career_ogg_len[0], g_career_ogg[1], g_career_ogg_len[1]);
          }
          if (cstage.sound_redflash) play_wav_oneshot(&audio, &snd_redflash);
          if (cstage.sound_scare) play_wav_oneshot(&audio, &snd_caught);
        }
        if (ext_career) for (int32_t k = 0; k < nplayers; k++) mad[k].stunt_gain = 0.0f;
        // The camera -- once per tick, after the cars move, as in the Java's
        // loop. (It ran once per display frame, which ran the fly-by orbit,
        // the orbit view and the camera's eases ~3x fast at 60Hz.)
        // GameSparker.java:958-1017 -- while starcnt >= 38 (from the moment
        // racing starts at starcnt==130 down through the whole pre-
        // countdown window), the camera runs medium.around() -- a fast
        // cinematic orbit around the player's own car sitting on the
        // starting grid -- instead of the normal chase cam. This is the
        // "preview of the cars before the countdown" the real Java shows;
        // `n47` in the Java picks car index 3 in multiplayer or 0 in
        // single-player -- always 0 here (single-player-only port), see
        // medium_around()'s own doc comment. ENTER/handbrake (Java:
        // `u[0].enter || u[0].handb`; KEY_EDGE(BTN_CONFIRM) substitutes for
        // `enter` the same way race_holdit's advance-check above does,
        // since control.enter is never populated by input_poll) skips
        // straight to the starcnt==38 transition below.
        // Extended's viewboost (GameSparker.java 2566-2584): the chase and far
        // cameras sit higher and further back for its biggest cars -- Titan,
        // Agent Waster and Tesco Lorry (its 18, 20 and 22; a new car built on
        // one of them gets it too).
        int32_t viewboost = 0;
        if (xt.extended) {
          const int32_t ve = ext_car_of(car_identity(mad[0].cn));
          viewboost = ve == 18 ? 65 : ve == 20 ? 130 : ve == 22 ? 300 : 0;
        }
        if (starcnt >= 38) {
          medium_around(&m, &co[0], true);
          mvect = 80; // :976
          if (!flyby_skip_armed && !control[0].handb && !race_confirm_latch) flyby_skip_armed = true;
          if (flyby_skip_armed && (control[0].handb || race_confirm_latch)) {
            starcnt = 38;
            control[0].handb = false;
          }
          if (starcnt == 38) {
            // :984-992 -- one-time reset back to the normal chase cam.
            // Both medium_around() above and medium_follow() here run on
            // this same tick, matching the Java exactly -- follow() simply
            // overwrites around()'s camera position/heading right after.
            m.vert = false;
            m.adv = 900;
            m.vxz = 180;
            check_points_checkstat(&cp, mad_ptrs, co_ptrs, &rpd, nplayers, 0, 0);
            medium_follow_boost(&m, co[0].x, co[0].y, co[0].z, mad[0].cxz, 0, viewboost);
          }
        } else if (race_view == 1) {
          // :987-991 -- orbit camera. Fixed mvect, no shaka and no lmxz
          // bookkeeping (Java's view==1 branch genuinely has neither).
          medium_around(&m, &co[0], false);
          mvect = 80;
        } else if (race_view == 3) {
          // Extended's far camera (its view 1, GameSparker.java:2595): high
          // and far behind, turning a fifteenth as much as the car.
          medium_watch_far(&m, &co[0], (double)mad[0].cxz / 15.0, viewboost);
          mvect = 80;
        } else if (race_view == 2) {
          // :992-999 -- the fixed "watch" tripod, aimed with the car's raw
          // movement heading (`mad.mxz`, not the smoothed `cxz` the chase
          // cam uses). Shares view 0's mvect/lmxz smoothing but, like
          // view 1, does no shaka.
          medium_watch(&m, &co[0], mad[0].mxz);
          mvect = 65 + (abs(lmxz - m.xz) / 5) * 100;
          if (mvect > 90) mvect = 90;
          lmxz = m.xz;
        } else {
          // JS: `medium.follow(array2[im], array3[im].cxz, this.u[im].lookback)`
          // -- the chase camera tracks `mad.cxz` (a SMOOTHED heading Mad.drive()
          // eases toward the car's actual movement direction, only adjusting
          // significantly above ~30 speed -- see mad_drive()'s own cxz-smoothing
          // block), not `contO.xz` (the car's own instantaneous heading) or
          // `mad.mxz` (contO's raw movement-direction reading). Passing contO.xz
          // here instead made the camera snap to match the car's heading every
          // tick with no lag at all, so it never showed the car's side turning
          // -- always dead-center behind it. `control.lookback` is the
          // "look back over your shoulder" hold, now actually driven by the
          // Z/X keys (shoulder triggers on Vita) -- see input.c.
          medium_follow_boost(&m, co[0].x, co[0].y, co[0].z, mad[0].cxz, control[0].lookback, viewboost);

          // GameSparker.java:973-983 -- runs in the exact same spot as
          // medium.follow() above (both are inside the real Java's `view==0`
          // branch, once per tick). Arms `shaka` from THIS tick's damage
          // (outshakedam is check_points_checkstat's own copy-then-zero of
          // mad->shakedam, already ported); `mvect` measures how far the
          // camera's heading (m.xz) moved since last tick, matching Java's
          // plain-int arithmetic exactly (no fr()/trunc() -- xz is already
          // an int, so is the result).
          if (mad[0].outshakedam > 0) {
            shaka = mad[0].outshakedam / 20;
            if (shaka > 25) shaka = 25;
            // Settings > Vibration: the same crash felt in the controller, as
            // strong and as long as the shake it arms (a no-op where the
            // platform has no vibration, platform_rumble()).
            if (settings.rumble) platform_rumble(0.25f + 0.75f * (float)shaka / 25.0f, 120u + 12u * (uint32_t)shaka);
          }
          mvect = 65 + (abs(lmxz - m.xz) / 5) * 100;
          if (mvect > 90) mvect = 90;
          lmxz = m.xz;
        }
        race_confirm_latch = false;
        accumulator_ms -= TICK_MS;
      }
      if (ticked_this_frame) {
        smooth_capture(&smooth_curr, co, nplayers, &m);
        smooth_captured = true;
      }
      // Mark this frame for the draw-phase effect guards -- see
      // ticked_this_frame's own comment above the loop. Set here rather
      // than at the draw site so it lands before ANY draw for this frame,
      // including medium_d()'s own per-pass PRNG bookkeeping, which is the
      // first thing the scene draw does.
      m.interpolating = !ticked_this_frame;
      race_ticked = ticked_this_frame;

      // In-race toggles -- GameSparker.java's keyDown() (:3626-3639) plus
      // Control's own mutem/mutes. All three are edge-triggered here
      // because the original handles them on key-DOWN only; holding the
      // key must not re-fire. `mutes` already had a consumer
      // (xt_graphics.c gates its SFX decisions on it); `mutem` gets
      // applied to the music stream just below, and the view cycle feeds
      // the camera dispatch further down.
      {
        // :3635-3638; Extended's far camera is 3. Settings > Gameplay can
        // take any but the chase camera (0) out of the cycle.
        const bool cam_on[4] = {true, settings.cam_orbit != 0, settings.cam_watch != 0,
                                xt.extended && settings.cam_far != 0};
        if (KEY_EDGE(BTN_VIEW)) do race_view = (race_view + 1) % 4; while (!cam_on[race_view]);
        if (!cam_on[race_view]) race_view = 0;
      }
      if (KEY_EDGE(BTN_MUTE_MUSIC)) {
        control[0].mutem = !control[0].mutem;
        audio_set_music_muted(&audio, control[0].mutem);
      }
      if (KEY_EDGE(BTN_MUTE_SFX)) {
        control[0].mutes = !control[0].mutes;
        xt.mutes = control[0].mutes;
      }
      // A / S -- GameSparker.java:3618-3625 and :3626-3633. Plain toggles
      // on the Control struct, exactly like mutem/mutes above; everything
      // that reacts to them reads control[0] further down (the arrow's
      // car-targeting branch, the arrace leaderboard, and radarstat()).
      if (KEY_EDGE(BTN_ARRACE)) {
        control[0].arrace = !control[0].arrace;
#ifdef NFM_SVCLOG
        fprintf(stderr, "input: arrace -> %d\n", control[0].arrace);
#endif
      }
      // Extended's S: fires the special once its bar is full (mad.c lets go
      // of a press made on a bar that is not).
      if (xt.extended && KEY_EDGE(BTN_SPECIAL)) control[0].spatk = !control[0].spatk;
      // Extended's D: the car list's bars show every car's special.
      if (xt.extended && KEY_EDGE(BTN_LISTBARS)) ext_listbars = !ext_listbars;
      if (KEY_EDGE(BTN_RADAR)) {
        control[0].radar = !control[0].radar;
#ifdef NFM_SVCLOG
        fprintf(stderr, "input: radar -> %d\n", control[0].radar);
#endif
      }

      // Pause -- xtGraphics.java:7579-7585, the `fase == 0` arm of stat()'s
      // own `if (control.enter || control.exit)` in the NOT-holdit branch.
      // Java stops the music track, goes to fase -6, and fase -6's whole
      // body (:1658-1663) is one frame of pauseimage() -- a blur of the
      // frame underneath -- before it sets fase -7. The blur is purely
      // cosmetic, so this jumps straight to STATE_PAUSED, the same
      // shortcut this file already takes for the other cosmetic
      // transitions (see the fase -4 logo flash's own comment).
      // NFM_SCREENSHOT_MENU=paused: the headless hook can't press START,
      // so it pauses the race a few frames before the dump instead.
      bool hook_pause = screenshot_menu &&
                        ((strcmp(screenshot_menu, "paused") == 0 && frame == screenshot_frame - 3) ||
                         (strcmp(screenshot_menu, "pausereplay") == 0 && frame == hook_replay_frame - 3) ||
                         (strcmp(screenshot_menu, "settings") == 0 && frame == screenshot_frame - 6));
      if (state == STATE_RACING && !race_holdit && frame != intro_confirm_frame &&
          (KEY_EDGE(BTN_PAUSE) || hook_pause || focus_pause)) {
        audio_set_music_muted(&audio, true);
        stop_all_sfx_loops(&audio, engine_channel, &last_engine_bank, &air_channel, &wasted_channel);
        pause_opselect = 0;
        pause_snapshot_pending = true;
        state = STATE_PAUSED;
      }

      // F10 manual-abandon hotkey -- this port's OWN convenience, not a
      // real Java trigger (Java has no single-key "give up" during fase
      // 0), so it intentionally skips the hold card and goes straight to
      // STATE_POST_RACE. Checked once per frame (not per tick) like every
      // other menu-nav key in this file.
      if (state == STATE_RACING && !race_holdit && KEY_EDGE(BTN_ABANDON)) {
        race_winner = false;
        state = STATE_POST_RACE;
        stop_all_sfx_loops(&audio, engine_channel, &last_engine_bank, &air_channel, &wasted_channel);
        game_progress_finish_stage(&progress, (GameMode)gmode, stage_num, race_winner);
        if (progress_path_ok) game_progress_save_to_disk(&progress, progress_path);
      }

    }

    // Every draw call below this point targets the offscreen render
    // target (when available), not the window directly -- see the
    // composite blit just after gfx_submit_gl() for where it actually
    // reaches the screen. This inner target DOES get cleared every
    // frame -- it's the freshly-rendered "this frame's scene", matching
    // Java's own `offImage` (a normal, fully-opaque render every time);
    // only the FINAL blit onto the window is the part that isn't
    // cleared first.
    // Where this frame is drawn, decided from the state it STARTS in (a
    // few transitions happen mid-draw; the composite below must agree with
    // what was drawn). An offscreen target is only needed where the frame
    // has to be read back: the motion-blur trail (racing, car select,
    // stage select and its locked overlay) and the pause snapshot (racing
    // and the replays it can be taken from). Every other screen draws
    // straight to the display -- one full-screen pass and one render-
    // target switch fewer per frame on the Vita.
    prof_render_start = platform_ticks_us();
    const GameState render_state = state;
    const bool letterboxed = render_state != STATE_RACING && render_state != STATE_REPLAY &&
                             render_state != STATE_PAUSED && render_state != STATE_PAUSE_REPLAY &&
                             render_state != STATE_CANTREPLY && render_state != STATE_BOOT_CLICK &&
                             render_state != STATE_SETTINGS && render_state != STATE_BENCH_RESULT;
    const bool use_rt = motion_blur_ok &&
                        (render_state == STATE_RACING || render_state == STATE_REPLAY ||
                         render_state == STATE_PAUSE_REPLAY || render_state == STATE_CAR_SELECT ||
                         render_state == STATE_STAGE_SELECT || render_state == STATE_STAGE_LOCKED ||
                         render_state == STATE_BOOT_CLICK || render_state == STATE_BOOT_RAD);

    // fase -6 (GameSparker.java:1658-1663): the target the previous frame
    // was drawn into still holds it, HUD included -- Java's offImage at the
    // moment pauseimage() reads it (here with that frame's trail already
    // folded in; under pauseimage()'s own grey smear the difference is
    // invisible).
    if (pause_snapshot_pending) {
      pause_snapshot_pending = false;
      if (motion_blur_ok) {
        if (!pause_flex_read) pause_flex_read = malloc((size_t)800 * 450 * 4);
        if (!pause_flex_rgba) pause_flex_rgba = malloc((size_t)800 * 450 * 4);
        if (pause_flex_read && pause_flex_rgba) {
          const GfxGlRenderTarget *shot = rt_pair[rt_cur ^ 1];
          gfx_gl_render_target_bind(shot);
          if (shot->px_w == 800 && shot->px_h == 450) {
            glReadPixels(0, 0, 800, 450, GL_RGBA, GL_UNSIGNED_BYTE, pause_flex_read);
          } else {
            // Graphics: HD -- the frame is display-sized; pause_image() works
            // on the original 800x450, so read it whole and bring it down.
            uint8_t *full = malloc((size_t)shot->px_w * (size_t)shot->px_h * 4);
            if (full) {
              glReadPixels(0, 0, shot->px_w, shot->px_h, GL_RGBA, GL_UNSIGNED_BYTE, full);
              downsample_to_game(full, shot->px_w, shot->px_h, pause_flex_read);
              free(full);
            }
          }
          gfx_gl_render_target_bind(NULL);
          pause_image(pause_flex_read, pause_flex_rgba);
          if (pause_flex_tex < 0) pause_flex_tex = gfx_gl_upload_texture(pause_flex_rgba, 800, 450);
          else gfx_gl_update_texture(pause_flex_tex, pause_flex_rgba, 800, 450);
        }
      }
    }

    // Racing redraws only on frames that ran a tick. Nothing on screen
    // moves between ticks -- the cars, the camera (medium_follow) and the
    // HUD all advance per tick, and every draw-driven effect already
    // freezes on such a frame (m.interpolating) -- so a frame without one
    // would rebuild and submit the identical scene. Worse, it fed that
    // identical scene through the trail blend again: Java's paint() blends
    // once per tick, so at 60Hz the trail decayed ~3 blends per tick and
    // was a fraction of the original's, the damage shake ran out 3x early,
    // and the HUD's per-draw timers (say/asay banners via tcnt, flicker,
    // the damage-bar blink) ran 3x fast -- "Checkpoint!" showed for 0.5s
    // instead of 1.6s. Presenting the last finished picture instead fixes
    // all of that and skips two thirds of the race's draw work.
    const bool is_replay = render_state == STATE_REPLAY || render_state == STATE_PAUSE_REPLAY;
    // (The boot prompt and intro too: their counters and blink advance per
    // draw, and the Java loop draws them at the same menu rate.)
    const bool is_paced_menu = render_state == STATE_CAR_SELECT || render_state == STATE_STAGE_SELECT ||
                               render_state == STATE_BOOT_CLICK || render_state == STATE_BOOT_RAD ||
                               render_state == STATE_STAGE_INTRO;
    bool paced_ticked = false;
    if (is_replay || is_paced_menu) {
      const double step_ms = is_replay ? TICK_MS : 40.0;
      uint32_t now_ms = platform_ticks_ms();
      if (paced_state_now != (int32_t)render_state) {
        // Entering the screen: its first frame draws straight away.
        paced_state_now = (int32_t)render_state;
        paced_acc_ms = step_ms;
        replay_confirm_latch = false;
      } else {
        paced_acc_ms += (double)(now_ms - paced_last_ms);
        // Not on the entering frame: the press that chose Replay (or passed
        // the win card) is still an edge there, and would skip the whole
        // replay straight to its last frame.
        if (is_replay && KEY_EDGE(BTN_CONFIRM)) replay_confirm_latch = true;
      }
      paced_last_ms = now_ms;
      // One step per loop iteration at most, like the Java's loop; a long
      // stall must not turn into a burst of catch-up frames.
      if (paced_acc_ms >= step_ms) {
        paced_ticked = true;
        paced_acc_ms -= step_ms;
        if (paced_acc_ms > step_ms) paced_acc_ms = step_ms;
      }
    } else {
      paced_state_now = -1;
    }
    const bool paced_state = render_state == STATE_RACING || is_replay || is_paced_menu;
    const bool advanced = render_state == STATE_RACING ? race_ticked : paced_ticked;
    // Smooth frames: racing draws every display frame instead, the scene
    // blended between the last two ticks (smooth_apply).
    if (!settings.smooth) smooth_ready = false;
    const bool smooth_on = use_rt && render_state == STATE_RACING && smooth_ready;
    const bool smooth_between = smooth_on && !race_ticked; // a frame between two ticks
    const bool reuse_frame = use_rt && paced_state && !advanced && !smooth_on &&
                             picture_state == (int32_t)render_state;
    if (reuse_frame) {
      // Nothing to draw; the composite presents the last picture.
    } else if (use_rt) {
      gfx_gl_render_target_bind(rt_pair[rt_cur]);
      // The target is the logical 800x450 game space in px_w x px_h pixels
      // (800x450 itself unless Graphics is HD); the composite at the end
      // stretches it to the display.
      glViewport(0, 0, rt_pair[rt_cur]->px_w, rt_pair[rt_cur]->px_h);
    } else {
      // Straight to the display: the viewport is the physical screen
      // (960x544 on the Vita), and the projection does the stretching the
      // composite blit would have done -- for a letterboxed menu it maps
      // just the original's 670x400 interior at (65,25) onto the screen.
      int32_t disp_w, disp_h;
      platform_display_size(&disp_w, &disp_h);
      glViewport(0, 0, disp_w, disp_h);
      if (letterboxed) set_game_projection(65.0, 735.0, 425.0, 25.0);
    }
    if (!reuse_frame) glClear(GL_COLOR_BUFFER_BIT);

    gfx_begin(&g);

    if (state == STATE_RACING && !reuse_frame) {
      // Ports GameSparker.js's own draw()'s setDrawPhase(true)/(false)
      // wrapper around its entire #draw body (medium.d + every ContO.d) --
      // its own comment explains why: everything drawn here must consume
      // the DRAW PRNG stream, never the SIM one drive() just ran on, or a
      // particle a puff/spark happens to spawn this frame would shift every
      // later simulation random. cont_o_d's pdust/dsprk calls both draw
      // random numbers (see cont_o.c), so this isn't optional even with a
      // single car and no netplay to desync -- without it those draws
      // silently landed on the sim stream instead, one sim random per
      // dust/spark event.
      if (smooth_on) {
        if (smooth_between) {
          for (int32_t i = 0; i < total_objs; i++) smooth_dist[i] = all_objs[i]->dist;
        }
        const double t = accumulator_ms / TICK_MS;
        smooth_apply(&smooth_prev, &smooth_curr, (float)(t < 1.0 ? t : 1.0), co, nplayers, &m);
      }
      if (ext_career && cstage.started) {
        // careermode$m's pulse of the ground's green outlines (stage 7).
        m.polyoutline_on = stage_num == 7 && cstage.polyoutline > 20;
        m.polyoutline[0] = m.polyoutline[2] = 0;
        m.polyoutline[1] = cstage.polyoutline > 255 ? 255 : (cstage.polyoutline < 0 ? 0 : cstage.polyoutline);
      }
      nfm_set_draw_phase(true);
      DIAG_PHASE("race draw: scene");
      medium_d(&m, &g); // ground/sky backdrop -- must run before any cont_o_d,
                         // which queues into m.nsp (medium_d zeroes it first)

      // ORDERING: painter's algorithm, no depth buffer -- ports GameSparker.js's
      // #draw object-level sort exactly (its own banner comment: "the
      // selection-sort below is O(n^2) and looks eminently improvable -- it
      // also produces a specific tie-breaking order the original depends on,
      // leave it exactly as it is"). Objects whose dist is 0 (either never
      // drawn before, or the visibility test failed) draw immediately,
      // unsorted; everything else sorts by *last frame's* dist, farthest
      // first -- cont_o_d recomputes each object's dist for the NEXT frame
      // as a side effect of drawing it now, so this is deliberately one
      // frame stale, not a bug to "fix" into a fresh two-pass sort.
      // careermode$m's norender (stage 13's other floors, the wasted taken
      // off): not drawn this frame (fade 255), put back after.
      static int32_t cstage_saved_fade[NFM_MAX_CARS];
      int32_t *cstage_piece_fade = NULL;
      if (ext_career && cstage.started) {
        for (int32_t k = 0; k < nplayers; k++) {
          cstage_saved_fade[k] = co[k].fade;
          if (cstage.norender[k]) {
            co[k].fade = 255;
          } else if (cstage.tele_fading[k] && 255 - cstage.telefade[k] > co[k].fade) {
            // Plane.d's teleported: fading out into a stage 13 portal.
            co[k].fade = 255 - cstage.telefade[k];
          }
          co[k].flame_on = cstage.flame_custom[k];
          for (int32_t q = 0; q < 3; q++) co[k].flame_rgb[q] = cstage.flame_rgb[k][q];
        }
        if (cstage.piece_norender && cstage.npieces == stage_count) {
          cstage_piece_fade = malloc(sizeof(int32_t) * (size_t)(stage_count > 0 ? stage_count : 1));
          if (cstage_piece_fade) {
            for (int32_t k = 0; k < stage_count; k++) {
              cstage_piece_fade[k] = stage_objects[k].fade;
              if (cstage.piece_norender[k]) stage_objects[k].fade = 255;
            }
          }
        }
      }
      int32_t nvis = 0;
      for (int32_t i = 0; i < total_objs; i++) {
        if (all_objs[i]->dist != 0) {
          visible_idx[nvis++] = i;
        } else {
          cont_o_d(all_objs[i], &g);
        }
      }
      for (int32_t i = 0; i < nvis; i++) rank[i] = 0;
      for (int32_t i = 0; i < nvis; i++) {
        for (int32_t j = i + 1; j < nvis; j++) {
          if (all_objs[visible_idx[i]]->dist < all_objs[visible_idx[j]]->dist) rank[i]++;
          else rank[j]++;
        }
        order[rank[i]] = i;
      }
      for (int32_t i = 0; i < nvis; i++) {
        cont_o_d(all_objs[visible_idx[order[i]]], &g);
      }
      nfm_set_draw_phase(false);
      if (ext_career && cstage.started) {
        for (int32_t k = 0; k < nplayers; k++) co[k].fade = cstage_saved_fade[k];
        if (cstage_piece_fade) {
          for (int32_t k = 0; k < stage_count; k++) stage_objects[k].fade = cstage_piece_fade[k];
          free(cstage_piece_fade);
        }
      }
      if (smooth_on) {
        // The tick's own values back. A frame between ticks also puts back
        // the dist its tick's draw left (the simulation reads it); a tick
        // frame's draw is the authoritative one, so its dist stays.
        smooth_restore(&smooth_curr, co, nplayers, &m);
        if (smooth_between) {
          for (int32_t i = 0; i < total_objs; i++) all_objs[i]->dist = smooth_dist[i];
        }
      }

      // GameSparker.java:958-963 -- a one-tick solid white flash painted
      // OVER the already-fully-rendered 3D scene above, the exact instant
      // racing starts (matches the Java's own draw order: object loop
      // first, then this fillRect). The HUD below still draws on top of
      // it, same as the real Java (the flash only erases the 3D world,
      // not the panels).
      if (starcnt == 130) {
        gfx_set_color(&g, 255, 255, 255);
        gfx_fill_rect(&g, 0, 0, width, height);
      }

      if (smooth_between) {
        // Between ticks: the HUD exactly as the last tick drew it. Drawing it
        // again would advance its blink and banner timers per display frame.
        gfx_clip_play(&g, &hud_clip);
      } else {
        DIAG_PHASE("race draw: HUD");
        gfx_flush(&g);
        const GfxMark hud_mark = gfx_mark(&g);
        // Real HUD: ports the actual XtGraphics.js draw calls (JS lines
        // ~1389-1401 for the panels/rank badge, ~2324-2339 for the
        // speedometer). Numbers (lap count, wasted count, speed) are plain
        // drawString text in the original too, so they go through
        // core/font.c. No draw-phase wrapper needed below (nothing here
        // calls medium_random()).

        // Checkpoint arrow -- JS line 1315, drawn BEFORE the panel images
        // (matches the original's own order, not just this port's habit of
        // scene-then-HUD). Guarded exactly as xtGraphics.java:7917 guards
        // its own `this.arrow(...)` call one line later (:7918): no arrow
        // and no missed/wrong-way banner during the 3-2-1-GO countdown
        // (starcnt != 0), behind the win/lose hold card (holdit), or on
        // stage 10. `fase != -6` and `multion < 2` are structurally always
        // true in this single-player port, so only these three survive.
        // Both were previously drawn unconditionally -- the arrow kept
        // pointing at the next checkpoint straight through the countdown
        // and stayed on screen overlapping the You Won / You're Wasted
        // card, neither of which the original ever shows.
        // A-key sync -- xtGraphics.java:7892-7916. The latched xt.arrace is
        // diffed against the live control[0].arrace so the announcement
        // fires once per flip. `multion < 2` and the `multion == 1` radar
        // auto-enable are multiplayer-only; nplayers != 1 is real and holds
        // whenever bots are racing.
        if (!NFM_NO_ARROW_STAGE && nplayers != 1 && xt.arrace != control[0].arrace) {
          xt.arrace = control[0].arrace;
          if (xt.arrace) {
            xt.wasay = true;
            snprintf(xt.say, sizeof(xt.say), " Arrow now pointing at >  CARS");
            xt.tcnt = -5;
          } else {
            xt.wasay = false;
            snprintf(xt.say, sizeof(xt.say), " Arrow now pointing at >  TRACK");
            xt.tcnt = -5;
            xt.cntan = 20;
            xt.alocked = -1;
          }
        }
        if (!race_holdit && starcnt == 0 && !NFM_NO_ARROW_STAGE && !(fp_race && !settings.fp_arrow)) {
          draw_checkpoint_arrow(&g, &m, &xt, &cp, mad[0].point, mad[0].missedcp,
                                xt.arrace, nplayers, sc);
          // :7919 -- the missed/wrong-way banner is the `if (!this.arrace)`
          // arm of the arrow call, so hunting cars suppresses it. The `else
          // if (alocked != lalocked)` arm next to it announces a change of
          // manually-locked car; both fields are permanently -1 here (only
          // the unported mouse-hover branch assigns them, see
          // xt_graphics.h), so that arm can never fire and is left out
          // rather than written as unreachable code.
          if (!xt.arrace) {
            hud_wrongway_tick(&g, &m, &xt, &mad[0]);
          }
        }
        // The wider `if (!this.holdit)` of :8009 -- everything from the
        // `looped` reset through the say/asay banners and "Bad Landing!".
        // Unlike the block above this one DOES keep running during the
        // countdown (Java draws "Get Ready!"-era say text there).
        if (!race_holdit) {
          hud_messages_tick(&g, &m, &xt, &mad[0], &cp, nplayers, sc);
        }

        // GameSparker.java:891-903 -- the single-player newcar rebuild.
        // Wipes every damage dent/recolor by rebuilding the ContO fresh from
        // its pristine base model, preserving position and orientation;
        // single-player never uses the multiplayer newedcar/colorCar
        // 10-tick "shiny repaint" variant (fase==7001 only, out of scope,
        // no netplay). Deliberately runs AFTER hud_messages_tick() above,
        // not before: in the real Java, this rebuild sits at the very top
        // of the SAME per-frame block whose OWN tick-equivalent (drive(),
        // :939-956) runs later and is what actually sets mad.newcar true --
        // so the rebuild a given frame executes always consumes the PREVIOUS
        // frame's flag, while stat()'s "Car Fixed" trigger (:958+, this
        // port's hud_messages_tick) sees that SAME frame's fresh flag before
        // it's cleared. This port's tick loop and draw pass are already
        // separate (unlike Java's unified per-frame loop), so matching that
        // exact same-frame ordering here means running the clear AFTER the
        // message check within this one draw pass, not before -- otherwise
        // the message trigger never observes newcar==true at all. This was
        // previously believed unreachable entirely (a stale assumption that
        // this port's CheckPoints has no real fix zones -- wrong:
        // game_sparker.c's stage parser DOES populate cp->fx/fy/fz/roted
        // from a real `fix(` stage command, and mad.c:1793 does set
        // mad->newcar whenever a damaged car reaches one). Without this
        // rebuild, mad->newcar never clears, so hud_messages_tick()'s
        // "Car Fixed" trigger re-forces tcnt=0 EVERY frame forever (message
        // never counts down/disappears) and the car keeps whatever damaged/
        // recoloured planes it had at the moment it was fixed instead of
        // the pristine paint a repaired car should show -- exactly the
        // reported "Car Fixed never goes away, car turns all blue" bug.
        for (int32_t i = 0; i < nplayers; i++) {
          if (mad[i].newcar) {
            int32_t saved_xz = co[i].xz, saved_xy = co[i].xy, saved_zy = co[i].zy;
            cont_o_recopy(&co[i], RACE_MODEL(i, mad[i].cn), co[i].x, co[i].y, co[i].z, 0);
            RACE_LOOK(i);
            co[i].xz = saved_xz;
            co[i].xy = saved_xy;
            co[i].zy = saved_zy;
            mad[i].newcar = false;
          }
        }

        // Dark-sky HUD backing -- xtGraphics.java:7971-7993, drawn directly
        // BEFORE the five panel images below (:7994+) so it sits underneath
        // them. The panel art (dmg/pwr/lap/pos) is mostly transparent with
        // dark line work, and on a night/stormy stage that reads as
        // invisible against the scene behind it, so the original lays down
        // a solid plate first: the stage's own sky colour pushed to a fixed
        // HSB brightness of 0.6, exactly the same recolour drawhi() uses
        // for the win/lose card (:8521-8534, and this file's own hold-card
        // block further down). The two drawLine() pairs flanking each
        // fillRect are a hand-drawn 2px "rounded" bevel -- stepping the
        // plate in by one pixel at each end -- which is why they are
        // individual lines rather than a border rect. All fifteen calls,
        // and every literal coordinate in them, are 1:1 with the source.
        //
        // Ported for completeness, NOT to fix an observed bug: measured
        // across all 32 shipped stages/N.txt, m.darksky comes out false
        // every time (the darkest csky any of them produces after
        // medium_setsnap's tint is stage 32's, at HSB brightness ~0.77,
        // well clear of medium.c:869's `< 0.6f` threshold), so with this
        // asset set the branch never runs. Same reachability status as the
        // hold-card's own `if (m.darksky)` sibling further down, which this
        // port already carried -- both are kept because the condition is
        // data-driven (a hand-written or later stage file could trip it),
        // not structurally impossible.
        if (m.darksky) {
          float hsb_hud[3];
          rgb_to_hsb(m.csky[0], m.csky[1], m.csky[2], hsb_hud);
          hsb_hud[2] = 0.6f;
          int32_t hud_rgb = hsb_to_rgb(hsb_hud[0], hsb_hud[1], hsb_hud[2]);
          gfx_set_color(&g, (hud_rgb >> 16) & 0xff, (hud_rgb >> 8) & 0xff, hud_rgb & 0xff);
          gfx_fill_rect(&g, 602, 9, 54, 14);   // :7977 -- behind dmg
          gfx_draw_line(&g, 601, 10, 601, 21); // :7978
          gfx_draw_line(&g, 600, 12, 600, 19); // :7979
          gfx_fill_rect(&g, 607, 29, 49, 14);  // :7980 -- behind pwr
          gfx_draw_line(&g, 606, 30, 606, 41); // :7981
          gfx_draw_line(&g, 605, 32, 605, 39); // :7982
          if (xt.extended && race_specials) {  // Extended's Special label, under Power
            gfx_fill_rect(&g, 602, 49, 54, 14);
            gfx_draw_line(&g, 601, 50, 601, 61);
            gfx_draw_line(&g, 600, 52, 600, 59);
          }
        }
        // The rest of the boxes: the port reads its counters' ink instead.
        if (m.darksky && kJavaDarkSkyBoxes) {
          gfx_fill_rect(&g, 18, 6, 155, 14);   // :7983 -- behind lap + was
          gfx_draw_line(&g, 17, 7, 17, 18);    // :7984
          gfx_draw_line(&g, 16, 9, 16, 16);    // :7985
          gfx_draw_line(&g, 173, 7, 173, 18);  // :7986
          gfx_draw_line(&g, 174, 9, 174, 16);  // :7987
          gfx_fill_rect(&g, 40, 26, 107, 21);  // :7988 -- behind pos + rank
          gfx_draw_line(&g, 39, 27, 39, 45);   // :7989
          gfx_draw_line(&g, 38, 29, 38, 43);   // :7990
          gfx_draw_line(&g, 147, 27, 147, 45); // :7991
          gfx_draw_line(&g, 148, 29, 148, 43); // :7992
        }

        // The race's font is the one loadingstage() left set (:1978).
        font_set(FONT_BOLD, 12);
        draw_hud_img(&g, hud_images.dmg, 600, 7);
        draw_hud_img(&g, hud_images.pwr, 600, 27);
        if (xt.extended && race_specials) draw_specials_hud(&g, &m, &specials, mad, nplayers, sc, &hud_images.spec);
        draw_hud_img(&g, hud_images.lap, 19, 7);
        hud_set_ink(&g, 0, 0, 100);
        char hud[64];
        if ((ext_career && (crace.bonus == 1 || crace.bonus == 3)) || (fp_race && settings.fp_win == 1))
          snprintf(hud, sizeof(hud), "- / %d", cp.nlaps);
        else if (cp.nlaps > 0) snprintf(hud, sizeof(hud), "%d / %d", mad[0].nlaps + 1, cp.nlaps);
        else snprintf(hud, sizeof(hud), "-");
        // A wider lap label (Spanish VUELTAS:) moves the rest of the row over.
        const int32_t lap_dx = hud_images.lap.w > 27 ? hud_images.lap.w - 27 : 0;
        font_draw(&g, hud, 51 + lap_dx, 18);
        if (ext_career) {
          // careermode$m's experience bar (XT 9164-9196), bottom left:
          // "level N" over a 200-wide bar in 50 cells.
          const int32_t me = crace.sc[0];
          char lv[24];
          snprintf(lv, sizeof(lv), "level %d", (int)csave.level[me]);
          font_set(FONT_BOLD, 10);
          const int32_t y0 = 450 - 24;
          gfx_set_composite(&g, 180.0f / 255.0f);
          gfx_set_color(&g, 0, 0, 0);
          gfx_fill_rect(&g, 7, y0, 211, 22);
          gfx_set_composite(&g, 1.0f);
          gfx_set_color(&g, 255, 255, 255);
          font_draw(&g, lv, 12, y0 + 11);
          int32_t fill = crun.expneeded > 0 ? (int32_t)(200.0f * ((float)csave.exp[me] / (float)crun.expneeded)) : 0;
          if (fill > 200) fill = 200;
          if (fill < 0) fill = 0;
          gfx_set_color(&g, 120, 120, 120);
          gfx_fill_rect(&g, 12, y0 + 14, 200, 4);
          gfx_set_color(&g, 0, 125, 250);
          gfx_fill_rect(&g, 12, y0 + 14, fill, 4);
          gfx_set_color(&g, 70, 70, 70);
          gfx_draw_rect(&g, 12, y0 + 14, 200, 4);
          {
            // This port's addition: the bar's numbers, and "+N XP" for what the
            // last second or so earned (checkpoints, wastes, stunts summed).
            static int32_t xp_prev = -1, xp_shown = 0, xp_timer = 0;
            if (starcnt > 0 || xp_prev < 0) xp_prev = csave.exp[me], xp_shown = 0, xp_timer = 0;
            const int32_t delta = csave.exp[me] - xp_prev;
            xp_prev = csave.exp[me];
            if (delta > 0) {   // a level-up lowers exp: not a gain
              xp_shown = xp_timer > 0 ? xp_shown + delta : delta;
              xp_timer = 90;
            }
            char xs[32];
            snprintf(xs, sizeof(xs), "%d / %d", (int)csave.exp[me], (int)crun.expneeded);
            gfx_set_color(&g, 200, 200, 200);
            font_draw(&g, xs, 212 - font_width(xs), y0 + 11);
            if (xp_timer > 0) {
              xp_timer--;
              snprintf(xs, sizeof(xs), "+%d XP", (int)xp_shown);
              gfx_set_color(&g, 90, 200, 255);
              font_draw(&g, xs, 18 + font_width(lv), y0 + 11);
            }
          }
          // stat$m's bonus stat point popups (XT 5631-5835), sliding in from
          // the left: the waste's in red, the checkpoint's under it in green.
          // Their shake is this frame's own, not the career's random stream.
          int32_t py = 218;
          {
            // stat$m's running totals (XT 5306-5535), above them: "N WASTES
            // WITH car" on each waste, "N CLEARED IN TOTAL" every 4th
            // checkpoint, "N CLEARED WITH car" every 2nd. Same slide, on ticks.
            // One already up keeps sliding (its number updates), and the
            // waste's cancels the checkpoints' (its block runs first and clears
            // shwcnt / shwcncnt). Settings > Interface > Career Counters hides them.
            static int32_t cnt_prev[3] = {-1, -1, -1}, cnt_x[3], cnt_show[3];
            static bool cnt_on[3], cnt_back[3];
            const int32_t now_v[3] = {csave.killscn[me], csave.wins, csave.winscn[me]};
            for (int32_t k = 0; k < 3; k++) {
              if (starcnt > 0 || cnt_prev[k] < 0) {
                cnt_on[k] = false;
              } else if (now_v[k] > cnt_prev[k] && (k == 0 || now_v[k] % (k == 1 ? 4 : 2) == 0) && !cnt_on[k]) {
                cnt_on[k] = true;
                cnt_x[k] = -55;
                cnt_show[k] = 0;
                cnt_back[k] = false;
              }
              cnt_prev[k] = now_v[k];
              if (k > 0 && cnt_on[0]) cnt_on[k] = false;
              if (!cnt_on[k] || !settings.career_counts) continue;
              if (race_ticked) {
                if (cnt_x[k] < 15 && !cnt_back[k]) cnt_x[k] += 8;
                if (cnt_x[k] >= 15) cnt_show[k]++;
                cnt_back[k] = cnt_show[k] >= 30;
                if (cnt_back[k]) cnt_x[k] -= 8;
                if (cnt_x[k] <= -400) cnt_on[k] = false;
              }
              char cl[96];
              if (k == 0) snprintf(cl, sizeof(cl), now_v[0] == 1 ? "%d WASTE WITH %s" : "%d WASTES WITH %s", (int)now_v[0], car_name(sc[0]));
              else if (k == 1) snprintf(cl, sizeof(cl), "%d CLEARED IN TOTAL", (int)now_v[1]);
              else snprintf(cl, sizeof(cl), "%d CLEARED WITH %s", (int)now_v[2], car_name(sc[0]));
              font_set(FONT_BOLD, 16);
              if (k == 0) gfx_set_color(&g, 130, 0, 0);
              else if (k == 1) gfx_set_color(&g, 0, 70, 0);
              else gfx_set_color(&g, 0, 110, 0);
              font_draw(&g, cl, cnt_x[k], py);
              py += 23;
            }
          }
          {
            static uint32_t shake = 12345u;
            for (int32_t i = 0; i < 2; i++) {
              const int32_t chance = i == 0 ? crun.killchance[1] : crun.winchance[1];
              if (chance > 1000 || crun.pop_amount[i] == 0) continue;
              char pl[48];
              if (crun.pop_amount[i] == 1) snprintf(pl, sizeof(pl), "YOU HAVE GAINED 1 BONUS STAT POINT!");
              else snprintf(pl, sizeof(pl), "YOU HAVE GAINED %d BONUS STAT POINTS!", (int)crun.pop_amount[i]);
              shake = shake * 1103515245u + 12345u;
              font_set(FONT_BOLD, 16);
              if (i == 0) gfx_set_color(&g, 130, 0, 0);
              else gfx_set_color(&g, 0, 110, 0);
              font_draw(&g, pl, crun.pop_x[i], py + (int32_t)((shake >> 16) % 5u));
              py += 23;
            }
          }
          // careermode$m's own HUD: the Titan's bar (XT 8472-8535), bonus
          // 4's last car (8308-8350), 250 wide, top centre; and its flashing
          // red line (drawcs(450, ...)), over the level bar.
          if (cstage.boss_bar || cstage.health_bar) {
            const int32_t bx = 400 - 125, by = 5;
            if (cstage.boss_bar) {
              gfx_set_color(&g, 0, hud_tint(225.0, m.snap[1]), 0);
              gfx_fill_rect(&g, bx, by, 250, 20);
              gfx_set_color(&g, hud_tint(225.0, m.snap[0]), 0, 0);
              gfx_fill_rect(&g, bx, by, (int32_t)(cstage.boss_fill * 250.0), 20);
            } else {
              gfx_set_color(&g, cstage.health_rgb[0], cstage.health_rgb[1], cstage.health_rgb[2]);
              gfx_fill_rect(&g, bx, by, cstage.health_fill, 20);
            }
            gfx_set_color(&g, 0, 0, 0);
            gfx_draw_rect(&g, bx, by, 250, 20);
            font_set(FONT_BOLD, 15);
            if (cstage.boss_bar) {
              draw_centered(&g, cstage.boss_pct, 400, by + 15);
              gfx_set_color(&g, cstage.boss_info_rgb[0], cstage.boss_info_rgb[1], cstage.boss_info_rgb[2]);
              draw_centered(&g, cstage.boss_info, 400, by + 40);
            } else if (cstage.health_slot >= 0 && cstage.health_slot < nplayers) {
              draw_centered(&g, career_names[cstage.health_slot], 400, by + 15);
            }
          }
          if (cstage.msg[0]) {
            font_set(FONT_BOLD, 15);
            gfx_set_color(&g, cstage.msg_r, 0, 0);
            draw_centered(&g, cstage.msg, 400, y0 - 8);
          }
          font_set(FONT_BOLD, 12);
        }
        if (PT_ACTIVE && (pt.match == 1 || pt.match == 4 || pt.match == 5)) {
          // tourney()'s scoreboard (XT 5718-5733): the top three and yours,
          // bottom left, moved from its 480-tall screen.
          int32_t top[3] = {-1, -1, -1};
          for (int32_t a = 0; a < EXT_PT_CARS; a++) {
            if (pt.position[a] >= 0 && pt.position[a] < 3) top[pt.position[a]] = a;
          }
          static const int32_t kTopRgb[3][3] = {{233, 187, 0}, {170, 170, 170}, {210, 105, 0}};
          font_set(FONT_BOLD, 16);
          for (int32_t r = 0; r < 3; r++) {
            if (top[r] < 0) continue;
            char line[48];
            gfx_set_color(&g, kTopRgb[r][0], kTopRgb[r][1], kTopRgb[r][2]);
            font_draw(&g, kExtPTOpponents[top[r]], 25, 345 + r * 27);
            snprintf(line, sizeof(line), "%d", pt.score[top[r]]);
            gfx_set_color(&g, 0, 100, 0);
            font_draw(&g, line, 190, 345 + r * 27);
          }
          char mine[32];
          snprintf(mine, sizeof(mine), "%d", pt.score[0]);
          gfx_set_color(&g, 0, 0, 0);
          font_draw(&g, "Your score:", 25, 426);
          gfx_set_color(&g, 0, 100, 0);
          font_draw(&g, mine, 190, 426);
          font_set(FONT_BOLD, 12);
        }
        draw_hud_img(&g, hud_images.was, 92 + lap_dx, 7);
        hud_set_ink(&g, 0, 0, 100);
        // Java: checkPoints.wasted / (nplayers-1), less the career's undead (XT 4602)
        snprintf(hud, sizeof(hud), "%d / %d", cp.wasted, nplayers - 1 - (ext_career ? career_stage_undeadextra(&cstage) : 0));
        font_draw(&g, hud, 150 + lap_dx, 18);
        draw_hud_img(&g, hud_images.pos, 42, 27);
        if (cp.pos[0] >= 0 && cp.pos[0] < 8) {
          draw_hud_img(&g, hud_images.rank[cp.pos[0]], 110, 28);
        } else if (cp.pos[0] >= 8) {
          // 1.gif-8.gif are all there are; a bigger field's 9th+ as text.
          char place[8];
          snprintf(place, sizeof(place), "%dth", cp.pos[0] + 1);
          hud_set_ink(&g, 0, 0, 100);
          font_set(FONT_BOLD, 13);
          font_draw(&g, place, 112, 43);
          font_set(FONT_BOLD, 12);
        }
        if (xt.extended) {
          // Extended's speedometer under the position (XT 5189-5193; its
          // fifawelcome face, Arial bold here).
          char mph[16];
          snprintf(mph, sizeof(mph), "%d MPH", (int)fabsf(mad[0].speed / 2.0f));
          hud_set_ink(&g, 0, 0, 0);
          font_set(FONT_BOLD, 17);
          font_draw(&g, mph, 12, 70);
          font_set(FONT_BOLD, 12);
        }

        // Ports drawstat(maxmag, hitmag, newcar, power)'s own two
        // fillPolygon bars (damage bar top, power bar bottom) -- JS lines
        // 1812-1890. `newcar` is read by the JS signature but never
        // actually used in its body (a real, harmless quirk of the
        // original, not a mistranslation -- preserved as-is).
        {
          int32_t maxmag = mad[0].cd->maxmag[mad[0].cn];   // live: a special changes it
          int32_t hitmag = mad[0].hitmag;
          if (hitmag > maxmag) hitmag = maxmag;
          float ratio = (float)hitmag / (float)maxmag; // fr(n2/n), case 1
          int32_t n4 = jtrunc(98.0f * ratio);           // fr(98.0*ratio), case 1

          int32_t bar_x[4] = {662, 662, 662 + n4, 662 + n4};
          int32_t bar_y[4] = {11, 20, 20, 11};

          int32_t n5 = 244, n6 = 244, n7 = 11;
          if (n4 > 33) {
            float x = (float)(n4 - 33);
            float y = x / 65.0f;
            float z = 233.0f * y;
            n6 = jtrunc(244.0f - z);
          }
          if (n4 > 70) {
            if (xt.dmcnt < 10) {
              if (xt.dmflk) { n6 = 170; xt.dmflk = false; }
              else xt.dmflk = true;
            }
            xt.dmcnt++;
            if ((float)xt.dmcnt > 167.0f - (float)n4 * 1.5f) xt.dmcnt = 0;
          }
          int32_t snap_arr[3] = {m.snap[0], m.snap[1], m.snap[2]};
          int32_t bar_rgb[3];
          int32_t base[3] = {n5, n6, n7};
          for (int32_t ch = 0; ch < 3; ch++) {
            float snap_term = (float)snap_arr[ch] / 100.0f;
            float term = (float)base[ch] * snap_term;
            float sum = (float)base[ch] + term;
            int32_t v = jtrunc(sum);
            if (v > 255) v = 255;
            if (v < 0) v = 0;
            bar_rgb[ch] = v;
          }
          gfx_set_color(&g, bar_rgb[0], bar_rgb[1], bar_rgb[2]);
          gfx_fill_polygon(&g, bar_x, bar_y, 4);

          float power = mad[0].power;
          int32_t n8 = 128;
          if (power == 98.0f) n8 = 64;
          int32_t n9 = jtrunc_d(190.0 + (double)power * 0.37); // no fr() in the original -- real double math
          int32_t n10 = 244;
          if (xt.auscnt < 45 && xt.aflk) { n8 = 128; n9 = 244; n10 = 244; }
          int32_t power_x2 = jtrunc(662.0f + power); // fr(662.0+n3), case 1

          int32_t bar2_x[4] = {662, 662, power_x2, power_x2};
          int32_t bar2_y[4] = {31, 40, 40, 31};
          int32_t base2[3] = {n8, n9, n10};
          int32_t bar2_rgb[3];
          for (int32_t ch = 0; ch < 3; ch++) {
            float snap_term = (float)snap_arr[ch] / 100.0f;
            float term = (float)base2[ch] * snap_term;
            float sum = (float)base2[ch] + term;
            int32_t v = jtrunc(sum);
            if (v > 255) v = 255;
            if (v < 0) v = 0;
            bar2_rgb[ch] = v;
          }
          gfx_set_color(&g, bar2_rgb[0], bar2_rgb[1], bar2_rgb[2]);
          gfx_fill_polygon(&g, bar2_x, bar2_y, 4);
        }

        // Radar overlay -- xtGraphics.java:8005-8007, the LAST thing the HUD
        // block does, right after drawstat() above. The speedometer used to
        // be drawn here unconditionally; it is not a base-HUD element at all
        // in the original, but the tail of radarstat(), so it moved inside
        // radar_stat() and now appears only while the radar is toggled on.
        // See that function's own doc comment for the evidence (`this.sped`
        // has exactly one draw site in the whole source, line 9067).
        if (control[0].radar && !NFM_NO_ARROW_STAGE) {
          radar_stat(&g, &m, &xt, &mad[0], &co[0], &cp, control[0].arrace, nplayers,
                     hud_images.sped);
        }

        // arrace leaderboard -- xtGraphics.java:3688's own gate. `starcnt <
        // 38` keeps it off the grid before the countdown starts, and the
        // `|| multion >= 2` arm beside it is multiplayer-only. The
        // dested-clears-the-lock step at :3689-3692 runs here too, even
        // though alocked never leaves -1 in this port (see xt_graphics.h),
        // because it is the source's own first statement in this block.
        if (xt.extended) {
          // Extended keeps its car list up all race, under the Special bar.
          if (starcnt == 0 && !race_holdit) {
            draw_ext_board(&g, &m, &cp, mad, nplayers, sc, &specials, control[0].arrace, ext_listbars,
                           race_ticked, PT_ACTIVE ? kExtPTOpponents : (ext_career ? career_names : NULL));
          }
        } else if (control[0].arrace && starcnt < 38 && !race_holdit && cp.stage != 10) {
          if (xt.alocked != -1 && cp.dested[xt.alocked] != 0) {
            xt.alocked = -1;
            xt.lalocked = -1;
          }
          draw_arrace_board(&g, &m, &cp, nplayers, sc, settings.board_names != 0);
        }

        // Countdown 3-2-1-GO overlay -- Java xtGraphics.java:8050-8055. Only
        // draws while starcnt is in (0, 35]; the 3/2/1 glyphs anchor at
        // (385, 50), the GO glyph (gocnt==0, wider) shifts left to (363, 50)
        // so it stays visually centred. cntdn[gocnt].tex<0 falls back to a
        // text number so the countdown still tells the player it's holding.
        // Also under the `!holdit` of :8009 (the countdown block :8010 is
        // that gate's first statement) -- can't actually co-occur here, but
        // kept explicit so the guard's extent matches the source's.
        if (!race_holdit && starcnt > 0 && starcnt <= 35) {
          // :8032-8044 -- which face. Mouth shut by default, open through a
          // 5-tick window straddling each of 3/2/1, and the third face from
          // GO onward. The windows are the source's own literals.
          int32_t duds = 0;
          if (starcnt <= 37 && starcnt > 32) duds = 1;
          if (starcnt <= 26 && starcnt > 21) duds = 1;
          if (starcnt <= 15 && starcnt > 10) duds = 1;
          if (starcnt <= 4) duds = 2;
          // :8045-8049 -- drawn BEFORE the glyph so the number sits on top,
          // and at 30% alpha. `dudo != -1` is the source's own "no face this
          // race" sentinel; musicomp() only ever sets 250 or 428, so the
          // test cannot fail here, but it costs nothing to keep.
          if (race_dudo != -1 && menu_dude[duds].tex >= 0) {
            gfx_set_composite(&g, 0.3f);
            gfx_draw_image(&g, menu_dude[duds].tex, race_dudo, 0,
                            menu_dude[duds].w, menu_dude[duds].h);
            gfx_set_composite(&g, 1.0f);
          }
          HudImg cd_img = hud_images.cntdn[gocnt];
          int32_t x = (gocnt != 0) ? 385 : 363;
          if (cd_img.tex >= 0) {
            gfx_draw_image(&g, cd_img.tex, x, 50, cd_img.w, cd_img.h);
          } else {
            const char *fallback = (gocnt == 0) ? "GO" : (gocnt == 1) ? "1" : (gocnt == 2) ? "2" : "3";
            gfx_set_color(&g, 255, 200, 0);
            font_set(FONT_BOLD, 22);
            draw_centered(&g, fallback, 400, 80);
            font_set(FONT_BOLD, 12);
          }
        }

        // Win/lose hold-card -- Java xtGraphics.java:7683-7803 (drawhi(<card>,
        // 70) + blinking message + "Press Enter to continue") shown over the
        // frozen-in-place racing scene until the player presses ENTER or
        // ~13.2s elapse (race_holdit/race_holdcnt, advanced in the tick loop
        // above). race_end_kind (set by that same tick-loop scan) picks the
        // card and text: youwastedem.gif + "You Won, all cars have been
        // wasted!" (:7683-7690), yourwasted.gif + no blink message, just the
        // continue prompt (:7708-7722), or the ordinary youwon.gif/youlost.gif
        // + "You finished first, nice job!" / "<car> finished first, race
        // over!" (:7742-7761, race_lost_car_name).
        if (race_holdit) {
          HudImg card;
          switch (race_end_kind) {
            case RACE_END_ALL_WASTED: card = hud_images.youwastedem; break;
            case RACE_END_PLAYER_WASTED: card = hud_images.yourwasted; break;
            default: card = race_winner ? hud_images.youwon : hud_images.youlost; break;
          }
          int32_t cy = 70;
          if (card.tex >= 0) {
            // Java drawhi():8521-8534 -- rounded card behind the glyph, only
            // when the stage's sky is dark (m.darksky). No rounded-rect
            // primitive in gfx.h (same simplification STATE_STAGE_LOADING's
            // panel already makes) -- a plain rect in the same derived
            // colour reads the same at this size.
            if (m.darksky && kJavaDarkSkyBoxes) {
              float hsb[3];
              rgb_to_hsb(m.csky[0], m.csky[1], m.csky[2], hsb);
              hsb[2] = 0.6f;
              int32_t rgb = hsb_to_rgb(hsb[0], hsb[1], hsb[2]);
              int32_t cr = (rgb >> 16) & 0xff, cgg = (rgb >> 8) & 0xff, cb = rgb & 0xff;
              gfx_set_color(&g, cr, cgg, cb);
              gfx_fill_rect(&g, 390 - card.w / 2, cy - 2, card.w + 20, card.h + 2);
              gfx_set_color(&g, (int32_t)(cr / 1.1f), (int32_t)(cgg / 1.1f), (int32_t)(cb / 1.1f));
              gfx_draw_rect(&g, 390 - card.w / 2, cy - 2, card.w + 20, card.h + 2);
            }
            gfx_draw_image(&g, card.tex, 400 - card.w / 2, cy, card.w, card.h);
          }
          // Blinking message -- Java :7744-7749/:7686-7689 alternates
          // (0,0,0)/(0,128,255) at y=120 each draw via the shared `aflk`
          // field; race_hold_aflk is this block's own toggle so it doesn't
          // fight the menu screens' mainmenu_aflk over the same variable.
          // RACE_END_PLAYER_WASTED draws no blink message at all -- Java
          // :7708-7722 has no drawcs(120, ...) call in that branch, unlike
          // its two siblings.
          if (race_end_kind != RACE_END_PLAYER_WASTED) {
            const int32_t cb = race_hold_aflk ? 0 : 128, cbb = race_hold_aflk ? 0 : 255;
            race_hold_aflk = !race_hold_aflk;
            char msg[64];
            if (race_end_kind == RACE_END_ALL_WASTED) {
              snprintf(msg, sizeof(msg), "You Won, all cars have been wasted!");
            } else if (race_winner) {
              snprintf(msg, sizeof(msg), "You finished first, nice job!");
            } else {
              snprintf(msg, sizeof(msg), "%s finished first, race over!", race_lost_car_name);
            }
            hud_say_draw(&g, &m, 120, msg, 0, cb, cbb, 0);
          }
          // "Press [ Enter ] to continue" -- Java :7690/:7721/:7788, drawn a
          // fixed black (0,0,0), not blinking, at y=350 for every ending in
          // this single-player path (the ordinary finish-line ending draws it
          // too -- :7788 -- a real gap in this port before this change, see
          // this block's own git history).
          hud_say_draw(&g, &m, 350, "Press  [ " KEY_CONTINUE " ]  to continue", 0, 0, 0, 0);

        }
        // Ready only once this race has ticked: a frame drawn before its
        // first tick would otherwise blend -- and restore into the cars --
        // the snapshots the LAST race left, scattering the starting grid.
        if (use_rt && settings.smooth && smooth_captured) {
          gfx_clip_save(&hud_clip, &g, hud_mark);
          smooth_ready = true;
        }
      }
    } else if (state == STATE_REPLAY && !reuse_frame) {
      // REPLAY -- GameSparker.java fase==-3, :1388-1630. Reconstructs each
      // car's frozen pose via record_playh() at replay_tick (0-299) and
      // swings the camera between record.wasted and the local player
      // (medium_around()/medium_transaround()) -- either a close-finish
      // reel (record.closefinish 0/1/else picks one of 3 choreographies)
      // or, when the LOCAL player is the one who was caught
      // (record.wasted==xt.im), a single-car "crash reel" driven by
      // replay_variant instead.
      if (replay_tick == 0) {
        // :1389-1409 -- pick this reel's variant / twist the camera 90
        // degrees for certain replays, then snap every car back to
        // record's frozen starting pose (record.starcar[]).
        if (rpd.wasted == 0) {
          if (rpd.whenwasted == 229) {
            replay_variant = 67;
            m.vxz += 90;
          } else {
            replay_variant = jtrunc(medium_random(&m) * 4.0f);
            if (replay_variant == 1 || replay_variant == 3) replay_variant = 69;
            if (replay_variant == 2 || replay_variant == 4) replay_variant = 30;
          }
        } else if (rpd.closefinish != 0 && replay_loops != 0) {
          m.vxz += 90;
        }
        for (int32_t i = 0; i < nplayers; i++) {
          cont_o_recopy(&co[i], &rpd.starcar[i], 0, 0, 0, 0);
        }
      }

      // :1414-1462 -- medium.d() + a painter's-algorithm depth sort,
      // reusing racing's all_objs/visible_idx/rank/order scratch buffers
      // (sized for nplayers+stage objects, allocated once and never freed
      // between states).
      //
      // NOT the same sort as racing's own draw block above, even though
      // the two look like they should share code: the ORIGINAL spells
      // them differently. Racing (GameSparker.java:921-935) compares with
      // a bare `if (a.dist < b.dist) rank[i]++ else rank[j]++`, which on
      // equal dist credits the LATER index. This replay copy (:1430-1454)
      // tests `dist != dist` first and, on a tie, takes `else if (n85 >
      // n84)` -- and since the inner loop always runs n85 = n84+1, that
      // branch is always taken, crediting the EARLIER index instead. So
      // two objects at exactly equal dist come out in opposite draw order
      // in the two blocks, and this one previously carried racing's
      // version, flipping which of an equidistant pair paints on top.
      //
      // Both spellings still produce a true permutation of 0..n-1 (each
      // is a consistent total order -- dist descending, ties broken by
      // ascending index in racing and descending index here), which is
      // why the source's own two final-draw loops are interchangeable:
      // :1456-1462 rescans for `rank == n` while :936-938 indexes through
      // order[]. Keeping order[] here, since with no duplicate ranks the
      // two are identical and this one is O(n) instead of O(n^2).
      draw_race_scene(&g, &m, all_objs, total_objs, visible_idx, rank, order);

      // :1463-1477 -- per-car fix-zone resurrection + frozen damage-dent
      // replay. Real fix zones do exist in stage data (see racing's own
      // newcar-rebuild comment above), so hfix[i] CAN match replay_tick
      // for a car that got fixed during the recorded window.
      for (int32_t i = 0; i < nplayers; i++) {
        // The repair flash's step, once per replay tick (the draw above used
        // to advance it; see cont_o_fixit).
        cont_o_step_fix(&co[i]);
        if (rpd.hfix[i] == replay_tick) {
          if (co[i].dist == 0) co[i].fcnt = 8;
          else co[i].fix = true;
        }
        if (co[i].fcnt == 7 || co[i].fcnt == 8) {
          cont_o_recopy(&co[i], RACE_MODEL(i, mad[i].cn), 0, 0, 0, 0);
          RACE_LOOK(i);
          rpd.cntdest[i] = 0;
        }
        record_playh(&rpd, &co[i], &mad[i], i, replay_tick, xt.im);
      }

      // :1481-1486/1478-1480 -- ENTER/handbrake (or 2 full loops through
      // the reel) skip straight past it. Java's own target is fase=-4 (the
      // Madness-logo flash); this port goes straight to STATE_POST_RACE
      // instead -- see the hold-card advance block's own comment on why
      // skipping that purely-cosmetic transition loses no functionality.
      bool replay_advance_enter = replay_confirm_latch || control[0].handb;
      replay_confirm_latch = false;
      control[0].handb = false;
      if ((replay_loops == 2 && replay_tick == 299) || replay_advance_enter) {
        state = STATE_POST_RACE;
      } else {
        // :1488 -- levelhigh()'s caption panel + flashing text.
        if (menu_gameh.tex >= 0) gfx_draw_image(&g, menu_gameh.tex, 301, 20, menu_gameh.w, menu_gameh.h);
        {
          int32_t cr = 16, cgg = 48, cb = 96;
          if (replay_tick < 50) {
            if (xt.aflk) { cr = 106; cgg = 176; cb = 255; xt.aflk = false; }
            else xt.aflk = true;
          }
          if (rpd.wasted != xt.im) {
            if (rpd.closefinish == 0) hud_say_draw(&g, &m, 60, "You Wasted 'em!", cr, cgg, cb, 0);
            else if (rpd.closefinish == 1) hud_say_draw(&g, &m, 60, "Close Finish!", cr, cgg, cb, 0);
            else hud_say_draw(&g, &m, 60, "Close Finish!  Almost got it!", cr, cgg, cb, 0);
          } else if (rpd.whenwasted == 229) {
            // Java's sibling "Disconnected!" branch is multiplayer-only
            // (discon never reaches 240 with no netplay) -- always this.
            hud_say_draw(&g, &m, 60, "Wasted!", cr, cgg, cb, 0);
          } else if (cp.stage > 2 || cp.stage < 0) {
            hud_say_draw(&g, &m, 60, "Stunts!", cr, cgg, cb, 0);
          } else {
            hud_say_draw(&g, &m, 60, "Best Stunt!", cr, cgg, cb, 0);
          }
          hud_say_draw(&g, &m, 380, "Press  [ " KEY_CONTINUE " ]  to continue", 0, 0, 0, 0);
        }

        // :1489-1492 -- a brief black flash covering the whole scene the
        // instant the reel (re)starts, painted AFTER the level-high
        // caption above, matching the Java's own draw order exactly.
        if (replay_tick == 0 || replay_tick == 1 || replay_tick == 2) {
          gfx_set_color(&g, 0, 0, 0);
          gfx_fill_rect(&g, 0, 0, width, height);
        }

        if (rpd.wasted != xt.im) {
          if (rpd.closefinish == 0) {
            // :1493-1516 -- two-car swing: around(local) -> transaround
            // -> around(wasted), gated on replay_tick > whenwasted.
            if (replay_cam == 9 || replay_cam == 11) {
              gfx_set_color(&g, 255, 255, 255);
              gfx_fill_rect(&g, 0, 0, width, height);
            }
            if (replay_cam == 0) medium_around(&m, &co[xt.im], false);
            if (replay_cam > 0 && replay_cam < 20) medium_transaround(&m, &co[xt.im], &co[rpd.wasted], replay_cam);
            if (replay_cam == 20) medium_around(&m, &co[rpd.wasted], false);
            if (replay_tick > rpd.whenwasted && replay_cam != 20) replay_cam++;
            if ((replay_cam == 0 || replay_cam == 20) && ++replay_tick == 300) {
              replay_tick = 0; replay_cam = 0; replay_loops++;
            }
          } else if (rpd.closefinish == 1) {
            // :1517-1552 -- around(local) -> trans -> around(wasted) ->
            // trans -> around(local), gated at replay_tick 160/230/280.
            if (replay_cam == 0) medium_around(&m, &co[xt.im], false);
            if (replay_cam > 0 && replay_cam < 20) medium_transaround(&m, &co[xt.im], &co[rpd.wasted], replay_cam);
            if (replay_cam == 20) medium_around(&m, &co[rpd.wasted], false);
            if (replay_cam > 20 && replay_cam < 40) medium_transaround(&m, &co[rpd.wasted], &co[xt.im], replay_cam - 20);
            if (replay_cam == 40) medium_around(&m, &co[xt.im], false);
            if (replay_cam > 40 && replay_cam < 60) medium_transaround(&m, &co[xt.im], &co[rpd.wasted], replay_cam - 40);
            if (replay_cam == 60) medium_around(&m, &co[rpd.wasted], false);
            if (replay_tick > 160 && replay_cam < 20) replay_cam++;
            if (replay_tick > 230 && replay_cam < 40) replay_cam++;
            if (replay_tick > 280 && replay_cam < 60) replay_cam++;
            if ((replay_cam == 0 || replay_cam == 20 || replay_cam == 40 || replay_cam == 60) && ++replay_tick == 300) {
              replay_tick = 0; replay_cam = 0; replay_loops++;
            }
          } else {
            // :1554-1599 -- the longest swing: around/trans x4, gated at
            // replay_tick 90/160/230/280.
            if (replay_cam == 0) medium_around(&m, &co[xt.im], false);
            if (replay_cam > 0 && replay_cam < 20) medium_transaround(&m, &co[xt.im], &co[rpd.wasted], replay_cam);
            if (replay_cam == 20) medium_around(&m, &co[rpd.wasted], false);
            if (replay_cam > 20 && replay_cam < 40) medium_transaround(&m, &co[rpd.wasted], &co[xt.im], replay_cam - 20);
            if (replay_cam == 40) medium_around(&m, &co[xt.im], false);
            if (replay_cam > 40 && replay_cam < 60) medium_transaround(&m, &co[xt.im], &co[rpd.wasted], replay_cam - 40);
            if (replay_cam == 60) medium_around(&m, &co[rpd.wasted], false);
            if (replay_cam > 60 && replay_cam < 80) medium_transaround(&m, &co[rpd.wasted], &co[xt.im], replay_cam - 60);
            if (replay_cam == 80) medium_around(&m, &co[xt.im], false);
            if (replay_tick > 90 && replay_cam < 20) replay_cam++;
            if (replay_tick > 160 && replay_cam < 40) replay_cam++;
            if (replay_tick > 230 && replay_cam < 60) replay_cam++;
            if (replay_tick > 280 && replay_cam < 80) replay_cam++;
            if ((replay_cam == 0 || replay_cam == 20 || replay_cam == 40 || replay_cam == 60 || replay_cam == 80) && ++replay_tick == 300) {
              replay_tick = 0; replay_cam = 0; replay_loops++;
            }
          }
        } else {
          // :1601-1629 -- single-car "crash reel": local player was the
          // one caught, so the camera just orbits their own wreck while
          // replay_variant (67/69/30) picks a strobe pattern.
          if (replay_variant == 67 && (replay_cam == 3 || replay_cam == 31 || replay_cam == 66)) {
            gfx_set_color(&g, 255, 255, 255);
            gfx_fill_rect(&g, 0, 0, width, height);
          }
          if (replay_variant == 69 && (replay_cam == 3 || replay_cam == 5 || replay_cam == 31 ||
                                        replay_cam == 33 || replay_cam == 66 || replay_cam == 68)) {
            gfx_set_color(&g, 255, 255, 255);
            gfx_fill_rect(&g, 0, 0, width, height);
          }
          if (replay_variant == 30 && replay_cam >= 1 && replay_cam < 30) {
            int32_t period = jtrunc(2.0f + medium_random(&m) * 3.0f);
            if (period != 0 && replay_cam % period == 0 && replay_flicker == 0) {
              gfx_set_color(&g, 255, 255, 255);
              gfx_fill_rect(&g, 0, 0, width, height);
              replay_flicker = 1;
            } else {
              replay_flicker = 0;
            }
          }
          if (replay_tick > rpd.whenwasted && replay_cam != replay_variant) replay_cam++;
          medium_around(&m, &co[xt.im], false);
          if ((replay_cam == 0 || replay_cam == replay_variant) && ++replay_tick == 300) {
            replay_tick = 0; replay_cam = 0; replay_loops++;
          }
        }
      }
    } else if (state == STATE_PAUSE_REPLAY && !reuse_frame) {
      // PAUSE REPLAY -- fase -1, GameSparker.java:1258-1345. Unlike the
      // post-race highlight reel (fase -3), this replays the RAW 300-tick
      // ring from its start, with the camera orbiting the player's car
      // (medium.around(array2[0], false) at :1345, after the frame).
      if (pause_replay_tick == 0) {
        // :1259-1263 -- stash every car's LIVE pose into ocar[] so the
        // last frame can put it back, then jump them to the ring's start.
        for (int32_t i = 0; i < nplayers; i++) {
          cont_o_recopy(&rpd.ocar[i], &co[i], 0, 0, 0, 0);
          cont_o_recopy(&co[i], &rpd.car[0][i], 0, 0, 0, 0);
        }
      }
      draw_race_scene(&g, &m, all_objs, total_objs, visible_idx, rank, order);

      // :1314-1319 -- confirm skips to the last frame rather than exiting
      // outright, which is what makes the restore below still run.
      if (replay_confirm_latch) pause_replay_tick = 299;
      replay_confirm_latch = false;

      // :1320-1337 -- fix-zone resurrection, then the frame itself.
      for (int32_t i = 0; i < nplayers; i++) {
        // The repair flash's step, once per replay tick (the draw above used
        // to advance it; see cont_o_fixit).
        cont_o_step_fix(&co[i]);
        if (rpd.fix[i] == pause_replay_tick) {
          if (co[i].dist == 0) co[i].fcnt = 8;
          else co[i].fix = true;
        }
        if (co[i].fcnt == 7 || co[i].fcnt == 8) {
          cont_o_recopy(&co[i], RACE_MODEL(i, mad[i].cn), 0, 0, 0, 0);
          RACE_LOOK(i);
          rpd.cntdest[i] = 0;
        }
        if (pause_replay_tick == 299) {
          cont_o_recopy(&co[i], &rpd.ocar[i], 0, 0, 0, 0);
        }
        record_play(&rpd, &co[i], &mad[i], i, pause_replay_tick);
      }

      // :1338-1344 -- one pass only, then back to the pause menu; every
      // other frame draws replyn()'s blinking caption (:4809-4818).
      pause_replay_tick++;
      if (pause_replay_tick == 300) {
        pause_replay_tick = 0;
        pause_snapshot_pending = true; // :1340 -- back through fase -6
        state = STATE_PAUSED;
      } else {
        if (xt.aflk) {
          hud_say_draw(&g, &m, 30, "Replay  > ", 0, 0, 0, 0);
          xt.aflk = false;
        } else {
          hud_say_draw(&g, &m, 30, "Replay  >>", 0, 128, 255, 0);
          xt.aflk = true;
        }
      }
      // :1345 -- the camera orbits the player's car every replay frame. This
      // was missing, so the view stayed frozen where the race was paused
      // and the replayed cars drove straight out of shot -- the replay
      // looked like it did nothing.
      medium_around(&m, &co[0], false);
    } else if (state == STATE_PT_INFO || state == STATE_PT_SCORES) {
      // Extended's ptstart / scoreshow, as plain panels on the game's dark
      // blue: the match and its rules, then the points.
      gfx_set_color(&g, 12, 18, 40);
      gfx_fill_rect(&g, 0, 0, 800, 450);
      gfx_set_color(&g, 255, 196, 0);
      font_set(FONT_BOLD, 22);
      draw_centered(&g, "THE PREMIER TOURNAMENT", 400, 50);
      if (state == STATE_PT_INFO) {
        const char *const *rules = ext_pt_rules(pt.match);
        font_set(FONT_BOLD, 18);
        gfx_set_color(&g, 255, 255, 255);
        draw_centered(&g, rules[0], 400, 90);
        font_set(FONT_BOLD, 13);
        int32_t y = 125;
        for (int32_t li = 1; rules[li]; li++) {
          gfx_set_color(&g, li <= 2 ? 120 : 220, li <= 2 ? 200 : 220, li <= 2 ? 255 : 220);
          font_draw(&g, rules[li], 120, y);
          y += li == 2 ? 30 : 22;
        }
        if (pt.match == 1) {
          gfx_set_color(&g, 170, 170, 170);
          font_set(FONT_BOLD, 12);
          font_draw(&g, "OPPONENTS: Motion, Redline, Crash, Swift, Omega, Olsie820,", 120, 330);
          font_draw(&g, "RadicalRacer, Kaffeinated (contender), InsanElite (contender), Grimjow (favourite)", 120, 348);
        }
      } else {
        // The table, most points first.
        int32_t order[EXT_PT_CARS];
        for (int32_t a = 0; a < EXT_PT_CARS; a++) order[a] = a;
        for (int32_t a = 0; a < EXT_PT_CARS; a++)
          for (int32_t b = a + 1; b < EXT_PT_CARS; b++)
            if (pt.points[order[b]] > pt.points[order[a]]) { int32_t t2 = order[a]; order[a] = order[b]; order[b] = t2; }
        font_set(FONT_BOLD, 16);
        gfx_set_color(&g, 255, 255, 255);
        char head[64];
        snprintf(head, sizeof(head), "AFTER MATCH %d", pt.match);
        draw_centered(&g, head, 400, 85);
        font_set(FONT_BOLD, 13);
        for (int32_t r = 0; r < EXT_PT_CARS; r++) {
          const int32_t a = order[r];
          char row[96];
          snprintf(row, sizeof(row), "%2d.  %s", r + 1, kExtPTOpponents[a]);
          if (a == 0) gfx_set_color(&g, 255, 196, 0); else gfx_set_color(&g, 220, 220, 220);
          font_draw(&g, row, 230, 118 + r * 24);
          snprintf(row, sizeof(row), "%d  (+%d)", pt.points[a], pt.gained[a]);
          font_draw(&g, row, 500, 118 + r * 24);
        }
        if (pt.match == EXT_PT_MATCHES) {
          const int32_t champ = ext_pt_champion(&pt);
          char line[96];
          snprintf(line, sizeof(line), champ == 0 ? "You won the Premier Tournament!" : "%s won the Premier Tournament!",
                   kExtPTOpponents[champ]);
          gfx_set_color(&g, 255, 196, 0);
          font_set(FONT_BOLD, 16);
          draw_centered(&g, line, 400, 395);
        }
      }
      gfx_set_color(&g, 150, 150, 150);
      font_set(FONT_BOLD, 12);
      draw_centered(&g, state == STATE_PT_INFO ? "Press " KEY_CONTINUE " to start the match" : "Press " KEY_CONTINUE " to continue", 400, 430);
    } else if (state == STATE_SETTINGS) {
      // Opaque, the whole 800x450 (see settings_screen_draw()).
      settings_screen_draw(&g, &settings_ui, &settings, platform_has_rumble());
      if (settings_ui.page == SET_RIVALS) {
        // Each rival's car in its row (settings_screen_draw left it room).
        int32_t rows[SETTINGS_MAX_ROWS], h, gap, ty, fs;
        const int32_t n = settings_window(&settings_ui, &settings, platform_has_rumble(), rows);
        settings_metrics(SET_RIVALS, n, &h, &gap, &ty, &fs);
        for (int32_t i = 0; i < n; i++) {
          const SettingsRow *row = &kSettingsPages[SET_RIVALS].rows[rows[i]];
          if (row->kind != ROW_CAR) continue;
          const int32_t v = *(const int32_t *)((const char *)&settings + row->off);
          ContO *model = (v >= 0 && v != CUSTOM_CAR_INDEX) ? CAR_MODEL(v) : NULL;
          if (model && model->p) draw_car_thumb(&g, &m, model, 545, 68 + i * (h + gap) + h / 2);
        }
      }
    } else if (state == STATE_BENCH_RESULT) {
      bench_result_draw(&g, &bench);
    } else if (state == STATE_PAUSED || state == STATE_CANTREPLY) {
      // PAUSE MENU -- fase -7, pausedgame() (xtGraphics.java:4695-4807),
      // and the fase -8 banner that sits on top of it.
      //
      // :4696-4698 -- `fleximg`, pauseimage()'s grey, smeared copy of the
      // frame the race was paused on, with the blue panel already tinted
      // in (see pause_image()). Without render targets there is no frame
      // to read back; redraw the frozen scene instead.
      if (pause_flex_tex >= 0) {
        gfx_draw_image(&g, pause_flex_tex, 0, 0, 800, 450);
      } else {
        draw_race_scene(&g, &m, all_objs, total_objs, visible_idx, rank, order);
      }

      // :4717-4760 -- the highlight behind the selected row,
      // fillRoundRect/drawRoundRect(...,7,20). `shaded` picks the outline colour, but
      // it is only ever set inside ctachm(), the MOUSE handler (:7395+),
      // so with no mouse it stays at its :417 false and the outline is
      // always (0,89,223).
      static const int32_t kPauseRow[4][3] = {
        {329, 45, 137}, {320, 73, 155}, {303, 99, 190}, {341, 125, 109},
      };
      // In Spanish the panel is only its frame (data/port/es/paused.png) and
      // the rows are text, drawn over it below; each highlight fits its row.
      static const char *const kPauseLabel[4] = {"Resume Game", "Instant Replay", "Game Instructions", "Quit Game"};
      const bool pause_text = i18n_lang() == I18N_LANG_ES;
      font_set(FONT_BOLD, 15);
      if (pause_opselect >= 0 && pause_opselect < 4) {
        int32_t rx = kPauseRow[pause_opselect][0];
        int32_t ry = kPauseRow[pause_opselect][1];
        int32_t rw = kPauseRow[pause_opselect][2];
        if (pause_text) {
          rw = font_width(kPauseLabel[pause_opselect]) + 36;
          rx = 400 - rw / 2;
        }
        gfx_set_color(&g, 64, 143, 223);
        gfx_fill_round_rect(&g, rx, ry, rw, 22, 7, 20);
        gfx_set_color(&g, 0, 89, 223);
        gfx_draw_round_rect(&g, rx, ry, rw, 22, 7, 20);
      }
      // :4761 -- the panel art itself, drawn OVER the highlight so its
      // labels read on top of the selected row's fill.
      draw_hud_img(&g, menu_paused, 281, 8);
      if (pause_text) {
        gfx_set_color(&g, 255, 255, 255);
        for (int32_t i = 0; i < 4; i++) draw_centered(&g, kPauseLabel[i], 400, kPauseRow[i][1] + 16);
        font_set(FONT_BOLD, 12);
        gfx_set_color(&g, 150, 190, 250);
        draw_centered(&g, "– PAUSED –", 400, 25);
        draw_centered(&g, "– PAUSED –", 400, 187);
      }
      // This port's fifth row, Settings, on its own plate under the panel.
      draw_pause_plate(&g, 320, 202, 160, 30);
      if (pause_opselect == 4) draw_pause_highlight(&g, 345, 206, 110, 22);
      draw_pause_plate(&g, 320, 236, 160, 30);
      if (pause_opselect == 5) draw_pause_highlight(&g, 345, 240, 110, 22);
      gfx_set_color(&g, 255, 255, 255);
      font_set(FONT_BOLD, 13);
      draw_centered(&g, "Settings", 400, 221);
      draw_centered(&g, "Restart Race", 400, 255);
      font_set(FONT_BOLD, 12);

      if (state == STATE_CANTREPLY) {
        // cantreply() -- :4820-4826, plus fase -8's own 150-frame
        // auto-dismiss (:1679).
        gfx_set_color(&g, 64, 143, 223);
        gfx_fill_round_rect(&g, 200, 73, 400, 23, 7, 20);
        gfx_set_color(&g, 0, 89, 223);
        gfx_draw_round_rect(&g, 200, 73, 400, 23, 7, 20);
        hud_say_draw(&g, &m, 89,
                     "Sorry not enough replay data to play available, please try again later.",
                     255, 255, 255, 1);
        cantreply_cnt++;
        if (cantreply_cnt >= 150) {
          cantreply_cnt = 0;
          state = STATE_PAUSED;
        }
      }
    } else if (state == STATE_BOOT_CLICK && !reuse_frame) {
      // clicknow() (xtGraphics.java:1443-1455) over the finished loading()
      // screen: the box redrawn over the bar, the prompt blinking.
      draw_boot_backdrop(&g, &boot_images);
      if (boot_aflk) gfx_set_color(&g, 0, 0, 0);
      else gfx_set_color(&g, 0, 67, 200);
      font_set(FONT_BOLD, 11);
      draw_centered(&g, KEY_START_PROMPT, 400, 380);
      boot_aflk = !boot_aflk;
      if (++boot_n >= 800) {
        boot_n = 0;
        state = STATE_BOOT_RAD;
      }
    } else if (state == STATE_BOOT_RAD && !reuse_frame) {
      // rad(n) (xtGraphics.java:1570-1624), 76 iterations of it (fase 9,
      // GameSparker.java:280-296): the logo parks, slides off at 40px an
      // iteration and back in, parks again with a jitter for 7 iterations.
      if (boot_n == 0) {
        if (!control[0].mutes && snd_powerup.samples) {
          audio_play(&audio, snd_powerup.samples, snd_powerup.frame_count, snd_powerup.sample_rate, 1.0f, false);
        }
        boot_radpx = 212;
        boot_pin = 0;
      }
      draw_trackbg(&g, &trackbg_state, menu_trackbg_normal, menu_trackbg_dodged, false);
      gfx_set_color(&g, 0, 0, 0);
      gfx_fill_rect(&g, 65, 135, 670, 59);
      if (menu_radicalplay.tex >= 0) {
        int32_t lx = boot_pin != 0 ? boot_radpx + jtrunc_d(8.0 * nfm_random() - 4.0) : 212;
        gfx_draw_image(&g, menu_radicalplay.tex, lx, 135, menu_radicalplay.w, menu_radicalplay.h);
      }
      if (boot_radpx != 212) {
        boot_radpx += 40;
        if (boot_radpx > 735) boot_radpx = -388;
      } else if (boot_pin != 0) {
        --boot_pin;
      }
      if (boot_n == 40) {
        boot_radpx = 213;
        boot_pin = 7;
      }
      font_set(FONT_BOLD, 11);
      if (boot_radpx == 212) {
        gfx_set_color(&g, 112, 120, 143);
        draw_centered(&g, "Radicalplay.com", 400, 185 + jtrunc(5.0f * medium_random(&m)));
      }
      if (boot_aflk) {
        gfx_set_color(&g, 112, 120, 143);
        draw_centered(&g, "And we are never going to find the new unless we get a little crazy...", 400, 215);
        boot_aflk = false;
      } else {
        gfx_set_color(&g, 150, 150, 150);
        draw_centered(&g, "And we are never going to find the new unless we get a little crazy...", 400, 217);

        boot_aflk = true;
      }
      if (menu_rpro.tex >= 0) gfx_draw_image(&g, menu_rpro.tex, 275, 265, menu_rpro.w, menu_rpro.h);
      gfx_set_color(&g, 0, 0, 0);
      gfx_fill_rect(&g, 0, 0, 65, 450);
      gfx_fill_rect(&g, 735, 0, 65, 450);
      gfx_fill_rect(&g, 65, 0, 670, 25);
      gfx_fill_rect(&g, 65, 425, 670, 25);
      if (++boot_n >= 76) {
        boot_n = 0;
        control_falseo(&control[0], 0); // :294 u[0].falseo(0)
        state = STATE_MAIN_MENU;
      }
    } else if (state == STATE_MAIN_MENU) {
      // MAIN MENU (Java fase 10 -- maini()). See draw_main_menu()'s own
      // header comment + native/docs/MENU_FLOW.md §3.2 for the 1:1 spec.
      draw_main_menu(&g,
                      menu_bgmain, menu_logomadbg, menu_logomadnes,
                      menu_dude[0], menu_logocars, menu_opback, menu_opti,
                      menu_byrd, menu_nfmcoms_asset, menu_opsettings,
                      mainbg_bgmy, &mainmenu_flkat,
                      &mainmenu_gxdu, &mainmenu_gydu, &mainmenu_movly,
                      mainmenu_opselect, &mainmenu_aflk);
    } else if (state == STATE_GAMEMODE_MENU) {
      // GAMEMODE SUBMENU (Java fase 102 -- maini2()). See
      // draw_gamemode_menu()'s own header comment + MENU_FLOW.md §3.3.
      // Shares the SAME bg-animation state as the main menu, so
      // switching between STATE_MAIN_MENU and STATE_GAMEMODE_MENU
      // doesn't restart the bgmain scroll or the dude blink.
      draw_gamemode_menu(&g,
                          menu_bgmain, menu_logomadbg, menu_logomadnes,
                          menu_dude[0], menu_logocars, menu_opback, menu_opti2,
                          menu_byrd, menu_nfmcoms_asset, menu_extlabel, menu_careerlabel,
                          mainbg_bgmy, &mainmenu_flkat,
                          &mainmenu_gxdu, &mainmenu_gydu, &mainmenu_movly,
                          gamemode_opselect, &mainmenu_aflk);
    } else if (state == STATE_INSTRUCTIONS) {
      // INSTRUCTIONS (Java fase 11 -- inst() at xtGraphics.java:4048), the
      // full 9-screen flipbook. See draw_instructions()'s own doc comment
      // for the page numbering and for why this replaced the earlier
      // single-page condensation.
      //
      // The three per-frame state updates below are Java's own, in Java's
      // own order, and deliberately live here rather than inside
      // draw_instructions(): inst() mutates them as a side effect of
      // rendering, so keeping the mutation at the call site leaves the
      // page renderer itself pure.

      // :4049-4078 -- the even-value fixups. Each paging step lands on an
      // even flipo for exactly one frame; this bumps it to the next odd
      // page and re-arms how long Coach Insano talks on it.
      if (inst_flipo == 0) inst_flipo = 1;
      else if (inst_flipo == 2)  { inst_flipo = 3;  inst_dudo = 200; }
      else if (inst_flipo == 4)  { inst_flipo = 5;  inst_dudo = 250; }
      else if (inst_flipo == 6)  { inst_flipo = 7;  inst_dudo = 200; }
      else if (inst_flipo == 8)  { inst_flipo = 9;  inst_dudo = 250; }
      else if (inst_flipo == 10) { inst_flipo = 11; inst_dudo = 200; }
      else if (inst_flipo == 12) { inst_flipo = 13; inst_dudo = 200; }
      else if (inst_flipo == 14) { inst_flipo = 15; inst_dudo = 100; }

      // :4087-4092 -- the shared frame-flip toggle, flipped once per frame
      // on this screen regardless of what uses it.
      mainmenu_aflk = !mainmenu_aflk;

      // :4094-4105 -- Coach Insano's mouth. While `dudo` is counting down
      // he is "talking", so every OTHER frame (gated on aflk) picks a new
      // mouth frame; the odd `random() > random()` test biases the pick
      // toward the first two frames roughly three times out of four.
      // Once the countdown ends he settles back to the closed mouth.
      if (inst_flipo != 1 && inst_flipo != 16) {
        if (inst_dudo > 0) {
          if (mainmenu_aflk) {
            double ra = (double)nfm_random() / (double)0xffffffffu;
            double rb = (double)nfm_random() / (double)0xffffffffu;
            double rc = (double)nfm_random() / (double)0xffffffffu;
            inst_duds = (int32_t)(rc * ((ra > rb) ? 3.0 : 2.0));
          }
          inst_dudo--;
        } else {
          inst_duds = 0;
        }
      }

      draw_instructions(&g, &inst_assets, inst_flipo, mainmenu_aflk,
                         inst_duds, inst_dudo, mainbg_bgmy, &inst_bgf);
      if (inst_flipo == GUIDE_FLIPO) draw_ext_guide(&g, &inst_assets, guide_page);
    } else if (state == STATE_CREDITS) {
      // CREDITS (Java fase 8 -- credits() at xtGraphics.java:1626).
      // Java has 3 sub-pages via flipo (1-100 rad splash, 101 credits
      // text, 102 nfmcom link). We show only the credits-text page
      // (flipo=101), the most informative one -- the rad splash is
      // pure intro animation and the nfmcom page is a clickable link
      // that doesn't apply on desktop-launched builds.
      //
      // :1639 -- credits uses mainbg(-1), the GREEN variant, not the
      // main menu's orange. See draw_mainbg_neg1's own comment.
      draw_mainbg_neg1(&g, menu_bgmain, mainbg_bgmy);

      // MADNESS wordmark at (283, 32). Java line 1640.
      if (menu_madness.tex >= 0) {
        gfx_draw_image(&g, menu_madness.tex, 283, 32, menu_madness.w, menu_madness.h);
      }

      if (credits_page > 0) {
        draw_extra_credits(&g, credits_page, menu_next, menu_back, menu_contin);
      } else {
      // Credits text -- Java :1641-1664, every line through drawcs(y,
      // text, r,g,b, 3). Mode 3 specifically SKIPS drawcs's snap[] tint
      // (:1627-1630 only tints modes other than 3/4/5), so these are the
      // raw colours, drawn centred at x=400. Strings verbatim, typos included
      // (U+2019 apostrophe as ASCII).
      font_set(FONT_BOLD, 13);
      gfx_set_color(&g, 0, 0, 0);
      draw_centered(&g, "At Radicalplay.com", 400, 90);
      draw_centered(&g, "Cartoon 3D Engine, Game Programming, 3D Models, Graphics and Sound Effects", 400, 165);
      gfx_set_color(&g, 40, 60, 0);
      draw_centered(&g, "By Omar Waly", 400, 185);
      gfx_set_color(&g, 0, 0, 0);
      draw_centered(&g, "Special Thanks!", 400, 225);
      font_set(FONT_BOLD, 11);
      gfx_set_color(&g, 66, 98, 0);
      draw_centered(&g, "Thanks to Dany Fernandez Diaz (DragShot) for imporving the game's music player to play more mod formats & effects!", 400, 245);
      draw_centered(&g, "Thanks to Badie El Zaman (Kingofspeed) for helping make the trees & cactus 3D models.", 400, 260);
      draw_centered(&g, "Thanks to Timothy Audrain Hardin (Legnak) for making hazard designs on stage parts & the new fence 3D model.", 400, 275);
      draw_centered(&g, "Thanks to Alex Miles (A-Mile) & Jaroslav Beleren (Phyrexian) for making trailer videos for the game.", 400, 290);
      draw_centered(&g, "A big thank you to everyone playing the game for sending their feedback, supporting the game and helping it improve!", 400, 305);
      font_set(FONT_BOLD, 13);
      gfx_set_color(&g, 0, 0, 0);
      draw_centered(&g, "Music from ModArchive.org", 400, 345);
      font_set(FONT_BOLD, 11);
      gfx_set_color(&g, 66, 98, 0);
      draw_centered(&g, "Most of the tracks where remixed by Omar Waly to match the game.", 400, 365);
      draw_centered(&g, "More details about the tracks and their original composers at:", 400, 380);
      gfx_set_color(&g, 33, 49, 0);
      {
        const char *link = "http://multiplayer.needformadness.com/music.html";
        const int32_t lw = font_width(link);
        draw_centered(&g, link, 400, 395);
        // :1665 -- Java underlines the link with a drawLine at y=396.
        gfx_draw_line(&g, 400 - lw / 2, 396, lw / 2 + 400, 396);
      }

      // :7543 -- the page's own advance affordance, at Java's (665,395).
      // Java hover-swaps next[0]/next[1]; with no mouse there is only
      // frame 0. The nfmcom.gif banner this port used to stack above a
      // centred CONTINUE button here is NOT drawn on this page in the
      // original -- it belongs to credits' own separate flipo==102
      // sub-page (:1674-1676), which this port doesn't show. Dropping it
      // also un-collides it from the music-source lines restored above.
      if (menu_next.tex >= 0) {
        gfx_draw_image(&g, menu_next.tex, 665, 395, menu_next.w, menu_next.h);
      }
      }   // credits_page 0
    } else if (state == STATE_POST_RACE) {
      // POST-RACE (Java fase -5 -- finish() at xtGraphics.java:6645).
      // Java's finish() would draw over the still-visible race scene at
      // low alpha (line 6647-6653 draws fleximg or a 10% black overlay).
      // We're between racing (which we've stopped ticking) and returning
      // to menu, so we don't have a live scene to overlay -- just draw
      // black and put the banner + text over it.
      gfx_set_color(&g, 0, 0, 0);
      gfx_fill_rect(&g, 0, 0, width, height);

      // Win: congrd.gif banner at (265, 87); Lose: gameov.gif at (315, 117).
      if (race_winner && menu_congrd.tex >= 0) {
        gfx_draw_image(&g, menu_congrd.tex, 265, 87, menu_congrd.w, menu_congrd.h);
      } else if (!race_winner && menu_gameov.tex >= 0) {
        gfx_draw_image(&g, menu_gameov.tex, 315, 117, menu_gameov.w, menu_gameov.h);
      }

      // Status text -- Java :6655-6683, Arial bold 11, NFM 2's stages
      // numbered from 1 again (`stage > 10 -> stage -= 10`).
      char post_line[80];
      const int32_t shown = stage_num > 10 ? stage_num - 10 : stage_num;
      font_set(FONT_BOLD, 11);
      snprintf(post_line, sizeof(post_line), "You %s!  At Stage %d!", race_winner ? "Won" : "Lost", shown);
      gfx_set_color(&g, 255, 161, 85);
      draw_centered(&g, post_line, 400, race_winner ? 137 : 167);
      gfx_set_color(&g, 255, 115, 0);
      draw_centered(&g, stage_name_buf, 400, race_winner ? 154 : 184);

      // Continue button at (355, 380) -- Java line 6993.
      HudImg confirm = (menu_contin.tex >= 0) ? menu_contin : menu_play;
      if (confirm.tex >= 0) {
        gfx_draw_image(&g, confirm.tex, 355, 380, confirm.w, confirm.h);
      }

      // Stage/car-unlock celebration overlay -- xtGraphics.java:6692-6852.
      // Gated on GameProgress::justwon1/justwon2 (progress.c's own faithful
      // port of the "did THIS race cross the campaign's unlock threshold"
      // check, set by game_progress_finish_stage() right after the race)
      // rather than re-deriving the threshold arithmetic here -- see
      // progress.h's own doc comment on why.
      //
      // The outer condition mirrors :6692's own
      // `winner && multion == 0 && gmode != 0 && (stage == unlocked[...] ||
      // stage == 27)`: multion is always 0 here, the threshold test is the
      // justwon latch, and stage 27 gets in on its OWN arm -- it is the
      // campaign finale, reached when the threshold no longer applies, so
      // it must not be filtered out by the latch. The body then splits on
      // :6778's `if (stage != 27)` exactly as the source does.
      if (ext_normal && race_winner) {
        // Extended's finish(): "Stage N+1 is now unlocked!" or, on a stage
        // already beaten, "Stage N Completed!" (XT 12448-12452).
        char line[64];
        if (ext_justwon) snprintf(line, sizeof(line), "Stage %d is now unlocked!", stage_num + 1);
        else snprintf(line, sizeof(line), "Stage %d Completed!", stage_num);
        if (ext_career && crace.bonus) snprintf(line, sizeof(line), "Bonus Stage %d Completed!", (int)crace.bonus);
        gfx_set_color(&g, xt.aflk ? 196 : 255, xt.aflk ? 176 : 247, xt.aflk ? 0 : 165);
        font_set(FONT_BOLD, 13);
        draw_centered(&g, line, 400, 260);
        for (int32_t k = 0; k < 4 && ext_career && career_new_cars[k] >= 0; k++) {
          char cl[80];
          snprintf(cl, sizeof(cl), "%s has been unlocked!", car_name(ext_car_to_port(career_new_cars[k])));
          draw_centered(&g, cl, 400, 282 + k * 18);
        }
      }
      bool just_crossed_threshold = (gmode == 1) ? progress.justwon1 : (gmode == 2 ? progress.justwon2 : false);
      if (race_winner && gmode != 0 && (just_crossed_threshold || stage_num == 27)) {
       if (stage_num != 27) {
        int32_t n4 = game_progress_bonus_car_for((GameMode)gmode, stage_num);
        int32_t pin = (n4 != 0) ? -20 : 60;

        int32_t stage_display = stage_num + 1 - (gmode - 1) * 10;
        char unlock_line[64];
        snprintf(unlock_line, sizeof(unlock_line), "Stage %d is now unlocked!", stage_display);
        gfx_set_color(&g, xt.aflk ? 196 : 255, xt.aflk ? 176 : 247, xt.aflk ? 0 : 165);
        font_set(FONT_BOLD, 13);
        draw_centered(&g, unlock_line, 400, 200 + pin);

        if (n4 != 0) {
          gfx_set_color(&g, xt.aflk ? 196 : 255, xt.aflk ? 176 : 247, xt.aflk ? 0 : 165);
          draw_centered(&g, "And:", 400, 200);

          // Card background wash -- Java re-rolls this random 50/50 EVERY
          // draw call (not just once), which is what makes the card
          // visibly flicker/shimmer rather than settle into a fixed look.
          if (nfm_random() > 0.5) {
            gfx_set_color(&g, 236, 226, 202);
            gfx_set_composite(&g, 0.5f);
            gfx_fill_rect(&g, 226, 211, 344, 125);
            gfx_set_composite(&g, 1.0f);
          }
          gfx_set_color(&g, 0, 0, 0);
          gfx_fill_rect(&g, 226, 211, 348, 4);
          gfx_fill_rect(&g, 226, 211, 4, 125);
          gfx_fill_rect(&g, 226, 332, 348, 4);
          gfx_fill_rect(&g, 570, 211, 4, 125);

          // Live 3D render of the newly-unlocked car inside the card --
          // same shared-base-model-mutate-then-cont_o_d pattern as
          // draw_car_preview() (see its own comment), with finish()'s OWN
          // camera constants (m.x/y/z/ground) instead of car-select's.
          if (n4 >= 0 && n4 < GAME_SPARKER_NUM_BASE_MODELS && base_models[n4].p) {
            ContO *unlocked_car = CAR_MODEL(n4);
            m.crs = true;
            m.x = -400;
            m.y = 0;
            m.z = -50;
            m.xz = 0;
            m.zy = 0;
            m.ground = 2470;
            m.focus_point = 400;
            m.cx = 400;
            m.cy = 225;
            m.cz = 50;
            m.iw = 0;
            m.ih = 0;
            m.w = 800;
            m.h = 450;
            unlocked_car->y = post_race_unlock_car_y(n4);
            unlocked_car->z = 1000;
            unlocked_car->x = 0;
            post_unlock_car_xz = (post_unlock_car_xz + 5) % 360;
            unlocked_car->xz = post_unlock_car_xz;
            unlocked_car->zy = 0;
            post_unlock_car_wzy = (post_unlock_car_wzy - 10) % 360;
            unlocked_car->wzy = post_unlock_car_wzy;
            bool had_shadow = unlocked_car->shadow;
            unlocked_car->shadow = false; // floating card thumbnail, not track-seated -- see draw_car_preview()
            nfm_set_draw_phase(true);
            cont_o_d(unlocked_car, &g);
            nfm_set_draw_phase(false);
            unlocked_car->shadow = had_shadow;
          }

          // Reflection-lines overlay -- also re-rolled every draw call.
          if (nfm_random() < 0.5) {
            gfx_set_composite(&g, 0.4f);
            gfx_set_color(&g, 236, 226, 202);
            for (int32_t i = 0; i < 30; i++) {
              gfx_draw_line(&g, 230, 215 + 4 * i, 569, 215 + 4 * i);
            }
            gfx_set_composite(&g, 1.0f);
          }

          char car_unlock_line[64];
          snprintf(car_unlock_line, sizeof(car_unlock_line), "%s has been unlocked!",
                   car_name(n4));
          gfx_set_color(&g, xt.aflk ? 196 : 255, xt.aflk ? 176 : 247, xt.aflk ? 0 : 165);
          draw_centered(&g, car_unlock_line, 400, 320);
          pin = 140;
        }

        gfx_set_color(&g, 230, 167, 0);
        font_set(FONT_BOLD, 11);
        draw_centered(&g, "GAME SAVED", 400, 220 + pin);
        // :6846-6851's `pin = (pin == 60) ? 30 : 0` fixup is deliberately
        // omitted: `this.pin` is reassigned to 60 unconditionally at :6697
        // on the next draw, before either reader (:6782, :6845) runs, so
        // the fixup can never be observed. Dead in the original too.
       } else {
        // :6853-6906 -- the CAMPAIGN-COMPLETION card, shown instead of the
        // ordinary unlock celebration when the stage just beaten is 27 (the
        // finale both campaigns funnel into -- see progress.c's own NFM1
        // 11 -> 27 remap). Five flashing lines around a Radicalplay logo
        // that slides in, parks, and slides away again on a 70-draw cycle.
        //
        // This whole branch was previously unimplemented, on the stated
        // grounds that the logo image was absent from this port's asset
        // set. That was wrong -- radicalplay.gif is present in
        // data/images.zip (139 entries) and now loads with the other menu
        // art -- so beating either campaign showed nothing at all where the
        // original shows this card.
        char done_line[64];
        snprintf(done_line, sizeof(done_line), "Woohoooo you finished NFM%d !!!", gmode);
        gfx_set_color(&g, xt.aflk ? 144 : 228, xt.aflk ? 167 : 240, 255);
        font_set(FONT_BOLD, 13);
        draw_centered(&g, done_line, 400, 180);

        // :6863/:6866 -- the two flash halves genuinely disagree about y
        // (210 vs 212), so the line jitters two pixels as it blinks. A real
        // quirk of the original, preserved rather than levelled to one y.
        gfx_set_color(&g, xt.aflk ? 144 : 228, xt.aflk ? 167 : 240, 255);
        draw_centered(&g, "You're Awesome!", 400, xt.aflk ? 210 : 212);

        // :6869/:6872 -- unlike its neighbours, this line's non-flash
        // colour is red (255,100,100), not the pale blue.
        if (xt.aflk) gfx_set_color(&g, 144, 167, 255);
        else gfx_set_color(&g, 255, 100, 100);
        draw_centered(&g, "You're truly a RADICAL GAMER!", 400, 240);

        // :6874-6876 -- black band, then the logo drawn over it with a
        // +-4px per-draw jitter (`(int)(8.0 * Math.random() - 4.0)`, a
        // double computation truncated toward zero, hence jtrunc_d on
        // nfm_random() -- Math.random(), not medium.random()).
        gfx_set_color(&g, 0, 0, 0);
        gfx_fill_rect(&g, 0, 255, 800, 62);
        if (menu_radicalplay.tex >= 0) {
          int32_t jitter = jtrunc_d(8.0 * nfm_random() - 4.0);
          gfx_draw_image(&g, menu_radicalplay.tex, post_radpx + jitter, 255,
                         menu_radicalplay.w, menu_radicalplay.h);
        }
        // :6877-6889 -- the slide/park cycle. See post_radpx's own decl.
        if (post_radpx != 212) {
          post_radpx += 40;
          if (post_radpx > 800) post_radpx = -468;
        }
        if (post_flipo == 40) post_radpx = 213;
        post_flipo++;
        if (post_flipo == 70) post_flipo = 0;
        // :6890-6899 -- only while the logo is parked, and at 11pt rather
        // than 13. The setFont sticks: "Now get up and dance!" below is in
        // 11 too while the logo is parked, 13 otherwise -- as in Java.
        if (post_radpx == 212) {
          gfx_set_color(&g, xt.aflk ? 144 : 228, xt.aflk ? 167 : 240, 255);
          font_set(FONT_BOLD, 11);
          draw_centered(&g, "A Game by Radicalplay.com", 400, 309);
        }
        gfx_set_color(&g, xt.aflk ? 144 : 228, xt.aflk ? 167 : 240, 255);
        draw_centered(&g, "Now get up and dance!", 400, 350);

       }

        // :6908-6912 -- one aflk flip per draw, shared by BOTH branches
        // above (it sits outside the stage-27 split but inside the outer
        // `if`), which is what makes every line on either card blink in
        // lockstep.
        xt.aflk = !xt.aflk;
      }
    } else if (state == STATE_CAR_SELECT && !reuse_frame) {
      // CAR SELECT (Java fase 7 -- carselect() at xtGraphics.java:5080).
      // See native/docs/MENU_FLOW.md §3.6 for the pixel-exact spec.
      // Layout order: black letterbox borders (65px each side), carsbg
      // (cars.gif) at (65,25), selectcar.gif caption at (321,37), 3D
      // spinning car (Java lines 5273-5297 place it at x=0, z=950,
      // y=-34-grat), car-name text centered at y=95 via drawcs, then
      // navigation: back at (95,275), next at (645,275), contin at
      // (355,385).
      //
      // 1. Letterbox borders. Java xtGraphics.java:5081-5085.
      gfx_set_color(&g, 0, 0, 0);
      gfx_fill_rect(&g, 0, 0, 65, height);
      gfx_fill_rect(&g, width - 65, 0, 65, height);
      gfx_fill_rect(&g, 65, 0, 670, 25);
      gfx_fill_rect(&g, 65, height - 25, 670, 25);

      // 2. Backdrop -- cars.gif (carsbg in Java) at (65, 25), but only
      // once the smoke-warp entrance (see CarSmokeWarp's own doc comment)
      // has settled. xtGraphics.java:5086-5102's own three-way dispatch:
      if (car_select_needs_intro) {
        car_smoke_warp_enter(&car_smoke_warp, &m);
        car_smoke_warp.flatrstart = 0;
        car_select_needs_intro = false;
        // GameSparker.java:323 -- `this.mvect = 50;` fires at the exact
        // same "-9 -> 7" transition as inishcarselect() (the smoke-warp's
        // own arming above), and fase==7's own block never resets it, so
        // car-select keeps a persistent 50%-opacity motion-blur/ghosting
        // trail (see the blit-time mvect comment near the render-target
        // composite below) for as long as the screen is showing -- not
        // the "no blur" every other menu gets.
        mvect = 50;
      }
      if (car_smoke_warp.flatrstart == 6 || car_smoke_warp.flexpix_tex < 0) {
        // Settled (or the warp failed to load at all) -- plain static
        // backdrop. Falls back to bggo.jpg then flat dark grey if carsbg
        // itself failed to load.
        if (menu_carsbg.tex >= 0) {
          gfx_draw_image(&g, menu_carsbg.tex, 65, 25, menu_carsbg.w, menu_carsbg.h);
        } else if (menu_bg.tex >= 0) {
          gfx_draw_image(&g, menu_bg.tex, 0, 0, width, height);
        } else {
          gfx_set_color(&g, 20, 20, 30);
          gfx_fill_rect(&g, 65, 25, 670, 400);
        }
      } else if (car_smoke_warp.flatrstart <= 1) {
        car_smoke_warp_step(&car_smoke_warp, &m, &g);
      } else {
        // xtGraphics.java:5097-5101 -- a single white-flash transition
        // frame between the warp animation (flatrstart 0-1) and settling
        // permanently at 6 (flatrstart increments inside
        // car_smoke_warp_step, so this branch is only ever reached for
        // exactly one frame per car-select entry).
        gfx_set_color(&g, 255, 255, 255);
        gfx_fill_rect(&g, 65, 25, 670, 400);
        car_smoke_warp_enter(&car_smoke_warp, &m);
        car_smoke_warp.flatrstart = 6;
      }

      // 3. selectcar.gif caption at (321, 37). Java line 5103.
      if (menu_selectcar.tex >= 0) {
        gfx_draw_image(&g, menu_selectcar.tex, 321, 37, menu_selectcar.w, menu_selectcar.h);
      } else {
        gfx_set_color(&g, 255, 220, 80);
        font_set(FONT_BOLD, 13);
        draw_centered(&g, "Select your car", 400, 50);
      }

      // 4. Live 3D car preview. draw_car_preview() already applies the
      // Java carselect() camera constants (xtGraphics.java:5058-5073)
      // AND the car placement constants (x=0, z=950/1000, y=-34-grat,
      // xz+=5, wzy-=10) -- see its own header comment. Multiplayer
      // color-tinting block (Java 5175-5225) skipped, only matters
      // when other players' cars are visible on the picker.
      //
      // Car-switch transition -- xtGraphics.java:6335-6425's `flipo`
      // state machine: while car_flipo > 10 the OLD car falls away
      // tumbling (y-=100, zy+=/-20 per frame, direction from car_nextc);
      // at exactly car_flipo==10 the next/previous car index is picked
      // (landing on locked cars too, see below) and placed 1100 units
      // above its resting height; then while car_flipo is 10..1 the NEW
      // car rises back in (y+=100/frame). Idle xz (yaw)/wzy (wheel) spin
      // freezes for the whole 20-frame transition, matching Java (those
      // only advance in its own `flipo == 0` branch).
      int32_t car_y_offset = 0, car_zy_value = 0;
      bool car_freeze_spin = false;
      if (car_flipo > 0) {
        car_freeze_spin = true;
        car_gatey = 300; // xtGraphics.java:6364 -- reset every frame while mid-transition
        if (car_flipo > 10) {
          car_transition_y -= 100;
          car_transition_zy += car_nextc * 20;
        } else {
          if (car_flipo == 10) {
            GameMode car_gm = (GameMode)gmode;
            // xtGraphics.java:5306-5365 -- LEFT/RIGHT lands on ANY car,
            // locked ones included (the locked-car gate is a purely
            // visual overlay drawn once landed there, see the gate/wave
            // draw block below), so this does NOT skip cars
            // game_progress_can_pick_car() rejects. The ONE real skip is
            // the custom "Simple_Car.rad" slot outside Free Play -- a
            // different, login-gated "Private Car" restriction in Java
            // (xtGraphics.java:6349) that's out of scope for this
            // single-player port (see game_progress_can_pick_car's own
            // doc comment) and was never reachable here to begin with.
            // Plain +-1 now that the arming check above clamps the range
            // (see its own comment): Java's own crossover just moves one
            // slot, and the end-of-list case can no longer reach here.
            // The clamp already keeps NFM1/NFM2 below the custom slot, so
            // the old wrap-and-skip loop is no longer needed; the bound is
            // recomputed here rather than shared because the two sites run
            // in different phases of the frame.
            int32_t car_maxsl = (car_gm == GMODE_FREE_PLAY) ? (ext_career ? CAR_COUNT - 1 : car_last_index(ext_normal)) : 15;
            const int32_t next = car_step(car_index, car_nextc > 0 ? 1 : -1, car_maxsl, car_filter, ext_normal);
            if (next >= 0) car_index = next;
            if (car_index < 0) car_index = 0;
            if (car_index > car_maxsl) car_index = car_maxsl;
            car_transition_y = -1100;
            car_transition_zy = 0;
          }
          car_transition_y += 100;
        }
        car_y_offset = car_transition_y;
        car_zy_value = car_transition_zy;
        car_flipo--;
      }
      {
        ContO *preview_base = (car_index == CUSTOM_CAR_INDEX) ? &car_base : CAR_MODEL(car_index);
        draw_car_preview(&g, &m, preview_base, &car_preview_xz, &car_preview_wzy,
                          car_y_offset, car_zy_value, car_freeze_spin);
      }

      // 5. Car-name text, :5233-5236 + :5260-5267. Both branches draw at
      // Java's y=95; the aflk (bright, 240,240,240) branch adds `n8`,
      // which is 2 ONLY while `flatrstart < 6`, i.e. only during the
      // smoke-warp entrance -- once that settles, n8 is 0 and the name
      // sits perfectly still, alternating brightness alone.
      //
      // This port had a PERMANENT 2px bob (y=90 vs y=92) and put the
      // offset on the dim branch rather than the bright one, so the name
      // jittered forever instead of only while the screen was animating
      // in.
      font_set(FONT_BOLD, 13); // :5231
      {
        const char *name = (car_index == CUSTOM_CAR_INDEX) ? "Simple Car (custom)" : car_name(car_index);
        int32_t name_n8 = (car_smoke_warp.flatrstart < 6) ? 2 : 0;
        if (mainmenu_aflk) {
          gfx_set_color(&g, 240, 240, 240);
          draw_centered(&g, name, 400, 95 + name_n8);
          mainmenu_aflk = false;
        } else {
          gfx_set_color(&g, 176, 176, 176);
          draw_centered(&g, name, 400, 95);
          mainmenu_aflk = true;
        }
        // Extended's credit over the name (XT 15341-15398), its modes only.
        const char *by = (ext_normal || gmode == GMODE_FREE_PLAY) ? car_author(car_index) : NULL;
        if (by) {
          char line[64];
          snprintf(line, sizeof(line), "Created by %s", by);
          // (Extended draws it at 60 over its name at 95; this port's title sits
          // lower, so between the two.)
          font_set(FONT_BOLD, 12);
          gfx_set_color(&g, 246, 246, 246);
          draw_centered(&g, line, 400, 74);
          font_set(FONT_BOLD, 13);
        }
      }

      // 5.5. Locked-car gate overlay -- xtGraphics.java:5306-5365. Drawn
      // OVER the car/name/stats above whenever the browsed slot is
      // locked: a 9-segment fence arch that slides down (car_gatey 300->0)
      // then plays a left-to-right wave bounce, plus "[ Car Locked ]" and
      // the unlock-condition text. Skipped for the custom car slot (its
      // own, unrelated "Private Car" restriction is out of scope, see
      // game_progress_can_pick_car's doc comment).
      if (ext_career && career_car_lock(&csave, ext_car_of(car_index)) == 0) {
        // The car's career: level, experience, unspent points.
        const int32_t ec = ext_car_of(car_index);
        // Right of the special's box, under the name.
        char line[96];
        snprintf(line, sizeof(line), "Level %d    XP %d / %d", (int)csave.level[ec], (int)csave.exp[ec],
                 (int)career_reqneed(csave.level[ec], ec));
        font_set(FONT_BOLD, 12);
        gfx_set_color(&g, 255, 196, 0);
        if (!career_panel) draw_centered(&g, line, 575, 116);
        snprintf(line, sizeof(line), "Stat points: %d", (int)csave.statpoints[ec]);
        if (!career_panel) draw_centered(&g, line, 575, 132);
        if (!career_panel && career_transfer_from < 0) {
          char hint[64];
          snprintf(hint, sizeof(hint), "Press %s to upgrade", KEY_SPECIAL);
          gfx_set_color(&g, 200, 200, 200);
          font_set(FONT_BOLD, 11);
          draw_centered(&g, hint, 575, 147);
        } else if (career_panel) {
          draw_career_panel(&g, &csave, ec, career_panel_page, career_panel_row,
                            career_panel_page < 2 ? career_pend[career_panel_page] : NULL);
        }
      }
      if (ext_career && career_transfer_from >= 0 && career_confirm == 0) {
        // shufflefase 5's box (XT 16438-16473): which car takes the transfer.
        const int32_t ec = ext_car_of(car_index);
        const bool here = ec != career_transfer_from && career_car_lock(&csave, ec) == 0;
        char l2[64];
        snprintf(l2, sizeof(l2), "%s?", car_name(car_index));
        const char *lines[2] = {here ? "transfer to" : "select car to", here ? l2 : "transfer to..."};
        gfx_set_composite(&g, 0.92f);
        gfx_set_color(&g, 255, 255, 255);
        gfx_fill_rect(&g, 290, 100, 220, 74);
        gfx_set_composite(&g, 1.0f);
        font_set(FONT_BOLD, 16);
        gfx_set_color(&g, 0, 0, 0);
        draw_centered(&g, lines[0], 400, 122);
        draw_centered(&g, lines[1], 400, 142);
        char hint[96];
        if (here) snprintf(hint, sizeof(hint), "%s transfer here    %s cancel", KEY_CONTINUE, KEY_BACK);
        else snprintf(hint, sizeof(hint), "Left / Right: a car    %s cancel", KEY_BACK);
        font_set(FONT_BOLD, 11);
        gfx_set_color(&g, 90, 90, 90);
        draw_centered(&g, hint, 400, 164);
      }
      if (ext_career && career_confirm != 0) {
        draw_career_confirm(&g, &csave, ext_car_of(car_index), career_transfer_from, career_confirm, career_confirm_yes);
      }
      if (car_index != CUSTOM_CAR_INDEX) {
        int32_t unlock_k = ext_career ? career_car_lock(&csave, ext_car_of(car_index))
                                      : game_progress_car_unlock_stage(&progress, (GameMode)gmode, car_index);
        if (unlock_k != 0) {
          if (car_gatey == 300) {
            for (int32_t seg = 0; seg < 9; seg++) { car_pgas[seg] = false; car_pgady[seg] = 0; }
            car_pgas[0] = true;
          }
          for (int32_t seg = 0; seg < 9; seg++) {
            if (menu_pgate.tex >= 0) {
              gfx_draw_image(&g, menu_pgate.tex, kCarGatePgatx[seg],
                              kCarGatePgaty[seg] + car_pgady[seg] - car_gatey, menu_pgate.w, menu_pgate.h);
            }
            if (car_pgas[seg]) {
              car_pgady[seg] -= (80 + 100 / (seg + 1) - abs(car_pgady[seg])) / 3;
              if (car_pgady[seg] < -(70 + 100 / (seg + 1))) {
                car_pgas[seg] = false;
                if (seg != 8) car_pgas[seg + 1] = true;
              }
            } else {
              car_pgady[seg] += (80 + 100 / (seg + 1) - abs(car_pgady[seg])) / 3;
              if (car_pgady[seg] > 0) car_pgady[seg] = 0;
            }
          }
          if (car_gatey != 0) car_gatey -= 100;

          // :5362-5363, both in the Arial bold 13 set above.
          gfx_set_color(&g, 210, 210, 210);
          draw_centered(&g, "[ Car Locked ]", 400, 355);
          char unlock_msg[96];
          snprintf(unlock_msg, sizeof(unlock_msg), "This car unlocks when stage %d is completed...", unlock_k);
          if (unlock_k < 0) {
            // The bonus stages' prizes (XT 15438-15462).
            const int32_t ecar = ext_car_of(car_index);
            if (ecar == 35 && csave.boncomp[1] == 2)
              snprintf(unlock_msg, sizeof(unlock_msg), "Win bonus stage 2 by racing to unlock this car...");
            else if (ecar == 36 && csave.boncomp[1] == 1)
              snprintf(unlock_msg, sizeof(unlock_msg), "Win bonus stage 2 by wasting to unlock this car...");
            else
              snprintf(unlock_msg, sizeof(unlock_msg), "Win bonus stage %d to unlock this car...", (int)-unlock_k);
          }
          if (unlock_k == 99) snprintf(unlock_msg, sizeof(unlock_msg), "A secret car...");
          gfx_set_color(&g, 255, 96, 0);
          draw_centered(&g, unlock_msg, 400, 375);
        }
      }

      // A locked car keeps its stats to itself.
      const bool car_locked =
          car_index != CUSTOM_CAR_INDEX &&
          (ext_career ? career_car_lock(&csave, ext_car_of(car_index))
                      : game_progress_car_unlock_stage(&progress, (GameMode)gmode, car_index)) != 0;
      // 6. Six stat bars (Top Speed / Acceleration / Handling / Stunts
      // / Strength / Endurance) in 2 columns of 3 rows. Java line
      // 6094-6139. Each bar is 156px wide, drawn statb (coloured bg) ->
      // black rect covering the "unfilled" fraction -> statbo (outline)
      // on top. Labels in Arial Bold 11, colour (181,120,40).
      //
      // Uses CarDefine's own stat fields (already ported) and the exact
      // formulas from Java xtGraphics.java:6110-6133, also documented
      // in web/preview.js's carStats() function. Per-stat comment
      // explains each formula's origin.
      //
      // Only meaningful when a base model was loaded (car_index < 16
      // has real CarDefine data; CUSTOM_CAR_INDEX uses car_base which
      // has its own loaded stats at slot CUSTOM_CAR_INDEX in cd).
      {
        int32_t cn = car_index; // CarDefine slot index for this car
        // (swits[cn][2] - 220) / 90, clamp >= 0.2 -- top speed baseline.
        float n19 = ((float)cd.swits[cn][2] - 220.0f) / 90.0f;
        if (n19 < 0.2f) n19 = 0.2f;
        // acelf[1]*[0]*[2]*grip / 7700, clamp <= 1 -- effective launch
        // acceleration (product of the three acceleration tuning
        // factors times grip).
        float n20 = cd.acelf[cn][1] * cd.acelf[cn][0] * cd.acelf[cn][2] * cd.grip[cn] / 7700.0f;
        if (n20 > 1.0f) n20 = 1.0f;
        // dishandle -- already in [0.25, 1.0], displayed as-is.
        float n21 = cd.dishandle[cn];
        // (airc*airs*bounce + 28) / 139, clamp <= 1 -- stunts
        // (air-control * air-spin * bounce, biased by 28 so lightweight
        // cars still show a nonzero bar).
        float n22 = ((float)cd.airc[cn] * cd.airs[cn] * cd.bounce[cn] + 28.0f) / 139.0f;
        if (n22 > 1.0f) n22 = 1.0f;
        // (moment + 0.5) / 2.6 -- strength (mass moment biased by 0.5,
        // normalised so 2.1 = full bar).
        float n23 = (cd.moment[cn] + 0.5f) / 2.6f;
        if (n23 > 1.0f) n23 = 1.0f;
        // outdam -- already in [0.35, 1.0], displayed as-is.
        float n24 = cd.outdam[cn];

        struct StatBar { const char *label; int32_t label_x, y; float fraction; };
        const struct StatBar bars[6] = {
          { "Top Speed:",    98, 343, n19 },  // left col, row 0 (bar bg at 162,337)
          { "Acceleration:", 88, 358, n20 },  // left col, row 1 (bar bg at 162,352)
          { "Handling:",    110, 373, n21 },  // left col, row 2 (bar bg at 162,367)
          { "Stunts:",      495, 343, n22 },  // right col, row 0 (bar bg at 536,337)
          { "Strength:",    483, 358, n23 },  // right col, row 1 (bar bg at 536,352)
          { "Endurance:",   473, 373, n24 },  // right col, row 2 (bar bg at 536,367)
        };
        // The career: Extended's numbered bars, the car's points in.
        const bool ext_bars = ext_career && !car_locked && car_index != CUSTOM_CAR_INDEX &&
                              ext_car_of(car_index) >= 0 && ext_car_of(car_index) < CAREER_CARS;
        if (ext_bars) draw_ext_stat_bars(&g, &cd, cn, csave.sp[ext_car_of(car_index)], 344);
        for (int32_t i = 0; i < 6 && !car_locked && !ext_bars; i++) {
          const struct StatBar *sb = &bars[i];
          int32_t bar_x = (i < 3) ? 162 : 536;
          int32_t bar_y = sb->y - 6; // Java's label baseline is 6px below the bar's top

          // Label -- Arial bold 11 (:6094) in the copper colour Java uses.
          gfx_set_color(&g, 181, 120, 40);
          font_set(FONT_BOLD, 11);
          // A translated label can be longer than the gap before its bar:
          // right-aligned to the bar instead (the original's own x in English).
          const int32_t label_x = i18n_lang() == I18N_LANG_EN ? sb->label_x : bar_x - 5 - font_width(sb->label);
          font_draw(&g, sb->label, label_x, sb->y);

          // Coloured bar background (statb, full 156x7).
          if (menu_statb.tex >= 0) {
            gfx_draw_image(&g, menu_statb.tex, bar_x, bar_y, menu_statb.w, menu_statb.h);
          } else {
            gfx_set_color(&g, 90, 90, 90);
            gfx_fill_rect(&g, bar_x, bar_y, 156, 7);
          }

          // Black rectangle covering the unfilled fraction on the right.
          // Java: fillRect((int)(base + 156*f), y, (int)(156*(1-f) + 1), 7).
          int32_t fill_x = bar_x + (int32_t)(156.0f * sb->fraction);
          int32_t unfilled_w = (int32_t)(156.0f * (1.0f - sb->fraction) + 1.0f);
          if (unfilled_w > 0) {
            gfx_set_color(&g, 0, 0, 0);
            gfx_fill_rect(&g, fill_x, bar_y, unfilled_w, 7);
          }

          // Outline/border overlay on top (statbo).
          if (menu_statbo.tex >= 0) {
            gfx_draw_image(&g, menu_statbo.tex, bar_x, bar_y, menu_statbo.w, menu_statbo.h);
          }
        }

        // 7. Class label -- Java lines 6140-6166. Shown as "Class A" .. "Class C"
        // depending on cd->cclass. Java uses a color pulse via `kbload`
        // counter; here we simply show it in the copper colour used by
        // stat labels. Java only shows this if `multion != 0 || testdrive`
        // is set -- both false here in single-player Free Play, so
        // strictly Java wouldn't draw this on a stock single-player
        // Free Play carselect either. We show it anyway because the
        // class is useful info for the player choosing a car.
        const char *class_str;
        switch (cd.cclass[cn]) {
          case 4: class_str = "Class A"; break;
          case 3: class_str = "Class A & B"; break;
          case 2: class_str = "Class B"; break;
          case 1: class_str = "Class B & C"; break;
          default: class_str = "Class C"; break;
        }
        gfx_set_color(&g, 176, 41, 0);
        font_set(FONT_BOLD, 13);
        // Extended's own cars carry no NFM 2 class.
        if (cn < 16 || cn >= CUSTOM_CAR_INDEX) font_draw(&g, class_str, 549 - font_width(class_str) / 2, 95);

        // Extended's "SPECIAL ATTACK:" panel (its carselect, xtGraphics.java:
        // 16120-16330), top left: a white tab sized to the lines. Moved by
        // (65, 27) into this screen's letterboxed 670x400 interior and
        // under the title.
        const char *const *spec = specials_describe(cn);
        int32_t lines = 0;
        while (lines < 4 && spec[lines]) lines++;
        if (lines > 0) {
          const int32_t dx = 65, dy = 27;
          const int32_t px[8] = {10 + dx, 20 + dx, 280 + dx, 290 + dx, 290 + dx, 280 + dx, 20 + dx, 10 + dx};
          const int32_t py[8] = {43 + dy, 28 + dy, 28 + dy, 43 + dy, 43 + dy + lines * 15, 58 + dy + lines * 15,
                                 58 + dy + lines * 15, 43 + dy + lines * 15};
          gfx_set_composite(&g, 210.0f / 255.0f);
          gfx_set_color(&g, 255, 255, 255);
          gfx_fill_polygon(&g, px, py, 8);
          gfx_set_composite(&g, 1.0f);
          gfx_set_color(&g, 30, 30, 30);
          font_set(FONT_BOLD, 13);
          draw_centered(&g, "SPECIAL ATTACK:", 140 + dx, 43 + dy);
          font_set(FONT_BOLD, 11);
          for (int32_t li = 0; li < lines; li++) font_draw(&g, spec[li], 20 + dx, 60 + dy + 15 * li);
        }
      }

      // 8. Navigation buttons -- :5298-5305. `back` at (95,275) is drawn
      // only when `sc[0] != minsl`, `next` at (645,275) only when
      // `sc[0] != maxsl`, so each arrow DISAPPEARS at its end of the
      // list -- the visual half of the same clamp the input handler
      // enforces (see its own comment for why the list clamps rather
      // than wraps, and where maxsl's value comes from). This port used
      // to draw both unconditionally, which advertised a move that the
      // (then-wrapping) list would make in the wrong direction.
      {
        int32_t car_maxsl = (gmode == GMODE_FREE_PLAY) ? (ext_career ? CAR_COUNT - 1 : car_last_index(ext_normal)) : 15;
        if (menu_back.tex >= 0 && car_step(car_index, -1, car_maxsl, car_filter, ext_normal) >= 0) {
          gfx_draw_image(&g, menu_back.tex, 95, 275, menu_back.w, menu_back.h);
        }
        if (menu_next.tex >= 0 && car_step(car_index, 1, car_maxsl, car_filter, ext_normal) >= 0) {
          gfx_draw_image(&g, menu_next.tex, 645, 275, menu_next.w, menu_next.h);
        }
        int32_t filters[8];
        if (!career_panel && car_filters(ext_career, gmode, filters) > 1) {
          // The filter, under the car's name.
          char line[64];
          snprintf(line, sizeof(line), "< %s >", kCarFilterName[car_filter]);
          font_set(FONT_BOLD, 12);
          gfx_set_color(&g, 255, 196, 0);
          draw_centered(&g, line, 400, 114);
        }
      }
      // continue.gif (Java's `contin[0]`) at (355, 385). Java line 6316.
      // This is the primary "advance to stage select" button. Falls back
      // to play.gif if continue.gif failed to load.
      HudImg confirm = (menu_contin.tex >= 0) ? menu_contin : menu_play;
      if (confirm.tex >= 0) {
        gfx_draw_image(&g, confirm.tex, 355, 385, confirm.w, confirm.h);
      }
    } else if (state == STATE_STAGE_SELECT && !reuse_frame) {
      // STAGE SELECT (Java fase 6's stageselect() UI, xtGraphics.java:2024,
      // COMPOSITED OVER a live 3D preview -- that preview isn't inside
      // stageselect() itself, it's GameSparker.java's own run() loop
      // (fase==1 block, :453-497) drawing the track UNDERNEATH before
      // calling stageselect(). Confirmed by reading run()'s fase dispatch
      // directly: fase==2 (one tick earlier) calls loadstage(), which arms
      // medium.trx/trz/hit/... and sets fase back to 1 (:2735-2749) --
      // read the very next frame by the fase==1 block below it in the SAME
      // sequential-if dispatch (GameSparker's fases aren't mutually
      // exclusive within one tick), which calls medium.aroundtrack() and
      // depth-sort-draws the stage every frame before stageselect() draws
      // its own frame/caption/name/arrows on top. See
      // native/docs/MENU_FLOW.md §3.7 for the pixel-exact 2D spec.
      //
      // Layout order (Java lines 2025-2034 for the shell, 2211-2226 for
      // the aflk-pulsed stage name):
      //   1. Black letterbox (65px each side + top/bottom 25px)
      //   2. Live 3D stage preview (GameSparker.java:466-497,
      //      Medium.aroundtrack()'s dive-and-orbit camera) filling the
      //      670x400 interior
      //   3. br.png torn-paper backdrop at (65, 25) -- 670x400, drawn ON
      //      TOP of the preview so its opaque edges crop it to the
      //      torn-paper "window" (same order the Java itself draws in:
      //      scene first, stageselect()'s own br.png second)
      //   4. select.gif ("SELECT") caption at (338, 35)
      //   5. Stage name centred at y=132 with aflk pulse
      //      (240,240,240)/(176,176,176), Arial Bold 13
      //   6. back.gif at (115, 135) if stage > 1
      //   7. next.gif at (625, 135) if stage < NUM_STAGES
      //   8. continue.gif at (355, 385) for confirm

      // 1. Letterbox.
      gfx_set_color(&g, 0, 0, 0);
      gfx_fill_rect(&g, 0, 0, 65, height);
      gfx_fill_rect(&g, width - 65, 0, 65, height);
      gfx_fill_rect(&g, 65, 0, 670, 25);
      gfx_fill_rect(&g, 65, height - 25, 670, 25);

      // Load the stage now (feeds the 3D preview below, and used by the
      // confirm transition to STATE_RACING -- pre-parsing while the
      // player browses makes that transition instant, and lets a stage
      // that fails to load get caught here instead of at racing time).
      // On a fresh load, also (re)arm the preview camera exactly like
      // GameSparker.java:2735-2749's `xtGraphics.fase == 2` block: dive
      // in from hit=45000, orbiting the stage's bounding-box center.
      if (stage_preview_loaded_num != stage_num) {
        int32_t center_x = 0, center_z = 0;
        bool loaded;
        const int32_t fp_ext = gmode == GMODE_FREE_PLAY ? free_play_ext_stage(stage_num) : 0;
        if (ext_normal || fp_ext) {
          char spec[32];
          ext_stage_spec(fp_ext ? fp_ext : stage_num, fp_ext ? 1 : ptmatch, spec, sizeof(spec));
          ExtStageInfo info;
          loaded = load_ext_stage_objects(&stage_objects, &stage_count, stage_count, ext_models, &m, &t, &cp, spec,
                                          ext_car_of(car_index), &info);
          center_x = info.center_x;
          center_z = info.center_z;
        } else {
          loaded = load_stage_objects(&stage_objects, &stage_count, stage_count, base_models, &m, &t, &cp, stage_num,
                                      &center_x, &center_z);
        }
        stage_preview_loaded_num = stage_num;
        stage_preview_ok = loaded;
        if (loaded) {
          m.trx = center_x;
          m.trz = center_z;
          m.ptr = 0;
          m.ptcnt = -10;
          m.hit = 45000;
          m.fallen = 0;
          m.nrnd = 0;
          m.trk = 1;
          m.ih = 25;
          m.iw = 65;
          m.h = 425;
          m.w = 735;
        }
        // GameSparker.java:452's fase==2 block ends with `this.mvect =
        // 20;` right before flipping to fase==1 -- re-armed every time
        // loadstage() runs (both the FIRST stage this screen shows, and
        // every subsequent stage the player browses to), matching this
        // block's own re-trigger condition exactly (`stage_preview_
        // loaded_num != stage_num`, one loadstage() call per highlight
        // change, same as the Java's own fase==2 re-entry per stage).
        mvect = 20;
      }

      // 2a. Beige backdrop -- Java's hipnoload() draws this BEFORE
      // calling stageselect() (xtGraphics.java:2718). Color derived from
      // the stage's snap[] but for a menu preview with snap=(0,0,0) it
      // resolves to (230,230,230). Left as a fallback under the 3D
      // preview for a stage that fails to load (stage_count == 0 below).
      gfx_set_color(&g, 230, 230, 230);
      gfx_fill_rect(&g, 65, 25, 670, 400);

      // 2b. Semi-transparent bggo.jpg overlay -- Java hipnoload() line
      // 2720 (drawImage(bggo, 0, -25) at 30% alpha). Adds a bit of
      // photo-noise texture over the flat beige. Java draws this
      // unconditionally too -- GameSparker's own fase==1 preview draw
      // (below) paints over most of it, same as here.
      if (menu_bg.tex >= 0) {
        gfx_set_composite(&g, 0.3f);
        gfx_draw_image(&g, menu_bg.tex, 0, -25, menu_bg.w, menu_bg.h);
        gfx_set_composite(&g, 1.0f);
      }

      // 2c. Live 3D preview -- ticks the dive-and-orbit camera and depth-
      // sort draws the loaded stage's geometry, the SAME painter's-
      // algorithm the racing loop further below uses. Guarded on
      // stage_count so a stage that failed to load just shows the flat
      // beige backdrop above instead of a crash or garbage draw.
      if (stage_count > 0 && stage_preview_ok) {
        medium_aroundtrack(&m, &cp);
        // GameSparker.java:466-467 -- once the dive settles (hit reaches
        // its 5000 cruise floor), the motion-blur trail slowly fades out
        // (mvect creeps from the 20 armed above up toward 40, 1/frame,
        // capped there) rather than vanishing -- the preview keeps a
        // persistent partial ghosting the whole time it's on screen,
        // never the full "no blur" every other menu gets.
        if (m.hit == 5000 && mvect < 40) mvect++;
        nfm_set_draw_phase(true); // cont_o_d may draw dust/sparks -- see main()'s racing-draw comment
        render_sorted_objects(stage_objects, stage_count, &g);
        nfm_set_draw_phase(false);
      }

      if (!stage_preview_ok) {
        gfx_set_color(&g, 0, 0, 0);
        font_set(FONT_BOLD, 13);
        draw_centered(&g, "Failed to load stage...", 400, 220);
      }

      // 3. br.png torn-paper frame on top -- opaque black edges crop the
      // 3D preview (or the flat beige backdrop) to the center "window".
      if (menu_br.tex >= 0) {
        gfx_draw_image(&g, menu_br.tex, 65, 25, menu_br.w, menu_br.h);
      }

      // 4. "SELECT" caption at (338, 35).
      if (menu_select.tex >= 0) {
        gfx_draw_image(&g, menu_select.tex, 338, 35, menu_select.w, menu_select.h);
      } else {
        gfx_set_color(&g, 0, 0, 0);
        font_set(FONT_BOLD, 13);
        draw_centered(&g, "Select", 400, 50);
      }

      // 5. Stage name centered at y=132, aflk-pulsed grey -- Java lines
      // 2219-2226. Uses the SAME shared mainmenu_aflk toggle that the
      // menu screens use, matching Java (all screens mutate this.aflk).
      // The "N#{nto}" top20 prefix (Java 2217) doesn't apply here (no
      // top20 tracking); the "Failed to load stage" branch (Java 2245)
      // is handled by our stage_read_name fallback that returns
      // "Stage N" for a missing stage.
      {
        // :2215-2226 draws the stage NAME alone (its only prefix is the
        // top20 "N#" one, which needs online score data this port has
        // no equivalent for). The "N/32" counter this port prepended is
        // its own invention, and it advertised a range that does not
        // exist: every navigation bound in the original is 27, and
        // :1899-1901 explicitly bounces any stage above 27 back into
        // 1..27, so stages 28-32 -- which DO ship as files -- are never
        // reachable through this picker.
        const char *label = stage_name_buf;
        // Java pulses (240,240,240)/(176,176,176), i.e. near-white on
        // what it expects to be a dark backdrop. Kept as dark greys here
        // because this port's preview reads much lighter through the
        // mvect=20 motion-blur accumulation, where near-white text would
        // vanish. (The original comment blamed a "beige hipnoload"
        // backdrop that this screen no longer uses -- the adaptation is
        // still right, the reason was stale.)
        if (mainmenu_aflk) {
          gfx_set_color(&g, 60, 60, 60);
        } else {
          gfx_set_color(&g, 20, 20, 20);
        }
        font_set(FONT_BOLD, 13); // :2211
        draw_centered(&g, label, 400, 132);
      }

      // 6-7. Nav arrows at Java's own y=135, now gated exactly as
      // :2029-2034 does -- back hidden at the first stage of the current
      // mode (1, or 11 in NFM2), next hidden at 27. These match the LEFT/
      // RIGHT guards in the input block one-for-one, so an arrow is on
      // screen if and only if that direction actually moves. The port
      // used to draw both unconditionally because its nav wrapped.
      if (menu_back.tex >= 0 && stage_num != 1
          && (stage_num != 11 || (GameMode)gmode != GMODE_NFM2)) {
        gfx_draw_image(&g, menu_back.tex, 115, 135, menu_back.w, menu_back.h);
      }
      if (menu_next.tex >= 0 &&
          stage_num != (ext_normal ? EXT_LAST_STAGE : gmode == GMODE_FREE_PLAY ? FREE_PLAY_LAST_STAGE : 27)) {
        gfx_draw_image(&g, menu_next.tex, 625, 135, menu_next.w, menu_next.h);
      }
      if (gmode == GMODE_FREE_PLAY && !ext_normal) {
        // Free Play's Race Setup at a glance, and the button that opens it.
        static const char *const kTierShort[5] = {"Any cars", "Classic", "Extended", "R&R", "Custom"};
        static const char *const kWinShort[3] = {"Race or Waste", "Waste only", "Race only"};
        char sl[128];
        // Built from parts, each translated (its format too: i18n_port_es.c).
        int32_t len = snprintf(sl, sizeof(sl), tr(settings.fp_opponents == 0 ? "%d rival: %s  -  %s" : "%d rivals: %s  -  %s"),
                               (int)settings.fp_opponents + 1,
                               tr(settings.fp_pick ? "Manual" : kTierShort[settings.fp_tier]), tr(kWinShort[settings.fp_win]));
        if (settings.fp_laps > 0) len += snprintf(sl + len, sizeof(sl) - (size_t)len, tr("  -  %d laps"), (int)settings.fp_laps);
        if (!settings.fp_arrow) len += snprintf(sl + len, sizeof(sl) - (size_t)len, "%s", tr("  -  no arrow"));
        if (!settings.fp_specials) snprintf(sl + len, sizeof(sl) - (size_t)len, "%s", tr("  -  no specials"));
        gfx_set_composite(&g, 0.6f);
        gfx_set_color(&g, 0, 0, 0);
        gfx_fill_rect(&g, 180, 326, 440, 40);
        gfx_set_composite(&g, 1.0f);
        font_set(FONT_BOLD, 12);
        gfx_set_color(&g, 230, 230, 230);
        draw_centered(&g, sl, 400, 340);
        char hl[48];
        snprintf(hl, sizeof(hl), "%s  Race Setup", KEY_VIEW);
        gfx_set_color(&g, 255, 196, 0);
        draw_centered(&g, hl, 400, 358);
      }

      if (ext_career && stage_num <= csave.unlocked && ext_car_of(car_index) >= 0 &&
          ext_car_of(car_index) < CAREER_CARS) {
        // What the race pays: the field's level and the experience
        // (career_stage_info), so a stage that gives none is known before.
        CareerStageInfo si;
        const bool modes_ok = stage_num < csave.unlocked && csave.unlocked >= 3;
        career_stage_info(&csave, ext_car_of(car_index), stage_num, modes_ok ? career_mode : 0, &si);
        char il[128];
        // Built from parts, each translated (its format too: i18n_port_es.c).
        int32_t len = snprintf(il, sizeof(il), tr("Stage level %d (you %d)  -  "), (int)si.level,
                               (int)csave.level[ext_car_of(car_index)]);
        if (si.noexp)
          snprintf(il + len, sizeof(il) - (size_t)len, tr("NO XP (%s)"), tr(si.capped ? "level cap reached" : "no levels"));
        else if (si.expmult < 1.0)
          snprintf(il + len, sizeof(il) - (size_t)len, tr("XP -%d%% (car too strong)%s"), (int)((1.0 - si.expmult) * 100.0 + 0.5),
                   si.nobsp ? tr("  -  no bonus stat points") : "");
        else
          snprintf(il + len, sizeof(il) - (size_t)len, tr("Full XP%s"),
                   si.nobsp ? tr("  -  no bonus stat points") : si.hard ? tr("  -  tough field: more bonus points") : "");
        gfx_set_composite(&g, 0.6f);
        gfx_set_color(&g, 0, 0, 0);
        gfx_fill_rect(&g, 180, 300, 440, 22);
        gfx_set_composite(&g, 1.0f);
        font_set(FONT_BOLD, 13);
        if (si.noexp) gfx_set_color(&g, 255, 90, 90);
        else if (si.expmult < 1.0) gfx_set_color(&g, 255, 170, 0);
        else gfx_set_color(&g, 120, 230, 120);
        draw_centered(&g, il, 400, 315);
      }
      if (ext_career && stage_num < csave.unlocked && csave.unlocked >= 3) {
        static const char *const kModes[4] = {"Normal", "Hard mode", "Scale levels", "No levels"};
        char ml[64];
        snprintf(ml, sizeof(ml), "Mode: %s   (Up / Down)", kModes[career_mode]);
        gfx_set_composite(&g, 0.6f);
        gfx_set_color(&g, 0, 0, 0);
        gfx_fill_rect(&g, 250, 326, 300, 22);
        gfx_set_composite(&g, 1.0f);
        font_set(FONT_BOLD, 13);
        gfx_set_color(&g, career_mode ? 255 : 220, career_mode ? 196 : 220, career_mode ? 0 : 220);
        draw_centered(&g, ml, 400, 340);
      }
      if (ext_career && stage_num <= EXT_UNLOCKED &&
          (stage_num == 5 || stage_num == 11 || stage_num == 15 || stage_num == 18)) {
        char bl[64];
        snprintf(bl, sizeof(bl), "Press %s for the BONUS STAGE!", KEY_SPECIAL);
        gfx_set_composite(&g, 0.6f);
        gfx_set_color(&g, 255, 255, 255);
        gfx_fill_rect(&g, 250, 352, 300, 24);
        gfx_set_composite(&g, 1.0f);
        gfx_set_color(&g, 0, 0, 0);
        font_set(FONT_BOLD, 13);
        draw_centered(&g, bl, 400, 369);
      }
      // 8. Confirm button (continue.gif) at (355, 385).
      HudImg confirm = (menu_contin.tex >= 0) ? menu_contin : menu_play;
      if (confirm.tex >= 0) {
        gfx_draw_image(&g, confirm.tex, 355, 385, confirm.w, confirm.h);
      }

      // If the player has ever entered this stage-select session, tick the
      // shared aflk pulse used by other menus for the stage-name colour --
      // see mainmenu_aflk's declaration comment. Java shares the flag; here
      // we mirror by driving it here too so a lone STAGE_SELECT frame
      // (screenshot hook) still shows both colour phases.
      mainmenu_aflk = !mainmenu_aflk;
    } else if (state == STATE_STAGE_LOCKED) {
      // Java cantgo() -- xtGraphics.java:1993. Uses the same trackbg
      // scrolling backdrop the loading screen uses, then draws br.png +
      // select.gif on top, then 9x pgate padlock glyphs and a pulsing
      // "Stage N Locked" label. Any key or the timer bounces back to
      // stage-select (handled in the input block above).
      draw_trackbg(&g, &trackbg_state, menu_trackbg_normal, menu_trackbg_dodged, /*force_normal=*/false);
      // Java :1996 -- br on top of trackbg. br.png's interior is
      // transparent, so the animated track still shows through the
      // middle of the torn-paper frame (that's the whole visual point).
      if (menu_br.tex >= 0) {
        gfx_draw_image(&g, menu_br.tex, 65, 25, menu_br.w, menu_br.h);
      }
      if (menu_select.tex >= 0) {
        gfx_draw_image(&g, menu_select.tex, 338, 35, menu_select.w, menu_select.h);
      }
      // Java :2001-2003 -- 9 padlock glyphs spaced 30px apart across the
      // frame, from x=277. If pgate.gif is missing, fall back to plain
      // filled squares so the "row of locks" reads visually.
      for (int32_t i = 0; i < 9; i++) {
        int32_t px = 277 + i * 30;
        if (menu_pgate.tex >= 0) {
          gfx_draw_image(&g, menu_pgate.tex, px, 215, menu_pgate.w, menu_pgate.h);
        } else {
          gfx_set_color(&g, 30, 30, 30);
          gfx_fill_rect(&g, px, 215, 20, 20);
        }
      }
      // Java :1998-2000 -- Arial Bold 13 dark grey: "This stage will be
      // unlocked when stage N is complete!" where N = unlocked[gmode-1].
      {
        char msg[128];
        int32_t nfrom = ext_normal ? EXT_UNLOCKED : (gmode == 1) ? progress.unlocked[0] : progress.unlocked[1];
        snprintf(msg, sizeof(msg), "This stage will be unlocked when stage %d is complete!", nfrom);
        // :2000 -- (177,177,177), a LIGHT grey that reads against the dark
        // track backdrop. This port used (60,60,60), which is dark on dark.
        gfx_set_color(&g, 177, 177, 177);
        font_set(FONT_BOLD, 13);
        draw_centered(&g, msg, 400, 130);
      }
      // Java :2006-2013 -- Arial Bold 12 pulsing red/orange:
      // "[ Stage N+1 Locked ]" (colour toggles via aflk each draw).
      {
        char lbl[64];
        int32_t nto;
        if (ext_normal) nto = EXT_UNLOCKED + 1;
        else if (gmode == 1) nto = progress.unlocked[0] + 1;
        else if (gmode == 2) nto = progress.unlocked[1] + 1;
        else nto = stage_num; // shouldn't happen (Free Play never locks)
        snprintf(lbl, sizeof(lbl), "[ Stage %d Locked ]", nto);
        if (mainmenu_aflk) gfx_set_color(&g, 255, 128, 0);
        else gfx_set_color(&g, 255, 0, 0);
        font_set(FONT_BOLD, 12);
        draw_centered(&g, lbl, 400, 185);
        mainmenu_aflk = !mainmenu_aflk;
      }
      // Java :2014 -- back button at (370, 345) to bail out early.
      if (menu_back.tex >= 0) {
        gfx_draw_image(&g, menu_back.tex, 370, 345, menu_back.w, menu_back.h);
      }
    } else if (state == STATE_STAGE_LOADING) {
      // Java loadingstage() -- xtGraphics.java:1971. Animated trackbg
      // + br.png + select.gif + a rounded beige "Loading, please wait..."
      // panel. We use plain fillRect for the panel since there's no
      // rounded-rect primitive in gfx.h.
      draw_trackbg(&g, &trackbg_state, menu_trackbg_normal, menu_trackbg_dodged, /*force_normal=*/true);
      if (menu_br.tex >= 0) {
        gfx_draw_image(&g, menu_br.tex, 65, 25, menu_br.w, menu_br.h);
      }
      // Java :1974-1977 -- (212,214,138) filled + (57,64,8) outlined
      // rounded-rect at (265,201,270,26). No rounded-rect primitive here,
      // so a plain rect is drawn with the same fill colour and a 1px
      // dark border via draw_rect.
      gfx_set_color(&g, 212, 214, 138);
      gfx_fill_rect(&g, 265, 201, 270, 26);
      gfx_set_color(&g, 57, 64, 8);
      gfx_draw_rect(&g, 265, 201, 270, 26);
      // Java :1980 -- (58, 61, 17) "Loading, please wait..." centred at y=219.
      gfx_set_color(&g, 58, 61, 17);
      font_set(FONT_BOLD, 12); // :1978 -- and the race inherits it
      draw_centered(&g, "Loading, please wait...", 400, 219);
      // Java :1982 -- select caption on top when the transition is fresh.
      if (menu_select.tex >= 0) {
        gfx_draw_image(&g, menu_select.tex, 338, 35, menu_select.w, menu_select.h);
      }
    } else if (state == STATE_STAGE_INTRO) {
      // xtGraphics.hipnoload() (:2685-2933). It draws straight to the
      // display every frame (no trail, mvect = 100), so its per-draw state
      // -- the coach's flicker, the hint's red flash, the start button's
      // blink -- only advances on the screen's own 40ms paced step.
      const bool step = paced_ticked;
      int32_t a[3];
      {
        const int32_t snap[3] = {m.snap[0], m.snap[1], m.snap[2]};
        opsnap_lift(snap, a);
      }
      gfx_set_color(&g, clamp255((int32_t)(230.0f - 230.0f * ((float)a[0] / 100.0f))),
                    clamp255((int32_t)(230.0f - 230.0f * ((float)a[1] / 100.0f))),
                    clamp255((int32_t)(230.0f - 230.0f * ((float)a[2] / 100.0f))));
      gfx_fill_rect(&g, 65, 25, 670, 400);
      if (menu_bg.tex >= 0) {
        gfx_set_composite(&g, 0.3f);
        gfx_draw_image(&g, menu_bg.tex, 0, -25, menu_bg.w, menu_bg.h);
        gfx_set_composite(&g, 1.0f);
      }
      gfx_set_color(&g, 0, 0, 0);
      gfx_fill_rect(&g, 0, 0, 65, 450);
      gfx_fill_rect(&g, 735, 0, 65, 450);
      gfx_fill_rect(&g, 65, 0, 670, 25);
      gfx_fill_rect(&g, 65, 425, 670, 25);
      font_set(FONT_BOLD, 13);
      draw_centered(&g, stage_asay, 400, 50);
      // Single player only: the stages with a hint lift the card to n3 = 0.
      static const char *const kHints[28][5] = {
          [1] = {"Hey!  Don't forget, to complete a lap you must pass through", "all checkpoints in the track!"},
          [2] = {"Remember, the more power you have the faster your car will be!"},
          [3] = {"> Hint: its easier to waste the other cars then to race in this stage!",
                 "%s to make the guidance arrow point to cars instead of to", "the track."},
          [4] = {"Remember, the better the stunt you perform the more power you get!"},
          [5] = {"Remember, the more power you have the stronger your car is!"},
          [10] = {"NOTE: Guidance Arrow is disabled in this stage!"},
          [11] = {"Hey!  Don't forget, to complete a lap you must pass through", "all checkpoints in the track!"},
          [12] = {"Remember, the more power you have the faster your car will be!"},
          [13] = {"Watch out!  Look out!  The policeman might be out to get you!",
                  "Don't upset him or you'll be arrested!", NULL, "Better run, run, run."},
          [14] = {"Don't waste your time.  Waste them instead!", "Try a taste of sweet revenge here (if you can)!", NULL,
                  "%s to make the guidance arrow point to cars instead of to", "the track."},
          [17] = {"Welcome to the realm of the king...", NULL,
                  "The key word here is 'POWER'.  The more you have of it the faster", "and STRONGER you car will be!"},
          [18] = {"Watch out, EL KING is out to get you now!", "He seems to be seeking revenge?", NULL,
                  "(To fly longer distances in the air try drifting your car on the ramp", "before take off)."},
          [19] = {"It's good to be the king!"},
          [20] = {"Remember, forward loops give your car a push forwards in the air", "and help in racing.", NULL,
                  "(You may need to do more forward loops here.  Also try keeping",
                  "your power at maximum at all times.  Try not to miss a ramp)."},
          [22] = {"Watch out!  Beware!  Take care!", "MASHEEN is hiding out there some where, don't get mashed now!"},
          [23] = {"Anyone for a game of Digger?!", "You can have fun using MASHEEN here!"},
          [26] = {"This is it!  This is the toughest stage in the game!", NULL,
                  "This track is actually a 4D object projected onto the 3D world.",
                  "It's been broken down, separated and, in many ways, it is also a", "maze!  GOOD LUCK!"},
      };
      const bool has_hint = !ext_normal && stage_num >= 1 && stage_num < 28 && kHints[stage_num][0] != NULL;
      const int32_t n3 = has_hint ? 0 : -90;
      if (has_hint) {
        if (intro_dudo > 0) {
          if (step) {
            if (intro_aflk) {
              double ra = nfm_random();
              double rb = nfm_random();
              intro_duds = (int32_t)(nfm_random() * ((ra > rb) ? 3.0 : 2.0));
              intro_aflk = false;
            } else {
              intro_aflk = true;
            }
            intro_dudo--;
          }
        } else {
          intro_duds = 0;
        }
        if (menu_dude[intro_duds].tex >= 0) {
          gfx_set_composite(&g, 0.3f);
          gfx_draw_image(&g, menu_dude[intro_duds].tex, 95, 35, menu_dude[intro_duds].w, menu_dude[intro_duds].h);
        }
        if (intro_flaot.tex >= 0) {
          gfx_set_composite(&g, 0.7f);
          gfx_draw_image(&g, intro_flaot.tex, 192, 67, intro_flaot.w, intro_flaot.h);
        }
        gfx_set_composite(&g, 1.0f);
        const int32_t r2 = clamp255((int32_t)(80.0f - 80.0f * ((float)a[0] / 100.0f)));
        const int32_t g2 = clamp255((int32_t)(80.0f - 80.0f * ((float)a[1] / 100.0f)));
        const int32_t b2 = clamp255((int32_t)(80.0f - 80.0f * ((float)a[2] / 100.0f)));
        gfx_set_color(&g, r2, g2, b2);
        if (stage_num == 10) {
          // The one warning flashes red, toggling every draw.
          if (intro_tflk) gfx_set_color(&g, 200, g2, b2);
          if (step) intro_tflk = !intro_tflk;
        }
        for (int32_t line = 0; line < 5; line++) {
          const char *txt = kHints[stage_num][line];
          if (txt) font_drawf(&g, 262, 92 + 20 * line, txt, KEY_ARRACE_HINT);
        }
      }
      if (intro_loadingmusic.tex >= 0) {
        gfx_set_composite(&g, 0.8f);
        gfx_draw_image(&g, intro_loadingmusic.tex, 289, 205 + n3, intro_loadingmusic.w, intro_loadingmusic.h);
        gfx_set_composite(&g, 1.0f);
      }
      gfx_set_color(&g, 0, 0, 0);
      font_set(FONT_BOLD, 11); // :2906
      if (intro_frame == 0) {
        // loadmusic()'s card: the track's download size, from sndsize[].
        static const int32_t kSndSize[33] = {39, 128, 23, 58, 106, 140, 81, 135, 38, 141, 106,
                                             76, 56, 116, 92, 208, 70, 80, 152, 102, 27, 65,
                                             52, 30, 151, 129, 80, 44, 57, 123, 202, 210, 111};
        int32_t n4 = stage_num - 1;
        if (n4 < 0 || n4 > 32) n4 = 32;
        char kb[24];
        snprintf(kb, sizeof(kb), "%d KB", kSndSize[n4]);
        draw_centered(&g, kb, 400, 340 + n3);
        draw_centered(&g, " Please Wait...", 400, 375 + n3);
      } else {
        draw_centered(&g, "Loading complete!  Press Start to begin...", 400, 365 + n3);

        if (intro_star[intro_pstar].tex >= 0) {
          gfx_set_composite(&g, 0.5f);
          gfx_draw_image(&g, intro_star[intro_pstar].tex, 359, 385 + n3,
                         intro_star[intro_pstar].w, intro_star[intro_pstar].h);
          gfx_set_composite(&g, 1.0f);
        }
        if (step) intro_pstar ^= 1;
      }
      if (ext_career && career_scout && intro_frame >= 1) {
        // The beast / shadow / undead header's flash (XT 9964-9980), on
        // this screen's paced step.
        if (step) {
          if (career_scout_flash < 20) career_scout_up = true;
          if (career_scout_flash > 240) career_scout_up = false;
          career_scout_flash += career_scout_up ? 12 : -12;
        }
        int32_t page = career_scout_page;
        if (page < career_scout_first(&crace, &csave)) page = career_scout_first(&crace, &csave);
        draw_career_scout(&g, &crace, &csave, &cd, page, career_scout_flash);
      }
      if (step) intro_frame++;
    }

    DIAG_PHASE("frame: submit to GL");
    prof_submit_start = platform_ticks_us();
    gfx_submit_gl(&g);

    // GameSparker.java's own paint() (decompilation/java-src/
    // GameSparker.java:1798-1861) -- see gfx_gl_render_target_blit's own
    // doc comment (gfx_gl.h) for the full mechanism this ports straight
    // from the Java (never in web/*.js -- see that comment). Blits the
    // scene just rendered above onto the window WITHOUT clearing it
    // first, at less-than-full alpha and (while `shaka` is counting
    // down) a small random jitter -- letting the previous frame's
    // pixels partially show through where this one doesn't fully cover
    // or blend them out is the entire "motion blur"/shake-ghosting
    // effect. `state != STATE_RACING` mirrors the many OTHER
    // `this.mvect = 100`/`this.shaka` resets scattered across every
    // non-gameplay Java `fase` this port doesn't otherwise model (chat,
    // lobby, connecting, ...) -- all of them reset to the same "no
    // effect" defaults, so collapsing them into one blanket reset
    // outside STATE_RACING matches every reachable one exactly. EXCEPT
    // car-select and stage-select: unlike every other menu, Java's own
    // fase==7 (carselect, mvect=50 -- GameSparker.java:323) and fase==1
    // (stageselect's 3D preview, mvect held at 20 then creeping to 40 --
    // GameSparker.java:452,466-467) genuinely keep a persistent partial
    // motion-blur/ghosting trail rather than resetting to "no effect"
    // like fase 10/102/-5/etc. do -- those two states arm/advance mvect
    // themselves (see car_select_needs_intro's own block and the stage
    // preview's `mvect = 20` / `hit == 5000` sites), so this blanket
    // reset must not clobber it back to 100 every frame. `shaka` (the
    // damage-shake jitter) has no such exception -- it's racing-only in
    // the Java too, so resetting it everywhere else is always correct.
    if (state != STATE_RACING) {
      shaka = 0;
      // STATE_STAGE_LOCKED (cantgo) also gets no reset: Java's fase==4
      // has no mvect assignment either, so it inherits whatever
      // stage-select (the only state that reaches it) already set.
      if (state != STATE_CAR_SELECT && state != STATE_STAGE_SELECT && state != STATE_STAGE_LOCKED) {
        mvect = 100;
      }
    }
    if (reuse_frame) {
      // The picture last drawn is still the frame to show: present it again,
      // at the shake offset it was presented with, without re-blending.
      gfx_gl_render_target_bind(NULL);
      int32_t disp_w, disp_h;
      platform_display_size(&disp_w, &disp_h);
      glViewport(0, 0, disp_w, disp_h);
      glClear(GL_COLOR_BUFFER_BIT);
      glDisable(GL_BLEND);
      if (letterboxed) {
        gfx_gl_render_target_blit_region(rt_pair[rt_cur ^ 1], 65.0f, 25.0f, 670.0f, 400.0f,
                                         race_present_dx, race_present_dy, 1.0f);
      } else {
        gfx_gl_render_target_blit(rt_pair[rt_cur ^ 1], race_present_dx, race_present_dy, 1.0f);
      }
      glEnable(GL_BLEND);
    } else if (use_rt) {
      float offset_x = 0.0f, offset_y = 0.0f;
      if (smooth_between) {
        // The shake steps once per tick (and its randoms are the race's).
        offset_x = race_present_dx;
        offset_y = race_present_dy;
      } else if (shaka > 0) {
        offset_x = (float)(int32_t)(shaka * 2.0 * nfm_random() - shaka);
        offset_y = (float)(int32_t)(shaka * 2.0 * nfm_random() - shaka);
        shaka--;
        // Settings > Screen Shake off: no jitter -- but the two randoms are
        // still drawn, so the sequence the rest of the race reads from stays
        // the original's whatever the setting.
        if (!settings.shake) offset_x = offset_y = 0.0f;
      }
      // The trail: paint() blits each new frame over the last one at
      // alpha mvect/100, i.e. history = scene*a + history*(1-a). With the
      // targets ping-ponged that is the previous frame's finished picture
      // laid over this frame's scene at (1-a) -- the same value, written
      // straight into the target that is presented next, instead of
      // copying the scene into a separate history texture first (one
      // full-screen pass and one target switch per frame fewer). Alpha
      // writes are masked off so the history keeps the scene's own alpha
      // and the blend weight cannot drift frame to frame. mvect == 100,
      // or the first frame after a stretch without a trail (the other
      // target is stale), takes the scene as is.
      // The Settings screen's Motion Blur scales the history's weight:
      // 100 keeps the original's 1 - mvect/100, 0 removes the trail (and
      // with it this pass).
      const int32_t mvect_eff = 100 - (100 - mvect) * settings.blur / 100;
      const bool trail_active = (mvect_eff < 100);
      if (trail_active && accum_valid) {
        // The trail decays by its weight once per TICK; drawing every frame,
        // each frame takes its share of that, by its length.
        float keep = 1.0f - (float)mvect_eff / 100.0f;
        if (smooth_on) keep = powf(keep, (float)(race_frame_ms / TICK_MS));
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
        gfx_gl_render_target_blit(rt_pair[rt_cur ^ 1], 0.0f, 0.0f, keep);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
      }
      accum_valid = trail_active;

      gfx_gl_render_target_bind(NULL); // back to the default (window) framebuffer
      // The physical display resolution, not the logical 800x450: on the
      // Vita the screen is 960x544, and this viewport is what stretches
      // the finished frame over all of it (the blit's quad stays in the
      // fixed 0..800/0..450 projection).
      int32_t disp_w, disp_h;
      platform_display_size(&disp_w, &disp_h);
      glViewport(0, 0, disp_w, disp_h);
      // Cleared: the shake offset can slide the quad far enough to leave a
      // strip along one edge uncovered, and on the Vita that strip would
      // show an unrelated older display buffer.
      glClear(GL_COLOR_BUFFER_BIT);
      // Blending OFF: this is "present the finished picture", not a
      // composite, and must not be modulated by the target's alpha.
      glDisable(GL_BLEND);
      // Menus live in the original's 670x400 letterbox at (65,25), so for
      // them only that rectangle is copied, stretched to the full screen.
      if (letterboxed) {
        gfx_gl_render_target_blit_region(rt_pair[rt_cur], 65.0f, 25.0f, 670.0f, 400.0f,
                                         offset_x, offset_y, 1.0f);
      } else {
        gfx_gl_render_target_blit(rt_pair[rt_cur], offset_x, offset_y, 1.0f);
      }
      glEnable(GL_BLEND);
      rt_cur ^= 1;
      picture_state = paced_state ? (int32_t)render_state : -1;
      race_present_dx = offset_x;
      race_present_dy = offset_y;
    } else {
      // Drawn straight to the display: nothing to present, and no history.
      accum_valid = false;
      picture_state = -1;
      if (letterboxed) set_game_projection(0.0, (double)width, (double)height, 0.0);
    }


    // FPS overlay: Settings > Interface > Show FPS -- FPS is the frame rate,
    // DETAILED adds the worst frame and where a frame's time went (the
    // measuring always runs, it is a few clock reads). Drawn HERE, after the composite blit above and
    // in its own 2D pass, rather than alongside the HUD: everything
    // submitted before that blit goes through the motion-blur render
    // target and comes out ghosted, and a smeared number is useless for
    // reading a frame rate off a handheld screen.
    {
      static uint32_t fps_window_start = 0; // start of the current 1s sample
      static uint32_t fps_prev_frame = 0;   // timestamp of the previous frame
      static int32_t fps_frames = 0;        // frames counted in this window
      static int32_t fps_value = 0;         // frames in the LAST full window
      static int32_t fps_worst = 0;         // slowest single frame, ms

      uint32_t now = platform_ticks_ms();
      if (fps_prev_frame != 0) {
        int32_t dt = (int32_t)(now - fps_prev_frame);
        if (dt > fps_worst) fps_worst = dt;
      }
      fps_prev_frame = now;
      if (fps_window_start == 0) fps_window_start = now;
      fps_frames++;
      uint64_t prof_now = platform_ticks_us();
      prof_sum[0] += prof_render_start - prof_frame_start;
      prof_sum[1] += prof_submit_start - prof_render_start;
      prof_sum[2] += prof_now - prof_submit_start;
      // prof_sum[3] (swap) is added after platform_swap_buffers below.

      // The Performance Test: one record per raced frame from the green
      // light (the swap is filled in after the swap), then the report.
      if (bench.active && render_state == STATE_RACING) {
        if (!bench.measuring && starcnt == 0) {
          bench.measuring = true;
          bench.start_us = prof_now;
          bench.last_us = 0;
        }
        if (bench.measuring) {
          if (bench.last_us != 0 && bench.n < BENCH_MAX_FRAMES) {
            BenchFrame *fr = &bench.f[bench.n++];
            fr->interval_us = (uint32_t)(prof_now - bench.last_us);
            fr->update_us = (uint32_t)(prof_render_start - prof_frame_start);
            fr->draw_us = (uint32_t)(prof_submit_start - prof_render_start);
            fr->submit_us = (uint32_t)(prof_now - prof_submit_start);
            fr->swap_us = 0;
            fr->objs = g.objDrawn;
            fr->faces = g.faceCalls;
            fr->verts = g.count;
          }
          bench.last_us = prof_now;
          if (prof_now - bench.start_us >= (uint64_t)bench_seconds * 1000000u || bench.n >= BENCH_MAX_FRAMES ||
              race_holdit) {
            bench.ended_early = race_holdit;
            fprintf(stderr, "bench end: holdit=%d kind=%d wasted=%d dest0=%d clear0=%d n=%d\n", (int)race_holdit,
                    (int)race_end_kind, cp.wasted, (int)mad[0].dest, cp.clear[0], bench.n);
            int32_t bdw, bdh;
            platform_display_size(&bdw, &bdh);
            bench_report(&bench, &settings, bdw, bdh, scene_rt.px_w, scene_rt.px_h, stage_name_buf,
                         progress_path_ok ? progress_path : NULL);
            bench.active = false;
            stop_all_sfx_loops(&audio, engine_channel, &last_engine_bank, &air_channel, &wasted_channel);
            audio_stop_music(&audio);
            state = STATE_BENCH_RESULT;
          }
        }
      } else if (bench.active) {
        bench.last_us = 0; // paused: the gap is not a frame
      }
      if (bench.active && render_state == STATE_RACING) {
        // What is running, and for how long yet.
        int32_t bdw, bdh;
        platform_display_size(&bdw, &bdh);
        glViewport(0, 0, bdw, bdh);
        gfx_begin(&g);
        char btxt[48];
        if (bench.measuring) {
          snprintf(btxt, sizeof(btxt), "PERFORMANCE TEST  %d S",
                   bench_seconds - (int32_t)((prof_now - bench.start_us) / 1000000u));
        } else {
          snprintf(btxt, sizeof(btxt), bench.stress ? "STRESS TEST" : "PERFORMANCE TEST");
        }
        font_set(FONT_BOLD, 13);
        const int32_t tw = font_width(btxt);
        gfx_set_color(&g, 0, 0, 0);
        gfx_fill_rect(&g, 400 - tw / 2 - 8, 62, tw + 16, 20);
        gfx_set_color(&g, 255, 255, 0);
        font_draw(&g, btxt, 400 - tw / 2, 77);
        gfx_submit_gl(&g);
      }
      if (now - fps_window_start >= 1000) {
        for (int32_t k = 0; k < 4; k++) {
          prof_avg_tenths[k] = (int32_t)(prof_sum[k] / (uint64_t)(100 * fps_frames));
          prof_sum[k] = 0;
        }
        fps_value = fps_frames;
        fps_frames = 0;
        fps_window_start = now;
        fps_worst = 0; // spike window resets with the average it sits next to
      }

      if (settings.show_fps == 1) {
        // The frame rate alone, top right, clear of the race HUD's panels.
        int32_t fps_dw, fps_dh;
        platform_display_size(&fps_dw, &fps_dh);
        glViewport(0, 0, fps_dw, fps_dh);
        gfx_begin(&g);
        char fps_txt[24];
        snprintf(fps_txt, sizeof(fps_txt), "%d FPS", fps_value);
        font_set(FONT_BOLD, 13);
        const int32_t tw = font_width(fps_txt);
        gfx_set_color(&g, 0, 0, 0);
        gfx_fill_rect(&g, 800 - tw - 18, 428, tw + 12, 18);
        gfx_set_color(&g, 255, 255, 0);
        font_draw(&g, fps_txt, 800 - tw - 12, 442);
        gfx_submit_gl(&g);
      } else if (settings.show_fps == 2) {
      // Same viewport the composite blit uses, so the overlay lands in
      // the same place whether or not the blur path ran this frame.
      int32_t fps_dw, fps_dh;
      platform_display_size(&fps_dw, &fps_dh);
      glViewport(0, 0, fps_dw, fps_dh);
      gfx_begin(&g);
      // Frames-per-second, then the slowest
      // single frame in ms since the last update. The worst-frame figure
      // is the point -- an average alone hides exactly the stalls that
      // make a screen feel bad while still reporting a healthy number.
      char fps_buf[32];
      snprintf(fps_buf, sizeof(fps_buf), "%d/%d", fps_value, fps_worst);
      gfx_set_color(&g, 255, 255, 0);
      // Bottom-left, clear of the race HUD's top-left lap/position panel.
      font_set(FONT_BOLD, 12);
      font_draw(&g, fps_buf, 4, height - 29);
      // Second line: where a frame's time went, mean ms over the last
      // second -- logic:build:gl:swap (see prof_sum's declaration). A high
      // swap with low everything else means the GPU, not the CPU, is the
      // limit; a high build means the per-face geometry work is.
      char prof_buf[64];
      snprintf(prof_buf, sizeof(prof_buf), "%d.%d:%d.%d:%d.%d:%d.%d",
               prof_avg_tenths[0] / 10, prof_avg_tenths[0] % 10,
               prof_avg_tenths[1] / 10, prof_avg_tenths[1] % 10,
               prof_avg_tenths[2] / 10, prof_avg_tenths[2] % 10,
               prof_avg_tenths[3] / 10, prof_avg_tenths[3] % 10);
      font_draw(&g, prof_buf, 4, height - 9);

      gfx_submit_gl(&g);
      }
    }

    // The headless screenshot hook, after every overlay so the dump is what
    // the screen shows.
    if (screenshot_path && frame >= screenshot_frame) {
      glFinish();
      // The whole display as presented (on desktop the same 800x450 window).
      int32_t shot_w, shot_h;
      platform_display_size(&shot_w, &shot_h);
      uint8_t *pixels = malloc((size_t)shot_w * shot_h * 3);
      glReadPixels(0, 0, shot_w, shot_h, GL_RGB, GL_UNSIGNED_BYTE, pixels);
      char shot_file[1024];
      if (screenshot_count > 1) snprintf(shot_file, sizeof(shot_file), "%s.%d", screenshot_path, frame - screenshot_frame);
      else snprintf(shot_file, sizeof(shot_file), "%s", screenshot_path);
      fprintf(stderr, "screenshot %s: ticked=%d t=%.2f checkpoints cleared:", shot_file, (int)race_ticked,
              accumulator_ms / TICK_MS);
      for (int32_t i = 0; i < nplayers; i++) fprintf(stderr, " %d", cp.clear[i]);
      fprintf(stderr, "\n");
      FILE *f = fopen(shot_file, "wb");
      if (f) {
        fprintf(f, "P6\n%d %d\n255\n", shot_w, shot_h);
        // glReadPixels' origin is bottom-left; PPM's is top-left.
        for (int row = shot_h - 1; row >= 0; row--) {
          fwrite(pixels + (size_t)row * shot_w * 3, 1, (size_t)shot_w * 3, f);
        }
        fclose(f);
      }
      free(pixels);
      if (frame >= screenshot_frame + screenshot_count - 1) running = false;
    }
    DIAG_PHASE("frame: swap (waiting on the GPU)");
    prof_swap_start = platform_ticks_us();
    platform_swap_buffers();
    prof_sum[3] += platform_ticks_us() - prof_swap_start;
    if (bench.active && bench.measuring && bench.n > 0 && bench.f[bench.n - 1].swap_us == 0) {
      bench.f[bench.n - 1].swap_us = (uint32_t)(platform_ticks_us() - prof_swap_start);
    }
    // Pace to 60 Hz: sleep only what is left of this frame's 16.7 ms. A flat
    // 16 ms sleep on top of the frame's own work held the Switch at ~47 fps
    // with ~6 ms of work a frame (its swap does not wait for vsync). Where
    // the swap does wait, the frame is already due and nothing sleeps. A
    // frame more than 50 ms late restarts the schedule instead of bursting.
    {
      static uint64_t next_frame_us = 0;
      const uint64_t now_us = platform_ticks_us();
      if (next_frame_us == 0 || now_us > next_frame_us + 50000) next_frame_us = now_us;
      next_frame_us += 16667;
      if (next_frame_us > now_us + 1000) platform_delay_ms((uint32_t)((next_frame_us - now_us) / 1000));
    }
    frame++;
    // Snapshot this frame's buttons as "previous" for the next frame's
    // KEY_EDGE() checks -- done at the very end of the loop so every
    // block above (menu + racing) sees the same edge state.
    memcpy(previous_held, held, sizeof(held));
  }
#undef KEY_EDGE

  gfx_free(&g);
  if (motion_blur_ok) {
    gfx_gl_render_target_free(&scene_rt);
    gfx_gl_render_target_free(&accum_rt); // the trail history, see its init above
  }
  free(visible_idx);
  free(rank);
  free(order);
  free(all_objs);
  if (state == STATE_RACING) {
    // Bounded by nplayers, not BOTS_MAX_PLAYERS -- co[nplayers..6] were
    // never constructed for a reduced-car-count race (NFM1) and still
    // hold garbage stack memory, not a zeroed/freeable ContO.
    for (int32_t i = 0; i < nplayers; i++) cont_o_free(&co[i]);
  }
  cont_o_free(&car_base);
  for (int32_t i = 0; i < stage_count; i++) cont_o_free(&stage_objects[i]);
  free(stage_objects);
  for (int32_t i = 0; i < GAME_SPARKER_NUM_BASE_MODELS; i++) {
    if (base_models[i].p) cont_o_free(&base_models[i]);
  }
  free(base_models);
  medium_free(&m);
  trackers_free_sect(&t);
  wav_free(&snd_checkpoint);
  wav_free(&snd_carfixed);
  wav_free(&snd_powerup);
  wav_free(&snd_three);
  wav_free(&snd_two);
  wav_free(&snd_one);
  wav_free(&snd_go);
  for (int32_t sig = 0; sig < 5; sig++) {
    for (int32_t rev = 0; rev < 5; rev++) wav_free(&snd_engine[sig][rev]);
  }
  for (int32_t i = 0; i < 6; i++) wav_free(&snd_air[i]);
  for (int32_t i = 0; i < 3; i++) {
    wav_free(&snd_crash[i]);
    wav_free(&snd_lowcrash[i]);
    wav_free(&snd_skid[i]);
    wav_free(&snd_dustskid[i]);
    wav_free(&snd_scrape[i]);
  }
  wav_free(&snd_tires);
  wav_free(&snd_wasted);
  wav_free(&snd_firewasted);
  wav_free(&snd_redflash);
  wav_free(&snd_caught);
  audio_stop_music(&audio); // stop referencing the tracks before freeing them
  radical_track_free(&stage_music);
  radical_track_free(&interface_music);
  car_smoke_warp_free(&car_smoke_warp);
  free(pause_flex_read);
  free(pause_flex_rgba);
  audio_shutdown(&audio);
  platform_shutdown();
  return 0;
}
