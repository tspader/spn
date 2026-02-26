#include "spn.h"

static int setup_ran = 0;

s32 setup_fn(spn_node_ctx_t* ctx) {
  spn_log(spn_node_ctx_get_build(ctx), "setup: initializing...");
  setup_ran = 1;
  return 0;
}

s32 codegen_fn(spn_node_ctx_t* ctx) {
  spn_log(spn_node_ctx_get_build(ctx), "codegen: generating header...");
  spn_write_file(spn_node_ctx_get_build(ctx), "generated.h",
    "#ifndef GENERATED_H\n"
    "#define GENERATED_H\n"
    "#define SETUP_COMPLETE 1\n"
    "#define GENERATED_VALUE 42\n"
    "#endif\n"
  );
  return 0;
}

void configure(spn_build_ctx_t* ctx) {
  spn_add_include(ctx, SPN_DIR_WORK, "");

  spn_node_t* setup = spn_add_node(ctx, "setup");
  spn_node_set_fn(setup, setup_fn);

  spn_node_t* codegen = spn_add_node(ctx, "codegen");
  spn_node_set_fn(codegen, codegen_fn);
  spn_node_add_output(codegen, spn_get_subdir(ctx, SPN_DIR_WORK, "generated.h"));
  spn_node_link(setup, codegen);
}

void package(spn_build_ctx_t* ctx) { (void)ctx; }
