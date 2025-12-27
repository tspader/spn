#include "spn.h"

void build(spn_build_ctx_t* dep) {
}

void package(spn_build_ctx_t* dep) {
  spn_copy(dep, SPN_DIR_SOURCE, "argparse.h", SPN_DIR_INCLUDE, "");
}
