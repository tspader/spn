#include "profile/profile.h"
#include "ctx/types.h"
#include "forward/types.h"
#include "sp/macro.h"
#include "intern/intern.h"
#include "pkg/types.h"
#include "spn.h"
#include "triple/triple.h"

sp_str_t spn_profile_build_path(sp_mem_t mem, sp_str_t build, const spn_profile_info_t* profile) {
  if (profile->targeted) {
    spn_triple_t target = { profile->arch, profile->os, profile->abi };
    build = sp_fs_join_path(mem, build, spn_triple_to_str(mem, target));
  }
  return sp_fs_join_path(mem, build, profile->name);
}

void overlay_profile(spn_profile_info_t* to, spn_profile_info_t* from) {
  if (!sp_str_empty(from->name)) {
    to->name = from->name;
  }
  if (!sp_str_empty(from->toolchain)) {
    to->toolchain = from->toolchain;
  }
  if (from->linkage) {
    to->linkage = from->linkage;
  }
  if (from->standard) {
    to->standard = from->standard;
  }
  if (from->mode) {
    to->mode = from->mode;
  }
  if (from->opt) {
    to->opt = from->opt;
  }
  if (from->sanitizers_set || from->sanitizers) {
    to->sanitizers = from->sanitizers;
    to->sanitizers_set = true;
  }
  if (from->os) {
    to->os = from->os;
    to->abi = from->abi;
  }
  else if (from->abi) {
    to->abi = from->abi;
  }
  if (from->arch) {
    to->arch = from->arch;
  }
  if (!sp_da_empty(from->options.clauses)) to->options = from->options;
}

static sp_str_t spn_profile_select_name(spn_profile_info_t* overrides) {
  if (!sp_str_empty(overrides->name))
    return overrides->name;

  if (overrides->mode == SPN_BUILD_MODE_RELEASE)
    return sp_str_lit("release");

  return sp_str_lit("debug");
}

void spn_profile_populate(spn_profile_table_t* profiles, spn_pkg_info_t* pkg) {
  struct {
    sp_str_t name;
    spn_profile_info_t automatic;
    spn_profile_info_t* user;
  } fallback = sp_zero;
  fallback.name = spn_intern_cstr("default");
  fallback.automatic = (spn_profile_info_t) {
    .name      = fallback.name,
    .toolchain = spn_intern_cstr("auto"),
    .standard  = SPN_C11,
    .mode      = SPN_BUILD_MODE_DEBUG,
  };
  sp_str_ht_insert(*profiles, fallback.name, fallback.automatic);

  // Start with the default, if present
  spn_profile_info_t** ptr = sp_om_getp(pkg->profiles, fallback.name);
  fallback.user = ptr ? *ptr : SP_NULLPTR;
  if (fallback.user) {
    overlay_profile(sp_str_ht_get(*profiles, fallback.name), fallback.user);
  }

  // Build the base debug and release profiles
  spn_profile_info_t base = *sp_str_ht_get(*profiles, fallback.name);
  struct {
    spn_profile_info_t debug;
    spn_profile_info_t release;
    spn_profile_info_t derived;
  } p = { base, base, base };

  p.debug.name = sp_str_lit("debug");
  p.debug.mode = SPN_BUILD_MODE_DEBUG;
  sp_str_ht_insert(*profiles, p.debug.name, p.debug);

  p.release.name = sp_str_lit("release");
  p.release.mode = SPN_BUILD_MODE_RELEASE;
  sp_str_ht_insert(*profiles, p.release.name, p.release);

  // Apply overlaid fields
  sp_str_om_for(pkg->profiles, it) {
    spn_profile_info_t* user = sp_str_om_at(pkg->profiles, it);
    if (sp_str_equal(user->name, fallback.name)) continue;

    spn_profile_info_t* entry = sp_str_ht_get(*profiles, user->name);
    if (entry) {
      overlay_profile(entry, user);
    } else {
      p.derived = base;
      p.derived.name = user->name;
      overlay_profile(&p.derived, user);
      sp_str_ht_insert(*profiles, p.derived.name, p.derived);
    }
  }
}

static spn_abi_t spn_profile_default_abi(spn_os_t os, bool shared) {
  switch (os) {
    case SPN_OS_WINDOWS: return SPN_ABI_GNU;
    case SPN_OS_LINUX:   return shared ? SPN_ABI_GNU : SPN_ABI_MUSL;
    case SPN_OS_MACOS:
    case SPN_OS_WASI:
    case SPN_OS_NONE:    return SPN_ABI_NONE;
  }
  SP_UNREACHABLE_RETURN(SPN_ABI_NONE);
}

spn_err_union_t spn_profile_resolve(spn_profile_table_t profiles, spn_profile_info_t* overrides, spn_triple_t host, bool is_shared, spn_profile_info_t* result) {
  sp_str_t name = spn_profile_select_name(overrides);

  if (sp_str_find_c8(name, '/') >= 0 || sp_str_find_c8(name, '\\') >= 0) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_PROFILE_INVALID,
      .profile = { .name = name },
    };
  }

  if (spn_triple_from_str(name).arch) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_PROFILE_INVALID,
      .profile = { .name = name },
    };
  }

  spn_profile_info_t* info = sp_str_ht_get(profiles, name);
  if (!info) {
    return (spn_err_union_t) {
      .kind = SPN_ERR_PROFILE_UNDEFINED,
      .profile = { .name = name },
    };
  }

  spn_profile_info_t merged = *info;
  overlay_profile(&merged, overrides);

  spn_triple_t target = { merged.arch, merged.os, merged.abi };
  bool targeted = target.arch || target.os || target.abi;
  bool shared = merged.linkage == SPN_LIB_KIND_SHARED || (!merged.linkage && is_shared);
  if (!target.arch) target.arch = host.arch;
  if (!target.os)   target.os = host.os;
  if (!target.abi)  target.abi = spn_profile_default_abi(target.os, shared);

  if (!merged.linkage) {
    merged.linkage = !shared && target.abi == SPN_ABI_MUSL ? SPN_LIB_KIND_STATIC : SPN_LIB_KIND_SHARED;
  }

  if (!merged.opt) {
    merged.opt = merged.mode == SPN_BUILD_MODE_RELEASE ? SPN_OPT_LEVEL_2 : SPN_OPT_LEVEL_0;
  }

  *result = (spn_profile_info_t) {
    .name       = merged.name,
    .toolchain  = merged.toolchain,
    .os         = target.os,
    .arch       = target.arch,
    .abi        = target.abi,
    .linkage    = merged.linkage,
    .standard   = merged.standard,
    .mode       = merged.mode,
    .opt        = merged.opt,
    .sanitizers = merged.sanitizers,
    .options    = merged.options,
    .targeted   = targeted,
  };
  return spn_result(SPN_OK);
}

