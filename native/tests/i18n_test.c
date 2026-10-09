// Host-buildable test for native/core/i18n.c (tr) and the UTF-8 half of
// font.c: the web port's lookup order, and that the Spanish glyphs measure.
#include <stdio.h>
#include <string.h>
#include "../core/font.h"
#include "../core/i18n.h"

static int failures = 0;
#define CHECK(cond, msg) do { \
  if (!(cond)) { fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); failures++; } \
} while (0)
#define CHECK_TR(en, es) CHECK(strcmp(tr(en), es) == 0, en)

int main(void) {
  // English: the string itself, not a copy.
  const char *s = "Settings";
  CHECK(tr(s) == s, "English passes through");

  i18n_set_lang(I18N_LANG_ES);
  CHECK_TR("Settings", "Ajustes");                       // exact
  CHECK_TR("  Settings ", "  Ajustes ");                 // trimmed, whitespace kept
  CHECK_TR("Press  [ Enter ]  to continue", "Presiona  [ Enter ]  para continuar");
  CHECK_TR("Stage 7 Completed!", "\xc2\xa1Pista 7 completada!");   // a pattern
  CHECK_TR("Hamer has wasted you!", "\xc2\xa1Hamer te destruy\xc3\xb3!");
  CHECK_TR("Hamer has wasted Nimi!", "\xc2\xa1Hamer destruy\xc3\xb3 a Nimi!");   // past a (?!...)
  CHECK_TR("1st", "1\xc2\xba");                          // an alternative, expanded
  CHECK_TR("STAT POINTS:  -12", "PUNTOS DE STATS:  -12");
  CHECK_TR("Settings\nMenu", "Ajustes\nMen\xc3\xba");     // line by line
  s = "Radical One";
  CHECK(tr(s) == s, "a car name passes through");
  CHECK(tr(s) == s, "...and again, from the miss cache");

  // UTF-8: an accented letter is one glyph, as wide as its base letter here;
  // an unknown character measures as '?'.
  font_set(FONT_BOLD, 13);
  CHECK(font_width("\xc3\xa1") > 0, "UTF-8 width");
  CHECK(font_width("\xc3\xa1") == font_width("a"), "a-acute as wide as a");
  CHECK(font_width("\xe2\x96\xb8") == font_width("?"), "no glyph: '?'");
  // font_width translates first.
  CHECK(font_width("Settings") == font_width("Ajustes"), "measured in Spanish");

  i18n_set_lang(I18N_LANG_EN);
  s = "Settings";
  CHECK(tr(s) == s, "back to English");

  if (failures == 0) printf("i18n_test: OK\n");
  return failures == 0 ? 0 : 1;
}
