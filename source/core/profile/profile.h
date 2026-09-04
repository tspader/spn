#ifndef SPN_PROFILE_PROFILE_H
#define SPN_PROFILE_PROFILE_H

#include "core/types.h"
#include "sp.h"
#include "spn/core.h"
#include "profile/types.h"
#include "spn/types.h"
#include "toolchain/types.h"

spn_when_facts_t      spn_profile_facts(const spn_profile_info_t* profile);
spn_profile_info_t    spn_profile_metaprogram(void);
void                  spn_profile_populate(spn_profile_table_t* profiles, spn_pkg_info_t* pkg);
spn_err_t             spn_profile_resolve(spn_profile_table_t profiles, const spn_profile_override_t* override, spn_triple_t host, const spn_pkg_info_t* pkg, spn_profile_info_t* result);
spn_toolchain_query_t spn_profile_query(const spn_profile_info_t* profile, spn_triple_t host);
void                  spn_profile_finalize(spn_profile_info_t* profile, spn_abi_t abi, spn_cc_driver_t driver);
sp_str_t              spn_profile_build_dir(sp_mem_t mem, const spn_profile_info_t* profile);

#endif
