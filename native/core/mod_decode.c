#include "mod_decode.h"

#include <stdlib.h>
#include <string.h>

static uint16_t read_u16be(const uint8_t *p) {
  return (uint16_t)((p[0] << 8) | p[1]);
}

bool mod_decode(const uint8_t *data, size_t len, ModFile *out) {
  memset(out, 0, sizeof(*out));
  // 20 (title) + 31*30 (sample headers) + 1 (song_length) + 1 (restart) +
  // 128 (order) + 4 (tag) = 1084 bytes minimum before any pattern data.
  if (len < 1084) return false;

  memcpy(out->title, data, 20);
  out->title[20] = '\0';

  size_t off = 20;
  for (int32_t i = 0; i < MOD_NUM_SAMPLES; i++) {
    ModSample *s = &out->samples[i];
    memcpy(s->name, data + off, 22);
    s->name[22] = '\0';
    s->length = (int32_t)read_u16be(data + off + 22) * 2;
    // Finetune is a signed nibble (source byte's low 4 bits): values
    // 0..7 mean 0..+7, values 8..15 mean -8..-1 (two's-complement in 4
    // bits) -- sign-extend by shifting into the top of an int8_t then
    // arithmetic-shifting back.
    int8_t raw_finetune = (int8_t)(data[off + 24] & 0x0f);
    s->finetune = (int8_t)((int8_t)(raw_finetune << 4) >> 4);
    s->volume = data[off + 25];
    if (s->volume > 64) s->volume = 64; // clamp -- malformed files exist in the wild
    s->repeat_offset = (int32_t)read_u16be(data + off + 26) * 2;
    s->repeat_length = (int32_t)read_u16be(data + off + 28) * 2;
    s->data = NULL;
    off += 30;
  }

  out->song_length = data[off]; off += 1;
  if (out->song_length < 1) out->song_length = 1;
  if (out->song_length > MOD_ORDER_LEN) out->song_length = MOD_ORDER_LEN;
  out->restart_position = data[off]; off += 1;

  memcpy(out->order, data + off, MOD_ORDER_LEN); off += MOD_ORDER_LEN;

  // Scope note in the header: only the "M.K." 4-channel/31-instrument
  // tag is present in the real asset set.
  if (memcmp(data + off, "M.K.", 4) != 0) return false;
  off += 4;

  int32_t max_pattern = 0;
  for (int32_t i = 0; i < out->song_length; i++) {
    if (out->order[i] > max_pattern) max_pattern = out->order[i];
  }
  out->num_patterns = max_pattern + 1;

  size_t pattern_bytes = (size_t)out->num_patterns * MOD_ROWS_PER_PATTERN * MOD_NUM_CHANNELS * 4;
  if (off + pattern_bytes > len) return false;

  out->patterns = malloc((size_t)out->num_patterns * sizeof(ModPattern));
  if (!out->patterns) return false;

  for (int32_t p = 0; p < out->num_patterns; p++) {
    for (int32_t row = 0; row < MOD_ROWS_PER_PATTERN; row++) {
      for (int32_t ch = 0; ch < MOD_NUM_CHANNELS; ch++) {
        const uint8_t *cell = data + off;
        off += 4;
        // Protracker cell layout (4 bytes):
        //   byte0: [sampleHi:4][periodHi:4]
        //   byte1: [periodLo:8]
        //   byte2: [sampleLo:4][effectCmd:4]
        //   byte3: [effectParam:8]
        uint8_t sample = (uint8_t)((cell[0] & 0xf0) | ((cell[2] & 0xf0) >> 4));
        uint16_t period = (uint16_t)(((cell[0] & 0x0f) << 8) | cell[1]);
        uint8_t effect = (uint8_t)(cell[2] & 0x0f);
        uint8_t param = cell[3];
        out->patterns[p].cells[row][ch] = (ModCell){sample, period, effect, param};
      }
    }
  }

  // Sample PCM data follows all pattern data, concatenated in table
  // order. Each sample's own `length` (already *2, i.e. in bytes) says
  // how many bytes to take; length 0 means an unused slot (data stays
  // NULL, matching an empty ModSample the player skips).
  for (int32_t i = 0; i < MOD_NUM_SAMPLES; i++) {
    ModSample *s = &out->samples[i];
    if (s->length <= 0) continue;
    if (off + (size_t)s->length > len) {
      mod_free(out);
      return false;
    }
    s->data = malloc((size_t)s->length);
    if (!s->data) { mod_free(out); return false; }
    memcpy(s->data, data + off, (size_t)s->length);
    off += (size_t)s->length;
  }

  return true;
}

void mod_free(ModFile *mod) {
  for (int32_t i = 0; i < MOD_NUM_SAMPLES; i++) {
    free(mod->samples[i].data);
    mod->samples[i].data = NULL;
  }
  free(mod->patterns);
  mod->patterns = NULL;
  mod->num_patterns = 0;
}
