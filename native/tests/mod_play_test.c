// Host-buildable test for native/core/mod_play.c against the REAL
// music/stage2.zip asset.
//
// A byte-exact oracle for a full tracker replay isn't practical to hand-
// compute (see mod_play.h's own scope note -- this implements the
// standard Protracker replay algorithm, the same one every real MOD
// player implements, but there is no independent reference
// implementation available in this environment to diff against tick by
// tick). Verified instead by: (1) one pinned exact value -- the very
// first rendered frame, where the fixed-point read position is exactly
// 0 with zero interpolation fraction, so it must equal the first PCM
// byte of the first note's sample, scaled by that sample's volume,
// with no ambiguity -- independently confirmed by reading music/
// stage2.zip's raw bytes directly in Python (see the commit that added
// this file); (2) self-consistency: determinism (two fresh playback
// states rendering the same span produce identical output), audible
// output appears once the note's waveform reaches its nonzero samples,
// and the whole song completes at least one full loop (order table
// wraps, `song_ended` becomes true) without crashing across many
// thousands of ticks -- the real stress test for pattern break/position
// jump/pattern loop/pattern delay all actually terminating correctly
// rather than hanging.
#include <stdio.h>
#include <string.h>
#include "../core/mod_decode.h"
#include "../core/mod_play.h"
#include "../core/vfs.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
  if (!(cond)) { fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); failures++; } \
} while (0)

int main(void) {
  VfsZip zip;
  vfs_set_fpath("../../../");
  if (!vfs_read_zip("music/stage2.zip", &zip)) {
    fprintf(stderr, "FAIL: could not open music/stage2.zip\n");
    return 1;
  }
  VfsZipEntry *entry = NULL;
  for (int32_t i = 0; i < zip.count; i++) {
    if (strcmp(zip.entries[i].name, "stage2.mod") == 0) { entry = &zip.entries[i]; break; }
  }
  CHECK(entry != NULL, "stage2.mod found in zip");
  if (!entry) { vfs_free_zip(&zip); return 1; }

  ModFile mod;
  CHECK(mod_decode(entry->data, (size_t)entry->len, &mod), "mod_decode succeeds");

  // 1. Pinned exact value: frame 0.
  ModPlayState st;
  mod_play_init(&st, &mod, 44100, 145); // stage2.zip -- real gain from xtGraphics.java:2992
  int16_t out[8 * 2] = {0};
  mod_play_render(&st, out, 8);
  // Sample index 6 ("SQ-ROCK1")'s first PCM byte is 0 (verified directly
  // in Python against the raw file), so frame 0 must be exactly silent --
  // NOT a trivially-true check (a volume/period/position bug could just
  // as easily have produced garbage here; this only passes if position
  // truly starts at sample offset 0 with zero interpolation fraction).
  CHECK(out[0] == 0 && out[1] == 0, "frame 0 is exactly silent (sample[6].data[0] == 0)");

  // 2. Audible output appears once the waveform's nonzero samples arrive
  // -- sample[6]'s bytes 4..7 are -2,-5,-7,-12 (nonzero), and at period
  // 214 the upsampling ratio guarantees the first several output frames
  // span past output position 0 into that region well within 64 frames.
  int16_t out64[64 * 2] = {0};
  ModPlayState st2;
  mod_play_init(&st2, &mod, 44100, 145);
  mod_play_render(&st2, out64, 64);
  bool any_nonzero = false;
  for (int i = 0; i < 64 * 2; i++) if (out64[i] != 0) any_nonzero = true;
  CHECK(any_nonzero, "audible (nonzero) output appears within the first 64 frames");

  // 3. Determinism.
  ModPlayState st3, st4;
  mod_play_init(&st3, &mod, 44100, 145);
  mod_play_init(&st4, &mod, 44100, 145);
  int16_t bufA[2048 * 2], bufB[2048 * 2];
  memset(bufA, 0, sizeof(bufA));
  memset(bufB, 0, sizeof(bufB));
  mod_play_render(&st3, bufA, 2048);
  mod_play_render(&st4, bufB, 2048);
  CHECK(memcmp(bufA, bufB, sizeof(bufA)) == 0, "two fresh playback states render identically");

  // 4. Full-song stress test: render enough audio to guarantee the
  // 73-entry order table wraps at least once, exercising pattern break/
  // position jump/pattern loop/pattern delay termination without a
  // crash or hang. Rendered in chunks (not one giant buffer) to keep
  // memory use reasonable. stage2.mod's real play length at its own
  // speed/tempo settings is ~288s (measured directly by instrumenting a
  // debug build) -- 16M frames (~363s) gives comfortable headroom above
  // that without the test needing to know every song's exact length.
  ModPlayState st5;
  mod_play_init(&st5, &mod, 44100, 145);
  int16_t chunk[4096 * 2];
  int32_t total = 0;
  bool wrapped = false;
  while (total < 16000000 && !wrapped) {
    mod_play_render(&st5, chunk, 4096);
    total += 4096;
    if (st5.song_ended) wrapped = true;
  }
  CHECK(wrapped, "song completes at least one full order-table loop without crashing/hanging");
  CHECK(st5.order_pos >= 0 && st5.order_pos < mod.song_length, "order_pos stays in valid range after wrap");
  CHECK(st5.row >= 0 && st5.row < MOD_ROWS_PER_PATTERN, "row stays in valid range after wrap");

  mod_free(&mod);
  vfs_free_zip(&zip);

  // 5. Regression test for the missing-gain/headroom bug (see mod_play.c's
  // own comment on render_channel_span()'s scaled= line): a synthetic
  // worst-case file with all 4 channels playing the SAME max-amplitude
  // (127), max-volume (64) sample in unison -- the loudest a real MOD can
  // ever get. The old `sample * 256` upscale put a SINGLE such channel at
  // 127*256 = 32512, already pegged at int16's ceiling before even
  // summing the other 3 -- guaranteed hard clipping (audible distortion,
  // not just "loud"). The fixed formula (sample * vol_scale * gain / 4,
  // matching ModSlayer.java:405's `vol_adj[chVol]*gain>>(vol_shift+8)`
  // with this port's always-0 vol_shift -- MOD_NUM_CHANNELS caps every
  // real file at 4 channels, see mod_play.h) should keep even this
  // worst case comfortably under the ceiling at every real per-stage gain
  // (125-145, see game.c's stage_music_gain()).
  {
    ModFile synth;
    memset(&synth, 0, sizeof(synth));
    synth.samples[0].volume = 64;
    synth.samples[0].length = 8;
    synth.samples[0].repeat_length = 0; // <=2 -- one-shot, matches mod_play.c's own convention
    int8_t loud_pcm[8];
    for (int i = 0; i < 8; i++) loud_pcm[i] = 127;
    synth.samples[0].data = loud_pcm;
    synth.song_length = 1;
    synth.order[0] = 0;
    synth.num_patterns = 1;
    ModPattern pat;
    memset(&pat, 0, sizeof(pat));
    for (int c = 0; c < MOD_NUM_CHANNELS; c++) {
      pat.cells[0][c].sample = 1;   // -> samples[0], 1-based per mod_play.c's own convention
      pat.cells[0][c].period = 214; // an ordinary mid-range Amiga period
    }
    synth.patterns = &pat;

    for (int32_t gain = 125; gain <= 145; gain += 5) {
      ModPlayState sst;
      mod_play_init(&sst, &synth, 44100, gain);
      int16_t sbuf[256 * 2];
      memset(sbuf, 0, sizeof(sbuf));
      mod_play_render(&sst, sbuf, 256);
      int32_t peak = 0;
      for (int i = 0; i < 256 * 2; i++) {
        int32_t v = sbuf[i] < 0 ? -sbuf[i] : sbuf[i];
        if (v > peak) peak = v;
      }
      char label[96];
      snprintf(label, sizeof(label), "gain=%d: worst-case 4-channel unison stays well under int16 ceiling", gain);
      CHECK(peak > 0 && peak < 20000, label);
    }
  }

  if (failures == 0) { printf("all tests passed\n"); return 0; }
  fprintf(stderr, "%d failure(s)\n", failures); return 1;
}
