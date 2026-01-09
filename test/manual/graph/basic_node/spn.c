#include "spn.h"

// Test: basic node that generates a header file
// The main.c will fail to compile if this doesn't run

spn_err_t generate_header(spn_node_ctx_t* ctx) {
  spn_log(ctx->build, "generating version.h...");
  spn_write_file(ctx->build, "version.h", 
    "#ifndef VERSION_H\n"
    "#define VERSION_H\n"
    "#define VERSION_MAJOR 1\n"
    "#define VERSION_MINOR 2\n"
    "#define VERSION_PATCH 3\n"
    "#endif\n"
  );
  return SPN_OK;
}

void configure(spn_build_ctx_t* ctx) {
  // Add work dir to include path so generated headers are found
  // Use relative path since spn_add_include joins with project dir
  spn_add_include(ctx, "build/debug/work");
  
  // Add nodes during configure - this is when the build graph is constructed
  spn_node_t gen = spn_add_node(ctx, "gen_version");
  spn_node_set_fn(gen, generate_header);
  
  // Output the generated header so the graph knows about it
  const c8* output = spn_get_subdir(ctx, SPN_DIR_WORK, "version.h");
  spn_node_add_output(gen, output);
}

void package(spn_build_ctx_t* ctx) {
  // Copy generated header to include dir
  spn_copy(ctx, SPN_DIR_WORK, "version.h", SPN_DIR_INCLUDE, "");
}
