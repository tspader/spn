#include "spn.h"

#include "gamma.h"

#if GAMMA_VALUE != 9
#error "gamma.h from the build dep store was not on the include path"
#endif

SPN_EXPORT
spn_err_t configure(spn_t* spn, spn_config_t* config) {
  return SPN_OK;
}
