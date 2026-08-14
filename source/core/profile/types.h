#ifndef SPN_PROFILE_TYPES_H
#define SPN_PROFILE_TYPES_H

#include "core/types.h"
#include "when/types.h"

struct spn_profile_info {
  sp_str_t name;
  sp_str_t toolchain;
  spn_os_t os;
  spn_arch_t arch;
  spn_abi_t abi;
  spn_linkage_t linkage;
  spn_c_standard_t standard;
  spn_build_mode_t mode;
  spn_opt_level_t opt;
  spn_sanitizer_set_t sanitizers;
  bool sanitizers_set;
  spn_when_t options;
  bool targeted;
  sp_str_t sysroot;
};

typedef sp_str_ht(spn_profile_info_t) spn_profile_table_t;

#endif
