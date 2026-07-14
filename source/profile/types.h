#ifndef SPN_PROFILE_TYPES_H
#define SPN_PROFILE_TYPES_H

#include "sp.h"
#include "spn.h"
#include "forward/types.h"
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

typedef struct {
  sp_str_t name;
  sp_str_t toolchain;
  sp_str_t mode;
  sp_str_t opt;
  sp_str_t sanitize;
  sp_str_t target;
  sp_str_t os;
  sp_str_t arch;
  sp_str_t abi;
} spn_profile_args_t;

typedef sp_str_ht(spn_profile_info_t) spn_profile_table_t;

#endif
