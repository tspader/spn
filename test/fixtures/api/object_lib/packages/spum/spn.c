#include "spn.h"

spn_err_t configure(spn_t* spn, spn_config_t* config) {
  // The same object lib shape as the manifest's, but declared from the script
  spn_target_t* extra = spn_add_lib(config, "scripted", SPN_LIB_KIND_OBJECT);
  spn_target_add_source(extra, "rt/extra2.c");
  spn_target_add_flag(extra, "-DVIA_FLAG=7");
  return SPN_OK;
}
