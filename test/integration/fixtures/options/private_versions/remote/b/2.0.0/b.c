#include "b.h"

int b_value(void) {
#ifdef B_X
  return 4;
#else
  return 3;
#endif
}
