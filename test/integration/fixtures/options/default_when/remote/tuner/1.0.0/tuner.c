#include "tuner.h"

int tuner_defaults_ok(void) {
#if defined(TUNER_MATCH) && !defined(TUNER_ELSE)
  return 1;
#else
  return 0;
#endif
}
