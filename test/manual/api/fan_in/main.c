#include "combined.h"
#include <stdio.h>

int main(void) {
  printf("Alpha: %d, Beta: %d, Gamma: %d, Delta: %d\n", ALPHA, BETA, GAMMA, DELTA);
  printf("Combined: %d\n", COMBINED);

  _Static_assert(ALPHA == 1, "ALPHA should be 1");
  _Static_assert(BETA == 2, "BETA should be 2");
  _Static_assert(GAMMA == 3, "GAMMA should be 3");
  _Static_assert(DELTA == 4, "DELTA should be 4");
  _Static_assert(COMBINED == 10, "COMBINED should be 10");

  return 0;
}
