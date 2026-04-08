#ifndef SPN_PROFILE_PROFILE_H
#define SPN_PROFILE_PROFILE_H

#include "err.h"
#include "profile/types.h"

void      spn_profile_overlay(spn_profile_info_t* dst, spn_profile_info_t* src);
sp_str_t  spn_profile_select_name(spn_profile_info_t* overrides);
void      spn_profile_populate(spn_profile_table_t* profiles, spn_pkg_t* pkg);
spn_err_t spn_profile_resolve(spn_profile_table_t profiles, spn_profile_info_t* overrides, spn_profile_t* result);

#endif
