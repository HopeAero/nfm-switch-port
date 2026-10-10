// car_facts: the Gallery's lines come out of the stat tables and the rules.
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "../core/car_facts.h"

static int failures = 0;
#define CHECK(cond, msg)                         \
  do {                                           \
    if (!(cond)) {                               \
      fprintf(stderr, "FAIL: %s\n", msg);        \
      failures++;                                \
    }                                            \
  } while (0)

static CarDefine cd;
static int32_t npl[CAR_FACTS_BASE];

// Whether car `cn`'s facts hold a line containing `needle`.
static bool says(int32_t cn, const char *needle) {
  char out[16][CAR_FACT_LEN];
  const int32_t n = car_facts(&cd, cn, cn, npl, npl[cn], out, 16);
  for (int32_t i = 0; i < n; i++) {
    if (strstr(out[i], needle)) return true;
  }
  return false;
}

int main(void) {
  car_define_init(&cd);
  car_define_extended(&cd);
  for (int32_t i = 0; i < CAR_FACTS_BASE; i++) npl[i] = 100 + i;

  // Port numbers: 0-15 NFM 2's, 16-38 Extended's 0-22.
  const int32_t tesco = 16 + 22, titan = 16 + 18, damn_van = 16 + 2;
  CHECK(says(tesco, "camera pulls back 300"), "Tesco Lorry: the camera pulls back");
  CHECK(says(titan, "camera pulls back 65"), "Titan: the camera pulls back");
  CHECK(says(2, "no knockback"), "Wow Caninaro (NFM 2's 2, Extended's 25): no knockback");
  CHECK(says(damn_van, "twice as easily"), "Damn Van dents twice as easily");
  CHECK(says(13, "Classic Mode"), "M A S H E E N: Classic Mode damage");
  CHECK(says(38, "most detailed"), "the car with the most planes says so");
  CHECK(says(0, "simplest"), "the car with the fewest planes says so");
  CHECK(!says(0, "camera"), "Tornado Shark has no camera fact");

  // The heaviest car of the 39 is named the heaviest, and only it.
  int32_t heaviest = 0;
  for (int32_t i = 1; i < CAR_FACTS_BASE; i++)
    if (cd.moment[i] > cd.moment[heaviest]) heaviest = i;
  CHECK(says(heaviest, "heaviest"), "the heaviest car is named the heaviest");
  int32_t named = 0;
  for (int32_t i = 0; i < CAR_FACTS_BASE; i++) named += says(i, "heaviest");
  CHECK(named == 1, "only one car is the heaviest");

  if (failures == 0) {
    printf("all tests passed\n");
    return 0;
  }
  fprintf(stderr, "%d failure(s)\n", failures);
  return 1;
}
