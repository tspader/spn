#include "spn.h"

SPN_EXPORT
s32 gen_version(spn_t* spn, spn_node_ctx_t* ctx) {
  spn_write_file(spn, "version.h", "#define DEFAULT_SCRIPT_VERSION 42\n");
  return 0;
}

SPN_EXPORT
spn_err_t configure(spn_t* spn) {
  spn_target_t* target = spn_get_target(spn, "default_script");
  spn_target_add_include(target, spn_get_dir(spn, SPN_DIR_WORK));

  spn_node_t* gen = spn_add_node((spn_config_t*)spn, "gen_version");
  spn_node_set_fn(gen, "gen_version");
  spn_node_add_output(gen, spn_get_subdir(spn, SPN_DIR_WORK, "version.h"));
  return SPN_OK;
}

SPN_EXPORT
spn_err_t package(spn_t* spn) {
  if (spn_copy(spn, SPN_DIR_WORK, "version.h", SPN_DIR_INCLUDE, "version.h")) return SPN_ERROR;
  return SPN_OK;
}
