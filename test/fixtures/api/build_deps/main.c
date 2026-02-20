#include "build_dep_value.h"

#include <stdio.h>

int main(void) {
  printf("Build dep value: %d\n", BUILD_DEP_VALUE);

  _Static_assert(BUILD_DEP_VALUE == 78, "BUILD_DEP_VALUE should be 78");

  return 0;
}
