#include "cli/cli.h"

#include "app/app.h"
#include "ctx/ctx.h"
#include "event/event.h"
#include "log/log.h"

sp_app_result_t spn_cli_build(spn_cli_t* cli) {
  spn_cli_build_t* command = &cli->build;

  app.config = (spn_app_config_t) {
    .force = command->force,
    .filter = (spn_target_filter_t) {
      .name = command->target,
      .disabled = {
        .test = sp_str_empty(command->target) && !command->tests,
        .script = sp_str_empty(command->target),
      }
    },
  };

  sp_try(spn_cli_set_profile(&app, command->profile));

  spn_event_buffer_push_ex(spn.events, &app.package, SP_NULLPTR, (spn_build_event_t) {
    .kind = SPN_EVENT_CLI_ENTRY,
    .cli_entry = {
      .command = sp_str_lit("build"),
      .profile = app.config.profile ? app.config.profile->name : sp_str_lit(""),
      .target = command->target,
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
