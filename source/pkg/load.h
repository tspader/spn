#ifndef SPN_PKG_LOAD_H
#define SPN_PKG_LOAD_H

#include "error/types.h"
#include "pkg/types.h"

spn_err_union_t spn_pkg_load(sp_mem_t mem, sp_intern_t* intern, sp_str_t path, spn_manifest_role_t role, sp_str_t name, spn_pkg_info_t* pkg);

#endif
