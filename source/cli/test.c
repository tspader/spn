#include "cli/cli.h"

#include "spn/host.h"

#include "tui/tui.h"

sp_cli_result_t spn_cli_test(sp_cli_t* cli) {
  spn_cli_test_t* cmd = &args.test;

  spn_target_names_t names = sp_da_new(spn.heap, sp_str_t);
  if (!sp_str_empty(cmd->name)) {
    sp_da_push(names, cmd->name);
  }
  spn_session_config_t config = {
    .selection = {
      .kind = SPN_TARGET_SELECTION_EXPLICIT,
      .test = {
        .kind = sp_da_empty(names) ? SPN_TARGET_RULE_ALL : SPN_TARGET_RULE_NAMED,
        .names = names,
      },
    },
  };
  sp_cli_result_t session = spn_cli_session(cli, config);
  if (session != SP_CLI_OK) {
    return session;
  }
  if (spn_op_build(spn.session)) {
    return SP_CLI_ERR;
  }
  spn_tui_handoff(&tui);
  return spn_cli_run_roots() ? SP_CLI_ERR : SP_CLI_OK;
}
