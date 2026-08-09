#include "cli/cli.h"

sp_cli_result_t spn_cli_test(sp_cli_t* cli) {
  try(spn_cli_open(false));

  spn_str_arr_t names = spn_cli_rest_names(cli);

  spn_session_config_t config = {
    .selection = {
      .test = {
        .kind = names.count ? SPN_TARGET_RULE_NAMED : SPN_TARGET_RULE_ALL,
        .names = names,
      },
    },
  };
  try(spn_cli_refresh_indexes());
  try(spn_cli_session(cli, config));
  try(spn_cli_op(spn_build(host.session)));
  return spn_cli_op(spn_run_tests(host.session));
}
