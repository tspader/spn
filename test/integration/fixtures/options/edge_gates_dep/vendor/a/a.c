#include "a.h"
#include "b.h"

#ifndef A_X
#error "expected A_X"
#endif

int a_value(void) {
  return b_value();
}
