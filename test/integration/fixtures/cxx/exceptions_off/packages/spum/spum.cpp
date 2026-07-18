#include "spum.h"

#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
#error "cxx.exceptions = false was not applied"
#endif

extern "C" int spum_value(void) {
  return 69;
}
