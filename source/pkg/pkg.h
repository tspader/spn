#ifndef SPN_PKG_PKG_H
#define SPN_PKG_PKG_H

#include "sp.h"

#include "error/types.h"
#include "pkg/types.h"

spn_pkg_info_t spn_pkg_new(sp_str_t name);

bool spn_pkg_has_lib_kind(spn_pkg_info_t* pkg, spn_linkage_t kind);
sp_str_t spn_pkg_get_url(spn_pkg_info_t* pkg);
spn_profile_info_t* spn_pkg_get_default_profile(spn_pkg_info_t* pkg);
spn_target_info_t* spn_pkg_get_target(spn_pkg_info_t* pkg, const c8* name);
spn_target_info_t* spn_pkg_get_target_ex(spn_pkg_info_t* pkg, sp_str_t name);

#endif
