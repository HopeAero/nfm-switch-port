// Host-buildable test for native/core/png_decode.c against the REAL
// asset files in data/images.zip. Same approach as gif_decode_test.c:
// no JS oracle exists (fresh decoder for an external file format, not
// game logic), so Python's PIL stands in as an independent reference --
// expected dimensions/pixel samples captured by running
// `Image.open(...).convert('RGBA')` over the actual files and sampling
// a handful of (x,y) points per file (corners, center, off-center).
#include <stdio.h>
#include <string.h>
#include "../core/png_decode.h"
#include "../core/vfs.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
  if (!(cond)) { fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); failures++; } \
} while (0)

typedef struct { int32_t x, y; uint8_t r, g, b, a; } Sample;

static void check_pixel(const PngImage *img, Sample s, const char *label) {
  int32_t idx = (s.y * img->width + s.x) * 4;
  bool ok = img->rgba[idx + 0] == s.r && img->rgba[idx + 1] == s.g &&
            img->rgba[idx + 2] == s.b && img->rgba[idx + 3] == s.a;
  char msg[128];
  snprintf(msg, sizeof(msg), "%s pixel (%d,%d)", label, s.x, s.y);
  CHECK(ok, msg);
}

static void check_png(VfsZip *zip, const char *name, int32_t want_w, int32_t want_h,
                       const Sample *samples, int32_t n_samples) {
  VfsZipEntry *entry = NULL;
  for (int32_t i = 0; i < zip->count; i++) {
    if (strcmp(zip->entries[i].name, name) == 0) { entry = &zip->entries[i]; break; }
  }
  if (!entry) { CHECK(false, name); return; }

  PngImage img;
  bool ok = png_decode(entry->data, (size_t)entry->len, &img);
  char msg[128];
  snprintf(msg, sizeof(msg), "%s decodes", name);
  CHECK(ok, msg);
  if (!ok) return;

  snprintf(msg, sizeof(msg), "%s dimensions", name);
  CHECK(img.width == want_w && img.height == want_h, msg);

  for (int32_t i = 0; i < n_samples; i++) check_pixel(&img, samples[i], name);

  png_free(&img);
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

  // br.png: the largest real asset (670x400) -- proof the inflate +
  // per-scanline unfilter path works at real scale, not just small files.
  Sample br[] = {{0,0,0,0,0,255},{669,0,0,0,0,255},{0,399,0,0,0,255},
                  {669,399,0,0,0,255},{335,200,255,255,255,0},{223,100,255,255,255,0}};
  check_png(&zip, "br.png", 670, 400, br, 6);

  Sample byrd[] = {{0,0,0,0,0,255},{193,0,0,0,0,0},{0,9,0,0,0,0},
                    {193,9,0,0,0,0},{97,5,0,0,0,211},{64,2,0,0,0,0}};
  check_png(&zip, "byrd.png", 194, 10, byrd, 6);

  // d1/d2/d3.png: the countdown "dude" face frames -- real alpha
  // gradients (partial transparency), not just fully opaque/transparent.
  Sample d1[] = {{0,0,0,0,0,0},{119,0,0,0,0,0},{0,161,0,0,0,0},
                  {119,161,0,0,0,0},{60,81,255,202,156,255},{40,40,255,189,146,255}};
  check_png(&zip, "d1.png", 120, 162, d1, 6);

  Sample d2[] = {{0,0,0,0,0,0},{119,0,0,0,0,0},{0,161,0,0,0,0},
                  {119,161,0,0,0,0},{60,81,255,202,156,255},{40,40,255,202,156,255}};
  check_png(&zip, "d2.png", 120, 162, d2, 6);

  Sample d3[] = {{0,0,0,0,0,0},{119,0,0,0,0,0},{0,161,0,0,0,0},
                  {119,161,0,0,0,0},{60,81,255,202,156,255},{40,40,255,200,154,255}};
  check_png(&zip, "d3.png", 120, 162, d3, 6);

  Sample fixhoop[] = {{0,0,0,0,0,0},{124,0,0,0,0,0},{0,114,0,0,0,0},
                       {124,114,0,0,0,0},{62,57,204,216,171,85},{41,28,175,90,0,255}};
  check_png(&zip, "fixhoop.png", 125, 115, fixhoop, 6);

  Sample logocars[] = {{0,0,0,0,0,0},{637,0,0,0,0,0},{0,242,0,0,0,0},
                        {637,242,0,0,0,0},{319,121,0,0,0,0},{212,60,81,83,74,255}};
  check_png(&zip, "logocars.png", 638, 243, logocars, 6);

  Sample logomad[] = {{0,0,0,0,0,0},{366,0,0,0,0,0},{0,40,0,0,0,0},
                       {366,40,0,0,0,0},{183,20,22,0,0,101},{122,10,231,0,0,247}};
  check_png(&zip, "logomad.png", 367, 41, logomad, 6);

  Sample nfmcoms[] = {{0,0,0,0,0,223},{160,0,0,0,0,255},{0,7,0,0,0,0},
                       {160,7,0,0,0,255},{80,4,0,0,0,0},{53,2,0,0,0,255}};
  check_png(&zip, "nfmcoms.png", 161, 8, nfmcoms, 6);

  Sample opback[] = {{0,0,0,0,0,0},{299,0,0,0,0,0},{0,176,0,0,0,0},
                      {299,176,0,0,0,0},{150,88,58,30,8,255},{100,44,58,30,8,255}};
  check_png(&zip, "opback.png", 300, 177, opback, 6);

  Sample options[] = {{0,0,0,0,0,0},{210,0,0,0,0,0},{0,104,0,0,0,0},
                       {210,104,0,0,0,0},{105,52,0,0,0,0},{70,26,0,0,0,0}};
  check_png(&zip, "options.png", 211, 105, options, 6);

  Sample options2[] = {{0,0,0,0,0,0},{106,0,0,0,0,0},{0,99,0,0,0,0},
                        {106,99,0,0,0,0},{53,50,0,0,0,0},{35,25,0,0,0,0}};
  check_png(&zip, "options2.png", 107, 100, options2, 6);

  Sample stunts[] = {{0,0,0,0,0,0},{463,0,0,0,0,0},{0,109,0,0,0,0},
                      {463,109,0,0,0,0},{232,55,0,0,0,0},{154,27,0,0,0,0}};
  check_png(&zip, "stunts.png", 464, 110, stunts, 6);

  vfs_free_zip(&zip);

  if (failures == 0) {
    printf("all tests passed\n");
    return 0;
  }
  fprintf(stderr, "%d failure(s)\n", failures);
  return 1;
}
