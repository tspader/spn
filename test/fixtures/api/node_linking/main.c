#include "generated.h"

#include <stdio.h>

int main(void) {
  printf("Setup complete: %d\n", SETUP_COMPLETE);
  printf("Generated value: %d\n", GENERATED_VALUE);

  _Static_assert(SETUP_COMPLETE == 1, "SETUP_COMPLETE should be 1");
  _Static_assert(GENERATED_VALUE == 42, "GENERATED_VALUE should be 42");

  return 0;
}
