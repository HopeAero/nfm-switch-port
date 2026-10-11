// Saved replays (gallery/replays/*.nfmr): a race's 300-tick replay ring,
// as record.c keeps it, with what is needed to set the race up again to
// watch it -- the mode, the stage, the cars -- and each car's model as the
// ring found it (dents and all). game.c saves them from the Instant Replay
// and the finish's replay; gallery.inc lists them and game.c plays them.
#ifndef NFM_REPLAY_FILE_H
#define NFM_REPLAY_FILE_H

#include <stdbool.h>
#include <stdint.h>

#include "cont_o.h"
#include "record.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int32_t gmode, ext_normal, ext_career, career_bonus, career_mode, ptmatch;
  int32_t stage_num, nplayers, car_index;
  int32_t sc[NFM_MAX_CARS];
  uint8_t beast[NFM_MAX_CARS], shadow[NFM_MAX_CARS];
  char stage_name[64];
  char car_name[48];   // the player's
  int64_t saved_at;    // time(), for the list
} ReplayMeta;

// What a stage's script did to the world beside the cars' poses (the RPG's
// moving floors, portals, glows, flames, hidden pieces): per tick, the look
// of every object (cars first, then stage pieces) that ever left the look it
// started with. An object starts being kept the tick it first changes, its
// earlier ticks filled with its starting look -- which it still had then.
enum { LOOK_MAX = 1024 };   // ponytail: objects kept; one more stays as it started
enum { LOOK_SHADOW = 1, LOOK_GLOW = 2, LOOK_FIRE = 4, LOOK_FLAME = 8 };
typedef struct {
  int32_t x, y, z;
  int16_t xz, xy, zy;
  uint8_t fade, flags, glow[3], flame[3];
} ObjLook;

typedef struct {
  int32_t nobj;
  ObjLook *start;              // [nobj] each object's look at the race start
  uint8_t *kept;               // [nobj] already in ring[]
  int32_t n, head;             // kept objects; head: the newest tick's slot
  int32_t idx[LOOK_MAX];
  ObjLook *ring[LOOK_MAX];     // [300] each, a circle ending at head
  uint8_t outline[300];        // the ground's outline pulse (stage 7), same circle
} LookRing;

typedef struct {
  LookRing live, high;         // high: the live ring as cotchinow froze it
} ReplayLooks;

/** The look `co` has, as drawn (`fade` after the script's own hiding). */
ObjLook look_of(const ContO *co, int32_t fade);
/** A new race: every object's look now is its start (frees the last race's). */
void looks_reset(LookRing *l, const ObjLook *now, int32_t nobj);
/** One tick: the looks now (all nobj of them) and the outline pulse. */
void looks_rec(LookRing *l, const ObjLook *now, uint8_t outline);
/** `to` as a copy of `from` (the highlight freeze). */
void looks_copy(LookRing *to, const LookRing *from);
void looks_free(LookRing *l);
/** Tick t (0 the oldest, 299 the newest) of kept object k. */
const ObjLook *looks_at(const LookRing *l, int32_t k, int32_t t);
uint8_t looks_outline(const LookRing *l, int32_t t);
/** The look onto `co`: pose too unless `pose` is false (a car: the ring has it). */
void look_apply(ContO *co, const ObjLook *lk, bool pose);

/**
 * Writes the ring to `path`: the live one (Instant Replay: x..., car[0])
 * or, `highlight`, the finish's frozen copy (hx..., starcar). `looks`, when
 * not NULL, is the stage script's part of the same ring.
 */
bool replay_save(const char *path, const ReplayMeta *meta, const Record *r, bool highlight, const LookRing *looks);

/** Only the header, to list the file or to set its race up. */
bool replay_read_meta(const char *path, ReplayMeta *meta);

/**
 * The ring into `r`'s live fields (record_play's), after the race is set
 * up again: `r->car[0][i]` must already be a copy of car i's model, whose
 * faces get the saved shape and colours when the model matches.
 */
bool replay_read(const char *path, ReplayMeta *meta, Record *r, LookRing *looks);

#ifdef __cplusplus
}
#endif

#endif
