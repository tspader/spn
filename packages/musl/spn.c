#include "spn.h"

void build(spn_build_ctx_t* dep) {
  spn_autoconf(dep);
  spn_make(dep);
}

void package(spn_build_ctx_t* dep) {
  spn_make_t* make = spn_make_new(dep);
  spn_make_add_target(make, "install");
  spn_make_run(make);
}
