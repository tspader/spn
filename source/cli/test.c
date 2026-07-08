#include "cli/cli.h"

#include "ctx/types.h"

sp_cli_result_t spn_cli_test(sp_cli_t* cli) {
  spn_cli_test_t* command = &spn.cli.test;

  app.config.filter = (spn_target_filter_t) {
    .name = command->name,
    .disabled = {
      .public = true,
      .script = true,
    }
  };
  app.config.run = (spn_run_config_t) {
    .kind = SPN_RUN_KIND_TESTS,
  };

  spn_task_enqueue(&app.tasks, SPN_TASK_RESOLVE);
  spn_task_enqueue(&app.tasks, SPN_TASK_SYNC_PACKAGES);
  spn_task_enqueue(&app.tasks, SPN_TASK_RUN_CONFIGURE_GRAPH);
  spn_task_enqueue(&app.tasks, SPN_TASK_CREATE_UNITS);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_RUN_BUILD_GRAPH);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_RUN);

  return SP_CLI_CONTINUE;
}
