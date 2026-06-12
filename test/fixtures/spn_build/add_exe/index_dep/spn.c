#include "spn.h"

spn_err_t configure(spn_t* spn, spn_config_t* config) {
  spn_target_t* foo = spn_add_exe(config, "foo");
  spn_target_add_source(foo, "main.c");
  return SPN_OK;
}
