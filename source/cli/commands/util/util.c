#include "commands/util/util.h"

#include "tui/tui.h"

spn_cli_host_t host;
spn_tui_t tui;

static void on_wake(void* user_data) {
  u8 byte = 0;
  sp_sys_write(host.doorbell.write, &byte, 1, SP_NULLPTR);
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

void spn_cli_boot() {
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

  host.mem = sp_mem_arena_as_allocator(sp_mem_arena_new(sp_mem_os_new()));
  sp_sys_pipe(&host.doorbell.read, &host.doorbell.write);
  host.ctx = spn_ctx_new(on_wake, SP_NULLPTR);
  spn_tui_open(&tui, host.ctx, mode, verbosity, host.doorbell.read, host.doorbell.write);
  sp_os_register_signal_handler(SP_OS_SIGNAL_INTERRUPT, on_signal, SP_NULLPTR);
}

s32 spn_cli_shutdown(bool ok) {
  spn_prompt_stop(&tui, ok ? SP_PROMPT_STATE_SUBMIT : SP_PROMPT_STATE_ERROR);
  spn_ctx_close(host.ctx, ok);
  spn_tui_flush(&tui);
  sp_io_flush(&tui.logger.err.base);
  return ok ? 0 : 1;
}

spn_str_arr_t spn_cli_rest_names(sp_cli_t* cli) {
  u32 count = 0;
  for (const c8** it = cli->rest; *it; it++) {
    count++;
  }

  spn_str_arr_t names = { .items = sp_alloc_n(host.mem, sp_str_t, count), .count = count };
  sp_for(it, count) {
    names.items[it] = sp_cstr_as_str(cli->rest[it]);
  }
  return names;
}

sp_cli_result_t spn_cli_open(bool project_optional) {
  spn_err_t err = spn_ctx_open(host.ctx, (spn_open_request_t) {
    .dir = host.args.project_dir,
    .index_refresh_seconds = host.args.refresh,
    .project_optional = project_optional,
  });
  return err ? SP_CLI_ERR : SP_CLI_OK;
}

sp_cli_result_t spn_cli_session(sp_cli_t* cli, spn_session_config_t config) {
  try(spn_cli_parse_profile(cli, &config.profile));
  return spn_ctx_open_session(host.ctx, &config, &host.session) ? SP_CLI_ERR : SP_CLI_OK;
}

sp_cli_result_t spn_cli_refresh_indexes() {
  return spn_cli_op(spn_sync_indexes(host.ctx, (spn_sync_request_t) sp_zero));
}

spn_err_t spn_cli_wait(spn_op_t* op) {
  while (true) {
    u8 drain [16];
    u64 drained = 0;
    while (sp_sys_read(host.doorbell.read, drain, sizeof(drain), &drained) == SP_OK && drained) {}

    if (sp_atomic_s32_load(&host.interrupted, SP_ATOMIC_SEQ_CST)) {
      spn_op_cancel(op);
    }

    spn_tui_poll(&tui, op);
    if (spn_op_done(op)) {
      break;
    }

    if (!spn_tui_wants_input(&tui)) {
      sp_sys_fd_t fds [1] = { host.doorbell.read };
      u8 ready [1] = sp_zero;
      sp_sys_fds_wait(fds, ready, 1);
    }
  }

  spn_tui_op_done(&tui, op);
  return spn_op_result(op).err;
}

sp_cli_result_t spn_cli_op(spn_op_t* op) {
  spn_err_t err = spn_cli_wait(op);
  spn_op_free(op);
  return err ? SP_CLI_ERR : SP_CLI_OK;
}

sp_cli_result_t spn_cli_error(sp_cli_t* cli, const c8* fmt, ...) {
  va_list args;
  va_start(args, fmt);
  sp_str_t message = sp_fmt_mem_v(host.mem, sp_cstr_as_str(fmt), args).value;
  va_end(args);
  return sp_cli_set_error(cli, message);
}
