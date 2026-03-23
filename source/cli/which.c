#include "cli/cli.h"

#include "app/app.h"

sp_app_result_t spn_cli_which(spn_cli_t* cli) {
  sp_try(spn_cli_set_profile(&app, sp_str_lit("")));

  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_RESOLVE);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_SYNC);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_CONFIGURE);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_WHICH);
  return SP_APP_CONTINUE;
}
