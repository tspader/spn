#include "spn.h"

static spn_config_t* stale;

SPN_EXPORT
s32 use_stale_config(spn_t* spn, spn_node_ctx_t* ctx) {
  if (spn_add_node(stale, "smuggled")) return 1;
  return 0;
}

SPN_EXPORT
spn_err_t configure(spn_t* spn, spn_config_t* config) {
  stale = config;
  spn_node_t* node = spn_add_node(config, "stale");
  spn_node_set_fn(node, "use_stale_config");
  return SPN_OK;
}
