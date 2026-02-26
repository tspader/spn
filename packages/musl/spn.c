#include "spn.h"

s32 run_autoconf(spn_node_ctx_t* ctx) {
  spn_autoconf(spn_node_ctx_get_build(ctx));
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

void package(spn_build_ctx_t* dep) {
  spn_make_t* make = spn_make_new(dep);
  spn_make_add_target(make, "install");
  spn_make_run(make);
}
