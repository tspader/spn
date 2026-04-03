#include "profile/profile.h"
#include "pkg/types.h"
#include "spn.h"

void spn_profile_overlay(spn_profile_info_t* dst, spn_profile_info_t* src) {
  if (!sp_str_empty(src->name))      dst->name = src->name;
  if (!sp_str_empty(src->toolchain)) dst->toolchain = src->toolchain;
  if (src->linkage)                  dst->linkage = src->linkage;
  if (src->standard)                 dst->standard = src->standard;
  if (src->mode)                     dst->mode = src->mode;
  if (src->os)                       dst->os = src->os;
  if (src->arch)                     dst->arch = src->arch;
  if (src->abi)                      dst->abi = src->abi;
}

sp_str_t spn_profile_select_name(spn_profile_info_t* overrides) {
  if (!sp_str_empty(overrides->name))
    return overrides->name;

  if (overrides->mode == SPN_BUILD_MODE_RELEASE)
    return sp_str_lit("release");

  return sp_str_lit("debug");
}

void spn_profile_populate(spn_profile_table_t* profiles, spn_pkg_t* pkg) {
  // 1. Seed the default profile with hardcoded base values
  spn_profile_info_t default_profile = {
    .name      = sp_str_lit("default"),
    .toolchain = sp_str_lit("builtin"),
    .linkage   = SPN_LIB_KIND_SHARED,
    .standard  = SPN_C11,
    .mode      = SPN_BUILD_MODE_DEBUG,
  };
  sp_str_ht_insert(*profiles, default_profile.name, default_profile);

  // 2. Apply user's [profile.default] if present
  sp_str_t default_name = sp_str_lit("default");
  spn_profile_info_t* user_default = sp_om_get(pkg->profiles, default_name);
  if (user_default) {
    spn_profile_overlay(sp_str_ht_get(*profiles, default_name), user_default);
  }

  // 3. Derive debug and release from default
  spn_profile_info_t base = *sp_str_ht_get(*profiles, default_name);

  spn_profile_info_t debug_profile = base;
  debug_profile.name = sp_str_lit("debug");
  debug_profile.mode = SPN_BUILD_MODE_DEBUG;
  sp_str_ht_insert(*profiles, debug_profile.name, debug_profile);

  spn_profile_info_t release_profile = base;
  release_profile.name = sp_str_lit("release");
  release_profile.mode = SPN_BUILD_MODE_RELEASE;
  sp_str_ht_insert(*profiles, release_profile.name, release_profile);

  // 4. Overlay remaining user profiles
  sp_om_for(pkg->profiles, it) {
    spn_profile_info_t* user = sp_om_at(pkg->profiles, it);
    if (sp_str_equal(user->name, sp_str_lit("default"))) continue;
    spn_profile_info_t* entry = sp_str_ht_get(*profiles, user->name);
    if (entry) {
      spn_profile_overlay(entry, user);
    } else {
      sp_str_ht_insert(*profiles, user->name, *user);
    }
  }
}

spn_err_t spn_profile_resolve(spn_profile_table_t profiles, spn_profile_info_t* overrides, spn_profile_t* result) {
  sp_str_t name = spn_profile_select_name(overrides);

  spn_profile_info_t* info = sp_str_ht_get(profiles, name);
  if (!info) {
    return SPN_ERROR;
  }

  spn_profile_info_t merged = *info;
  spn_profile_overlay(&merged, overrides);

  *result = (spn_profile_t) {
    .name      = merged.name,
    .toolchain = merged.toolchain,
    .os        = merged.os,
    .arch      = merged.arch,
    .abi       = merged.abi,
    .linkage   = merged.linkage,
    .standard  = merged.standard,
    .mode      = merged.mode,
  };
  return SPN_OK;
}

spn_linkage_t spn_profile_get_linkage(spn_profile_t* profile) {
  return profile->linkage;
}

spn_c_standard_t spn_profile_get_standard(spn_profile_t* profile) {
  return profile->standard;
}

spn_build_mode_t spn_profile_get_mode(spn_profile_t* profile) {
  return profile->mode;
}
