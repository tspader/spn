#include "spn.h"

void configure(spn_build_ctx_t* b) {
  spn_pkg_t* pkg = spn_get_pkg(b);
  spn_pkg_add_system_dep(pkg, "m");
}
