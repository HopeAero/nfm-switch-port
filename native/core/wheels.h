// ports web/Wheels.js
//
// Wheel geometry builder: wheels_set_rims() sets rim colour/size/depth,
// wheels_make() emits 19 Plane objects forming a wheel's hub face, 6 rim
// spoke faces, and 12 tyre side-panels. Called once per wheel at car-load
// time (native/core/cont_o.c's #initBuf port).
#ifndef NFM_WHEELS_H
#define NFM_WHEELS_H

#include <stdint.h>
#include "medium.h"
#include "trackers.h"
#include "plane.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int32_t ground;
  int32_t mast;
  int32_t sparkat;
  int32_t rc[3];
  float size;
  float depth;
} Wheels;

void wheels_init(Wheels *w);
void wheels_set_rims(Wheels *w, int32_t n, int32_t n2, int32_t n3, int32_t n4, int32_t n5);

/**
 * Writes 19 Planes into array[n..n+18] (array must have room; ContO's
 * #initBuf port increments its own plane count by 19 after calling this,
 * matching web/ContO.js). Each Plane in the slice must already be
 * uninitialised (plane_init is called on it here) or the caller must have
 * freed any prior contents first -- this does not free anything.
 */
void wheels_make(Wheels *w, Medium *m, Trackers *t, Plane *array, int32_t n,
                  int32_t n2, int32_t n3, int32_t n4, int32_t n5, int32_t n6,
                  int32_t n7, int32_t n8);

#ifdef __cplusplus
}
#endif

#endif
