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
#include "mod_play.h"

typedef struct {
  SDL_AudioDeviceID device;
  AudioMixer mixer;
  ModPlayState music;
  bool music_active;
  // GameSparker.java's `mutem` (toggled on the M key, :3633-3640) reaches
  // the mixer through here. Java pauses/resumes its track rather than
  // silencing it, so this SKIPS the music render entirely -- which also
  // freezes the tracker position, matching resume-from-where-you-left.
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

// Starts/restarts background music from `mod` (Java's own strack --
// xtGraphics.java:2989 `loadstrack` loads music/stage{N}.zip per stage
// and calls strack.play(), looping continuously for the whole stage --
// see mod_play.h's own doc comment on mod_play_render()'s auto-loop).
// `mod` is BORROWED -- the caller (game.c) owns the decoded ModFile and
// must keep it alive until the NEXT audio_start_music() call or
// audio_stop_music(), both of which fully stop referencing the previous
// one (under lock) before returning, so the caller can safely mod_free()
// the old ModFile right after calling either. `gain` is the real
// per-stage RadicalMod gain (see ModPlayState::gain's doc comment in
// mod_play.h) -- game.c looks it up per stage.
void audio_start_music(Audio *al, const ModFile *mod, int32_t gain);
void audio_stop_music(Audio *al);

/** Pauses/unpauses background music without discarding it (Java's own
 * mutem pause/resume, not a stop). Safe to call when nothing is
 * playing. */
void audio_set_music_muted(Audio *al, bool muted);

#endif
