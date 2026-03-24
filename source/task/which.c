#include "app/types.h"
#include "cli/types.h"

#include "enum/enum.h"
#include "session/session.h"
#include "log.h"
#include "unit/build.h"

spn_task_result_t spn_task_which(spn_app_t* app, spn_cli_which_t* which) {
  spn_pkg_dir_t kind = SPN_DIR_STORE;
  if (sp_str_valid(which->dir)) {
    kind = spn_cache_dir_kind_from_str(which->dir);
  }

  if (sp_str_valid(which->package)) {
    spn_pkg_unit_t* dep = spn_session_find_pkg_or_assert(&app->session, which->package);
    sp_str_t dir = spn_build_ctx_get_dir(&dep->ctx, kind);
    spn_log_info("{}", SP_FMT_STR(dir));
  }
  else {
    spn_log_info("{}", SP_FMT_STR(spn_cache_dir_kind_to_path(kind)));
  }

  return SPN_TASK_DONE;
}
