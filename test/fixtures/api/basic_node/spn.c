#include "spn.h"

s32 generate_header(spn_node_ctx_t* ctx) {
  spn_log(spn_node_ctx_get_build(ctx), "generating version.h...");
  spn_write_file(spn_node_ctx_get_build(ctx), "version.h",
    "#ifndef VERSION_H\n"
    "#define VERSION_H\n"
    "#define VERSION_MAJOR 69\n"
    "#define VERSION_MINOR 2\n"
    "#define VERSION_PATCH 3\n"
    "#endif\n"
  );
  return 0;
}

void configure(spn_build_ctx_t* ctx) {
  spn_add_include(ctx, SPN_DIR_WORK, "");

  spn_node_t* gen = spn_add_node(ctx, "gen_version");
  spn_node_set_fn(gen, generate_header);

  spn_node_add_output(gen, spn_get_subdir(ctx, SPN_DIR_WORK, "version.h"));
}

void package(spn_build_ctx_t* ctx) {
  spn_copy(ctx, SPN_DIR_WORK, "version.h", SPN_DIR_INCLUDE, "");
}
