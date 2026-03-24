#include "ctx/types.h"

#include "cli/cli.h"
#include "event/event.h"

sp_app_result_t spn_cli_clean(spn_cli_t* cli) {
  spn_cli_clean_t* cmd = &cli->clean;

  spn_build_ctx_t ctx = SP_ZERO_INITIALIZE();
  ctx.name = sp_str_lit("package");

  sp_str_t build_dir = sp_fs_join_path(app.paths.dir, sp_str_lit("build"));

  if (sp_str_valid(cmd->profile)) {
    sp_str_t profile_dir = sp_fs_join_path(build_dir, cmd->profile);
    if (sp_fs_exists(profile_dir)) {
      spn_event_buffer_push_ctx(spn.events, &ctx, (spn_build_event_t) {
        .kind = SPN_EVENT_CLEAN,
        .clean.path = profile_dir
      });
      sp_fs_remove_dir(profile_dir);
    }
  } else {
    if (sp_fs_exists(build_dir)) {
      spn_event_buffer_push_ctx(spn.events, &ctx, (spn_build_event_t) {
        .kind = SPN_EVENT_CLEAN,
        .clean.path = build_dir
      });
      sp_fs_remove_dir(build_dir);
    }

    if (sp_fs_exists(app.paths.lock)) {
      spn_event_buffer_push_ctx(spn.events, &ctx, (spn_build_event_t) {
        .kind = SPN_EVENT_CLEAN,
        .clean.path = app.paths.lock
      });
      sp_fs_remove_file(app.paths.lock);
    }
  }

  return SP_APP_QUIT;
}
