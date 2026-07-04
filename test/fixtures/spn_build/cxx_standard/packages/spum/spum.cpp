#include "spum.h"

static_assert(__cplusplus == 201402L, "cxx.standard was not applied");

extern "C" int spum_value(void) {
  return 69;
}
