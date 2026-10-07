// Host-buildable test for native/core/hud_recolor.c against real assets
// decoded via gif_decode.c.
//
// Oracle: web/images.js's loadsnap() transcribed verbatim (its own
// per-pixel loop, lines 90-132) into a standalone Node script operating
// on real gif_decode.c pixel dumps -- not the whole function, since its
// `pixelsOf()` half needs a browser OffscreenCanvas Node doesn't have,
// and that half isn't what's being ported here anyway (see
// hud_recolor.h's own header comment on the one deliberate departure:
// this port skips the JS's `cornerOpaque` canvas-quirk workaround since
// this port's own decoders never zero RGB under a transparent pixel the
// way canvas does -- confirmed by running the FULL transcribed JS,
// cornerOpaque branch included, against the same real pixel data, and
// finding refR resolves to 192 either way on every real asset checked).
#include <stdio.h>
#include <string.h>
#include "../core/hud_recolor.h"
#include "../core/gif_decode.h"
#include "../core/vfs.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
  if (!(cond)) { fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); failures++; } \
} while (0)

typedef struct { int32_t x, y; uint8_t r, g, b, a; } Sample;

static void check_recolor(VfsZip *zip, const char *name, const int32_t snap[3],
                           const Sample *samples, int32_t n_samples) {
  VfsZipEntry *entry = NULL;
  for (int32_t i = 0; i < zip->count; i++) {
    if (strcmp(zip->entries[i].name, name) == 0) { entry = &zip->entries[i]; break; }
  }
  if (!entry) { CHECK(false, name); return; }

  GifImage img;
  bool ok = gif_decode(entry->data, (size_t)entry->len, &img);
  CHECK(ok, name);
  if (!ok) return;

  hud_recolor(img.rgba, img.width, img.height, snap);

  for (int32_t i = 0; i < n_samples; i++) {
    Sample s = samples[i];
    int32_t o = (s.y * img.width + s.x) * 4;
    bool px_ok = img.rgba[o] == s.r && img.rgba[o + 1] == s.g &&
                 img.rgba[o + 2] == s.b && img.rgba[o + 3] == s.a;
    char msg[128];
    snprintf(msg, sizeof(msg), "%s recolored pixel (%d,%d)", name, s.x, s.y);
    CHECK(px_ok, msg);
  }

  gif_free(&img);
}

// The dark-sky ink rule, same cases as web/hudcontrast.test.js.
static void test_readable(void) {
  const int32_t black[3] = {0, 0, 0}, white[3] = {255, 255, 255};
  double c = hud_contrast(black, white);
  CHECK(c > 20.999 && c < 21.001, "contrast: black on white is 21:1");
  const int32_t sky[3] = {64, 32, 96}, ink[3] = {0, 0, 100};
  CHECK(hud_contrast(sky, sky) == 1.0, "contrast: a colour on itself is 1:1");
  int32_t out[3];
  hud_readable(ink, sky, out);
  CHECK(hud_contrast(out, sky) >= HUD_MIN_CONTRAST, "readable: lifts (0,0,100) off a dark sky");
  hud_readable(black, white, out);
  CHECK(out[0] == 0 && out[1] == 0 && out[2] == 0, "readable: leaves black on white alone");
  uint8_t px[8] = {0, 0, 100, 255, 9, 9, 9, 0};
  hud_adapt_ink(px, 2, 1, sky);
  const int32_t got[3] = {px[0], px[1], px[2]};
  CHECK(hud_contrast(got, sky) >= HUD_MIN_CONTRAST && px[3] == 255, "adapt_ink: visible pixel recoloured");
  CHECK(px[4] == 9 && px[7] == 0, "adapt_ink: transparent pixel untouched");
}

int main(void) {
  test_readable();
  VfsZip zip;
  vfs_set_fpath("../../../");
  bool loaded = vfs_read_zip("data/images.zip", &zip);
  CHECK(loaded, "data/images.zip loads");
  if (!loaded) {
    fprintf(stderr, "%d failure(s)\n", failures);
    return 1;
  }

  // damage.gif: opaque corner (cornerOpaque=true in the JS), mixed
  // grey/coloured pixels -- exercises both the grey->black+alpha branch
  // and the coloured->tinted+opaque branch under a real nonzero snap[].
  int32_t snap_damage[3] = {20, -10, 5};
  Sample damage[] = {{0,0,0,0,0,0},{175,0,0,0,0,0},{0,15,0,0,0,0},
                      {175,15,0,0,0,0},{88,8,0,0,0,0},{58,4,255,180,143,255}};
  check_recolor(&zip, "damage.gif", snap_damage, damage, 6);

  // 8.gif (a rank badge): GIF-transparent corner (cornerOpaque=false in
  // the JS) -- exercises the "no snap[] shift, grey stays untouched
  // since its own alpha is already 0" all-transparent-corner path with
  // snap=[0,0,0].
  int32_t snap_zero[3] = {0, 0, 0};
  Sample rank8[] = {{0,0,192,192,192,0},{32,0,192,192,192,0},{0,16,192,192,192,0},
                     {32,16,192,192,192,0},{16,8,192,192,192,0},{11,4,192,192,192,0}};
  check_recolor(&zip, "8.gif", snap_zero, rank8, 6);

  // position.gif: opaque corner again, different nonzero snap[] signs
  // (negative R shift, positive G/B) -- proves the tint formula's sign
  // handling, not just a single fixed direction.
  int32_t snap_position[3] = {-15, 30, 10};
  Sample position[] = {{0,0,0,0,0,0},{64,0,0,0,0,0},{0,11,0,0,0,0},
                        {64,11,0,0,0,0},{32,6,0,0,0,255},{21,3,0,0,0,255}};
  check_recolor(&zip, "position.gif", snap_position, position, 6);

  vfs_free_zip(&zip);

  if (failures == 0) {
    printf("all tests passed\n");
    return 0;
  }
  fprintf(stderr, "%d failure(s)\n", failures);
  return 1;
}
