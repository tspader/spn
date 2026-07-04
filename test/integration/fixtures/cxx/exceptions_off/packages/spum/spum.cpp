#include "spum.h"

#ifdef __cpp_exceptions
#error "cxx.exceptions = false was not applied"
#endif

extern "C" int spum_value(void) {
  return 69;
}
