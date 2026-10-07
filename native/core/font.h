// The game's text: Arial (as Liberation Sans, its metric-compatible open
// substitute), antialiased, drawn the way xtGraphics.java draws it --
//
//   rd.setFont(new Font("Arial", 1, 13));   ->  font_set(FONT_BOLD, 13);
//   ftm.stringWidth(s)                      ->  font_width(s)
//   rd.drawString(s, x, y)                  ->  font_draw(g, s, x, y)
//
// so a Java call site ports line for line, same strings, same coordinates
// (`y` is the BASELINE, as in Java). The colour is the Graphics2D's current
// one. Glyphs come from one atlas texture (data/port/font.png, baked with
// font_data.c by tools/bake_font.py) set once with font_set_texture.
#ifndef NFM_FONT_H
#define NFM_FONT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Graphics2D;

enum { FONT_PLAIN = 0, FONT_BOLD = 1 };

#define FONT_GLYPH_COUNT 95 // ' ' .. '~'
#define FONT_SIZE_COUNT 6

typedef struct {
  int16_t x, y, w, h; // atlas rect, at FONT_REF px
  int16_t left, top;  // offset from the pen position on the baseline
} FontGlyph;

extern const int32_t FONT_REF, FONT_ATLAS_W, FONT_ATLAS_H;
extern const int32_t FONT_SIZES[FONT_SIZE_COUNT];
extern const FontGlyph FONT_GLYPHS[2][FONT_GLYPH_COUNT];
extern const uint8_t FONT_ADVANCE[2][FONT_SIZE_COUNT][FONT_GLYPH_COUNT];

void font_set_texture(int32_t tex);
void font_set(int32_t style, int32_t size);
int32_t font_width(const char *s);
void font_draw(struct Graphics2D *g, const char *s, int32_t x, int32_t y);

#ifdef __cplusplus
}
#endif

#endif
