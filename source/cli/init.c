#include "cli/cli.h"

#include "app/app.h"
#include "ctx/ctx.h"

sp_app_result_t spn_cli_init(spn_cli_t* cli) {
  spn_cli_init_t* cmd = &cli->init;

  spn_app_t app = spn_app_init_and_write(
    spn.paths.cwd,
    sp_fs_get_stem(spn.paths.cwd),
    cmd->bare ? SPN_APP_INIT_BARE : SPN_APP_INIT_NORMAL
  );

  SP_LOG("Initialized project {:fg brightcyan}. Run {:fg brightyellow} to build.", SP_FMT_STR(app.package.name), SP_FMT_CSTR("spn build"));
  return SP_APP_QUIT;
}
