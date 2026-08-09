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
#include "sp/queue.h"
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
  sp_cli_t cli;
} app;

static void on_wake(void* user_data) {
  sp_semaphore_signal(&host.wake);
}

static void on_signal(sp_os_signal_t signal, void* userdata) {
  switch (signal) {
    case SP_OS_SIGNAL_INTERRUPT: {
      sp_atomic_s32_store(&host.interrupted, 1, SP_ATOMIC_SEQ_CST);
      spn_op_t* op = (spn_op_t*)sp_atomic_ptr_load(&host.active, SP_ATOMIC_ACQUIRE);
      if (op) {
        spn_op_cancel(op);
      }
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
  sp_semaphore_init(&host.wake);
  host.ctx = spn_ctx_new(on_wake, SP_NULLPTR);
  sp_os_register_signal_handler(SP_OS_SIGNAL_INTERRUPT, on_signal, SP_NULLPTR);

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

  bool ok = status != SP_CLI_ERR;
  spn_prompt_stop(&tui, ok ? SP_PROMPT_STATE_SUBMIT : SP_PROMPT_STATE_ERROR);
  spn_ctx_close(host.ctx, ok);
  spn_tui_flush(&tui);
  sp_io_flush(&tui.logger.err.base);

  return ok ? 0 : 1;
}
SP_MAIN(spn_main)
