#include "a.h"

int a_value(void) {
  int v = 0;
#ifdef A_X
  v += 1;
#endif
#ifdef A_Y
  v += 2;
#endif
  return v;
}
