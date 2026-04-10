#include "cli/cli.h"

#include "app/app.h"

sp_app_result_t spn_cli_which(spn_cli_t* cli) {
  spn_task_enqueue(&app.tasks, SPN_TASK_RESOLVE);
  spn_task_enqueue(&app.tasks, SPN_TASK_SYNC_PACKAGES);
  spn_task_enqueue(&app.tasks, SPN_TASK_RUN_CONFIGURE_GRAPH);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_WHICH);
  return SP_APP_CONTINUE;
}
