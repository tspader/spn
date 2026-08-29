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
    .arch = (spn_arch_t)cmd->arch.value,
    .os = (spn_os_t)cmd->os.value,
    .abi = (spn_abi_t)cmd->abi.value,
  };

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
    .mode = (spn_mode_t)cmd->mode.value,
    .opt = (spn_opt_level_t)cmd->opt.value,
    .sanitizers = sanitizers,
    .sanitizers_set = sanitizers_set,
    .triple = spn_triple_merge(target, parts),
  };
  return SP_CLI_OK;
}
