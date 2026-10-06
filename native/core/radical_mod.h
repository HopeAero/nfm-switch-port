// The original's own music pipeline, ported line for line:
//
//   RadicalMod.java          -- the per-track parameters and the wrapper
//   ds/nfm/ModuleLoader.java -- first entry of the zip, parsed as a MOD
//   ds/nfm/mod/Mod.java      -- the MOD parser
//   ds/nfm/mod/ModSlayer.java-- the renderer: the WHOLE song is rendered
//                               up front into one byte stream
//   SuperClip.java           -- plays that stream, 16-bit mono LE at 22000Hz,
//                               in 22000-byte chunks, looping
//
// Replaces mod_play.c, a generic Protracker player that was not a port of
// anything and sounded different in four ways: it ignored the BPM argument
// (stage 1 is 135, not 125), ignored the rate argument (a speed/pitch
// factor of 8000/rate), took the BPM for the gain (the real gain is
// vol*0.8, 96..320), and did not reproduce the renderer's sound.
//
// That sound comes from a bug the port keeps on purpose. ModSlayer's
// intToBytes16() advances its output by ONE byte per sample, so each sample
// leaves only its high byte behind, and SuperClip then reads the bytes in
// pairs as little-endian 16-bit frames: the music the original actually
// plays is the rendered signal at half its sample count, with the next
// sample's high byte as the MSB and this one's as the LSB. With
// samplingrate = rate/8000*44000 rendered samples per second of music and
// 44000 bytes per second played, the music runs at 8000/rate speed. Every
// byte here matches the jar's own output (see tests/radical_mod_test.c).
#ifndef NFM_RADICAL_MOD_H
#define NFM_RADICAL_MOD_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// The rendered stream plus SuperClip's two loop fields, already in the form
// RadicalMod hands them over (roll_back_trig = oln - slayer.rollBackTrig).
typedef struct {
  uint8_t *bytes;
  int32_t len;            // SuperClip's stream length (oln, one past the last full sample)
  int32_t roll_back_pos;  // 0 = no loop point: the whole stream repeats
  int32_t roll_back_trig;
} RadicalTrack;

// ModuleLoader.prepareSlayer(mod, samplingrate, gain, bpm) + turnbytesNorm(false).
// `mod` is the MOD file itself. Returns false (track zeroed) where the Java
// would have ended up with an unloaded RadicalMod: a parse failure, or an
// exception while rendering.
bool radical_render(const uint8_t *mod, size_t mod_len, int32_t samplingrate, int32_t gain,
                    int32_t bpm, RadicalTrack *out);

// new RadicalMod(file, vol, rate, bpm, false, false) -- the stage tracks.
bool radical_render_stage(const uint8_t *mod, size_t mod_len, int32_t vol, int32_t rate,
                          int32_t bpm, RadicalTrack *out);

// new RadicalMod(file) + loadimod(false) -- the menu track, interface.zip.
bool radical_render_interface(const uint8_t *mod, size_t mod_len, RadicalTrack *out);

void radical_track_free(RadicalTrack *t);

// SuperClip's playback loop, as a pull source. 22000 frames per second.
#define RADICAL_PLAY_RATE 22000

typedef struct {
  const RadicalTrack *track; // borrowed
  int32_t pos;               // ByteArrayInputStream position
  uint8_t pending[44000];    // up to two chunks queued by one loop iteration
  int32_t pending_len, pending_idx;
  // Resampling to the device rate (linear, like audio_mixer.c's clips).
  uint64_t frac;             // Q32 position between cur and next
  uint64_t step;
  int16_t cur, next;
} RadicalPlayer;

// SuperClip.play(): from the start of the stream.
void radical_player_start(RadicalPlayer *p, const RadicalTrack *t, int32_t output_rate);

// Next 22000Hz frame of the stream, exactly as SuperClip writes it.
int16_t radical_player_next_frame(RadicalPlayer *p);

// ADDS `frames` stereo frames at the output rate into `out` (clamped) --
// the platform callback renders the effects first, then this on top.
void radical_player_render(RadicalPlayer *p, int16_t *out, int32_t frames);

/** Settings > Audio > Music: the track's samples times this (0..1, default 1;
 * at 1 the output is the original's, bit for bit). */
extern float radical_music_gain;

#ifdef __cplusplus
}
#endif

#endif
