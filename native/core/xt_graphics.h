// M2 STUB for web/XtGraphics.js -- NOT a port of the real file (2578
// lines: full HUD, speedo/damage display, minimap, menus, multiplayer
// lobby state -- all out of scope, see TASKS_NATIVE.md). Started as a
// data-only stub for the small, bounded set of XtGraphics fields/methods
// Mad.js's drive()/colide() touch (screen-shake/particle triggers, audio
// playback, informational stat counters -- none of which feed back INTO
// the physics), but the physics-triggered SFX state machine (crash()/
// skid()/scrape()/gscrape()/sparkeng()/#playsounds()) is now a REAL port
// (Part 16), not a no-op -- see each function's own doc comment. Still
// stubbed: the rest of the real 2578-line file (HUD drawing beyond what
// main.c already ports directly, minimap, multiplayer lobby).
//
// When the real XtGraphics.js gets ported (M3), this file's fields should
// become a subset of that one, not a separate thing living on.
#ifndef NFM_XT_GRAPHICS_H
#define NFM_XT_GRAPHICS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Forward-declared, not #included: mad.h/control.h/medium.h all sit
// "above" this file in the dependency order (mad.h itself #includes
// xt_graphics.h), so xt_graphics_stub_scrape/playsounds only take
// pointers to these, never touching their fields directly here.
struct Medium;
struct Mad;
struct Control;

typedef struct {
  int32_t im;      // which player slot is "the local viewer" -- HUD/camera-shake gate
  int32_t multion; // 0 = single-player; most of drive()'s xt.multion-gated code is dead at 0
  bool lan;
  int32_t starcnt;
  bool mutes;
  // Music mute -- synced from control->mutem inside #playsounds() (see
  // xt_graphics.c), same "wired but no key bound yet" state as
  // control->lookback elsewhere in this port.
  bool mutem;
  bool isbot[8];
  int32_t dcrashes[8];
  int32_t beststunt;
  int32_t laptime;
  int32_t fastestlap;

  // drawstat()'s own persistent flicker state (see native/platform/
  // linux/main.c's HUD drawing, which ports drawstat's two fillPolygon
  // bars directly). `dmcnt`/`dmflk` gate a "flashing red" low-power
  // warning on the power bar fill itself. `auscnt`/`aflk` are the SAME
  // fields the stunt announcer below drives (a real shared pair in the
  // original, not a coincidence: `xt.auscnt < 45 && xt.aflk` is how the
  // power bar's own flash syncs to whatever stunt banner is currently
  // showing) -- constructor defaults (auscnt=45, aflk=false) match the
  // JS's own idle state before any stunt has landed.
  int32_t dmcnt;
  bool dmflk;
  int32_t auscnt;
  bool aflk;

  // The speedometer's own last-frame car position, used to compute an
  // actual km/h reading from position delta each render -- see
  // main.c's HUD drawing (ports XtGraphics.js's speedo block directly).
  int32_t lcarx, lcary, lcarz;

  // arrow()'s own persistent state (see main.c's HUD drawing, which ports
  // BOTH branches of XtGraphics.js's arrow(): the checkpoint-bearing one
  // and, since the A key was wired, the arrace car-targeting one. The
  // claim that used to sit here -- that arrace "stays false permanently in
  // this single-player-only stub" -- was only ever true because nothing
  // bound the key; the original toggles it from GameSparker.java:3618-3625
  // in plain single-player, and the nplayers!=1 gate at xtGraphics.java:7892
  // guards the say-line sync, not the feature.
  // `ana` is the arrow's own smoothed bearing (degrees, world-relative);
  // `flk`/`cntan` drive its fill-colour flicker and "off-course" state --
  // `flk` is ALSO the same flicker flag XtGraphics.js's "Checkpoint
  // Missed!" message reuses (a real shared flag in the original, not a
  // coincidence), and `cntan` feeds into "Wrong Way!"'s own trigger
  // (cntovn, alongside the message system's other fields -- see the
  // message-system fields below).
  int32_t ana;
  bool flk;
  int32_t cntan;

  // arrow()/radarstat()'s manual car-lock (xtGraphics.java:3828 sets it,
  // :7957-7969 announces changes, :8990 highlights the locked blip, and
  // arrow():8652 would honour it). It is only ever ASSIGNED from the
  // mouse-hover branch at :3823-3830, which this port has no equivalent
  // for -- so in practice it stays at its -1 "nothing locked" default and
  // arrow()'s own `multion == 0 || alocked == -1` test always takes the
  // auto-nearest-target path. Carried anyway so the code that reads it
  // reads the real field rather than a hardcoded -1. `lalocked` is the
  // previous value :7957 diffs against to fire the announcement once.
  int32_t alocked;
  int32_t lalocked;

  // The LATCHED copy of Control.arrace that xtGraphics.java:7892-7916
  // diffs the live toggle against, so flipping A announces the change
  // exactly once ("Arrow now pointing at > CARS" / "> TRACK") instead of
  // re-firing the say line every frame the key stays flipped. Control
  // owns the live flag; this is the previous value.
  bool arrace;

  // Message/announcer state (XtGraphics.js's own `say`/`tcnt`/`wasay`/
  // `tflk`/`cntovn` -- see main.c's HUD drawing for the drawcs()-style
  // centered-message helper and the concrete triggers ported: "Checkpoint!",
  // "Checkpoint Missed!", "Wrong Way!", "Car Fixed". `hud_clear` mirrors
  // XtGraphics.js's own `this.clear` (the HUD's last-seen `mad.clear`,
  // used only to detect a NEW checkpoint clear this tick) -- named
  // differently from `mad->clear` here purely to avoid same-name-
  // different-struct confusion in this port, not a JS field rename.
  char say[128];
  int32_t tcnt;
  bool wasay;
  bool tflk;
  int32_t cntovn;
  int32_t hud_clear;
  // xtGraphics.dested[]: the HUD's last-seen CheckPoints.dested, so a car's
  // wasting is announced once, the tick it changes.
  int32_t hud_dested[8];

  // Stunt announcer state -- XtGraphics.js's own `loop`/`spin`/`asay`/
  // `looped`/`pwcnt`/`pwflk`/`skidup` (see main.c's hud_stunt_detect()/
  // hud_messages_tick() for the ported logic; `auscnt`/`aflk` above are
  // shared with the power bar flash, see that field's own comment). The
  // PHYSICS side (mad->loop state machine, travxy/travzy/travxz,
  // powerup scoring, beststunt above) is ordinary Mad.c physics, already
  // ported; these fields are purely the announcer text this stub is
  // named for. `loop`/`spin` are the two named-trick fragments ("Forward
  // loop", "Rollspin", ...) `asay` gets assembled from; sized for the
  // longest literal ("Tabletop and reversed Tabletop" / "massive Roll
  // spinning") plus the "Hanged " prefix. `asay` itself is sized
  // generously for the worst case (surfer prefix + adj + loop + " with "
  // + spin + " by 900 and beyond" + exlm).
  char loop[40];
  char spin[32];
  char asay[192];
  int32_t looped;
  int32_t pwcnt;
  bool pwflk;
  bool skidup;
  // crash()'s own turn-rotation latch (see that function's doc comment) --
  // shared with the stunt announcer's `skidup` sibling, both toggled by
  // the SAME "Car Fixed" trigger in the real XtGraphics.js.
  bool crashup;

  // Physics-triggered SFX state -- XtGraphics.js's crash()/skid()/
  // scrape()/gscrape()/sparkeng()/#playsounds() (xtGraphics.java:9081-
  // 9280ish). See xt_graphics.c's own doc comments on each ported
  // function for the exact translation; game.c's tick loop drains the
  // `pending_*`/`air_*`/`pengs[]` outputs once per tick into real
  // audio_play() calls, since this file stays platform-audio-agnostic
  // (no SDL/sceAudio dependency), same split as every other HUD/audio
  // trigger point in this stub.
  int32_t crshturn;         // crash()'s crash1-3/lowcrash1-3 variant rotation
  int32_t bfcrash;          // crash() debounce
  int32_t bfskid;           // skid() debounce
  int32_t skflg, dskflg;    // skid()'s skid1-3/dustskid1-3 variant rotation
  int32_t bfscrape;         // scrape() debounce
  int32_t sturn0, sturn1;   // scrape()'s scrape1/2 alternation state
  int32_t bfsc1, bfsc2;     // gscrape()'s two-key (scrape3/scrape3b) debounce
  int32_t pwait;            // #playsounds()'s engine-rev top-slot hold counter
  int32_t stopcnt;          // #playsounds()'s air-loop auto-stop countdown
  int32_t cntwis;           // #playsounds()'s "about to be wasted" ramp (0-8)
  int32_t lcn;              // sparkeng()'s last car number (which engine bank pengs[] refers to)
  bool pengs[5];            // sparkeng(): which of the 5 engine rev slots SHOULD be looping now
  bool grrd, aird;          // #playsounds()'s air-loop latches (grounded/airborne edge guards)
  bool pwastd;              // #playsounds(): the "about to be wasted" loop SHOULD be playing now

  // One-shot pending sound this tick, one slot per independent debounce
  // (crash/skid/scrape/gscrape can all fire in the SAME tick -- e.g.
  // colide()'s crash() and drive()'s skid() -- so they need separate
  // slots, not one shared field). NONE (0) if that trigger didn't fire.
  // main.c's tick loop reads+clears these once per tick, right after
  // calling mad_colide()/mad_drive() for the player.
  int32_t pending_crash;    // XtPendingCrashSnd, see xt_graphics.h
  int32_t pending_skid;     // XtPendingSkidSnd
  int32_t pending_scrape;   // XtPendingScrapeSnd
  bool pending_gscrape;     // always scrape3.wav -- see gscrape()'s own doc comment on scrape3/3b
  bool pending_firewasted;

  // #playsounds()'s air[] (whoosh) loop decision this tick -- reset to
  // (false,-1) at entry, mutated in JS call order, so a later write in
  // the SAME call correctly overrides an earlier one (see the aird
  // branch's own doc comment: it can supersede grrd's fresh loop within
  // the same call via stopairs()). At most one air clip is ever actually
  // meant to be audible at a time by construction of the algorithm, so
  // main.c only needs to track ONE active air channel, not six.
  bool air_stop_all;
  int32_t air_start_slot;   // 0-5, or -1 = no new loop requested this tick
} XtGraphicsStub;

// crash()'s pending_crash outcome -- crash1-3/lowcrash1-3/tires, matching
// sounds.zip's own file names.
typedef enum {
  XT_CRASH_NONE = 0,
  XT_CRASH_CRASH1, XT_CRASH_CRASH2, XT_CRASH_CRASH3,
  XT_CRASH_LOWCRASH1, XT_CRASH_LOWCRASH2, XT_CRASH_LOWCRASH3,
  XT_CRASH_TIRES,
} XtPendingCrashSnd;

// skid()'s pending_skid outcome.
typedef enum {
  XT_SKID_NONE = 0,
  XT_SKID_SKID1, XT_SKID_SKID2, XT_SKID_SKID3,
  XT_SKID_DUSTSKID1, XT_SKID_DUSTSKID2, XT_SKID_DUSTSKID3,
} XtPendingSkidSnd;

// scrape()'s pending_scrape outcome.
typedef enum {
  XT_SCRAPE_NONE = 0,
  XT_SCRAPE_SCRAPE1, XT_SCRAPE_SCRAPE2, XT_SCRAPE_SCRAPE3,
} XtPendingScrapeSnd;

void xt_graphics_stub_init(XtGraphicsStub *xt);

/** xt.human(n): true if slot n is the locally-viewed car (n === xt->im).
 * See xt_graphics.c's own comment on why this, not `!isbot[n]`, matches
 * the real XtGraphics.js in this stub's single-player-only scope. */
bool xt_graphics_stub_human(const XtGraphicsStub *xt, int32_t n);

// Physics-triggered SFX state machine -- REAL ports now (not no-ops), see
// xt_graphics.c's own doc comments for each. All are pure state: no audio
// API call happens here (see xt_graphics.h's own header comment on why:
// this file stays platform-audio-agnostic). `xt_graphics_stub_crash`/
// `_skid`/`_scrape`/`_gscrape` are called from mad.c's physics at the
// exact JS call sites (crash on hard collision, skid on wheelspin,
// scrape/gscrape on chassis-vs-geometry contact); `_scrape` alone needs
// `m` for `this.m.random()`'s scrape1-vs-scrape2 pick.
void xt_graphics_stub_skid(XtGraphicsStub *xt, int32_t n, float mag);
void xt_graphics_stub_gscrape(XtGraphicsStub *xt, int32_t x, int32_t y, int32_t z);
void xt_graphics_stub_scrape(XtGraphicsStub *xt, int32_t x, int32_t y, int32_t z, struct Medium *m);
void xt_graphics_stub_crash(XtGraphicsStub *xt, float mag, int32_t n2);

/** Ports `sparkeng(n, lcn)` (xtGraphics.java:9264) -- ensures exactly rev
 * slot `n` (-1 = none) of car `lcn`'s engine bank is the one flagged
 * "should be looping" in `xt->pengs[]`, switching bank first if `lcn`
 * changed since the last call. Pure state (see xt_graphics.c for why
 * this needn't touch `pengs[j]`'s PREVIOUS value to get the right
 * outcome); main.c reconciles `pengs[]` against real mixer channels. */
void xt_graphics_stub_sparkeng(XtGraphicsStub *xt, int32_t n, int32_t lcn);

/** Ports `#playsounds(mad, control, n)` (xtGraphics.java:9081) -- the
 * per-tick sound pump: picks the engine rev (via sparkeng), the air
 * whoosh loop, the "about to be wasted" loop, and decrements every
 * crash/skid/scrape/gscrape debounce. The JS's own third parameter
 * (stage number) is never read in its body (a real, harmless quirk of
 * the original -- see the same convention noted for `drawstat`'s unused
 * `newcar` param elsewhere in this port), so it's dropped here. Call
 * once per TICK (not per rendered frame) for the SAME reason
 * hud_stunt_detect() in main.c is tick-scoped: engine/air loop
 * decisions and the debounce counters they depend on are physics-
 * authoritative, and firing them once per frame would re-trigger events
 * however many extra frames render before the next tick. `starcnt` is
 * the 3-2-1-GO countdown value (xt->starcnt itself stays an unsynced
 * field elsewhere in this port -- main.c's own tick loop owns the
 * canonical countdown counter, so it's threaded in explicitly here
 * rather than duplicating a second source of truth). */
void xt_graphics_stub_playsounds(XtGraphicsStub *xt, struct Mad *mad, struct Control *control, struct Medium *m, int32_t starcnt);

#ifdef __cplusplus
}
#endif

#endif
