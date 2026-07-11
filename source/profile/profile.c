#include "profile/profile.h"
#include "sp/macro.h"
#include "enum/enum.h"
#include "intern/intern.h"
#include "pkg/types.h"
#include "spn.h"
#include "triple/triple.h"

void spn_profile_overlay(spn_profile_info_t* dst, spn_profile_info_t* src) {
  if (!sp_str_empty(src->name))      dst->name = src->name;
  if (!sp_str_empty(src->toolchain)) dst->toolchain = src->toolchain;
  if (src->linkage)                  dst->linkage = src->linkage;
  if (src->standard)                 dst->standard = src->standard;
  if (src->mode)                     dst->mode = src->mode;
  if (src->opt)                      dst->opt = src->opt;
  if (src->sanitizers_set || src->sanitizers) {
    dst->sanitizers = src->sanitizers;
    dst->sanitizers_set = true;
  }
  if (src->os)                       dst->os = src->os;
  if (src->arch)                     dst->arch = src->arch;
  if (src->abi)                      dst->abi = src->abi;
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
  // 1. Seed the default profile with hardcoded base values
  spn_profile_info_t default_profile = {
    .name      = sp_str_lit("default"),
    .toolchain = sp_str_lit("system"),
    .linkage   = SPN_LIB_KIND_SHARED,
    .standard  = SPN_C11,
    .mode      = SPN_BUILD_MODE_DEBUG,
  };
  sp_str_ht_insert(*profiles, default_profile.name, default_profile);

  // 2. Apply user's [profile.default] if present
  sp_str_t default_name = spn_intern(sp_str_lit("default"));
  spn_profile_info_t* user_default = sp_str_om_has(pkg->profiles, default_name) ?
    sp_str_om_get(pkg->profiles, default_name) :
    SP_NULLPTR;
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

  // 4. Overlay remaining user profiles; new names derive from default like
  // debug and release do, so a profile setting one field inherits the rest
  sp_str_om_for(pkg->profiles, it) {
    spn_profile_info_t* user = sp_str_om_at(pkg->profiles, it);
    if (sp_str_equal(user->name, sp_str_lit("default"))) continue;
    spn_profile_info_t* entry = sp_str_ht_get(*profiles, user->name);
    if (entry) {
      spn_profile_overlay(entry, user);
    } else {
      spn_profile_info_t derived = base;
      derived.name = user->name;
      spn_profile_overlay(&derived, user);
      sp_str_ht_insert(*profiles, derived.name, derived);
    }
  }
}

spn_err_union_t spn_profile_resolve(spn_profile_table_t profiles, spn_profile_info_t* overrides, spn_profile_info_t* result) {
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

  // Resolve the target triple: fill empty fields with host values.
  spn_triple_t host = spn_triple_host();
  spn_triple_t target = { merged.arch, merged.os, merged.abi };
  bool targeted = target.arch || target.os || target.abi;
  target = spn_triple_merge(host, target);

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

sp_str_t spn_profile_identity_to_str(sp_mem_t mem, const spn_profile_info_t* profile) {
  spn_triple_t triple = { profile->arch, profile->os, profile->abi };
  return sp_fmt(mem, "toolchain={} triple={} mode={} opt={} sanitize={} linkage={} standard={}",
    sp_fmt_str(profile->toolchain),
    sp_fmt_str(spn_triple_to_str(mem, triple)),
    sp_fmt_str(spn_build_mode_to_str(profile->mode)),
    sp_fmt_str(spn_opt_level_to_str(profile->opt)),
    sp_fmt_str(spn_sanitizer_set_to_str(mem, profile->sanitizers)),
    sp_fmt_str(spn_linkage_to_str(profile->linkage)),
    sp_fmt_str(spn_c_standard_to_str(profile->standard))).value;
}

static spn_err_union_t spn_flag_invalid(const c8* flag, sp_str_t value, const c8* expected) {
  return (spn_err_union_t) {
    .kind = SPN_ERR_FLAG_INVALID,
    .flag = {
      .name = sp_str_view(flag),
      .value = value,
      .expected = sp_str_view(expected),
    },
  };
}

spn_err_union_t spn_profile_overrides_parse(spn_profile_args_t* args, spn_profile_info_t* result) {
  spn_triple_t target = SP_ZERO_INITIALIZE();
  if (!sp_str_empty(args->target)) {
    const c8* expected = "an <arch>-<os>-<abi> triple like x86_64-linux-gnu";

    sp_str_t segments [3] = SP_ZERO_INITIALIZE();
    u32 num_segments = 0;
    sp_str_t remaining = args->target;
    while (true) {
      s32 separator = sp_str_find_c8(remaining, '-');
      sp_str_t segment = separator < 0 ? remaining : sp_str_prefix(remaining, separator);
      if (sp_str_empty(segment) || num_segments == sp_carr_len(segments)) {
        return spn_flag_invalid("--target", args->target, expected);
      }
      segments[num_segments++] = segment;
      if (separator < 0) break;
      remaining = sp_str_suffix(remaining, remaining.len - separator - 1);
    }

    target.arch = spn_arch_from_str(segments[0]);
    if (!target.arch) {
      return spn_flag_invalid("--target", args->target, expected);
    }
    if (num_segments > 1) {
      target.os = spn_os_from_str(segments[1]);
      if (!target.os) {
        return spn_flag_invalid("--target", args->target, expected);
      }
    }
    if (num_segments > 2) {
      target.abi = spn_abi_from_str(segments[2]);
      if (!target.abi) {
        return spn_flag_invalid("--target", args->target, expected);
      }
    }
  }

  spn_triple_t parts = {
    .arch = spn_arch_from_str(args->arch),
    .os = spn_os_from_str(args->os),
    .abi = spn_abi_from_str(args->abi),
  };
  if (!sp_str_empty(args->arch) && !parts.arch) {
    return spn_flag_invalid("--arch", args->arch, "x86_64, aarch64, wasm32");
  }
  if (!sp_str_empty(args->os) && !parts.os) {
    return spn_flag_invalid("--os", args->os, "linux, macos, windows, wasi");
  }
  if (!sp_str_empty(args->abi) && !parts.abi) {
    return spn_flag_invalid("--abi", args->abi, "gnu, musl, msvc, mingw");
  }

  spn_build_mode_t mode = spn_build_mode_from_str(args->mode);
  if (!sp_str_empty(args->mode) && !mode) {
    return spn_flag_invalid("--mode", args->mode, "debug, release");
  }

  spn_opt_level_t opt = spn_opt_level_from_str(args->opt);
  if (!sp_str_empty(args->opt) && !opt) {
    return spn_flag_invalid("--opt", args->opt, "0, 1, 2, 3, s, z");
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
        return spn_flag_invalid("--sanitize", args->sanitize, "a comma-separated list of address, thread, undefined, memory, leak, or none");
      }
      sanitizers |= sanitizer;
    }
    sp_mem_end_scratch(scratch);
    if (spn_sanitizer_set_conflicting(sanitizers)) {
      return spn_flag_invalid("--sanitize", args->sanitize, "a compatible set (thread and memory don't combine with each other, address, or leak)");
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
