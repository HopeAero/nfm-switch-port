// Need for Madness 2 Extended's per-stage career scripts: what
// xtGraphics.careermode$m does every race frame on each of the 31 career
// stages and the four bonus stages (web/ext/xtGraphics.js 7562-9074; Java
// xtGraphics.java careermode ~7421), with the helpers it calls (teleport
// 6212, attack 6181, randtele 6188, realwalls 6287), the stage parts of
// stat$m's boss trigger (3908-4026), nitroandspecials' stage stat effects
// (6925-7051), GameSparker's per-stage collision gating (2134-2227), and the
// stage-gated pieces of Madness.drive the career adds, as helpers mad.c can
// call (gravity, ground level, ice/water/desert grip, nofix, the stage 11
// wreck into an undead, stage 13's portals). Pure logic: nothing here draws
// or plays a sound; game.c reads the HUD/music/sound fields below.
//
// Indices: a "slot" is the car's race slot (Madness.im), 0 the player. The
// original's one conto[] array holds the cars at 0..nplayers-1 and the stage
// pieces after them; here piece k (CareerStageWorld.pieces[k]) is the
// original's conto[nplayers + k].
//
// Random draws are kept in the original's order: Math.random() is
// career_random() (career.h), Medium.random() is medium_random(w->m).
#ifndef NFM_CAREER_STAGE_H
#define NFM_CAREER_STAGE_H

#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <stdlib.h>

#include "nfm_limits.h"
#include "career.h"
#include "car_define.h"
#include "check_points.h"
#include "cont_o.h"
#include "control.h"
#include "mad.h"
#include "medium.h"
#include "trackers.h"
#include "xt_graphics.h"

#ifdef __cplusplus
extern "C" {
#endif

// The original's per-slot arrays are 101 long; the ones teleport() can
// index with a stage piece's conto index (nplayers, nplayers + 1) stay so.
#define CAREER_STAGE_XT 101

/** Music the scripts switch (game.c plays it). */
enum {
  CAREER_MUSIC_NONE = 0,
  CAREER_MUSIC_STOP,   // stage 23, cstimer 2: the stage's music stops (XT 8429-8435)
  CAREER_MUSIC_BOSS    // stage 23, cstimer 3: Files/careermusic/bossbattlea.ogg once, then bossbattleb.ogg looping
};

/** Everything careermode$m and its helpers keep between frames. Zeroed and
 * set up by career_stage_start; per-slot arrays are indexed by slot. */
typedef struct CareerStage {
  // -- the race ---------------------------------------------------------------
  int32_t stage;        // CareerRace.stage (checkpoints.stage in the original)
  int32_t bonus;        // 0, or the bonus stage 1..4 (bonusstage[bonus - 1])
  bool hard;            // the newest stage or hard mode (unlocked[1] == stage || hardstage)
  int32_t nplayers;
  int32_t npieces;
  bool started;         // career_stage_start ran
  int32_t averagelevel; // CareerRace.averagelevel
  float avgstartgrip;   // the field's mean base grip (Madness 2257-2262)
  int32_t car[NFM_MAX_CARS];          // each slot's car, Extended's numbers (CareerRace.sc)
  bool beast[NFM_MAX_CARS], shadow[NFM_MAX_CARS];
  // ContO.invisiblepiece of the cars (255 opaque); written to ContO.fade
  // (255 - it), never below the see-through the car started with.
  int32_t car_inv[NFM_MAX_CARS], car_basefade[NFM_MAX_CARS];
  int32_t last_clear[NFM_MAX_CARS];   // each car's clear at the last tick (portal checkpoint clears)
  bool music_stopped;

  // -- per slot (xtGraphics arrays, Madness fields the C Mad lacks) -------------
  int32_t floor[NFM_MAX_CARS];        // stage 13's tower floor each car is on (resetstat 3; the grid's)
  float groundlevel[NFM_MAX_CARS];    // Madness.groundlevel: floor * -10000, 250 on the ground floor
  bool norender[NFM_MAX_CARS];        // not drawn and no arrow (stage 13's other floors, wasted undead)
  bool noarrow[NFM_MAX_CARS];         // out of the arrow and the ranking (undead, bonus 4)
  bool newflame[NFM_MAX_CARS];        // ContO.greenflame: the undead's green flames (draw only)
  bool undead[NFM_MAX_CARS];          // xtGraphics.undead: wrecked once, then immortal at full power
  bool fakedest[NFM_MAX_CARS];        // Madness.fakedest: stage 11's wrecked cars counted as wasted
  bool revive[NFM_MAX_CARS];          // stage 23: a wasted car the Titan brought back
  int32_t speedhack[CAREER_STAGE_XT]; // ticks a teleported car keeps its top speed
  bool specialflag[CAREER_STAGE_XT];  // xtGraphics.specialflag: the car is upside down along xy (Madness.drive)
  bool entered[NFM_MAX_CARS];         // stage 21: made it into the arena; stage 6: carried by the ghost
  bool countfall[NFM_MAX_CARS];
  bool crumblefail[NFM_MAX_CARS];     // stage 21: fell with the floor
  int32_t lives[NFM_MAX_CARS];        // stages 20/21: ticks spent outside the walls (1000 = just respawned)
  int32_t timesfallen[NFM_MAX_CARS];
  bool respawning[NFM_MAX_CARS];      // Madness.respawning: next tick resets instead of driving
  int32_t destimer[NFM_MAX_CARS];     // stages 3/13/17/21: ticks since a car was wasted
  int32_t glitchtimer[NFM_MAX_CARS];  // stage 19
  int32_t statdrain[NFM_MAX_CARS];    // stage 11: ticks since the car was last fixed
  bool safezone[NFM_MAX_CARS];        // stage 11: inside the first van's yard
  int32_t undeadlock[NFM_MAX_CARS];   // stage 11: which car each van's yard holds
  bool nohit[NFM_MAX_CARS];           // the field's beasts can not be hit at the start
  int32_t nostunts[NFM_MAX_CARS];     // Madness.nostunts: stage 21's entry stunts
  int32_t xzadjust[NFM_MAX_CARS];     // Madness.xzadjust: turns the 0..360 wrap took off conto.xz
  int32_t sp_ts[NFM_MAX_CARS];        // madness[k].aitssp[sc[k]] (stage 11 slows the vans to the player)
  float speedmulti[NFM_MAX_CARS];     // Madness.speedmulti (stages 16, 18, 24), 1
  float powermulti[NFM_MAX_CARS];     // Madness.powermulti (stages 9, 18), 1
  float nuclearmod[NFM_MAX_CARS];     // Madness.nuclearmod (stage 11), 1
  int32_t oldx[NFM_MAX_CARS], oldz[NFM_MAX_CARS];

  // stage 13: the portals (Madness.drive 3252-3361) and the floor guardians
  bool forcehandb[NFM_MAX_CARS];
  int32_t teletimer[NFM_MAX_CARS];
  float initialspeed[NFM_MAX_CARS];
  int32_t sendtofloor[NFM_MAX_CARS];
  int32_t slowstable[NFM_MAX_CARS];
  int32_t teleinvul[NFM_MAX_CARS];
  bool telechk[NFM_MAX_CARS];         // Madness.telechk: cleared a checkpoint while braking into a portal
  int32_t telefade[NFM_MAX_CARS];     // ContO.telefade (copies start at 255)
  bool tele_fading[NFM_MAX_CARS];     // ContO.teleported: the car is fading out into a portal (draw)
  int32_t dmgcolour[NFM_MAX_CARS][3]; // ContO.dmgcolours: a guardian's flames by its damage
  bool floorguardian[NFM_MAX_CARS];   // ContO.floorguardian
  bool guardswitch[NFM_MAX_CARS];     // ContO.guardswitch
  // one-shot portal events for the bots (game.c applies them): the car
  // arrived (Bots.specialtimer[k] = Bots.botoffset[portal_whichset[k]][k]),
  // and whether it gets its recording back (Bots.botbreak[k] = false)
  bool portal_arrived[NFM_MAX_CARS];
  int32_t portal_whichset[NFM_MAX_CARS];
  bool portal_unbreak[NFM_MAX_CARS];

  // stage 15: outside every track piece's footprint (ContO.outoftrack)
  bool outoftrack[NFM_MAX_CARS];
  int32_t *piece_ext;                 // [npieces][4]: x min, x max, z min, z max (ContO.xextreme/zextreme)
  bool *piece_norender;               // [npieces]: xtGraphics.norender[nplayers + k]

  // -- stage 6: the ghost (pieces 0 and 1), stage 7's shadow ------------------
  bool wallcountdown;
  int32_t wallimmunity;
  int32_t ghosttimer, ghostfade, ghostattempt, ghostattack, ghostfar;
  bool ghostflash[3];
  int32_t ghostflashtimer;
  bool ghosthit, ghosttele;
  int32_t ghostteletimer, whatghostdo;
  bool shownghost;
  int32_t randomtimes;
  bool scareflash, turnbackon;
  int32_t scareflashtime;
  bool playonce;

  // stage 7: Medium.polyoutline[1], the green of the ground patches'
  // outlines (draw only: Medium.d outlines them while it is > 20), 0..132
  int32_t polyoutline;

  // -- stages 12 and 19: the moving pieces ----------------------------------------
  int32_t pieceglitch;
  bool piecespin[50];
  int32_t spintime[50];
  int32_t origposx[50], origposz[50], origposxz[50];

  // -- stage 17: the undead hunt -----------------------------------------------
  int32_t undeadswitch, undeadtarget;
  bool newtarget, generate, sendwarning;
  int32_t positions[CAREER_STAGE_XT], sortpos[CAREER_STAGE_XT];

  // -- stage 21: the crumbling entry -------------------------------------------
  int32_t startfalling;
  bool crumble;

  // -- stage 22: night, and the shadow that hunts in it -------------------------
  int32_t targetcar;                  // 100 = none
  int32_t teledelay, telecooldown, telewait;
  bool verydark;                      // xtGraphics.verydark: the hunt is on this night
  // Medium's day/night cycle (Medium.js 1615-1665, effect[2]); stepped by
  // career_stage_frame. `dn_dim` is how far the colours are darkened, 0..20
  // (the original lowers Medium.snap by 2 a step: snap = osnap - dn_dim).
  bool dn_on, dn_changingsnap, dn_verydark;
  int32_t dn_makefase, dn_switchfase, dn_effecttime, dn_dim;

  // -- stage 23: the Titan -----------------------------------------------------
  bool bossbattle;
  int32_t cstimer, stunthealth;
  bool unlimitedlaps;                 // the boss fight runs past the laps
  int32_t greystage;                  // Medium.greystage, 1..5 (draw only)

  // -- the start --------------------------------------------------------------
  int32_t tempinv;
  bool invulnerable;

  // -- outputs for game.c (read after each career_stage_tick) -------------------
  // The cars' flame colour (Plane.d 481-490), for ContO.flame_on/flame_rgb:
  // the undead's green, a stage 13 guardian's health colour.
  bool flame_custom[NFM_MAX_CARS];
  int32_t flame_rgb[NFM_MAX_CARS][3];
  int32_t music_event;                // CAREER_MUSIC_*, this tick
  bool sound_redflash;                // teleport warning (redflash.wav), this tick
  bool sound_scare;                   // stage 6's scare (fuucked.wav), this tick
  bool release_hold;                  // stage 23 cstimer 2: game.c clears its hold (holdit) and haltall
  bool hold_by_boss;                  // stage 23 cstimer 2, the player wasted: hold again (holdit = true)
  // The centred flashing message (drawcs(450, msg, r, 0, 0)): "Fix your car
  // at the fix hoop!", "Damage Titan by stunting!", "The undead cars are
  // targeting ...". Empty when none. Red alternates 190/95 each frame.
  char msg[96];
  int32_t msg_r;
  bool flash;                         // xtGraphics.absolutefuckingbullshit
  // The Titan's bar (stage 23, cstimer 4..9999): green 250x20 at (310,5),
  // red fill boss_fill * 250, "<pct> %" at y 22, and the line at y 47
  // cycling level / strength / speed in boss_info_rgb.
  bool boss_bar;
  double boss_fill;
  char boss_pct[24];
  char boss_info[48];
  int32_t boss_info_rgb[3];
  // Bonus 4's last car health bar (clear >= 46): 250 * health_fill wide at
  // (310,5), coloured health_rgb, the car's name at y 23.
  bool health_bar;
  int32_t health_fill;                // 0..250 px
  int32_t health_rgb[3];
  int32_t health_slot;
} CareerStage;

/** What one call needs of the race. Pointers are borrowed. */
typedef struct {
  Mad *mads;                 // [nplayers]
  ContO *cars;               // [nplayers]
  ContO *pieces;             // the stage's objects: the original's conto[nplayers + k]
  int32_t npieces;
  CheckPoints *cp;
  Control *controls;         // [nplayers]
  Medium *m;
  Trackers *t;
  const XtContva *contva;    // completed / biglead / needhelp (NULL reads zeros)
  const CarDefine *orig;     // the cars' own stats before the career's points (game.c's `cd`)
  const bool *botbreak;      // CareerBots.brk (NULL: none broken)
  const char *const *names;  // each slot's name, for the undead warning (NULL: "a car")
  int32_t nplayers;
  // conto[wallcode[k]]: x of the right (maxr) and left (maxl) walls, z of the
  // top (maxt) and bottom (maxb) ones (ExtStageInfo.wallr/walll/wallt/wallb).
  int32_t walls[4];
  int32_t starcnt;           // xtGraphics.starcnt
  int32_t holdcnt;           // xtGraphics.holdcnt (stage 6's wasted player fades after 30, 85)
  bool winner;               // xtGraphics.winner
  bool mutes;                // sound effects off (the redflash/scare sounds are not asked for)
} CareerStageWorld;

/** Race start (resetstat's career fields, then what loadstage leaves). Call
 * after the cars and stage pieces are placed and before the first tick;
 * `w->pieces` must be at their loaded positions (stage 15's footprints are
 * measured here). */
void career_stage_start(CareerStage *cs, const CareerRace *r, const CareerSave *s, const CareerStageWorld *w);
void career_stage_free(CareerStage *cs);

/** Every race tick, before the cars move, the countdown included: Medium's
 * stage 22 day/night cycle (the original steps it in Medium.d, drawn before
 * the frame's physics). */
void career_stage_frame(CareerStage *cs);

/** Every physics tick (starcnt == 0), right before the drive loop: what
 * Madness.drive reads from the car before moving it (specialflag). */
void career_stage_before_drive(CareerStage *cs, const CareerStageWorld *w);

/** careermode$m (and realwalls before it, when starcnt == 0). The original
 * runs it once per frame while starcnt < 38 -- the last 37 countdown ticks
 * and the race -- AFTER that tick's physics (colide, drive, record, checkstat,
 * preform, Contva.sortvariables), nitroandspecials and stat$m:
 * GameSparker.js 2361-2485. In game.c: inside the tick, after specials_tick
 * and the career experience, before mad[k].stunt_gain is zeroed (the Titan
 * takes stunt damage from mad[0].stunt_gain). `run` may be NULL. */
void career_stage_tick(CareerStage *cs, const CareerRace *r, const CareerSave *s, CareerRun *run,
                       const CareerStageWorld *w);

// ---- stat$m (the hold card) -------------------------------------------------

/** stat$m 3992-4002: the undead that do not count towards "all wasted". */
int32_t career_stage_undeadextra(const CareerStage *cs);
/** CheckPoints.checkstat's wasted count in the career: wasted, or stage
 * 11's undead fakedest (CheckPoints 149-155). game.c sets cp.wasted from it
 * right after checkstat, so stat$m's end rules see it the same tick. (The
 * beaten Titan is left out by career_stage_undeadextra instead.) */
int32_t career_stage_wasted(const CareerStage *cs, const Mad *mads, int32_t nplayers);
/** stat$m 4004: "all wasted" may end the race (cstimer < 2 or 10000). */
bool career_stage_wasted_end_allowed(const CareerStage *cs);
/** stat$m 4127: the finish line may end the race (cstimer < 2). */
bool career_stage_finish_end_allowed(const CareerStage *cs);
/** The player won (by racing or wasting). True when it is the Titan's
 * fight instead (stage 23, newest or hard, cstimer < 10000): game.c holds
 * (holdit, haltall) but it is not a win (stat$m 4020-4024, 4142-4146). */
bool career_stage_win_is_boss(CareerStage *cs);
/** The player was wasted (stat$m 4121-4123): the fight is off. */
void career_stage_player_wasted(CareerStage *cs);
/** The hold card is dismissed (Enter, or 250 ticks) while bossbattle:
 * stat$m 3935-3947 -- cstimer 0 -> 2, every car but the Titan wrecked, the
 * player marked damaged. game.c keeps holding; the next tick's
 * career_stage_tick releases it (release_hold). True if it handled it. */
bool career_stage_hold_advance(CareerStage *cs, const CareerStageWorld *w);

// ---- nitroandspecials' stage stat effects ----------------------------------------

/** True when the stage changes the cars' stats every tick (stage 9's fire,
 * stage 11's drain, stage 20's speed cut, bonus 4's), so game.c must
 * rebuild them with career_stage_stats. */
bool career_stage_has_stat_effects(const CareerStage *cs);
/** nitroandspecials 6925-7051 for slot `slot`: `base` (game.c's race_base,
 * what specials_tick builds the live stats from) becomes `orig` with the
 * career's points (career_apply_stats, the beast's x3 power loss and x2
 * reach) and the stage's effects in. `live_grip` is the car's grip right
 * now (madness.grip, last tick's). Call once per tick before specials_tick.
 * Also sets powermulti (stage 9) and nuclearmod (stage 11). */
void career_stage_stats(CareerStage *cs, const CareerRace *r, int32_t slot, CarDefine *base, const CarDefine *orig,
                        int32_t cn, float live_grip, const CheckPoints *cp);

// ---- GameSparker's collision gating ---------------------------------------------

/** GameSparker.js 2134-2227: ghost[a][b] true when car a must not collide
 * with b this tick (game.c skips mad_colide(a, b) when ghost[a][b] ||
 * ghost[b][a]). */
void career_stage_ghostmode(const CareerStage *cs, const CareerRace *r, const CareerStageWorld *w,
                            bool ghost[NFM_MAX_CARS][NFM_MAX_CARS]);

// ---- Madness.drive's career parts (for mad.c; all safe with cs == NULL) -------------

/** Madness 1885-1899: the pull down per tick (7; stage 15 2 off the track /
 * 10 on it; stage 21 50 before the entry; stage 23 25 for the player in the
 * fight). */
static inline float career_phys_gravity(const CareerStage *cs, int32_t im);
/** Madness.groundlevel: where the ground is (250; stage 13's floors). */
static inline float career_phys_groundlevel(const CareerStage *cs, int32_t im);

/** Stage 24 (not car 19): the water's drag, from the car's grip (Madness
 * 1332-1344, 1449-1462, 1594-1605, 1788-1799). All 1 elsewhere. */
typedef struct {
  float bouncemod;   // bounce x this
  float waterdrag;   // the stunt controls (airs) x this
  double aircres;    // the air control (airc) x this
  float accelmod;    // the acceleration steps x this
  double turnmod;    // the steering x this
} CareerWater;
static inline CareerWater career_phys_water(const CareerStage *cs, int32_t car, float grip);

/** Madness 1976-2021, on the ground after the road type's own scaling:
 * stage 16's ice, stage 18's sand, stage 24's water change the grip `*f7`
 * and set the car's speedmulti / powermulti (kept here). `car` is the
 * Extended car number; `roadtyp` the surface under the car. */
static inline void career_phys_grip(CareerStage *cs, int32_t im, int32_t car, float grip, int32_t roadtyp, float *f7);
static inline float career_phys_speedmulti(const CareerStage *cs, int32_t im);
static inline float career_phys_powermulti(const CareerStage *cs, int32_t im);

/** xtGraphics.entered (stage 21's arena, stage 6's ghost ride): a landed
 * surf counts no extra (Madness 2837). */
static inline bool career_phys_entered(const CareerStage *cs, int32_t im);
/** Stage 21: a car inside the arena keeps its full power (Madness 1568). */
static inline bool career_phys_noslow(const CareerStage *cs, int32_t im);
/** Stage 15 off the track: diving pushes along the heading instead of
 * down (Madness 1519-1527). */
static inline bool career_phys_outoftrack(const CareerStage *cs, int32_t im);
/** A stunt landed (Madness 2844-2847): stage 21's entry count. */
static inline void career_phys_stunt_landed(CareerStage *cs, int32_t im);
/** Stage 21's entry jump fills no special (Madness 2868). */
static inline bool career_phys_no_special_gain(const CareerStage *cs, int32_t im);
/** The special bar's cap for the stunt gain (Madness 2873, 3009): 125 from stage 22 on. */
static inline float career_phys_splimit(const CareerStage *cs);
/** The trick counter reset (Madness 2942): the entry jump is spent. */
static inline void career_phys_trick_reset(CareerStage *cs, int32_t im);
/** Madness 3527-3567: the car can not be fixed (hoops or fix checkpoints). */
static inline bool career_phys_nofix(const CareerStage *cs, const Mad *mad);
/** The wreck (Madness 3119-3172). career_phys_undead: the car is never
 * wrecked (hitmag past maxmag does nothing). career_phys_fakedest_stage:
 * on stage 11 an opponent's wreck tests and ends in cs->fakedest[im] in
 * place of mad->dest -- at cntdest 7 career_phys_wreck_done makes it undead
 * (and counted as wasted) instead of setting dest. */
static inline bool career_phys_undead(const CareerStage *cs, int32_t im);
static inline bool career_phys_fakedest_stage(const CareerStage *cs, int32_t im);
static inline void career_phys_wreck_done(CareerStage *cs, Mad *mad);
/** Stage 11's vans 2..4 ignore the track pieces (Madness 2224). */
static inline bool career_phys_no_trackers(const CareerStage *cs, int32_t im);
/** Hitting a wall (Madness 2240-2297): the career's wall damage factor
 * (field level, the car's grip against the field's, bonus 4, stage 6's
 * ghost-hit immunity). `grip` is the car's grip now. */
static inline float career_phys_wall_damage(const CareerStage *cs, int32_t im, float grip);
/** Stage 13: a checkpoint on another floor does not count (Madness 3437, 3476, 3499). */
static inline bool career_phys_rightfloor(const CareerStage *cs, int32_t im, int32_t cpfloor);
/** Madness.xz (1331): conto.xz with the wraps careermode$m took off, for the spin count. */
static inline int32_t career_phys_xz(const CareerStage *cs, int32_t im, int32_t contoxz);
/** A teleported car does not skid (Madness 2148). */
static inline bool career_phys_speedhack(const CareerStage *cs, int32_t im);
/** Stage 13: braking into a portal (Madness 1606-1609, 1610, 1671, 1694-1705).
 * `career_phys_forcehandb`: up and down are ignored. career_phys_brake
 * returns the handbrake step for this tick (the car's handb, or a twentieth
 * of the speed it came in with) and counts the portal's timer; `*apply`
 * true when the brake acts (handb held or the portal's). Call where the
 * ground branch brakes, once per tick on the ground. */
static inline bool career_phys_forcehandb(const CareerStage *cs, int32_t im);
static inline void career_phys_ground_speed(CareerStage *cs, int32_t im, float speed);
static inline float career_phys_brake(CareerStage *cs, int32_t im, float handb, bool handb_held, bool *apply);

/** Stage 13's portals, end of Madness.drive (3252-3349): the fade and the
 * move to the next floor, the guardians. mad.c runs it inside drive()
 * (career_phys_portal_move); this is the same for one slot of a race. */
void career_stage_portal_move(CareerStage *cs, const CareerStageWorld *w, int32_t im);
/** Madness 3353-3361: driving into a portal checkpoint starts the brake.
 * mad.c checks each checkpoint inside its loop (career_phys_portal_check);
 * this runs the whole loop for one slot after the fact. */
void career_stage_portal_detect(CareerStage *cs, const CareerStageWorld *w, int32_t im);

/** The portal parts of Madness.drive, inline for mad.c: Madness.teleport
 * (3691), the move at the end of drive (3252-3349), one checkpoint's
 * portal test (3353-3361), and a checkpoint cleared while braking in
 * (3379, 3401: the bot gets its recording back on arrival). */
static inline void career_phys_madness_teleport(const CareerStage *cs, ContO *co, int32_t user);
static inline void career_phys_portal_move(CareerStage *cs, Mad *mad, ContO *co, Control *c, const CheckPoints *cp);
static inline void career_phys_portal_check(CareerStage *cs, const Mad *mad, const ContO *co, const CheckPoints *cp,
                                            int32_t j);
static inline void career_phys_portal_cleared(CareerStage *cs, int32_t im);

/** Madness.respawn (3659-3689): back to the last checkpoint, fixed; the car
 * resets instead of driving next tick (career_stage_respawn_reset). */
void career_stage_respawn(CareerStage *cs, Mad *mad, ContO *co, const CheckPoints *cp);
/** GameSparker 2368-2373: a respawning car resets (Madness.reseto keeps its
 * laps, checkpoints and power) instead of driving. game.c calls it in the
 * drive loop in place of mad_drive when cs->respawning[k]. */
void career_stage_respawn_reset(CareerStage *cs, Mad *mad, int32_t cn, ContO *co, CheckPoints *cp);

/** What the career AI (control.c) reads of this state: undead, entered,
 * floor, undeadlock, undeadtarget, targetcar, verydark, bossbattle,
 * invulnerable, nostunts, nofix, groundlevel, floorguardian, guardswitch.
 * game.c calls it after career_stage_tick, before the next preform. */
void career_stage_export_ai(const CareerStage *cs, const CareerStageWorld *w, XtCareerAI *ai);

/** Test hook: when set, used in place of medium_random(m). */
extern float (*career_stage_mrandom_hook)(Medium *m);

// ---- the career_phys_* bodies (inline: mad.c calls them every tick) ----------
static inline bool career_slot_ok(const CareerStage *cs, int32_t im) { return cs && im >= 0 && im < NFM_MAX_CARS; }

static inline float career_phys_gravity(const CareerStage *cs, int32_t im) {
  float gravity = 7.0f;
  if (!career_slot_ok(cs, im)) return gravity;
  if (cs->stage == 15 && !cs->bonus) gravity = cs->outoftrack[im] ? 2.0f : 10.0f;
  if (cs->stage == 21 && ((!cs->entered[im] && cs->nostunts[im] <= 1) || cs->crumblefail[im])) gravity = 50.0f;
  if (cs->stage == 23 && cs->bossbattle && cs->cstimer >= 4 && im == 0) gravity = 25.0f;
  return gravity;
}

static inline float career_phys_groundlevel(const CareerStage *cs, int32_t im) {
  return career_slot_ok(cs, im) ? cs->groundlevel[im] : 250.0f;
}

/** MD 1334-1341 (and its copies): the grip's share of the water's effects. */
static inline float career_water_affect(float grip) {
  float gripmod = (grip - 28.5f) / 100.0f;
  if (gripmod < 0.55f) gripmod = 0.55f;
  if (gripmod > 1.0f) gripmod = 1.0f;
  return (gripmod - 0.55f) / 0.45f;
}

static inline CareerWater career_phys_water(const CareerStage *cs, int32_t car, float grip) {
  CareerWater wv = {1.0f, 1.0f, 1.0, 1.0f, 1.0};
  if (!cs || cs->stage != 24 || car == 19) return wv;
  const float ga = career_water_affect(grip);
  wv.bouncemod = 1.5f - ga * 0.45f;
  wv.waterdrag = 0.2f + 0.6f * ga;
  wv.aircres = 0.5 + (double)(0.35f * ga);
  wv.accelmod = 0.25f + 0.55f * ga;
  wv.turnmod = 0.7 + (double)(ga * 0.25f);
  return wv;
}

static inline void career_phys_grip(CareerStage *cs, int32_t im, int32_t car, float grip, int32_t roadtyp, float *f7) {
  if (!career_slot_ok(cs, im)) return;
  if (cs->stage == 16 && car != 3 && car != 15 && car != 38) {
    float gripmod4 = (grip - 31.0f) / 54.0f;
    if (gripmod4 < 0.5f) gripmod4 = 0.5f;
    if (gripmod4 > 1.0f) gripmod4 = 1.0f;
    const float ga = (gripmod4 - 0.5f) / 0.5f;
    if (roadtyp == 0) {
      cs->speedmulti[im] = 0.8f + 0.1f * ga;
      *f7 = *f7 * (0.35f + ga * 0.5f);
    }
    if (roadtyp == 1) {
      cs->speedmulti[im] = 0.25f + 0.65f * ga;
      *f7 = *f7 * (0.6f + ga * 0.1f);
    }
    if (roadtyp == 2 || roadtyp == 3 || roadtyp == 4) {
      cs->speedmulti[im] = 0.8f + 0.1f * ga;
      *f7 = *f7 * (0.25f + ga * 0.2f);
    }
  }
  if (cs->stage == 18 && car != 16 && cs->bonus != 4) {
    float gripmod4 = (grip - 33.5f) / 59.0f;
    if (gripmod4 < 0.5f) gripmod4 = 0.5f;
    if (gripmod4 > 1.0f) gripmod4 = 1.0f;
    const float ga = (gripmod4 - 0.5f) / 0.5f;
    cs->speedmulti[im] = 0.45f + ga * 0.45f;
    cs->powermulti[im] = 2.5f - ga * 1.3f;
  }
  if (cs->stage == 24 && car != 19) cs->speedmulti[im] = 0.8f + career_water_affect(grip) * 0.2f;
}

static inline float career_phys_speedmulti(const CareerStage *cs, int32_t im) { return career_slot_ok(cs, im) ? cs->speedmulti[im] : 1.0f; }
static inline float career_phys_powermulti(const CareerStage *cs, int32_t im) { return career_slot_ok(cs, im) ? cs->powermulti[im] : 1.0f; }

static inline bool career_phys_entered(const CareerStage *cs, int32_t im) { return career_slot_ok(cs, im) && cs->entered[im]; }

static inline bool career_phys_noslow(const CareerStage *cs, int32_t im) {
  return career_slot_ok(cs, im) && cs->stage == 21 && cs->entered[im];
}

static inline bool career_phys_outoftrack(const CareerStage *cs, int32_t im) {
  return career_slot_ok(cs, im) && cs->stage == 15 && !cs->bonus && cs->outoftrack[im];
}

static inline void career_phys_stunt_landed(CareerStage *cs, int32_t im) {
  if (career_slot_ok(cs, im) && cs->stage == 21 && !cs->entered[im]) cs->nostunts[im]++;
}

static inline bool career_phys_no_special_gain(const CareerStage *cs, int32_t im) {
  return career_slot_ok(cs, im) && cs->stage == 21 && !cs->entered[im] && cs->nostunts[im] <= 1;
}

static inline float career_phys_splimit(const CareerStage *cs) { return cs && cs->stage >= 22 ? 125.0f : 100.0f; }

static inline void career_phys_trick_reset(CareerStage *cs, int32_t im) {
  if (career_slot_ok(cs, im) && cs->stage == 21 && !cs->entered[im] && cs->nostunts[im] <= 1) cs->nostunts[im] = 2;
}

static inline bool career_phys_nofix(const CareerStage *cs, const Mad *mad) {
  if (!cs) return false;
  const int32_t im = mad->im;
  if (!career_slot_ok(cs, im)) return false;
  bool nofix = false;
  const float health = (100.0f * (float)mad->hitmag) / (float)mad->cd->maxmag[mad->cn];
  if (cs->bonus == 2 && cs->beast[im] && health < 85.0f && cs->car[im] == 36) nofix = true;
  if (cs->bonus == 3 && im > 0) nofix = true;
  if (cs->bonus == 4 && (im > 0 || mad->clear < 26)) nofix = true;
  if (cs->undead[im]) nofix = true;
  if (cs->stage == 13 && cs->beast[im] && health <= 70.0f) nofix = true;
  if (cs->stage == 19 || cs->stage == 6) {
    float benchmark = 75.0f;
    if (cs->stage == 19) {
      benchmark = 50.0f;
      if (cs->shadow[im]) benchmark = 70.0f;
    }
    nofix = (cs->beast[im] || cs->shadow[im]) && health <= benchmark;
  }
  if (cs->stage == 23 && cs->bossbattle && cs->cstimer >= 4 && im > 0) nofix = true;
  return nofix;
}

static inline bool career_phys_undead(const CareerStage *cs, int32_t im) { return career_slot_ok(cs, im) && cs->undead[im]; }

static inline bool career_phys_fakedest_stage(const CareerStage *cs, int32_t im) {
  return career_slot_ok(cs, im) && cs->stage == 11 && cs->bonus != 2 && im != 0;
}

static inline void career_phys_wreck_done(CareerStage *cs, Mad *mad) {
  if (!career_slot_ok(cs, mad->im)) return;
  cs->undead[mad->im] = true;
  cs->fakedest[mad->im] = true;
}

static inline bool career_phys_no_trackers(const CareerStage *cs, int32_t im) {
  return cs && cs->stage == 11 && cs->bonus != 2 && im >= 2 && im <= 4;
}

static inline float career_phys_wall_damage(const CareerStage *cs, int32_t im, float grip) {
  if (!cs) return 1.0f;
  const int32_t al = cs->averagelevel;
  float wallmulti = 1.0f;
  if (al >= 8 && cs->stage != 14 && cs->stage != 10) {
    if (al <= 58) wallmulti = 1.0f + (float)(al - 8) * 0.06f;
    else wallmulti = 4.0f + (float)(al - 58) * 0.04f;
  }
  if (cs->bonus == 4) wallmulti = im == 0 ? 10.0f : 4.0f;
  float reddmg = 1.0f;
  const float expectgrip = cs->avgstartgrip + (float)(al - 1) * 0.2f;
  if (grip >= expectgrip) {
    const float mainboistat = (grip - 10.0f) / 20.0f, targetstat = (expectgrip - 10.0f) / 20.0f;
    const float difference = (mainboistat - targetstat) * 37.0f;
    reddmg = 1.0f - difference / 20.0f;
    if (reddmg < 0.2f) reddmg = 0.2f;
  } else {
    const float mainboistat = (grip - 10.0f) / 20.0f, targetstat = (expectgrip - 10.0f) / 20.0f;
    const float difference = (targetstat - mainboistat) * 37.0f;
    reddmg = 1.0f + difference / 20.0f;
    if (reddmg > 1.5f) reddmg = 1.5f;
  }
  float totaldmgmod = wallmulti * reddmg;
  if (im == 0 && cs->stage == 6) {
    int32_t thelevel = al + 2;
    if (thelevel > 20) thelevel = 20;
    const float cap = 0.35f + (float)thelevel * 0.05f;
    if (cs->wallimmunity > 0 && totaldmgmod > cap) totaldmgmod = cap;
  }
  return totaldmgmod;
}

static inline bool career_phys_rightfloor(const CareerStage *cs, int32_t im, int32_t cpfloor) {
  return !(career_slot_ok(cs, im) && cs->stage == 13 && cpfloor != cs->floor[im]);
}

static inline int32_t career_phys_xz(const CareerStage *cs, int32_t im, int32_t contoxz) {
  return career_slot_ok(cs, im) ? contoxz + cs->xzadjust[im] * 360 : contoxz;
}

static inline bool career_phys_speedhack(const CareerStage *cs, int32_t im) { return career_slot_ok(cs, im) && cs->speedhack[im] > 0; }

static inline bool career_phys_forcehandb(const CareerStage *cs, int32_t im) { return career_slot_ok(cs, im) && cs->forcehandb[im]; }

static inline void career_phys_ground_speed(CareerStage *cs, int32_t im, float speed) {
  if (!career_slot_ok(cs, im) || cs->forcehandb[im]) return;
  cs->initialspeed[im] = speed;
  cs->teletimer[im] = 0;
}

static inline float career_phys_brake(CareerStage *cs, int32_t im, float handb, bool handb_held, bool *apply) {
  float changeby = handb;
  const bool force = career_slot_ok(cs, im) && cs->forcehandb[im];
  if (force) {
    changeby = fabsf(cs->initialspeed[im]) / 20.0f;
    cs->teletimer[im]++;
  }
  *apply = handb_held || force;
  return changeby;
}

static inline void career_phys_madness_teleport(const CareerStage *cs, ContO *co, int32_t user) {
  co->xz = 0 + 180 * (cs->specialflag[user] ? 1 : 0);
}

static inline void career_phys_portal_move(CareerStage *cs, Mad *mad, ContO *co, Control *c, const CheckPoints *cp) {
  const int32_t im = mad->im;
  if (!career_slot_ok(cs, im)) return;
  const int32_t car = cs->car[im];
  if (cs->forcehandb[im]) {
    cs->tele_fading[im] = true;
    if (cs->telefade[im] >= 15) cs->telefade[im] -= 15;
    if (cs->teletimer[im] >= 20) {
      if (!mad->capsized && mad->mtouch) {
        mad->speed = 0.0f;
        mad->pxy = 0;
        mad->pzy = 0;
        career_phys_madness_teleport(cs, co, im);
        if (cs->slowstable[im] >= 4 && cs->telefade[im] < 15) {
          if ((!mad->specialact || car == 13 || car == 36) && cs->telechk[im]) {
            cs->portal_unbreak[im] = true;
            cs->telechk[im] = false;
          }
          const int32_t f = cs->sendtofloor[im];
          cs->floor[im] = f;
          cs->speedhack[im] = 10;
          cs->teleinvul[im] = im == 0 ? 30 : 10;
          if (f > 0) c->setfixfloor = true;
          mad->xtpower = 100;
          co->x = f == 1 ? -5000 : 0;
          co->z = 0;
          co->y = f == 0 ? 250 - co->grat : -(f * 10000) - co->grat;
          int32_t whichset = (3 - f) * 2 + 1;
          if (mad->spatk == 120.0f && !mad->specialact) whichset = (3 - f) * 2 + 2;
          if (car == 13 || car == 36) whichset = (3 - f) + 1;
          cs->portal_arrived[im] = true;
          cs->portal_whichset[im] = whichset;
          cs->telefade[im] = 255;
          cs->tele_fading[im] = false;
          c->down = false;
          c->left = false;
          c->right = false;
          c->handb = false;
          cs->forcehandb[im] = false;
        }
      }
      if (mad->speed == 0.0f && mad->pxy == 0 && mad->pzy == 0) cs->slowstable[im]++;
    }
  } else {
    cs->teleinvul[im]--;
    cs->slowstable[im] = 0;
  }
  if (cs->stage == 13) {
    // m.effect[9] (MD 3320-3333)
    cs->floorguardian[im] = !(im % 3 == 1 || im == 0 || im > 9);
    if (cs->speedhack[im] > 0) {
      co->x = cs->sendtofloor[im] == 1 ? -5000 : 0;
      career_phys_madness_teleport(cs, co, im);
    }
  }
  if (cs->floorguardian[im]) {
    bool nomercy = true;
    for (int32_t a7 = 1; a7 < cs->nplayers; a7++) {
      if ((a7 == 1 || a7 == 4 || a7 == 7 || a7 >= 10) && cp->dested[a7] == 0) {
        nomercy = false;
        break;
      }
    }
    cs->guardswitch[im] = true;
    if ((cp->clear[0] >= 13 && cs->hard) || nomercy) cs->guardswitch[im] = false;
  }
}


static inline void career_phys_portal_check(CareerStage *cs, const Mad *mad, const ContO *co, const CheckPoints *cp,
                                            int32_t j) {
  const int32_t im = mad->im;
  if (!career_slot_ok(cs, im) || cp->telefloor[j] <= -1 || cs->floorguardian[im] || cs->forcehandb[im]) return;
  const float scz = fabsf(((mad->scz[0] + mad->scz[1]) + mad->scz[2]) + mad->scz[3]) / 4.0f + 60.0f;
  const float scx = fabsf(((mad->scx[0] + mad->scx[1]) + mad->scx[2]) + mad->scx[3]) / 4.0f + 60.0f;
  if (cp->rotation[j] % 180 == 0 && (double)abs(co->z - cp->z[j]) < (double)scz && abs(co->x - cp->x[j]) < 700 &&
      abs(co->y - cp->y[j]) < 800) {
    cs->sendtofloor[im] = cp->telefloor[j];
    cs->forcehandb[im] = true;
  }
  if (cp->rotation[j] % 90 == 0 && (double)abs(co->x - cp->x[j]) < (double)scx && abs(co->z - cp->z[j]) < 700 &&
      abs(co->y - cp->y[j]) < 800) {
    cs->sendtofloor[im] = cp->telefloor[j];
    cs->forcehandb[im] = true;
  }
}

static inline void career_phys_portal_cleared(CareerStage *cs, int32_t im) {
  if (career_slot_ok(cs, im) && cs->forcehandb[im]) cs->telechk[im] = true;
}

#ifdef __cplusplus
}
#endif

#endif
