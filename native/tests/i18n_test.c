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
  // The port's own dictionary (i18n_port_es.c): an entry, a format, a pattern
  // with a translated capture ($t1), and its precedence over the web's order.
  CHECK_TR("Reset RPG Mode", "Reiniciar modo RPG");
  CHECK_TR("Drive your car using the %s and %s", "Maneja tu auto con %s y %s");
  CHECK_TR("Reshuffles: 2   Transfers: 1", "Mezclas: 2   Pases: 1");
  CHECK_TR("Press Enter to continue", "Presiona Enter para continuar");
  CHECK_TR("< All Cars >", "< Todos los autos >");
  CHECK_TR("Level 12    XP 30 / 400", "Nivel 12    XP 30 / 400");
  CHECK_TR("Hamer has wasted all the cars!", "\xc2\xa1Hamer destruy\xc3\xb3 todos los autos!");
  // The web's function patterns, as code: stunt calls (Extended's and the
  // base game's), the arrow, counted nouns, car classes.
  CHECK_TR("Breathtaking forward loop by 360!", "\xc2\xa1" "Asombrosa giro adelante por 360!");
  CHECK_TR("Wicked Forward loop with Rollspin by 180 and beyond!!",
           "\xc2\xa1" "Brutal Giro adelante con Giro de lado por 180 y m\xc3\xa1s all\xc3\xa1!!");
  CHECK_TR("Nice job!", "Nice job!");   // not the stunt grammar
  CHECK_TR("Arrow now pointing at > CARS", "La flecha apunta a los autos");
  CHECK_TR("1 car", "1 auto");
  CHECK_TR("12 cars", "12 autos");
  CHECK_TR("Class A & B", "Clase A & B");
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
