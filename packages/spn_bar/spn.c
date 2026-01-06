#include "spn.h"

void build(spn_build_ctx_t* dep) {
}

void package(spn_build_ctx_t* dep) {
  spn_copy(dep, SPN_DIR_SOURCE, "test/ps.c", SPN_DIR_INCLUDE, "");
}
