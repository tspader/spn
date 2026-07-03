#include "spn.h"

SPN_EXPORT
spn_err_t configure(spn_t* spn) {
  spn_config_t* config = (spn_config_t*)spn;
  (void)config;
  spn_add_define(config, "PKG_DEFINE");

  spn_target_t* target = spn_get_target(spn, "main");
  spn_target_add_define(target, "TARGET_DEFINE");

  return SPN_OK;
}
