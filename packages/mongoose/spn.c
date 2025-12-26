#include "spn.h"

void package(spn_pkg_ctx_t* dep) {
  spn_copy(dep, SPN_DIR_SOURCE, "mongoose.h", SPN_DIR_INCLUDE, "");
  spn_copy(dep, SPN_DIR_SOURCE, "mongoose.c", SPN_DIR_INCLUDE, "");
}
