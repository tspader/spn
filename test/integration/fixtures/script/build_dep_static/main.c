#include "build_dep_static_value.h"

int main(void) {
  _Static_assert(BUILD_DEP_STATIC_VALUE == 78, "BUILD_DEP_STATIC_VALUE should be 78");
  return 0;
}
