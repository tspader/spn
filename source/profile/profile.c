#include "profile/profile.h"

#include "intern.h"
#include "external/cc.h"

spn_cc_kind_t spn_profile_get_cc(spn_profile_t* profile) {
  return profile->cc.kind;
}

const c8* spn_profile_get_cc_exe(spn_profile_t* profile) {
  return profile->cc.exe.data;
}

spn_linkage_t spn_profile_get_linkage(spn_profile_t* profile) {
  return profile->linkage;
}

spn_libc_kind_t spn_profile_get_libc(spn_profile_t* profile) {
  return profile->libc;
}

spn_c_standard_t spn_profile_get_standard(spn_profile_t* profile) {
  return profile->standard;
}

spn_build_mode_t spn_profile_get_mode(spn_profile_t* profile) {
  return profile->mode;
}

void spn_profile_set_cc(spn_profile_t* profile, spn_cc_kind_t kind) {
  profile->cc.kind = kind;

  switch (kind) {
    case SPN_CC_NONE: {
      profile->cc.exe = spn_intern_cstr("");
      break;
    }
    case SPN_CC_GCC: {
      profile->cc.exe = spn_intern_cstr("gcc");
      break;
    }
    case SPN_CC_CLANG: {
      profile->cc.exe = spn_intern_cstr("clang");
      break;
    }
    case SPN_CC_MUSL_GCC: {
      profile->cc.exe = spn_intern_cstr("musl-gcc");
      break;
    }
    case SPN_CC_TCC: {
      profile->cc.exe = spn_intern_cstr("tcc");
      break;
    }
    case SPN_CC_COSMOCC: {
      profile->cc.exe = spn_intern_cstr("cosmocc");
      break;
    }
    case SPN_CC_ZIG: {
      profile->cc.exe = spn_intern_cstr("zcc");
      break;
    }
    case SPN_CC_CUSTOM: {
      break;
    }
  }
}

void spn_profile_set_cc_exe(spn_profile_t* profile, const c8* exe) {
  profile->cc.exe = sp_str_view(exe);
  profile->cc.kind = spn_cc_kind_from_str(profile->cc.exe);
}

void spn_profile_set_linkage(spn_profile_t* profile, spn_linkage_t linkage) {
  profile->linkage = linkage;
}

void spn_profile_set_libc(spn_profile_t* profile, spn_libc_kind_t libc) {
  profile->libc = libc;
}

void spn_profile_set_standard(spn_profile_t* profile, spn_c_standard_t standard) {
  profile->standard = standard;
}

void spn_profile_set_mode(spn_profile_t* profile, spn_build_mode_t mode) {
  profile->mode = mode;
}

void spn_profile_set_kind(spn_profile_t* profile, spn_profile_kind_t kind) {
  profile->kind = kind;
}
