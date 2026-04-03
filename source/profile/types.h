#ifndef SPN_PROFILE_TYPES_H
#define SPN_PROFILE_TYPES_H

#include "sp.h"
#include "spn.h"

typedef struct {
  sp_str_t name;
  sp_str_t toolchain;
  spn_os_t os;
  spn_arch_t arch;
  spn_linkage_t linkage;
  spn_c_standard_t standard;
  spn_build_mode_t mode;
} spn_profile_info_t;

struct spn_profile {
  sp_str_t name;
  sp_str_t toolchain;
  spn_os_t os;
  spn_arch_t arch;
  spn_linkage_t linkage;
  spn_c_standard_t standard;
  spn_build_mode_t mode;
};

#endif
