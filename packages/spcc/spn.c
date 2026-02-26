#include "spn.h"

s32 run_autoconf(spn_node_ctx_t* ctx) {
  spn_build_ctx_t* build = spn_node_ctx_get_build(ctx);
  spn_profile_t* profile = spn_get_profile(build);

  spn_autoconf_t* ac = spn_autoconf_new(build);
  if (spn_profile_get_libc(profile) == SPN_LIBC_MUSL) {
    spn_autoconf_add_flag(ac, "--config-musl");
  }

  spn_autoconf_run(ac);
  return 0;
}

s32 run_make(spn_node_ctx_t* ctx) {
  spn_make(spn_node_ctx_get_build(ctx));
  return 0;
}

void configure(spn_build_ctx_t* ctx) {
  spn_node_t* autoconf = spn_add_node(ctx, "autoconf");
  spn_node_set_fn(autoconf, run_autoconf);

  spn_node_t* make = spn_add_node(ctx, "make");
  spn_node_set_fn(make, run_make);
  spn_node_link(autoconf, make);
}

void package(spn_build_ctx_t* ctx) {
  spn_make_t* make = spn_make_new(ctx);
  spn_make_add_target(make, "install");
  spn_make_run(make);
}
