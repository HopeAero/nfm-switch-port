// ports web/graphics.js (geometry-building half only -- see gfx.h)
#include "gfx.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_VERTS (1 << 12)
#define INITIAL_CMDS (1 << 6)

void gfx_init(Graphics2D *g, int32_t width, int32_t height) {
  memset(g, 0, sizeof(*g));
  g->width = width;
  g->height = height;
  g->capacity = INITIAL_VERTS;
  g->verts = malloc(sizeof(GfxVert) * (size_t)g->capacity);
  g->cmd_capacity = INITIAL_CMDS;
  g->cmds = malloc(sizeof(GfxDrawCmd) * (size_t)g->cmd_capacity);
  g->a = 1.0f;
  g->rgba = 0xff000000u;
  g->lineWidth = 1.0f;
}

void gfx_free(Graphics2D *g) {
  free(g->verts);
  g->verts = NULL;
  free(g->cmds);
  g->cmds = NULL;
}

void gfx_begin(Graphics2D *g) {
  g->count = 0;
  g->cmd_count = 0;
  g->flushed = 0;
  g->inputVerts = 0;
  g->objCalls = 0;
  g->objDrawn = 0;
  g->faceCalls = 0;
  g->projVerts = 0;
  g->a = 1.0f;
  // recompute rgba with a=1 (matches web/graphics.js's begin(), which does
  // this._pack() with this.a forced to 1 for the same "a throw mid-composite
  // must not dim the next frame" reason).
  uint8_t r8 = (uint8_t)(g->r * 255.0f + 0.5f);
  uint8_t g8 = (uint8_t)(g->g * 255.0f + 0.5f);
  uint8_t b8 = (uint8_t)(g->b * 255.0f + 0.5f);
  g->rgba = (0xffu << 24) | ((uint32_t)b8 << 16) | ((uint32_t)g8 << 8) | r8;
}

// Round-and-clamp a 0..255 channel, matching web/graphics.js's clampByte.
static uint8_t clamp_byte(float v) {
  int32_t n = (int32_t)(v + 0.5f);
  if (n < 0) return 0;
  if (n > 255) return 255;
  return (uint8_t)n;
}

static void repack(Graphics2D *g) {
  uint8_t r = clamp_byte(g->r * 255.0f);
  uint8_t gg = clamp_byte(g->g * 255.0f);
  uint8_t b = clamp_byte(g->b * 255.0f);
  uint8_t a = clamp_byte(g->a * 255.0f);
  g->rgba = ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)gg << 8) | r;
}

void gfx_set_color(Graphics2D *g, int32_t r, int32_t gg, int32_t b) {
  g->r = (float)r / 255.0f;
  g->g = (float)gg / 255.0f;
  g->b = (float)b / 255.0f;
  repack(g);
}

void gfx_set_composite(Graphics2D *g, float alpha) {
  g->a = alpha;
  repack(g);
}

static void ensure_capacity(Graphics2D *g, int32_t needed) {
  if (needed <= g->capacity) return;
  int32_t cap = g->capacity;
  while (cap < needed) cap *= 2;
  g->verts = realloc(g->verts, sizeof(GfxVert) * (size_t)cap);
  g->capacity = cap;
}

// Unchecked: the caller has already reserved room (emit_tri, or a fill
// that reserves for a whole polygon). Checking capacity per vertex was ~6%
// of a race frame's CPU.
static inline void put_vert(Graphics2D *g, float x, float y) {
  GfxVert *v = &g->verts[g->count++];
  v->x = x;
  v->y = y;
  v->rgba = g->rgba;
}

static inline void put_tri(Graphics2D *g, float x0, float y0, float x1, float y1, float x2, float y2) {
  put_vert(g, x0, y0);
  put_vert(g, x1, y1);
  put_vert(g, x2, y2);
}

static void emit_tri(Graphics2D *g, float x0, float y0, float x1, float y1, float x2, float y2) {
  ensure_capacity(g, g->count + 3);
  put_tri(g, x0, y0, x1, y1, x2, y2);
}

/** True if every turn has the same sign -- i.e. the polygon is convex. */
static bool is_convex(const int32_t *xs, const int32_t *ys, int32_t n) {
  int32_t sign = 0;
  for (int32_t i = 0; i < n; i++) {
    double ax = (double)xs[(i + 1) % n] - (double)xs[i];
    double ay = (double)ys[(i + 1) % n] - (double)ys[i];
    double bx = (double)xs[(i + 2) % n] - (double)xs[(i + 1) % n];
    double by = (double)ys[(i + 2) % n] - (double)ys[(i + 1) % n];
    double cross = ax * by - ay * bx;
    if (cross == 0) continue; // collinear, tells us nothing
    int32_t s = cross > 0 ? 1 : -1;
    if (sign == 0) sign = s;
    else if (s != sign) return false;
  }
  return true;
}

typedef struct { double lo, hi; } Span;

/**
 * Even-odd fill of a concave or self-intersecting polygon, as trapezoids.
 * Ports web/graphics.js's _fillTrapezoid line for line. All double-precision
 * screen-space geometry -- there is no Java equivalent for this file (it
 * replaces java.awt's rasterizer, not transpiles it), so none of the
 * fr()/trunc() int32-Java machinery applies; this is just "compute in
 * double, matching what JS numbers already are."
 */
static void fill_trapezoid(Graphics2D *g, const int32_t *xs, const int32_t *ys, int32_t n) {
  double H = g->height;
  double ymin = 1e300, ymax = -1e300;
  for (int32_t i = 0; i < n; i++) {
    if (ys[i] < ymin) ymin = ys[i];
    if (ys[i] > ymax) ymax = ys[i];
  }
  double top = ymin < 0 ? 0 : ymin;
  double bot = ymax > H ? H : ymax;
  if (bot - top <= 0) return;

  // Worst case: 2 (top/bot) + n (vertices in range) + n*(n-1)/2 (proper
  // pairwise intersections).
  // Stack buffers cover every polygon the game makes (faces are <= 28
  // vertices -> 408 events); the heap is only a fallback. This runs for
  // every concave polygon, hundreds a frame, and two malloc/free pairs
  // apiece is real time on the Vita's CPU.
  int32_t ev_cap = 2 + n + (n * (n - 1)) / 2;
  double ev_stack[512];
  Span span_stack[64];
  double *ev = ev_cap <= 512 ? ev_stack : malloc(sizeof(double) * (size_t)ev_cap);
  int32_t ev_n = 0;
  ev[ev_n++] = top;
  ev[ev_n++] = bot;
  for (int32_t i = 0; i < n; i++) {
    double y = ys[i];
    if (y > top && y < bot) ev[ev_n++] = y;
  }
  for (int32_t i = 0; i < n; i++) {
    double ax = xs[i], ay = ys[i];
    double bx = xs[(i + 1) % n], by = ys[(i + 1) % n];
    for (int32_t j = i + 1; j < n; j++) {
      double cx = xs[j], cy = ys[j];
      double dx = xs[(j + 1) % n], dy = ys[(j + 1) % n];
      double den = (bx - ax) * (dy - cy) - (by - ay) * (dx - cx);
      if (den == 0) continue;
      double t = ((cx - ax) * (dy - cy) - (cy - ay) * (dx - cx)) / den;
      if (t <= 0 || t >= 1) continue;
      double u = ((cx - ax) * (by - ay) - (cy - ay) * (bx - ax)) / den;
      if (u <= 0 || u >= 1) continue;
      double y = ay + t * (by - ay);
      if (y > top && y < bot) ev[ev_n++] = y;
    }
  }
  // Insertion sort: ev_n is small (bounded as above), and this avoids
  // pulling in qsort's comparator-callback overhead per polygon.
  for (int32_t i = 1; i < ev_n; i++) {
    double key = ev[i];
    int32_t j = i - 1;
    while (j >= 0 && ev[j] > key) { ev[j + 1] = ev[j]; j--; }
    ev[j + 1] = key;
  }

  Span *span = n <= 64 ? span_stack : malloc(sizeof(Span) * (size_t)n); // at most n edges can cross a band
  for (int32_t e = 0; e + 1 < ev_n; e++) {
    double ya = ev[e], yb = ev[e + 1];
    if (yb - ya < 1e-9) continue;
    double mid = (ya + yb) * 0.5;
    int32_t span_n = 0;
    for (int32_t i = 0, j = n - 1; i < n; j = i++) {
      double y1 = ys[j], y2 = ys[i];
      if ((y1 <= mid && y2 > mid) || (y2 <= mid && y1 > mid)) {
        double x1 = xs[j];
        double slope = (xs[i] - x1) / (y2 - y1);
        span[span_n].lo = x1 + (ya - y1) * slope;
        span[span_n].hi = x1 + (yb - y1) * slope;
        span_n++;
      }
    }
    if (span_n < 2) continue;
    // Sort by (lo, hi) -- matches JS's BY_X comparator. Insertion sort again,
    // span_n <= n which is small here.
    for (int32_t i = 1; i < span_n; i++) {
      Span key = span[i];
      int32_t j = i - 1;
      while (j >= 0 && (span[j].lo > key.lo || (span[j].lo == key.lo && span[j].hi > key.hi))) {
        span[j + 1] = span[j]; j--;
      }
      span[j + 1] = key;
    }
    for (int32_t k = 0; k + 1 < span_n; k += 2) {
      Span l = span[k], r = span[k + 1];
      emit_tri(g, (float)l.lo, (float)ya, (float)r.lo, (float)ya, (float)r.hi, (float)yb);
      emit_tri(g, (float)l.lo, (float)ya, (float)r.hi, (float)yb, (float)l.hi, (float)yb);
    }
  }
  if (span != span_stack) free(span);
  if (ev != ev_stack) free(ev);
}

void gfx_fill_polygon(Graphics2D *g, const int32_t *xs, const int32_t *ys, int32_t n) {
  g->inputVerts += n;
  if (n < 3) return;
  if (n == 3 || is_convex(xs, ys, n)) {
    ensure_capacity(g, g->count + 3 * (n - 2));
    for (int32_t i = 1; i + 1 < n; i++) {
      put_tri(g, (float)xs[0], (float)ys[0], (float)xs[i], (float)ys[i], (float)xs[i + 1], (float)ys[i + 1]);
    }
    return;
  }
  fill_trapezoid(g, xs, ys, n);
}

static void segment(Graphics2D *g, double x0, double y0, double x1, double y1) {
  double dx = x1 - x0, dy = y1 - y0;
  double len = sqrt(dx * dx + dy * dy);
  if (len < 1e-6) return;
  double h = g->lineWidth / 2.0;
  double nx = (-dy / len) * h, ny = (dx / len) * h;
  emit_tri(g, (float)(x0 + nx), (float)(y0 + ny), (float)(x1 + nx), (float)(y1 + ny), (float)(x1 - nx), (float)(y1 - ny));
  emit_tri(g, (float)(x0 + nx), (float)(y0 + ny), (float)(x1 - nx), (float)(y1 - ny), (float)(x0 - nx), (float)(y0 - ny));
}

void gfx_draw_polygon(Graphics2D *g, const int32_t *xs, const int32_t *ys, int32_t n) {
  g->inputVerts += n;
  for (int32_t i = 0; i < n; i++) {
    int32_t j = (i + 1) % n;
    segment(g, xs[i], ys[i], xs[j], ys[j]);
  }
}

void gfx_draw_line(Graphics2D *g, int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
  segment(g, x0, y0, x1, y1);
}

void gfx_fill_rect(Graphics2D *g, int32_t x, int32_t y, int32_t w, int32_t h) {
  emit_tri(g, (float)x, (float)y, (float)(x + w), (float)y, (float)(x + w), (float)(y + h));
  emit_tri(g, (float)x, (float)y, (float)(x + w), (float)(y + h), (float)x, (float)(y + h));
}

void gfx_draw_rect(Graphics2D *g, int32_t x, int32_t y, int32_t w, int32_t h) {
  segment(g, x, y, x + w, y);
  segment(g, x + w, y, x + w, y + h);
  segment(g, x + w, y + h, x, y + h);
  segment(g, x, y + h, x, y);
}

void gfx_fill_oval(Graphics2D *g, int32_t x, int32_t y, int32_t w, int32_t h) {
  double cx = x + w / 2.0, cy = y + h / 2.0, rx = w / 2.0, ry = h / 2.0;
  int32_t steps = (int32_t)ceil((rx + ry) * 0.7);
  if (steps < 8) steps = 8;
  if (steps > 64) steps = 64;
  for (int32_t i = 0; i < steps; i++) {
    double a0 = ((double)i / steps) * M_PI * 2.0;
    double a1 = ((double)(i + 1) / steps) * M_PI * 2.0;
    emit_tri(g, (float)cx, (float)cy,
             (float)(cx + cos(a0) * rx), (float)(cy + sin(a0) * ry),
             (float)(cx + cos(a1) * rx), (float)(cy + sin(a1) * ry));
  }
}

// Points around a round rect, clockwise from the top-left corner's top end:
// four quarter-ellipse arcs of ROUND_STEPS segments each.
#define ROUND_STEPS 6
static int32_t round_rect_path(double x, double y, double w, double h, double arc_w, double arc_h,
                               double *px, double *py) {
  double rx = arc_w / 2.0, ry = arc_h / 2.0;
  if (rx > w / 2.0) rx = w / 2.0;
  if (ry > h / 2.0) ry = h / 2.0;
  // Corner ellipse centres, in path order: top-right, bottom-right,
  // bottom-left, top-left; each arc starts at angle -90 + 90*k degrees.
  const double cx[4] = {x + w - rx, x + w - rx, x + rx, x + rx};
  const double cy[4] = {y + ry, y + h - ry, y + h - ry, y + ry};
  int32_t n = 0;
  for (int32_t k = 0; k < 4; k++) {
    for (int32_t s = 0; s <= ROUND_STEPS; s++) {
      double a = (-90.0 + 90.0 * k + 90.0 * s / ROUND_STEPS) * M_PI / 180.0;
      px[n] = cx[k] + cos(a) * rx;
      py[n] = cy[k] + sin(a) * ry;
      n++;
    }
  }
  return n;
}

void gfx_fill_round_rect(Graphics2D *g, int32_t x, int32_t y, int32_t w, int32_t h,
                         int32_t arc_w, int32_t arc_h) {
  double px[4 * (ROUND_STEPS + 1)], py[4 * (ROUND_STEPS + 1)];
  int32_t n = round_rect_path(x, y, w, h, arc_w, arc_h, px, py);
  // Convex, so a fan from the centre; emitted in place like every fill.
  float cx = (float)(x + w / 2.0), cy = (float)(y + h / 2.0);
  for (int32_t i = 0; i < n; i++) {
    int32_t j = (i + 1) % n;
    emit_tri(g, cx, cy, (float)px[i], (float)py[i], (float)px[j], (float)py[j]);
  }
}

void gfx_draw_round_rect(Graphics2D *g, int32_t x, int32_t y, int32_t w, int32_t h,
                         int32_t arc_w, int32_t arc_h) {
  double px[4 * (ROUND_STEPS + 1)], py[4 * (ROUND_STEPS + 1)];
  int32_t n = round_rect_path(x, y, w, h, arc_w, arc_h, px, py);
  for (int32_t i = 0; i < n; i++) {
    int32_t j = (i + 1) % n;
    segment(g, px[i], py[i], px[j], py[j]);
  }
}

// Ports web/graphics.js's clearRect verbatim, INCLUDING its quirk: it sets
// r/g/b/a to black-opaque and restores them afterward, but never calls
// _pack() (setColor/setComposite's job) in between -- so the packed `rgba`
// vertex colour that fillRect actually reads is never touched, and this
// draws with whatever colour was last set via setColor, not black. Verified
// against the real JS (see the commit message for the oracle run); this is
// not a slip to fix, per web/TRANSPILE_SPEC.md §3.
void gfx_clear_rect(Graphics2D *g, int32_t x, int32_t y, int32_t w, int32_t h) {
  float r = g->r, gg = g->g, b = g->b, a = g->a;
  g->r = g->g = g->b = 0.0f;
  g->a = 1.0f;
  gfx_fill_rect(g, x, y, w, h);
  g->r = r; g->g = gg; g->b = b; g->a = a;
}

GfxMark gfx_mark(const Graphics2D *g) {
  GfxMark k = {g->count, g->cmd_count, g->flushed};
  return k;
}

void gfx_rewind(Graphics2D *g, GfxMark mark) {
  g->count = mark.count;
  g->cmd_count = mark.cmd_count;
  g->flushed = mark.flushed;
}

void gfx_set_rendering_hint(Graphics2D *g) {
  (void)g;
}

static GfxDrawCmd *push_cmd(Graphics2D *g) {
  if (g->cmd_count >= g->cmd_capacity) {
    g->cmd_capacity *= 2;
    g->cmds = realloc(g->cmds, sizeof(GfxDrawCmd) * (size_t)g->cmd_capacity);
  }
  return &g->cmds[g->cmd_count++];
}

void gfx_flush(Graphics2D *g) {
  if (g->count > g->flushed) {
    GfxDrawCmd *batch = push_cmd(g);
    batch->vert_start = g->flushed;
    batch->vert_count = g->count - g->flushed;
    batch->image_id = -1;
    g->flushed = g->count;
  }
}

void gfx_clip_save(GfxClip *c, const Graphics2D *g, GfxMark from) {
  c->nverts = g->count - from.count;
  c->ncmds = g->cmd_count - from.cmd_count;
  c->flushed = g->flushed - from.count;
  if (c->nverts > c->vcap) {
    c->vcap = c->nverts;
    c->verts = realloc(c->verts, sizeof(GfxVert) * (size_t)c->vcap);
  }
  if (c->ncmds > c->ccap) {
    c->ccap = c->ncmds;
    c->cmds = realloc(c->cmds, sizeof(GfxDrawCmd) * (size_t)c->ccap);
  }
  memcpy(c->verts, g->verts + from.count, sizeof(GfxVert) * (size_t)c->nverts);
  memcpy(c->cmds, g->cmds + from.cmd_count, sizeof(GfxDrawCmd) * (size_t)c->ncmds);
  for (int32_t i = 0; i < c->ncmds; i++) {
    if (c->cmds[i].vert_count > 0) c->cmds[i].vert_start -= from.count;
  }
}

void gfx_clip_play(Graphics2D *g, const GfxClip *c) {
  gfx_flush(g);
  const int32_t base = g->count;
  ensure_capacity(g, base + c->nverts);
  memcpy(g->verts + base, c->verts, sizeof(GfxVert) * (size_t)c->nverts);
  for (int32_t i = 0; i < c->ncmds; i++) {
    GfxDrawCmd *d = push_cmd(g);
    *d = c->cmds[i];
    if (d->vert_count > 0) d->vert_start += base;
  }
  g->count = base + c->nverts;
  g->flushed = base + c->flushed;
}

void gfx_draw_image(Graphics2D *g, int32_t image_id, int32_t x, int32_t y, int32_t w, int32_t h) {
  gfx_draw_image_sub(g, image_id, x, y, w, h, 0, 0, w, h, w, h);
}

void gfx_draw_image_sub(Graphics2D *g, int32_t image_id, int32_t dst_x, int32_t dst_y,
                         int32_t dst_w, int32_t dst_h,
                         int32_t src_x, int32_t src_y, int32_t src_w, int32_t src_h,
                         int32_t image_w, int32_t image_h) {
  gfx_flush(g);
  GfxDrawCmd *img = push_cmd(g);
  img->vert_start = 0;
  img->vert_count = 0;
  img->image_id = image_id;
  img->x = (float)dst_x;
  img->y = (float)dst_y;
  img->w = (float)dst_w;
  img->h = (float)dst_h;
  img->alpha = g->a;
  // Convert source-pixel rect to UVs. When image_w/image_h == 0 (which
  // gfx_draw_image never passes but a caller could) fall back to full
  // texture to avoid a NaN.
  float sw = image_w > 0 ? (float)image_w : 1.0f;
  float sh = image_h > 0 ? (float)image_h : 1.0f;
  img->u0 = (float)src_x / sw;
  img->v0 = (float)src_y / sh;
  img->u1 = (float)(src_x + src_w) / sw;
  img->v1 = (float)(src_y + src_h) / sh;
}

void gfx_vertex_at(const Graphics2D *g, int32_t i, float *x, float *y) {
  *x = g->verts[i].x;
  *y = g->verts[i].y;
}

void gfx_color_at(const Graphics2D *g, int32_t i, int32_t *r, int32_t *gg, int32_t *b, int32_t *a) {
  uint32_t c = g->verts[i].rgba;
  *r = c & 255;
  *gg = (c >> 8) & 255;
  *b = (c >> 16) & 255;
  *a = (c >> 24) & 255;
}
