#include "spn.h"

spn_err_t gen_info(spn_node_ctx_t* ctx) {
  const spn_build_ctx_t* log_dep = spn_get_dep(ctx->build, "spn_log");

  if (log_dep) {
    const c8* log_inc = spn_get_dir(log_dep, SPN_DIR_INCLUDE);
    spn_log(ctx->build, "found spn_log include dir");
    (void)log_inc;

    spn_write_file(ctx->build, "dep_info.h",
      "#ifndef DEP_INFO_H\n"
      "#define DEP_INFO_H\n"
      "#define HAS_LOG_DEP 1\n"
      "#endif\n"
    );
  }
  else {
    spn_log(ctx->build, "WARNING: spn_log dependency not found");
    spn_write_file(ctx->build, "dep_info.h",
      "#ifndef DEP_INFO_H\n"
      "#define DEP_INFO_H\n"
      "#define HAS_LOG_DEP 0\n"
      "#endif\n"
    );
  }

  return SPN_OK;
}

spn_err_t setup_phase(spn_node_ctx_t* ctx) {
  spn_log(ctx->build, "setup_phase: running setup...");
  return SPN_OK;
}

void configure(spn_build_ctx_t* ctx) {
  spn_add_include(ctx, SPN_DIR_WORK, "");

  spn_node_t setup = spn_add_node(ctx, "setup");
  spn_node_set_fn(setup, setup_phase);

  spn_node_t info = spn_add_node(ctx, "gen_info");
  spn_node_set_fn(info, gen_info);
  spn_node_add_output(info, spn_get_subdir(ctx, SPN_DIR_WORK, "dep_info.h"));
  spn_node_link(setup, info);
}

void package(spn_build_ctx_t* ctx) { (void)ctx; }
