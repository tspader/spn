#include "ctx/types.h"

#include "cli/cli.h"
#include "task/task.h"

sp_cli_result_t spn_cli_build(sp_cli_t* cli) {
  spn_cli_build_t* command = &spn.cli.build;

  app.config.force = command->force;
  app.config.filter = (spn_target_filter_t) {
    .name = command->name,
    .only = {
      .bin = command->only.bin,
      .lib = command->only.lib,
      .test = command->only.test,
      .script = command->only.script,
    },
    .disabled = {
      .script = sp_str_empty(command->name) && !command->only.script,
    }
  };

  return spn_plan(
    SPN_TASK_SYNC_INDEXES,
    SPN_TASK_RESOLVE,
    SPN_TASK_SYNC_PACKAGES,
    SPN_TASK_CONFIGURE_GRAPH,
    SPN_TASK_CREATE_UNITS,
    SPN_TASK_BUILD_GRAPH
  );
}
