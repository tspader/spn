#include "cli/cli.h"

#include "spn/host.h"

#include "tui/tui.h"

static spn_err_union_t publish_dry() {
  spn_cli_publish_t* cmd = &args.publish;

  spn_publish_opts_t opts = {
    .mem = spn.mem,
    .intern = spn.intern,
    .cwd = spn.project->paths.root,
    .url = cmd->source_url,
    .revision = cmd->source_rev,
    .allow_dirty = cmd->allow_dirty,
  };

  spn_index_release_t release = sp_zero;
  try_union(spn_publish_build(&opts, &release));

  spn_print("{}", sp_fmt_str(spn_index_release_to_json(spn.mem, &release)));
  spn_print_err("{.cyan}: dry run, nothing published", sp_fmt_cstr("note"));
  return spn_result(SPN_OK);
}

sp_cli_result_t spn_cli_publish(sp_cli_t* cli) {
  spn_cli_publish_t* cmd = &args.publish;
  try_cli(spn_cli_require_project());

  if (cmd->dry) {
    sp_str_t index_name = sp_str_empty(cmd->index) ? sp_str_lit("core") : cmd->index;
    if (!spn_find_index(index_name)) {
      return spn_cli_error(cli, "unknown index: {.cyan}", sp_fmt_str(index_name));
    }
    spn_cli_exec(cli)->finish = publish_dry;
    return SP_CLI_OK;
  }

  return spn_cli_result(cli, spn_op_publish(&spn, (spn_publish_request_t) {
    .index = cmd->index,
    .url = cmd->source_url,
    .revision = cmd->source_rev,
    .allow_dirty = cmd->allow_dirty,
  }));
}
