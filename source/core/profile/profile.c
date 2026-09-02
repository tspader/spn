#include "profile/profile.h"
#include "ctx/types.h"
#include "error/error.h"
#include "core/types.h"
#include "macro/macro.h"
#include "intern/intern.h"
#include "pkg/types.h"
#include "spn/core.h"
#include "triple/triple.h"

sp_str_t spn_profile_build_dir(sp_mem_t mem, const spn_profile_info_t* profile) {
  if (!profile->targeted) {
    return profile->name;
  }
  spn_triple_t target = { profile->arch, profile->os, profile->abi };
  return sp_fs_join_path(mem, spn_triple_to_str(mem, target), profile->name);
}

static void overlay_profile(spn_profile_info_t* to, spn_profile_info_t* from) {
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

static sp_str_t select_name(const spn_profile_override_t* override) {
  if (!sp_str_empty(override->name)) {
    return override->name;
  }

  if (override->mode == SPN_MODE_RELEASE) {
    return sp_str_lit("release");
  }

  return sp_str_lit("debug");
}

static spn_profile_info_t override_to_info(const spn_profile_override_t* override) {
  return (spn_profile_info_t) {
    .name = override->name,
    .toolchain = override->toolchain,
    .mode = override->mode,
    .opt = override->opt,
    .sanitizers = override->sanitizers,
    .sanitizers_set = override->sanitizers_set,
    .os = override->triple.os,
    .arch = override->triple.arch,
    .abi = override->triple.abi,
  };
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
    .mode      = SPN_MODE_DEBUG,
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
  p.debug.mode = SPN_MODE_DEBUG;
  sp_str_ht_insert(*profiles, p.debug.name, p.debug);

  p.release.name = sp_str_lit("release");
  p.release.mode = SPN_MODE_RELEASE;
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

static void push_abi(spn_abi_list_t* list, spn_abi_t abi) {
  sp_for(it, list->count) {
    if (list->items[it] == abi) {
      return;
    }
  }
  list->items[list->count++] = abi;
}

static spn_abi_list_t abi_order(const spn_profile_info_t* profile, spn_triple_t host) {
  spn_abi_list_t list = sp_zero;
  if (profile->abi) {
    push_abi(&list, profile->abi);
    return list;
  }

  const spn_abi_t* abis = SP_NULLPTR;
  u32 count = spn_os_abis(profile->os, &abis);
  bool native = profile->arch == host.arch && profile->os == host.os;
  if (!native && count > 1) {
    return list;
  }
  if (native && profile->os == SPN_OS_LINUX) {
    push_abi(&list, profile->linkage == SPN_LIB_KIND_SHARED ? host.abi : SPN_ABI_MUSL);
  }
  sp_for(it, count) {
    push_abi(&list, abis[it]);
  }
  return list;
}

spn_toolchain_query_t spn_profile_query(const spn_profile_info_t* profile, spn_triple_t host) {
  bool automatic = sp_str_equal_cstr(profile->toolchain, "auto");
  return (spn_toolchain_query_t) {
    .kind = automatic ? SPN_TOOLCHAIN_QUERY_AUTO : SPN_TOOLCHAIN_QUERY_NAMED,
    .name = profile->toolchain,
    .target = { profile->arch, profile->os, profile->abi },
    .abis = abi_order(profile, host),
  };
}

static spn_linkage_t abi_linkage(spn_abi_t abi) {
  switch (abi) {
    case SPN_ABI_GNU:
    case SPN_ABI_MSVC:
    case SPN_ABI_APPLE: {
      return SPN_LIB_KIND_SHARED;
    }
    case SPN_ABI_MUSL:
    case SPN_ABI_BARE: {
      return SPN_LIB_KIND_STATIC;
    }
    case SPN_ABI_NONE:
    case SPN_ABI_COUNT: {
      sp_unreachable_case();
    }
  }

  sp_unreachable_return(SPN_LIB_KIND_NONE);
}

void spn_profile_finalize(spn_profile_info_t* profile, spn_abi_t abi) {
  profile->abi = abi;
  if (!profile->linkage) {
    profile->linkage = abi_linkage(abi);
  }
}

static bool shared_demand(const spn_pkg_info_t* pkg) {
  sp_da_for(pkg->config, it) {
    const spn_pkg_config_t* config = &pkg->config[it].value;
    if (!sp_opt_is_null(config->kind) && config->kind.value == SPN_LIB_KIND_SHARED) {
      return true;
    }
  }
  sp_str_om_for(pkg->libs, it) {
    spn_linkage_set_t linkages = sp_str_om_at(pkg->libs, it)->linkages;
    if (linkages.shared && !linkages.static_lib && !linkages.source) {
      return true;
    }
  }
  return false;
}

static spn_linkage_t resolve_linkage(spn_linkage_t linkage, spn_os_t os, const spn_pkg_info_t* pkg) {
  if (linkage) {
    return linkage;
  }
  if (os == SPN_OS_FREESTANDING) {
    return SPN_LIB_KIND_STATIC;
  }
  if (shared_demand(pkg)) {
    return SPN_LIB_KIND_SHARED;
  }
  return SPN_LIB_KIND_NONE;
}

spn_err_t spn_profile_resolve(spn_profile_table_t profiles, const spn_profile_override_t* override, spn_triple_t host, const spn_pkg_info_t* pkg, spn_profile_info_t* result) {
  sp_str_t name = select_name(override);

  if (sp_str_find_c8(name, '/') >= 0 || sp_str_find_c8(name, '\\') >= 0) {
    return spn_err_emit(&spn, (spn_err_union_t) {
      .kind = SPN_ERR_PROFILE_INVALID,
      .profile = { .name = name },
    });
  }

  if (spn_triple_from_str(name).arch) {
    return spn_err_emit(&spn, (spn_err_union_t) {
      .kind = SPN_ERR_PROFILE_INVALID,
      .profile = { .name = name },
    });
  }

  spn_profile_info_t* info = sp_str_ht_get(profiles, name);
  if (!info) {
    return spn_err_emit(&spn, (spn_err_union_t) {
      .kind = SPN_ERR_PROFILE_UNDEFINED,
      .profile = { .name = name },
    });
  }

  spn_profile_info_t merged = *info;
  spn_profile_info_t lifted = override_to_info(override);
  overlay_profile(&merged, &lifted);

  spn_triple_t target = { merged.arch, merged.os, merged.abi };
  bool targeted = target.arch || target.os || target.abi;

  if (!merged.opt) {
    merged.opt = merged.mode == SPN_MODE_RELEASE ? SPN_OPT_LEVEL_2 : SPN_OPT_LEVEL_0;
  }

  spn_triple_t pinned = {
    .arch = target.arch ? target.arch : host.arch,
    .os = target.os ? target.os : host.os,
    .abi = target.abi,
  };
  if (!spn_os_has_arch(pinned.os, pinned.arch)) {
    return spn_err_emit(&spn, (spn_err_union_t) {
      .kind = SPN_ERR_PROFILE_ARCH,
      .profile = { .name = name, .target = pinned },
    });
  }
  if (pinned.abi && !spn_os_has_abi(pinned.os, pinned.abi)) {
    return spn_err_emit(&spn, (spn_err_union_t) {
      .kind = SPN_ERR_PROFILE_ABI,
      .profile = { .name = name, .target = pinned },
    });
  }

  *result = (spn_profile_info_t) {
    .name       = merged.name,
    .toolchain  = merged.toolchain,
    .os         = pinned.os,
    .arch       = pinned.arch,
    .abi        = pinned.abi,
    .linkage    = resolve_linkage(merged.linkage, pinned.os, pkg),
    .standard   = merged.standard,
    .mode       = merged.mode,
    .opt        = merged.opt,
    .sanitizers = merged.sanitizers,
    .options    = merged.options,
    .targeted   = targeted,
  };
  return SPN_OK;
}

spn_when_facts_t spn_profile_facts(const spn_profile_info_t* profile) {
  return (spn_when_facts_t) {
    .os = profile->os,
    .arch = profile->arch,
    .abi = profile->abi,
    .mode = profile->mode,
    .opt = profile->opt,
    .sanitizers = profile->sanitizers,
  };
}
