#include "spn.h"

SPN_EXPORT
s32 gen(spn_t* spn) {
  return 0;
}

SPN_EXPORT
spn_err_t configure(spn_t* spn, spn_config_t* config) {
  spn_node_t* node = spn_add_node(config, "gen");
  spn_node_set_fn(node, "gen");
  spn_node_add_output_dir(node, spn_get_dir(spn, SPN_DIR_INCLUDE));
  return SPN_OK;
}
