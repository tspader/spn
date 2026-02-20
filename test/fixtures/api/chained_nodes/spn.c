#include "spn.h"

spn_err_t phase1_fn(spn_node_ctx_t* ctx) {
  spn_log(ctx->build, "phase1: generating intermediate.h...");
  spn_write_file(ctx->build, "intermediate.h",
    "#ifndef INTERMEDIATE_H\n"
    "#define INTERMEDIATE_H\n"
    "#define PHASE1_VALUE 100\n"
    "#endif\n"
  );
  return SPN_OK;
}

spn_err_t phase2_fn(spn_node_ctx_t* ctx) {
  spn_log(ctx->build, "phase2: generating final.h...");
  spn_write_file(ctx->build, "final.h",
    "#ifndef FINAL_H\n"
    "#define FINAL_H\n"
    "#include \"intermediate.h\"\n"
    "#define PHASE2_VALUE (PHASE1_VALUE + 50)\n"
    "#endif\n"
  );
  return SPN_OK;
}

void configure(spn_build_ctx_t* ctx) {
  spn_add_include(ctx, SPN_DIR_WORK, "");

  const c8* intermediate = spn_get_subdir(ctx, SPN_DIR_WORK, "intermediate.h");
  const c8* final = spn_get_subdir(ctx, SPN_DIR_WORK, "final.h");

  spn_node_t phase1 = spn_add_node(ctx, "phase1");
  spn_node_set_fn(phase1, phase1_fn);
  spn_node_add_output(phase1, intermediate);

  spn_node_t phase2 = spn_add_node(ctx, "phase2");
  spn_node_set_fn(phase2, phase2_fn);
  spn_node_add_input(phase2, intermediate);
  spn_node_add_output(phase2, final);
}

void package(spn_build_ctx_t* ctx) { (void)ctx; }
