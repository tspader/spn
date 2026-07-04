#include "spum.h"

#ifdef __cpp_rtti
#error "cxx.rtti = false was not applied"
#endif

extern "C" int spum_value(void) {
  return 69;
}
