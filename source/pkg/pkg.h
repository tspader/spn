#ifndef SPN_PKG_PKG_H
#define SPN_PKG_PKG_H

#include "enum/enum.h"
#include "err.h"
#include "pkg/types.h"

spn_pkg_t spn_pkg_new(sp_str_t name);
spn_pkg_t spn_pkg_from_bare_default(sp_str_t path, sp_str_t name);
spn_pkg_t spn_pkg_from_default(sp_str_t path, sp_str_t name);
spn_err_t spn_pkg_from_index(spn_pkg_t* pkg, sp_str_t index_path);
spn_err_t spn_pkg_from_manifest(spn_pkg_t* pkg, sp_str_t manifest_path);

bool spn_pkg_has_lib_kind(spn_pkg_t* pkg, spn_linkage_t kind);
sp_str_t spn_pkg_get_url(spn_pkg_t* pkg);
spn_profile_info_t* spn_pkg_get_default_profile(spn_pkg_t* pkg);
spn_target_t* spn_pkg_get_target(spn_pkg_t* pkg, const c8* name);
spn_target_t* spn_pkg_get_target_ex(spn_pkg_t* pkg, sp_str_t name);

#endif
