#include "spn.h"

SPN_EXPORT
s32 gen_info(spn_t* spn, spn_node_ctx_t* ctx) {
  const spn_t* dep = spn_get_dep(spn, "spum");

  if (dep) {
    const c8* dep_include = spn_get_dir(dep, SPN_DIR_INCLUDE);
    spn_log(spn, "found spum include dir");
    (void)dep_include;

    spn_write_file(spn, "dep_info.h",
      "#ifndef DEP_INFO_H\n"
      "#define DEP_INFO_H\n"
      "#define HAS_LOG_DEP 1\n"
      "#endif\n"
    );
  }
  else {
    spn_log(spn, "WARNING: spum dependency not found");
    spn_write_file(spn, "dep_info.h",
      "#ifndef DEP_INFO_H\n"
      "#define DEP_INFO_H\n"
      "#define HAS_LOG_DEP 0\n"
      "#endif\n"
    );
  }

  return 0;
}

SPN_EXPORT
s32 setup_phase(spn_t* spn, spn_node_ctx_t* ctx) {
  spn_log(spn, "setup_phase: running setup...");
  return 0;
}

SPN_EXPORT
spn_err_t configure(spn_t* spn) {
  spn_config_t* config = (spn_config_t*)spn;
  (void)config;
  spn_add_include(config, spn_get_dir(spn, SPN_DIR_WORK));

  spn_node_t* setup = spn_add_node(config, "setup");
  spn_node_set_fn(setup, "setup_phase");

  spn_node_t* info = spn_add_node(config, "gen_info");
  spn_node_set_fn(info, "gen_info");
  spn_node_add_output(info, spn_get_subdir(spn, SPN_DIR_WORK, "dep_info.h"));
  spn_node_link(setup, info);
  return SPN_OK;
}
