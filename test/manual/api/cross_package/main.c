#include "dep_info.h"
#include <stdio.h>

int main(void) {
  printf("Has log dependency: %d\n", HAS_LOG_DEP);
  
  _Static_assert(HAS_LOG_DEP == 1, "HAS_LOG_DEP should be 1");
  
  return 0;
}
