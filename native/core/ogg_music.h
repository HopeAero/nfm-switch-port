// Extended's career music: Ogg Vorbis, an intro (stageNa.ogg) played once
// and then a loop (stageNb.ogg) for as long as the race lasts -- the jar's
// OggClip pair. Decoded as it plays, from the audio callback (stb_vorbis,
// third_party/), resampled linearly to the device rate.
#ifndef NFM_OGG_MUSIC_H
#define NFM_OGG_MUSIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  struct stb_vorbis *v;
  const uint8_t *intro, *loop;   // borrowed: the caller keeps both alive while it plays
  int32_t intro_len, loop_len;
  bool in_loop;
  int32_t channels, rate, out_rate;
  int16_t buf[4096 * 2];   // decoded frames, interleaved stereo
  int32_t buf_len, buf_idx;
  uint64_t frac, step;     // Q32 position between cur and next
  int16_t cur[2], next[2];
} OggMusic;

/** Starts the intro (may be NULL: the loop from the start); false when
 * neither decodes. */
bool ogg_music_start(OggMusic *o, const uint8_t *intro, int32_t intro_len, const uint8_t *loop, int32_t loop_len,
                     int32_t out_rate);

/** ADDS `frames` stereo frames into `out` (clamped), scaled by
 * radical_music_gain like the modules. */
void ogg_music_render(OggMusic *o, int16_t *out, int32_t frames);

void ogg_music_stop(OggMusic *o);

#ifdef __cplusplus
}
#endif

#endif
