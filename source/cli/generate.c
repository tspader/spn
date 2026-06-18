#include "cli/cli.h"

#include "app/app.h"

sp_app_result_t spn_cli_generate(spn_cli_t* cli) {
  spn_cli_generate_t* command = &cli->generate;

  if (sp_str_valid(command->path) && !sp_str_valid(command->generator)) {
    SP_FATAL(
      "output path was specified, but no generator. try e.g.:\n  spn generate --path {} {.fg yellow}",
      SP_FMT_STR(command->path),
      SP_FMT_CSTR("--generator make")
    );
  }
  if (!sp_str_valid(command->generator)) command->generator = sp_str_lit("");
  if (!sp_str_valid(command->compiler)) command->compiler = sp_str_lit("gcc");

  if (!app.lock.some) {
    SP_FATAL("No lock file found. Run {.fg yellow} first.", SP_FMT_CSTR("spn build"));
  }

  spn_task_enqueue(&app.tasks, SPN_TASK_RESOLVE);
  spn_task_enqueue(&app.tasks, SPN_TASK_SYNC_PACKAGES);
  spn_task_enqueue(&app.tasks, SPN_TASK_RUN_CONFIGURE_GRAPH);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_GENERATE);

  return SP_APP_CONTINUE;
}
