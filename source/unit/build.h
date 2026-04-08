#ifndef SPN_UNIT_BUILD_H
#define SPN_UNIT_BUILD_H

#include "err.h"
#include "log/types.h"
#include "unit/types.h"

sp_str_t spn_cache_dir_kind_to_path(spn_pkg_dir_t kind);
sp_str_t spn_build_ctx_resolve_dir(const spn_build_ctx_t* ctx, spn_pkg_dir_t kind, sp_str_t sub);
sp_str_t spn_build_ctx_get_dir(const spn_build_ctx_t* ctx, spn_pkg_dir_t kind);

#endif
