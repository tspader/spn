#include "spn.h"

void configure(spn_build_ctx_t* b) {
  spn_add_define(b, "PKG_DEFINE");

  spn_target_t* target = spn_get_target(b, "main");
  spn_target_add_define(target, "TARGET_DEFINE");
}
