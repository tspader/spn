#include "cli/cli.h"

#include "spn/host.h"

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

  try_cli(spn_cli_open_session(config));
  try_cli(spn_op_build(spn.session));
  spn_cli_exec(cli)->finish = spn_cli_run_roots;
  return SP_CLI_OK;
}
