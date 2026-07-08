#include "wrap.h"

int wrap_value(void) {
#ifdef BASE_FAST
  return 20;
#else
  return 10;
#endif
}
