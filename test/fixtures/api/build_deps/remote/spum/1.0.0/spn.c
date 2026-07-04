#include "spn.h"

SPN_EXPORT
spn_err_t configure(spn_t* spn, spn_config_t* config) {
  spn_fs_copy("/source/spum.h", "/store/include");
  return SPN_OK;
}
