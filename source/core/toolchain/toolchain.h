#ifndef SPN_TOOLCHAIN_H
#define SPN_TOOLCHAIN_H

#include "paths/types.h"
#include "toolchain/types.h"
#include "toolchain/catalog.h"
#include "toolchain/select.h"
#include "toolchain/provision.h"
#include "toolchain/linker.h"
#include "toolchain/sdk.h"
#include "toolchain/libc.h"
#include "common.gen.h"

spn_path_check_t         spn_toolchain_path(spn_toolchain_source_t source, spn_path_root_t base, sp_str_t str, spn_path_t* path);
spn_path_check_t         spn_toolchain_sdk_path(spn_toolchain_source_t source, spn_path_root_t base, sp_str_t str, spn_path_t* path);
spn_path_check_t         spn_toolchain_program(spn_toolchain_source_t source, spn_path_root_t base, sp_str_t program, spn_arg_t* arg);
spn_cc_cap_set_t         spn_toolchain_driver_caps(spn_cc_driver_t driver);
bool                     spn_toolchain_driver_retargets(spn_cc_driver_t driver);
spn_sanitizer_set_t      spn_toolchain_stock_sanitizers(spn_cc_driver_t driver, spn_triple_t host);
bool                     spn_toolchain_driver_composes(spn_cc_driver_t driver, spn_ld_dialect_t dialect);
spn_abi_t                spn_default_abi(spn_cc_driver_t driver, spn_os_t os);
spn_linkage_t            spn_abi_linkage(spn_abi_t abi);
spn_path_t               spn_toolchain_artifact_root(spn_artifact_t artifact);
spn_toolchain_launcher_t spn_toolchain_launcher_with_root(sp_mem_t mem, spn_toolchain_launcher_t launcher, spn_path_t root);
sp_str_t                 spn_toolchain_launcher_to_str(const spn_path_roots_t* roots, sp_mem_t mem, spn_toolchain_launcher_t launcher);
bool                     spn_toolchain_has_cxx(spn_toolchain_info_t* toolchain);
spn_wasi_spelling_t      spn_toolchain_wasi_spelling(const spn_path_roots_t* roots, sp_mem_t mem, const spn_toolchain_info_t* toolchain);
spn_toolchain_ref_t      spn_toolchain_ref_from_str(sp_str_t str);

#endif
