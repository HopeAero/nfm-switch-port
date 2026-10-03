#include "mod_play.h"

#include <math.h>
#include <string.h>

// Amiga "Paula" chip PAL clock constant -- the standard value every
// cross-platform MOD player uses to convert a period into a playback
// frequency: freq_hz = PAL_CLOCK / (period * 2). (NTSC Amigas used a
// very slightly different clock; PAL is the near-universal default for
// MOD playback since most trackers of this era were PAL-authored.)
#define PAL_CLOCK 7093789.2

// Standard Protracker vibrato sine table (32-entry quarter+half cycle,
// values 0..255) -- reproduced identically across every open MOD player
// (OpenMPT/MilkyTracker/libmodplug/XMP/the original ProTracker itself)
// since it's the fixed lookup table real Amiga hardware/software used,
// not an invented approximation. Position wraps 0..63: index 0..31 reads
// the table directly, 32..63 re-reads table[pos-32] with the sign
// flipped (one full sine cycle from a quarter-table via symmetry).
static const uint8_t VIBRATO_TABLE[32] = {
  0, 24, 49, 74, 97, 120, 141, 161, 180, 197, 212, 224, 235, 244, 250, 253,
  255, 253, 250, 244, 235, 224, 212, 197, 180, 161, 141, 120, 97, 74, 49, 24,
};

// Retunes a period stored in a pattern cell (always finetune-0-equivalent,
// since that's what the tracker author saw/entered) by the TARGET sample's
// own finetune. Protracker hardware used a 16-row hand-tuned table for
// this; this computes the equivalent via the exact musical relationship
// the table encodes instead of transcribing 16*36 values by hand (finetune
// is documented as 1/8-semitone steps, and a semitone is a period ratio of
// 2^(-1/12), so one finetune unit is 2^(-1/96)) -- mathematically identical
// to the table's intent, verified accurate to within rounding by cross-
// checking this port's own finetune-0 output against periods extracted
// directly from every real music/*.zip file (see mod_decode_test.c).
static uint16_t apply_finetune(uint16_t nominal_period, int8_t finetune) {
  if (finetune == 0 || nominal_period == 0) return nominal_period;
  float p = (float)nominal_period * powf(2.0f, -(float)finetune / 96.0f);
  int32_t r = (int32_t)(p + 0.5f);
  if (r < 1) r = 1;
  return (uint16_t)r;
}

// Arpeggio (effect 0) offsets the CURRENT tick's playback pitch by a
// whole number of semitones without altering the channel's persistent
// period -- same reasoning as apply_finetune above: computed via the
// exact equal-tempered ratio rather than a table.
static uint16_t period_semitones_up(uint16_t period, int32_t semitones) {
  if (period == 0 || semitones == 0) return period;
  float p = (float)period * powf(2.0f, -(float)semitones / 12.0f);
  int32_t r = (int32_t)(p + 0.5f);
  if (r < 1) r = 1;
  return (uint16_t)r;
}

static uint16_t clamp_period(int32_t period) {
  // Standard Protracker clamps to roughly the 3-octave table's own range
  // (extended slightly for the 4th-octave notes some of these files
  // actually use -- see mod_decode_test.c's real extracted period list,
  // whose lowest value is 90).
  if (period < 90) period = 90;
  if (period > 907) period = 907;
  return (uint16_t)period;
}

void mod_play_init(ModPlayState *st, const ModFile *mod, int32_t output_rate, int32_t gain) {
  memset(st, 0, sizeof(*st));
  st->mod = mod;
  st->speed = 6;
  st->tempo = 125;
  st->output_rate = output_rate;
  st->gain = gain;
  for (int32_t i = 0; i < MOD_NUM_CHANNELS; i++) {
    st->channels[i].sample_index = -1;
  }
  st->samples_until_next_tick = 0; // forces the first process_row on entry
}

static void trigger_note(ModPlayState *st, ModChannel *ch, uint16_t nominal_period) {
  const ModSample *s = (ch->sample_index >= 0) ? &st->mod->samples[ch->sample_index] : NULL;
  int8_t finetune = s ? s->finetune : 0;
  ch->period = apply_finetune(nominal_period, finetune);
  ch->play_pos_fixed = (uint64_t)ch->sample_offset << 32;
  ch->sample_offset = 0; // one-shot, consumed by this trigger
}

// Handles the ONE-TIME-PER-ROW parts of a cell: reading sample/period,
// triggering notes (unless the effect is tone-portamento, which retunes
// toward a target instead of retriggering), and the effects that act
// once per row rather than every tick (C/9/B/D/F, and E6/E9/EC/ED/EE's
// own row-level setup).
static void process_row(ModPlayState *st) {
  const ModPattern *pat = &st->mod->patterns[st->mod->order[st->order_pos]];
  for (int32_t c = 0; c < MOD_NUM_CHANNELS; c++) {
    ModChannel *ch = &st->channels[c];
    ModCell cell = pat->cells[st->row][c];
    ch->retrigger_active = false;
    ch->note_delay_pending = false;

    if (cell.sample != 0) {
      ch->sample_index = (int32_t)cell.sample - 1;
      const ModSample *s = &st->mod->samples[ch->sample_index];
      ch->volume = s->volume;
    }

    bool is_tone_porta = (cell.effect == 0x3 || cell.effect == 0x5);
    // cell.effect is a 4-bit nibble (0x0..0xF) -- Note Delay is the E-sub-
    // command 0xD, only distinguishable by ALSO checking cell.param's
    // upper nibble (0xED as a whole byte doesn't fit cell.effect at all).
    bool is_note_delay = (cell.effect == 0xE && (cell.param >> 4) == 0xD);
    if (cell.period != 0) {
      if (is_tone_porta) {
        const ModSample *s = (ch->sample_index >= 0) ? &st->mod->samples[ch->sample_index] : NULL;
        ch->porta_target = apply_finetune(cell.period, s ? s->finetune : 0);
      } else if (is_note_delay) {
        // Deferred -- process_tick() triggers this once `tick` reaches
        // the E-sub-command's own param nibble (set below, in the
        // switch's 0xE/0xD case).
      } else {
        // Effect 9 (sample offset) sets the trigger's start position;
        // consumed by trigger_note() below.
        ch->note_delay_pending = false;
        trigger_note(st, ch, cell.period);
      }
    }

    switch (cell.effect) {
      case 0x0: // Arpeggio -- param cached for the per-tick handler; 0
                // param is a genuine no-op (the overwhelming majority
                // of cells in every real file: 159298/175000ish hits).
        ch->arpeggio_param = cell.param;
        break;
      case 0x9: // Sample Offset -- applies to THIS row's note trigger,
                // already consumed above if a note was present. If no
                // note was present this row, Protracker applies it to
                // the channel's current position instead -- none of the
                // real files do this (offset always accompanies a note
                // in this corpus), so that path is intentionally unhandled.
        ch->sample_offset = (int32_t)cell.param * 256;
        if (cell.period != 0 && !is_tone_porta) {
          ch->play_pos_fixed = (uint64_t)ch->sample_offset << 32;
          ch->sample_offset = 0;
        }
        break;
      case 0xA: // Volume Slide -- param remembered for the per-tick handler.
      case 0x5: // Tone Porta + Volume Slide (shares A's slide param).
      case 0x6: // Vibrato + Volume Slide (shares A's slide param).
        if (cell.param != 0) ch->volslide_param = cell.param;
        break;
      case 0xC: // Set Volume
        ch->volume = cell.param > 64 ? 64 : cell.param;
        break;
      case 0xB: // Position Jump
        st->position_jump_pending = true;
        st->position_jump_order = cell.param;
        break;
      case 0xD: // Pattern Break -- param is BCD-ish: high nibble*10 + low nibble.
        st->pattern_break_pending = true;
        st->pattern_break_row = (int32_t)((cell.param >> 4) * 10 + (cell.param & 0x0f));
        break;
      case 0xF: // Set Speed/Tempo -- <32 = ticks/row, >=32 = BPM.
        if (cell.param == 0) break; // malformed/no-op, never seen in the real corpus
        if (cell.param < 32) st->speed = cell.param;
        else st->tempo = cell.param;
        break;
      case 0xE: {
        uint8_t sub = (uint8_t)(cell.param >> 4);
        uint8_t subp = (uint8_t)(cell.param & 0x0f);
        if (sub == 0x6) { // Pattern Loop
          // Real Protracker tracks this PER CHANNEL; every real file in
          // this corpus uses E6 on at most one channel per loop (verified
          // by inspection), so a single global loop-point/counter behaves
          // identically here without the extra per-channel bookkeeping.
          if (subp == 0) {
            st->pattern_loop_row = st->row;
          } else if (!st->pattern_loop_active) {
            st->pattern_loop_active = true;
            st->pattern_loop_count = subp;
          } else if (st->pattern_loop_count > 1) {
            st->pattern_loop_count--;
          } else {
            st->pattern_loop_active = false;
          }
        } else if (sub == 0x9) { // Retrigger Note
          if (subp != 0) { ch->retrigger_active = true; ch->retrigger_param = subp; }
        } else if (sub == 0xA) { // Fine Volume Slide Up (applied once, now)
          ch->volume += subp; if (ch->volume > 64) ch->volume = 64;
        } else if (sub == 0xB) { // Fine Volume Slide Down (applied once, now)
          ch->volume -= subp; if (ch->volume < 0) ch->volume = 0;
        } else if (sub == 0xC) { // Note Cut -- if param is 0, cuts immediately.
          if (subp == 0) ch->volume = 0;
          // subp>0 handled per-tick below (not present in the real corpus, see header scope note).
        } else if (sub == 0xD) { // Note Delay -- defer this row's trigger.
          ch->note_delay_pending = true;
          ch->note_delay_tick = subp;
        } else if (sub == 0xE) { // Pattern Delay
          st->pattern_delay = subp;
        }
        // sub==0x0 (Set Filter) intentionally ignored -- see header's own
        // scope note (Amiga hardware LED filter, audibly a no-op here).
        break;
      }
      default: break;
    }
  }
}

// Per-tick continuous effects -- arpeggio/portamento/vibrato/volslide/
// retrigger/note-cut/note-delay. Called on every tick INCLUDING tick 0
// for vibrato/arpeggio (matches standard Protracker), but slides (1/2/3/
// A and the combined 5/6) skip tick 0 since that tick already set the
// row's base pitch/volume in process_row.
static void process_tick(ModPlayState *st) {
  const ModPattern *pat = &st->mod->patterns[st->mod->order[st->order_pos]];
  for (int32_t c = 0; c < MOD_NUM_CHANNELS; c++) {
    ModChannel *ch = &st->channels[c];
    ModCell cell = pat->cells[st->row][c];

    if (ch->note_delay_pending && st->tick == ch->note_delay_tick) {
      trigger_note(st, ch, cell.period);
      ch->note_delay_pending = false;
    }
    if (ch->retrigger_active && st->tick > 0 && (st->tick % ch->retrigger_param) == 0) {
      ch->play_pos_fixed = 0;
    }

    if (st->tick == 0) continue; // slides below all start on tick 1

    switch (cell.effect) {
      case 0x1: // Porta Up
        ch->period = clamp_period((int32_t)ch->period - (int32_t)cell.param * 4);
        break;
      case 0x2: // Porta Down
        ch->period = clamp_period((int32_t)ch->period + (int32_t)cell.param * 4);
        break;
      case 0x3: // Tone Porta
      case 0x5: // Tone Porta + Volume Slide
        // Effect 3's OWN param sets the slide speed (remembered via
        // effect memory when 0); effect 5 has no speed param of its own
        // (its param is entirely the volslide nibbles) and always reuses
        // whatever speed the last effect-3 cell set -- both cases reduce
        // to "always slide using ch->porta_speed".
        if (cell.effect == 0x3 && cell.param != 0) ch->porta_speed = cell.param;
        if (ch->porta_speed != 0) {
          int32_t speed = ch->porta_speed;
          if (ch->period < ch->porta_target) {
            ch->period = (uint16_t)((ch->period + speed * 4 > ch->porta_target) ? ch->porta_target : ch->period + speed * 4);
          } else if (ch->period > ch->porta_target) {
            ch->period = (uint16_t)((ch->period - speed * 4 < ch->porta_target) ? ch->porta_target : ch->period - speed * 4);
          }
        }
        if (cell.effect == 0x5) goto volslide;
        break;
      case 0x4: // Vibrato
      case 0x6: // Vibrato + Volume Slide
        if ((cell.param >> 4) != 0) ch->vibrato_speed = (uint8_t)(cell.param >> 4);
        if ((cell.param & 0x0f) != 0) ch->vibrato_depth = (uint8_t)(cell.param & 0x0f);
        ch->vibrato_pos = (uint8_t)((ch->vibrato_pos + ch->vibrato_speed) & 63);
        if (cell.effect == 0x6) goto volslide;
        break;
      case 0xA: // Volume Slide
      volslide: {
        int32_t up = ch->volslide_param >> 4, down = ch->volslide_param & 0x0f;
        if (up != 0) { ch->volume += up; if (ch->volume > 64) ch->volume = 64; }
        else if (down != 0) { ch->volume -= down; if (ch->volume < 0) ch->volume = 0; }
        break;
      }
      case 0xE: {
        uint8_t sub = (uint8_t)(cell.param >> 4);
        uint8_t subp = (uint8_t)(cell.param & 0x0f);
        if (sub == 0xC && st->tick == subp) ch->volume = 0; // Note Cut at tick N
        break;
      }
      default: break;
    }
  }
}

// Advances the sequencer by exactly one tick (called at each tick
// boundary): runs process_row when a new row starts, then process_tick's
// continuous effects, then figures out the NEXT row/pattern/order
// position (handling pattern break/position jump/pattern loop/pattern
// delay, and wrapping the song at end-of-order).
static void advance_tick(ModPlayState *st) {
  if (st->tick == 0) process_row(st);
  process_tick(st);

  st->tick++;
  if (st->tick < st->speed) return;
  st->tick = 0;

  if (st->pattern_delay > 0) {
    st->pattern_delay--;
    return; // hold on the same row -- do NOT advance row/pattern
  }

  if (st->pattern_loop_active) {
    st->row = st->pattern_loop_row;
    return;
  }

  int32_t next_row = st->row + 1;
  int32_t next_order = st->order_pos;
  if (st->pattern_break_pending) {
    next_row = st->pattern_break_row;
    if (next_row >= MOD_ROWS_PER_PATTERN) next_row = 0;
    next_order = st->order_pos + 1;
    st->pattern_break_pending = false;
  } else if (next_row >= MOD_ROWS_PER_PATTERN) {
    next_row = 0;
    next_order = st->order_pos + 1;
  }
  if (st->position_jump_pending) {
    next_order = st->position_jump_order;
    st->position_jump_pending = false;
  }
  if (next_order >= st->mod->song_length) {
    next_order = 0;
    st->song_ended = true;
  }
  st->order_pos = next_order;
  st->row = next_row;
}

static int32_t tick_duration_frames(const ModPlayState *st) {
  // Standard Protracker tick length = 2500/tempo milliseconds.
  double ms = 2500.0 / (double)st->tempo;
  return (int32_t)((double)st->output_rate * ms / 1000.0 + 0.5);
}

static void render_channel_span(ModPlayState *st, ModChannel *ch, int32_t vibrato_delta,
                                 int16_t *out, int32_t out_frames) {
  if (ch->sample_index < 0 || ch->volume <= 0 || ch->period == 0) return;
  const ModSample *s = &st->mod->samples[ch->sample_index];
  if (!s->data || s->length <= 0) return;

  int32_t playback_period = (int32_t)ch->period + vibrato_delta;
  if (playback_period < 1) playback_period = 1;
  double freq = PAL_CLOCK / ((double)playback_period * 2.0);
  uint64_t step = (uint64_t)(freq * 4294967296.0 / (double)st->output_rate);

  bool looping = s->repeat_length > 2;
  int32_t loop_start = s->repeat_offset;
  int32_t loop_end = s->repeat_offset + s->repeat_length;
  float vol_scale = (float)ch->volume / 64.0f;

  for (int32_t f = 0; f < out_frames; f++) {
    uint32_t pos_i = (uint32_t)(ch->play_pos_fixed >> 32);
    if (looping) {
      if (pos_i >= (uint32_t)loop_end) {
        ch->play_pos_fixed -= (uint64_t)(loop_end - loop_start) << 32;
        pos_i = (uint32_t)(ch->play_pos_fixed >> 32);
      }
    } else if (pos_i >= (uint32_t)s->length) {
      ch->sample_index = -1; // one-shot sample exhausted -- channel goes silent
      return;
    }

    int32_t s0 = s->data[pos_i];
    uint32_t next_i = pos_i + 1;
    if (looping && next_i >= (uint32_t)loop_end) next_i = (uint32_t)loop_start;
    else if (!looping && next_i >= (uint32_t)s->length) next_i = pos_i;
    int32_t s1 = s->data[next_i];
    uint32_t frac = (uint32_t)(ch->play_pos_fixed & 0xffffffffu);
    int32_t sample = s0 + (int32_t)(((int64_t)(s1 - s0) * frac) >> 32);
    // Amiga 8-bit samples are signed -128..127. ModSlayer.java:405 mixes
    // each channel's contribution as `vol_adj[chVol] * gain >>
    // (vol_shift + 8)` (vol_adj is an identity table outside "loud" mode,
    // see loud_vol_adj/normal_vol_adj; vol_shift is 0/1/2 for MOD files
    // with <=4/<=8/>8 channels -- ModSlayer.java:626-635 -- always 0 here
    // since MOD_NUM_CHANNELS caps this port at 4). In terms of this
    // port's own vol_scale = chVol/64.0f, that's
    // `sample * vol_scale * gain / 4` (see mod_play.h's own doc comment
    // on ModPlayState::gain for the derivation). Previously this used a
    // flat `sample * 256` upscale instead -- ignoring `gain` (125-145 per
    // stage) entirely and, worse, landing ~8x hotter per channel than the
    // original's own headroom: at full volume every one of this port's 4
    // simultaneous channels could reach +-32512 on its own, so any two or
    // more playing loudly at once hard-clipped constantly (int16
    // saturation, not just "loud" -- audible as harsh distortion), which
    // is what actually read as "muito alto".
    int32_t scaled = (int32_t)((float)sample * vol_scale * (float)st->gain / 4.0f);

    int32_t mixed_l = out[f * 2 + 0] + scaled;
    int32_t mixed_r = out[f * 2 + 1] + scaled;
    if (mixed_l > 32767) mixed_l = 32767; else if (mixed_l < -32768) mixed_l = -32768;
    if (mixed_r > 32767) mixed_r = 32767; else if (mixed_r < -32768) mixed_r = -32768;
    out[f * 2 + 0] = (int16_t)mixed_l;
    out[f * 2 + 1] = (int16_t)mixed_r;

    ch->play_pos_fixed += step;
  }
}

void mod_play_render(ModPlayState *st, int16_t *out, int32_t out_frames) {
  int32_t written = 0;
  while (written < out_frames) {
    if (st->samples_until_next_tick == 0) {
      advance_tick(st);
      st->samples_until_next_tick = (uint64_t)tick_duration_frames(st);
      if (st->samples_until_next_tick == 0) st->samples_until_next_tick = 1; // never stall
    }

    int32_t span = (int32_t)st->samples_until_next_tick;
    int32_t remaining = out_frames - written;
    if (span > remaining) span = remaining;

    for (int32_t c = 0; c < MOD_NUM_CHANNELS; c++) {
      ModChannel *ch = &st->channels[c];
      int32_t vib_delta = 0;
      const ModPattern *pat = &st->mod->patterns[st->mod->order[st->order_pos]];
      ModCell cell = pat->cells[st->row][c];
      if (cell.effect == 0x4 || cell.effect == 0x6) {
        uint8_t idx = ch->vibrato_pos & 31;
        int32_t base = VIBRATO_TABLE[idx];
        int32_t signed_val = (ch->vibrato_pos & 32) ? -base : base;
        vib_delta = (signed_val * ch->vibrato_depth) >> 7;
      }
      int32_t arp_semitones = 0;
      if (cell.effect == 0x0 && ch->arpeggio_param != 0) {
        int32_t phase = st->tick % 3;
        if (phase == 1) arp_semitones = ch->arpeggio_param >> 4;
        else if (phase == 2) arp_semitones = ch->arpeggio_param & 0x0f;
      }
      if (arp_semitones != 0) {
        uint16_t saved = ch->period;
        ch->period = period_semitones_up(ch->period, arp_semitones);
        render_channel_span(st, ch, 0, out + written * 2, span);
        ch->period = saved;
      } else {
        render_channel_span(st, ch, vib_delta, out + written * 2, span);
      }
    }

    written += span;
    st->samples_until_next_tick -= (uint64_t)span;
  }
}
