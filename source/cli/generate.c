#include "sp.h"
#include "sp/macro.h"
#include "cli/cli.h"

#include "ctx/types.h"
#include "task/task.h"

sp_cli_result_t spn_cli_generate(sp_cli_t* cli) {
  spn_cli_generate_t* command = &spn.cli.generate;

  if (sp_str_valid(command->path) && !sp_str_valid(command->generator)) {
    return spn_cli_errf(cli,
      "output path was specified, but no generator. try e.g.:\n  spn generate --path {} {.yellow}",
      sp_fmt_str(command->path),
      sp_fmt_cstr("--generator make")
    );
  }
  if (!sp_str_valid(command->generator)) command->generator = sp_str_lit("");
  if (!sp_str_valid(command->compiler)) command->compiler = sp_str_lit("gcc");

  if (!app.lock.some) {
    return spn_cli_errf(cli, "no lock file found; run {.yellow} first", sp_fmt_cstr("spn build"));
  }

  return spn_plan(
    SPN_TASK_SYNC_INDEXES,
    SPN_TASK_RESOLVE,
    SPN_TASK_SYNC_PACKAGES,
    SPN_TASK_PLAN,
    SPN_TASK_CONFIGURE_GRAPH,
    SPN_TASK_GENERATE
  );
}
