// See career_bots.h.
#include "career_bots.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void career_bots_free(CareerBots *b) {
  for (int32_t i = 0; i < NFM_MAX_CARS; i++) free(b->keys[i]);
  memset(b, 0, sizeof(*b));
}

int32_t career_bots_slots(int32_t stage, int32_t nplayers, bool hard, int32_t bonus, int32_t *slots) {
  int32_t n = 0;
  if (stage == 13 && hard) {
    for (int32_t a = nplayers - 6; a < nplayers; a++) slots[n++] = a;
  }
  if (stage == 14 || stage == 18 || stage == 10 || stage == 9 || stage == 5 || stage == 20) slots[n++] = nplayers - 1;
  if ((stage == 11 || stage == 21) && hard && bonus != 2) {
    slots[n++] = 8;
    slots[n++] = 9;
  }
  return n;
}

/** GameSparker.getint(name, line, 0): the first number after "name(". */
static int32_t first_int(const char *line, size_t skip) {
  return (int32_t)strtol(line + skip, NULL, 10);
}

bool career_bots_parse(CareerBots *b, int32_t slot, const char *text) {
  if (slot < 0 || slot >= NFM_MAX_CARS || !text) return false;
  free(b->keys[slot]);
  b->keys[slot] = calloc(CAREER_BOTS_TICKS, 1);
  if (!b->keys[slot]) return false;
  static const struct { const char *name; uint8_t bit; } kKeys[] = {
      {"up(", BOT_UP}, {"down(", BOT_DOWN}, {"left(", BOT_LEFT}, {"right(", BOT_RIGHT}, {"handb(", BOT_HANDB}};
  int32_t offset = 0;
  const char *p = text;
  while (*p) {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    const char *eol = strchr(p, '\n');
    if (strncmp(p, "offset(", 7) == 0) offset = first_int(p, 7);
    for (size_t k = 0; k < sizeof(kKeys) / sizeof(kKeys[0]); k++) {
      const size_t len = strlen(kKeys[k].name);
      if (strncmp(p, kKeys[k].name, len) != 0) continue;
      const int32_t t = first_int(p, len) + offset;
      if (t >= 0 && t < CAREER_BOTS_TICKS) b->keys[slot][t] |= kKeys[k].bit;
    }
    if (!eol) break;
    p = eol + 1;
  }
  return true;
}

uint8_t career_bots_keys(const CareerBots *b, int32_t slot, bool special) {
  if (slot < 0 || slot >= NFM_MAX_CARS || !b->keys[slot] || b->brk[slot]) return 0;
  const int32_t t = special ? b->specialtimer[slot] : b->timer;
  return (t >= 0 && t < CAREER_BOTS_TICKS) ? b->keys[slot][t] : 0;
}
