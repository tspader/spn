#include "spn.h"

void build(spn_build_ctx_t* ctx) { (void)ctx; }

void package(spn_build_ctx_t* ctx) {
  spn_copy(ctx, SPN_DIR_SOURCE, "spn_net.h", SPN_DIR_INCLUDE, "");
}
