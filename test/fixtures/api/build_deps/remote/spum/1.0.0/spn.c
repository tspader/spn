#include "spn.h"

spn_err_t configure(spn_t* spn, spn_config_t* config) {
  spn_copy(spn, SPN_DIR_SOURCE, "spum.h", SPN_DIR_INCLUDE, "");
  return SPN_OK;
}
