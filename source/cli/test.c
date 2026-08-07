#include "cli/cli.h"

sp_cli_result_t spn_cli_test(sp_cli_t* cli) {
  spn_target_names_t names = sp_da_new(host.mem, sp_str_t);
  for (const c8** it = cli->rest; *it; it++) {
    sp_da_push(names, sp_cstr_as_str(*it));
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
  if (spn_op_build(host.session)) {
    return SP_CLI_ERR;
  }
  return spn_op_test(host.session) ? SP_CLI_ERR : SP_CLI_OK;
}
