#include "cli/cli.h"

#include "app/app.h"

sp_app_result_t spn_cli_test(spn_cli_t* cli) {
  spn_cli_test_t* command = &cli->test;

  app.config = (spn_app_config_t) {
    .filter = (spn_target_filter_t) {
      .name = command->target,
      .disabled = {
        .public = true,
        .script = true,
      }
    },
    .run = {
      .kind = SPN_RUN_KIND_TESTS,
    },
  };

  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_RESOLVE);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_SYNC);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_CONFIGURE);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_PREPARE_BUILD_GRAPH);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_RUN_BUILD_GRAPH);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_RUN);

  sp_try_as(spn_cli_set_profile(&app, command->profile), SP_APP_ERR);

  return SP_APP_CONTINUE;
}
