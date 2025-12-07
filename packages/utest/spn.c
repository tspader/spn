#include "spn.h"

void build(spn_pkg_ctx_t* dep) {
}

void package(spn_pkg_ctx_t* dep) {
  spn_copy(dep, SPN_DIR_SOURCE, "utest.h", SPN_DIR_INCLUDE, "");
}
