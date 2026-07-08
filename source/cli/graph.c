#include "cli/cli.h"

sp_cli_result_t spn_cli_graph(sp_cli_t* cli) {
  app.config.filter = (spn_target_filter_t) {
    .disabled = {
      .public = false,
      .test = false,
    }
  };

  spn_task_enqueue(&app.tasks, SPN_TASK_RESOLVE);
  spn_task_enqueue(&app.tasks, SPN_TASK_SYNC_PACKAGES);
  spn_task_enqueue(&app.tasks, SPN_TASK_RUN_CONFIGURE_GRAPH);
  spn_task_enqueue(&app.tasks, SPN_TASK_CREATE_UNITS);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_RENDER_BUILD_GRAPH);

  return SP_CLI_CONTINUE;
}
