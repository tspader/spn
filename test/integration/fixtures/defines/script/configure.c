#include "spn.h"

#if (defined(_MSC_VER) && !defined(__clang__)) != defined(SPN_BUILD_DRIVER_MSVC)
  #error SPN_BUILD_DRIVER_MSVC disagrees with _MSC_VER
#endif
#if (defined(__GNUC__) && !defined(__clang__)) != defined(SPN_BUILD_DRIVER_GCC)
  #error SPN_BUILD_DRIVER_GCC disagrees with __GNUC__
#endif
#if defined(__clang__) != (defined(SPN_BUILD_DRIVER_CLANG) || defined(SPN_BUILD_DRIVER_ZIG))
  #error SPN_BUILD_DRIVER_CLANG and SPN_BUILD_DRIVER_ZIG disagree with __clang__
#endif

__attribute__((export_name("configure")))
spn_err_t configure(spn_t* spn, spn_config_t* config) {
  return SPN_OK;
}
