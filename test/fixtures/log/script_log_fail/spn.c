#include "spn.h"

s32 fail_node(spn_t* spn, spn_node_ctx_t* ctx) {
  return 1;
}

spn_err_t configure(spn_t* spn, spn_config_t* config) {
  spn_log(spn, "spn-script-probe-fail");

  spn_node_t* node = spn_add_node(config, "fail_node");
  spn_node_set_fn(node, fail_node);
  return SPN_OK;
}
