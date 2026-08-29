#include "sp.h"

#include "spn/host.h"

#include "commands/commands.h"
#include "host/host.h"
#include "tui/tui.h"
#include "version.h"

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

static s32 err(sp_tty_t* tty, sp_cli_t* cli) {
  sp_tty_fmt(tty, "{.red}: ", sp_fmt_cstr("error"));
  sp_cli_err_print(tty, cli->err);
  sp_tty_fmt(tty, "\n");
  return 1;
}

static s32 help(sp_tty_t* tty, sp_cli_t* cli) {
  sp_cli_write_help(tty, cli);
  return 0;
}

static s32 version(sp_tty_t* tty) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  sp_da(sp_str_t) parts = sp_da_new(scratch.mem, sp_str_t);

  sp_str_t channel = sp_cstr_as_str(spn_build_channel);
  if (!sp_str_equal_cstr(channel, "stable")) {
    sp_da_push(parts, channel);
  }

  sp_str_t commit = sp_cstr_as_str(spn_build_commit);
  if (commit.len) {
    sp_da_push(parts, sp_str_prefix(commit, sp_min((s32)commit.len, 9)));
  }

  if (sp_da_size(parts)) {
    sp_tty_fmt(tty, "spn {} ({})\n", sp_fmt_cstr(SPN_VERSION), sp_fmt_str(sp_str_join_n(scratch.mem, parts, sp_da_size(parts), sp_str_lit(" "))));
  } else {
    sp_tty_fmt(tty, "spn {}\n", sp_fmt_cstr(SPN_VERSION));
  }

  sp_mem_end_scratch(scratch);
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

  spn_tui_init(&tui, (spn_tui_desc_t) {
    .ctx = host.ctx,
    .mode = (spn_tui_mode_t)host.args.output.value,
    .verbosity = verbosity,
    .wake = host.doorbell,
  });

  sp_tty_t* io = tui.out;

  if (cli.status == SP_CLI_ERR) {
    return err(io, &cli);
  }
  if (host.args.version) {
    return version(io);
  }
  if (cli.status == SP_CLI_HELP) {
    return help(io, &cli);
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
