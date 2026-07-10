#ifndef SPN_ENUM_ENUM_H
#define SPN_ENUM_ENUM_H

#include "sp.h"
#include "spn.h"
#include "event/types.h"
#include "index/types.h"
#include "pkg/types.h"
#include "target/types.h"
#include "toolchain/types.h"
#include "when/types.h"

spn_arch_t      spn_arch_from_str(sp_str_t str);
sp_str_t        spn_arch_to_str(spn_arch_t arch);
spn_os_t        spn_os_from_sp_os(sp_os_kind_t os);
spn_os_t        spn_os_from_str(sp_str_t str);
sp_str_t        spn_os_to_str(spn_os_t os);
spn_cc_driver_t spn_cc_driver_from_str(sp_str_t str);
sp_str_t        spn_cc_driver_to_str(spn_cc_driver_t driver);
spn_abi_t       spn_abi_from_str(sp_str_t str);
sp_str_t        spn_abi_to_str(spn_abi_t abi);

spn_libc_kind_t spn_libc_kind_from_str(sp_str_t str);
sp_str_t spn_libc_kind_to_str(spn_libc_kind_t libc);
spn_build_mode_t spn_build_mode_from_str(sp_str_t str);
sp_str_t spn_build_mode_to_str(spn_build_mode_t mode);

spn_linkage_t spn_lib_kind_from_str(sp_str_t str);
spn_linkage_t spn_linkage_from_str(sp_str_t str);
sp_str_t spn_linkage_to_str(spn_linkage_t kind);

spn_option_type_t spn_option_type_from_str(sp_str_t str);
sp_str_t spn_option_type_to_str(spn_option_type_t type);
sp_str_t spn_option_setter_to_str(spn_option_setter_t setter);

spn_dir_t spn_cache_dir_kind_from_str(sp_str_t str);

spn_cc_kind_t spn_cc_kind_from_str(sp_str_t str);
spn_c_standard_t spn_c_standard_from_str(sp_str_t str);
sp_str_t spn_c_standard_to_str(spn_c_standard_t standard);
spn_cxx_standard_t spn_cxx_standard_from_str(sp_str_t str);
sp_str_t spn_cxx_standard_to_str(spn_cxx_standard_t standard);
spn_lang_t spn_lang_from_path(sp_str_t path);

sp_str_t spn_pkg_source_to_str(spn_pkg_source_t kind);
spn_pkg_source_t spn_pkg_source_from_str(sp_str_t str);

sp_str_t spn_index_protocol_to_str(spn_index_protocol_t protocol);
spn_index_protocol_t spn_index_protocol_from_str(sp_str_t str);

sp_str_t spn_index_kind_to_str(spn_index_kind_t kind);
spn_index_kind_t spn_index_kind_from_str(sp_str_t str);

sp_str_t spn_index_dep_kind_to_str(spn_index_dep_kind_t kind);
spn_index_dep_kind_t spn_index_dep_kind_from_str(sp_str_t str);

sp_os_lib_kind_t spn_lib_kind_to_sp_os_lib_kind(spn_linkage_t kind);
spn_target_kind_t spn_linkage_to_target_kind(spn_linkage_t kind);
spn_linkage_t spn_target_kind_to_pkg_linkage(spn_target_kind_t kind);

#endif
