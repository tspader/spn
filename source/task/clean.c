#include "app/types.h"
#include "ctx/types.h"

#include "sp/os.h"
#include "task/task.h"

spn_task_step_t spn_task_clean(spn_app_t* app) {
  bool whole_build = sp_str_empty(spn.cli.profile.name);
  sp_str_t path = whole_build ? app->session.paths.build : app->session.paths.profile;

  if (sp_fs_remove(path) != SP_OK) {
    return spn_task_fail(SPN_ERR_FS_REMOVE, .fs = { .path = path });
  }

  return spn_task_done();
}
