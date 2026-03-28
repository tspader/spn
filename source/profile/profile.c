#include "enum/enum.h"
#include "profile/profile.h"
#include "intern.h"

spn_linkage_t spn_profile_get_linkage(spn_profile_t* profile) {
  return profile->linkage;
}

spn_c_standard_t spn_profile_get_standard(spn_profile_t* profile) {
  return profile->standard;
}

spn_build_mode_t spn_profile_get_mode(spn_profile_t* profile) {
  return profile->mode;
}

void spn_profile_set_linkage(spn_profile_t* profile, spn_linkage_t linkage) {
  profile->linkage = linkage;
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
