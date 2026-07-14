#include "sp.h"
#include "sp/macro.h"
#include "sp/str.h"
#include "pkg/pkg.h"

#include "intern/intern.h"
#include "pkg/mutate.h"
#include "profile/types.h"
#include "target/mutate.h"

static sp_hash_t hash_push(sp_hash_t hash, sp_hash_t value) {
  sp_hash_t parts [] = { hash, value };
  return sp_hash_combine(parts, sp_carr_len(parts));
}

sp_hash_t spn_pkg_hash_platform(spn_pkg_info_t* pkg, const spn_profile_info_t* profile) {
  sp_hash_t hash = 0;

  spn_target_map_t maps [] = { pkg->libs, pkg->exes, pkg->scripts, pkg->tests };

  switch (profile->os) {
    case SPN_OS_MACOS: {
      hash = hash_push(hash, sp_hash_str(profile->sysroot));
      hash = hash_push(hash, sp_hash_bytes(&pkg->macos.min_os, sizeof(pkg->macos.min_os), 0));
      sp_da_for(pkg->macos.frameworks, it) {
        hash = hash_push(hash, sp_hash_str(pkg->macos.frameworks[it]));
      }
      sp_carr_for(maps, mt) {
        sp_om_for(maps[mt], it) {
          spn_target_info_t* target = sp_str_om_at(maps[mt], it);
          hash = hash_push(hash, sp_hash_str(target->name));
          hash = hash_push(hash, sp_hash_bytes(&target->macos.min_os, sizeof(target->macos.min_os), 0));
          sp_da_for(target->macos.frameworks, ft) {
            hash = hash_push(hash, sp_hash_str(target->macos.frameworks[ft]));
          }
        }
      }
      break;
    }
    case SPN_OS_WINDOWS: {
      sp_carr_for(maps, mt) {
        sp_om_for(maps[mt], it) {
          spn_target_info_t* target = sp_str_om_at(maps[mt], it);
          if (target->windows.subsystem == SPN_WIN_SUBSYSTEM_NONE) continue;
          hash = hash_push(hash, sp_hash_str(target->name));
          hash = hash_push(hash, (sp_hash_t)target->windows.subsystem);
        }
      }
      break;
    }
    case SPN_OS_LINUX:
    case SPN_OS_WASI:
    case SPN_OS_NONE: {
      break;
    }
  }

  return hash;
}

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


