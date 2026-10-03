// Host-buildable test for native/core/gif_decode.c against the REAL
// asset files in data/images.zip.
//
// No JS oracle exists for this one (see gif_decode.h's own scope
// comment -- this isn't a port of game logic, it's a fresh decoder for a
// well-defined external file format). Instead, Python's PIL (Pillow) --
// an independent, widely-trusted GIF decoder -- serves the same role:
// expected dimensions/pixel samples below were captured by running
// `Image.open(...).convert('RGBA')` over the actual files in
// data/images.zip and reading back a handful of sampled (x,y) pixels
// per file (corners, center, and one off-center point), not the full
// pixel grid -- same "pin a representative sample, not everything"
// approach as this session's other geometry-heavy tests (e.g.
// plane_test.c's vertex checks).
#include <stdio.h>
#include <string.h>
#include "../core/gif_decode.h"
#include "../core/vfs.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
  if (!(cond)) { fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); failures++; } \
} while (0)

typedef struct { int32_t x, y; uint8_t r, g, b, a; } Sample;

static void check_pixel(const GifImage *img, Sample s, const char *label) {
  int32_t idx = (s.y * img->width + s.x) * 4;
  bool ok = img->rgba[idx + 0] == s.r && img->rgba[idx + 1] == s.g &&
            img->rgba[idx + 2] == s.b && img->rgba[idx + 3] == s.a;
  char msg[128];
  snprintf(msg, sizeof(msg), "%s pixel (%d,%d)", label, s.x, s.y);
  CHECK(ok, msg);
}

static void check_gif(VfsZip *zip, const char *name, int32_t want_w, int32_t want_h,
                       const Sample *samples, int32_t n_samples) {
  VfsZipEntry *entry = NULL;
  for (int32_t i = 0; i < zip->count; i++) {
    if (strcmp(zip->entries[i].name, name) == 0) { entry = &zip->entries[i]; break; }
  }
  if (!entry) { CHECK(false, name); return; }

  GifImage img;
  bool ok = gif_decode(entry->data, (size_t)entry->len, &img);
  char msg[128];
  snprintf(msg, sizeof(msg), "%s decodes", name);
  CHECK(ok, msg);
  if (!ok) return;

  snprintf(msg, sizeof(msg), "%s dimensions", name);
  CHECK(img.width == want_w && img.height == want_h, msg);

  for (int32_t i = 0; i < n_samples; i++) check_pixel(&img, samples[i], name);

  gif_free(&img);
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

  Sample damage[] = {{0,0,192,192,192,255},{175,0,192,192,192,255},{0,15,192,192,192,255},
                      {175,15,192,192,192,255},{88,8,192,192,192,255},{58,4,223,200,137,255}};
  check_gif(&zip, "damage.gif", 176, 16, damage, 6);

  Sample power[] = {{0,0,192,192,192,255},{175,0,192,192,192,255},{0,15,192,192,192,255},
                     {175,15,192,192,192,255},{88,8,192,192,192,255},{58,4,137,192,223,255}};
  check_gif(&zip, "power.gif", 176, 16, power, 6);

  Sample position[] = {{0,0,192,192,192,255},{64,0,192,192,192,255},{0,11,192,192,192,255},
                        {64,11,192,192,192,255},{32,6,0,0,0,255},{21,3,0,0,0,255}};
  check_gif(&zip, "position.gif", 65, 12, position, 6);

  // speed.gif: no transparency chunk, but includes genuinely grey (non-192)
  // antialiasing pixels -- exercises the plain palette-lookup path with
  // varied grey values, not just the flat 192/0 pixels the others hit.
  Sample speed[] = {{0,0,189,189,189,255},{177,0,189,189,189,255},{0,12,86,86,86,255},
                     {177,12,189,189,189,255},{89,6,70,70,70,255},{59,3,189,189,189,255}};
  check_gif(&zip, "speed.gif", 178, 13, speed, 6);

  Sample wasted[] = {{0,0,192,192,192,255},{52,0,192,192,192,255},{0,11,192,192,192,255},
                      {52,11,192,192,192,255},{26,6,192,192,192,255},{17,3,3,3,3,255}};
  check_gif(&zip, "wasted.gif", 53, 12, wasted, 6);

  Sample lap[] = {{0,0,192,192,192,255},{26,0,192,192,192,255},{0,11,192,192,192,255},
                   {26,11,192,192,192,255},{13,6,75,75,75,255},{9,3,192,192,192,255}};
  check_gif(&zip, "lap.gif", 27, 12, lap, 6);

  Sample rank1[] = {{0,0,192,192,192,255},{32,0,192,192,192,255},{0,16,192,192,192,255},
                     {32,16,192,192,192,255},{16,8,192,192,192,255},{11,4,192,192,192,255}};
  check_gif(&zip, "1.gif", 33, 17, rank1, 6);

  // bob.gif: a real, previously-decoder-breaking regression -- a tiny
  // (1x3-pixel) image whose LZW stream omits the trailing end-code
  // entirely once exactly width*height pixels have been produced.
  // Non-conformant strictly, but real encoders do this and reference
  // decoders (confirmed against Python's PIL) accept it. Found via a
  // full sweep of all 122 real GIFs in data/images.zip (not by this
  // targeted sample list, which was written before that sweep existed).
  Sample bob[] = {{0,0,156,156,156,255},{0,1,140,140,140,255},{0,2,173,173,173,255}};
  check_gif(&zip, "bob.gif", 1, 3, bob, 3);

  // mycl.gif: a real, previously-decoder-breaking regression -- one of
  // 4 real assets (also dome.gif, myfr.gif, roomp.gif) that actually
  // ARE interlaced, despite an image-info dict check that looked
  // authoritative claiming none of the 122 real GIFs were (see
  // gif_decode.h's own note on that dead end). Exercises the 4-pass
  // interlaced row deinterleave.
  Sample mycl[] = {{0,0,190,190,190,0},{80,0,190,190,190,0},{0,15,37,37,37,255},
                    {80,15,190,190,190,0},{40,8,190,190,190,0},{27,4,190,190,190,0}};
  check_gif(&zip, "mycl.gif", 81, 16, mycl, 6);

  // 0c.gif: exercises actual transparency (GCE-declared transparent index
  // -> alpha=0), unlike every sample above (opaque throughout).
  Sample countdown0[] = {{0,0,255,255,255,0},{72,0,255,255,255,0},{0,38,255,255,255,0},
                          {72,38,255,255,255,0},{36,19,255,255,255,0},{24,9,255,255,255,0}};
  check_gif(&zip, "0c.gif", 73, 39, countdown0, 6);

  // cars.gif: the largest real asset (670x400, 45KB compressed) -- proof
  // the LZW decoder's code-size growth (up to the 12-bit ceiling) and
  // dictionary-reset-on-clear-code path both work correctly at real
  // scale, not just on the small ~1KB HUD panel files above.
  Sample cars[] = {{0,0,16,16,16,255},{669,0,16,16,16,255},{0,399,8,8,8,255},
                    {669,399,16,16,16,255},{335,200,115,115,115,255},{223,100,231,231,231,255}};
  check_gif(&zip, "cars.gif", 670, 400, cars, 6);

  vfs_free_zip(&zip);

  if (failures == 0) {
    printf("all tests passed\n");
    return 0;
  }
  fprintf(stderr, "%d failure(s)\n", failures);
  return 1;
}
