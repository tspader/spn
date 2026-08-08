#include "sp.h"

#ifdef SP_WIN32
  #ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
  #endif

  #ifndef NOMINMAX
    #define NOMINMAX
  #endif

  #include <windows.h>
  #include <shlobj.h>
  #include <commdlg.h>
  #include <shellapi.h>
  #include <conio.h>
  #include <io.h>
#endif

#include "spn/host.h"

#include "cli/cli.h"
#include "tui/tui.h"

#define SP_IMPLEMENTATION
#include "sp.h"
#include "sp/atomic_file.h"
#include "sp/sp_prompt.h"
#include "sp/sp_cli.h"
#include "sp/sp_template.h"
#include "sp/coff.h"
#include "sp/sp_elf.h"
#include "sp/macho.h"

#define SP_MATH_IMPLEMENTATION
#include "sp/sp_math.h"

#define SP_GLOB_IMPLEMENTATION
#include "sp/sp_glob.h"

#define TOML_IMPLEMENTATION
#include "toml.h"

static struct {
  s32 num_args;
  const c8** args;
  bool initted;
  sp_cli_t cli;
} app;

static void on_signal(sp_os_signal_t signal, void* userdata) {
  switch (signal) {
    case SP_OS_SIGNAL_INTERRUPT: {
      spn_ctx_cancel(host.ctx);
      break;
    }
    case SP_OS_SIGNAL_ABORT:
    case SP_OS_SIGNAL_TERMINATE: {
      break;
    }
  }
}

static void print_error(sp_cli_err_t err) {
  sp_fmt_io(&tui.logger.err.base, "{.red}: ", sp_fmt_cstr("error"));
  sp_cli_err_print(&tui.logger.err.base, err);
  sp_fmt_io(&tui.logger.err.base, "\n");
}

static sp_app_result_t on_init(sp_app_t* sp) {
  spn_tui_init(&tui);

  sp_cli_parse((sp_cli_desc_t) {
    .root = spn_cli(),
    .args = app.args,
    .num_args = app.num_args,
  }, &app.cli);

  switch (app.cli.status) {
    case SP_CLI_HELP: {
      sp_cli_write_help(&tui.logger.out.base, &app.cli);
      return SP_APP_QUIT;
    }
    case SP_CLI_ERR: {
      print_error(app.cli.err);
      return SP_APP_ERR;
    }
    case SP_CLI_OK:
    case SP_CLI_CONTINUE: {
      break;
    }
  }

  spn_verbosity_t verbosity = SPN_VERBOSITY_NORMAL;
  if (args.quiet) {
    verbosity = SPN_VERBOSITY_QUIET;
  } else if (args.verbose) {
    verbosity = SPN_VERBOSITY_VERBOSE;
  }

  spn_tui_mode_t mode = SPN_OUTPUT_MODE_INTERACTIVE;
  if (!sp_str_empty(args.output)) {
    mode = spn_output_mode_from_str(args.output);
  }
  spn_tui_open(&tui, mode, verbosity);

  host.mem = sp_mem_arena_as_allocator(sp_mem_arena_new(sp_mem_os_new()));
  host.ctx = spn_ctx_new();
  app.initted = true;
  sp_os_register_signal_handler(SP_OS_SIGNAL_INTERRUPT, on_signal, SP_NULLPTR);

  return SP_APP_CONTINUE;
}

static sp_app_result_t on_update(sp_app_t* sp) {
  sp_cli_result_t status = sp_cli_dispatch(&app.cli);

  spn_prompt_stop(&tui, status != SP_CLI_ERR);
  spn_tui_flush(&tui);

  switch (status) {
    case SP_CLI_HELP: {
      sp_cli_write_help(&tui.logger.out.base, &app.cli);
      return SP_APP_QUIT;
    }
    case SP_CLI_ERR: {
      if (app.cli.err.kind) {
        print_error(app.cli.err);
      }
      return SP_APP_ERR;
    }
    case SP_CLI_OK:
    case SP_CLI_CONTINUE: {
      break;
    }
  }

  return SP_APP_QUIT;
}

static void on_deinit(sp_app_t* sp) {
  if (tui.mode == SPN_OUTPUT_MODE_INTERACTIVE) {
    spn_prompt_stop(&tui, true);
    sp_io_flush(&tui.logger.err.base);
  }

  if (app.initted) {
    spn_ctx_close(host.ctx, sp->result != SP_APP_ERR);
    spn_tui_flush(&tui);
  }
}

sp_app_config_t spn_main(s32 num_args, const c8** args) {
  app.args = args;
  app.num_args = num_args;

  return (sp_app_config_t) {
    .on_init = on_init,
    .on_update = on_update,
    .on_deinit = on_deinit,
    .fps = 144,
  };
}
SP_APP_MAIN(spn_main)
