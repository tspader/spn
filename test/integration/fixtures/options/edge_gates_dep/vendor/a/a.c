#include "a.h"

#ifdef A_X
#include "b.h"
#endif

int a_value(void) {
#ifdef A_X
  return b_value();
#else
  return 0;
#endif
}
