#include "app/app.h"
#include "cli.h"
#include "ctx/ctx.h"
#include "session/session.h"
#include "unit/build.h"

spn_task_result_t spn_task_which(spn_app_t* app) {
  spn_cli_which_t* cmd = &spn.cli.which;

  spn_pkg_dir_t kind = SPN_DIR_STORE;
  if (sp_str_valid(cmd->dir)) {
    kind = spn_cache_dir_kind_from_str(cmd->dir);
  }

  if (sp_str_valid(cmd->package)) {
    spn_pkg_unit_t* dep = spn_session_find_pkg_or_assert(&app->session, cmd->package);
    sp_str_t dir = spn_build_ctx_get_dir(&dep->ctx, kind);
    spn_log_info("{}", SP_FMT_STR(dir));
  }
  else {
    spn_log_info("{}", SP_FMT_STR(spn_cache_dir_kind_to_path(kind)));
  }

  return SPN_TASK_DONE;
}
