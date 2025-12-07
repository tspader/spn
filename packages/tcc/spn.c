#include "spn.h"

void build(spn_pkg_ctx_t* dep) {
  spn_autoconf_t* ac = spn_autoconf_new(dep);
  if (spn_dep_get_libc(dep) == SPN_LIBC_MUSL) {
    spn_autoconf_add_flag(ac, "--config-musl");
  }

  spn_autoconf_run(ac);
  spn_make(dep);
}

void package(spn_pkg_ctx_t* dep) {
  spn_make_t* make = spn_make_new(dep);
  spn_make_add_target(make, "install");
  spn_make_run(make);
}
