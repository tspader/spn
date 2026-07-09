#include "b.h"

int b_value(void) {
#ifdef A_X
  return 2;
#else
  return 1;
#endif
}
