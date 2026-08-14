#include "host/host.h"

#include "tui/tui.h"

static struct {
  sp_str_t index;
  sp_str_t source_url;
  sp_str_t source_rev;
  bool dry;
  bool allow_dirty;
} args;

static spn_publish_request_t publish_request() {
  return (spn_publish_request_t) {
    .index = args.index,
    .url = args.source_url,
    .revision = args.source_rev,
    .allow_dirty = args.allow_dirty,
    .dry = args.dry,
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

static sp_cli_result_t publish(sp_cli_t* cli) {
  try(spn_cli_open(false));

  if (args.dry) {
    return publish_dry();
  }

  return spn_cli_op(spn_publish(host.ctx, publish_request()));
}

sp_cli_cmd_t spn_cmd_publish = {
  .name = "publish",
  .summary = "Publish a release to an index",
  .opts = {
    {
      .brief = 'i',
      .name = "index",
      .kind = SP_CLI_OPT_STR,
      .summary = "Index to publish to",
      .placeholder = "NAME",
      .ptr = &args.index,
    },
    {
      .name = "source-url",
      .kind = SP_CLI_OPT_STR,
      .summary = "Source repository URL (autodetected if omitted)",
      .placeholder = "URL",
      .ptr = &args.source_url,
    },
    {
      .name = "source-rev",
      .kind = SP_CLI_OPT_STR,
      .summary = "Source commit (autodetected if omitted)",
      .placeholder = "REV",
      .ptr = &args.source_rev,
    },
    {
      .name = "dry",
      .summary = "Print the release entry without publishing it",
      .kind = SP_CLI_OPT_BOOLEAN,
      .ptr = &args.dry,
    },
    {
      .name = "allow-dirty",
      .summary = "Publish even if the working tree has uncommitted changes",
      .ptr = &args.allow_dirty,
    },
  },
  .handler = publish,
};
