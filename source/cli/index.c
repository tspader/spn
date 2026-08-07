#include "cli/cli.h"

#include "spn/host.h"

#include "enum/enum.h"
#include "tui/tui.h"

sp_cli_result_t spn_cli_index(sp_cli_t* cli) {
  return SP_CLI_HELP;
}

static void index_list() {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();

  struct { u32 name; u32 kind; u32 protocol; } width = sp_zero;
  sp_da_for(spn.indexes, it) {
    width.name = sp_max(width.name, spn.indexes[it].name.len);
    width.kind = sp_max(width.kind, spn_index_kind_to_str(spn.indexes[it].kind).len);
    width.protocol = sp_max(width.protocol, spn_index_protocol_to_str(spn.indexes[it].protocol).len);
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

  sp_da_for(spn.indexes, it) {
    spn_index_info_t* index = &spn.indexes[it];
    spn_print("{:>$} {:>$} {:>$} {}",
      sp_fmt_uint(width.name),
      sp_fmt_str(index->name),
      sp_fmt_uint(width.kind),
      sp_fmt_str(spn_index_kind_to_str(index->kind)),
      sp_fmt_uint(width.protocol),
      sp_fmt_str(spn_index_protocol_to_str(index->protocol)),
      sp_fmt_str(spn_index_source(index)));
  }

  sp_mem_end_scratch(scratch);
}

sp_cli_result_t spn_cli_index_list(sp_cli_t* cli) {
  spn_tui_handoff(&tui);
  index_list();
  return SP_CLI_OK;
}

sp_cli_result_t spn_cli_index_path(sp_cli_t* cli) {
  if (!spn_find_index(&spn, args.index.name)) {
    return spn_cli_error(cli, "unknown index: {.cyan}", sp_fmt_str(args.index.name));
  }

  spn_tui_handoff(&tui);
  spn_print("{}", sp_fmt_str(spn_find_index(&spn, args.index.name)->location));
  return SP_CLI_OK;
}

sp_cli_result_t spn_cli_index_sync(sp_cli_t* cli) {
  if (!sp_str_empty(args.index.name) && !spn_find_index(&spn, args.index.name)) {
    return spn_cli_error(cli, "unknown index '{}'", sp_fmt_str(args.index.name));
  }

  spn_err_t err = spn_op_sync_indexes(&spn, (spn_index_refresh_t) {
    .force = true,
    .only = args.index.name,
  });
  return err ? SP_CLI_ERR : SP_CLI_OK;
}
