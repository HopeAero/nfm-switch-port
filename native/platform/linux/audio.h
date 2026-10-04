// SDL2 audio-device backend for native/core/audio_mixer.c -- the gfx_gl.c
// role for audio (see audio_mixer.h's own top comment): the mixer itself
// is pure buffer math, this is the platform glue that actually opens a
// device and pumps audio_mixer_render() from SDL's callback thread.
//
// Struct fields are genuinely platform-specific (SDL_AudioDeviceID has no
// Vita equivalent), so -- like gl_include.h -- this header is duplicated
// per platform/<name>/ directory rather than living in common/; only the
// six function NAMES below are the shared contract every platform's
// audio.h must expose (game.c calls them without knowing which backend
// it's linked against).
#ifndef NFM_AUDIO_H
#define NFM_AUDIO_H

#include <SDL.h>
#include <stdbool.h>
#include "audio_mixer.h"
#include "radical_mod.h"

typedef struct {
  SDL_AudioDeviceID device;
  AudioMixer mixer;
  RadicalPlayer music;
  bool music_active;
  // GameSparker.java's `mutem` (toggled on the M key, :3633-3640) reaches
  // the mixer through here. Java pauses/resumes its track rather than
  // silencing it, so this SKIPS the music render entirely -- which also
  // freezes the stream position, matching resume-from-where-you-left.
  bool music_muted;
} Audio;

// Opens a stereo S16 SDL2 audio device at `output_rate` Hz and starts it
// unpaused. Returns false (device left unopened) on failure -- callers
// should treat that as "no audio this session" rather than a fatal error,
// matching how a missing data/images.zip degrades to vfont-only menus
// elsewhere in this file.
bool audio_init(Audio *al, int32_t output_rate);

void audio_shutdown(Audio *al);

// Thread-safe wrappers around audio_mixer_play/stop -- the SDL callback
// runs on its own thread and reads mixer.channels[] concurrently with
// whatever the main thread does here, so every mutation from the main
// thread must be bracketed by SDL_Lock/UnlockAudioDevice (matches SDL2's
// own documented pattern for touching callback-owned state).
int32_t audio_play(Audio *al, const int16_t *samples, int32_t frame_count,
                    int32_t sample_rate, float volume, bool loop);
void audio_stop(Audio *al, int32_t channel);

// Starts background music from the top of `track` (SuperClip.play(): the
// stream resets to its start and loops by its own rollBack fields for as
// long as it plays -- see radical_mod.h). `track` is BORROWED -- the caller
// (game.c) owns the rendered RadicalTrack and must keep it alive until the
// NEXT audio_start_music() call or audio_stop_music(), both of which stop
// referencing the previous one (under lock) before returning, so the caller
// can radical_track_free() the old track right after calling either.
void audio_start_music(Audio *al, const RadicalTrack *track);
void audio_stop_music(Audio *al);

/** Pauses/unpauses background music without discarding it (Java's own
 * mutem pause/resume, not a stop). Safe to call when nothing is
 * playing. */
void audio_set_music_muted(Audio *al, bool muted);

#endif
