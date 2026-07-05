#include "spn.h"

#include "foo.h"

#if FOO_VERSION != 10
  #error "the build script must compile against foo 1.0.0"
#endif

SPN_EXPORT
spn_err_t configure(spn_t* spn, spn_config_t* config) {
  return SPN_OK;
}
