#include "cli/cli.h"

#include "spn/host.h"

#include "tui/tui.h"

static spn_publish_request_t publish_request() {
  spn_cli_publish_t* cmd = &args.publish;
  return (spn_publish_request_t) {
    .index = cmd->index,
    .url = cmd->source_url,
    .revision = cmd->source_rev,
    .allow_dirty = cmd->allow_dirty,
  };
}

static sp_cli_result_t publish_dry() {
  spn_index_release_t release = sp_zero;
  if (spn_op_publish_build(&spn, publish_request(), &release)) {
    return SP_CLI_ERR;
  }

  spn_tui_handoff(&tui);
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  spn_print("{}", sp_fmt_str(spn_index_release_to_json(scratch.mem, &release)));
  sp_mem_end_scratch(scratch);
  spn_print_err("{.cyan}: dry run, nothing published", sp_fmt_cstr("note"));
  return SP_CLI_OK;
}

sp_cli_result_t spn_cli_publish(sp_cli_t* cli) {
  if (args.publish.dry) {
    return publish_dry();
  }

  return spn_op_publish(&spn, publish_request()) ? SP_CLI_ERR : SP_CLI_OK;
}
