#include "spn.h"

spn_err_t gen_base(spn_node_ctx_t* ctx) {
  spn_log(ctx->build, "gen_base: creating base.h");
  spn_write_file(ctx->build, "base.h",
    "#ifndef BASE_H\n"
    "#define BASE_H\n"
    "#define BASE_VAL 10\n"
    "#endif\n"
  );
  return SPN_OK;
}

spn_err_t gen_left(spn_node_ctx_t* ctx) {
  spn_log(ctx->build, "gen_left: creating left.h");
  spn_write_file(ctx->build, "left.h",
    "#ifndef LEFT_H\n"
    "#define LEFT_H\n"
    "#include \"base.h\"\n"
    "#define LEFT_VAL (BASE_VAL * 2)\n"
    "#endif\n"
  );
  return SPN_OK;
}

spn_err_t gen_right(spn_node_ctx_t* ctx) {
  spn_log(ctx->build, "gen_right: creating right.h");
  spn_write_file(ctx->build, "right.h",
    "#ifndef RIGHT_H\n"
    "#define RIGHT_H\n"
    "#include \"base.h\"\n"
    "#define RIGHT_VAL (BASE_VAL * 3)\n"
    "#endif\n"
  );
  return SPN_OK;
}

spn_err_t gen_final(spn_node_ctx_t* ctx) {
  spn_log(ctx->build, "gen_final: creating final.h");
  spn_write_file(ctx->build, "final.h",
    "#ifndef FINAL_H\n"
    "#define FINAL_H\n"
    "#include \"left.h\"\n"
    "#include \"right.h\"\n"
    "#define FINAL_VAL (LEFT_VAL + RIGHT_VAL)\n"
    "#endif\n"
  );
  return SPN_OK;
}

void configure(spn_build_ctx_t* ctx) {
  spn_add_include(ctx, "build/debug/work");

  const c8* base_h = spn_get_subdir(ctx, SPN_DIR_WORK, "base.h");
  const c8* left_h = spn_get_subdir(ctx, SPN_DIR_WORK, "left.h");
  const c8* right_h = spn_get_subdir(ctx, SPN_DIR_WORK, "right.h");
  const c8* final_h = spn_get_subdir(ctx, SPN_DIR_WORK, "final.h");

  spn_node_t base = spn_add_node(ctx, "gen_base");
  spn_node_set_fn(base, gen_base);
  spn_node_add_output(base, base_h);

  spn_node_t left = spn_add_node(ctx, "gen_left");
  spn_node_set_fn(left, gen_left);
  spn_node_add_input(left, base_h);
  spn_node_add_output(left, left_h);

  spn_node_t right = spn_add_node(ctx, "gen_right");
  spn_node_set_fn(right, gen_right);
  spn_node_add_input(right, base_h);
  spn_node_add_output(right, right_h);

  spn_node_t final = spn_add_node(ctx, "gen_final");
  spn_node_set_fn(final, gen_final);
  spn_node_add_input(final, left_h);
  spn_node_add_input(final, right_h);
  spn_node_add_output(final, final_h);
}

void package(spn_build_ctx_t* ctx) { (void)ctx; }
