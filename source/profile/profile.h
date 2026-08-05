#ifndef SPN_PROFILE_PROFILE_H
#define SPN_PROFILE_PROFILE_H

#include "error/types.h"
#include "forward/types.h"
#include "profile/types.h"

spn_err_union_t spn_profile_parse(spn_profile_args_t* args, spn_profile_info_t* result);
void            spn_profile_populate(spn_profile_table_t* profiles, spn_pkg_info_t* pkg);
spn_err_union_t spn_profile_resolve(spn_profile_table_t profiles, spn_profile_info_t* overrides, spn_triple_t host, bool shared_demand, spn_profile_info_t* result);
sp_str_t        spn_profile_build_path(sp_mem_t mem, sp_str_t build, const spn_profile_info_t* profile);

#endif
