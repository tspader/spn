#include "host/host.h"

#include "tui/tui.h"

static struct {
  sp_str_t name;
} args;

static void render_list() {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  spn_index_arr_t indexes = spn_get_indexes(scratch.mem, host.ctx);

  struct { u32 name; u32 kind; u32 protocol; } width = sp_zero;
  sp_for(it, indexes.count) {
    width.name = sp_max(width.name, indexes.items[it].name.len);
    width.kind = sp_max(width.kind, spn_index_kind_to_str(indexes.items[it].kind).len);
    width.protocol = sp_max(width.protocol, spn_index_protocol_to_str(indexes.items[it].protocol).len);
  }
  const c8* headers [] = { "name", "kind", "protocol", "source" };
  width.name = sp_max(width.name, sp_cstr_len(headers[0]));
  width.kind = sp_max(width.kind, sp_cstr_len(headers[1]));
  width.protocol = sp_max(width.protocol, sp_cstr_len(headers[2]));

  spn_print(&tui, "{:<$ .gray} {:<$ .gray} {:<$ .gray} {.gray}",
    sp_fmt_uint(width.name),
    sp_fmt_cstr(headers[0]),
    sp_fmt_uint(width.kind),
    sp_fmt_cstr(headers[1]),
    sp_fmt_uint(width.protocol),
    sp_fmt_cstr(headers[2]),
    sp_fmt_cstr(headers[3]));

  sp_for(it, indexes.count) {
    spn_index_desc_t* index = &indexes.items[it];
    spn_print(&tui, "{:<$} {:<$} {:<$} {}",
      sp_fmt_uint(width.name),
      sp_fmt_str(index->name),
      sp_fmt_uint(width.kind),
      sp_fmt_str(spn_index_kind_to_str(index->kind)),
      sp_fmt_uint(width.protocol),
      sp_fmt_str(spn_index_protocol_to_str(index->protocol)),
      sp_fmt_str(index->source));
  }

  sp_mem_end_scratch(scratch);
}

static sp_cli_result_t help(sp_cli_t* cli) {
  return SP_CLI_HELP;
}

static sp_cli_result_t list(sp_cli_t* cli) {
  try(spn_cli_open(true));

  spn_tui_handoff(&tui);
  render_list();
  return SP_CLI_OK;
}

static sp_cli_result_t path(sp_cli_t* cli) {
  try(spn_cli_open(true));

  spn_index_desc_t index = sp_zero;
  if (!spn_get_index(host.ctx, args.name, &index)) {
    return spn_cli_usage("unknown index: {.cyan}", sp_fmt_str(args.name));
  }

  spn_tui_handoff(&tui);
  spn_print(&tui, "{}", sp_fmt_str(index.location));
  return SP_CLI_OK;
}

static sp_cli_result_t refresh(sp_cli_t* cli) {
  try(spn_cli_open(true));

  return spn_cli_op(spn_sync_indexes(host.ctx, (spn_sync_request_t) {
    .force = true,
    .only = args.name,
  }));
}

static sp_cli_cmd_t cmd_list = {
  .name = "list",
  .summary = "List configured indexes",
  .handler = list,
};

static sp_cli_cmd_t cmd_path = {
  .name = "path",
  .summary = "Print the local checkout path of an index",
  .args = {
    {
      .name = "name",
      .arity = SP_CLI_ARG_OPTIONAL,
      .kind = SP_CLI_OPT_STR,
      .summary = "Index name (default: core)",
      .ptr = &args.name,
    },
  },
  .handler = path,
};

static sp_cli_cmd_t cmd_sync = {
  .name = "sync",
  .summary = "Refresh indexes, ignoring the staleness window",
  .args = {
    {
      .name = "name",
      .arity = SP_CLI_ARG_OPTIONAL,
      .kind = SP_CLI_OPT_STR,
      .summary = "Only sync this index",
      .ptr = &args.name,
    },
  },
  .handler = refresh,
};

sp_cli_cmd_t spn_cmd_index = {
  .name = "index",
  .summary = "Inspect and manage package indexes",
  .commands = {
    &cmd_list,
    &cmd_path,
    &cmd_sync,
  },
  .handler = help,
};
