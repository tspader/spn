#include "spn.h"
#include "local.h"
#include "probe.h"

#ifndef LOCAL_VALUE
#error "local.h from [package.build] include was not on the include path"
#endif

#ifndef PROBE_VALUE
#error "probe.h from the build dep was not on the include path"
#endif

__attribute__((export_name("package")))
spn_err_t package(spn_t* spn) {
  if (spn_copy(spn, SPN_DIR_WORK, "version.h", SPN_DIR_INCLUDE, "version.h")) {
    return SPN_ERROR;
  }

  return SPN_OK;
}
