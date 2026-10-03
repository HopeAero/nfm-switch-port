// See vfont.h for scope/design.
#include "vfont.h"
#include "gfx.h"
#include <ctype.h>
#include <string.h>

// Each glyph is a flat list of (x,y) int8_t pairs on the 5x7 grid
// (VFONT_CELL_W x VFONT_CELL_H). {-1,-1} lifts the pen and starts a new
// stroke (the points before/after it are NOT connected to each other).
// A glyph with no points at all (space, and the "undefined" fallback)
// draws nothing.
#define PU -1, -1 // "pen up"

static const int8_t G_SPACE[] = {0};
// Digits use a classic 7-segment layout on the 5x7 grid:
//   top:    (0,0)-(4,0)      topL: (0,0)-(0,3)   topR: (4,0)-(4,3)
//   mid:    (0,3)-(4,3)      botL: (0,3)-(0,6)   botR: (4,3)-(4,6)
//   bottom: (0,6)-(4,6)
// Encoded per-digit below as whichever of those 7 segments it lights,
// each as its own 2-point pen-down/pen-up stroke (simplest to author
// and verify against a real 7-segment reference, at the cost of a few
// redundant PU markers -- this runs once per glyph per frame, not hot).
#define SEG_TOP 0, 0, 4, 0
#define SEG_TOPL 0, 0, 0, 3
#define SEG_TOPR 4, 0, 4, 3
#define SEG_MID 0, 3, 4, 3
#define SEG_BOTL 0, 3, 0, 6
#define SEG_BOTR 4, 3, 4, 6
#define SEG_BOT 0, 6, 4, 6

static const int8_t G_DIGIT_0[] = {SEG_TOP, PU, SEG_TOPL, PU, SEG_TOPR, PU, SEG_BOTL, PU, SEG_BOTR, PU, SEG_BOT};
static const int8_t G_DIGIT_1[] = {SEG_TOPR, PU, SEG_BOTR};
static const int8_t G_DIGIT_2[] = {SEG_TOP, PU, SEG_TOPR, PU, SEG_MID, PU, SEG_BOTL, PU, SEG_BOT};
static const int8_t G_DIGIT_3[] = {SEG_TOP, PU, SEG_TOPR, PU, SEG_MID, PU, SEG_BOTR, PU, SEG_BOT};
static const int8_t G_DIGIT_4[] = {SEG_TOPL, PU, SEG_TOPR, PU, SEG_MID, PU, SEG_BOTR};
static const int8_t G_DIGIT_5[] = {SEG_TOP, PU, SEG_TOPL, PU, SEG_MID, PU, SEG_BOTR, PU, SEG_BOT};
static const int8_t G_DIGIT_6[] = {SEG_TOP, PU, SEG_TOPL, PU, SEG_MID, PU, SEG_BOTL, PU, SEG_BOTR, PU, SEG_BOT};
static const int8_t G_DIGIT_7[] = {SEG_TOP, PU, SEG_TOPR};
static const int8_t G_DIGIT_8[] = {SEG_TOP, PU, SEG_TOPL, PU, SEG_TOPR, PU, SEG_MID, PU, SEG_BOTL, PU, SEG_BOTR, PU, SEG_BOT};
static const int8_t G_DIGIT_9[] = {SEG_TOP, PU, SEG_TOPL, PU, SEG_TOPR, PU, SEG_MID, PU, SEG_BOTR, PU, SEG_BOT};

static const int8_t G_A[] = {0, 6, 0, 2, 2, 0, 4, 2, 4, 6, PU, 0, 3, 4, 3};
static const int8_t G_B[] = {0, 0, 0, 6, PU, 0, 0, 3, 0, 4, 1, 3, 3, 0, 3, PU, 0, 3, 3, 3, 4, 4, 3, 6, 0, 6};
static const int8_t G_C[] = {4, 1, 3, 0, 1, 0, 0, 1, 0, 5, 1, 6, 3, 6, 4, 5};
static const int8_t G_D[] = {0, 0, 0, 6, PU, 0, 0, 2, 0, 4, 2, 4, 4, 2, 6, 0, 6};
static const int8_t G_E[] = {4, 0, 0, 0, 0, 6, 4, 6, PU, 0, 3, 3, 3};
static const int8_t G_F[] = {4, 0, 0, 0, 0, 6, PU, 0, 3, 3, 3};
static const int8_t G_G[] = {4, 1, 3, 0, 1, 0, 0, 1, 0, 5, 1, 6, 3, 6, 4, 5, 4, 3, 2, 3};
static const int8_t G_H[] = {0, 0, 0, 6, PU, 4, 0, 4, 6, PU, 0, 3, 4, 3};
static const int8_t G_I[] = {1, 0, 3, 0, PU, 2, 0, 2, 6, PU, 1, 6, 3, 6};
static const int8_t G_J[] = {3, 0, 3, 5, 2, 6, 1, 6, 0, 5};
static const int8_t G_K[] = {0, 0, 0, 6, PU, 4, 0, 0, 3, 4, 6};
static const int8_t G_L[] = {0, 0, 0, 6, 4, 6};
static const int8_t G_M[] = {0, 6, 0, 0, 2, 3, 4, 0, 4, 6};
static const int8_t G_N[] = {0, 6, 0, 0, 4, 6, 4, 0};
static const int8_t G_O[] = {0, 1, 1, 0, 3, 0, 4, 1, 4, 5, 3, 6, 1, 6, 0, 5, 0, 1};
static const int8_t G_P[] = {0, 6, 0, 0, 3, 0, 4, 1, 3, 3, 0, 3};
static const int8_t G_Q[] = {0, 1, 1, 0, 3, 0, 4, 1, 4, 5, 3, 6, 1, 6, 0, 5, 0, 1, PU, 2, 4, 4, 6};
static const int8_t G_R[] = {0, 6, 0, 0, 3, 0, 4, 1, 3, 3, 0, 3, PU, 2, 3, 4, 6};
static const int8_t G_S[] = {4, 1, 3, 0, 1, 0, 0, 1, 0, 2, 1, 3, 3, 3, 4, 4, 4, 5, 3, 6, 1, 6, 0, 5};
static const int8_t G_T[] = {0, 0, 4, 0, PU, 2, 0, 2, 6};
static const int8_t G_U[] = {0, 0, 0, 5, 1, 6, 3, 6, 4, 5, 4, 0};
static const int8_t G_V[] = {0, 0, 2, 6, 4, 0};
static const int8_t G_W[] = {0, 0, 1, 6, 2, 3, 3, 6, 4, 0};
static const int8_t G_X[] = {0, 0, 4, 6, PU, 4, 0, 0, 6};
static const int8_t G_Y[] = {0, 0, 2, 3, 4, 0, PU, 2, 3, 2, 6};
static const int8_t G_Z[] = {0, 0, 4, 0, 0, 6, 4, 6};

static const int8_t G_DOT[] = {1, 6, 2, 6};
static const int8_t G_COMMA[] = {2, 6, 1, 7};
static const int8_t G_COLON[] = {1, 2, 2, 2, PU, 1, 5, 2, 5};
static const int8_t G_DASH[] = {0, 3, 4, 3};
static const int8_t G_SLASH[] = {0, 6, 4, 0};
static const int8_t G_PERCENT[] = {0, 6, 4, 0, PU, 0, 0, 1, 1, PU, 3, 5, 4, 6};
static const int8_t G_BANG[] = {2, 0, 2, 4, PU, 1, 6, 2, 6};
static const int8_t G_APOS[] = {2, 0, 2, 2};
// Parentheses -- added once the Instructions screen (game.c's
// draw_instructions) started drawing real body text that uses them; they
// were blank fallbacks before, which silently ate the brackets in every
// menu string containing a parenthetical. Same 5x7 grid, drawn as a
// shallow arc rather than a straight bar so they read as brackets and
// not as stray vertical strokes next to the letters.
static const int8_t G_LPAREN[] = {3, 0, 1, 2, 1, 4, 3, 6};
static const int8_t G_RPAREN[] = {1, 0, 3, 2, 3, 4, 1, 6};
static const int8_t G_PLUS[] = {0, 3, 4, 3, PU, 2, 1, 2, 5};

typedef struct {
  const int8_t *pts;
  int32_t len; // number of (x,y) pairs
} Glyph;

#define GLYPH(arr) {arr, (int32_t)(sizeof(arr) / sizeof((arr)[0]) / 2)}

// Indexed by ch - ' ' (0x20), covering the printable ASCII range up to
// 'Z' (0x5A) -- everything past that (lowercase, braces, ...) isn't in
// this table and falls through to vfont_draw_string's own "undefined ->
// blank" fallback.
static const Glyph GLYPHS[] = {
    GLYPH(G_SPACE),  // ' ' 0x20
    GLYPH(G_BANG),   // '!' 0x21
    GLYPH(G_SPACE),  // '"' 0x22 (undefined -- blank)
    GLYPH(G_SPACE),  // '#' 0x23
    GLYPH(G_SPACE),  // '$' 0x24
    GLYPH(G_PERCENT),// '%' 0x25
    GLYPH(G_SPACE),  // '&' 0x26
    GLYPH(G_APOS),   // '\'' 0x27
    GLYPH(G_LPAREN), // '(' 0x28
    GLYPH(G_RPAREN), // ')' 0x29
    GLYPH(G_SPACE),  // '*' 0x2A
    GLYPH(G_PLUS),   // '+' 0x2B
    GLYPH(G_COMMA),  // ',' 0x2C
    GLYPH(G_DASH),   // '-' 0x2D
    GLYPH(G_DOT),    // '.' 0x2E
    GLYPH(G_SLASH),  // '/' 0x2F
    GLYPH(G_DIGIT_0), GLYPH(G_DIGIT_1), GLYPH(G_DIGIT_2), GLYPH(G_DIGIT_3), GLYPH(G_DIGIT_4),
    GLYPH(G_DIGIT_5), GLYPH(G_DIGIT_6), GLYPH(G_DIGIT_7), GLYPH(G_DIGIT_8), GLYPH(G_DIGIT_9),
    GLYPH(G_COLON),  // ':' 0x3A
    GLYPH(G_SPACE),  // ';' 0x3B
    GLYPH(G_SPACE),  // '<' 0x3C
    GLYPH(G_DASH),   // '=' 0x3D (reuse dash -- close enough for a HUD)
    GLYPH(G_SPACE),  // '>' 0x3E
    GLYPH(G_SPACE),  // '?' 0x3F
    GLYPH(G_SPACE),  // '@' 0x40
    GLYPH(G_A), GLYPH(G_B), GLYPH(G_C), GLYPH(G_D), GLYPH(G_E), GLYPH(G_F), GLYPH(G_G),
    GLYPH(G_H), GLYPH(G_I), GLYPH(G_J), GLYPH(G_K), GLYPH(G_L), GLYPH(G_M), GLYPH(G_N),
    GLYPH(G_O), GLYPH(G_P), GLYPH(G_Q), GLYPH(G_R), GLYPH(G_S), GLYPH(G_T), GLYPH(G_U),
    GLYPH(G_V), GLYPH(G_W), GLYPH(G_X), GLYPH(G_Y), GLYPH(G_Z),
};
#define GLYPHS_COUNT (int32_t)(sizeof(GLYPHS) / sizeof(GLYPHS[0]))

static const Glyph *glyph_for(char c) {
  char up = (char)toupper((unsigned char)c);
  int32_t idx = (int32_t)up - 0x20;
  if (idx < 0 || idx >= GLYPHS_COUNT) return &GLYPHS[0]; // blank fallback
  return &GLYPHS[idx];
}

static void draw_glyph(Graphics2D *g, const Glyph *gl, int32_t ox, int32_t oy, int32_t scale, float lw) {
  g->lineWidth = lw;
  bool pen_down = false;
  int32_t px = 0, py = 0;
  for (int32_t i = 0; i < gl->len; i++) {
    int32_t gx = gl->pts[i * 2];
    int32_t gy = gl->pts[i * 2 + 1];
    if (gx == -1 && gy == -1) {
      pen_down = false;
      continue;
    }
    int32_t x = ox + gx * scale;
    int32_t y = oy + gy * scale;
    if (pen_down) gfx_draw_line(g, px, py, x, y);
    px = x;
    py = y;
    pen_down = true;
  }
}

void vfont_draw_string(Graphics2D *g, const char *s, int32_t x, int32_t y, int32_t scale, float line_width) {
  int32_t cursor = x;
  for (const char *p = s; *p; p++) {
    if (*p == '\n') {
      cursor = x;
      y += VFONT_CELL_H * scale + scale;
      continue;
    }
    const Glyph *gl = glyph_for(*p);
    draw_glyph(g, gl, cursor, y, scale, line_width);
    cursor += VFONT_ADVANCE * scale;
  }
}

int32_t vfont_text_width(const char *s, int32_t scale) {
  int32_t n = (int32_t)strlen(s);
  if (n == 0) return 0;
  return n * VFONT_ADVANCE * scale - scale; // no trailing inter-char gap
}
