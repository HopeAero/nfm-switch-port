// Host-buildable test for native/core/font.c. The widths are FreeType's
// hinted advances of Windows' own arialbd.ttf / arial.ttf at each size --
// what Java's FontMetrics.stringWidth returns for the original's Arial --
// so a centred string lands where the original centres it.
#include <stdio.h>
#include "../core/font.h"
#include "../core/gfx.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
  if (!(cond)) { fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); failures++; } \
} while (0)

static int32_t width_at(int32_t style, int32_t size, const char *s) {
  font_set(style, size);
  return font_width(s);
}

int main(void) {
  CHECK(width_at(FONT_BOLD, 13, "M A I N    C O N T R O L S") == 161, "bold 13");
  CHECK(width_at(FONT_BOLD, 12, "Press  [ Enter ]  to continue") == 154, "bold 12");
  CHECK(width_at(FONT_BOLD, 11, "Accelerate") == 55, "bold 11");
  CHECK(width_at(FONT_PLAIN, 10, "Hue  | ") == 31, "plain 10");
  CHECK(width_at(FONT_BOLD, 22, "GO") == 34, "bold 22");
  CHECK(width_at(FONT_BOLD, 13, "") == 0, "empty");

  // One textured quad per inked glyph, none for spaces; the pen starts at x
  // and the glyph sits on the baseline (y), ink above it.
  Graphics2D g;
  gfx_init(&g, 800, 450);
  gfx_begin(&g);
  gfx_set_color(&g, 10, 20, 30);
  font_set_texture(7);
  font_set(FONT_BOLD, 13);
  font_draw(&g, "A b", 100, 200);
  CHECK(g.cmd_count == 2, "two glyph quads, space skipped");
  CHECK(g.cmds[0].image_id == 7, "atlas texture");
  CHECK(g.cmds[0].x >= 100.0f && g.cmds[0].x < 101.0f, "A starts at the pen");
  CHECK(g.cmds[0].y + g.cmds[0].h <= 200.5f && g.cmds[0].y < 192.0f, "A stands on the baseline");
  CHECK(g.cmds[1].x >= 100.0f + (float)width_at(FONT_BOLD, 13, "A "), "b after A and the space");
  CHECK(g.cmds[0].tr > 0.03f && g.cmds[0].tr < 0.05f, "tinted by the current colour");
  gfx_free(&g);

  if (failures == 0) printf("font_test: OK\n");
  return failures == 0 ? 0 : 1;
}
