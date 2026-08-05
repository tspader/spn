#include "cli/cli.h"

#include "ctx/types.h"
#include "semver/parser.h"
#include "sp/sp_cli.h"

sp_cli_result_t spn_cli_add(sp_cli_t* cli) {
  spn_cli_add_t* cmd = &spn.cli.add;
  if (cmd->test && cmd->build) {
    return cli_error(cli, "pass at most one of {.cyan} and {.cyan}", sp_fmt_cstr("--test"), sp_fmt_cstr("--build"));
  }

  sp_str_pair_t request = sp_str_cleave_c8(cmd->package, '@');
  if (sp_str_empty(request.first)) {
    return cli_error(cli, "expected a package name");
  }

  spn_semver_range_t range = spn_semver_any();
  if (!sp_str_empty(request.second) && spn_semver_parse_range(request.second, &range)) {
    return cli_error(cli, "invalid version {.red}", sp_fmt_str(request.second));
  }

  spn.exec.desc = (spn_op_desc_t) {
    .kind = SPN_OP_ADD,
    .add = {
      .key = request.first,
      .requested = request.second,
      .range = range,
      .dep = cmd->test ? SPN_ADD_DEP_TEST : cmd->build ? SPN_ADD_DEP_BUILD : SPN_ADD_DEP_PACKAGE,
    },
  };
  return SP_CLI_CONTINUE;
}
