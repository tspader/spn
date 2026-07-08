#include "ctx/types.h"

#include "cli/cli.h"

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

  spn_task_enqueue(&app.tasks, SPN_TASK_RESOLVE);
  spn_task_enqueue(&app.tasks, SPN_TASK_SYNC_PACKAGES);
  spn_task_enqueue(&app.tasks, SPN_TASK_RUN_CONFIGURE_GRAPH);
  spn_task_enqueue(&app.tasks, SPN_TASK_CREATE_UNITS);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_RUN_BUILD_GRAPH);

  return SP_CLI_CONTINUE;
}
