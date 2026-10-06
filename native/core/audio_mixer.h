// Fixed-channel-count PCM mixer: takes any number of already-decoded
// mono int16 clips (wav_decode.c's own output, or a future MOD engine's
// per-instrument samples) and renders them, resampled + volume-scaled +
// summed + clamped, into a single interleaved stereo int16 output buffer
// at one fixed output sample rate.
//
// NOT a port of anything in web/ or java-src/ -- there is no JS audio
// port to translate (see PORT_SPEC.md's own scope notes on audio being
// out of the web port entirely) and Java's own soundClip/Clip machinery
// is a thin wrapper over javax.sound.sampled with no mixing logic of its
// own to port (the JRE's mixer does that invisibly). This is new code,
// scoped to what a small fixed number of simultaneous one-shot SFX (plus,
// later, a 4-channel MOD engine) actually need: per-channel linear-
// interpolated resampling (so an 8kHz/11kHz/22kHz clip plays at correct
// pitch against a 44100Hz output device), mono source anywhere in the
// stereo field (center-panned -- none of the current SFX need stereo
// positioning), and simple additive mixing with int32 headroom before
// the final int16 clamp.
//
// Pure CPU-side buffer math, no device/platform code -- fully host-
// testable, same split as gfx.c/gfx_gl.c (this file is the gfx.c side;
// a platform audio backend, e.g. native/platform/linux/audio.c,
// is the gfx_gl.c side that actually opens a device and calls
// audio_mixer_render() from its callback).
#ifndef NFM_AUDIO_MIXER_H
#define NFM_AUDIO_MIXER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 16 concurrent one-shot voices is generous headroom for this game's own
// SFX set (at most a handful of overlapping crash/skid/scrape/checkpoint
// sounds at once) plus 4 reserved for a future MOD engine's channels.
#define AUDIO_MIXER_MAX_CHANNELS 16

typedef struct {
  const int16_t *samples; // borrowed, NOT owned/copied -- caller (main.c's
                           // asset table) must outlive every mixer call
                           // that might still reference this channel.
  int32_t frame_count;
  int32_t sample_rate;
  bool active;
  bool loop; // true = wrap to frame 0 at end instead of deactivating (for
             // future MOD/engine-loop use; every current SFX trigger uses
             // one-shot playback, i.e. loop=false)
  uint64_t pos_fixed;  // Q32.32 fixed-point read position, in SOURCE frames
  uint64_t step_fixed; // Q32.32 advance per OUTPUT frame = sample_rate/output_rate
  float volume;        // 0..1, linear
} MixerChannel;

typedef struct {
  int32_t output_rate;
  MixerChannel channels[AUDIO_MIXER_MAX_CHANNELS];
} AudioMixer;

void audio_mixer_init(AudioMixer *mx, int32_t output_rate);

/**
 * Starts playing `samples` (frame_count mono int16 frames at sample_rate
 * Hz) on the first free/inactive channel, or steals the channel that
 * least recently started if all are busy (matches the practical effect
 * of Java's own soundClip.play() -- see xtGraphics.java's soundClip.java
 * doc comment on `if (!clip.isOpen()) open+loop(0) else loop(1)`: a
 * retrigger while still playing just restarts, it doesn't queue).
 * `samples` is NOT copied -- must stay valid until the channel finishes
 * or is stolen. Returns the channel index used, for callers that want to
 * `audio_mixer_stop()` a specific looping voice later (e.g. an engine
 * idle loop); -1 only if frame_count/sample_rate are invalid.
 */
int32_t audio_mixer_play(AudioMixer *mx, const int16_t *samples, int32_t frame_count,
                          int32_t sample_rate, float volume, bool loop);

/** Stops a specific channel immediately (for a `loop=true` voice that
 * needs an explicit end, e.g. Java's soundClip.stop() on wastd/air). */
void audio_mixer_stop(AudioMixer *mx, int32_t channel);

/**
 * Renders `out_frames` STEREO frames (2 int16s each, L then R,
 * interleaved) into `out`, mixing every active channel and clamping to
 * the int16 range. Advances each channel's position; channels that
 * reach end-of-clip (and aren't looping) go inactive mid-call, silent
 * for the remainder of this render. Always fully overwrites `out` (silence
 * where nothing is playing), matching a real audio device callback's
 * contract of filling the whole requested buffer every time.
 */
void audio_mixer_render(AudioMixer *mx, int16_t *out, int32_t out_frames);

/** Settings > Audio > Effects: every voice's volume times this (0..1, default 1). */
extern float audio_sfx_gain;

#ifdef __cplusplus
}
#endif

#endif
