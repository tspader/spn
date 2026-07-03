#include "spn.h"
#include "probe.h"

#ifndef DEFINE_FROM_TOML
#error "The #define from spn.toml was not applied"
#endif

#ifndef DEFINE_FROM_HEADER
#error "The #define from probe.h was not applied"
#endif

__attribute__((export_name("configure")))
spn_err_t configure(spn_t* spn, spn_config_t* config) {
  spn_target_t* target = spn_get_target(spn, "configure_table");
  if (!target) return SPN_ERROR;
  return SPN_OK;
}
