#include "spn.h"
#include "stdio.h"

void build(spn_build_ctx_t* spn) {
  spn_log(spn, "hello!");
}

void package(spn_pkg_ctx_t* dep) {
  spn_copy(dep, SPN_DIR_SOURCE, "sp.h", SPN_DIR_INCLUDE, "");
}
