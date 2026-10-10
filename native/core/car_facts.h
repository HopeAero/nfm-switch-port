// The Gallery's "did you know" lines for a car (platform/common/gallery.inc):
// worked out from its stat table and the rules the engine applies to it,
// nothing written by hand.
#ifndef NFM_CAR_FACTS_H
#define NFM_CAR_FACTS_H

#include <stdint.h>

#include "car_define.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CAR_FACTS_BASE 39   // NFM 2's 16 and Extended's 23, port numbering
#define CAR_FACT_LEN 112

/**
 * Up to `max` lines about car slot `cn` into `out`; returns how many.
 * `ident` is the car whose rules it races by (car_identity: a new car's
 * donor, else cn). `npl` holds the 39 base cars' plane counts and `my_npl`
 * this car's. The rankings set it against the 39 base cars.
 */
int32_t car_facts(const CarDefine *cd, int32_t cn, int32_t ident, const int32_t npl[CAR_FACTS_BASE], int32_t my_npl,
                  char out[][CAR_FACT_LEN], int32_t max);

#ifdef __cplusplus
}
#endif

#endif
