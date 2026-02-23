#include "profile.h"

#include "cc.h"
#include "intern.h"

spn_libc_kind_t spn_libc_kind_from_str(sp_str_t str) {
  if (sp_str_equal_cstr(str, "gnu")) {
    return SPN_LIBC_GNU;
  }
  if (sp_str_equal_cstr(str, "musl")) {
    return SPN_LIBC_MUSL;
  }
  if (sp_str_equal_cstr(str, "cosmopolitan")) {
    return SPN_LIBC_COSMOPOLITAN;
  }
  if (sp_str_equal_cstr(str, "custom")) {
    return SPN_LIBC_CUSTOM;
  }

  SP_FATAL("Unknown libc {:fg brightyellow}; options are [gnu, musl, cosmopolitan, custom]", SP_FMT_STR(str));
  SP_UNREACHABLE_RETURN(SPN_LIBC_GNU);
}

sp_str_t spn_libc_kind_to_str(spn_libc_kind_t libc) {
  switch (libc) {
    case SPN_LIBC_GNU: {
      return SP_LIT("gnu");
    }
    case SPN_LIBC_MUSL: {
      return SP_LIT("musl");
    }
    case SPN_LIBC_COSMOPOLITAN: {
      return SP_LIT("cosmopolitan");
    }
    case SPN_LIBC_CUSTOM: {
      return SP_LIT("custom");
    }
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_build_mode_t spn_dep_build_mode_from_str(sp_str_t str) {
  if (sp_str_equal_cstr(str, "release")) {
    return SPN_DEP_BUILD_MODE_RELEASE;
  }
  if (sp_str_equal_cstr(str, "debug")) {
    return SPN_DEP_BUILD_MODE_DEBUG;
  }

  SP_FATAL("Unknown mode {:fg brightyellow}; options are [release, debug]", SP_FMT_STR(str));
  SP_UNREACHABLE_RETURN(SPN_DEP_BUILD_MODE_RELEASE);
}

sp_str_t spn_dep_build_mode_to_str(spn_build_mode_t mode) {
  switch (mode) {
    case SPN_DEP_BUILD_MODE_RELEASE: {
      return sp_str_lit("release");
    }
    case SPN_DEP_BUILD_MODE_DEBUG: {
      return sp_str_lit("debug");
    }
  }

  SP_UNREACHABLE_RETURN(sp_str_lit(""));
}

spn_cc_kind_t spn_profile_get_cc(spn_profile_t* profile) {
  return profile->cc.kind;
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

const c8* spn_profile_get_cc_exe(spn_profile_t* profile) {
  return profile->cc.exe.data;
}

void spn_profile_set_cc_exe(spn_profile_t* profile, const c8* exe) {
  profile->cc.exe = sp_str_view(exe);
  profile->cc.kind = spn_cc_kind_from_str(profile->cc.exe);
}

spn_linkage_t spn_profile_get_linkage(spn_profile_t* profile) {
  return profile->linkage;
}

void spn_profile_set_linkage(spn_profile_t* profile, spn_linkage_t linkage) {
  profile->linkage = linkage;
}

spn_libc_kind_t spn_profile_get_libc(spn_profile_t* profile) {
  return profile->libc;
}

void spn_profile_set_libc(spn_profile_t* profile, spn_libc_kind_t libc) {
  profile->libc = libc;
}

spn_c_standard_t spn_profile_get_standard(spn_profile_t* profile) {
  return profile->standard;
}

void spn_profile_set_standard(spn_profile_t* profile, spn_c_standard_t standard) {
  profile->standard = standard;
}

spn_build_mode_t spn_profile_get_mode(spn_profile_t* profile) {
  return profile->mode;
}

void spn_profile_set_mode(spn_profile_t* profile, spn_build_mode_t mode) {
  profile->mode = mode;
}

void spn_profile_set_kind(spn_profile_t* profile, spn_profile_kind_t kind) {
  profile->kind = kind;
}
