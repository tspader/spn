#include "cli/cli.h"

#include "spn/host.h"

#include "tui/tui.h"

sp_cli_result_t spn_cli_index(sp_cli_t* cli) {
  return SP_CLI_HELP;
}

sp_cli_result_t spn_cli_index_list(sp_cli_t* cli) {
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
  return SP_CLI_OK;
}

sp_cli_result_t spn_cli_index_path(sp_cli_t* cli) {
  sp_str_t name = sp_str_empty(args.index.name) ? sp_str_lit("core") : args.index.name;
  spn_index_info_t* index = spn_find_index(name);
  if (!index) {
    return spn_cli_error(cli, "unknown index: {.cyan}", sp_fmt_str(name));
  }

  spn_print("{}", sp_fmt_str(index->location));
  return SP_CLI_OK;
}

sp_cli_result_t spn_cli_index_sync(sp_cli_t* cli) {
  if (!sp_str_empty(args.index.name) && !spn_find_index(args.index.name)) {
    return spn_cli_error(cli, "unknown index '{}'", sp_fmt_str(args.index.name));
  }

  spn_cli_command(cli)->op = (spn_op_desc_t) {
    .kind = SPN_OP_SYNC_INDEXES,
    .refresh = {
      .force = true,
      .only = args.index.name,
    },
  };
  return SP_CLI_CONTINUE;
}
