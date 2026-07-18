#include "spum.h"

#if defined(__cpp_rtti) || defined(_CPPRTTI)
#error "cxx.rtti = false was not applied"
#endif

extern "C" int spum_value(void) {
  return 69;
}
