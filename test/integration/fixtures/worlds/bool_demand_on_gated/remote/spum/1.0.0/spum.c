#include "spum.h"

int spum_value(void) {
#ifdef SPUM_GRUM
  return 2;
#else
  return 1;
#endif
}
