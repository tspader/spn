#include "app/types.h"
#include "cli/cli.h"

#include "cli/types.h"
#include "ctx/types.h"
#include "pkg/id.h"
#include "semver/parser.h"
#include "sp/sp_cli.h"
#include "task/task.h"

sp_cli_result_t spn_cli_add(sp_cli_t* cli) {
  spn_cli_add_t* cmd = &spn.cli.add;
  if (cmd->test && cmd->build) {
    return spn_cli_errf(cli, "pass at most one of {.cyan} and {.cyan}", sp_fmt_cstr("--test"), sp_fmt_cstr("--build"));
  }

  sp_str_pair_t request = sp_str_cleave_c8(cmd->package, '@');
  if (sp_str_empty(request.first)) {
    return spn_cli_errf(cli, "expected a package name");
  }

  spn_semver_range_t range = spn_semver_any();
  if (!sp_str_empty(request.second) && spn_semver_parse_range(request.second, &range)) {
    return spn_cli_errf(cli, "invalid version {.red}", sp_fmt_str(request.second));
  }

  app.request.add = (spn_add_request_t) {
    .name = spn_pkg_name_from_qualified(request.first),
    .key = sp_str_copy(spn.heap, request.first),
    .requested = sp_str_copy(spn.heap, request.second),
    .range = range,
    .dep = cmd->test ? SPN_ADD_DEP_TEST : cmd->build ? SPN_ADD_DEP_BUILD : SPN_ADD_DEP_PACKAGE,
  };

  return spn_plan(SPN_TASK_SYNC_INDEXES, SPN_TASK_ADD);
}
