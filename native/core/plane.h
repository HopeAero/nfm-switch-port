// ports web/Plane.js
//
// One face (a polygon) of a car or track object. Owns its own colour/damage/
// shard-chip animation state; d() draws it, s() draws its ground shadow.
//
// PAINTER'S ALGORITHM: faces are submitted in the order plane_d/plane_s call
// gfx_fillPolygon/gfx_drawPolygon. There is no depth buffer. Never reorder
// them -- see native/core/gfx.h's banner and AGENTS.md.
#ifndef NFM_PLANE_H
#define NFM_PLANE_H

#include <stdint.h>
#include <stdbool.h>
#include "medium.h"
#include "trackers.h"

#ifdef __cplusplus
extern "C" {
#endif

struct Graphics2D;

typedef struct {
  int32_t c[3];
  int32_t oc[3];
  float hsb[3];
  int32_t glass;
  int32_t gr;
  int32_t fs;
  int32_t disline;
  bool road;
  bool solo;
  int32_t light;
  int32_t master;
  int32_t wx, wz, wy;
  float deltaf;
  float projf;
  int32_t av;
  int32_t bfase;
  bool nocol;
  int32_t chip;
  float ctmag;
  int32_t cxz, cxy, czy;
  int32_t cox[3], coz[3], coy[3];
  int32_t dx, dy, dz;
  int32_t vx, vy, vz;
  int32_t embos;
  int32_t typ;
  int32_t pa, pb;
  int32_t flx;
  int32_t colnum;

  Medium *m;
  Trackers *t;

  // n is MUTABLE at draw time (plane_d can grow it to 12 or 20 for
  // `master` objects -- see plane_d's own comment), but the backing arrays
  // are sized once, at construction, to the `n` the caller passed in. The
  // caller (CarDefine.js's equivalent, not yet ported) is responsible for
  // passing an n large enough for any master-object growth, exactly as the
  // Java relies on the array the constructor is handed being sized that
  // way -- this struct does not defend against a caller getting that wrong,
  // same as the JS doesn't.
  int32_t n;  // <= PLANE_MAX_N, see below
  int32_t cap; // allocated capacity of ox/oy/oz, i.e. the constructor's n
  int32_t *ox, *oz, *oy;
} Plane;

// Largest vertex count a face can have: cont_o_init_buf reads a polygon's
// points into 100-entry arrays (master growth only ever sets n to 12/20),
// and the shipped models top out at 48. plane_d/plane_s size their
// per-call scratch arrays with it on the stack -- they used to malloc and
// free seven (plane_d) and five (plane_s) arrays per face, tens of
// thousands of heap round trips a frame, ~20% of the race draw's CPU.
#define PLANE_MAX_N 100

/**
 * ox/oz/oy are copied in (length n); oc is copied in (length 3, and may be
 * mutated in place for gr==-15 objects -- matches the JS, which does the
 * same to its `array4` parameter). `m`/`t` are borrowed, not owned.
 */
void plane_init(Plane *p, Medium *m, Trackers *t, const int32_t *ox, const int32_t *oz,
                 const int32_t *oy, int32_t n, int32_t *oc, int32_t glass, int32_t gr,
                 int32_t fs, int32_t wx, int32_t wy, int32_t wz, int32_t disline,
                 int32_t bfase, bool road, int32_t light, bool solo);

void plane_free(Plane *p);

void plane_deltafntyp(Plane *p);
void plane_loadprojf(Plane *p);

/** Rotate a point set about (n,n2) by n3 degrees -- identical math to
 * medium_rot (Plane.rot() calls this.m.cos/sin, same as Medium's own rot()
 * does), so this just delegates rather than reimplementing it. */
static inline void plane_rot(Plane *p, int32_t *array, int32_t *array2, int32_t n, int32_t n2, float n3, int32_t n4) {
  medium_rot(p->m, array, array2, n, n2, n3, n4);
}

/** Perspective projection -- identical math to medium_xs/ys, delegates. */
static inline int32_t plane_xs(Plane *p, int32_t n, int32_t cz) { return medium_xs(p->m, n, cz); }
static inline int32_t plane_ys(Plane *p, int32_t n, int32_t cz) { return medium_ys(p->m, n, cz); }

int32_t plane_spy(Plane *p, int32_t n, int32_t n2);

/**
 * Draw one face. Parameter names are procyon's; positionally:
 * (g, x, y, z, cxz, xy, zy, wxRot, wzRot, farAway, objDist).
 */
void plane_d(Plane *p, struct Graphics2D *g, int32_t n, int32_t n2, int32_t n3, int32_t cxz,
             int32_t n4, int32_t n5, int32_t n6, int32_t n7, bool b, int32_t n8);

/** Shadow projection onto the track. */
void plane_s(Plane *p, struct Graphics2D *g, int32_t n, int32_t n2, int32_t n3, int32_t n4,
             int32_t n5, int32_t n6, int32_t n7);

#ifdef __cplusplus
}
#endif

#endif
