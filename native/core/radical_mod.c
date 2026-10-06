#include "radical_mod.h"

#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------- Mod.java

#define RM_MAX_TRACKS 32
#define RM_REALBYTES 18000000 // turnbytesNorm()'s fixed output array

typedef struct {
  int8_t *samples; // sample_length + 8, like the Java array
  int32_t sample_length;
  int32_t finetune_rate, period_low_limit, period_high_limit;
  int32_t finetune_value, volume, repeat_point, repeat_length;
} RmInstrument;

typedef struct {
  int32_t numtracks;
  int32_t numpatterns;
  int8_t **patterns;
  RmInstrument insts[31];
  int32_t ninsts;
  int8_t positions[128];
  int32_t song_length_patterns;
} RmMod;

static uint32_t fourcc(const char *s) {
  return (uint32_t)(uint8_t)s[3] | (uint32_t)(uint8_t)s[2] << 8 | (uint32_t)(uint8_t)s[1] << 16 |
         (uint32_t)(uint8_t)s[0] << 24;
}

static void mod_free(RmMod *m) {
  for (int32_t i = 0; i < m->ninsts; i++) free(m->insts[i].samples);
  if (m->patterns) {
    for (int32_t i = 0; i < m->numpatterns; i++) free(m->patterns[i]);
    free(m->patterns);
  }
  memset(m, 0, sizeof(*m));
}

static int32_t rd_u8(const uint8_t *d, size_t len, size_t *p, bool *eof) {
  if (*p >= len) { *eof = true; return 0; }
  return d[(*p)++];
}

static int32_t rd_u16(const uint8_t *d, size_t len, size_t *p, bool *eof) {
  int32_t hi = rd_u8(d, len, p, eof);
  int32_t lo = rd_u8(d, len, p, eof);
  return (hi << 8 | lo) & 0xFFFF;
}

// Mod.loadMod(). Only the 31-sample signatures: the "Format unknown" branch
// for 15-sample files is not ported (no shipped track takes it).
static bool mod_parse(const uint8_t *d, size_t len, RmMod *m) {
  memset(m, 0, sizeof(*m));
  if (len < 1084) return false;
  static const char *const kSigs[] = {"M.K.", "M!K!", "M&K!", "FLT4", "FLT8", "8CHN", "6CHN", "10CH",
                                      "12CH", "14CH", "16CH", "18CH", "20CH", "22CH", "24CH", "26CH",
                                      "28CH", "30CH", "32CH", "11CH", "13CH", "15CH", "TDZ1", "TDZ2",
                                      "TDZ3", "5CHN", "7CHN", "9CHN"};
  static const int32_t kTracks[] = {4, 4, 4, 4, 4, 8, 6, 10, 12, 14, 16, 18, 20, 22, 24, 26,
                                    28, 30, 32, 11, 13, 15, 1, 2, 3, 5, 7, 9};
  uint32_t sig = (uint32_t)d[1080] << 24 | (uint32_t)d[1081] << 16 | (uint32_t)d[1082] << 8 | d[1083];
  int32_t found = -1;
  for (int32_t i = 0; i < (int32_t)(sizeof(kTracks) / sizeof(kTracks[0])); i++) {
    if (sig == fourcc(kSigs[i])) { found = i; break; }
  }
  if (found < 0) return false;
  m->numtracks = kTracks[found];
  m->ninsts = 31;

  size_t p = 20;
  bool eof = false;
  for (int32_t j = 0; j < 31; j++) {
    RmInstrument *in = &m->insts[j];
    p += 22;
    in->sample_length = rd_u16(d, len, &p, &eof) << 1;
    in->samples = calloc((size_t)in->sample_length + 8, 1);
    if (!in->samples) { mod_free(m); return false; }
    int32_t fine = rd_u8(d, len, &p, &eof) & 0xF;
    fine = fine > 7 ? fine - 16 : fine;
    in->finetune_value = (int8_t)(fine << 4);
    in->volume = rd_u8(d, len, &p, &eof);
    in->repeat_point = rd_u16(d, len, &p, &eof) << 1;
    in->repeat_length = rd_u16(d, len, &p, &eof) << 1;
    if (in->repeat_point > in->sample_length) in->repeat_point = in->sample_length - 1;
    if (in->repeat_point + in->repeat_length > in->sample_length)
      in->repeat_length = in->sample_length - in->repeat_point;
  }
  // readSequence()
  m->song_length_patterns = rd_u8(d, len, &p, &eof);
  rd_u8(d, len, &p, &eof); // song_repeat_patterns, unused by the renderer
  if (p + 128 > len) eof = true;
  if (eof) { mod_free(m); return false; }
  memcpy(m->positions, d + p, 128);
  p += 128;
  m->numpatterns = 0;
  for (int32_t i = 0; i < 128; i++) {
    if (m->positions[i] > m->numpatterns) m->numpatterns = m->positions[i];
  }
  m->numpatterns++;
  p += 4; // the signature
  // readPatterns(): a short read here is an exception the Mod doesn't catch.
  size_t psize = (size_t)m->numtracks * 4 * 64;
  m->patterns = calloc((size_t)m->numpatterns, sizeof(int8_t *));
  if (!m->patterns) { mod_free(m); return false; }
  for (int32_t j = 0; j < m->numpatterns; j++) {
    if (p + psize > len) { mod_free(m); return false; }
    m->patterns[j] = malloc(psize);
    if (!m->patterns[j]) { mod_free(m); return false; }
    memcpy(m->patterns[j], d + p, psize);
    p += psize;
  }
  // readSampleData(): EOF is caught -- the partial sample keeps what it got
  // and the rest stay silent.
  for (int32_t k = 0; k < 31; k++) {
    RmInstrument *in = &m->insts[k];
    size_t avail = p < len ? len - p : 0;
    if (avail < (size_t)in->sample_length) {
      memcpy(in->samples, d + p, avail);
      break;
    }
    memcpy(in->samples, d + p, (size_t)in->sample_length);
    p += (size_t)in->sample_length;
    if (in->repeat_length > 3) memmove(in->samples + in->sample_length, in->samples + in->repeat_point, 8);
  }
  return true;
}

// ----------------------------------------------------------- ModSlayer.java

typedef struct {
  const int8_t *samples;
  int32_t alen; // samples' Java array length, sample_length + 8
  int32_t position, length, repeat, replen, volume, error, pitch;
  int32_t start_period, period, effect, portto, vibpos, oldsampofs;
  int32_t arp[3], arpindex;
  int32_t vol_slide, port_inc, port_up, port_down, vib_rate, vib_depth;
  int32_t finetune_rate, period_low_limit, period_high_limit;
  int32_t noterestart, notelimit;
} RmTrack;

static const int32_t kVolAdj[65] = {0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
                                    17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33,
                                    34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50,
                                    51, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 63};
static const int32_t kSin[32] = {0,   25,  50,  74,  98,  120, 142, 162, 180, 197, 212,
                                 225, 236, 244, 250, 254, 255, 254, 250, 244, 236, 225,
                                 212, 197, 180, 162, 142, 120, 98,  74,  50,  25};
static const int32_t kPeriods[84] = {
    1712, 1616, 1525, 1440, 1359, 1283, 1211, 1143, 1078, 1018, 961, 907, 856, 808, 763, 720, 679,
    641,  605,  571,  539,  509,  480,  453,  428,  404,  381,  360, 340, 321, 303, 286, 270, 254,
    240,  227,  214,  202,  191,  180,  170,  160,  151,  143,  135, 127, 120, 113, 107, 101, 95,
    90,   85,   80,   76,   71,   67,   64,   60,   57,   53,   50,  48,  45,  42,  40,  38,  36,
    34,   32,   30,   28,   27,   25,   24,   22,   21,   20,   19,  18,  17,  16,  15,  14};

typedef struct {
  RmMod *mod;
  RmTrack tracks[RM_MAX_TRACKS];
  int32_t numtracks;
  int32_t order_pos, tempo, tempo_wait, bpm, row, break_row, bpm_samples, pattofs;
  const int8_t *patt;
  int32_t mixspeed, samplingrate, gain, vol_shift, nloops;
  bool mod_done;
  int32_t loopA, loopB, loops;
  bool onLoop;
  int32_t jumpTo, jumpLocation;
  bool reverseJump;
  int32_t *patternOffsets;
  int32_t oln, rollBackPos, rollBackTrig;
  bool loopMark;
  bool failed; // the Java threw: AIOOBE or a division by zero
} RmSlayer;

static int32_t sdiv(RmSlayer *s, int32_t a, int32_t b) {
  if (b == 0) { s->failed = true; return 0; }
  if (a == INT32_MIN && b == -1) return a;
  return a / b;
}

static void beattrack(RmSlayer *s, RmTrack *t) {
  if (t->period_low_limit == 0) t->period_low_limit = 1;
  if (t->effect & 0x1) {
    t->volume += t->vol_slide;
    if (t->volume < 0) t->volume = 0;
    if (t->volume > 64) t->volume = 64;
  }
  if (t->effect & 0x2) {
    if ((t->period += t->port_down) > t->period_high_limit) t->period = t->period_high_limit;
    t->pitch = sdiv(s, t->finetune_rate, t->period);
  }
  if (t->effect & 0x4) {
    if ((t->period -= t->port_up) < t->period_low_limit) t->period = t->period_low_limit;
    t->pitch = sdiv(s, t->finetune_rate, t->period);
  }
  if (t->effect & 0x20) {
    if (t->portto < t->period) {
      if ((t->period += t->port_inc) > t->portto) t->period = t->portto;
    } else if (t->portto > t->period && (t->period -= t->port_inc) < t->portto) {
      t->period = t->portto;
    }
    t->pitch = sdiv(s, t->finetune_rate, t->period);
  }
  if (t->effect & 0x8) {
    t->vibpos += t->vib_rate << 2;
    int32_t i = kSin[t->vibpos >> 2 & 0x1F] * t->vib_depth >> 7;
    if (t->vibpos & 0x80) i = -i;
    i += t->period;
    i = i > 0 ? i : 1;
    t->pitch = sdiv(s, t->finetune_rate, i);
  }
  if (t->effect & 0x10) {
    t->pitch = sdiv(s, t->finetune_rate, t->arp[t->arpindex]);
    if (++t->arpindex >= 3) t->arpindex = 0;
  }
}

static int32_t get_track(RmSlayer *s, RmTrack *t, const int8_t *is, int32_t i) {
  int32_t inst = is[i] & 0xF0;
  int32_t period = (is[i++] & 0xF) << 8;
  period |= is[i++] & 0xFF;
  const int32_t eff = is[i] & 0xF;
  inst |= (is[i++] & 0xF0) >> 4;
  int32_t prm = is[i++];
  t->effect = 0;
  if (inst != 0) {
    --inst;
    if (inst >= s->mod->ninsts) { s->failed = true; return i; }
    const RmInstrument *in = &s->mod->insts[inst];
    t->volume = in->volume;
    t->length = in->sample_length;
    t->repeat = in->repeat_point;
    t->replen = in->repeat_length;
    t->finetune_rate = in->finetune_rate;
    t->samples = in->samples;
    t->alen = in->sample_length + 8;
    t->period_low_limit = in->period_low_limit;
    t->period_high_limit = in->period_high_limit;
  }
  t->notelimit = -1;
  t->noterestart -= s->tempo;
  if (t->noterestart < -1) t->noterestart = -1;
  if (period != 0) {
    t->portto = period;
    if (eff != 3 && eff != 5) {
      t->period = period;
      t->start_period = period;
      t->pitch = sdiv(s, t->finetune_rate, period);
      t->position = 0;
    }
  }
  if (eff == 0 && prm == 0) return i;
  switch (eff) {
    case 0: {
      int32_t k;
      for (k = 12; k < 48 && t->period < kPeriods[k]; ++k) {}
      t->arp[0] = kPeriods[k];
      t->arp[1] = kPeriods[k + (prm & 0xF)];
      t->arp[2] = kPeriods[k + ((prm & 0xF0) >> 4)];
      t->arpindex = 0;
      t->effect |= 0x10;
      break;
    }
    case 1:
      t->effect |= 0x4;
      if (prm != 0) t->port_up = prm;
      break;
    case 2:
      t->effect |= 0x2;
      if (prm != 0) t->port_down = prm;
      break;
    case 3:
      if (prm != 0) t->port_inc = prm & 0xFF;
      t->effect |= 0x20;
      break;
    case 4:
      if (prm & 0xF) t->vib_depth = prm & 0xF;
      if (prm & 0xF0) t->vib_rate = (prm & 0xF0) >> 4;
      if (period != 0) t->vibpos = 0;
      t->effect |= 0x8;
      break;
    case 9:
      if (prm == 0) prm = t->oldsampofs;
      t->oldsampofs = prm;
      t->position = (prm & 0xFF) << 8;
      break;
    case 5:
    case 6:
    case 10:
      // The Java's fall-through chain: 5 adds tone portamento, 6 vibrato,
      // and all three slide the volume.
      if (eff == 5) t->effect |= 0x20;
      if (eff == 6) t->effect |= 0x8;
      t->vol_slide = ((prm & 0xF0) >> 4) - (prm & 0xF);
      t->effect |= 0x1;
      break;
    case 11:
      if (s->jumpLocation == s->order_pos && s->reverseJump) {
        s->reverseJump = false;
        break;
      }
      if (s->reverseJump || prm >= 128 || prm < 0) break;
      s->jumpTo = prm;
      s->jumpLocation = s->order_pos;
      if (s->jumpTo >= s->order_pos) break;
      s->reverseJump = true;
      if (s->order_pos == s->mod->song_length_patterns) {
        if (prm < s->order_pos - 2) {
          if (prm >= s->mod->song_length_patterns) { s->failed = true; break; }
          s->rollBackPos = s->patternOffsets[prm];
          s->loopMark = true;
        }
        s->jumpTo = -1;
        s->jumpLocation = 0;
      }
      break;
    case 12:
      t->volume = (prm > 64 || prm < 0) ? 64 : prm;
      break;
    case 13:
      s->break_row = ((prm & 0xF0) >> 4) * 10 + (prm & 0xF);
      s->row = 64;
      break;
    case 14: {
      const int32_t sub = (prm & 0xF0) >> 4;
      prm &= 0xF;
      switch (sub) {
        case 1:
          t->period += prm;
          if (t->period > t->period_high_limit) t->period = t->period_high_limit;
          t->pitch = sdiv(s, t->finetune_rate, t->period);
          break;
        case 2:
          t->period -= prm;
          if (t->period < t->period_low_limit) t->period = t->period_low_limit;
          t->pitch = sdiv(s, t->finetune_rate, t->period);
          break;
        case 6:
          if (s->loops != 0 || s->onLoop) break;
          if (prm <= 0) {
            s->loopA = s->row;
            break;
          }
          s->loopB = s->row;
          s->loops = prm;
          s->onLoop = true;
          break;
        case 9:
          t->noterestart = prm;
          break;
        case 10:
          t->volume += prm;
          if (t->volume > 64) t->volume = 64;
          if (t->volume < 0) t->volume = 0;
          break;
        case 11:
          t->volume -= prm;
          if (t->volume > 64) t->volume = 64;
          if (t->volume < 0) t->volume = 0;
          break;
        case 12:
          t->notelimit = prm;
          break;
        default:
          break;
      }
      break;
    }
    case 15:
      if (prm == 0) break;
      prm &= 0xFF;
      if (prm <= 32) {
        s->tempo = prm;
        s->tempo_wait = prm;
        break;
      }
      s->bpm = prm;
      s->bpm_samples = sdiv(s, s->samplingrate, 103 * prm >> 8);
      break;
    default:
      break;
  }
  return i;
}

static bool load_pattern(RmSlayer *s) {
  if (s->order_pos < 0 || s->order_pos >= 128) return false;
  int32_t pat = s->mod->positions[s->order_pos];
  if (pat < 0 || pat >= s->mod->numpatterns) return false;
  s->patt = s->mod->patterns[pat];
  return true;
}

static void updatetracks(RmSlayer *s) {
  s->tempo_wait = s->tempo;
  if (s->jumpTo != -1) {
    s->onLoop = false;
    s->loopA = s->row;
    s->row = s->break_row;
    s->break_row = 0;
    s->order_pos = s->jumpTo;
    if (!load_pattern(s)) { s->failed = true; return; }
    s->pattofs = s->row * 4 * s->numtracks;
    ++s->order_pos;
    s->jumpTo = -1;
  }
  if (s->row >= 64) {
    s->onLoop = false;
    s->loopA = s->row;
    if (s->order_pos >= s->mod->song_length_patterns) {
      s->order_pos = 0;
      if (--s->nloops == 0) s->mod_done = true;
    }
    s->row = s->break_row;
    s->break_row = 0;
    // (positions[] holds signed bytes, so the Java's `== 255` never fires.)
    if (!load_pattern(s)) { s->failed = true; return; }
    s->pattofs = s->row * 4 * s->numtracks;
    ++s->order_pos;
  } else {
    if (s->loops > 0 && s->row == s->loopB) {
      s->row = s->loopA - 1;
      s->pattofs = s->row * 4 * s->numtracks;
      --s->loops;
    }
    if (s->loops == 0 && s->row == s->loopB + 1) {
      s->onLoop = false;
      s->loopA = s->row;
    }
  }
  ++s->row;
  for (int32_t i = 0; i < s->numtracks; ++i) {
    if (s->pattofs < 0 || s->pattofs + 4 > s->numtracks * 4 * 64) { s->failed = true; return; }
    s->pattofs = get_track(s, &s->tracks[i], s->patt, s->pattofs);
    if (s->failed) return;
  }
}

// One source byte of a track, AIOOBE-checked against the Java array.
static inline int32_t smp(RmSlayer *s, const RmTrack *t, int32_t i, int32_t alen) {
  if ((uint32_t)i >= (uint32_t)alen) { s->failed = true; return 0; }
  return t->samples[i];
}

static void mixtrack_16_mono(RmSlayer *s, RmTrack *t, int32_t *buffer, int32_t buffpos, int32_t bufflen) {
  int32_t samplepos = t->position;
  const int32_t volume = kVolAdj[t->volume] * s->gain >> (s->vol_shift + 8);
  int32_t error = t->error;
  const int32_t lopitch = t->pitch & 0xFFF;
  const int32_t hipitch = t->pitch >> 12;
  const int32_t alen = t->samples ? t->alen : 0;
  if (t->replen < 3) {
    const int32_t endtr = t->length;
    if (samplepos < endtr) {
      const int32_t buffend = buffpos + bufflen;
      while (samplepos < endtr && buffpos < buffend) {
        if (t->notelimit == -1 || s->tempo - s->tempo_wait < t->notelimit) {
          const int32_t n = buffpos++;
          if (t->pitch < 4096) {
            buffer[n] += (smp(s, t, samplepos, alen) * (4096 - error) + smp(s, t, samplepos + 1, alen) * error) *
                             volume >> 12;
          } else {
            buffer[n] += smp(s, t, samplepos, alen) * volume;
          }
        } else {
          t->volume = 0;
        }
        samplepos += hipitch + ((error += lopitch) >> 12);
        error &= 0xFFF;
        if (s->failed) return;
      }
      t->error = error;
      if (t->noterestart == -1 || s->tempo - s->tempo_wait < t->noterestart) {
        t->position = samplepos;
      } else {
        t->position = 0;
        t->noterestart = -1;
      }
    }
  } else {
    const int32_t endtr = t->replen + t->repeat;
    while (bufflen > 0) {
      if (samplepos >= endtr) samplepos -= t->replen;
      if (samplepos < 0) samplepos = 0;
      while (samplepos >= alen) {
        if (alen <= 0) { s->failed = true; return; }
        samplepos -= alen;
      }
      if (t->notelimit == -1 || s->tempo - s->tempo_wait < t->notelimit) {
        const int32_t n = buffpos++;
        if (t->pitch < 4096) {
          buffer[n] += (smp(s, t, samplepos, alen) * (4096 - error) + smp(s, t, samplepos + 1, alen) * error) *
                           volume >> 12;
        } else {
          buffer[n] += smp(s, t, samplepos, alen) * volume;
        }
      } else {
        t->volume = 0;
      }
      samplepos += hipitch + ((error += lopitch) >> 12);
      error &= 0xFFF;
      --bufflen;
      if (s->failed) return;
    }
    t->error = error;
    if (t->noterestart == -1 || s->tempo - s->tempo_wait < t->noterestart) {
      t->position = samplepos;
    } else {
      t->position = 0;
      t->noterestart = -1;
    }
  }
}

static void startplaying(RmSlayer *s, int32_t bpm) {
  s->mixspeed = s->samplingrate; // oversample is always 1
  s->order_pos = 0;
  s->tempo = 6;
  s->tempo_wait = 6;
  s->bpm = bpm;
  s->row = 64;
  s->break_row = 0;
  s->bpm_samples = sdiv(s, s->samplingrate, 24 * s->bpm / 60);
  s->numtracks = s->mod->numtracks;
  for (int32_t i = 0; i < s->numtracks; i++) {
    memset(&s->tracks[i], 0, sizeof(RmTrack));
    s->tracks[i].noterestart = -1;
    s->tracks[i].notelimit = -1;
  }
  for (int32_t k = 0; k < s->mod->ninsts; k++) {
    RmInstrument *in = &s->mod->insts[k];
    const int32_t den = s->mixspeed * (1536 - in->finetune_value);
    if (den == 0) { s->failed = true; return; }
    in->finetune_rate = (int32_t)(22748294283264LL / (int64_t)den);
    in->period_low_limit = 113;
    in->period_high_limit = 856;
  }
  s->vol_shift = s->numtracks > 8 ? 2 : (s->numtracks > 4 ? 1 : 0);
}

bool radical_render(const uint8_t *mod_bytes, size_t mod_len, int32_t samplingrate, int32_t gain,
                    int32_t bpm, RadicalTrack *out) {
  memset(out, 0, sizeof(*out));
  RmMod mod;
  if (!mod_parse(mod_bytes, mod_len, &mod)) return false;
  RmSlayer *s = calloc(1, sizeof(RmSlayer));
  if (!s) { mod_free(&mod); return false; }
  s->mod = &mod;
  s->samplingrate = samplingrate;
  s->gain = gain;
  s->nloops = 1;
  s->jumpTo = -1;
  startplaying(s, bpm);

  int32_t *buf = calloc((size_t)(s->mixspeed > 0 ? s->mixspeed : 1), sizeof(int32_t));
  s->patternOffsets = calloc((size_t)(mod.song_length_patterns > 0 ? mod.song_length_patterns : 1), sizeof(int32_t));
  // The Java fills a fixed 18MB array; this grows to what is used.
  int32_t cap = 1 << 20;
  uint8_t *bytes = malloc((size_t)cap);
  bool ok = buf && s->patternOffsets && bytes && !s->failed;
  if (bytes) bytes[0] = 0; // the stream's last byte when nothing was ever rendered

  while (ok && !s->mod_done) {
    if (--s->tempo_wait > 0) {
      for (int32_t c = 0; c < s->numtracks; ++c) beattrack(s, &s->tracks[c]);
    } else {
      updatetracks(s);
    }
    if (s->failed) { ok = false; break; }
    if (!s->mod_done) {
      if (s->row == 1 && s->tempo_wait == s->tempo) {
        int32_t at = s->order_pos - 1;
        if (at < 0 || at >= mod.song_length_patterns) { ok = false; break; }
        s->patternOffsets[at] = s->oln;
      }
      const int32_t n = s->bpm_samples;
      if (n < 0 || n > s->mixspeed) { ok = false; break; }
      memset(buf, 0, (size_t)n * sizeof(int32_t));
      for (int32_t i = 0; i < s->numtracks; ++i) mixtrack_16_mono(s, &s->tracks[i], buf, 0, n);
      if (s->failed) { ok = false; break; }
      if (s->oln + n < RM_REALBYTES) {
        // intToBytes16(): one byte per sample (its high byte), then the
        // last sample's low byte one past the end, which the next block's
        // first high byte overwrites.
        if (s->oln + n + 1 > cap) {
          while (s->oln + n + 1 > cap) cap *= 2;
          if (cap > RM_REALBYTES) cap = RM_REALBYTES;
          uint8_t *nb = realloc(bytes, (size_t)cap);
          if (!nb) { ok = false; break; }
          bytes = nb;
        }
        for (int32_t i = 0; i < n; i++) bytes[s->oln + i] = (uint8_t)(buf[i] >> 8);
        if (n > 0) bytes[s->oln + n] = (uint8_t)(buf[n - 1] & 0xFF);
        s->oln += n;
      }
    }
    if (s->loopMark) {
      s->rollBackTrig = s->oln;
      s->loopMark = false;
    }
  }

  if (ok) ++s->oln; // every block already wrote the byte this takes in
  if (ok) {
    out->bytes = bytes;
    out->len = s->oln;
    out->roll_back_pos = s->rollBackPos;
    out->roll_back_trig = s->oln - s->rollBackTrig;
  } else {
    free(bytes);
  }
  free(buf);
  free(s->patternOffsets);
  free(s);
  mod_free(&mod);
  return ok;
}

bool radical_render_stage(const uint8_t *mod, size_t mod_len, int32_t vol, int32_t rate, int32_t bpm,
                          RadicalTrack *out) {
  // RadicalMod's constructor, float for float.
  const int32_t samplingrate = (int32_t)((float)rate / 8000.0f * 2.0f * 22000.0f);
  const int32_t gain = (int32_t)((float)vol * 0.8f);
  return radical_render(mod, mod_len, samplingrate, gain, bpm, out);
}

bool radical_render_interface(const uint8_t *mod, size_t mod_len, RadicalTrack *out) {
  return radical_render(mod, mod_len, 44000, 160, 125, out);
}

void radical_track_free(RadicalTrack *t) {
  free(t->bytes);
  memset(t, 0, sizeof(*t));
}

// ----------------------------------------------------------- SuperClip.java

// One pass of SuperClip.run()'s loop: what it writes to the line.
static void superclip_step(RadicalPlayer *p) {
  const RadicalTrack *t = p->track;
  const int32_t skiprate = RADICAL_PLAY_RATE;
  p->pending_len = 0;
  p->pending_idx = 0;
  int32_t available = t->len - p->pos;
  if (available % 2 != 0) ++available;
  int32_t alen = available > skiprate ? skiprate : available;
  int32_t read = -1;
  if (p->pos < t->len) {
    read = t->len - p->pos < alen ? t->len - p->pos : alen;
    memcpy(p->pending, t->bytes + p->pos, (size_t)read);
    memset(p->pending + read, 0, (size_t)(alen - read));
    p->pos += read;
  }
  bool wrap = read == -1 || (t->roll_back_pos != 0 && available < t->roll_back_trig);
  if (wrap) {
    if (read != -1) p->pending_len = alen;
    p->pos = 0;
    if (t->roll_back_pos != 0) p->pos = t->roll_back_pos < t->len ? t->roll_back_pos : t->len;
    int32_t available2 = t->len - p->pos;
    if (available2 % 2 != 0) ++available2;
    int32_t alen2 = available2 > skiprate ? skiprate : available2;
    uint8_t *dst = p->pending + p->pending_len;
    int32_t got = t->len - p->pos < alen2 ? t->len - p->pos : alen2;
    if (got < 0) got = 0;
    memcpy(dst, t->bytes + p->pos, (size_t)got);
    memset(dst + got, 0, (size_t)(alen2 - got));
    p->pos += got;
    p->pending_len += alen2;
  } else {
    p->pending_len = alen;
  }
}

int16_t radical_player_next_frame(RadicalPlayer *p) {
  if (!p->track || p->track->len <= 0) return 0;
  if (p->pending_idx + 2 > p->pending_len) {
    // Chunks are always an even length, so frames never straddle them.
    for (int32_t guard = 0; guard < 4 && p->pending_idx + 2 > p->pending_len; guard++) superclip_step(p);
    if (p->pending_idx + 2 > p->pending_len) return 0;
  }
  int16_t v = (int16_t)(p->pending[p->pending_idx] | p->pending[p->pending_idx + 1] << 8);
  p->pending_idx += 2;
  return v;
}

void radical_player_start(RadicalPlayer *p, const RadicalTrack *t, int32_t output_rate) {
  memset(p, 0, sizeof(*p));
  p->track = t;
  p->step = ((uint64_t)RADICAL_PLAY_RATE << 32) / (uint64_t)(output_rate > 0 ? output_rate : RADICAL_PLAY_RATE);
  p->cur = radical_player_next_frame(p);
  p->next = radical_player_next_frame(p);
}

float radical_music_gain = 1.0f;

void radical_player_render(RadicalPlayer *p, int16_t *out, int32_t frames) {
  if (!p->track) return;
  for (int32_t f = 0; f < frames; f++) {
    const int32_t s0 = p->cur, s1 = p->next;
    const int32_t v = (int32_t)((float)(s0 + (int32_t)(((int64_t)(s1 - s0) * (int64_t)(p->frac & 0xffffffffu)) >> 32)) * radical_music_gain);
    for (int32_t c = 0; c < 2; c++) {
      int32_t m = out[f * 2 + c] + v;
      if (m > 32767) m = 32767; else if (m < -32768) m = -32768;
      out[f * 2 + c] = (int16_t)m;
    }
    p->frac += p->step;
    while (p->frac >= ((uint64_t)1 << 32)) {
      p->frac -= (uint64_t)1 << 32;
      p->cur = p->next;
      p->next = radical_player_next_frame(p);
    }
  }
}
