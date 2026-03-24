#ifndef SPN_PROFILE_TYPES_H
#define SPN_PROFILE_TYPES_H

#include "sp.h"
#include "spn.h"

typedef enum {
  SPN_PROFILE_BUILTIN,
  SPN_PROFILE_USER,
} spn_profile_kind_t;

struct spn_profile {
  sp_str_t name;
  spn_linkage_t linkage;
  spn_libc_kind_t libc;
  spn_c_standard_t standard;
  spn_build_mode_t mode;
  spn_profile_kind_t kind;
  struct {
    spn_cc_kind_t kind;
    sp_str_t exe;
  } cc;
};

#endif
