#include "spn.h"

SPN_EXPORT
spn_err_t configure(spn_t* spn, spn_config_t* config) {
  spn_profile_t* profile = spn_get_profile(spn);
  if (spn_profile_get_os(profile) == SPN_OS_WINDOWS) {
    spn_add_system_dep(config, "user32");
  }
  else {
    spn_add_system_dep(config, "m");
  }
  return SPN_OK;
}
