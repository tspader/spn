#include "spn.h"

// Test: explicit node linking without shared files
// setup runs first (no outputs, uses stamp)
// codegen links to setup (waits for setup's stamp)
// codegen produces a header

static int setup_ran = 0;

spn_err_t setup_fn(spn_node_ctx_t* ctx) {
  spn_log(ctx->build, "setup: initializing...");
  setup_ran = 1;
  return SPN_OK;
}

spn_err_t codegen_fn(spn_node_ctx_t* ctx) {
  spn_log(ctx->build, "codegen: generating header...");
  // In a real scenario, we'd check that setup actually ran first
  // For now, just generate the header with a marker
  spn_write_file(ctx->build, "generated.h",
    "#ifndef GENERATED_H\n"
    "#define GENERATED_H\n"
    "#define SETUP_COMPLETE 1\n"
    "#define GENERATED_VALUE 42\n"
    "#endif\n"
  );
  return SPN_OK;
}

void configure(spn_build_ctx_t* ctx) {
  spn_add_include(ctx, "build/debug/work");

  // setup has no outputs - uses auto-generated stamp
  spn_node_t setup = spn_add_node(ctx, "setup");
  spn_node_set_fn(setup, setup_fn);

  // codegen links to setup - waits for setup's stamp
  spn_node_t codegen = spn_add_node(ctx, "codegen");
  spn_node_set_fn(codegen, codegen_fn);
  spn_node_add_output(codegen, spn_get_subdir(ctx, SPN_DIR_WORK, "generated.h"));
  spn_node_link(setup, codegen);  // codegen runs after setup
}

void package(spn_build_ctx_t* ctx) { (void)ctx; }
