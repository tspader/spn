#include "cli/cli.h"
#include "spn/host.h"
#include "sp/sp_cli.h"

sp_cli_result_t spn_cli_add(sp_cli_t* cli) {
  spn_cli_add_t* cmd = &args.add;
  if (cmd->test && cmd->build) {
    return spn_cli_error(cli, "pass at most one of {.cyan} and {.cyan}", sp_fmt_cstr("--test"), sp_fmt_cstr("--build"));
  }

  sp_str_pair_t spec = sp_str_cleave_c8(cmd->package, '@');
  if (sp_str_empty(spec.first)) {
    return spn_cli_error(cli, "expected a package name");
  }

  spn_semver_range_t range = spn_semver_any();
  if (!sp_str_empty(spec.second) && spn_semver_parse_range(spec.second, &range)) {
    return spn_cli_error(cli, "invalid version {.red}", sp_fmt_str(spec.second));
  }

  spn_err_t err = spn_op_add(&spn, (spn_add_request_t) {
    .key = spec.first,
    .requested = spec.second,
    .range = range,
    .dep = cmd->test ? SPN_ADD_DEP_TEST : cmd->build ? SPN_ADD_DEP_BUILD : SPN_ADD_DEP_PACKAGE,
  });
  return err ? SP_CLI_ERR : SP_CLI_OK;
}
