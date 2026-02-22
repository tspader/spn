#ifndef SPN_PROFILE_H
#define SPN_PROFILE_H

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

void spn_profile_set_cc(spn_profile_t* profile, spn_cc_kind_t kind);
void spn_profile_set_cc_exe(spn_profile_t* profile, const c8* exe);
void spn_profile_set_linkage(spn_profile_t* profile, spn_linkage_t linkage);
void spn_profile_set_libc(spn_profile_t* profile, spn_libc_kind_t libc);
void spn_profile_set_standard(spn_profile_t* profile, spn_c_standard_t standard);
void spn_profile_set_mode(spn_profile_t* profile, spn_build_mode_t mode);

#endif
