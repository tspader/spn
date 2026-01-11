#include "spn.h"

void configure(spn_build_ctx_t* ctx) {
}

void package(spn_build_ctx_t* ctx) {
  spn_copy(ctx, SPN_DIR_SOURCE, "spn_alloc.h", SPN_DIR_INCLUDE, "");
}
