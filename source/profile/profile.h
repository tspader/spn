#ifndef SPN_PROFILE_PROFILE_H
#define SPN_PROFILE_PROFILE_H

#include "error/types.h"
#include "forward/types.h"
#include "profile/types.h"

void            spn_profile_overlay(spn_profile_info_t* dst, spn_profile_info_t* src);
void            spn_profile_populate(spn_profile_table_t* profiles, spn_pkg_info_t* pkg);
spn_err_union_t spn_profile_overrides_parse(spn_profile_args_t* args, spn_profile_info_t* result);
spn_err_union_t spn_profile_resolve(spn_profile_table_t profiles, spn_profile_info_t* overrides, spn_profile_info_t* result);
sp_str_t        spn_profile_identity_to_str(sp_mem_t mem, const spn_profile_info_t* profile);

#endif
