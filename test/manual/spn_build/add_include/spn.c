#include "spn.h"

void configure(spn_build_ctx_t* b) {
  spn_pkg_t* pkg = spn_get_pkg(b);
  spn_pkg_add_include(pkg, "include/pkg");

  spn_target_t* target = spn_get_target(b, "main");
  spn_target_add_include(target, "include/target");
}
