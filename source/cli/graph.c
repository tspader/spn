#include "cli/cli.h"

#include "app/app.h"
#include "enum/enum.h"

sp_app_result_t spn_cli_graph(spn_cli_t* cli) {
  spn_cli_build_t* command = &cli->build;

  app.config.force = command->force;
  app.config.filter = (spn_target_filter_t) {
    .name = command->name,
    .disabled = {
      .public = false,
      .test = false,
    }
  };
  app.config.overrides = (spn_profile_info_t) {
    .name = command->profile,
    .toolchain = command->toolchain,
    .mode = sp_str_empty(command->mode) ? 0 : spn_build_mode_from_str(command->mode),
  };

  spn_task_enqueue(&app.tasks, SPN_TASK_RESOLVE);
  spn_task_enqueue(&app.tasks, SPN_TASK_SYNC_PACKAGES);
  spn_task_enqueue(&app.tasks, SPN_TASK_RUN_CONFIGURE_GRAPH);
  spn_task_enqueue(&app.tasks, SPN_TASK_CREATE_UNITS);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_RENDER_BUILD_GRAPH);

  return SP_APP_CONTINUE;
}
