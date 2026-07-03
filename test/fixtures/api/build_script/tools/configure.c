#include "spn.h"

s32 gen_version(spn_t* spn, spn_node_ctx_t* ctx) {
  spn_write_file(spn, "version.h", "#define BUILD_SCRIPT_VERSION 69\n");
  return 0;
}

__attribute__((export_name("configure")))
spn_err_t configure(spn_t* spn) {
  spn_target_t* target = spn_get_target(spn, "build_script");
  if (!target) return SPN_ERROR;
  spn_target_add_include(target, spn_get_dir(spn, SPN_DIR_WORK));

  spn_node_t* gen = spn_add_node((spn_config_t*)spn, "gen_version");
  spn_node_set_fn(gen, gen_version);
  spn_node_add_output(gen, spn_get_subdir(spn, SPN_DIR_WORK, "version.h"));

  return SPN_OK;
}
