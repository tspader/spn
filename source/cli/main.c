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
  bool booted;
  sp_cli_t cli;
  spn_cli_exec_t exec;
  struct {
    sp_thread_t thread;
    sp_atomic_s32_t done;
    sp_cli_result_t status;
  } dispatch;
} entry;

static void on_signal(sp_os_signal_t signal, void* userdata) {
  switch (signal) {
    case SP_OS_SIGNAL_INTERRUPT: {
      spn_ctx_cancel(&spn);
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

static s32 on_thread(void* userdata) {
  entry.dispatch.status = sp_cli_dispatch(&entry.cli);
  sp_atomic_s32_set(&entry.dispatch.done, 1);
  return 0;
}

static sp_app_result_t on_init(sp_app_t* sp) {
  spn_tui_init(&tui);

  entry.cli = sp_cli_parse((sp_cli_desc_t) {
    .root = spn_cli(),
    .args = entry.args,
    .num_args = entry.num_args,
    .user_data = &entry.exec,
  });

  switch (entry.cli.status) {
    case SP_CLI_HELP: {
      sp_cli_write_help(&tui.logger.out.base, &entry.cli);
      return SP_APP_QUIT;
    }
    case SP_CLI_ERR: {
      print_error(entry.cli.err);
      return SP_APP_ERR;
    }
    case SP_CLI_OK:
    case SP_CLI_CONTINUE: {
      break;
    }
  }

  if (args.ci) {
    sp->fps = 100000;
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

  spn_ctx_init(&spn);
  entry.booted = true;
  sp_os_register_signal_handler(SP_OS_SIGNAL_INTERRUPT, on_signal, SP_NULLPTR);

  if (spn_ctx_mount(&spn)) {
    return SP_APP_ERR;
  }
  spn_tui_open_log(&tui, spn.paths.log);

  if (spn_ctx_load_project(&spn, args.project_dir, args.refresh)) {
    return SP_APP_ERR;
  }
  spn_tui_flush(&tui);

  sp_thread_init(&entry.dispatch.thread, on_thread, SP_NULLPTR);

  return SP_APP_CONTINUE;
}

static sp_app_result_t on_poll(sp_app_t* sp) {
  if (spn_ctx_cancelled(&spn)) {
    sp_atomic_s32_set(&sp->shutdown, 1);
  }

  spn_tui_flush(&tui);
  spn_prompt_pump(&tui);

  return SP_APP_CONTINUE;
}

static sp_app_result_t on_update(sp_app_t* sp) {
  bool shutdown = sp_atomic_s32_get(&sp->shutdown) != 0;
  if (!shutdown && !sp_atomic_s32_get(&entry.dispatch.done)) {
    return SP_APP_CONTINUE;
  }

  sp_thread_join(&entry.dispatch.thread);

  spn_prompt_stop(&tui, entry.dispatch.status != SP_CLI_ERR);
  spn_tui_flush(&tui);

  switch (entry.dispatch.status) {
    case SP_CLI_HELP: {
      sp_cli_write_help(&tui.logger.out.base, &entry.cli);
      return SP_APP_QUIT;
    }
    case SP_CLI_ERR: {
      if (entry.cli.err.kind) {
        print_error(entry.cli.err);
      }
      return SP_APP_ERR;
    }
    case SP_CLI_OK:
    case SP_CLI_CONTINUE: {
      break;
    }
  }

  if (spn_ctx_cancelled(&spn)) {
    return SP_APP_QUIT;
  }

  if (entry.exec.finish) {
    if (entry.exec.finish()) {
      return SP_APP_ERR;
    }
  }

  return SP_APP_QUIT;
}

static void on_deinit(sp_app_t* sp) {
  if (tui.mode == SPN_OUTPUT_MODE_INTERACTIVE) {
    spn_prompt_stop(&tui, true);
    sp_io_flush(&tui.logger.err.base);
  }

  if (entry.booted) {
    spn_ctx_close(&spn, sp->result != SP_APP_ERR);
    spn_tui_flush(&tui);
  }
}

sp_app_config_t spn_main(s32 num_args, const c8** args) {
  entry.num_args = num_args;
  entry.args = args;

  return (sp_app_config_t) {
    .on_init = on_init,
    .on_poll = on_poll,
    .on_update = on_update,
    .on_deinit = on_deinit,
    .fps = 144,
  };
}
SP_APP_MAIN(spn_main)
