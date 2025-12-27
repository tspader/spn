#include "spn.h"

void package(spn_build_ctx_t* dep) {
  spn_copy(dep, SPN_DIR_SOURCE, "cJSON.h", SPN_DIR_INCLUDE, "");
  spn_copy(dep, SPN_DIR_SOURCE, "cJSON.c", SPN_DIR_INCLUDE, "");
}
