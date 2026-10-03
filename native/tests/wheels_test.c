// Host-buildable test for native/core/wheels.c.
//
// No web/Wheels.test.js exists, so -- same fallback as trackers_test.c and
// friends -- expected values were captured by running the real
// web/Wheels.js under Node (see the commit that added this file for the
// oracle script) and are reproduced here as literals. Checked exhaustively:
// all 19 planes, every field, every coordinate matched the JS exactly on
// the first attempt -- no fix was needed here, unlike plane_test.c's
// chip-debris case.
#include <stdio.h>
#include "../core/java_compat.h"
#include "../core/medium.h"
#include "../core/trackers.h"
#include "../core/plane.h"
#include "../core/wheels.h"

static int failures = 0;

#define CHECK(cond, msg) do { \
  if (!(cond)) { fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); failures++; } \
} while (0)

static void check_plane(int i, Plane *p, int32_t n, int32_t master, int32_t gr, int32_t fs,
                         const int32_t *ox, const int32_t *oy, const int32_t *oz) {
  char label[64];
  snprintf(label, sizeof(label), "plane %d n/master/gr/fs", i);
  CHECK(p->n == n && p->master == master && p->gr == gr && p->fs == fs, label);
  snprintf(label, sizeof(label), "plane %d wx/wy/wz", i);
  CHECK(p->wx == -78 && p->wy == -30 && p->wz == 250, label);
  for (int32_t k = 0; k < n; k++) {
    snprintf(label, sizeof(label), "plane %d ox[%d]", i, k);
    CHECK(p->ox[k] == ox[k], label);
    snprintf(label, sizeof(label), "plane %d oy[%d]", i, k);
    CHECK(p->oy[k] == oy[k], label);
    snprintf(label, sizeof(label), "plane %d oz[%d]", i, k);
    CHECK(p->oz[k] == oz[k], label);
  }
}

static void test_set_rims(void) {
  Wheels w; wheels_init(&w);
  wheels_set_rims(&w, 80, 80, 80, 18, 12);
  CHECK(w.rc[0] == 80 && w.rc[1] == 80 && w.rc[2] == 80, "rc");
  CHECK(w.size == 1.7999999523162842f, "size");
  CHECK(w.depth == 1.2000000476837158f, "depth");
}

static void test_make(void) {
  nfm_set_seed(5001);
  Medium m; medium_init(&m);
  Trackers t; trackers_init(&t);
  Wheels w; wheels_init(&w);
  wheels_set_rims(&w, 80, 80, 80, 18, 12);

  Plane arr[19];
  wheels_make(&w, &m, &t, arr, 0, -150, -30, 250, 11, 180, 200, 5);
  CHECK(w.ground == 230, "ground");
  CHECK(w.sparkat == 480, "sparkat");

  int32_t ox0[20] = {-222,-222,-222,-222,-222,-222,-222,-222,-222,-222,-222,-222,-222,-222,-222,-222,-222,-222,-222,-222};
  int32_t oy0[20] = {-213,-281,-281,-213,-97,37,153,221,221,153,37,-97,-30,-14,-14,-30,-45,-45,-30,-97};
  int32_t oz0[20] = {433,317,182,66,-1,-1,66,182,317,433,501,501,268,259,241,232,241,259,268,501};
  check_plane(0, &arr[0], 20, 1, 5, 0, ox0, oy0, oz0);

  int32_t spoke_ox[3] = {-222,-222,-171};
  struct { int32_t oy[3]; int32_t oz[3]; } spokes[6] = {
    {{-30,-14,-30}, {268,259,250}},
    {{-14,-14,-30}, {259,241,250}},
    {{-14,-30,-30}, {241,232,250}},
    {{-30,-45,-30}, {232,241,250}},
    {{-45,-45,-30}, {241,259,250}},
    {{-45,-30,-30}, {259,268,250}},
  };
  for (int i = 0; i < 6; i++) {
    check_plane(1 + i, &arr[1 + i], 3, 2, 2, 0, spoke_ox, spokes[i].oy, spokes[i].oz);
  }

  int32_t tire_ox[4] = {-222,-222,-78,-78};
  struct { int32_t fs; int32_t oy[4]; int32_t oz[4]; } tires[12] = {
    {-1, {-281,-281,-281,-281}, {317,182,182,317}},
    {1,  {-213,-281,-281,-213}, {66,182,182,66}},
    {1,  {-213,-97,-97,-213},   {66,-1,-1,66}},
    {-1, {-97,37,37,-97},       {-1,-1,-1,-1}},
    {1,  {153,37,37,153},       {66,-1,-1,66}},
    {1,  {153,221,221,153},     {66,182,182,66}},
    {-1, {221,221,221,221},     {182,317,317,182}},
    {1,  {153,221,221,153},     {433,317,317,433}},
    {1,  {153,37,37,153},       {433,501,501,433}},
    {-1, {37,-97,-97,37},       {501,501,501,501}},
    {1,  {-213,-97,-97,-213},   {433,501,501,433}},
    {1,  {-213,-281,-281,-213}, {433,317,317,433}},
  };
  for (int i = 0; i < 12; i++) {
    check_plane(7 + i, &arr[7 + i], 4, 0, 5, tires[i].fs, tire_ox, tires[i].oy, tires[i].oz);
  }

  for (int i = 0; i < 19; i++) plane_free(&arr[i]);
  medium_free(&m);
}

int main(void) {
  test_set_rims();
  test_make();
  if (failures == 0) {
    printf("all tests passed\n");
    return 0;
  }
  fprintf(stderr, "%d failure(s)\n", failures);
  return 1;
}
