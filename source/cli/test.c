#include "cli/cli.h"

#include "ctx/types.h"
#include "task/task.h"

sp_cli_result_t spn_cli_test(sp_cli_t* cli) {
  spn_cli_test_t* command = &spn.cli.test;

  app.config.requests = sp_da_new(spn.heap, spn_build_request_t);
  sp_da_push(app.config.requests, ((spn_build_request_t) {
    .filter = {
      .name = command->name,
      .disabled = {
        .public = true,
        .script = true,
      },
    },
  }));
  app.config.run = (spn_run_config_t) {
    .kind = SPN_RUN_KIND_ROOTS,
  };

  return spn_plan(
    SPN_TASK_SYNC_INDEXES,
    SPN_TASK_RESOLVE,
    SPN_TASK_SYNC_PACKAGES,
    SPN_TASK_CONFIGURE_GRAPH,
    SPN_TASK_CREATE_UNITS,
    SPN_TASK_BUILD_GRAPH,
    SPN_TASK_RUN
  );
}
