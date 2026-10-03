#include "audio_mixer.h"

#include <string.h>

void audio_mixer_init(AudioMixer *mx, int32_t output_rate) {
  memset(mx, 0, sizeof(*mx));
  mx->output_rate = output_rate;
}

static int32_t find_channel_slot(AudioMixer *mx) {
  for (int32_t i = 0; i < AUDIO_MIXER_MAX_CHANNELS; i++) {
    if (!mx->channels[i].active) return i;
  }
  // All busy -- steal round-robin via a static rotor derived from a
  // cheap hash of the mixer pointer's low bits isn't needed; a fixed
  // rotating counter stored in channel 0's otherwise-unused pos_fixed
  // high bits would be overkill. Simplest deterministic policy: steal
  // channel 0. With 16 channels and this game's short one-shot SFX,
  // stealing is a corner case (more than 16 overlapping sounds at once)
  // that's inaudible either way.
  return 0;
}

int32_t audio_mixer_play(AudioMixer *mx, const int16_t *samples, int32_t frame_count,
                          int32_t sample_rate, float volume, bool loop) {
  if (frame_count <= 0 || sample_rate <= 0) return -1;
  int32_t idx = find_channel_slot(mx);
  MixerChannel *ch = &mx->channels[idx];
  ch->samples = samples;
  ch->frame_count = frame_count;
  ch->sample_rate = sample_rate;
  ch->active = true;
  ch->loop = loop;
  ch->pos_fixed = 0;
  ch->step_fixed = ((uint64_t)sample_rate << 32) / (uint64_t)mx->output_rate;
  ch->volume = volume;
  return idx;
}

void audio_mixer_stop(AudioMixer *mx, int32_t channel) {
  if (channel < 0 || channel >= AUDIO_MIXER_MAX_CHANNELS) return;
  mx->channels[channel].active = false;
}

void audio_mixer_render(AudioMixer *mx, int16_t *out, int32_t out_frames) {
  memset(out, 0, (size_t)out_frames * 2 * sizeof(int16_t));

  for (int32_t c = 0; c < AUDIO_MIXER_MAX_CHANNELS; c++) {
    MixerChannel *ch = &mx->channels[c];
    if (!ch->active) continue;

    for (int32_t f = 0; f < out_frames; f++) {
      uint32_t src_i = (uint32_t)(ch->pos_fixed >> 32);
      if (src_i >= (uint32_t)ch->frame_count) {
        if (ch->loop) {
          ch->pos_fixed -= (uint64_t)ch->frame_count << 32;
          src_i = (uint32_t)(ch->pos_fixed >> 32);
        } else {
          ch->active = false;
          break;
        }
      }
      // Linear interpolation between src_i and src_i+1 (clamped to the
      // last sample at the tail, or the loop-wrapped first sample).
      int32_t s0 = ch->samples[src_i];
      uint32_t next_i = src_i + 1;
      if (next_i >= (uint32_t)ch->frame_count) {
        next_i = ch->loop ? 0 : src_i;
      }
      int32_t s1 = ch->samples[next_i];
      uint32_t frac = (uint32_t)(ch->pos_fixed & 0xffffffffu); // Q0.32
      int32_t sample = s0 + (int32_t)(((int64_t)(s1 - s0) * frac) >> 32);
      int32_t scaled = (int32_t)((float)sample * ch->volume);

      int32_t mixed_l = out[f * 2 + 0] + scaled;
      int32_t mixed_r = out[f * 2 + 1] + scaled;
      if (mixed_l > 32767) mixed_l = 32767; else if (mixed_l < -32768) mixed_l = -32768;
      if (mixed_r > 32767) mixed_r = 32767; else if (mixed_r < -32768) mixed_r = -32768;
      out[f * 2 + 0] = (int16_t)mixed_l;
      out[f * 2 + 1] = (int16_t)mixed_r;

      ch->pos_fixed += ch->step_fixed;
      // Post-advance check so a one-shot channel deactivates the SAME
      // call it plays its last frame, not one call later -- frees the
      // slot immediately rather than leaving it occupied-but-silent
      // until the next audio_mixer_render() happens to probe it.
      if (!ch->loop && (uint32_t)(ch->pos_fixed >> 32) >= (uint32_t)ch->frame_count) {
        ch->active = false;
        break;
      }
    }
  }
}
