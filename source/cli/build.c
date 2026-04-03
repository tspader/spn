#include "ctx/types.h"

#include "cli/cli.h"
#include "enum/enum.h"
#include "event/event.h"

sp_app_result_t spn_cli_build(spn_cli_t* cli) {
  spn_cli_build_t* command = &cli->build;

  app.config.force = command->force,
  app.config.filter = (spn_target_filter_t) {
    .name = command->name,
    .only = {
      .bin = command->only.bin,
      .lib = command->only.lib,
      .test = command->only.test,
      .script = command->only.script,
    },
    .disabled = {
      .test = sp_str_empty(command->name) && !command->only.test,
      .script = sp_str_empty(command->name) && !command->only.script,
    }
  };

  app.config.overrides = (spn_profile_info_t) {
    .name = command->profile,
    .toolchain = command->toolchain,
    .mode = sp_str_empty(command->mode) ? 0 : spn_dep_build_mode_from_str(command->mode),
  };

  spn_event_buffer_push_ex(spn.events, &app.package, SP_NULLPTR, (spn_build_event_t) {
    .kind = SPN_EVENT_CLI_ENTRY,
    .cli_entry = {
      .command = sp_str_lit("build"),
      .profile = command->profile,
      .target = command->name,
      .force = command->force,
      .cwd = spn.paths.cwd,
      .manifest = app.package.paths.manifest,
    }
  });

  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_RESOLVE);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_SYNC);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_CONFIGURE);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_PREPARE_BUILD_GRAPH);
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_RUN_BUILD_GRAPH);

  return SP_APP_CONTINUE;
}
