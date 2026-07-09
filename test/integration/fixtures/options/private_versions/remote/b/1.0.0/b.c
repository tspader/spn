#include "b.h"

int b_value(void) {
#ifdef B_X
  return 2;
#else
  return 1;
#endif
}
