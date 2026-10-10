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

/**
 * Writes the ring to `path`: the live one (Instant Replay: x..., car[0])
 * or, `highlight`, the finish's frozen copy (hx..., starcar).
 */
bool replay_save(const char *path, const ReplayMeta *meta, const Record *r, bool highlight);

/** Only the header, to list the file or to set its race up. */
bool replay_read_meta(const char *path, ReplayMeta *meta);

/**
 * The ring into `r`'s live fields (record_play's), after the race is set
 * up again: `r->car[0][i]` must already be a copy of car i's model, whose
 * faces get the saved shape and colours when the model matches.
 */
bool replay_read(const char *path, ReplayMeta *meta, Record *r);

#ifdef __cplusplus
}
#endif

#endif
