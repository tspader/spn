#include "cli/cli.h"

#include "spn/host.h"

#include "tui/tui.h"

static spn_err_union_t finish_publish_dry() {
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
  spn_command_t* command = spn_cli_command(cli);
  command->project = true;

  if (cmd->dry) {
    sp_str_t index_name = sp_str_empty(cmd->index) ? sp_str_lit("core") : cmd->index;
    if (!spn_find_index(index_name)) {
      return spn_cli_error(cli, "unknown index: {.cyan}", sp_fmt_str(index_name));
    }
    command->finish = finish_publish_dry;
    return SP_CLI_CONTINUE;
  }

  command->op = (spn_op_desc_t) {
    .kind = SPN_OP_PUBLISH,
    .publish = {
      .index = cmd->index,
      .url = cmd->source_url,
      .revision = cmd->source_rev,
      .allow_dirty = cmd->allow_dirty,
    },
  };
  return SP_CLI_CONTINUE;
}
