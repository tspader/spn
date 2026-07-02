#ifndef SPN_TOOLCHAIN_PROVISION_H
#define SPN_TOOLCHAIN_PROVISION_H

#include "error/types.h"
#include "toolchain/types.h"

spn_err_t spn_fetch_curl(sp_str_t url, sp_str_t dest, void* user_data);

sp_str_t  spn_toolchain_store_path(spn_toolchain_store_t* store, spn_artifact_t artifact);
sp_str_t  spn_artifact_resolve_url(sp_mem_t mem, spn_artifact_t artifact, sp_str_t mirror);
spn_err_union_t spn_toolchain_provision(spn_toolchain_store_t* store, spn_toolchain_t* toolchain, sp_str_t* root);

#endif
