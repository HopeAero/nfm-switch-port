// See ogg_music.h.
#include "ogg_music.h"

#include <string.h>

#include "radical_mod.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_PUSHDATA_API
#include "../third_party/stb_vorbis.c"
#pragma GCC diagnostic pop

static bool open_part(OggMusic *o, const uint8_t *data, int32_t len) {
  if (o->v) stb_vorbis_close(o->v);
  o->v = data ? stb_vorbis_open_memory(data, len, NULL, NULL) : NULL;
  if (!o->v) return false;
  const stb_vorbis_info info = stb_vorbis_get_info(o->v);
  o->channels = info.channels;
  o->rate = (int32_t)info.sample_rate;
  o->step = ((uint64_t)o->rate << 32) / (uint64_t)o->out_rate;
  o->buf_len = o->buf_idx = 0;
  return true;
}

/** The next decoded frame; past the intro's end, the loop's start, and past
 * the loop's end, the loop again. */
static void next_frame(OggMusic *o, int16_t f[2]) {
  for (int32_t tries = 0; o->buf_idx >= o->buf_len && tries < 3; tries++) {
    o->buf_idx = 0;
    o->buf_len = o->v ? stb_vorbis_get_samples_short_interleaved(o->v, 2, o->buf, 4096 * 2) : 0;
    if (o->buf_len > 0) break;
    o->in_loop = true;   // the intro ended, or the loop did: (re)start the loop
    if (!open_part(o, o->loop, o->loop_len)) break;
  }
  if (o->buf_idx >= o->buf_len) {
    f[0] = f[1] = 0;
    return;
  }
  f[0] = o->buf[o->buf_idx * 2];
  f[1] = o->buf[o->buf_idx * 2 + 1];
  o->buf_idx++;
}

bool ogg_music_start(OggMusic *o, const uint8_t *intro, int32_t intro_len, const uint8_t *loop, int32_t loop_len,
                     int32_t out_rate) {
  ogg_music_stop(o);
  o->intro = intro;
  o->intro_len = intro_len;
  o->loop = loop;
  o->loop_len = loop_len;
  o->out_rate = out_rate;
  o->in_loop = !intro || !open_part(o, intro, intro_len);
  if (o->in_loop && !open_part(o, loop, loop_len)) return false;
  o->frac = 0;
  next_frame(o, o->cur);
  next_frame(o, o->next);
  return true;
}

void ogg_music_render(OggMusic *o, int16_t *out, int32_t frames) {
  if (!o->v) return;
  for (int32_t i = 0; i < frames; i++) {
    const int32_t t = (int32_t)(o->frac >> 16);   // 0..65535
    for (int32_t c = 0; c < 2; c++) {
      const int32_t s = o->cur[c] + (((o->next[c] - o->cur[c]) * t) >> 16);
      int32_t v = out[i * 2 + c] + (int32_t)((float)s * radical_music_gain);
      out[i * 2 + c] = (int16_t)(v > 32767 ? 32767 : (v < -32768 ? -32768 : v));
    }
    o->frac += o->step;
    while (o->frac >= ((uint64_t)1 << 32)) {
      o->frac -= (uint64_t)1 << 32;
      o->cur[0] = o->next[0];
      o->cur[1] = o->next[1];
      next_frame(o, o->next);
    }
  }
}

void ogg_music_stop(OggMusic *o) {
  if (o->v) stb_vorbis_close(o->v);
  memset(o, 0, sizeof(*o));
}
