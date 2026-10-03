// Protracker ".mod" module file parser for the per-stage music tracks in
// music/*.zip (33 real files, one per stage plus interface.zip/party.zip).
//
// NOT a port of any web/*.js file -- the web port has no music playback
// at all (out of that port's scope), and Java's own soundClip-based
// `strack`/`intertrack` fields just hand a raw byte buffer to
// javax.sound.sampled, which has no MOD support of its own either --
// Java's OWN jar must therefore either bundle a third-party MOD player
// applet/library or (more likely, given the file is literally named
// stageN.mod) rely on a system codec; either way there's no Java*.java
// source to port from, only the file format itself to implement. This is
// new code implementing the Protracker MOD spec directly, scoped to
// exactly what the real 33 files use (verified by parsing the header of
// every one): the "M.K." 4-channel/31-instrument variant (the
// overwhelmingly dominant MOD format from the Amiga era this game's
// soundtrack was authored in) -- standard 30-byte-per-sample header
// block, 128-entry pattern order table, 64-row/4-channel/4-byte-per-cell
// pattern data. NOT implemented because nothing in music/*.zip needs it:
// other channel-count tags (M!K!/6CHN/8CHN/FLT4/etc), extended sample
// count (>31, a later-era extension), or any non-Protracker module
// format (S3M/XM/IT).
//
// This module ONLY parses the file into a structured in-memory form --
// no playback/synthesis here, see mod_play.h for the sequencer+mixer
// that actually turns this into audio (same split as gif_decode.c
// decoding pixels vs gfx_gl.c uploading/drawing them).
#ifndef NFM_MOD_DECODE_H
#define NFM_MOD_DECODE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MOD_NUM_SAMPLES 31
#define MOD_NUM_CHANNELS 4
#define MOD_ROWS_PER_PATTERN 64
#define MOD_ORDER_LEN 128

typedef struct {
  char name[23];          // NUL-terminated (source is 22 bytes, not
                           // necessarily NUL-terminated itself)
  int32_t length;          // sample data length, in BYTES (header stores
                           // 16-bit words; this is already *2)
  int8_t finetune;         // signed -8..7 (source is a signed nibble,
                           // sign-extended here)
  int32_t volume;           // 0..64, the sample's default volume
  int32_t repeat_offset;    // loop start, in BYTES
  int32_t repeat_length;    // loop length, in BYTES. <=2 conventionally
                             // means "no loop" (see mod_play.c's own use
                             // of this field) -- matches every real
                             // Protracker-compatible player's convention,
                             // not something this parser decides itself.
  int8_t *data;              // length bytes, signed 8-bit PCM, malloc'd.
                             // NULL for an unused sample slot (length==0).
} ModSample;

typedef struct {
  uint8_t sample;   // 1..31 (0 = no sample-change this cell)
  uint16_t period;  // Amiga hardware period, 0 = no note this cell
  uint8_t effect;   // 0x0..0xF (effect command nibble)
  uint8_t param;    // 0x00..0xFF (effect parameter byte)
} ModCell;

typedef struct {
  ModCell cells[MOD_ROWS_PER_PATTERN][MOD_NUM_CHANNELS];
} ModPattern;

typedef struct {
  char title[21];    // NUL-terminated (source is 20 bytes)
  ModSample samples[MOD_NUM_SAMPLES];
  int32_t song_length;               // valid entries in `order` (1..128)
  int32_t restart_position;          // historical field, rarely meaningful
  uint8_t order[MOD_ORDER_LEN];
  int32_t num_patterns;              // distinct patterns actually stored
                                      // (== max(order[:song_length]) + 1)
  ModPattern *patterns;              // num_patterns entries, malloc'd
} ModFile;

/** Decodes `data` (raw bytes of a .mod file, `len` long) into `out`.
 * Returns false (leaving `out` zeroed) on any parse error or unsupported
 * feature (see this header's own scope note -- in practice, any tag
 * other than "M.K." at offset 1080). */
bool mod_decode(const uint8_t *data, size_t len, ModFile *out);

void mod_free(ModFile *mod);

#ifdef __cplusplus
}
#endif

#endif
