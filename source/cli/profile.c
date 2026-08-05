#include "cli/cli.h"

#include "enum/enum.h"
#include "profile/profile.h"
#include "triple/triple.h"

static spn_err_union_t invalid_flag(const c8* flag, sp_str_t value, const c8* expected) {
  return (spn_err_union_t) {
    .kind = SPN_ERR_FLAG_INVALID,
    .flag = {
      .name = sp_cstr_as_str(flag),
      .value = value,
      .expected = sp_cstr_as_str(expected),
    },
  };
}

spn_err_union_t spn_cli_parse_profile(spn_profile_args_t* args, spn_profile_info_t* result) {
  spn_triple_t target = sp_zero;
  if (!sp_str_empty(args->target) && spn_triple_parse(args->target, &target)) {
    return invalid_flag("--target", args->target, "an <arch>-<os>-<abi> triple like x86_64-linux-gnu");
  }

  spn_triple_t parts = {
    .arch = spn_arch_from_str(args->arch),
    .os = spn_os_from_str(args->os),
    .abi = spn_abi_from_str(args->abi),
  };
  if (!sp_str_empty(args->arch) && !parts.arch) {
    return invalid_flag("--arch", args->arch, "x86_64, aarch64, wasm32");
  }
  if (!sp_str_empty(args->os) && !parts.os) {
    return invalid_flag("--os", args->os, "linux, macos, windows, wasi");
  }
  if (!sp_str_empty(args->abi) && !parts.abi) {
    return invalid_flag("--abi", args->abi, "gnu, musl, msvc, mingw");
  }

  spn_build_mode_t mode = spn_build_mode_from_str(args->mode);
  if (!sp_str_empty(args->mode) && !mode) {
    return invalid_flag("--mode", args->mode, "debug, release");
  }

  spn_opt_level_t opt = spn_opt_level_from_str(args->opt);
  if (!sp_str_empty(args->opt) && !opt) {
    return invalid_flag("--opt", args->opt, "0, 1, 2, 3, s, z");
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
        return invalid_flag("--sanitize", args->sanitize, "a comma-separated list of address, thread, undefined, memory, leak, or none");
      }
      sanitizers |= sanitizer;
    }
    sp_mem_end_scratch(scratch);
    if (spn_sanitizer_set_conflicting(sanitizers)) {
      return invalid_flag("--sanitize", args->sanitize, "a compatible set (thread and memory don't combine with each other, address, or leak)");
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
