#include "cli/cli.h"

sp_cli_result_t spn_cli_add(sp_cli_t* cli) {
  sp_cli_result_t opened = spn_cli_open(false);
  if (opened != SP_CLI_OK) {
    return opened;
  }

  spn_cli_add_t* cmd = &args.add;
  if (cmd->test && cmd->build) {
    return spn_cli_error(cli, "pass at most one of {.cyan} and {.cyan}", sp_fmt_cstr("--test"), sp_fmt_cstr("--build"));
  }

  sp_str_pair_t spec = sp_str_cleave_c8(cmd->package, '@');
  if (sp_str_empty(spec.first)) {
    return spn_cli_error(cli, "expected a package name");
  }

  spn_err_t err = spn_op_add(host.ctx, (spn_add_request_t) {
    .name = spec.first,
    .version = spec.second,
    .kind = cmd->test ? SPN_DEP_KIND_TEST : cmd->build ? SPN_DEP_KIND_BUILD : SPN_DEP_KIND_PACKAGE,
  });
  return err ? SP_CLI_ERR : SP_CLI_OK;
}
