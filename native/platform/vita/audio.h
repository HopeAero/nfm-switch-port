// sceAudioOut backend for native/core/audio_mixer.c -- the gfx_gl.c role
// for audio on this platform (see audio_mixer.h's own top comment): the
// mixer itself is pure buffer math, this is the platform glue that
// actually opens an output port and pumps audio_mixer_render() into it.
//
// Struct fields are genuinely platform-specific (no SDL_AudioDeviceID
// equivalent here -- a raw sceAudioOut port int plus the dedicated
// output thread/lock this platform's blocking-output model needs
// instead of SDL's callback), so -- like gl_include.h -- this header is
// duplicated per platform/<name>/ directory rather than living in
// common/; only the six function NAMES below (matching
// platform/linux/audio.h exactly) are the shared contract game.c calls
// without knowing which backend it's linked against.
//
// Written against the documented sceAudioOut/sceKernel APIs but not
// compiled anywhere in this repo -- no VitaSDK in this environment, see
// ../../TASKS_NATIVE.md's Part 18 entry; verify the exact call shapes
// (particularly the port's sample rate, see audio.c's own comment) on a
// real toolchain before trusting them.
#ifndef NFM_AUDIO_H
#define NFM_AUDIO_H

#include <psp2/kernel/threadmgr.h>
#include <psp2/types.h>
#include <stdbool.h>
#include "audio_mixer.h"
#include "radical_mod.h"

typedef struct {
  int port;              // sceAudioOut port handle, -1 if not opened
  int32_t grain;          // frames per sceAudioOutOutput() call (the port's fixed "len")
  AudioMixer mixer;
  RadicalPlayer music;
  bool music_active;
  // GameSparker.java's `mutem` (toggled on the M key, :3633-3640) reaches
  // the mixer through here. Java pauses/resumes its track rather than
  // silencing it, so this SKIPS the music render entirely -- which also
  // freezes the stream position, matching resume-from-where-you-left.
  bool music_muted;
  SceUID thread_id;
  SceUID lock;            // lightweight mutex guarding mixer/music/music_active below
  volatile bool thread_running; // cleared by audio_shutdown() to stop the output thread
} Audio;

// Opens a stereo S16 sceAudioOut port at (as close as the hardware
// allows to) `output_rate` Hz and starts a dedicated output thread
// pumping audio_mixer_render()/radical_player_render() into it. Returns false
// (port left unopened) on failure -- callers should treat that as "no
// audio this session" rather than a fatal error, matching how a missing
// data/images.zip degrades to vfont-only menus elsewhere in game.c.
bool audio_init(Audio *al, int32_t output_rate);

void audio_shutdown(Audio *al);

// Thread-safe wrappers around audio_mixer_play/stop -- the output thread
// reads mixer.channels[] concurrently with whatever the main thread does
// here, so every mutation from the main thread must be bracketed by the
// same lock the output thread holds while rendering (matches
// platform/linux/audio.c's identical SDL_Lock/UnlockAudioDevice
// discipline, just against this platform's own lightweight mutex instead
// of SDL's).
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
