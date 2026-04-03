#ifndef SPN_PROFILE_PROFILE_H
#define SPN_PROFILE_PROFILE_H

#include "err.h"
#include "profile/types.h"

void spn_profile_overlay(spn_profile_info_t* dst, spn_profile_info_t* src);
sp_str_t spn_profile_select_name(spn_profile_info_t* overrides);
void spn_profile_populate(spn_profile_table_t* profiles, spn_pkg_t* pkg);
spn_err_t spn_profile_resolve(spn_profile_table_t profiles, spn_profile_info_t* overrides, spn_profile_t* result);

spn_cc_kind_t spn_profile_get_cc(spn_profile_t* profile);
const c8* spn_profile_get_cc_exe(spn_profile_t* profile);
spn_linkage_t spn_profile_get_linkage(spn_profile_t* profile);
spn_libc_kind_t spn_profile_get_libc(spn_profile_t* profile);
spn_c_standard_t spn_profile_get_standard(spn_profile_t* profile);
spn_build_mode_t spn_profile_get_mode(spn_profile_t* profile);

#endif
