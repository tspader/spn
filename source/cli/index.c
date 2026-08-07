#include "cli/cli.h"

#include "tui/tui.h"

sp_cli_result_t spn_cli_index(sp_cli_t* cli) {
  return SP_CLI_HELP;
}

static void index_list() {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  spn_index_arr_t indexes = spn_ctx_indexes(scratch.mem, host.ctx);

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

  spn_print("{:<$ .gray} {:<$ .gray} {:<$ .gray} {.gray}",
    sp_fmt_uint(width.name),
    sp_fmt_cstr(headers[0]),
    sp_fmt_uint(width.kind),
    sp_fmt_cstr(headers[1]),
    sp_fmt_uint(width.protocol),
    sp_fmt_cstr(headers[2]),
    sp_fmt_cstr(headers[3]));

  sp_for(it, indexes.count) {
    spn_index_desc_t* index = &indexes.items[it];
    spn_print("{:>$} {:>$} {:>$} {}",
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

sp_cli_result_t spn_cli_index_list(sp_cli_t* cli) {
  sp_cli_result_t opened = spn_cli_open(true);
  if (opened != SP_CLI_OK) {
    return opened;
  }

  spn_tui_handoff(&tui);
  index_list();
  return SP_CLI_OK;
}

sp_cli_result_t spn_cli_index_path(sp_cli_t* cli) {
  sp_cli_result_t opened = spn_cli_open(true);
  if (opened != SP_CLI_OK) {
    return opened;
  }

  spn_index_desc_t index = sp_zero;
  if (!spn_ctx_find_index(host.ctx, args.index.name, &index)) {
    return spn_cli_error(cli, "unknown index: {.cyan}", sp_fmt_str(args.index.name));
  }

  spn_tui_handoff(&tui);
  spn_print("{}", sp_fmt_str(index.location));
  return SP_CLI_OK;
}

sp_cli_result_t spn_cli_index_sync(sp_cli_t* cli) {
  sp_cli_result_t opened = spn_cli_open(true);
  if (opened != SP_CLI_OK) {
    return opened;
  }

  spn_err_t err = spn_op_sync_indexes(host.ctx, (spn_sync_request_t) {
    .force = true,
    .only = args.index.name,
  });
  return err ? SP_CLI_ERR : SP_CLI_OK;
}
