#include "spn.h"

void configure(spn_build_ctx_t* b) {
  spn_pkg_t* pkg = spn_get_pkg(b);
  spn_pkg_add_define(pkg, "PKG_DEFINE");

  spn_target_t* target = spn_get_target(b, "main");
  spn_target_add_define(target, "TARGET_DEFINE");
}
