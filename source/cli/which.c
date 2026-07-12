#include "cli/cli.h"

#include "app/app.h"
#include "task/task.h"

sp_cli_result_t spn_cli_which(sp_cli_t* cli) {
  return spn_plan(
    SPN_TASK_SYNC_INDEXES,
    SPN_TASK_RESOLVE,
    SPN_TASK_SYNC_PACKAGES,
    SPN_TASK_PLAN,
    SPN_TASK_CONFIGURE_GRAPH,
    SPN_TASK_WHICH
  );
}
