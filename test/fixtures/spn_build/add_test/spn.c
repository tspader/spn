#include "spn.h"

SPN_EXPORT
spn_err_t configure(spn_t* spn) {
  spn_config_t* config = (spn_config_t*)spn;
  (void)config;
  spn_target_t* test = spn_add_test(config, "test");
  spn_target_add_source(test, "main.c");
  return SPN_OK;
}
