#include "spn.h"

void configure(spn_build_ctx_t* build) {
  spn_copy(build, SPN_DIR_SOURCE, "spum.h", SPN_DIR_INCLUDE, "");
}
