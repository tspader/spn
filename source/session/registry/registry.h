#ifndef SPN_SESSION_REGISTRY_H
#define SPN_SESSION_REGISTRY_H

#include "sp.h"
#include "spn.h"

#include "intern/types.h"
#include "session/registry/types.h"

spn_registry_pkg_t* spn_registry_load_file_pkg(spn_pkg_registry_t* registry, sp_mem_t mem, sp_intern_t* intern, sp_str_t qualified, sp_str_t manifest, spn_registry_err_t* err);

#endif
