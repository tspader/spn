#include "spn.h"
#include "b.h"

#if SPUM != 69
#error "expected SPUM"
#endif

SPN_EXPORT
spn_err_t configure(spn_t* spn, spn_config_t* config) {
  return SPN_OK;
}
