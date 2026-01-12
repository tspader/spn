#include "spn.h"

void package(spn_build_ctx_t* dep) {
  spn_copy(dep, SPN_DIR_SOURCE, "sp.h", SPN_DIR_INCLUDE, "");
}
