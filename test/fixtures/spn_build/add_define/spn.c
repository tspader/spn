#include "spn.h"

spn_err_t configure(spn_t* spn, spn_config_t* config) {
  spn_add_define(config, "PKG_DEFINE");

  spn_target_t* target = spn_get_target(spn, "main");
  spn_target_add_define(target, "TARGET_DEFINE");

  return SPN_OK;
}
