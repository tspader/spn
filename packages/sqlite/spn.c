#include "spn.h"

s32 run_autoconf(spn_node_ctx_t* ctx) {
  spn_autoconf_t* ac = spn_autoconf_new(spn_node_ctx_get_build(ctx));
  spn_autoconf_add_flag(ac, "--disable-tcl");
  spn_autoconf_run(ac);
  return 0;
}

s32 run_make(spn_node_ctx_t* ctx) {
  spn_make(spn_node_ctx_get_build(ctx));
  return 0;
}

s32 run_install(spn_node_ctx_t* ctx) {
  spn_make_t* make = spn_make_new(spn_node_ctx_get_build(ctx));
  spn_make_add_target(make, "install");
  spn_make_run(make);
  return 0;
}

void configure(spn_build_ctx_t* ctx) {
  spn_node_t* autoconf = spn_add_node(ctx, "autoconf");
  spn_node_set_fn(autoconf, run_autoconf);

  spn_node_t* make = spn_add_node(ctx, "make");
  spn_node_set_fn(make, run_make);
  spn_node_link(autoconf, make);

  spn_node_t* install = spn_add_node(ctx, "install");
  spn_node_set_fn(install, run_install);
  spn_node_link(make, install);
}

void package(spn_build_ctx_t* ctx) { (void)ctx; }
