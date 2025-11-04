#include "spn/spn.h"

void build(spn_dep_context_t* dep) {
}

void package(spn_dep_context_t* dep) {
  spn_copy(dep, SPN_DIR_SOURCE, "toml.h", SPN_DIR_STORE, "");
}
