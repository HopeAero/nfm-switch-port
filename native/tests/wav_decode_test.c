// Host-buildable test for native/core/wav_decode.c against the REAL
// asset files in data/sounds.zip.
//
// No JS oracle exists for this one (see wav_decode.h's own scope
// comment -- this isn't a port of game logic, it's a fresh decoder for a
// well-defined external file format). Instead, Python's stdlib `wave`
// module -- an independent WAV decoder -- serves the same role:
// expected rate/frame-count/sample values below were captured by running
// `wave.open(...)` over the actual files in data/sounds.zip and reading
// back a handful of sampled frame indices per file (first, second,
// middle, last), same "pin a representative sample, not everything"
// approach as gif_decode_test.c/png_decode_test.c.
#include <stdio.h>
#include <string.h>
#include "../core/wav_decode.h"
#include "../core/vfs.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
  if (!(cond)) { fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); failures++; } \
} while (0)

typedef struct { int32_t idx; int16_t value; } Sample;

static void check_wav(VfsZip *zip, const char *name, int32_t want_rate, int32_t want_frames,
                       const Sample *samples, int32_t n_samples) {
  VfsZipEntry *entry = NULL;
  for (int32_t i = 0; i < zip->count; i++) {
    if (strcmp(zip->entries[i].name, name) == 0) { entry = &zip->entries[i]; break; }
  }
  if (!entry) { fprintf(stderr, "FAIL: %s not found in zip\n", name); failures++; return; }

  WavClip clip;
  bool ok = wav_decode(entry->data, (size_t)entry->len, &clip);
  char label[128];
  snprintf(label, sizeof(label), "%s decode ok", name);
  CHECK(ok, label);
  if (!ok) return;

  snprintf(label, sizeof(label), "%s sample_rate", name);
  CHECK(clip.sample_rate == want_rate, label);
  snprintf(label, sizeof(label), "%s frame_count", name);
  CHECK(clip.frame_count == want_frames, label);

  for (int32_t i = 0; i < n_samples; i++) {
    snprintf(label, sizeof(label), "%s sample[%d]", name, samples[i].idx);
    CHECK(clip.samples[samples[i].idx] == samples[i].value, label);
  }

  wav_free(&clip);
}

int main(void) {
  VfsZip zip;
  vfs_set_fpath("../../../"); // native/tests/build/ -> repo root, see gif_decode_test.c's own use of this
  if (!vfs_read_zip("data/sounds.zip", &zip)) {
    fprintf(stderr, "FAIL: could not open data/sounds.zip\n");
    return 1;
  }

  Sample checkpoint_samples[] = {
    {0, 0}, {1, 0}, {11363, -2304}, {22725, 0},
  };
  check_wav(&zip, "checkpoint.wav", 22050, 22726, checkpoint_samples, 4);

  Sample go_samples[] = {
    {0, -256}, {1, -256}, {11526, 0}, {23052, 0},
  };
  check_wav(&zip, "go.wav", 22050, 23053, go_samples, 4);

  Sample three_samples[] = {
    {0, -256}, {1, -256}, {11451, 0}, {22902, 0},
  };
  check_wav(&zip, "three.wav", 22050, 22903, three_samples, 4);

  // Covers the other two real sample rates in the asset set (11025 and
  // 8000 Hz -- see this file's own header comment on the full set).
  Sample crash1_samples[] = {
    {0, -768}, {1, -1280}, {5812, -512}, {11624, 0},
  };
  check_wav(&zip, "crash1.wav", 11025, 11625, crash1_samples, 4);

  Sample engine00_samples[] = {
    {0, 0}, {1, 0}, {2945, 1536}, {5890, 256},
  };
  check_wav(&zip, "00.wav", 8000, 5891, engine00_samples, 4);

  vfs_free_zip(&zip);

  if (failures == 0) { printf("all tests passed\n"); return 0; }
  fprintf(stderr, "%d failure(s)\n", failures); return 1;
}
