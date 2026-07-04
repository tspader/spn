#include "spn.h"
#include "local.h"
#include "probe.h"

#ifndef LOCAL_VALUE
#error "local.h from [package.build] include was not on the include path"
#endif

#ifndef PROBE_VALUE
#error "probe.h from the build dep was not on the include path"
#endif

SPN_EXPORT s32 gen_version(spn_t* spn, spn_node_ctx_t* ctx) {
  spn_write_file(spn, "version.h", "#define BUILD_SCRIPT_VERSION 69\n");
  return 0;
}

SPN_EXPORT s32 package(spn_t* spn, spn_node_ctx_t* ctx) {
  if (spn_copy(spn, SPN_DIR_WORK, "version.h", SPN_DIR_INCLUDE, "version.h")) {
    return 1;
  }

  return 0;
}
