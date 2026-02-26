#include "spn.h"

s32 gen_info(spn_node_ctx_t* ctx) {
  const spn_build_ctx_t* log_dep = spn_get_dep(spn_node_ctx_get_build(ctx), "spum");

  if (log_dep) {
    const c8* log_inc = spn_get_dir(log_dep, SPN_DIR_INCLUDE);
    spn_log(spn_node_ctx_get_build(ctx), "found spum include dir");
    (void)log_inc;

    spn_write_file(spn_node_ctx_get_build(ctx), "dep_info.h",
      "#ifndef DEP_INFO_H\n"
      "#define DEP_INFO_H\n"
      "#define HAS_LOG_DEP 1\n"
      "#endif\n"
    );
  }
  else {
    spn_log(spn_node_ctx_get_build(ctx), "WARNING: spum dependency not found");
    spn_write_file(spn_node_ctx_get_build(ctx), "dep_info.h",
      "#ifndef DEP_INFO_H\n"
      "#define DEP_INFO_H\n"
      "#define HAS_LOG_DEP 0\n"
      "#endif\n"
    );
  }

  return 0;
}

s32 setup_phase(spn_node_ctx_t* ctx) {
  spn_log(spn_node_ctx_get_build(ctx), "setup_phase: running setup...");
  return 0;
}

void configure(spn_build_ctx_t* ctx) {
  spn_add_include(ctx, SPN_DIR_WORK, "");

  spn_node_t* setup = spn_add_node(ctx, "setup");
  spn_node_set_fn(setup, setup_phase);

  spn_node_t* info = spn_add_node(ctx, "gen_info");
  spn_node_set_fn(info, gen_info);
  spn_node_add_output(info, spn_get_subdir(ctx, SPN_DIR_WORK, "dep_info.h"));
  spn_node_link(setup, info);
}

void package(spn_build_ctx_t* ctx) { (void)ctx; }
