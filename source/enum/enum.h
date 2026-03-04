#ifndef SPN_ENUM_ENUM_H
#define SPN_ENUM_ENUM_H

#include "sp.h"
#include "spn.h"
#include "index/types.h"
#include "pkg/types.h"
#include "target/types.h"

spn_libc_kind_t spn_libc_kind_from_str(sp_str_t str);
sp_str_t spn_libc_kind_to_str(spn_libc_kind_t libc);
spn_build_mode_t spn_dep_build_mode_from_str(sp_str_t str);
sp_str_t spn_dep_build_mode_to_str(spn_build_mode_t mode);

sp_str_t spn_visibility_to_str(spn_visibility_t kind);
spn_visibility_t spn_visibility_from_str(sp_str_t str);
spn_linkage_t spn_lib_kind_from_str(sp_str_t str);
spn_linkage_t spn_pkg_linkage_from_str(sp_str_t str);
sp_str_t spn_pkg_linkage_to_str(spn_linkage_t kind);

spn_pkg_dir_t spn_cache_dir_kind_from_str(sp_str_t str);

spn_cc_kind_t spn_cc_kind_from_str(sp_str_t str);
spn_c_standard_t spn_c_standard_from_str(sp_str_t str);
sp_str_t spn_c_standard_to_str(spn_c_standard_t standard);

sp_str_t spn_package_kind_to_str(spn_pkg_kind_t kind);
spn_pkg_kind_t spn_package_kind_from_str(sp_str_t str);

sp_str_t spn_index_protocol_to_str(spn_index_protocol_t protocol);
spn_index_protocol_t spn_index_protocol_from_str(sp_str_t str);

sp_os_lib_kind_t spn_lib_kind_to_sp_os_lib_kind(spn_linkage_t kind);
spn_target_kind_t spn_pkg_linkage_to_target_kind(spn_linkage_t kind);
spn_linkage_t spn_target_kind_to_pkg_linkage(spn_target_kind_t kind);

#endif
