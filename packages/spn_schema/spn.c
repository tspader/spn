#include "spn.h"

void package(spn_build_ctx_t* ctx) {
  spn_copy(ctx, SPN_DIR_SOURCE, "spn_schema.h", SPN_DIR_INCLUDE, "");
}
