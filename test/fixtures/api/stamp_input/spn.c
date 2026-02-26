#include "spn.h"

s32 gen_data(spn_node_ctx_t* ctx) {
  spn_log(spn_node_ctx_get_build(ctx), "gen_data: creating data.txt");
  spn_write_file(spn_node_ctx_get_build(ctx), "data.txt", "hello world");
  return 0;
}

s32 validate(spn_node_ctx_t* ctx) {
  spn_log(spn_node_ctx_get_build(ctx), "validate: checking data.txt exists (stamp-only, no output)");
  return 0;
}

s32 finalize(spn_node_ctx_t* ctx) {
  spn_log(spn_node_ctx_get_build(ctx), "finalize: creating header after validation stamp");
  spn_write_file(spn_node_ctx_get_build(ctx), "validated.h",
    "#ifndef VALIDATED_H\n"
    "#define VALIDATED_H\n"
    "#define VALIDATION_PASSED 1\n"
    "#endif\n"
  );
  return 0;
}

void configure(spn_build_ctx_t* ctx) {
  spn_add_include(ctx, SPN_DIR_WORK, "");

  const c8* data_txt = spn_get_subdir(ctx, SPN_DIR_WORK, "data.txt");
  const c8* validated_h = spn_get_subdir(ctx, SPN_DIR_WORK, "validated.h");

  spn_node_t* gen = spn_add_node(ctx, "gen_data");
  spn_node_set_fn(gen, gen_data);
  spn_node_add_output(gen, data_txt);

  spn_node_t* validate_node = spn_add_node(ctx, "validate");
  spn_node_set_fn(validate_node, validate);
  spn_node_add_input(validate_node, data_txt);

  spn_node_t* final = spn_add_node(ctx, "finalize");
  spn_node_set_fn(final, finalize);
  spn_node_add_output(final, validated_h);
  spn_node_link(validate_node, final);
}

void package(spn_build_ctx_t* ctx) { (void)ctx; }
