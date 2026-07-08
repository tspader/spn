#include "app/types.h"
#include "ctx/types.h"

#include "cli/cli.h"
#include "event/event.h"
#include "sp/os.h"
#include "task/task.h"

spn_task_result_t spn_task_clean(spn_app_t* app) {
  bool whole_build = sp_str_empty(spn.cli.profile.name);
  sp_str_t path = whole_build ? app->session.paths.build : app->session.paths.profile;

  if (sp_fs_remove(path) != SP_OK) {
    spn_event_buffer_push(spn.events, (spn_build_event_t) {
      .kind = SPN_EVENT_ERR,
      .err = {
        .kind = SPN_ERR_FS_REMOVE,
        .fs = { .path = path },
      },
    });
    return SPN_TASK_ERROR;
  }

  return SPN_TASK_DONE;
}

sp_cli_result_t spn_cli_clean(sp_cli_t* cli) {
  spn_task_enqueue(&app.tasks, SPN_TASK_KIND_CLEAN);
  return SP_CLI_CONTINUE;
}
