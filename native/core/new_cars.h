// Cars beyond the game's 39: Revised and Recharged's 25 (ext/recharged/
// models.radq) and any .rad the player drops in the SD card's cars folder.
// Each is a Car Maker car: its model and its stat()/physics()/handling()
// lines through car_define_loadcar (NFM 2's formulas), plus the Car Maker's
// Extended lines -- extspecial(n) borrows Extended car n's special and AI
// quirks (its "donor"; by class when missing), exthealth(p) and extdamage(p)
// scale health and damage taken. As the web port's newcars*.js.
#ifndef NFM_NEW_CARS_H
#define NFM_NEW_CARS_H

#include <stdbool.h>
#include <stdint.h>

#include "car_define.h"
#include "cont_o.h"
#include "medium.h"
#include "trackers.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NEW_CAR_FIRST 40   // this port's number of the first one (39 is the custom car)
#define NEW_CARS_MAX 64

typedef struct {
  char name[48];
  int32_t donor;      // this port's number of the car whose special and quirks it takes
  bool from_sd;       // the player's own, not Revised and Recharged's
} NewCarInfo;

extern int32_t g_new_car_count;
extern NewCarInfo g_new_cars[NEW_CARS_MAX];

/** Who a car counts as for specials and the AI: itself, or a new car's donor. */
int32_t car_identity(int32_t cn);

/** Loads R&R's cars, then the .rad files in `sd_dir` (NULL: none), into
 * models[0..] and cd slots NEW_CAR_FIRST..; returns how many. A file that
 * is not a car the game can drive is skipped with a line on stderr. */
int32_t new_cars_load(ContO *models, CarDefine *cd, Medium *m, Trackers *t, const char *sd_dir);

/** One .rad text as new car number `i` (models[i], slot NEW_CAR_FIRST + i);
 * false when it is not a drivable car. `stock`: a game's own model, which
 * may skip the Car Maker's checks; a player's .rad must pass them. */
bool new_car_from_rad(const char *name, const char *text, ContO *model, CarDefine *cd, int32_t i, Medium *m,
                      Trackers *t, bool stock, NewCarInfo *out);

#ifdef __cplusplus
}
#endif

#endif
