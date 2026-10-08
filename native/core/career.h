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
#define CAREER_PERKS 40         // xtGraphics.statnames
#define CAREER_PERK_SLOTS 6     // each car's six (statsalc)
#define CAREER_PERK_MAX 20      // points per perk

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
  int32_t statchangers[2];          // free reshuffles, level transfers (one each per stage won)
  int32_t carpoints;
  // Car points spent on each of a car's six perks, 0-20, by slot: the
  // original's specialstats[car][statsalc[car][slot]][slot].
  int32_t perk[CAREER_CARS][CAREER_PERK_SLOTS];
  double rebsp[CAREER_CARS], xbsp[CAREER_CARS];   // level transfers' bonus-point ratios (1, 0 untouched)
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
  int32_t bonuspoints[CAREER_MAX_PLAYERS];   // each opponent's bonus stat points (scouting shows them)
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

/** Which perk each of a car's six slots holds (xtGraphics.statsalc), by
 * Extended car number; NFM 2's cars (23-30) and 31 hold ENERGY six times. */
extern const int8_t career_statsalc[CAREER_CARS][CAREER_PERK_SLOTS];
/** xtGraphics.statnames. */
extern const char *const career_perk_name[CAREER_PERKS];
/** writeboosts' text for a perk: up to two lines of what it does, up to two
 * of its effect at the maximum (NULL when absent). */
typedef struct {
  const char *what[2];
  const char *max[2];
} CareerPerkText;
extern const CareerPerkText career_perk_text[CAREER_PERKS];
/** The original describes 40 perks but applies only these 24 (0-7, 9-24). */
bool career_perk_applied(int32_t perk);

/** The player's perks as the race reads them (Madness and nitroandspecials
 * look them up by car: xt.specialstats). NULL outside the career. */
typedef struct CareerPerks {
  int8_t special[CAREER_CARS][CAREER_PERKS][CAREER_PERK_SLOTS];   // xt.specialstats, from the save's perk[][]
  int32_t car[CAREER_MAX_PLAYERS];                   // each racing slot's Extended car
  bool beast[CAREER_MAX_PLAYERS], shadow[CAREER_MAX_PLAYERS];
  int32_t killtime[2];   // BERSERK's and SAFETY's ticks left after a waste (xt.killtime)
} CareerPerks;

/** specialstats[car][perk][slot]: the points in `perk` if slot `slot` holds it. */
static inline int32_t career_specialstat(const CareerPerks *p, int32_t car, int32_t perk, int32_t slot) {
  if (!p || car < 0 || car >= CAREER_CARS) return 0;
  return p->special[car][perk][slot];
}

/** xtGraphics.resetstats: bonus car `car`'s (31-38) own points, plus
 * `addon` on each; other cars are left alone. */
void career_resetstats(CareerSave *s, int32_t car, int32_t addon);
/** The car select gives a bonus car its own points while it has none
 * (XT 15885-15887). */
void career_bonus_car_points(CareerSave *s, int32_t car);
/** A car point into slot `slot`'s perk (XT 15746-15753); false when there is
 * no point to spend or the perk is full. */
bool career_spend_perk(CareerSave *s, int32_t car, int32_t slot);
/** RESHUFFLE STATS (carselect shufflefase 2 and 4, XT 16199-16205,
 * 16408-16437): the stat points it costs (none while a free reshuffle is
 * left), and the reshuffle -- every spent point back, the bonus ones less
 * that cost, one free reshuffle used. */
int32_t career_reshuffle_cost(const CareerSave *s, int32_t car);
void career_reshuffle(CareerSave *s, int32_t car);
/** SELL CAR / RESET CAR (XT 16151-16160, 17340-17361): level / 3 car points
 * (the original pays them before bonus stage 4 too, though its text says
 * none), and the car back to level 1. Its perks stay. */
int32_t career_sell_price(const CareerSave *s, int32_t car);
void career_sell(CareerSave *s, int32_t car);
/** LEVEL TRANSFER (shufflefase 7 and 8, XT 16559-16627): `to` takes `from`'s
 * level, counters and bonus points (scaled by the cars' xbspratio), `from`
 * goes back to level 1; one transfer used. */
void career_transfer(CareerSave *s, int32_t from, int32_t to);

/** Scouting's six numbers for a car (xtGraphics.scouting XT 9843-9860):
 * top speed, acceleration, control, stunting, strength, defence, from the
 * car's own tables (`cn` its slot in `cd`, `car` its Extended number) and
 * its points. */
void career_scout_stats(const CarDefine *cd, int32_t cn, int32_t car, const int32_t sp[CS_N], int32_t out[CS_N]);

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
  // Bonus stat points (stat$m XT 5030-5120, 5320-5420; awarded as their
  // popups come up, 5531-5835): each checkpoint and waste rolls [0] out of
  // 1000 against its chance, a hit rolls [1] for the award; 1001 none.
  int32_t winchance[2], killchance[2];
  bool rcestatgain, wststatgain;   // this popup's points are in
  // The two popups' slide (xkcnt, showfor, xmoveback, xmove) and the points
  // each shows: [0] the waste's (xkcnt[2]), [1] the checkpoint's (xkcnt[5]).
  int32_t pop_x[2], pop_show[2], pop_amount[2];
  bool pop_back[2];
  CareerPerks perks;
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
/** The bonus stat point popups, every race tick after the above: they
 * slide in and out and pay their points as they first show. */
void career_popups_tick(CareerRun *run, const CareerRace *r, CareerSave *s);
/** GREED's experience factor, GAMBLER's chance factor (1 without the perk). */
double career_perk_mod(const CareerSave *s, int32_t car, int32_t perk);
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
