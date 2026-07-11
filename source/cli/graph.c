#include "cli/cli.h"

#include "ctx/types.h"
#include "task/task.h"

sp_cli_result_t spn_cli_graph(sp_cli_t* cli) {
  app.config.requests = sp_da_new(spn.heap, spn_build_request_t);
  sp_da_push(app.config.requests, ((spn_build_request_t) {
    .filter = {
      .disabled = {
        .public = false,
        .test = false,
      },
    },
  }));

  return spn_plan(
    SPN_TASK_SYNC_INDEXES,
    SPN_TASK_RESOLVE,
    SPN_TASK_SYNC_PACKAGES,
    SPN_TASK_CONFIGURE_GRAPH,
    SPN_TASK_CREATE_UNITS,
    SPN_TASK_RENDER_GRAPH
  );
}
