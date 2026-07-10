#include "cli/cli.h"

#include "ctx/ctx.h"
#include "ctx/types.h"
#include "enum/enum.h"
#include "task/task.h"

sp_cli_result_t spn_cli_index(sp_cli_t* cli) {
  return SP_CLI_HELP;
}

sp_cli_result_t spn_cli_index_list(sp_cli_t* cli) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();

  struct { u32 name; u32 protocol; } width = sp_zero;
  sp_da_for(spn.indexes, it) {
    width.name = SP_MAX(width.name, spn.indexes[it].name.len);
    width.protocol = SP_MAX(width.protocol, spn_index_protocol_to_str(spn.indexes[it].protocol).len);
  }
  const c8* headers [] = { "name", "protocol", "url" };
  width.name = SP_MAX(width.name, sp_cstr_len(headers[0]));
  width.protocol = SP_MAX(width.name, sp_cstr_len(headers[1]));

  sp_fmt_io(&spn.logger.out.base, "{:<$ .gray} {:<$ .gray} {.gray}\n",
    sp_fmt_uint(width.name),
    sp_fmt_cstr(headers[0]),
    sp_fmt_uint(width.protocol),
    sp_fmt_cstr(headers[1]),
    sp_fmt_cstr(headers[2]));

  sp_da_for(spn.indexes, it) {
    spn_index_info_t* index = &spn.indexes[it];
    sp_fmt_io(&spn.logger.out.base, "{:>$} {:>$} {}\n",
      sp_fmt_uint(width.name),
      sp_fmt_str(index->name),
      sp_fmt_uint(width.protocol),
      sp_fmt_str(spn_index_protocol_to_str(index->protocol)),
      sp_fmt_str(index->url));
  }

  sp_mem_end_scratch(scratch);
  return SP_CLI_OK;
}

sp_cli_result_t spn_cli_index_path(sp_cli_t* cli) {
  sp_str_t name = sp_str_empty(spn.cli.index.name) ? sp_str_lit("core") : spn.cli.index.name;
  spn_index_info_t* index = spn_find_index(name);
  if (!index) {
    return spn_cli_errf(cli, "unknown index: {.cyan}", SP_FMT_STR(name));
  }

  sp_fmt_io(&spn.logger.out.base, "{}\n", sp_fmt_str(index->location));
  return SP_CLI_OK;
}

sp_cli_result_t spn_cli_index_sync(sp_cli_t* cli) {
  if (!sp_str_empty(spn.cli.index.name) && !spn_find_index(spn.cli.index.name)) {
    return spn_cli_errf(cli, "unknown index '{}'", SP_FMT_STR(spn.cli.index.name));
  }

  spn.cli.index.force = true;
  return spn_plan(SPN_TASK_SYNC_INDEXES);
}
