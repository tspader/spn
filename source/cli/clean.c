#include "cli/cli.h"

sp_cli_result_t spn_cli_clean(sp_cli_t* cli) {
  sp_cli_result_t opened = spn_cli_open(false);
  if (opened != SP_CLI_OK) {
    return opened;
  }

  if (sp_str_empty(args.profile.name)) {
    return spn_op_clean(host.ctx) ? SP_CLI_ERR : SP_CLI_OK;
  }

  sp_cli_result_t session = spn_cli_session(cli, sp_zero_s(spn_session_config_t));
  if (session != SP_CLI_OK) {
    return session;
  }
  return spn_op_clean_profile(host.session) ? SP_CLI_ERR : SP_CLI_OK;
}
