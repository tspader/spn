#ifndef SPN_SESSION_REGISTRY_H
#define SPN_SESSION_REGISTRY_H

#include "sp.h"
#include "spn.h"

#include "session/registry/types.h"

spn_loaded_pkg_t* spn_registry_load_file_pkg(spn_pkg_registry_t* registry, sp_str_t qualified, sp_str_t path);

#endif
