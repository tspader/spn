#include "spn.h"

s32 doit(spn_node_ctx_t* c) {
  // spn_build_ctx_t* ctx = c->build;
  // spn_profile_t* profile = spn_get_profile(ctx);
  //
  // spn_autoconf_t* ac = spn_autoconf_new(ctx);
  // if (spn_profile_get_libc(profile) == SPN_LIBC_MUSL) {
  //   spn_autoconf_add_flag(ac, "--config-musl");
  // }
  //
  // spn_autoconf_run(ac);
  // spn_make(ctx);

  return 0;
}

void configure(spn_build_ctx_t* ctx) {
  spn_node_t node = spn_add_node(ctx, "hello");
  spn_node_set_fn(node, doit);
}

void package(spn_build_ctx_t* ctx) {
  spn_make_t* make = spn_make_new(ctx);
  spn_make_add_target(make, "install");
  spn_make_run(make);
}
