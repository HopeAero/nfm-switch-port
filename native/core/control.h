// ports web/Control.js
//
// PARTIAL PORT. One Control instance per car -- both a human player's
// live input flags (left/right/up/down/handb/...) and (for AI-controlled
// slots) the huge state machine that computes those same flags
// automatically from track geometry.
//
// Ported: the full field layout, `falseo` (clears input flags -- called on
// focus loss / race pause), `reset` (per-race setup: fix-point proximity
// targets, clears input flags), `py`/`pys` (squared/rooted planar
// distance, delegate to trackers_py), and `preform` (~1950 lines, the AI
// driver -- computes left/right/up/down/handb from track geometry +
// checkpoints for AI-controlled cars; see control_preform() below, wired
// into the sim loop by native/platform/common/game.c matching
// GameSparker.java's own tick ordering). `Mad.js`'s `drive()` only READS
// the input flags (left/right/up/down/handb/steer/touchTrick*/wall/
// zyinv) -- it doesn't care whether a human or `preform()` set them, so a
// human-driven car works fully without `preform()`.
#ifndef NFM_CONTROL_H
#define NFM_CONTROL_H

#include <stdbool.h>
#include <stdint.h>
#include "medium.h"

#ifdef __cplusplus
extern "C" {
#endif

struct CheckPoints;
struct Mad;
struct ContO;
struct Trackers;

typedef struct Control {
  bool left, right, up, down, handb;
  int32_t lookback;
  bool enter, exit;
  bool arrace;
  bool mutem, mutes;
  bool radar;
  int32_t chatup;
  int32_t multion;
  int32_t pan;
  int32_t attack;
  int32_t acr;
  bool afta;
  int32_t fpnt[50];   // one per fix hoop (CHECK_POINTS_MAX_FIX: Extended's stages hold up to 50)
  float steer;
  bool touchTrick;
  int32_t touchTrickX, touchTrickY;
  int32_t trfix;
  bool forget;
  bool bulistc;
  int32_t runbul;
  int32_t acuracy;
  int32_t upwait;
  bool agressed;
  float skiplev;
  int32_t clrnce;
  int32_t rampp;
  int32_t turntyp;
  float aim;
  int32_t saftey;
  bool perfection;
  float mustland;
  bool usebounce;
  float trickprf;
  int32_t stuntf;
  bool zyinv;
  bool lastl, wlastl;
  int32_t hold;
  int32_t wall, lwall;
  int32_t stcnt;
  int32_t statusque;
  int32_t turncnt;
  int32_t randtcnt;
  int32_t upcnt;
  int32_t trickfase;
  int32_t swat;
  bool udcomp, lrcomp, udbare, lrbare;
  bool onceu, onced, oncel, oncer;
  int32_t lrdirect, uddirect, lrstart, udstart;
  int32_t oxy, ozy;
  int32_t flycnt;
  bool lrswt, udswt;
  bool gowait;
  int32_t actwait;
  int32_t cntrn;
  int32_t revstart;
  int32_t oupnt;
  int32_t wtz, wtx;
  int32_t frx, frz, frad;
  int32_t apunch;
  bool exitattack;
  int32_t avoidnlev;
  bool spatk;   // Extended: the special's button (toggled), or the AI firing it
  // Extended Mode's own AI state (Control.java: stuck/downuse/fewsecs back a
  // car off a wall it is pinned against; fixby is the damage % it repairs at).
  int32_t stuck, downuse, fewsecs;
  bool fewsecson;
  int32_t fixby;

  Medium *m; // borrowed, not owned
} Control;

void control_init(Control *c, Medium *m);

/** Clears input flags; n selects which auxiliary flags also clear (1/2/3
 * skip radar+arrace+chatup / multion / mutem+mutes respectively) --
 * matches the JS's own n-gated skips exactly. */
void control_falseo(Control *c, int32_t n);

/** Per-race setup: computes each fix-point's nearest-checkpoint target
 * (fpnt[]), applies stage-specific AI hold/revstart tuning, clears input
 * flags. */
void control_reset(Control *c, struct CheckPoints *cp, int32_t n);

/** control_reset, then Extended Mode's own per-stage hold/revstart tuning
 * (Control.java:9824-9937, outside career) in place of NFM 2's. The extended
 * build calls this instead of control_reset; `classic` is xt.classicmode
 * (cp->stage is then NFM 2's 11-27, else Extended's own 1-28). */
void control_reset_ext(Control *c, struct CheckPoints *cp, int32_t n, bool classic);

int32_t control_py(int32_t n, int32_t n2, int32_t n3, int32_t n4);
int32_t control_pys(int32_t n, int32_t n2, int32_t n3, int32_t n4);

/**
 * The AI driver -- ports web/Control.js's preform() (itself a line-by-
 * line transpile of Control.java's preform(), Control.java:295-2350;
 * see web/Control.js's own header comment), which computes this frame's
 * left/right/up/down/handb/steer from track geometry, checkpoints, and
 * per-stage scripted tuning/encounters. Only called for AI-controlled
 * slots -- a human-driven car never calls this (mad_drive() only READS
 * the input flags, see control.h's own file-level comment).
 *
 * Structured as five sections in the SAME order as the source, each
 * documented at its own call site in control.c: (1) per-decision-cycle
 * personality/difficulty tuning + attack-target selection, gated by
 * `stcnt > statusque` so it only re-rolls periodically, not every tick;
 * (2) main steering -- waypoint/checkpoint navigation with per-stage
 * scripted overrides, or combat aiming when `attack != 0`; (3) turning
 * the aim angle into left/right/handb/down; (4) wall-avoidance/recovery;
 * (5) airborne stunt/trick control + landing correction, taken instead
 * of (1)-(4) whenever the car isn't touching the ground.
 */
void control_preform(Control *c, struct Mad *mad, struct ContO *contO,
                      struct CheckPoints *checkPoints, struct Trackers *trackers);

#ifdef __cplusplus
}
#endif

#endif
