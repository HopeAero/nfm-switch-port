#include "font.h"
#include "gfx.h"
#include "i18n.h"

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

/** The glyph index of the UTF-8 character at *s, advancing *s past it. A
 * character the atlas lacks, or a malformed sequence, is '?'. */
static int32_t next_glyph(const char **s) {
  const unsigned char *p = (const unsigned char *)*s;
  int32_t c = *p++;
  if (c >= 0x80) {
    int32_t n = c >= 0xf0 ? 3 : c >= 0xe0 ? 2 : c >= 0xc0 ? 1 : 0;
    c = n ? c & (0x3f >> n) : -1;
    for (; n > 0 && (*p & 0xc0) == 0x80; n--) c = c << 6 | (*p++ & 0x3f);
    if (n) c = -1;
    *s = (const char *)p;
    int32_t lo = 0, hi = FONT_EXTRA_COUNT - 1;
    while (lo <= hi) {
      const int32_t mid = (lo + hi) / 2;
      if (FONT_EXTRA_CP[mid] == c) return 95 + mid;
      if (FONT_EXTRA_CP[mid] < c) lo = mid + 1;
      else hi = mid - 1;
    }
    return '?' - 32;
  }
  *s = (const char *)p;
  return c < 32 || c > 126 ? '?' - 32 : c - 32;
}

// Hinted advance of glyph i, from the baked table. A size the original never
// uses has no table: scale the 13 px one (ponytail: add it to SIZES in
// bake_font.py if a port screen ever needs exact metrics at another size).
static int32_t advance(int32_t i) {
  for (int32_t k = 0; k < FONT_SIZE_COUNT; k++)
    if (FONT_SIZES[k] == cur_size) return FONT_ADVANCE[cur_style][k][i];
  return (FONT_ADVANCE[cur_style][3][i] * cur_size + 6) / 13;
}

int32_t font_width(const char *s) {
  s = tr(s);
  int32_t w = 0;
  while (*s) {
    if (*s == FONT_ICON && s[1]) {
      w += icon(NULL, s, 0, 0);
      s += 2;
    } else {
      w += advance(next_glyph(&s));
    }
  }
  return w;
}

void font_draw(Graphics2D *g, const char *s, int32_t x, int32_t y) {
  if (font_tex < 0) return;
  const float k = (float)cur_size / (float)FONT_REF;
  const float iw = 1.0f / (float)FONT_ATLAS_W, ih = 1.0f / (float)FONT_ATLAS_H;
  int32_t pen = x;
  s = tr(s);
  while (*s) {
    if (*s == FONT_ICON && s[1]) {
      pen += icon(g, s, pen, y);
      s += 2;
      continue;
    }
    const int32_t c = next_glyph(&s);
    const FontGlyph *gl = &FONT_GLYPHS[cur_style][c];
    if (gl->w > 0)
      gfx_draw_glyph(g, font_tex, (float)pen + gl->left * k, (float)y + gl->top * k,
                     gl->w * k, gl->h * k, gl->x * iw, gl->y * ih,
                     (gl->x + gl->w) * iw, (gl->y + gl->h) * ih);
    pen += advance(c);
  }
}
