// Need for Madness 2 Extended v2.8's career (its "RPG MODE"): 31 stages of
// careertracks.radq, levels and experience per car, stat points, opponents
// levelled per stage. Map of the original: docs/extended-career.md. Pure
// logic -- game.c draws and races. Car numbers here are EXTENDED's (0-38;
// ext_car_of / ext_car_to_port convert), as the original's tables are.
#ifndef NFM_CAREER_H
#define NFM_CAREER_H

#include <stdbool.h>
#include <stdint.h>

#include "car_define.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CAREER_STAGES 31
#define CAREER_CARS 39
#define CAREER_MAX_PLAYERS 20   // nplayers tops at 19 (stages 3, 17)

/** The six stats points go into, in the original's order: top speed,
 * acceleration, grip (handling), stunts, strength, endurance (Madness's
 * aitssp, aiaccsp, aigripsp, aistusp, aistrsp, aiendsp). */
enum { CS_TS, CS_ACC, CS_GRIP, CS_STU, CS_STR, CS_END, CS_N };

/** What the player keeps between races (the save). */
typedef struct {
  int32_t unlocked;                 // highest career stage open, 1..31 (xtGraphics.unlocked[1])
  int32_t laststage, lastcar;       // where the player left off
  int32_t level[CAREER_CARS], exp[CAREER_CARS];
  int32_t statpoints[CAREER_CARS];  // unspent
  int32_t sp[CAREER_CARS][CS_N];    // spent, per stat
  int32_t killscn[CAREER_CARS], winscn[CAREER_CARS], extpoints[CAREER_CARS];
  int32_t kills, wins;
  int32_t boncomp[6];               // bonus stages done (bonus 2: 1 by racing, 2 by wasting, 3 both)
  int32_t statchangers[2];
  int32_t carpoints;
} CareerSave;

/** One race's setup: who races and how strong each is. Slot 0 is the player. */
typedef struct {
  int32_t stage;                    // 1..31 (on a bonus stage, the stage it hangs off: 5/11/15/18)
  int32_t bonus;                    // 0 none, 1..4 the bonus stage
  bool hardstage, scalelevels, nolevels;
  int32_t nplayers;
  int32_t noshadows;                // randomno()'s shadow count
  int32_t sc[CAREER_MAX_PLAYERS];   // each slot's car (Extended's numbers)
  bool beast[CAREER_MAX_PLAYERS];   // beastopponent[]
  bool shadow[CAREER_MAX_PLAYERS];  // Madness.shadowcar
  bool undead[CAREER_MAX_PLAYERS];
  int32_t level[CAREER_MAX_PLAYERS];
  int32_t sp[CAREER_MAX_PLAYERS][CS_N];
  int32_t softlevelcap, averagelevel;
} CareerRace;

/** The original's random sources: Math.random() and Medium.random(), both
 * uniform [0,1). Tests replace them to replay a Java/JS run. */
extern double (*career_random)(void);
extern double (*career_mrandom)(void);

void career_reset(CareerSave *s);
bool career_load(const char *path, CareerSave *s);
bool career_save(const char *path, const CareerSave *s);

/** 0 when Extended car `car` is open in the career; else the stage whose
 * win opens it, -k for bonus stage k's prize, 99 for never (the secret
 * cars only a save opens). */
int32_t career_car_lock(const CareerSave *s, int32_t car);

/** Experience to the next level for car `car` at `level` (xtGraphics.reqneed). */
int32_t career_reqneed(int32_t level, int32_t car);

/** Stat points a level is worth (spcalc), and a beast's (beastspcalc). */
int32_t career_spcalc(int32_t level);
int32_t career_beastspcalc(int32_t level);

/** healthcalc: maxmag from the car's own and its endurance points. */
int32_t career_healthcalc(int32_t initialhealth, int32_t statpoints, int32_t car, float modifier);

/** xtGraphics.randomno (career): nplayers and noshadows for r->stage. */
void career_randomno(CareerRace *r, const CareerSave *s);

/** xtGraphics.sortcars (career and bonus branches), then beasts() and
 * sortshadows(): fills r->sc[1..], r->beast[], r->shadow[], r->undead[].
 * r->sc[0] is the player's car. */
void career_sortcars(CareerRace *r, const CareerSave *s);

/** xtGraphics.airpgstats: every opponent's level and stat points (slots
 * 1..nplayers-1); slot 0 takes the player's from `s`. `gripreset[car]` is
 * each car's base grip (Madness.gripreset). Also sets softlevelcap and
 * averagelevel. */
void career_airpgstats(CareerRace *r, const CareerSave *s, const float *gripreset);

/** A racing car's stats with its points in (nitroandspecials' career
 * rebuild): `cn` is the slot in `cd` (this port's car number), `car` the
 * Extended number. `player`: the bonus cars' handicap applies. */
void career_apply_stats(CarDefine *cd, int32_t cn, int32_t car, const int32_t sp[CS_N], int32_t level, bool shadow,
                        bool player);

/** One race's experience bookkeeping for the player (careermode$m, stat$m). */
typedef struct {
  int32_t startinglevel;
  double expmult;          // 1, or less past the field's level (softlevelcap)
  bool isithard, noexp;
  int32_t startexp, startsp, statgain;   // what losing the newest stage gives back
  bool losepoints, winfix;
  double fullpownit;
  int32_t powxpadjust;
  int32_t expneeded;       // to the next level
  int32_t levelups;        // this race's
  int32_t last_gain;       // the last award, for the HUD
} CareerRun;

void career_run_start(CareerRun *run, const CareerRace *r, const CareerSave *s);
/** Every race tick: experience rate, full-power experience, level-ups.
 * `started`: the countdown is over; `full_power`: the player's power is 98
 * and it is moving, not wasted. */
void career_tick(CareerRun *run, const CareerRace *r, CareerSave *s, bool started, bool full_power);
/** The player cleared a checkpoint (`clear0` its count so far). */
void career_xp_checkpoint(CareerRun *run, const CareerRace *r, CareerSave *s, int32_t clear0);
/** The player wasted slot `k`. */
void career_xp_waste(CareerRun *run, const CareerRace *r, CareerSave *s, int32_t k);
/** The player won, by racing or by wasting. */
void career_xp_win(CareerRun *run, const CareerRace *r, CareerSave *s, int32_t nsp, int32_t nlaps, bool racing);
/** The player lost (finished behind, or wasted). */
void career_xp_lose(CareerRun *run, const CareerRace *r, CareerSave *s);
/** A landed stunt's powerup, by the stunt stat (career_stunt_stat). */
void career_xp_stunt(CareerRun *run, const CareerRace *r, CareerSave *s, float powerup, int32_t stat3);
int32_t career_stunt_stat(const CarDefine *cd, int32_t cn, const int32_t sp[CS_N]);

/** Slot `j`'s start (GameSparker.loadstage's grid in the career): x, y
 * (the car's ground offset `grat` taken off) and z, and the floor it starts
 * on (stage 13's tower, 0 elsewhere). */
void career_grid(const CareerRace *r, const CareerSave *s, int32_t j, int32_t grat, int32_t *x, int32_t *y, int32_t *z,
                 int32_t *floor);

/** The player's power factor (0.76 outside career, Madness.drive). */
double career_power_factor(int32_t car, const int32_t sp[CS_N], int32_t level);

#ifdef __cplusplus
}
#endif

#endif
