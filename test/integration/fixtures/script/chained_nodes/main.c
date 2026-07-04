#include "final.h"

#include <stdio.h>

int main(void) {
  printf("Phase1 value: %d\n", PHASE1_VALUE);
  printf("Phase2 value: %d\n", PHASE2_VALUE);

  _Static_assert(PHASE1_VALUE == 100, "PHASE1_VALUE should be 100");
  _Static_assert(PHASE2_VALUE == 150, "PHASE2_VALUE should be 150");

  return 0;
}
