#include "spn.h"

#include "spum.h"

s32 generate_build_dep_value(spn_node_ctx_t* ctx) {
  s32 value = SPUM_MAGIC + 1;
  if (value != 78) {
    spn_log(spn_node_ctx_get_build(ctx), "unexpected spum value");
    return 1;
  }

  spn_write_file(spn_node_ctx_get_build(ctx), "build_dep_value.h",
    "#ifndef BUILD_DEP_VALUE_H\n"
    "#define BUILD_DEP_VALUE_H\n"
    "#define BUILD_DEP_VALUE 78\n"
    "#endif\n"
  );

  return 0;
}

void configure(spn_build_ctx_t* ctx) {
  spn_add_include(ctx, SPN_DIR_WORK, "");

  spn_node_t* gen = spn_add_node(ctx, "generate_build_dep_value");
  spn_node_set_fn(gen, generate_build_dep_value);
  spn_node_add_output(gen, spn_get_subdir(ctx, SPN_DIR_WORK, "build_dep_value.h"));
}

void package(spn_build_ctx_t* ctx) { (void)ctx; }
