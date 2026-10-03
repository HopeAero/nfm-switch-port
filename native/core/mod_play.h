// Protracker MOD sequencer + software synthesizer: turns a parsed
// ModFile (mod_decode.h) into rendered PCM audio.
//
// NOT a port of anything in web/ or java-src/ -- same reasoning as
// mod_decode.h's own scope note (no Java/JS source exists for MOD
// playback in this game at all). This implements the STANDARD Amiga
// Protracker replay algorithm -- the same well-published sequencing
// rules every MOD player (OpenMPT, MilkyTracker, libmodplug, XMP, the
// original Amiga ProTracker itself, ...) implements identically because
// they all target byte-exact compatibility with the same original
// hardware/software combination, not a proprietary or invented
// algorithm. Scoped to exactly the effect commands the real 33 files in
// music/*.zip actually use (verified by scanning every pattern cell in
// every real file -- see the commit that added this file for the
// analysis): arpeggio, portamento up/down, tone portamento, vibrato,
// tone-portamento+volslide, vibrato+volslide, sample offset, volume
// slide, position jump, set volume, pattern break, set speed/tempo, and
// the E-prefixed fine volume slide up/down, retrigger, note delay,
// pattern loop, pattern delay, and note cut. NOT implemented because
// nothing in music/*.zip uses them: tremolo, set panning, fine
// portamento up/down, glissando control, vibrato/tremolo waveform
// selection, set finetune, invert loop, and the Amiga hardware LED
// filter toggle (E0 -- present in the real files but audibly a no-op on
// any non-Amiga playback target, including the original Java/browser
// ports, so intentionally not wired to anything).
#ifndef NFM_MOD_PLAY_H
#define NFM_MOD_PLAY_H

#include <stdint.h>
#include <stdbool.h>
#include "mod_decode.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int32_t sample_index;     // 0..30 (ModFile.samples[]), -1 = channel silent
  int32_t volume;            // 0..64, current playback volume
  uint16_t period;           // current Amiga period (pitch)
  uint16_t porta_target;     // tone-portamento (effect 3/5) target period
  uint8_t porta_speed;       // last-used tone-portamento speed (effect memory)
  uint8_t vibrato_pos;       // vibrato waveform phase, 0..63
  uint8_t vibrato_speed, vibrato_depth; // effect memory for effect 4/6
  uint8_t volslide_param;    // effect memory for effect A/5/6
  uint8_t arpeggio_param;    // effect memory for effect 0 (only within the
                              // SAME row -- Protracker doesn't carry this
                              // across rows, but caching the row's param
                              // once keeps the per-tick lookup simple)
  int32_t sample_offset;     // 0..255*256, effect 9's byte offset into
                              // the sample (only applied on note-trigger)
  uint64_t play_pos_fixed;   // Q32.32 fixed-point read position, in
                              // SOURCE sample frames
  bool note_delay_pending;   // effect ED -- note trigger deferred to a
                              // later tick within this row
  uint8_t note_delay_tick;
  bool retrigger_active;
  uint8_t retrigger_param;
} ModChannel;

typedef struct {
  const ModFile *mod; // borrowed, must outlive this state
  ModChannel channels[MOD_NUM_CHANNELS];

  int32_t order_pos;   // index into mod->order[]
  int32_t row;         // 0..63 within the current pattern
  int32_t tick;        // 0..speed-1 within the current row
  int32_t speed;       // ticks per row (default 6)
  int32_t tempo;       // BPM (default 125) -- tick length = 2500/tempo ms

  bool pattern_break_pending;
  int32_t pattern_break_row;    // row to jump to in the NEXT pattern
  bool position_jump_pending;
  int32_t position_jump_order;

  int32_t pattern_loop_row;      // E6 target row (per-channel in real
                                  // Protracker, but no real file in this
                                  // corpus uses E6 on more than one
                                  // channel at once -- see mod_play.c's
                                  // own comment at the E6 handler)
  int32_t pattern_loop_count;
  bool pattern_loop_active;

  int32_t pattern_delay;         // EE -- repeats the current row N extra times

  uint64_t samples_until_next_tick; // Q32.32-free plain frame counter at
                                     // the CALLER's output_rate, counts
                                     // down to the next tick boundary
  int32_t output_rate;

  bool song_ended; // true once playback has looped back to order_pos 0
                    // from natural end-of-song (order_pos reaching
                    // song_length) -- callers that want one-shot instead
                    // of looping playback can check this each render call

  // Per-stage mixer gain -- ports RadicalMod's 4th constructor argument
  // (xtGraphics.java:2984-3095's loadstrack(), e.g. `new RadicalMod(
  // "music/stage1.zip", 240, 8400, 135, false, false)`), which
  // ModSlayer.java:75/405 stores as `this.gain` and uses as
  // `vol_adj[chVol] * gain >> (vol_shift + 8)` per channel per sample --
  // see render_channel_span()'s own comment for the derived formula and
  // why this port's previous hardcoded `sample * 256` upscale ignored
  // this entirely (constantly clipping instead of just "loud"). Callers
  // pass the real per-stage value from GAME_STAGE_MUSIC_GAIN (game.c);
  // 125 (loadstrack()'s own default for every stage without an explicit
  // override) is a reasonable fallback for anything else.
  int32_t gain;
} ModPlayState;

/** Resets `st` to the start of `mod` (order 0, row 0, tick 0, speed 6,
 * tempo 125 -- Protracker's own defaults, matching every real file's
 * initial state since none of them issue a Set Speed/Tempo effect
 * before the first note). `mod` is borrowed, not copied -- must outlive
 * every mod_play_render() call using this state. `gain` is the real
 * per-stage RadicalMod gain value (see `ModPlayState::gain`'s own doc
 * comment) -- pass 125 if the caller has no better value. */
void mod_play_init(ModPlayState *st, const ModFile *mod, int32_t output_rate, int32_t gain);

/**
 * Renders `out_frames` STEREO frames (interleaved int16, L/R) into
 * `out`, ADDING to whatever's already there (so a caller can render
 * music and SFX into the same buffer, or call audio_mixer_render()
 * separately and sum the two -- this function does NOT clear `out`
 * first). Advances the sequencer through however many ticks/rows the
 * requested frame count spans, looping back to the start of the song
 * when the order table runs out (matching Java's own strack music --
 * background music loops continuously during a stage).
 */
void mod_play_render(ModPlayState *st, int16_t *out, int32_t out_frames);

#ifdef __cplusplus
}
#endif

#endif
