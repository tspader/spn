#include "cli/cli.h"

#include "app/app.h"
#include "enum/enum.h"

sp_app_result_t spn_cli_test(spn_cli_t* cli) {
  spn_cli_test_t* command = &cli->test;

  app.config.filter = (spn_target_filter_t) {
    .name = command->name,
    .disabled = {
      .public = true,
      .script = true,
    }
  };
  app.config.run = (spn_run_config_t) {
    .kind = SPN_RUN_KIND_TESTS,
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
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_RUN_BUILD_GRAPH);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_RUN);

  return SP_APP_CONTINUE;
}
