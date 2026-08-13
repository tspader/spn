#include "cli/cli.h"

#include "tui/tui.h"

static spn_publish_request_t publish_request() {
  spn_cli_publish_t* cmd = &args.publish;
  return (spn_publish_request_t) {
    .index = cmd->index,
    .url = cmd->source_url,
    .revision = cmd->source_rev,
    .allow_dirty = cmd->allow_dirty,
    .dry = cmd->dry,
  };
}

static sp_cli_result_t publish_dry() {
  spn_op_t* op = spn_publish(host.ctx, publish_request());
  if (spn_cli_wait(op)) {
    spn_op_free(op);
    return SP_CLI_ERR;
  }

  spn_tui_handoff(&tui);
  spn_print(&tui, "{}", sp_fmt_str(spn_op_result(op).publish.json));
  spn_op_free(op);
  spn_print_err(&tui, "{.cyan}: dry run, nothing published", sp_fmt_cstr("note"));
  return SP_CLI_OK;
}

sp_cli_result_t spn_cli_publish(sp_cli_t* cli) {
  try(spn_cli_open(false));

  if (args.publish.dry) {
    return publish_dry();
  }

  return spn_cli_op(spn_publish(host.ctx, publish_request()));
}
