#include "validated.h"
#include <stdio.h>

int main(void) {
  printf("Validation passed: %d\n", VALIDATION_PASSED);
  _Static_assert(VALIDATION_PASSED == 1, "VALIDATION_PASSED should be 1");
  return 0;
}
