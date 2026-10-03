// Host-buildable test for native/core/mod_decode.c against the REAL
// music/stage2.zip asset.
//
// No JS/Java oracle exists for this one (see mod_decode.h's own scope
// comment -- fresh code for a well-defined external file format, not a
// port). Expected values below were captured by an independent from-
// scratch Python parser written directly against the Protracker MOD
// spec's published byte layout (not by importing any MOD-aware
// library), reading music/stage2.zip's real stage2.mod -- same
// methodology as gif_decode_test.c's PIL cross-check, minus the library
// (none exists in Python's stdlib for MOD, so the "independent" half of
// the oracle is a second, differently-written implementation of the
// same spec instead of a trusted third-party decoder).
#include <stdio.h>
#include <string.h>
#include "../core/mod_decode.h"
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
  bool ok = mod_decode(entry->data, (size_t)entry->len, &mod);
  CHECK(ok, "mod_decode succeeds");
  if (!ok) { vfs_free_zip(&zip); return 1; }

  CHECK(strcmp(mod.title, "Madness stage2") == 0, "title");
  CHECK(mod.song_length == 73, "song_length");
  CHECK(mod.restart_position == 0, "restart_position");
  int32_t want_order[8] = {0, 1, 3, 4, 5, 6, 7, 8};
  for (int i = 0; i < 8; i++) {
    char label[32]; snprintf(label, sizeof(label), "order[%d]", i);
    CHECK(mod.order[i] == want_order[i], label);
  }
  CHECK(mod.num_patterns == 44, "num_patterns");

  CHECK(strcmp(mod.samples[0].name, "st-04:ANIMATE-KICK") == 0, "sample[0].name");
  CHECK(mod.samples[0].length == 2630, "sample[0].length");
  CHECK(mod.samples[0].finetune == 0, "sample[0].finetune");
  CHECK(mod.samples[0].volume == 64, "sample[0].volume");
  CHECK(mod.samples[0].repeat_offset == 0, "sample[0].repeat_offset");
  CHECK(mod.samples[0].repeat_length == 0, "sample[0].repeat_length");

  CHECK(strcmp(mod.samples[1].name, "st-01:Snare5") == 0, "sample[1].name");
  CHECK(mod.samples[1].length == 4000, "sample[1].length");

  CHECK(strcmp(mod.samples[6].name, "st-06:SQ-ROCK1") == 0, "sample[6].name");
  CHECK(mod.samples[6].length == 27464, "sample[6].length");

  // Pattern cell spot-checks -- (sample, period, effect, param).
  ModCell c;
  c = mod.patterns[0].cells[0][0];
  CHECK(c.sample == 7 && c.period == 214 && c.effect == 15 && c.param == 3, "pattern0 row0 ch0");
  c = mod.patterns[0].cells[0][1];
  CHECK(c.sample == 7 && c.period == 428 && c.effect == 0 && c.param == 0, "pattern0 row0 ch1");
  c = mod.patterns[0].cells[1][0];
  CHECK(c.sample == 0 && c.period == 0 && c.effect == 0 && c.param == 0, "pattern0 row1 ch0 (empty cell)");
  c = mod.patterns[43].cells[63][3];
  CHECK(c.sample == 0 && c.period == 0 && c.effect == 0 && c.param == 0, "last pattern last row last channel (empty)");

  // Sample PCM data spot-check -- sample[0]'s first 8 bytes and last byte,
  // as signed 8-bit PCM.
  CHECK(mod.samples[0].data != NULL, "sample[0].data non-NULL");
  if (mod.samples[0].data) {
    int8_t want_first8[8] = {0, 0, 0, 0, 81, -71, 1, 1};
    for (int i = 0; i < 8; i++) {
      char label[32]; snprintf(label, sizeof(label), "sample[0].data[%d]", i);
      CHECK(mod.samples[0].data[i] == want_first8[i], label);
    }
    CHECK(mod.samples[0].data[2629] == -3, "sample[0].data[last]");
  }

  mod_free(&mod);
  vfs_free_zip(&zip);

  if (failures == 0) { printf("all tests passed\n"); return 0; }
  fprintf(stderr, "%d failure(s)\n", failures); return 1;
}
