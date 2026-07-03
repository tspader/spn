#include "spn.h"

SPN_EXPORT
spn_err_t configure(spn_t* spn) {
  spn_config_t* config = (spn_config_t*)spn;
  (void)config;
  spn_log(spn, "spn-script-probe-log");
  return SPN_OK;
}
