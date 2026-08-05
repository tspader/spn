#include "error/error.h"
#include "error/types.h"
#include "sp.h"

// STANDARD
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

// SPN
#include "spn.h"

#include "ctx/ctx.h"
#include "ctx/init.h"
#include "ctx/types.h"
#include "forward/types.h"

#include "cli/cli.h"
#include "event/event.h"
#include "session/session.h"
#include "op/op.h"
#include "tui/tui.h"

// SINGLE HEADER
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

spn_ctx_t spn;

static struct {
  s32 num_args;
  const c8** args;
  spn_err_union_t result;
  struct {
    spn_err_union_t (*finish)();
    spn_op_t* op;
  } exec;
} entry;

static void on_signal(sp_os_signal_t signal, void* userdata) {
  switch (signal) {
    case SP_OS_SIGNAL_INTERRUPT: {
      sp_app_t* sp = (sp_app_t*)userdata;
      spn_ctx_cancel(&spn);
      sp_atomic_s32_set(&sp->shutdown, 1);
      break;
    }
    case SP_OS_SIGNAL_ABORT:
    case SP_OS_SIGNAL_TERMINATE: {
      break;
    }
  }
}

#define try(expr) \
  do { \
    spn_err_union_t __err = (expr); \
    if (__err.kind) { \
      entry.result = spn_err_emit(__err); \
      return SP_APP_ERR; \
    } \
  } while (0)

static sp_app_result_t on_init(sp_app_t* sp) {
  spn_tui_init(&tui);

  spn_command_t command = sp_zero;
  sp_cli_t parsed = sp_cli_parse((sp_cli_desc_t) {
    .root = spn_cli(),
    .args = entry.args,
    .num_args = entry.num_args,
    .user_data = &command,
  });

  switch (parsed.status) {
    case SP_CLI_HELP: {
      sp_cli_write_help(&tui.logger.out.base, &parsed);
      return SP_APP_QUIT;
    }
    case SP_CLI_ERR: {
      sp_fmt_io(&tui.logger.err.base, "{.red}: ", sp_fmt_cstr("error"));
      sp_cli_err_print(&tui.logger.err.base, parsed.err);
      sp_fmt_io(&tui.logger.err.base, "\n");
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
  sp_os_register_signal_handler(SP_OS_SIGNAL_INTERRUPT, on_signal, sp);

  try(spn_ctx_mount(&spn));
  spn_tui_open_log(&tui, spn.paths.log);

  try(spn_ctx_load_project(&spn, args.project_dir, args.refresh));
  if (spn_cli_requires_manifest(parsed.cmd)) {
    if (!spn.project) {
      entry.result = spn_err_emit((spn_err_union_t) {
        .kind = SPN_ERR_NO_MANIFEST,
        .no_manifest = { .path = spn.paths.project },
      });
      return SP_APP_ERR;
    }
  }

  try(spn_cli_parse_profile(&args.profile, &command.config.overrides));

  switch (sp_cli_dispatch(&parsed)) {
    case SP_CLI_CONTINUE: {
      break;
    }
    case SP_CLI_OK: {
      return SP_APP_QUIT;
    }
    case SP_CLI_HELP: {
      sp_cli_write_help(&tui.logger.out.base, &parsed);
      return SP_APP_QUIT;
    }
    case SP_CLI_ERR: {
      sp_fmt_io(&tui.logger.err.base, "{.red}: ", sp_fmt_cstr("error"));
      sp_cli_err_print(&tui.logger.err.base, parsed.err);
      sp_fmt_io(&tui.logger.err.base, "\n");
      return SP_APP_ERR;
    }
  }

  if (spn.project) {
    try(spn_ctx_open_session(&spn, command.config));
  }

  entry.exec.finish = command.finish;
  if (command.op.kind) {
    entry.exec.op = spn_op_start(spn.heap, &spn, command.op);
  }

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
  if (sp_atomic_s32_get(&sp->shutdown)) {
    if (entry.exec.op) {
      spn_err_union_t result = spn_op_result(entry.exec.op);
      entry.exec.op = SP_NULLPTR;
      if (result.kind) {
        entry.result = spn_err_emit(result);
        return SP_APP_ERR;
      }
    }
    return SP_APP_QUIT;
  }

  if (entry.exec.op) {
    if (!spn_op_poll(entry.exec.op)) {
      return SP_APP_CONTINUE;
    }

    spn_err_union_t result = spn_op_result(entry.exec.op);
    entry.exec.op = SP_NULLPTR;

    if (result.kind) {
      entry.result = spn_err_emit(result);
      spn_prompt_stop(&tui, false);
      spn_tui_flush(&tui);
      return SP_APP_ERR;
    }

    spn_prompt_stop(&tui, true);
    spn_tui_flush(&tui);
  }

  if (entry.exec.finish) {
    spn_err_union_t err = entry.exec.finish();
    entry.exec.finish = SP_NULLPTR;
    if (err.kind) {
      entry.result = spn_err_emit(err);
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

  if (spn.events) {
    bool ok = sp->result != SP_APP_ERR;
    spn_err_t kind = entry.result.kind;
    if (!ok && !kind) {
      kind = SPN_ERROR;
    }
    spn_event_buffer_push(spn.events, (spn_build_event_t) {
      .kind = SPN_EVENT_RESULT,
      .result = {
        .ok = ok,
        .err = sp_cstr_as_str(spn_err_to_str(kind)),
      },
    });
  }

  spn_tui_flush(&tui);

  if (spn.session) {
    spn_session_finalize(spn.session);
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
