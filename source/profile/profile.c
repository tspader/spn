#include "profile/profile.h"
#include "ctx/types.h"
#include "forward/types.h"
#include "sp/macro.h"
#include "enum/enum.h"
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

void spn_profile_overlay(spn_profile_info_t* dst, spn_profile_info_t* src) {
  if (!sp_str_empty(src->name)) {
    dst->name = src->name;
  }
  if (!sp_str_empty(src->toolchain)) {
    dst->toolchain = src->toolchain;
  }
  if (src->linkage) {
    dst->linkage = src->linkage;
  }
  if (src->standard) {
    dst->standard = src->standard;
  }
  if (src->mode) {
    dst->mode = src->mode;
  }
  if (src->opt) {
    dst->opt = src->opt;
  }
  if (src->sanitizers_set || src->sanitizers) {
    dst->sanitizers = src->sanitizers;
    dst->sanitizers_set = true;
  }
  if (src->os) {
    dst->os = src->os;
    dst->abi = src->abi;
  }
  else if (src->abi) {
    dst->abi = src->abi;
  }
  if (src->arch) {
    dst->arch = src->arch;
  }
  if (!sp_da_empty(src->options.clauses)) dst->options = src->options;
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
    spn_profile_overlay(sp_str_ht_get(*profiles, fallback.name), fallback.user);
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
      spn_profile_overlay(entry, user);
    } else {
      p.derived = base;
      p.derived.name = user->name;
      spn_profile_overlay(&p.derived, user);
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
  spn_profile_overlay(&merged, overrides);

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

static spn_err_union_t invalid_field(spn_profile_field_t field, sp_str_t value) {
  return (spn_err_union_t) {
    .kind = SPN_ERR_FLAG_INVALID,
    .flag = {
      .field = field,
      .value = value,
    },
  };
}

spn_err_union_t spn_profile_parse(spn_profile_args_t* args, spn_profile_info_t* result) {
  spn_triple_t target = sp_zero;
  if (!sp_str_empty(args->target) && spn_triple_parse(args->target, &target)) {
    return invalid_field(SPN_PROFILE_FIELD_TARGET, args->target);
  }

  spn_triple_t parts = {
    .arch = spn_arch_from_str(args->arch),
    .os = spn_os_from_str(args->os),
    .abi = spn_abi_from_str(args->abi),
  };
  if (!sp_str_empty(args->arch) && !parts.arch) {
    return invalid_field(SPN_PROFILE_FIELD_ARCH, args->arch);
  }
  if (!sp_str_empty(args->os) && !parts.os) {
    return invalid_field(SPN_PROFILE_FIELD_OS, args->os);
  }
  if (!sp_str_empty(args->abi) && !parts.abi) {
    return invalid_field(SPN_PROFILE_FIELD_ABI, args->abi);
  }

  spn_build_mode_t mode = spn_build_mode_from_str(args->mode);
  if (!sp_str_empty(args->mode) && !mode) {
    return invalid_field(SPN_PROFILE_FIELD_MODE, args->mode);
  }

  spn_opt_level_t opt = spn_opt_level_from_str(args->opt);
  if (!sp_str_empty(args->opt) && !opt) {
    return invalid_field(SPN_PROFILE_FIELD_OPT, args->opt);
  }

  spn_sanitizer_set_t sanitizers = 0;
  bool sanitizers_set = false;
  if (sp_str_equal_cstr(args->sanitize, "none")) {
    sanitizers_set = true;
  }
  else if (!sp_str_empty(args->sanitize)) {
    sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
    sp_da(sp_str_t) names = sp_str_split_c8(scratch.mem, args->sanitize, ',');
    sp_da_for(names, it) {
      spn_sanitizer_t sanitizer = spn_sanitizer_from_str(names[it]);
      if (!sanitizer) {
        sp_mem_end_scratch(scratch);
        return invalid_field(SPN_PROFILE_FIELD_SANITIZE, args->sanitize);
      }
      sanitizers |= sanitizer;
    }
    sp_mem_end_scratch(scratch);
    if (spn_sanitizer_set_conflicting(sanitizers)) {
      return invalid_field(SPN_PROFILE_FIELD_SANITIZE_CONFLICT, args->sanitize);
    }
  }

  target = spn_triple_merge(target, parts);

  *result = (spn_profile_info_t) {
    .name = args->name,
    .toolchain = args->toolchain,
    .mode = mode,
    .opt = opt,
    .sanitizers = sanitizers,
    .sanitizers_set = sanitizers_set,
    .os = target.os,
    .arch = target.arch,
    .abi = target.abi,
  };
  return spn_result(SPN_OK);
}
