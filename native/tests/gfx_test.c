// Host-buildable test for native/core/gfx.c (the geometry-building half of
// the WebGL Graphics2D port -- no GL context involved, see gfx.h).
//
// No web/gfx.test.js exists (this is a new C module ported from
// web/graphics.js, which has its own web/graphics.test.js) -- expected
// values here are either copied straight from web/graphics.test.js's own
// assertions (same test names/shapes, ported) or, where graphics.test.js
// doesn't cover a case (the concave trapezoid fill, clearRect's packing
// quirk), captured by running the real web/graphics.js headless under Node
// (see the commit that added this file for the scripts).
#include <stdio.h>
#include "../core/gfx.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
  if (!(cond)) { fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); failures++; } \
} while (0)

static void check_vertex(Graphics2D *g, int32_t i, float wantX, float wantY, const char *label) {
  float x, y;
  gfx_vertex_at(g, i, &x, &y);
  CHECK(x == wantX && y == wantY, label);
}

// Ported from web/graphics.test.js: "a convex quad fans into two triangles"
static void test_convex_quad_fan(void) {
  Graphics2D g;
  gfx_init(&g, 800, 450);
  gfx_begin(&g);
  gfx_set_color(&g, 10, 20, 30);
  int32_t xs[4] = {0, 10, 10, 0};
  int32_t ys[4] = {0, 0, 10, 10};
  gfx_fill_polygon(&g, xs, ys, 4);
  CHECK(g.count == 6, "convex quad vertex count");
  check_vertex(&g, 0, 0, 0, "quad v0");
  check_vertex(&g, 1, 10, 0, "quad v1");
  check_vertex(&g, 2, 10, 10, "quad v2");
  check_vertex(&g, 3, 0, 0, "quad v3");
  check_vertex(&g, 4, 10, 10, "quad v4");
  check_vertex(&g, 5, 0, 10, "quad v5");
  gfx_free(&g);
}

// Ported from web/graphics.test.js: "colour is per-vertex..."
static void test_color_per_vertex(void) {
  Graphics2D g;
  gfx_init(&g, 800, 450);
  gfx_begin(&g);
  gfx_set_color(&g, 255, 0, 0);
  int32_t xs[3] = {0, 1, 1}, ys[3] = {0, 0, 1};
  gfx_fill_polygon(&g, xs, ys, 3);
  gfx_set_color(&g, 0, 255, 0);
  gfx_fill_polygon(&g, xs, ys, 3);
  CHECK(g.count == 6, "color test vertex count");
  int32_t r, gg, b, a;
  for (int i = 0; i < 3; i++) {
    gfx_color_at(&g, i, &r, &gg, &b, &a);
    CHECK(r == 255 && gg == 0 && b == 0, "first tri red");
  }
  for (int i = 3; i < 6; i++) {
    gfx_color_at(&g, i, &r, &gg, &b, &a);
    CHECK(r == 0 && gg == 255 && b == 0, "second tri green");
  }
  gfx_free(&g);
}

// Ported from web/graphics.test.js: "submission order is preserved across
// mixed primitive types" -- the load-bearing test.
static void test_submission_order(void) {
  Graphics2D g;
  gfx_init(&g, 800, 450);
  gfx_begin(&g);
  int32_t xs3[3] = {0, 9, 9}, ys3[3] = {0, 0, 9};
  gfx_set_color(&g, 1, 0, 0); gfx_fill_polygon(&g, xs3, ys3, 3);
  gfx_set_color(&g, 2, 0, 0); gfx_draw_line(&g, 0, 0, 10, 0);
  gfx_set_color(&g, 3, 0, 0); gfx_fill_rect(&g, 0, 0, 5, 5);
  gfx_set_color(&g, 4, 0, 0); gfx_draw_polygon(&g, xs3, ys3, 3);
  CHECK(g.count == 33, "mixed order vertex count");
  int32_t want[33] = {
    1,1,1, 2,2,2,2,2,2, 3,3,3,3,3,3, 4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
  };
  int32_t r, gg, b, a;
  for (int i = 0; i < 33; i++) {
    gfx_color_at(&g, i, &r, &gg, &b, &a);
    char label[32];
    snprintf(label, sizeof(label), "order[%d]", i);
    CHECK(r == want[i], label);
  }
  gfx_free(&g);
}

// Ported from web/graphics.test.js: "outlines are triangles..."
static void test_outline_triangle_count(void) {
  Graphics2D g;
  gfx_init(&g, 800, 450);
  gfx_begin(&g);
  int32_t xs[3] = {0, 10, 10}, ys[3] = {0, 0, 10};
  gfx_draw_polygon(&g, xs, ys, 3);
  CHECK(g.count == 3 * 6, "outline vertex count");
  gfx_free(&g);
}

// Ported from web/graphics.test.js: "zero-length segments are dropped"
static void test_zero_length_segment_dropped(void) {
  Graphics2D g;
  gfx_init(&g, 800, 450);
  gfx_begin(&g);
  gfx_draw_line(&g, 5, 5, 5, 5);
  CHECK(g.count == 0, "zero-length segment produces no vertices");
  gfx_free(&g);
}

// Oracle: L-shaped (concave) hexagon, captured from the real web/graphics.js
// running headless under Node.
static void test_concave_trapezoid_fill(void) {
  Graphics2D g;
  gfx_init(&g, 800, 450);
  gfx_begin(&g);
  gfx_set_color(&g, 10, 20, 30);
  int32_t xs[6] = {0, 4, 4, 2, 2, 0};
  int32_t ys[6] = {0, 0, 2, 2, 4, 4};
  gfx_fill_polygon(&g, xs, ys, 6);
  CHECK(g.count == 12, "L-shape vertex count");
  float want_x[12] = {0,4,4, 0,4,0, 0,2,2, 0,2,0};
  float want_y[12] = {0,0,2, 0,2,2, 2,2,4, 2,4,4};
  for (int i = 0; i < 12; i++) {
    char label[32];
    snprintf(label, sizeof(label), "L-shape v%d", i);
    check_vertex(&g, i, want_x[i], want_y[i], label);
  }
  int32_t r, gg, b, a;
  for (int i = 0; i < 12; i++) {
    gfx_color_at(&g, i, &r, &gg, &b, &a);
    CHECK(r == 10 && gg == 20 && b == 30, "L-shape colour uniform");
  }
  gfx_free(&g);
}

// Oracle: clearRect's packing quirk (see gfx.c's comment on gfx_clear_rect).
static void test_clear_rect_uses_stale_packed_color(void) {
  Graphics2D g;
  gfx_init(&g, 800, 450);
  gfx_begin(&g);
  gfx_set_color(&g, 9, 8, 7);
  gfx_clear_rect(&g, 0, 0, 10, 10);
  CHECK(g.count == 6, "clearRect vertex count");
  int32_t r, gg, b, a;
  for (int i = 0; i < 6; i++) {
    gfx_color_at(&g, i, &r, &gg, &b, &a);
    char label[32];
    snprintf(label, sizeof(label), "clearRect colour v%d", i);
    CHECK(r == 9 && gg == 8 && b == 7 && a == 255, label);
  }
  gfx_free(&g);
}

// gfx_draw_image is new native functionality (see gfx.h's own doc
// comment -- no browser API to port a texture pipeline from), so there
// is no oracle to run against; this checks the command-list bookkeeping
// itself, which is what actually matters here: flat-coloured batches
// and image draws must interleave in submission order (no depth buffer,
// see this file's own top comment on that constraint) rather than being
// sorted into two separate "all triangles then all images" passes.
static void test_draw_image_interleaves_with_batches(void) {
  Graphics2D g;
  gfx_init(&g, 800, 450);
  gfx_begin(&g);
  gfx_set_color(&g, 1, 0, 0);
  gfx_fill_rect(&g, 0, 0, 5, 5); // 6 verts, batch A
  gfx_set_composite(&g, 0.5f);
  gfx_draw_image(&g, 42, 10, 20, 30, 40);
  gfx_set_composite(&g, 1.0f);
  gfx_set_color(&g, 2, 0, 0);
  gfx_fill_rect(&g, 0, 0, 5, 5); // 6 verts, NOT yet flushed into a cmd

  CHECK(g.count == 12, "12 verts total (two 6-vert rects)");
  CHECK(g.cmd_count == 2, "batch-before-image + image = 2 commands (trailing batch stays unflushed)");
  CHECK(g.flushed == 6, "flushed covers only the pre-image batch");

  GfxDrawCmd batch = g.cmds[0];
  CHECK(batch.image_id == -1 && batch.vert_start == 0 && batch.vert_count == 6, "cmd0 is the pre-image batch");

  GfxDrawCmd img = g.cmds[1];
  CHECK(img.image_id == 42, "cmd1 image_id");
  CHECK(img.x == 10.0f && img.y == 20.0f && img.w == 30.0f && img.h == 40.0f, "cmd1 position/size");
  CHECK(img.alpha == 0.5f, "cmd1 captured the composite alpha at call time, not the later 1.0");

  gfx_free(&g);
}

// A frame with only images and no flat-coloured primitives at all still
// needs exactly one command per image, no spurious empty batch commands.
static void test_draw_image_only_no_batches(void) {
  Graphics2D g;
  gfx_init(&g, 800, 450);
  gfx_begin(&g);
  gfx_draw_image(&g, 1, 0, 0, 10, 10);
  gfx_draw_image(&g, 2, 10, 0, 10, 10);
  CHECK(g.count == 0, "no vertices from image-only draws");
  CHECK(g.cmd_count == 2, "one command per image, no empty batch commands");
  CHECK(g.cmds[0].image_id == 1 && g.cmds[1].image_id == 2, "image ids in submission order");
  gfx_free(&g);
}

int main(void) {
  test_convex_quad_fan();
  test_color_per_vertex();
  test_submission_order();
  test_outline_triangle_count();
  test_zero_length_segment_dropped();
  test_concave_trapezoid_fill();
  test_clear_rect_uses_stale_packed_color();
  test_draw_image_interleaves_with_batches();
  test_draw_image_only_no_batches();
  if (failures == 0) {
    printf("all tests passed\n");
    return 0;
  }
  fprintf(stderr, "%d failure(s)\n", failures);
  return 1;
}
