#include "host.h"
#include "tui/tui.h"

spn_cli_host_t host;
spn_tui_t tui;

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
    sp_sys_event_clear(host.doorbell);

    if (sp_atomic_s32_load(&host.interrupted, SP_ATOMIC_SEQ_CST)) {
      spn_op_cancel(op);
    }

    spn_tui_poll(&tui, op);
    if (spn_op_done(op)) {
      break;
    }

    if (!spn_tui_wants_input(&tui)) {
      sp_sys_fd_t fds [1] = { host.doorbell.fd };
      u64 signaled = 0;
      sp_sys_wait(fds, 1, 0, &signaled);
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
