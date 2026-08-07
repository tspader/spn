#include "cli/cli.h"

sp_cli_result_t spn_cli_clean(sp_cli_t* cli) {
  sp_cli_result_t session = spn_cli_session(cli, sp_zero_s(spn_session_config_t));
  if (session != SP_CLI_OK) {
    return session;
  }
  spn_clean_scope_t scope = sp_str_empty(args.profile.name) ? SPN_CLEAN_ALL : SPN_CLEAN_PROFILE;
  return spn_op_clean(host.session, scope) ? SP_CLI_ERR : SP_CLI_OK;
}
