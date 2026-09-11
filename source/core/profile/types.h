#ifndef SPN_PROFILE_TYPES_H
#define SPN_PROFILE_TYPES_H

#include "core/types.h"
#include "toolchain/types.h"
#include "when/types.h"

struct spn_profile_info {
  sp_str_t name;
  spn_toolchain_ref_t toolchain;
  spn_cc_driver_t driver;
  spn_ld_family_t linker;
  spn_os_t os;
  spn_arch_t arch;
  spn_abi_t abi;
  spn_linkage_t linkage;
  spn_c_standard_t standard;
  spn_mode_t mode;
  spn_opt_level_t opt;
  spn_sanitizer_set_t sanitizers;
  bool sanitizers_set;
  spn_when_t options;
  bool targeted;
  spn_sdk_t sdk;
  spn_path_t libc;
};

typedef sp_str_ht(spn_profile_info_t) spn_profile_table_t;

static inline spn_triple_t spn_profile_triple(const spn_profile_info_t* profile) {
  return (spn_triple_t) { profile->arch, profile->os, profile->abi };
}

#endif
