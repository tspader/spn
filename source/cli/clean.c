#include "app/types.h"
#include "ctx/types.h"

#include "cli/cli.h"
#include "log/log.h"
#include "profile/profile.h"
#include "sp/os.h"

static sp_str_t spn_clean_run(spn_cli_clean_t* command, sp_mem_arena_marker_t s) {
  sp_str_t path = app.session.paths.build;

  if (!sp_str_empty(command->profile)) {
    spn_profile_info_t overrides = { .name = command->profile };
    spn_profile_info_t profile = sp_zero;
    switch (spn_profile_resolve(app.session.profiles, &overrides, &profile)) {
      case SPN_OK: break;
      case SPN_ERR_PROFILE_INVALID: return sp_fmt(s.mem, "invalid profile {.cyan}", sp_fmt_str(command->profile)).value;
      default: return sp_fmt(s.mem, "profile {.cyan} isn't defined", sp_fmt_str(command->profile)).value;
    }
    path = sp_fs_join_path(s.mem, path, profile.name);
  }

  if (sp_fs_remove(path) != SP_OK) {
    return sp_fmt(s.mem, "failed to remove {.cyan}", sp_fmt_str(path)).value;
  }

  return sp_zero_s(sp_str_t);
}

sp_cli_result_t spn_cli_clean(sp_cli_t* cli) {
  sp_mem_arena_marker_t s = sp_mem_begin_scratch();
  sp_str_t error = spn_clean_run(&spn.cli.clean, s);
  if (!sp_str_empty(error)) {
    spn_log_error("{}", sp_fmt_str(error));
  }
  sp_mem_end_scratch(s);
  return sp_str_empty(error) ? SP_CLI_OK : SP_CLI_ERR;
}
