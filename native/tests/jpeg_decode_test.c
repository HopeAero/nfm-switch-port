// Host-buildable test for native/core/jpeg_decode.c against the REAL
// asset files in data/images.zip. Same "no JS oracle, use Python's PIL"
// approach as gif_decode_test.c/png_decode_test.c, with one difference:
// JPEG is LOSSY, and this decoder's IDCT/chroma-upsampling/rounding
// choices don't have to bit-match libjpeg's (PIL's own backend) to be a
// CORRECT baseline JPEG decoder -- only close, the same way any two
// independent spec-compliant decoders' outputs are close but rarely
// pixel-identical. Expected samples below use a small per-channel
// tolerance rather than exact equality; a whole-image sweep (throwaway
// script, not checked in) measured mean/max absolute difference across
// all 4 real JPGs against PIL and found mean error under 0.4/255 and
// max error under 16/255 on every one -- the tolerance here is set
// well above that, so it catches a real decode break (wrong colours,
// garbage output, systematic shift) without being sensitive to the
// ordinary rounding variance between two independently-written decoders.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../core/jpeg_decode.h"
#include "../core/vfs.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
  if (!(cond)) { fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); failures++; } \
} while (0)

#define TOLERANCE 24

typedef struct { int32_t x, y; uint8_t r, g, b; } Sample;

static void check_pixel(const JpegImage *img, Sample s, const char *label) {
  int32_t idx = (s.y * img->width + s.x) * 4;
  int32_t dr = abs((int32_t)img->rgba[idx + 0] - s.r);
  int32_t dg = abs((int32_t)img->rgba[idx + 1] - s.g);
  int32_t db = abs((int32_t)img->rgba[idx + 2] - s.b);
  bool ok = dr <= TOLERANCE && dg <= TOLERANCE && db <= TOLERANCE && img->rgba[idx + 3] == 255;
  char msg[160];
  snprintf(msg, sizeof(msg), "%s pixel (%d,%d) got (%d,%d,%d) want ~(%d,%d,%d)", label, s.x, s.y,
           img->rgba[idx + 0], img->rgba[idx + 1], img->rgba[idx + 2], s.r, s.g, s.b);
  CHECK(ok, msg);
}

static void check_jpeg(VfsZip *zip, const char *name, int32_t want_w, int32_t want_h,
                        const Sample *samples, int32_t n_samples) {
  VfsZipEntry *entry = NULL;
  for (int32_t i = 0; i < zip->count; i++) {
    if (strcmp(zip->entries[i].name, name) == 0) { entry = &zip->entries[i]; break; }
  }
  if (!entry) { CHECK(false, name); return; }

  JpegImage img;
  bool ok = jpeg_decode(entry->data, (size_t)entry->len, &img);
  char msg[128];
  snprintf(msg, sizeof(msg), "%s decodes", name);
  CHECK(ok, msg);
  if (!ok) return;

  snprintf(msg, sizeof(msg), "%s dimensions", name);
  CHECK(img.width == want_w && img.height == want_h, msg);

  for (int32_t i = 0; i < n_samples; i++) check_pixel(&img, samples[i], name);

  jpeg_free(&img);
}

int main(void) {
  VfsZip zip;
  vfs_set_fpath("../../../");
  bool loaded = vfs_read_zip("data/images.zip", &zip);
  CHECK(loaded, "data/images.zip loads");
  if (!loaded) {
    fprintf(stderr, "%d failure(s)\n", failures);
    return 1;
  }

  // bggo.jpg: 4:2:0 chroma subsampling, largest asset (800x450).
  Sample bggo[] = {{0,0,154,41,0},{799,0,228,82,9},{0,449,87,67,58},
                    {799,449,64,55,58},{400,225,172,84,44},{266,112,148,49,7}};
  check_jpeg(&zip, "bggo.jpg", 800, 450, bggo, 6);

  // bgmain.jpg: 4:2:0, near-flat grey -- exercises the low-frequency/
  // mostly-DC-coefficient path.
  Sample bgmain[] = {{0,0,191,191,191},{669,0,186,186,186},{0,399,188,188,188},
                      {669,399,189,189,189},{335,200,190,190,190},{223,100,190,190,190}};
  check_jpeg(&zip, "bgmain.jpg", 670, 400, bgmain, 6);

  // logomadbg.jpg: the one real asset using 4:4:4 (NO chroma
  // subsampling, h_samp=v_samp=1 for every component) -- exercises the
  // other sampling-ratio path the other 3 files don't reach.
  Sample logomadbg[] = {{0,0,204,68,8},{669,0,253,127,24},{0,399,74,61,53},
                         {669,399,105,82,68},{335,200,233,122,50},{223,100,159,53,5}};
  check_jpeg(&zip, "logomadbg.jpg", 670, 400, logomadbg, 6);

  Sample track[] = {{0,0,12,12,12},{669,0,35,35,35},{0,399,38,38,38},
                     {669,399,21,21,21},{335,200,11,11,11},{223,100,37,37,37}};
  check_jpeg(&zip, "track.jpg", 670, 400, track, 6);

  vfs_free_zip(&zip);

  if (failures == 0) {
    printf("all tests passed\n");
    return 0;
  }
  fprintf(stderr, "%d failure(s)\n", failures);
  return 1;
}
