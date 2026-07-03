#include "spn.h"

SPN_EXPORT
spn_err_t configure(spn_t* spn, spn_config_t* config) {
  if (spn_add_exe((spn_config_t*)spn, "smuggled")) return SPN_ERROR;
  if (spn_get_target((spn_t*)config, "wrong_handle")) return SPN_ERROR;
  if (!spn_get_target(spn, "wrong_handle")) return SPN_ERROR;
  return SPN_OK;
}
