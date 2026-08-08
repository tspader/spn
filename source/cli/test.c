#include "cli/cli.h"

sp_cli_result_t spn_cli_test(sp_cli_t* cli) {
  sp_cli_result_t opened = spn_cli_open(false);
  if (opened != SP_CLI_OK) {
    return opened;
  }

  spn_str_arr_t names = spn_cli_rest_names(cli);

  spn_session_config_t config = {
    .selection = {
      .test = {
        .kind = names.count ? SPN_TARGET_RULE_NAMED : SPN_TARGET_RULE_ALL,
        .names = names,
      },
    },
  };
  sp_cli_result_t session = spn_cli_session(cli, config);
  if (session != SP_CLI_OK) {
    return session;
  }
  if (spn_cli_op(spn_build(host.session)) != SP_CLI_OK) {
    return SP_CLI_ERR;
  }
  return spn_cli_op(spn_run_tests(host.session));
}
