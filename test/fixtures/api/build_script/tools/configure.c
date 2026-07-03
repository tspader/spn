#include "spn.h"

SPN_EXPORT spn_err_t configure(spn_t* spn, spn_config_t* config) {
  spn_target_t* target = spn_get_target(spn, "build_script");
  if (!target) return SPN_ERROR;
  spn_target_add_include(target, spn_get_dir(spn, SPN_DIR_WORK));

  spn_node_t* gen = spn_add_node(config, "gen_version");
  spn_node_set_fn(gen, "gen_version");
  spn_node_add_output(gen, spn_get_subdir(spn, SPN_DIR_WORK, "version.h"));

  return SPN_OK;
}
