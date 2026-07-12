#include "app/types.h"
#include "ctx/types.h"

#include "sp/os.h"
#include "task/task.h"
#include "triple/triple.h"

spn_task_step_t spn_task_clean(spn_app_t* app) {
  bool whole_build = sp_str_empty(spn.cli.profile.name);
  sp_str_t path = app->session.paths.build;
  if (!whole_build) {
    if (app->session.profile.targeted) {
      spn_triple_t target = {
        app->session.profile.arch,
        app->session.profile.os,
        app->session.profile.abi,
      };
      path = sp_fs_join_path(app->session.mem, path, spn_triple_to_str(app->session.mem, target));
    }
    path = sp_fs_join_path(app->session.mem, path, app->session.profile.name);
  }

  if (sp_fs_remove(path) != SP_OK) {
    return spn_task_fail(SPN_ERR_FS_REMOVE, .fs = { .path = path });
  }

  return spn_task_done();
}
