#include "cli/cli.h"

sp_cli_result_t spn_cli_clean(sp_cli_t* cli) {
  try(spn_cli_open(false));

  if (sp_str_empty(args.profile.name)) {
    return spn_cli_op(spn_clean(host.ctx));
  }

  try(spn_cli_session(cli, sp_zero_s(spn_session_config_t)));
  return spn_cli_op(spn_clean_profile(host.session));
}
