#include "spum.h"

#if defined(_MSVC_LANG)
static_assert(_MSVC_LANG == 201402L, "cxx.standard was not applied");
#else
static_assert(__cplusplus == 201402L, "cxx.standard was not applied");
#endif

extern "C" int spum_value(void) {
  return 69;
}
