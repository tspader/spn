#include "cli/cli.h"

#include "task/task.h"

sp_cli_result_t spn_cli_graph(sp_cli_t* cli) {
  app.config.filter = (spn_target_filter_t) {
    .disabled = {
      .public = false,
      .test = false,
    }
  };

  return spn_plan(
    SPN_TASK_SYNC_INDEXES,
    SPN_TASK_RESOLVE,
    SPN_TASK_SYNC_PACKAGES,
    SPN_TASK_CONFIGURE_GRAPH,
    SPN_TASK_CREATE_UNITS,
    SPN_TASK_RENDER_GRAPH
  );
}
