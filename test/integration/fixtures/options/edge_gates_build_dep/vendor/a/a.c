#include "a.h"

int a_value(void) {
#ifdef A_X
  return 1;
#else
  return 0;
#endif
}
