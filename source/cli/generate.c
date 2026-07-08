#include "sp.h"
#include "sp/macro.h"
#include "cli/cli.h"

#include "ctx/types.h"
#include "log/log.h"

sp_cli_result_t spn_cli_generate(sp_cli_t* cli) {
  spn_cli_generate_t* command = &spn.cli.generate;

  if (sp_str_valid(command->path) && !sp_str_valid(command->generator)) {
    spn_log_error(
      "output path was specified, but no generator. try e.g.:\n  spn generate --path {} {.yellow}",
      SP_FMT_STR(command->path),
      SP_FMT_CSTR("--generator make")
    );
    return SP_CLI_ERR;
  }
  if (!sp_str_valid(command->generator)) command->generator = sp_str_lit("");
  if (!sp_str_valid(command->compiler)) command->compiler = sp_str_lit("gcc");

  if (!app.lock.some) {
    spn_log_error("no lock file found; run {.yellow} first", SP_FMT_CSTR("spn build"));
    return SP_CLI_ERR;
  }

  spn_task_enqueue(&app.tasks, SPN_TASK_RESOLVE);
  spn_task_enqueue(&app.tasks, SPN_TASK_SYNC_PACKAGES);
  spn_task_enqueue(&app.tasks, SPN_TASK_RUN_CONFIGURE_GRAPH);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_GENERATE);

  return SP_CLI_CONTINUE;
}
