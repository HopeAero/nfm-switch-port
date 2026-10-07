#include "font.h"
#include "gfx.h"

#include <stddef.h>

static int32_t font_tex = -1;
static int32_t cur_style = FONT_BOLD, cur_size = 13;

static FontIconFn icon_fn;

void font_set_texture(int32_t tex) { font_tex = tex; }
void font_set_icon_fn(FontIconFn fn) { icon_fn = fn; }

/** An inline icon at s (FONT_ICON + its byte): its advance, drawn if g. */
static int32_t icon(Graphics2D *g, const char *s, int32_t x, int32_t y) {
  if (!icon_fn) return 0;
  const int32_t style = cur_style, size = cur_size;
  const int32_t adv = icon_fn(g, (unsigned char)s[1] - 'a', x, y, size);
  cur_style = style;
  cur_size = size;
  return adv;
}

void font_set(int32_t style, int32_t size) {
  cur_style = style ? FONT_BOLD : FONT_PLAIN;
  cur_size = size;
}

// Hinted advance, from the baked table. A size the original never uses has
// no table: scale the 13 px one (ponytail: add it to SIZES in bake_font.py
// if a port screen ever needs exact metrics at another size).
static int32_t advance(int32_t c) {
  if (c < 32 || c > 126) c = '?';
  const int32_t i = c - 32;
  for (int32_t k = 0; k < FONT_SIZE_COUNT; k++)
    if (FONT_SIZES[k] == cur_size) return FONT_ADVANCE[cur_style][k][i];
  return (FONT_ADVANCE[cur_style][3][i] * cur_size + 6) / 13;
}

int32_t font_width(const char *s) {
  int32_t w = 0;
  for (; *s; s++) {
    if (*s == FONT_ICON && s[1]) w += icon(NULL, s++, 0, 0);
    else w += advance((unsigned char)*s);
  }
  return w;
}

void font_draw(Graphics2D *g, const char *s, int32_t x, int32_t y) {
  if (font_tex < 0) return;
  const float k = (float)cur_size / (float)FONT_REF;
  const float iw = 1.0f / (float)FONT_ATLAS_W, ih = 1.0f / (float)FONT_ATLAS_H;
  int32_t pen = x;
  for (; *s; s++) {
    if (*s == FONT_ICON && s[1]) {
      pen += icon(g, s++, pen, y);
      continue;
    }
    int32_t c = (unsigned char)*s;
    if (c < 32 || c > 126) c = '?';
    const FontGlyph *gl = &FONT_GLYPHS[cur_style][c - 32];
    if (gl->w > 0)
      gfx_draw_glyph(g, font_tex, (float)pen + gl->left * k, (float)y + gl->top * k,
                     gl->w * k, gl->h * k, gl->x * iw, gl->y * ih,
                     (gl->x + gl->w) * iw, (gl->y + gl->h) * ih);
    pen += advance(c);
  }
}
