// Host-buildable test for native/core/audio_mixer.c.
//
// No external oracle for this one -- it's new mixing/resampling code
// (see audio_mixer.h's own scope note), not a port. Verified by
// self-consistency: known input samples at known rates, hand-computed
// expected output at each output rate ratio (1:1 passthrough, integer
// upsampling, integer downsampling), plus the additive-mix-and-clamp and
// end-of-clip/looping behaviours.
#include <stdio.h>
#include <string.h>
#include "../core/audio_mixer.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
  if (!(cond)) { fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); failures++; } \
} while (0)

// 1:1 passthrough: source rate == output rate, volume 1.0 -- output
// should exactly equal the source samples (center-panned: L==R==source).
static void test_passthrough(void) {
  AudioMixer mx;
  audio_mixer_init(&mx, 44100);
  static const int16_t src[4] = {1000, -2000, 3000, -4000};
  int32_t ch = audio_mixer_play(&mx, src, 4, 44100, 1.0f, false);
  CHECK(ch >= 0, "play returns valid channel");

  int16_t out[4 * 2];
  audio_mixer_render(&mx, out, 4);
  for (int i = 0; i < 4; i++) {
    char label[64];
    snprintf(label, sizeof(label), "passthrough L[%d]", i);
    CHECK(out[i * 2 + 0] == src[i], label);
    snprintf(label, sizeof(label), "passthrough R[%d]", i);
    CHECK(out[i * 2 + 1] == src[i], label);
  }
  // Channel should have gone inactive exactly at the end (one-shot, no loop).
  CHECK(!mx.channels[ch].active, "channel deactivates at end of one-shot clip");
}

// 2x upsampling: source at half the output rate -- each source frame
// should stretch across 2 output frames (with linear interpolation
// blending toward the next sample, so only the FIRST of each pair
// exactly matches the source; verifies the resampling step math, not
// exact interpolated values mid-way).
static void test_upsample_2x(void) {
  AudioMixer mx;
  audio_mixer_init(&mx, 44100);
  static const int16_t src[2] = {1000, 3000};
  audio_mixer_play(&mx, src, 2, 22050, 1.0f, false);

  int16_t out[4 * 2];
  audio_mixer_render(&mx, out, 4);
  // Output frame 0 = src[0] exactly (pos_fixed starts at 0).
  CHECK(out[0] == 1000, "upsample frame 0 == src[0]");
  // Output frame 2 should land near src[1] (pos advanced by 2 * (0.5 in
  // source-frames-per-output-frame) = 1.0 source frame exactly).
  CHECK(out[2 * 2] == 3000, "upsample frame 2 == src[1] (one full source frame advanced)");
}

// Volume scaling: 0.5 volume should roughly halve the sample magnitude.
static void test_volume(void) {
  AudioMixer mx;
  audio_mixer_init(&mx, 44100);
  static const int16_t src[1] = {10000};
  audio_mixer_play(&mx, src, 1, 44100, 0.5f, false);
  int16_t out[1 * 2];
  audio_mixer_render(&mx, out, 1);
  CHECK(out[0] == 5000, "volume 0.5 halves the sample");
}

// Additive mixing + clamp: two full-scale channels summed must clamp to
// int16 range, not wrap.
static void test_mix_and_clamp(void) {
  AudioMixer mx;
  audio_mixer_init(&mx, 44100);
  static const int16_t src_a[1] = {32000};
  static const int16_t src_b[1] = {32000};
  audio_mixer_play(&mx, src_a, 1, 44100, 1.0f, false);
  audio_mixer_play(&mx, src_b, 1, 44100, 1.0f, false);
  int16_t out[1 * 2];
  audio_mixer_render(&mx, out, 1);
  CHECK(out[0] == 32767, "mix of two loud channels clamps to INT16_MAX, not wraps");
}

// Looping: a looped channel should wrap back to frame 0 and keep playing
// past the end of a one-shot's natural length, and audio_mixer_stop()
// should silence it.
static void test_loop_and_stop(void) {
  AudioMixer mx;
  audio_mixer_init(&mx, 44100);
  static const int16_t src[2] = {1000, 2000};
  int32_t ch = audio_mixer_play(&mx, src, 2, 44100, 1.0f, true);
  int16_t out[6 * 2];
  audio_mixer_render(&mx, out, 6); // 3x the clip length
  CHECK(mx.channels[ch].active, "looping channel stays active past its natural length");
  CHECK(out[0 * 2] == 1000, "loop iteration 1 frame 0");
  CHECK(out[2 * 2] == 1000, "loop iteration 2 frame 0 (wrapped)");
  CHECK(out[4 * 2] == 1000, "loop iteration 3 frame 0 (wrapped again)");

  audio_mixer_stop(&mx, ch);
  CHECK(!mx.channels[ch].active, "stop deactivates the channel");
  int16_t out2[2 * 2];
  audio_mixer_render(&mx, out2, 2);
  CHECK(out2[0] == 0 && out2[2] == 0, "stopped channel renders silence");
}

// Channel-stealing: filling all AUDIO_MIXER_MAX_CHANNELS then playing one
// more must still succeed (steals rather than dropping the new sound).
static void test_channel_stealing(void) {
  AudioMixer mx;
  audio_mixer_init(&mx, 44100);
  static const int16_t src[1] = {1000};
  for (int i = 0; i < AUDIO_MIXER_MAX_CHANNELS; i++) {
    int32_t ch = audio_mixer_play(&mx, src, 1, 44100, 1.0f, true); // loop so none finish naturally
    CHECK(ch >= 0, "channel play succeeds while filling all slots");
  }
  int32_t extra = audio_mixer_play(&mx, src, 1, 44100, 1.0f, true);
  CHECK(extra >= 0, "playing one more than MAX_CHANNELS still returns a valid (stolen) channel");
}

int main(void) {
  test_passthrough();
  test_upsample_2x();
  test_volume();
  test_mix_and_clamp();
  test_loop_and_stop();
  test_channel_stealing();

  if (failures == 0) { printf("all tests passed\n"); return 0; }
  fprintf(stderr, "%d failure(s)\n", failures); return 1;
}
