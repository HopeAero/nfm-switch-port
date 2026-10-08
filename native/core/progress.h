// Ports xtGraphics.java's `unlocked[]`/`scm[]`/`justwon1`/`justwon2` (see
// xtGraphics.java:56-57 for the field declarations, :422-423 for the
// initial values, and the game_progress_can_pick_car / _can_pick_stage
// doc comments below for the exact gate conditions). Pure state + tiny
// pure decision functions -- fully platform-agnostic and host-testable;
// disk persistence is a thin optional layer at the bottom so the same
// struct can be exercised by unit tests without touching the filesystem.
//
// One `GameProgress` value is owned by main.c and threaded through the
// menu code (car-select + stage-select gates) and the finish() hook
// (increment on winning the current unlock threshold). See PORT_SPEC.md
// §M5 for the wider progression contract.
#ifndef NFM_PROGRESS_H
#define NFM_PROGRESS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Mirrors xtGraphics.java's gmode field (:_ -- see maini2 line 4640):
//   0 = Free Play (no progression, any car, any stage)
//   1 = NFM 1 campaign (stages 1..10 sequential)
//   2 = NFM 2 campaign (stages 11..27 sequential)
typedef enum {
  GMODE_FREE_PLAY = 0,
  GMODE_NFM1      = 1,
  GMODE_NFM2      = 2,
} GameMode;

typedef struct {
  // unlocked[0] = max unlocked stage in NFM1 (1..11), unlocked[1] = same for NFM2 (1..17).
  // Java initial state: new int[] { 1, 1 } -- only stage 1 of each campaign is playable.
  // Incremented by game_progress_finish_stage() when the player beats the
  // stage exactly equal to `unlocked[gmode-1] + (gmode-1)*10` -- see
  // xtGraphics.java:7002-7017.
  int32_t unlocked[2];

  // scm[gmode-1] = the scaffold car UNLOCKED as a side-effect of beating
  // certain milestone stages. Java xtGraphics.java:6701-6775 sets these
  // to specific fixed car indices per (gmode, stage) pair. Zero-initialised
  // (Java: new int[] { 0, 0 }) -- means "no bonus car unlocked yet".
  int32_t scm[2];

  // Latched booleans reset every finish(). `justwon1` = "the player just
  // beat NFM1's current unlock-threshold stage on this race"; same for
  // justwon2/NFM2. Read by stageselect's own re-initialisation of
  // checkPoints.stage (Java :1914, :1925) so the winner keeps looking at
  // the next-to-play stage instead of snapping back to the one just cleared.
  bool justwon1;
  bool justwon2;
} GameProgress;

// Initialise to Java's exact starting values (unlocked={1,1}, scm={0,0},
// no justwon). Used both at process start (before load_from_disk) and by
// tests that want a clean slate.
void game_progress_reset(GameProgress *p);

// Java carselect gate (xtGraphics.java:5306-5323): whether `car_index` is
// unlocked in the current campaign's progression. Returns true if pickable,
// false if locked. NOTE: in the real Java, LEFT/RIGHT nav does NOT skip
// locked cars -- landing on one instead shows an animated fence-gate
// overlay + "[ Car Locked ]" text (see game_progress_car_unlock_stage()
// below and game.c's own STATE_CAR_SELECT draw block). This function is
// the gate CONFIRM checks before proceeding to stage-select, and that
// draw block uses to decide whether to show the gate overlay.
//   gmode == 0 (Free Play): every built-in car is always pickable
//     (:5306 wraps the whole gate in `if (this.gmode != 0)`).
//   gmode == 1 (NFM1): cars 5/6/11/14/15 gated on unlocked[0]>2/4/6/8/10
//     respectively (:5307-5320). All other cars always pickable.
//   gmode == 2 (NFM2): cars 8..15 gated on unlocked[1] > (car-7)*2
//     (:5323). All other cars always pickable.
// The custom "Simple_Car.rad" slot (>=16) is treated as Free-Play-only and
// always pickable there, never in NFM1/NFM2 (Java's testdrive slot is
// gated separately and irrelevant to the single-player port).
bool game_progress_can_pick_car(const GameProgress *p, GameMode gmode, int32_t car_index);

// Java carselect's own local `k` (xtGraphics.java:5306-5325), used ONLY
// for the locked-car gate overlay's "This car unlocks when stage K is
// completed..." message -- NOT the same value game_progress_can_pick_car
// checks against (unlocked[]), it's the literal number the real message
// text names. Returns 0 if `car_index` is already unlocked (no message
// needed). NOTE (preserved exactly, not "fixed"): for NFM2 cars this is
// (car_index-7)*2, the campaign-RELATIVE unlock index (2,4,..16), not the
// real absolute stage number (12,14,..26) -- Java's own message genuinely
// says e.g. "stage 4" for what's actually stage 14.
int32_t game_progress_car_unlock_stage(const GameProgress *p, GameMode gmode, int32_t car_index);

// Java stageselect gate: in NFM1/NFM2 the player can navigate through
// stages 1..(unlocked[gmode-1]+1); the "+1" position is the next-to-play
// stage which shows the `cantgo()` locked overlay when confirmed. Returns
// true if the given `stage_num` is directly playable (no cantgo), false
// if it should trigger the cantgo screen or is entirely out of range for
// this gmode.
// Free Play: every stage 1..27 is directly playable.
// NFM1: 1..unlocked[0] directly playable, unlocked[0]+1 shows cantgo.
// NFM2: 11..(unlocked[1]+10) directly playable, unlocked[1]+11 shows cantgo.
bool game_progress_can_pick_stage(const GameProgress *p, GameMode gmode, int32_t stage_num);

// Returns true if `stage_num` is in-range for `gmode` (whether or not it's
// unlocked). Used by stage-select nav to know when to wrap around.
//   Free Play: 1..27
//   NFM1: 1..11 (10 stages + "locked" cantgo slot at 11)
//   NFM2: 11..27 (17 stages + cantgo, mirrors Java's :1925 layout)
bool game_progress_stage_in_range(GameMode gmode, int32_t stage_num);

// Returns the first-in-list stage for `gmode` (Java stageselect's
// initialisation): NFM1 => 1, NFM2 => 11, Free Play => 1. Used when the
// gamemode submenu transitions into car-select then stage-select so the
// picker doesn't start on an out-of-range stage from a prior campaign.
int32_t game_progress_first_stage(GameMode gmode);

// Returns the initially-selected stage on entering the stage picker,
// mirroring Java's :1914/:1925: for NFM1, `unlocked[0]` (the next-to-play
// stage) unless the campaign is complete AND the player hasn't just won
// -- in which case snap back to the last-played. For Free Play, always
// stage 1. This is the state the picker cursor should start on.
int32_t game_progress_default_stage(const GameProgress *p, GameMode gmode);

// Java :6697-6776 -- when winning the campaign's current unlock-threshold
// stage, the same finish() screen unlocks a specific bonus car for that
// stage. Returns 0 if the (gmode, stage) pair grants no bonus car. Exported
// (not just used internally by game_progress_finish_stage) so game.c's
// post-race unlock-celebration screen can re-derive the SAME value fresh
// each draw, the way Java's finish() recomputes its own local `n4` from
// scratch every call -- reading back GameProgress::scm[] instead would be
// wrong, since scm[] only updates when a stage actually grants a bonus
// car and so holds a stale value from an earlier campaign milestone on
// every other stage's finish screen.
int32_t game_progress_bonus_car_for(GameMode gmode, int32_t stage_num);

// Java finish() progression trigger (xtGraphics.java:7002-7023). Called
// AFTER a race with `won == mad.nlaps >= cp.nlaps` (win) or false (lose).
// If `won` and `stage_num == unlocked[gmode-1] + (gmode-1)*10` (i.e. the
// player just cleared the current unlock threshold, not a replay), then:
//   - ++unlocked[gmode-1]
//   - set scm[gmode-1] to the fixed bonus car index for that (gmode, stage)
//     pair per Java's :6697-6776 table (0 if no bonus).
//   - set justwon1 (NFM1) or justwon2 (NFM2) true; else false.
// Free Play (gmode==0): always no-op, per Java's :6692 gate.
void game_progress_finish_stage(GameProgress *p, GameMode gmode, int32_t stage_num, bool won);

// Persist to `path` (a full file path, caller-resolved -- see
// platform/common/platform.h's platform_progress_path(), which picks
// $XDG_DATA_HOME/nfm-psivta/progress.bin or the $HOME fallback on
// desktop, ux0:data/<title-id>/progress.bin on Vita; keeping that
// platform-specific resolution out of this file is what lets progress.c
// itself stay the "fully platform-agnostic, host-testable" file its own
// top comment promises). Creates the parent directory if needed.
// Fixed-layout binary, versioned so future field additions can still
// read older saves. Returns true on success, false if the file couldn't
// be opened / short-written (the caller keeps the in-memory value either
// way; missing progression on next boot is a soft failure, not a crash).
bool game_progress_save_to_disk(const GameProgress *p, const char *path);

// Read the persisted progress from `path` (see game_progress_save_to_disk
// above). On failure (missing file, unknown version, truncated data),
// `*p` is reset to game_progress_reset() defaults and false is returned.
// On success `*p` holds the on-disk values and true is returned. Justwon
// flags always come back false regardless of what was on disk -- they're
// per-race latches, not durable state.
bool game_progress_load_from_disk(GameProgress *p, const char *path);

// --- Player settings (this port's own pause-menu Settings screen) -------
// Not part of the original's save; kept in a small text file next to the
// progress file ("settings.txt" in the same directory), so the progress
// format stays exactly what it was.

/** Motion-blur intensity, 0..100 in steps of 20; 100 is the original's
 * trail strength, 0 turns the trail off. The extended build starts with it
 * off, as it does the screen shake (the user, 2026-10-07). */
#define GAME_SETTINGS_BLUR_DEFAULT 0

/** How the race (and the car/stage selects) reach the screen: drawn at the
 * original 800x450 and stretched with no filtering, the same stretched with
 * linear filtering, or drawn at the display's own resolution. */
typedef enum { GFX_ORIGINAL = 0, GFX_SMOOTH = 1, GFX_HD = 2, GFX_QUALITY_COUNT } GfxQuality;

/** The race actions Settings > Controls can move to another button. */
typedef enum {
  BIND_ACCEL, BIND_BRAKE, BIND_HANDB, BIND_VIEW, BIND_ARRACE, BIND_RADAR,
  BIND_PAUSE, BIND_MUSIC, BIND_SFX, BIND_SPECIAL, BIND_LISTBARS, BIND_COUNT
} BindAction;

/** The Settings screen's values, saved beside the progress file. */
typedef struct {
  // Graphics
  int32_t graphics;     // GfxQuality
  int32_t draw_dist;    // 0 original, 1 far (150%), 2 max (200%) -- medium_draw_distance
  int32_t detail;       // 0 high (the original's), 1 low (its own resdown 2 mode)
  int32_t shadows;      // 0/1
  int32_t particles;    // 0/1 -- dust and sparks
  int32_t blur;         // 0..100, step 20
  int32_t smooth;       // 0/1: draw every display frame, blending the two latest ticks
  // Audio
  int32_t music_vol;    // 0..100, step 10
  int32_t sfx_vol;      // 0..100, step 10
  // Interface
  int32_t show_fps;     // 0 off, 1 the frame rate, 2 plus the frame-time breakdown
  int32_t board_names;  // 0/1: car names in the race standings (not in the original's single player)
  // Gameplay
  int32_t shake;        // 0/1: the screen shake on a crash (the original's `shaka`)
  int32_t rumble;       // 0/1: controller vibration on a crash, where the platform has it
  int32_t cam_orbit;    // 0/1: the view button cycles through the orbit camera...
  int32_t cam_watch;    // 0/1: ...the roadside tripod...
  int32_t cam_far;      // 0/1: ...and Extended's far camera (the chase camera always)
  // Controls (the Switch only)
  int32_t steer_dpad;          // 0 steer and stunt with the left stick, 1 with the D-pad
  int32_t bind[BIND_COUNT];    // per BindAction, an index into game.c's kPadNames
} GameSettings;

/** Defaults: no trail and no shake (the extended build), vibration on, and `graphics`
 * as the caller passes (each platform picks its own). */
GameSettings game_settings_defaults(int32_t graphics);

/** Reads settings.txt beside `progress_path` into `s`, which holds the
 * defaults for anything missing or invalid. The blur is snapped to its grid. */
void game_settings_load(const char *progress_path, GameSettings *s);

/** Writes it there (creating the directory if needed). */
bool game_settings_save(const char *progress_path, const GameSettings *s);

#ifdef __cplusplus
}
#endif

#endif
