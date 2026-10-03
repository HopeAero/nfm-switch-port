#include "wav_decode.h"

#include <stdlib.h>
#include <string.h>

static uint32_t read_u32le(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static uint16_t read_u16le(const uint8_t *p) {
  return (uint16_t)(p[0] | (p[1] << 8));
}

bool wav_decode(const uint8_t *data, size_t len, WavClip *out) {
  memset(out, 0, sizeof(*out));
  if (len < 12) return false;
  if (memcmp(data, "RIFF", 4) != 0) return false;
  if (memcmp(data + 8, "WAVE", 4) != 0) return false;

  bool have_fmt = false;
  uint16_t audio_format = 0, channels = 0, bits_per_sample = 0;
  uint32_t sample_rate = 0;
  const uint8_t *data_ptr = NULL;
  uint32_t data_size = 0;

  size_t off = 12;
  while (off + 8 <= len) {
    char id[5] = {0};
    memcpy(id, data + off, 4);
    uint32_t chunk_size = read_u32le(data + off + 4);
    size_t body = off + 8;
    if (body + chunk_size > len) break; // truncated/corrupt chunk, stop here

    if (memcmp(id, "fmt ", 4) == 0) {
      if (chunk_size < 16) return false;
      audio_format = read_u16le(data + body + 0);
      channels = read_u16le(data + body + 2);
      sample_rate = read_u32le(data + body + 4);
      bits_per_sample = read_u16le(data + body + 14);
      have_fmt = true;
    } else if (memcmp(id, "data", 4) == 0) {
      data_ptr = data + body;
      data_size = chunk_size;
    }

    off = body + chunk_size + (chunk_size & 1); // chunks are word-aligned
  }

  if (!have_fmt || !data_ptr) return false;
  // Scope note in the header: only PCM mono 16-bit is present in the real
  // asset set, so that's all this decodes.
  if (audio_format != 1 || channels != 1 || bits_per_sample != 16) return false;

  int32_t frame_count = (int32_t)(data_size / 2);
  int16_t *samples = malloc((size_t)frame_count * sizeof(int16_t));
  if (!samples) return false;
  for (int32_t i = 0; i < frame_count; i++) {
    samples[i] = (int16_t)read_u16le(data_ptr + i * 2);
  }

  out->sample_rate = (int32_t)sample_rate;
  out->frame_count = frame_count;
  out->samples = samples;
  return true;
}

void wav_free(WavClip *clip) {
  free(clip->samples);
  clip->samples = NULL;
  clip->frame_count = 0;
}
