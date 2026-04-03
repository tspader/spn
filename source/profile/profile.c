#include "profile/profile.h"

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

spn_linkage_t spn_profile_get_linkage(spn_profile_t* profile) {
  return profile->linkage;
}

spn_c_standard_t spn_profile_get_standard(spn_profile_t* profile) {
  return profile->standard;
}

spn_build_mode_t spn_profile_get_mode(spn_profile_t* profile) {
  return profile->mode;
}
