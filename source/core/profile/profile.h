#ifndef SPN_PROFILE_PROFILE_H
#define SPN_PROFILE_PROFILE_H

#include "core/types.h"
#include "sp.h"
#include "spn/core.h"
#include "profile/types.h"
#include "spn/types.h"

void      spn_profile_populate(spn_profile_table_t* profiles, spn_pkg_info_t* pkg);
spn_err_t spn_profile_resolve(spn_profile_table_t profiles, const spn_profile_override_t* override, spn_profile_info_t* result);
bool      spn_profile_shared(const spn_profile_info_t* profile, bool shared_demand);
void      spn_profile_finalize(spn_profile_info_t* profile, spn_triple_t triple, bool shared);
sp_str_t  spn_profile_build_dir(sp_mem_t mem, const spn_profile_info_t* profile);

#endif
