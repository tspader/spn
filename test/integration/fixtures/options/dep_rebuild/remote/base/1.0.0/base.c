#include "base.h"

int base_value(void) {
#ifdef BASE_FAST
  return 2;
#else
  return 1;
#endif
}
