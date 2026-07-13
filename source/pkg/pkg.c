#include "sp.h"
#include "sp/macro.h"
#include "pkg/pkg.h"

#include "intern/intern.h"
#include "pkg/mutate.h"
#include "target/mutate.h"

spn_pkg_info_t spn_pkg_new(sp_mem_t mem, sp_str_t name) {
  spn_pkg_info_t pkg = sp_zero;
  spn_pkg_init(mem, &pkg, name);
  return pkg;
}

bool spn_pkg_has_lib_kind(spn_pkg_info_t* pkg, spn_linkage_t kind) {
  sp_str_om_for(pkg->libs, it) {
    spn_target_info_t* lib = sp_str_om_at(pkg->libs, it);
    if (spn_linkage_set_has(lib->linkages, kind)) {
      return true;
    }
  }

  return false;
}

spn_target_info_t* spn_pkg_get_target(spn_pkg_info_t* pkg, const c8* name) {
  return spn_pkg_get_target_ex(pkg, sp_str_view(name));
}

// @spader
// This doesn't look quite right. It's suspicious that we'd need to get a target without caring
// where it came from specifically.
spn_target_info_t* spn_pkg_get_target_ex(spn_pkg_info_t* pkg, sp_str_t name) {
  // Target maps are keyed by interned names
  name = spn_intern(name);

  if (sp_str_om_has(pkg->exes, name)) {
    return sp_str_om_get(pkg->exes, name);
  }
  if (sp_str_om_has(pkg->scripts, name)) {
    return sp_str_om_get(pkg->scripts, name);
  }
  if (sp_str_om_has(pkg->tests, name)) {
    return sp_str_om_get(pkg->tests, name);
  }
  if (sp_str_om_has(pkg->libs, name)) {
    return sp_str_om_get(pkg->libs, name);
  }

  return SP_NULLPTR;
}

spn_profile_info_t* spn_pkg_get_default_profile(spn_pkg_info_t* pkg) {
  sp_str_om_for(pkg->profiles, it) {
    return sp_str_om_at(pkg->profiles, it);
  }

  SP_UNREACHABLE_RETURN(SP_NULLPTR);
}


