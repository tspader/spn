#include "cli/cli.h"

#include "ctx/ctx.h"
#include "ctx/types.h"
#include "index/json.h"
#include "index/publish.h"
#include "project/types.h"
#include "tui/tui.h"

static sp_cli_result_t publish_dry(sp_cli_t* cli, spn_cli_publish_t* cmd) {
  sp_str_t index_name = sp_str_empty(cmd->index) ? sp_str_lit("core") : cmd->index;
  if (!spn_find_index(index_name)) {
    return spn_cli_error(cli, "unknown index: {.cyan}", sp_fmt_str(index_name));
  }

  spn_publish_opts_t opts = {
    .mem = spn.mem,
    .intern = spn.intern,
    .cwd = spn.project->paths.root,
    .url = cmd->source_url,
    .revision = cmd->source_rev,
    .allow_dirty = cmd->allow_dirty,
  };

  spn_index_release_t release = sp_zero;
  spn_err_union_t err = spn_publish_build(&opts, &release);
  if (err.kind) {
    sp_mem_arena_marker_t s = sp_mem_begin_scratch();
    spn_build_event_t event = { .kind = SPN_EVENT_ERR, .err = err };
    sp_cli_result_t result = spn_cli_error(cli, "{}", sp_fmt_str(spn_tui_render_event_detail(s.mem, &event)));
    sp_mem_end_scratch(s);
    return result;
  }

  spn_print("{}", sp_fmt_str(spn_index_release_to_json(spn.mem, &release)));
  spn_print_err("{.cyan}: dry run, nothing published", sp_fmt_cstr("note"));
  return SP_CLI_OK;
}

sp_cli_result_t spn_cli_publish(sp_cli_t* cli) {
  spn_cli_publish_t* cmd = &args.publish;

  if (cmd->dry) {
    return publish_dry(cli, cmd);
  }

  spn_cli_command(cli)->op = (spn_op_desc_t) {
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
