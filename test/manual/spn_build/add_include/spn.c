#include "spn.h"

void configure(spn_build_ctx_t* b) {
  spn_add_include(b, "include/pkg");

  spn_target_t* target = spn_get_target(b, "main");
  spn_target_add_include(target, "include/target");
}
