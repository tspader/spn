#include "spn.h"

void build(spn_build_ctx_t* ctx) {
  spn_autoconf_t* ac = spn_autoconf_new(ctx);
  if (spn_get_libc(ctx) == SPN_LIBC_MUSL) {
    spn_autoconf_add_flag(ac, "--config-musl");
  }

  spn_autoconf_run(ac);
  spn_make(ctx);
}

void package(spn_build_ctx_t* ctx) {
  spn_make_t* make = spn_make_new(ctx);
  spn_make_add_target(make, "install");
  spn_make_run(make);
}
