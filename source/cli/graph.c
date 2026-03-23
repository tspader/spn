#include "cli/cli.h"

#include "app/app.h"

sp_app_result_t spn_cli_graph(spn_cli_t* cli) {
  spn_cli_build_t* command = &cli->build;

  app.config = (spn_app_config_t) {
    .force = command->force,
    .filter = (spn_target_filter_t) {
      .name = command->target,
      .disabled = {
        .public = false,
        .test = false,
      }
    },
  };

  sp_try(spn_cli_set_profile(&app, command->profile));

  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_RESOLVE);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_SYNC);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_CONFIGURE);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_PREPARE_BUILD_GRAPH);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_RENDER_BUILD_GRAPH);

  return SP_APP_CONTINUE;
}
