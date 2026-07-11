#include "ctx/types.h"

#include "cli/cli.h"
#include "task/task.h"

sp_cli_result_t spn_cli_build(sp_cli_t* cli) {
  spn_cli_build_t* command = &spn.cli.build;

  app.config.force = command->force;
  app.config.requests = sp_da_new(spn.heap, spn_build_request_t);
  spn_target_filter_t filter = {
    .only = {
      .bin = command->only.bin,
      .lib = command->only.lib,
      .test = command->only.test,
      .script = command->only.script,
    },
    .disabled = {
      .script = !command->only.script,
    }
  };
  if (cli->num_rest) {
    sp_for(it, cli->num_rest) {
      filter.name = sp_cstr_as_str(cli->rest[it]);
      filter.disabled.script = false;
      sp_da_push(app.config.requests, ((spn_build_request_t) {
        .filter = filter,
      }));
    }
  }
  else {
    sp_da_push(app.config.requests, ((spn_build_request_t) {
      .filter = filter,
    }));
  }

  return spn_plan(
    SPN_TASK_SYNC_INDEXES,
    SPN_TASK_RESOLVE,
    SPN_TASK_SYNC_PACKAGES,
    SPN_TASK_CONFIGURE_GRAPH,
    SPN_TASK_CREATE_UNITS,
    SPN_TASK_BUILD_GRAPH
  );
}
