#include "profile/profile.h"
#include "ctx/types.h"
#include "error/error.h"
#include "core/types.h"
#include "enum/enum.h"
#include "macro/macro.h"
#include "intern/intern.h"
#include "pkg/types.h"
#include "spn/core.h"
#include "toolchain/toolchain.h"
#include "triple/triple.h"
#include "when/when.h"

sp_str_t spn_profile_build_dir(sp_mem_t mem, const spn_profile_info_t* profile) {
  if (!profile->targeted) {
    return profile->name;
  }
  return sp_fs_join_path(mem, spn_triple_to_str(mem, spn_profile_triple(profile)), profile->name);
}

static void overlay_profile(spn_profile_info_t* to, const spn_profile_info_t* from) {
  if (from->toolchain.kind) {
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

static spn_mode_t builtin_mode(sp_str_t name) {
  if (sp_str_equal_cstr(name, "release")) {
    return SPN_MODE_RELEASE;
  }
  if (sp_str_equal_cstr(name, "debug")) {
    return SPN_MODE_DEBUG;
  }
  return SPN_MODE_NONE;
}

static bool is_builtin(sp_str_t name) {
  return sp_str_equal_cstr(name, "default") || builtin_mode(name) != SPN_MODE_NONE;
}

static const spn_profile_decl_t* find_decl(spn_profile_map_t profiles, sp_str_t name) {
  spn_profile_decl_t** slot = sp_str_om_getp(profiles, name);
  return slot ? *slot : SP_NULLPTR;
}

static spn_triple_t decl_platform(const spn_profile_decl_t* decl) {
  return (spn_triple_t) { .arch = decl->arch, .os = decl->os };
}

static sp_str_t pick(spn_gated_list_t candidates, spn_when_env_t* env) {
  sp_da_for(candidates, it) {
    if (spn_when_eval(&candidates[it].when, env)) {
      return candidates[it].value;
    }
  }
  return sp_str_lit("");
}

static spn_profile_info_t evaluate(const spn_profile_decl_t* decl, spn_when_env_t* env) {
  return (spn_profile_info_t) {
    .toolchain = spn_toolchain_ref_from_str(pick(decl->toolchain, env)),
    .os = decl->os,
    .arch = decl->arch,
    .abi = spn_abi_from_str(pick(decl->abi, env)),
    .linkage = spn_linkage_from_str(pick(decl->linkage, env)),
    .standard = spn_c_standard_from_str(pick(decl->standard, env)),
    .mode = spn_mode_from_str(pick(decl->mode, env)),
    .opt = spn_opt_level_from_str(pick(decl->opt, env)),
    .sanitizers = decl->sanitizers,
    .sanitizers_set = decl->sanitizers_set,
    .options = decl->options,
  };
}

static spn_profile_info_t override_to_info(const spn_profile_override_t* override) {
  return (spn_profile_info_t) {
    .toolchain = spn_toolchain_ref_from_str(override->toolchain),
    .mode = override->mode,
    .opt = override->opt,
    .sanitizers = override->sanitizers,
    .sanitizers_set = override->sanitizers_set,
    .os = override->triple.os,
    .arch = override->triple.arch,
    .abi = override->triple.abi,
  };
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

  bool native = profile->arch == host.arch && profile->os == host.os;
  if (!native) {
    return list;
  }
  if (profile->os == SPN_OS_LINUX) {
    push_abi(&list, profile->linkage == SPN_LIB_KIND_SHARED ? host.abi : SPN_ABI_MUSL);
  }
  const spn_abi_t* abis = SP_NULLPTR;
  u32 count = spn_os_completions(profile->os, &abis);
  sp_for(it, count) {
    push_abi(&list, abis[it]);
  }
  return list;
}

spn_toolchain_query_t spn_profile_query(const spn_profile_info_t* profile, spn_triple_t host) {
  return (spn_toolchain_query_t) {
    .toolchain = profile->toolchain,
    .target = spn_profile_triple(profile),
    .abis = abi_order(profile, host),
    .sanitizers = profile->sanitizers,
    .linkage = profile->linkage,
  };
}

void spn_profile_finalize(spn_profile_info_t* profile, const spn_toolchain_selection_t* selection) {
  profile->abi = selection->row.triple.abi;
  profile->driver = selection->toolchain->driver;
  profile->linker = selection->toolchain->lld ? SPN_LD_FAMILY_LLD : spn_ld_native(selection->toolchain->driver, selection->row.triple);
  profile->sdk = selection->row.sdk;
  if (!profile->linkage) {
    profile->linkage = spn_abi_linkage(profile->abi);
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

static spn_linkage_t resolve_linkage(spn_linkage_t linkage, spn_triple_t target, const spn_pkg_info_t* pkg) {
  if (linkage) {
    return linkage;
  }
  if (!spn_triple_dynamic(target)) {
    return SPN_LIB_KIND_STATIC;
  }
  if (shared_demand(pkg)) {
    return SPN_LIB_KIND_SHARED;
  }
  return SPN_LIB_KIND_NONE;
}

spn_err_t spn_profile_resolve(const spn_profile_override_t* override, spn_triple_t host, const spn_pkg_info_t* pkg, spn_profile_info_t* result) {
  sp_str_t name = select_name(override);

  if (sp_str_find_c8(name, '/') >= 0 || sp_str_find_c8(name, '\\') >= 0) {
    return spn_err_emit(&spn, (spn_err_union_t) {
      .kind = SPN_ERR_PROFILE_INVALID,
      .profile = { .name = name },
    });
  }

  spn_triple_t collision = sp_zero;
  if (spn_triple_parse(name, &collision) == SPN_OK) {
    return spn_err_emit(&spn, (spn_err_union_t) {
      .kind = SPN_ERR_PROFILE_INVALID,
      .profile = { .name = name },
    });
  }

  const spn_profile_decl_t* selected = find_decl(pkg->profiles, name);
  if (!selected && !is_builtin(name)) {
    return spn_err_emit(&spn, (spn_err_union_t) {
      .kind = SPN_ERR_PROFILE_UNDEFINED,
      .profile = { .name = name },
    });
  }

  spn_profile_decl_t none = sp_zero;
  const spn_profile_decl_t* base = find_decl(pkg->profiles, sp_str_lit("default"));
  base = base ? base : &none;
  selected = selected ? selected : &none;

  spn_triple_t platform = { .arch = host.arch, .os = host.os };
  platform = spn_triple_merge(platform, decl_platform(base));
  platform = spn_triple_merge(platform, decl_platform(selected));
  platform = spn_triple_merge(platform, (spn_triple_t) { .arch = override->triple.arch, .os = override->triple.os });

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  spn_when_env_t env;
  spn_when_env_init(scratch.mem, &env);
  spn_when_env_set_platform(&env, platform.os, platform.arch);
  spn_profile_info_t from_base = evaluate(base, &env);
  spn_profile_info_t from_selected = evaluate(selected, &env);
  sp_mem_end_scratch(scratch);

  spn_profile_info_t builtin = { .mode = builtin_mode(name) };
  spn_profile_info_t lifted = override_to_info(override);
  spn_profile_info_t merged = {
    .toolchain = { .kind = SPN_TOOLCHAIN_REF_AUTO },
    .standard = SPN_C11,
    .mode = SPN_MODE_DEBUG,
  };
  overlay_profile(&merged, &from_base);
  overlay_profile(&merged, &builtin);
  overlay_profile(&merged, &from_selected);
  overlay_profile(&merged, &lifted);

  bool targeted = merged.arch || merged.os || merged.abi;

  if (!merged.opt) {
    merged.opt = merged.mode == SPN_MODE_RELEASE ? SPN_OPT_LEVEL_2 : SPN_OPT_LEVEL_0;
  }

  spn_triple_t pinned = { .arch = platform.arch, .os = platform.os, .abi = merged.abi };
  spn_triple_t full = sp_zero;
  switch (spn_triple_entry(pinned, &full)) {
    case SPN_TRIPLE_ENTRY_OK: {
      pinned = full;
      break;
    }
    case SPN_TRIPLE_ENTRY_MISSING_ABI: {
      break;
    }
    case SPN_TRIPLE_ENTRY_FOREIGN_ARCH: {
      return spn_err_emit(&spn, (spn_err_union_t) {
        .kind = SPN_ERR_PROFILE_ARCH,
        .profile = { .name = name, .target = pinned, .targets = spn_arch_triples(spn.mem, pinned.arch) },
      });
    }
    case SPN_TRIPLE_ENTRY_FOREIGN_ABI: {
      return spn_err_emit(&spn, (spn_err_union_t) {
        .kind = SPN_ERR_PROFILE_ABI,
        .profile = { .name = name, .target = pinned, .targets = spn_os_triples(spn.mem, pinned.arch, pinned.os) },
      });
    }
    case SPN_TRIPLE_ENTRY_MISSING_ARCH:
    case SPN_TRIPLE_ENTRY_MISSING_OS: {
      sp_unreachable_case();
    }
  }
  if (merged.linkage == SPN_LIB_KIND_SHARED && !spn_triple_dynamic(pinned)) {
    return spn_err_emit(&spn, (spn_err_union_t) {
      .kind = SPN_ERR_PROFILE_LINKAGE,
      .profile = { .name = name, .target = pinned },
    });
  }

  *result = (spn_profile_info_t) {
    .name       = name,
    .toolchain  = merged.toolchain,
    .os         = pinned.os,
    .arch       = pinned.arch,
    .abi        = pinned.abi,
    .linkage    = resolve_linkage(merged.linkage, pinned, pkg),
    .standard   = merged.standard,
    .mode       = merged.mode,
    .opt        = merged.opt,
    .sanitizers = merged.sanitizers,
    .options    = merged.options,
    .targeted   = targeted,
  };
  return SPN_OK;
}

spn_profile_info_t spn_profile_metaprogram(void) {
  return (spn_profile_info_t) {
    .name = sp_str_lit("metaprogram"),
    .toolchain = { .kind = SPN_TOOLCHAIN_REF_AUTO },
    .arch = SPN_ARCH_WASM32,
    .os = SPN_OS_WASI,
    .abi = SPN_ABI_MUSL,
    .mode = SPN_MODE_DEBUG,
    .opt = SPN_OPT_LEVEL_2,
    .standard = SPN_C99,
    .linkage = SPN_LIB_KIND_STATIC,
  };
}

spn_when_facts_t spn_profile_facts(const spn_profile_info_t* profile) {
  return (spn_when_facts_t) {
    .os = profile->os,
    .arch = profile->arch,
    .abi = profile->abi,
    .driver = profile->driver,
    .linker = profile->linker,
    .mode = profile->mode,
    .opt = profile->opt,
    .sanitizers = profile->sanitizers,
  };
}
