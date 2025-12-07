#include "spn.h"
#include "stdio.h"

void package(spn_pkg_ctx_t* dep) {
  spn_copy(dep, SPN_DIR_SOURCE, "sp.h", SPN_DIR_INCLUDE, "");
}
