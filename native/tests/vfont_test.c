// Host-buildable test for native/core/vfont.c. Not a JS port (see
// vfont.h's own scope comment -- there's no oracle to compare against),
// so this checks the module's own internal consistency instead: known
// glyphs emit the expected vertex counts (each gfx_draw_line segment is
// 2 triangles = 6 verts, see gfx.c's own segment()), unsupported/space
// characters draw nothing, lowercase folds to the same glyph as
// uppercase, and vfont_text_width's formula matches what
// vfont_draw_string actually advances the cursor by.
#include <stdio.h>
#include <string.h>
#include "../core/vfont.h"
#include "../core/gfx.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
  if (!(cond)) { fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); failures++; } \
} while (0)

static int32_t verts_for(const char *s, int32_t scale) {
  Graphics2D g;
  gfx_init(&g, 800, 450);
  gfx_begin(&g);
  gfx_set_color(&g, 255, 255, 255);
  vfont_draw_string(&g, s, 0, 0, scale, 2.0f);
  int32_t count = g.count;
  gfx_free(&g);
  return count;
}

int main(void) {
  CHECK(verts_for(" ", 1) == 0, "space draws nothing");
  CHECK(verts_for("", 1) == 0, "empty string draws nothing");
  CHECK(verts_for("@", 1) == 0, "unsupported char falls back to blank");

  // '8': all 7 segments lit -> 7 * 6 = 42 verts.
  CHECK(verts_for("8", 1) == 42, "digit 8 (7 segments)");
  // '1': topR + botR -> 2 * 6 = 12 verts.
  CHECK(verts_for("1", 1) == 12, "digit 1 (2 segments)");
  // '7': top + topR -> 2 * 6 = 12 verts.
  CHECK(verts_for("7", 1) == 12, "digit 7 (2 segments)");

  // 'A': stroke1 has 5 points (4 segments), stroke2 has 2 points (1
  // segment) -> 5 segments * 6 = 30 verts.
  CHECK(verts_for("A", 1) == 30, "letter A (5 segments)");
  CHECK(verts_for("a", 1) == verts_for("A", 1), "lowercase folds to uppercase");

  // Two non-space glyphs draw strictly more than one.
  CHECK(verts_for("AB", 1) > verts_for("A", 1), "second glyph adds geometry");

  // Degenerate-dot fix: ':' and '!' must draw something (were silently
  // dropped before -- gfx.c's segment() skips zero-length lines).
  CHECK(verts_for(":", 1) == 12, "colon (2 short segments)");
  CHECK(verts_for("!", 1) == 12, "bang (2 segments)");

  // vfont_text_width matches vfont_draw_string's own cursor advance:
  // n chars * VFONT_ADVANCE * scale, minus one trailing gap.
  CHECK(vfont_text_width("A", 1) == VFONT_CELL_W, "text_width single char");
  CHECK(vfont_text_width("AB", 1) == 2 * VFONT_ADVANCE - 1, "text_width two chars");
  CHECK(vfont_text_width("AB", 3) == 2 * VFONT_ADVANCE * 3 - 3, "text_width scales");
  CHECK(vfont_text_width("", 1) == 0, "text_width empty string");

  if (failures == 0) {
    printf("all tests passed\n");
    return 0;
  }
  fprintf(stderr, "%d failure(s)\n", failures);
  return 1;
}
