#include "sp.h"

#ifdef SP_WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif

  #ifndef NOMINMAX
    #define NOMINMAX
  #endif

#endif

#include "spn/host.h"

#include "commands/commands.h"
#include "commands/util/util.h"
#include "tui/tui.h"

static struct {
  s32 num_args;
  const c8** args;
  sp_cli_t cli;
} app;

static void print_error(sp_cli_err_t err) {
  sp_fmt_io(&tui.logger.err.base, "{.red}: ", sp_fmt_cstr("error"));
  sp_cli_err_print(&tui.logger.err.base, err);
  sp_fmt_io(&tui.logger.err.base, "\n");
}

s32 spn_main(s32 num_args, const c8** argv) {
  app.args = argv;
  app.num_args = num_args;

  spn_tui_init(&tui);

  sp_cli_parse((sp_cli_desc_t) {
    .root = spn_cli(),
    .args = app.args,
    .num_args = app.num_args,
  }, &app.cli);

  switch (app.cli.status) {
    case SP_CLI_HELP: {
      sp_cli_write_help(&tui.logger.out.base, &app.cli);
      return 0;
    }
    case SP_CLI_ERR: {
      print_error(app.cli.err);
      return 1;
    }
    case SP_CLI_OK:
    case SP_CLI_CONTINUE: {
      break;
    }
  }

  spn_cli_boot();

  sp_cli_result_t status = sp_cli_dispatch(&app.cli);

  switch (status) {
    case SP_CLI_HELP: {
      sp_cli_write_help(&tui.logger.out.base, &app.cli);
      break;
    }
    case SP_CLI_ERR: {
      if (app.cli.err.kind) {
        print_error(app.cli.err);
      }
      break;
    }
    case SP_CLI_OK:
    case SP_CLI_CONTINUE: {
      break;
    }
  }

  return spn_cli_shutdown(status != SP_CLI_ERR);
}
SP_MAIN(spn_main)
