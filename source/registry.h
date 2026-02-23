#ifndef SPN_REGISTRY_H
#define SPN_REGISTRY_H

#include "sp.h"
#include "spn.h"

#include "pkg.h"

struct spn_registry {
  sp_str_t name;
  sp_str_t location;
  spn_pkg_kind_t kind;
};

sp_str_t spn_registry_get_path(spn_registry_t* registry);

#endif
