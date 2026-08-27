#include "host/host.h"

static sp_cli_result_t invalid(sp_cli_t* cli, sp_str_t value, const c8* flag, const c8* expected) {
  return spn_cli_error(cli, "invalid value {.red} for {.yellow}; expected {}",
    sp_fmt_str(value),
    sp_fmt_cstr(flag),
    sp_fmt_cstr(expected));
}

sp_cli_result_t spn_cli_parse_profile(sp_cli_t* cli, spn_profile_override_t* profile) {
  spn_cli_profile_t* cmd = &host.args.profile;

  spn_triple_t target = sp_zero;
  if (!sp_str_empty(cmd->target) && spn_triple_parse(cmd->target, &target)) {
    return invalid(cli, cmd->target, "--target", "an <arch>-<os>-<abi> triple like x86_64-linux-gnu");
  }

  spn_triple_t parts = {
    .arch = spn_arch_from_str(cmd->arch),
    .os = spn_os_from_str(cmd->os),
    .abi = spn_abi_from_str(cmd->abi),
  };
  if (!sp_str_empty(cmd->arch) && !parts.arch) {
    return invalid(cli, cmd->arch, "--arch", "x86_64, aarch64, wasm32");
  }
  if (!sp_str_empty(cmd->os) && !parts.os) {
    return invalid(cli, cmd->os, "--os", "linux, macos, windows, wasi");
  }
  if (!sp_str_empty(cmd->abi) && !parts.abi) {
    return invalid(cli, cmd->abi, "--abi", "gnu, musl, msvc, apple");
  }

  spn_build_mode_t mode = spn_build_mode_from_str(cmd->mode);
  if (!sp_str_empty(cmd->mode) && !mode) {
    return invalid(cli, cmd->mode, "--mode", "debug, release");
  }

  spn_opt_level_t opt = spn_opt_level_from_str(cmd->opt);
  if (!sp_str_empty(cmd->opt) && !opt) {
    return invalid(cli, cmd->opt, "--opt", "0, 1, 2, 3, s, z");
  }

  spn_sanitizer_set_t sanitizers = sp_zero;
  bool sanitizers_set = false;
  if (sp_str_equal_cstr(cmd->sanitize, "none")) {
    sanitizers_set = true;
  }
  else if (!sp_str_empty(cmd->sanitize)) {
    sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
    sp_da(sp_str_t) names = sp_str_split_c8(scratch.mem, cmd->sanitize, ',');
    sp_da_for(names, it) {
      spn_sanitizer_t sanitizer = spn_sanitizer_from_str(names[it]);
      if (!sanitizer) {
        sp_mem_end_scratch(scratch);
        return invalid(cli, cmd->sanitize, "--sanitize", "a comma-separated list of address, thread, undefined, memory, leak, or none");
      }
      sanitizers |= sanitizer;
    }
    sp_mem_end_scratch(scratch);
    if (spn_sanitizer_set_has_conflict(sanitizers)) {
      return invalid(cli, cmd->sanitize, "--sanitize", "a compatible set (thread and memory don't combine with each other, address, or leak)");
    }
  }

  *profile = (spn_profile_override_t) {
    .name = cmd->name,
    .toolchain = cmd->toolchain,
    .mode = mode,
    .opt = opt,
    .sanitizers = sanitizers,
    .sanitizers_set = sanitizers_set,
    .triple = spn_triple_merge(target, parts),
  };
  return SP_CLI_OK;
}
