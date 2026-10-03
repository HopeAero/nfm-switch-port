#include "audio.h"

#include <stdio.h>
#include <string.h>

static void audio_callback(void *userdata, Uint8 *stream, int len) {
  Audio *al = (Audio *)userdata;
  // len is bytes; output is stereo S16 (4 bytes/frame).
  int32_t frames = len / 4;
  // SFX first (clears + fills `stream`), then background music ADDS on
  // top -- mod_play_render() never clears its output, see its own doc
  // comment, so call order here matters.
  audio_mixer_render(&al->mixer, (int16_t *)stream, frames);
  if (al->music_active && !al->music_muted) {
    mod_play_render(&al->music, (int16_t *)stream, frames);
  }
}

bool audio_init(Audio *al, int32_t output_rate) {
  memset(al, 0, sizeof(*al));
  if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
      fprintf(stderr, "SDL_InitSubSystem(AUDIO) failed: %s\n", SDL_GetError());
      return false;
    }
  }

  audio_mixer_init(&al->mixer, output_rate);

  SDL_AudioSpec want, have;
  memset(&want, 0, sizeof(want));
  want.freq = output_rate;
  want.format = AUDIO_S16SYS;
  want.channels = 2;
  want.samples = 1024;
  want.callback = audio_callback;
  want.userdata = al;

  al->device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0 /* no changes allowed -- mixer assumes exactly `want` */);
  if (al->device == 0) {
    fprintf(stderr, "SDL_OpenAudioDevice failed: %s -- sound effects disabled\n", SDL_GetError());
    return false;
  }
  SDL_PauseAudioDevice(al->device, 0);
  return true;
}

void audio_shutdown(Audio *al) {
  if (al->device != 0) {
    SDL_CloseAudioDevice(al->device);
    al->device = 0;
  }
}

int32_t audio_play(Audio *al, const int16_t *samples, int32_t frame_count,
                    int32_t sample_rate, float volume, bool loop) {
  if (al->device == 0) return -1; // no device -- silently no-op, see this file's own init doc comment
  SDL_LockAudioDevice(al->device);
  int32_t ch = audio_mixer_play(&al->mixer, samples, frame_count, sample_rate, volume, loop);
  SDL_UnlockAudioDevice(al->device);
  return ch;
}

void audio_stop(Audio *al, int32_t channel) {
  if (al->device == 0) return;
  SDL_LockAudioDevice(al->device);
  audio_mixer_stop(&al->mixer, channel);
  SDL_UnlockAudioDevice(al->device);
}

void audio_start_music(Audio *al, const ModFile *mod, int32_t gain) {
  if (al->device == 0) return;
  SDL_LockAudioDevice(al->device);
  mod_play_init(&al->music, mod, al->mixer.output_rate, gain);
  al->music_active = true;
  SDL_UnlockAudioDevice(al->device);
}

void audio_stop_music(Audio *al) {
  if (al->device == 0) return;
  SDL_LockAudioDevice(al->device);
  al->music_active = false;
  SDL_UnlockAudioDevice(al->device);
}

void audio_set_music_muted(Audio *al, bool muted) {
  al->music_muted = muted;
}
