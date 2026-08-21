#include "sp.h"

#include "spn/host.h"

#include "commands/commands.h"
#include "host/host.h"
#include "tui/tui.h"

static void on_wake(void* user_data) {
  sp_sys_event_signal(host.doorbell);
}

static void on_signal(sp_os_signal_t signal, void* userdata) {
  switch (signal) {
    case SP_OS_SIGNAL_INTERRUPT: {
      sp_atomic_s32_store(&host.interrupted, 1, SP_ATOMIC_SEQ_CST);
      on_wake(SP_NULLPTR);
      break;
    }
    case SP_OS_SIGNAL_ABORT:
    case SP_OS_SIGNAL_TERMINATE: {
      break;
    }
  }
}

static s32 err(sp_io_writer_t* io, sp_cli_t* cli) {
  sp_cli_err_t err = cli->err;
  sp_fmt_io(io, "{.red}: ", sp_fmt_cstr("error"));
  sp_cli_err_print(io, err);
  sp_fmt_io(io, "\n");
  return 1;
}

static s32 help(sp_io_writer_t* io, sp_cli_t* cli) {
  sp_cli_write_help(io, cli);
  return 0;
}

s32 spn_main(s32 num_args, const c8** args) {
  sp_mem_heap_t* heap = sp_mem_heap_new();
  host.mem = sp_mem_heap_as_allocator(heap);
  sp_sys_event_open(&host.doorbell);
  host.ctx = spn_ctx_new(on_wake, SP_NULLPTR);
  sp_os_register_signal_handler(SP_OS_SIGNAL_INTERRUPT, on_signal, SP_NULLPTR);

  sp_cli_t cli = sp_zero;
  sp_cli_parse((sp_cli_desc_t) {
    .root = spn_cli(),
    .args = args,
    .num_args = num_args,
  }, &cli);

  spn_verbosity_t verbosity = SPN_VERBOSITY_NORMAL;
  if (host.args.quiet) {
    verbosity = SPN_VERBOSITY_QUIET;
  } else if (host.args.verbose) {
    verbosity = SPN_VERBOSITY_VERBOSE;
  }

  spn_tui_mode_t mode = SPN_OUTPUT_MODE_INTERACTIVE;
  if (!sp_str_empty(host.args.output)) {
    mode = spn_output_mode_from_str(host.args.output);
  }

  spn_tui_init(&tui, (spn_tui_desc_t) {
    .ctx = host.ctx,
    .mode = mode,
    .verbosity = verbosity,
    .wake = host.doorbell,
  });

  sp_io_writer_t* io = &tui.logger.out.base;

  switch (cli.status) {
    case SP_CLI_HELP: return help(io, &cli);
    case SP_CLI_ERR: return err(io, &cli);
    case SP_CLI_OK:
    case SP_CLI_CONTINUE: break;
  }

  sp_cli_result_t status = sp_cli_dispatch(&cli);

  switch (status) {
    case SP_CLI_HELP: help(io, &cli);
    case SP_CLI_ERR: err(io, &cli);
    case SP_CLI_OK:
    case SP_CLI_CONTINUE: break;
  }

  return spn_cli_shutdown(status != SP_CLI_ERR);
}
SP_MAIN(spn_main)
