#include "cli/cli.h"

#include "tui/tui.h"

sp_cli_result_t spn_cli_run(sp_cli_t* cli) {
  sp_cli_result_t opened = spn_cli_open(false);
  if (opened != SP_CLI_OK) {
    return opened;
  }

  spn_cli_run_t* cmd = &args.run;

  spn_target_kind_t kind = sp_zero;
  bool script = spn_ctx_find_target(host.ctx, cmd->entry, &kind) && kind == SPN_TARGET_KIND_SCRIPT;

  if (!script && sp_str_equal(sp_fs_get_ext(cmd->entry), sp_str_lit("c"))) {
    return spn_cli_error(cli, "{.yellow} cannot run native sources; build scripts are wasm", sp_fmt_str(cmd->entry));
  }

  if (!script) {
    return spn_cli_error(cli, "script target {.yellow} is not defined", sp_fmt_str(cmd->entry));
  }

  spn_session_config_t config = {
    .selection = {
      .script = {
        .kind = SPN_TARGET_RULE_NAMED,
        .names = { .items = &cmd->entry, .count = 1 },
      },
    },
  };
  sp_cli_result_t session = spn_cli_session(cli, config);
  if (session != SP_CLI_OK) {
    return session;
  }
  if (spn_op_build(host.session)) {
    return SP_CLI_ERR;
  }

  spn_target_t* target = spn_session_find_target(host.session, cmd->entry);
  sp_assert(target);

  spn_tui_handoff(&tui);
  spn_tui_run_banner(&tui, spn_target_name(target), spn_target_path(host.mem, target));
  return spn_op_run_target(host.session, target) ? SP_CLI_ERR : SP_CLI_OK;
}
