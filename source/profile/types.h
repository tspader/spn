#ifndef SPN_PROFILE_TYPES_H
#define SPN_PROFILE_TYPES_H

#include "sp.h"
#include "spn.h"
#include "forward/types.h"

struct spn_profile {
  sp_str_t name;
  sp_str_t toolchain;
  spn_os_t os;
  spn_arch_t arch;
  spn_abi_t abi;
  spn_linkage_t linkage;
  spn_c_standard_t standard;
  spn_build_mode_t mode;
};

typedef sp_str_ht(spn_profile_info_t) spn_profile_table_t;

#endif
