#include "cli/cli.h"

#include "task/task.h"

sp_cli_result_t spn_cli_publish(sp_cli_t* cli) {
  return spn_plan(SPN_TASK_SYNC_INDEXES, SPN_TASK_PUBLISH);
}
