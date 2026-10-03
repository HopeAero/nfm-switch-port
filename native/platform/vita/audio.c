// See audio.h for the shared contract and this platform's own caveats
// (untested against real hardware/VitaSDK).
#include "audio.h"

#include <psp2/audioout.h>
#include <psp2/kernel/processmgr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Frames per sceAudioOutOutput() call ("grain", sceAudioOut's own term).
// Must be a multiple of 64 on real hardware; 1024 matches the buffer size
// platform/linux/audio.c's SDL backend already uses (`want.samples =
// 1024`), so both platforms render/mix in the same-sized chunks.
#define AUDIO_GRAIN 1024

// sceAudioOut's documented port types historically only guarantee 48000Hz
// on real hardware regardless of what freq is requested -- unlike SDL2,
// which negotiates a device rate and lets audio_mixer.c's own per-channel
// resampler (see audio_mixer.h) handle any mismatch against the WAV/MOD
// assets' own rates. Passed straight to sceAudioOutOpenPort's `freq` arg
// as given rather than hardcoding 48000 here, since (a) audio_mixer.c's
// resampler makes the exact rate a performance/quality question rather
// than a correctness one -- every channel already resamples toward
// whatever `output_rate` the mixer was initialised with -- and (b) this
// has never run against real hardware to confirm which rates the actual
// port negotiation accepts. Flag this loudly if audio comes out pitched
// wrong or sceAudioOutOpenPort rejects the rate outright on real
// hardware; the fix is a one-line change to what game.c passes
// audio_init (currently 44100, matching the Linux backend).
static int audio_thread_entry(SceSize args, void *argp) {
  (void)args;
  Audio *al = *(Audio **)argp;
  // TWO buffers, alternating -- not one. sceAudioOutOutput hands the
  // buffer to the hardware and returns once the PREVIOUS grain has
  // drained, which means the block just passed in is still being read
  // out while this loop runs its next iteration. Rendering into that
  // same array immediately overwrites audio the hardware has not
  // finished playing, and the tear is continuous because it happens
  // every single grain: on real hardware the result was not
  // recognisable music at all, just hiss. Alternating means the buffer
  // being filled is never the buffer being played.
  static int16_t bufs[2][AUDIO_GRAIN * 2]; // stereo; static, not stack --
                                           // 8KB is a lot of a 64KB thread
                                           // stack, and these live for the
                                           // whole thread anyway
  int32_t cur = 0;

  while (al->thread_running) {
    int16_t *buf = bufs[cur];
    sceKernelLockMutex(al->lock, 1, NULL);
    audio_mixer_render(&al->mixer, buf, AUDIO_GRAIN);
    if (al->music_active && !al->music_muted) {
      mod_play_render(&al->music, buf, AUDIO_GRAIN);
    }
    sceKernelUnlockMutex(al->lock, 1);

    // Blocks until the previously queued grain has finished playing --
    // this call IS this thread's pacing, no separate sleep needed (same
    // role SDL's own callback-driven pacing plays on the Linux backend).
    sceAudioOutOutput(al->port, buf);
    cur ^= 1;
  }
  return 0;
}

bool audio_init(Audio *al, int32_t output_rate) {
  memset(al, 0, sizeof(*al));
  al->port = -1;
  al->grain = AUDIO_GRAIN;

  // SCE_AUDIO_OUT_PORT_TYPE_MAIN accepts exactly one sample rate on real
  // hardware: 48000. (Only PORT_TYPE_VOICE takes the wider 8000..48000
  // set that includes 44100.) game.c asks for 44100, matching the Linux
  // backend, so the open here failed outright, port stayed -1, and every
  // audio call became a silent no-op -- the "no sound at all on the Vita"
  // this fixes. The caveat was written down in this file's own header
  // comment from the start; it just had never been run on a device.
  //
  // Asking first and falling back second, rather than hardcoding 48000:
  // if a future firmware or port type does accept the requested rate,
  // that path costs one extra resample per channel to avoid.
  int32_t rate = output_rate;
  al->port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, AUDIO_GRAIN,
                                  rate, SCE_AUDIO_OUT_MODE_STEREO);
  if (al->port < 0 && rate != 48000) {
    rate = 48000;
    al->port = sceAudioOutOpenPort(SCE_AUDIO_OUT_PORT_TYPE_MAIN, AUDIO_GRAIN,
                                    rate, SCE_AUDIO_OUT_MODE_STEREO);
  }
  if (al->port < 0) {
    fprintf(stderr, "sceAudioOutOpenPort failed: 0x%08x -- sound effects disabled\n", al->port);
    al->port = -1;
    return false;
  }

  // Initialised with the rate the port ACTUALLY opened at, not the one
  // that was requested -- audio_mixer.c resamples every channel toward
  // this value, so a mixer told 44100 while the device runs at 48000
  // would play everything about 9% flat. This is also why the mixer is
  // set up after the port instead of before it, as it used to be.
  audio_mixer_init(&al->mixer, rate);

  SceUID lock = sceKernelCreateMutex("nfm_audio_lock", 0, 0, NULL);
  if (lock < 0) {
    fprintf(stderr, "sceKernelCreateMutex failed: 0x%08x -- sound effects disabled\n", lock);
    sceAudioOutReleasePort(al->port);
    al->port = -1;
    return false;
  }
  al->lock = lock;

  al->thread_running = true;
  // Priority slightly above SCE_KERNEL_DEFAULT_PRIORITY_USER's usual
  // 0x10000100 -- audio threads on this platform conventionally run a
  // notch higher than the main thread so a heavy render frame can't
  // starve it into an audible stutter (same reasoning SDL's own callback
  // thread gets scheduled with elevated priority internally).
  SceUID thid = sceKernelCreateThread("nfm_audio_thread", audio_thread_entry,
                                       0x10000100 - 10, 0x10000, 0, 0, NULL);
  if (thid < 0) {
    fprintf(stderr, "sceKernelCreateThread(audio) failed: 0x%08x -- sound effects disabled\n", thid);
    sceKernelDeleteMutex(al->lock);
    sceAudioOutReleasePort(al->port);
    al->port = -1;
    return false;
  }
  al->thread_id = thid;
  Audio *self = al;
  sceKernelStartThread(thid, sizeof(self), &self);
  return true;
}

void audio_shutdown(Audio *al) {
  if (al->port < 0) return;
  al->thread_running = false;
  // The thread wakes up on its own every ~AUDIO_GRAIN/output_rate seconds
  // (sceAudioOutOutput's own pacing) and exits its loop the next time it
  // checks thread_running -- no separate wake signal needed.
  sceKernelWaitThreadEnd(al->thread_id, NULL, NULL);
  sceKernelDeleteThread(al->thread_id);
  sceKernelDeleteMutex(al->lock);
  sceAudioOutReleasePort(al->port);
  al->port = -1;
}

int32_t audio_play(Audio *al, const int16_t *samples, int32_t frame_count,
                    int32_t sample_rate, float volume, bool loop) {
  if (al->port < 0) return -1; // no device -- silently no-op, see audio_init's own doc comment
  sceKernelLockMutex(al->lock, 1, NULL);
  int32_t ch = audio_mixer_play(&al->mixer, samples, frame_count, sample_rate, volume, loop);
  sceKernelUnlockMutex(al->lock, 1);
  return ch;
}

void audio_stop(Audio *al, int32_t channel) {
  if (al->port < 0) return;
  sceKernelLockMutex(al->lock, 1, NULL);
  audio_mixer_stop(&al->mixer, channel);
  sceKernelUnlockMutex(al->lock, 1);
}

void audio_start_music(Audio *al, const ModFile *mod, int32_t gain) {
  if (al->port < 0) return;
  sceKernelLockMutex(al->lock, 1, NULL);
  mod_play_init(&al->music, mod, al->mixer.output_rate, gain);
  al->music_active = true;
  sceKernelUnlockMutex(al->lock, 1);
}

void audio_stop_music(Audio *al) {
  if (al->port < 0) return;
  sceKernelLockMutex(al->lock, 1, NULL);
  al->music_active = false;
  sceKernelUnlockMutex(al->lock, 1);
}

void audio_set_music_muted(Audio *al, bool muted) {
  // The `port < 0` guard every other entry point here already had. This
  // was the one function without it, so when audio_init() bailed out
  // before creating the mutex, this still tried to lock al->lock -- which
  // memset left as 0, not a valid SceUID. Harmless in practice (the
  // kernel rejects the id and returns an error rather than blocking) but
  // it meant the one code path that ran while sound was disabled was also
  // the only one making kernel calls with an invalid handle.
  if (al->port < 0) return;
  // Guarded by the same mutex the render thread takes, since it reads
  // this flag every grain.
  sceKernelLockMutex(al->lock, 1, NULL);
  al->music_muted = muted;
  sceKernelUnlockMutex(al->lock, 1);
}
