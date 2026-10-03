// Minimal RIFF/WAVE PCM decoder for the 56 sound-effect assets in
// data/sounds.zip.
//
// NOT a port of any web/*.js file -- same reasoning as gif_decode.h/
// png_decode.h/jpeg_decode.h: this is new code implementing a well-known
// file format directly, scoped to exactly what the real assets need
// (verified by inspecting all 56 files): PCM (audioformat=1), mono,
// 16-bit signed samples, sample rates of 8000/11025/22050 Hz. NOT
// implemented because nothing in data/sounds.zip needs it: stereo,
// 8-bit/24-bit/float samples, compressed formats (ADPCM/MP3-in-WAV/etc),
// extensible fmt chunks (WAVE_FORMAT_EXTENSIBLE).
#ifndef NFM_WAV_DECODE_H
#define NFM_WAV_DECODE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int32_t sample_rate;  // Hz, as stored in the file's fmt chunk
  int32_t frame_count;  // number of samples (mono, so 1 int16 per frame)
  int16_t *samples;     // frame_count int16s, malloc'd
} WavClip;

/** Decodes `data` (raw bytes of a .wav file, `len` long) into `out`.
 * Returns false (leaving `out` zeroed) on any parse error or unsupported
 * feature (see this header's own scope note). */
bool wav_decode(const uint8_t *data, size_t len, WavClip *out);

void wav_free(WavClip *clip);

#ifdef __cplusplus
}
#endif

#endif
