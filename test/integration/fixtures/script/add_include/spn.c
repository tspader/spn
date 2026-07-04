#include "spn.h"

SPN_EXPORT
spn_err_t configure(spn_t* spn, spn_config_t* config) {
  spn_add_include(config, "include/pkg");

  spn_target_t* target = spn_get_target(spn, "main");
  spn_target_add_include(target, "include/target");

  return SPN_OK;
}
