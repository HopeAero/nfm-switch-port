// The new cars' registry and who each counts as (new_cars.h): kept apart from
// new_cars.c's loaders so the physics, the AI and the specials link without
// the model and file code.
#include "new_cars.h"

int32_t g_new_car_count = 0;
NewCarInfo g_new_cars[NEW_CARS_MAX];

int32_t car_identity(int32_t cn) {
  if (cn >= NEW_CAR_FIRST && cn < NEW_CAR_FIRST + g_new_car_count) return g_new_cars[cn - NEW_CAR_FIRST].donor;
  return cn;
}
