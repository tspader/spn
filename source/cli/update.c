#include "cli/cli.h"

#include "app/app.h"

sp_cli_result_t spn_cli_update(sp_cli_t* cli) {
  spn_task_enqueue(&app.tasks, SPN_TASK_RESOLVE);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_UPDATE);
  return SP_CLI_CONTINUE;
}
