#include "i18n.h"

#include <stdbool.h>
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

static const char *lookup(const char *s) {
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

/** The replacement `to` with its $n filled in from caps, appended at *o. */
static bool expand(char *out, size_t *o, const char *to, const Caps *caps) {
  for (; *to; to++) {
    if (to[0] == '$' && to[1] >= '1' && to[1] <= '9') {
      const int32_t k = to[1] - '1';
      if (!put(out, o, caps->at[k], (size_t)caps->len[k])) return false;
      to++;
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
  for (int32_t i = 0; i < I18N_ES_PATTERN_COUNT; i++) {
    Caps caps;
    if (!match(I18N_ES_PATTERNS[i].re, core, &caps, 0)) continue;
    char out[I18N_BUF];
    size_t o = 0;
    out[0] = 0;
    if (!expand(out, &o, I18N_ES_PATTERNS[i].to, &caps)) return s;
    return compose(s, a, out, tail);
  }
  // Several messages joined into one text: each line on its own.
  if (strchr(core, '\n')) {
    char out[I18N_BUF];
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
