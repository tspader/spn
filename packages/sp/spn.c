#include "spn.h"

void build(spn_dep_context_t* dep) {
  spn_dep_log(dep, "hello, world\n");
}

void package(spn_dep_context_t* dep) {
  spn_copy(dep, SPN_DIR_SOURCE, "sp.h", SPN_DIR_INCLUDE, "");
}
