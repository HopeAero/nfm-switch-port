#include "i18n.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static int32_t lang = I18N_LANG_EN;

// Strings with no translation, by hash: most of what a frame draws (numbers,
// names, the English-only port text) would otherwise run every pattern every
// frame. Direct-mapped; a key too long for a slot is not cached.
#define MISS_SLOTS 64
#define MISS_LEN 64
static char miss[MISS_SLOTS][MISS_LEN];

void i18n_set_lang(int32_t l) {
  lang = l == I18N_LANG_ES ? I18N_LANG_ES : I18N_LANG_EN;
  memset(miss, 0, sizeof(miss));
}

int32_t i18n_lang(void) { return lang; }

static char ring[I18N_RING][I18N_BUF];
static int32_t ring_next;

static bool is_space(char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v'; }
static bool is_digit(char c) { return c >= '0' && c <= '9'; }

// The port's table is hand-written in whatever order reads best; it is
// searched through this index, sorted on first use.
static const I18nEntry **port_index;

static int cmp_entry(const void *a, const void *b) {
  return strcmp((*(const I18nEntry *const *)a)->en, (*(const I18nEntry *const *)b)->en);
}

static const char *lookup_port(const char *s) {
  if (!port_index) {
    port_index = malloc(sizeof(*port_index) * (size_t)I18N_PORT_ES_COUNT);
    if (!port_index) return NULL;
    for (int32_t i = 0; i < I18N_PORT_ES_COUNT; i++) port_index[i] = &I18N_PORT_ES[i];
    qsort(port_index, (size_t)I18N_PORT_ES_COUNT, sizeof(*port_index), cmp_entry);
  }
  int32_t lo = 0, hi = I18N_PORT_ES_COUNT - 1;
  while (lo <= hi) {
    const int32_t mid = (lo + hi) / 2;
    const int c = strcmp(port_index[mid]->en, s);
    if (c == 0) return port_index[mid]->es;
    if (c < 0) lo = mid + 1;
    else hi = mid - 1;
  }
  return NULL;
}

static const char *lookup(const char *s) {
  const char *hit = lookup_port(s);
  if (hit) return hit;
  int32_t lo = 0, hi = I18N_ES_COUNT - 1;
  while (lo <= hi) {
    const int32_t mid = (lo + hi) / 2;
    const int c = strcmp(I18N_ES[mid].en, s);
    if (c == 0) return I18N_ES[mid].es;
    if (c < 0) lo = mid + 1;
    else hi = mid - 1;
  }
  return NULL;
}

// ---- patterns (the token bytes in i18n.h) ----

#define MAX_CAPS 9
typedef struct { const char *at[MAX_CAPS]; int32_t len[MAX_CAPS]; } Caps;

/** Whether c can be byte i of a run of token t. */
static bool in_run(int32_t t, char c, int32_t i) {
  switch (t) {
    case I18N_T_DIGITS: case I18N_T_DIGIT: return is_digit(c);
    case I18N_T_SDIGITS: return is_digit(c) || (i == 0 && c == '-');
    case I18N_T_NUM: return is_digit(c) || c == '.';
    case I18N_T_SPACES: case I18N_T_WS: case I18N_T_WS1: return is_space(c);
    default: return c != '\n';   // '.' as in JS
  }
}

/** Backtracking match of the rest of pattern p against the rest of s, the
 * k-th capture next. */
static bool match(const char *p, const char *s, Caps *caps, int32_t k) {
  for (;;) {
    const unsigned char t = (unsigned char)*p;
    if (!t) return !*s;
    if (t > I18N_T_END) {   // literal text
      if (*s != *p) return false;
      p++;
      s++;
      continue;
    }
    if (t == I18N_T_OPT) {
      if (*s == p[1] && match(p + 2, s + 1, caps, k)) return true;
      p += 2;
      continue;
    }
    if (t == I18N_T_NOT) {
      const char *end = strchr(p + 1, I18N_T_END);
      const size_t n = (size_t)(end - (p + 1));
      if (strncmp(s, p + 1, n) == 0) return false;
      p = end + 1;
      continue;
    }
    // A run of one class: the longest it can be, then shorter (lazy: the reverse).
    const bool captured = t < I18N_T_WS;
    const int32_t lo = t == I18N_T_ANY1 || t == I18N_T_DIGITS || t == I18N_T_SDIGITS || t == I18N_T_DIGIT ||
                       t == I18N_T_NUM || t == I18N_T_WS1;
    int32_t hi = 0;
    while (s[hi] && in_run(t, s[hi], hi) && !(t == I18N_T_DIGIT && hi == 1)) hi++;
    if (captured && k >= MAX_CAPS) return false;
    for (int32_t i = 0; i <= hi - lo; i++) {
      const int32_t n = t == I18N_T_LAZY ? lo + i : hi - i;
      if (t == I18N_T_SDIGITS && n == 1 && s[0] == '-') continue;
      if (captured) {
        caps->at[k] = s;
        caps->len[k] = n;
      }
      if (match(p + 1, s + n, caps, k + captured)) return true;
    }
    return false;
  }
}

/** Appends n bytes of src at *o, false if out (of I18N_BUF) is full. */
static bool put(char *out, size_t *o, const char *src, size_t n) {
  if (*o + n >= I18N_BUF) return false;
  memcpy(out + *o, src, n);
  *o += n;
  out[*o] = 0;
  return true;
}

static bool puts_(char *out, size_t *o, const char *src) { return put(out, o, src, strlen(src)); }

/** The replacement `to` with its $n (and $tn, translated) filled in from
 * caps, appended at *o. */
static bool expand(char *out, size_t *o, const char *to, const Caps *caps) {
  for (; *to; to++) {
    if (to[0] == '$' && to[1] >= '1' && to[1] <= '9') {
      const int32_t k = to[1] - '1';
      if (!put(out, o, caps->at[k], (size_t)caps->len[k])) return false;
      to++;
    } else if (to[0] == '$' && to[1] == 't' && to[2] >= '1' && to[2] <= '9') {
      const int32_t k = to[2] - '1';
      char cap[I18N_BUF];
      memcpy(cap, caps->at[k], (size_t)caps->len[k]);
      cap[caps->len[k]] = 0;
      if (!puts_(out, o, tr(cap))) return false;
      to += 2;
    } else {
      if (to[0] == '$' && to[1] == '$') to++;
      if (!put(out, o, to, 1)) return false;
    }
  }
  return true;
}

/** lead + mid + tail in the next ring buffer, or NULL if it does not fit. */
static const char *compose(const char *lead, size_t nlead, const char *mid, const char *tail) {
  char *out = ring[ring_next];
  size_t o = 0;
  out[0] = 0;
  if (!put(out, &o, lead, nlead) || !put(out, &o, mid, strlen(mid)) || !put(out, &o, tail, strlen(tail))) return NULL;
  ring_next = (ring_next + 1) % I18N_RING;
  return out;
}

static uint32_t hash(const char *s) {
  uint32_t h = 2166136261u;
  for (; *s; s++) h = (h ^ (unsigned char)*s) * 16777619u;
  return h;
}

/** The first pattern of `pats` that matches core, expanded into out. */
static int32_t try_patterns(const I18nPattern *pats, int32_t n, const char *core, char *out) {
  for (int32_t i = 0; i < n; i++) {
    Caps caps;
    if (!match(pats[i].re, core, &caps, 0)) continue;
    size_t o = 0;
    out[0] = 0;
    return expand(out, &o, pats[i].to, &caps) ? 1 : -1;
  }
  return 0;
}

// ---- the web's patterns whose replacement is a function (web/i18n.js,
// web/i18n-ext.js), as code ----

/** s starts with `pre`: the rest, else NULL. */
static const char *after(const char *s, const char *pre) {
  const size_t n = strlen(pre);
  return strncmp(s, pre, n) == 0 ? s + n : NULL;
}

// A stunt call, the base game's ("Wicked Forward loop with Rollspin by 360!!")
// or Extended's ("Breathtaking forward loop by 360!"): an adjective, then the
// pieces in the jar's order, then its exclamation marks. Translated piece by
// piece, as web/i18n-ext.js's trStunt() and the base port's assembly do.
static const I18nEntry kStuntAdj[] = {   // longest first
    {"World class", "De clase mundial"}, {"Breathtaking", "Asombrosa"}, {"Impressive", "Impresionante"},
    {"Incredible", "Incre\xc3\xad" "ble"}, {"Excellent", "Excelente"}, {"Legendary", "Legendaria"},
    {"Quality", "De calidad"}, {"Alright", "Bien"}, {"Amazing", "Incre\xc3\xad" "ble"}, {"Awesome", "Asombroso"},
    {"Ripping", "Tremendo"}, {"Radical", "Radical"}, {"Decent", "Decente"}, {"Wicked", "Brutal"},
    {"Super", "S\xc3\xbaper"}, {"Cool", "Genial"}, {"Nice", "Buena"}};
static const I18nEntry kStuntPiece[] = {   // longest first, so "double forward" wins over "forward"
    {"tabletop and reversed tabletop", "tabletop y tabletop invertido"},
    {"Tabletop and reversed Tabletop", "Tabletop y Tabletop invertido"},
    {"massive forward looping", "enorme giro adelante"}, {"massive Forward looping", "enorme giro adelante"},
    {"massive back looping", "enorme giro atr\xc3\xa1s"}, {"massive Back looping", "enorme giro atr\xc3\xa1s"},
    {"massive roll spinning", "enorme giro de lado"}, {"massive Roll spinning", "enorme giro de lado"},
    {"double rollspin", "doble giro de lado"}, {"double Rollspin", "doble giro de lado"},
    {"triple rollspin", "triple giro de lado"}, {"triple Rollspin", "triple giro de lado"},
    {"double forward", "doble adelante"}, {"double Forward", "doble adelante"},
    {"triple forward", "triple adelante"}, {"triple Forward", "triple adelante"},
    {"forward loop", "giro adelante"}, {"Forward loop", "Giro adelante"},
    {"double back", "doble atr\xc3\xa1s"}, {"double Back", "doble atr\xc3\xa1s"},
    {"triple back", "triple atr\xc3\xa1s"}, {"triple Back", "triple atr\xc3\xa1s"},
    {"backloop", "giro atr\xc3\xa1s"}, {"Backloop", "Giro atr\xc3\xa1s"},
    {"rollspin", "giro de lado"}, {"Rollspin", "Giro de lado"},
    {"flipside", "vuelta lateral"}, {"Flipside", "Vuelta lateral"},
    {"tabletop", "tabletop"}, {"Tabletop", "Tabletop"}, {"hanged ", "colgado "}, {"Hanged ", "Colgado "},
    {"surf style", "estilo surf"}, {"off the lip", "desde el borde"}, {"bounce back", "rebote"},
    {"off the ramp", "desde la rampa"}, {"radical rebound", "rebote radical"},
    {" and beyond", " y m\xc3\xa1s all\xc3\xa1"}, {" with ", " con "}, {" by ", " por "}};
#define COUNT(a) ((int32_t)(sizeof(a) / sizeof((a)[0])))

static bool tr_stunt(const char *s, char *out) {
  const I18nEntry *adj = NULL;
  for (int32_t i = 0; i < COUNT(kStuntAdj) && !adj; i++)
    if (after(s, kStuntAdj[i].en)) adj = &kStuntAdj[i];
  if (!adj) return false;
  size_t end = strlen(s), bangs = 0;
  while (end > 0 && s[end - 1] == '!') end--, bangs++;
  if (bangs == 0) return false;
  const char *rest = s + strlen(adj->en), *stop = s + end;
  if (rest < stop && *rest != ' ') return false;
  size_t o = 0;
  out[0] = 0;
  if (!puts_(out, &o, "\xc2\xa1") || !puts_(out, &o, adj->es)) return false;
  while (rest < stop) {
    const I18nEntry *hit = NULL;
    for (int32_t i = 0; i < COUNT(kStuntPiece) && !hit; i++) {
      const size_t n = strlen(kStuntPiece[i].en);
      if ((size_t)(stop - rest) >= n && strncmp(rest, kStuntPiece[i].en, n) == 0) hit = &kStuntPiece[i];
    }
    if (hit) {
      if (!puts_(out, &o, hit->es)) return false;
      rest += strlen(hit->en);
      continue;
    }
    size_t n = 0;
    if (*rest == ' ') n = 1;
    else while (rest + n < stop && is_digit(rest[n])) n++;
    if (n == 0) return false;   // not the stunt grammar: leave it to the other rules
    if (!put(out, &o, rest, n)) return false;
    rest += n;
  }
  while (bangs--)
    if (!put(out, &o, "!", 1)) return false;
  return true;
}

/** "N car(s)", "N game(s)", "N player(s)": the count, and whether it is plural. */
static const char *count_of(const char *s, int32_t *n) {
  *n = 0;
  const char *c = s;
  while (is_digit(*c)) *n = *n * 10 + (*c++ - '0');
  return c > s ? c : NULL;
}

/** The function patterns that read like ordinary ones: tried before the tables'. */
static bool tr_special(const char *core, char *out) {
  size_t o = 0;
  out[0] = 0;
  const char *r;
  int32_t n;
  // Arrow now pointing at > CARS / TRACK, maybe with the radar hint.
  if ((r = after(core, "Arrow now pointing at >"))) {
    while (is_space(*r)) r++;
    const bool cars = after(r, "CARS") != NULL;
    if (cars || after(r, "TRACK")) {
      const char *what = cars ? "La flecha apunta a los autos" : "La flecha apunta a la pista";
      r += cars ? 4 : 5;
      const char *hint = r;
      while (is_space(*hint)) hint++;
      if (!*r) return puts_(out, &o, what);
      if (hint > r && strcmp(hint, "Press [S] to toggle Radar!") == 0)
        return puts_(out, &o, what) && puts_(out, &o, " \xe2\x80\x94 [S] alterna el radar");
    }
  }
  if (tr_stunt(core, out)) return true;
  // N car(s)[ · rest]
  if ((r = count_of(core, &n)) && after(r, " car")) {
    const char *t = r + 4;
    if (*t == 's') t++;
    if (!*t || after(t, " \xc2\xb7 ")) {
      char num[16];
      memcpy(num, core, (size_t)(r - core));
      num[r - core] = 0;
      if (!puts_(out, &o, num) || !puts_(out, &o, strcmp(num, "1") == 0 ? " auto" : " autos")) return false;
      if (*t) return puts_(out, &o, " \xc2\xb7 ") && puts_(out, &o, tr(t + 4));
      return true;
    }
  }
  // N game(s) · Enter to join
  if ((r = count_of(core, &n)) && after(r, " game")) {
    const char *t = r + 5;
    if (*t == 's') t++;
    if (strcmp(t, " \xc2\xb7 Enter to join") == 0)
      return put(out, &o, core, (size_t)(r - core)) && puts_(out, &o, n == 1 && r - core == 1 ? " partida" : " partidas") &&
             puts_(out, &o, " \xc2\xb7 Enter para unirse");
  }
  // Start Race - N player(s)
  if ((r = after(core, "Start Race \xe2\x80\x94 ")) && (r = count_of(r, &n)) && after(r, " player")) {
    const char *t = r + 7;
    if (*t == 's') t++;
    if (!*t) {
      const char *num = core + strlen("Start Race \xe2\x80\x94 ");
      return puts_(out, &o, "Iniciar carrera \xe2\x80\x94 ") && put(out, &o, num, (size_t)(r - num)) &&
             puts_(out, &o, (r - num == 1 && *num == '1') ? " jugador" : " jugadores");
    }
  }
  // Class X[ · my car]
  if ((r = after(core, "Class ")) && *r) {
    char c[I18N_BUF];
    strncpy(c, r, sizeof(c) - 1);
    c[sizeof(c) - 1] = 0;
    const char *mine = " \xc2\xb7 my car";
    const size_t cl = strlen(c), ml = strlen(mine);
    const bool has_mine = cl > ml && strcmp(c + cl - ml, mine) == 0;
    if (has_mine) c[cl - ml] = 0;
    return puts_(out, &o, "Clase ") && puts_(out, &o, tr(c)) && (!has_mine || puts_(out, &o, " \xc2\xb7 mi auto"));
  }
  return false;
}

/** The catch-all function patterns: tried after the tables'. */
static bool tr_special_last(const char *core, char *out) {
  size_t o = 0;
  out[0] = 0;
  const size_t n = strlen(core);
  // "<title> replay"
  if (n > 7 && strcmp(core + n - 7, " replay") == 0) {
    char t[I18N_BUF];
    memcpy(t, core, n - 7);
    t[n - 7] = 0;
    return puts_(out, &o, "Repetici\xc3\xb3n: ") && puts_(out, &o, tr(t));
  }
  // "<word> ·", what a key hint leaves between its keys
  if (n > 3 && strcmp(core + n - 3, " \xc2\xb7") == 0) {
    char w[I18N_BUF];
    memcpy(w, core, n - 3);
    w[n - 3] = 0;
    return puts_(out, &o, tr(w)) && puts_(out, &o, " \xc2\xb7");
  }
  return false;
}

static const char *translate(const char *s) {
  const char *hit = lookup(s);
  if (hit) return hit;
  const size_t len = strlen(s);
  size_t a = 0, b = len;
  while (a < len && is_space(s[a])) a++;
  while (b > a && is_space(s[b - 1])) b--;
  if (a == b || b - a >= I18N_BUF) return s;
  char core[I18N_BUF];
  memcpy(core, s + a, b - a);
  core[b - a] = 0;
  const char *tail = s + b;
  if (a > 0 || b < len) {
    hit = lookup(core);
    if (hit) return compose(s, a, hit, tail);
  }
  // Text that wraps: match on the words, not the layout.
  char flat[I18N_BUF];
  size_t f = 0;
  for (const char *c = core; *c; c++) {
    if (!is_space(*c)) flat[f++] = *c;
    else if (f == 0 || flat[f - 1] != ' ') flat[f++] = ' ';
  }
  flat[f] = 0;
  if (strcmp(flat, core) != 0) {
    hit = lookup(flat);
    if (hit) return compose(s, a, hit, tail);
  }
  char out[I18N_BUF];
  int32_t r = try_patterns(I18N_PORT_ES_PATTERNS, I18N_PORT_ES_PATTERN_COUNT, core, out);
  if (r == 0) r = tr_special(core, out) ? 1 : 0;
  if (r == 0) r = try_patterns(I18N_ES_PATTERNS, I18N_ES_PATTERN_COUNT, core, out);
  if (r == 0) r = tr_special_last(core, out) ? 1 : 0;
  if (r < 0) return s;
  if (r > 0) return compose(s, a, out, tail);
  // Several messages joined into one text: each line on its own.
  if (strchr(core, '\n')) {
    size_t o = 0;
    out[0] = 0;
    for (char *line = core;;) {
      char *nl = strchr(line, '\n');
      if (nl) *nl = 0;
      const char *t = tr(line);
      if (!put(out, &o, t, strlen(t))) return s;
      if (!nl) break;
      if (!put(out, &o, "\n", 1)) return s;
      line = nl + 1;
    }
    return compose(s, a, out, tail);
  }
  return s;
}

const char *tr(const char *s) {
  if (lang == I18N_LANG_EN || !s || !*s) return s;
  const size_t len = strlen(s);
  char *slot = len < MISS_LEN ? miss[hash(s) % MISS_SLOTS] : NULL;
  if (slot && strcmp(slot, s) == 0) return s;
  const char *out = translate(s);
  if (!out) return s;
  if (out == s && slot) memcpy(slot, s, len + 1);
  return out;
}
