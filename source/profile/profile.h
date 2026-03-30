#ifndef SPN_PROFILE_PROFILE_H
#define SPN_PROFILE_PROFILE_H

#include "profile/types.h"

spn_cc_kind_t spn_profile_get_cc(spn_profile_t* profile);
const c8* spn_profile_get_cc_exe(spn_profile_t* profile);
spn_linkage_t spn_profile_get_linkage(spn_profile_t* profile);
spn_libc_kind_t spn_profile_get_libc(spn_profile_t* profile);
spn_c_standard_t spn_profile_get_standard(spn_profile_t* profile);
spn_build_mode_t spn_profile_get_mode(spn_profile_t* profile);

#endif
