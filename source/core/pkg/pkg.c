#include "sp.h"
#include "macro/macro.h"
#include "str/str.h"
#include "pkg/pkg.h"

#include "hash/digest/digest.h"
#include "intern/intern.h"
#include "paths/paths.h"
#include "pkg/mutate.h"
#include "profile/types.h"
#include "target/mutate.h"
#include "when/when.h"

static sp_hash_t hash_push(sp_hash_t hash, sp_hash_t value) {
  sp_hash_t parts [] = { hash, value };
  return spn_digest_hash_combine(parts, sp_carr_len(parts));
}

static sp_hash_t hash_gated(sp_hash_t hash, spn_gated_list_t list) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_da_for(list, it) {
    hash = hash_push(hash, spn_digest_hash_str(list[it].value));
    hash = hash_push(hash, spn_digest_hash_str(spn_when_to_str(scratch.mem, &list[it].when)));
  }
  sp_mem_end_scratch(scratch);
  return hash;
}

sp_hash_t spn_pkg_hash_platform(spn_pkg_info_t* pkg, const spn_profile_info_t* profile) {
  sp_hash_t hash = 0;

  spn_target_map_t maps [] = { pkg->libs, pkg->exes, pkg->scripts, pkg->tests };

  switch (profile->os) {
    case SPN_OS_MACOS: {
      hash = hash_push(hash, (sp_hash_t)profile->sysroot.root);
      hash = hash_push(hash, spn_digest_hash_str(profile->sysroot.sub));
      hash = hash_push(hash, spn_digest_hash(&pkg->macos.min_os, sizeof(pkg->macos.min_os)));
      hash = hash_gated(hash, pkg->gated.frameworks);
      sp_carr_for(maps, mt) {
        sp_om_for(maps[mt], it) {
          spn_target_info_t* target = sp_str_om_at(maps[mt], it);
          hash = hash_push(hash, spn_digest_hash_str(target->name));
          hash = hash_push(hash, spn_digest_hash(&target->macos.min_os, sizeof(target->macos.min_os)));
          hash = hash_gated(hash, target->gated.frameworks);
        }
      }
      break;
    }
    case SPN_OS_WINDOWS: {
      sp_carr_for(maps, mt) {
        sp_om_for(maps[mt], it) {
          spn_target_info_t* target = sp_str_om_at(maps[mt], it);
          if (target->windows.subsystem == SPN_WIN_SUBSYSTEM_NONE) continue;
          hash = hash_push(hash, spn_digest_hash_str(target->name));
          hash = hash_push(hash, (sp_hash_t)target->windows.subsystem);
        }
      }
      break;
    }
    case SPN_OS_LINUX:
    case SPN_OS_WASI:
    case SPN_OS_FREESTANDING:
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

  if (sp_str_om_has(pkg->libs, name)) {
    return sp_str_om_get(pkg->libs, name);
  }
  if (sp_str_om_has(pkg->exes, name)) {
    return sp_str_om_get(pkg->exes, name);
  }
  if (sp_str_om_has(pkg->scripts, name)) {
    return sp_str_om_get(pkg->scripts, name);
  }
  if (sp_str_om_has(pkg->tests, name)) {
    return sp_str_om_get(pkg->tests, name);
  }
  if (sp_str_om_has(pkg->examples, name)) {
    return sp_str_om_get(pkg->examples, name);
  }

  return SP_NULLPTR;
}

spn_profile_info_t* spn_pkg_get_default_profile(spn_pkg_info_t* pkg) {
  sp_str_om_for(pkg->profiles, it) {
    return sp_str_om_at(pkg->profiles, it);
  }

  SP_UNREACHABLE_RETURN(SP_NULLPTR);
}


spn_pkg_root_t spn_pkg_upstream(spn_pkg_info_t* info) {
  if (sp_str_empty(info->upstream.url)) {
    return (spn_pkg_root_t) { .kind = SPN_PKG_ROOT_NONE };
  }
  return (spn_pkg_root_t) {
    .kind = SPN_PKG_ROOT_GIT,
    .git = { .url = info->upstream.url, .rev = info->upstream.commit },
  };
}
