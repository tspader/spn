#include "cli/cli.h"

#include "app/app.h"
#include "ctx/ctx.h"
#include "session/session.h"
#include "unit/build.h"

sp_app_result_t spn_cli_copy(spn_cli_t* cli) {
  spn_cli_copy_t* cmd = &cli->copy;

  sp_str_t destination = sp_fs_normalize_path(cmd->directory);
  sp_str_t to = sp_fs_join_path(spn.paths.cwd, destination);
  sp_fs_create_dir(to);

  sp_om_for(app.session.units.packages, it) {
    spn_pkg_unit_t* dep = sp_om_at(app.session.units.packages, it);
    spn_build_ctx_t* ctx = &dep->ctx;

    sp_dyn_array(sp_fs_entry_t) entries = sp_fs_collect(ctx->paths.lib);
    sp_dyn_array_for(entries, i) {
      sp_fs_entry_t* entry = entries + i;
      sp_fs_copy_file(
        entry->file_path,
        sp_fs_join_path(to, sp_fs_get_name(entry->file_path))
      );
    }
  }
  return SP_APP_QUIT;
}
