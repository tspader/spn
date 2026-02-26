#include "spn.h"

static s32 order[4];
static s32 order_idx = 0;

s32 step1_fn(spn_node_ctx_t* ctx) {
  order[order_idx++] = 1;
  spn_log(spn_node_ctx_get_build(ctx), "step1");
  return 0;
}

s32 step2_fn(spn_node_ctx_t* ctx) {
  order[order_idx++] = 2;
  spn_log(spn_node_ctx_get_build(ctx), "step2");
  return 0;
}

s32 step3_fn(spn_node_ctx_t* ctx) {
  order[order_idx++] = 3;
  spn_log(spn_node_ctx_get_build(ctx), "step3");
  return 0;
}

s32 final_fn(spn_node_ctx_t* ctx) {
  order[order_idx++] = 4;
  spn_log(spn_node_ctx_get_build(ctx), "final: writing result.h");
  spn_write_file(spn_node_ctx_get_build(ctx), "result.h",
    "#ifndef RESULT_H\n"
    "#define RESULT_H\n"
    "#define CHAIN_COMPLETE 1\n"
    "#endif\n"
  );
  return 0;
}

void configure(spn_build_ctx_t* ctx) {
  spn_add_include(ctx, SPN_DIR_WORK, "");

  spn_node_t* step1 = spn_add_node(ctx, "step1");
  spn_node_set_fn(step1, step1_fn);

  spn_node_t* step2 = spn_add_node(ctx, "step2");
  spn_node_set_fn(step2, step2_fn);
  spn_node_link(step1, step2);

  spn_node_t* step3 = spn_add_node(ctx, "step3");
  spn_node_set_fn(step3, step3_fn);
  spn_node_link(step2, step3);

  spn_node_t* final = spn_add_node(ctx, "final");
  spn_node_set_fn(final, final_fn);
  spn_node_add_output(final, spn_get_subdir(ctx, SPN_DIR_WORK, "result.h"));
  spn_node_link(step3, final);
}

void package(spn_build_ctx_t* ctx) { (void)ctx; }
