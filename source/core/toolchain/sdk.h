#ifndef SPN_TOOLCHAIN_SDK_H
#define SPN_TOOLCHAIN_SDK_H

#include "sp.h"
#include "sp/sp_msvc.h"
#include "paths/types.h"
#include "toolchain/types.h"

spn_sdk_kind_t spn_sdk_kind(spn_triple_t target);
bool           spn_sdk_declarable(spn_sdk_kind_t kind);
spn_sdk_t      spn_sdk_sysroot(spn_path_t root);
spn_sdk_t      spn_sdk_macos(sp_mem_t mem, spn_path_t root);
spn_sdk_t      spn_sdk_msvc(sp_mem_t mem, spn_path_t root, spn_arch_t arch);
spn_sdk_msvc_t spn_sdk_from_msvc(sp_mem_t mem, const sp_msvc_sdk_t* kits, const sp_msvc_vs_t* vs, spn_arch_t arch);
spn_sdk_host_t spn_sdk_detect(sp_mem_t mem, const spn_path_roots_t* roots, sp_env_t* env, spn_triple_t host);
spn_sdk_t      spn_sdk_from_host(const spn_sdk_host_t* host, spn_triple_t target);
spn_sdk_t      spn_sdk_resolve(sp_mem_t mem, const spn_sdk_host_t* host, const spn_toolchain_selection_t* selection);
sp_hash_t      spn_sdk_hash(const spn_sdk_t* sdk);

#endif
