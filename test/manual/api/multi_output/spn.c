#include "spn.h"

spn_err_t gen_all_headers(spn_node_ctx_t* ctx) {
  spn_log(ctx->build, "gen_all_headers: creating 3 headers from single node");

  spn_write_file(ctx->build, "types.h",
    "#ifndef TYPES_H\n"
    "#define TYPES_H\n"
    "typedef int my_int_t;\n"
    "typedef float my_float_t;\n"
    "#endif\n"
  );

  spn_write_file(ctx->build, "constants.h",
    "#ifndef CONSTANTS_H\n"
    "#define CONSTANTS_H\n"
    "#define MAX_SIZE 1024\n"
    "#define MIN_SIZE 16\n"
    "#endif\n"
  );

  spn_write_file(ctx->build, "macros.h",
    "#ifndef MACROS_H\n"
    "#define MACROS_H\n"
    "#define SQUARE(x) ((x) * (x))\n"
    "#define CUBE(x) ((x) * (x) * (x))\n"
    "#endif\n"
  );

  return SPN_OK;
}

spn_err_t gen_combined(spn_node_ctx_t* ctx) {
  spn_log(ctx->build, "gen_combined: aggregating all headers");
  spn_write_file(ctx->build, "all.h",
    "#ifndef ALL_H\n"
    "#define ALL_H\n"
    "#include \"types.h\"\n"
    "#include \"constants.h\"\n"
    "#include \"macros.h\"\n"
    "#endif\n"
  );
  return SPN_OK;
}

void configure(spn_build_ctx_t* ctx) {
  spn_add_include(ctx, SPN_DIR_WORK, "");

  const c8* types_h = spn_get_subdir(ctx, SPN_DIR_WORK, "types.h");
  const c8* constants_h = spn_get_subdir(ctx, SPN_DIR_WORK, "constants.h");
  const c8* macros_h = spn_get_subdir(ctx, SPN_DIR_WORK, "macros.h");
  const c8* all_h = spn_get_subdir(ctx, SPN_DIR_WORK, "all.h");

  spn_node_t gen = spn_add_node(ctx, "gen_all_headers");
  spn_node_set_fn(gen, gen_all_headers);
  spn_node_add_output(gen, types_h);
  spn_node_add_output(gen, constants_h);
  spn_node_add_output(gen, macros_h);

  spn_node_t combined = spn_add_node(ctx, "gen_combined");
  spn_node_set_fn(combined, gen_combined);
  spn_node_add_input(combined, types_h);
  spn_node_add_input(combined, constants_h);
  spn_node_add_input(combined, macros_h);
  spn_node_add_output(combined, all_h);
}

void package(spn_build_ctx_t* ctx) { (void)ctx; }
