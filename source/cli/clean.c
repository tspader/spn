#include "cli/cli.h"

#include "spn/host.h"

sp_cli_result_t spn_cli_clean(sp_cli_t* cli) {
  try_cli(spn_cli_open_session(sp_zero_s(spn_session_config_t)));
  return spn_cli_result(cli, spn_op_clean(spn.session, sp_str_empty(args.profile.name)));
}
