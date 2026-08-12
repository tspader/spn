#include "ctx/types.h"
#include "error/types.h"
#include "spn/core.h"
#include "unit/types.h"
#include "session/types.h"

#include "external/cc.h"
#include "compiler/driver.h"
#include "compiler/exports.h"
#include "compiler/toc.h"
#include "error/error.h"
#include "event/event.h"
#include "intern/intern.h"
#include "session/invocation.h"
#include "sp/io.h"
#include "sp/os.h"
#include "graph/build.h"
#include "graph/nodes/nodes.h"
#include "unit/package.h"

spn_err_union_t spn_build_link_invocation(sp_mem_t mem, spn_target_unit_t* target, const spn_cc_link_files_t* files, spn_invocation_t* invocation) {
  spn_profile_info_t* profile = &target->pkg->build->profile;
  spn_cc_toolchain_t* toolchain = &target->pkg->build->toolchain->cc;

  switch (target->kind) {
    case SPN_CC_OUTPUT_STATIC_LIB: {
      spn_cc_archive_files_t archive_files = {
        .output = files->output,
        .objects = files->objects,
      };
      spn_try_union(spn_cc_render_archive(mem, toolchain, profile, &archive_files, invocation));
      break;
    }
    case SPN_CC_OUTPUT_EXE:
    case SPN_CC_OUTPUT_SHARED_LIB:
    case SPN_CC_OUTPUT_REACTOR: {
      spn_try_union(spn_cc_render_link(mem, toolchain, profile, &target->link.cc, files, invocation));
      break;
    }
    case SPN_CC_OUTPUT_OBJECT: {
      sp_unreachable_case();
    }
  }

  invocation->cwd = target->pkg->paths.work;
  return spn_result(SPN_OK);
}


static spn_err_t emit_link_passed(spn_target_unit_t* unit, spn_invocation_t* invocation, sp_str_t output, sp_str_t out, u64 elapsed) {
  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_LINK_PASSED,
    .pkg = unit->pkg->info,
    .io = &unit->logs,
    .link_passed = {
      .target = unit->info->name,
      .output_path = output,
      .command = spn_invocation_to_str(spn.mem, invocation),
      .out = out,
      .time = elapsed,
    }
  });
  return SPN_OK;
}

static spn_err_t emit_link_failed(spn_target_unit_t* unit, spn_invocation_t* invocation, s32 rc, sp_str_t out, sp_str_t err) {
  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_LINK_FAILED,
    .pkg = unit->pkg->info,
    .io = &unit->logs,
    .link_failed = {
      .target = unit->info->name,
      .exit_code = rc,
      .command = spn_invocation_to_str(spn.mem, invocation),
      .out = out,
      .err = err,
    }
  });
  return SPN_ERROR;
}


typedef sp_str_ht(u8) spn_symbol_set_t;

static spn_err_union_t read_archive_symbols(sp_str_t path, sp_da(sp_str_t)* symbols, spn_symbol_set_t* seen) {
  sp_io_file_reader_t reader = sp_zero;
  if (sp_io_file_reader_from_path(&reader, path)) {
    return (spn_err_union_t) { .kind = SPN_ERR_FS_READ, .fs.path = path };
  }

  spn_toc_parser_t toc;
  spn_err_t err = spn_toc_init(&toc, &reader.base);

  sp_str_t symbol = sp_zero;
  while (spn_toc_next(&toc, &symbol)) {
    sp_str_t interned = spn_intern(symbol);
    if (sp_str_ht_exists(*seen, interned)) {
      continue;
    }
    sp_str_ht_insert(*seen, interned, (u8)true);
    sp_da_push(*symbols, interned);
  }
  if (!err) {
    err = toc.err;
  }

  sp_io_file_reader_close(&reader);
  return spn_result(err);
}

static s32 spn_link_exports_exec(sp_mem_t scratch, spn_target_unit_t* target, sp_da(sp_str_t) objects, sp_str_t output) {
  spn_pkg_unit_t* pkg = target->pkg;
  spn_profile_info_t* profile = &pkg->build->profile;
  spn_cc_toolchain_t* toolchain = &pkg->build->toolchain->cc;

  spn_cc_archive_files_t files = {
    .output = sp_fmt(spn.mem, "{}.a", SP_FMT_STR(output)).value,
    .objects = objects,
  };
  spn_invocation_t* invocation = sp_alloc_type(spn.mem, spn_invocation_t);
  spn_try(spn_err_emit(&spn, spn_cc_render_archive(spn.mem, toolchain, profile, &files, invocation)));
  invocation->cwd = pkg->paths.work;

  spn_invocation_result_t run = spn_invocation_run(invocation);
  if (run.result.status.exit_code) {
    return emit_link_failed(target, invocation, run.result.status.exit_code, run.result.out, run.result.err);
  }

  spn_symbol_set_t seen;
  sp_str_ht_init(scratch, seen);
  sp_da(sp_str_t) symbols = sp_da_new(scratch, sp_str_t);
  spn_try(spn_err_emit(&spn, read_archive_symbols(files.output, &symbols, &seen)));
  sp_da_for(target->link.cc.whole_archives, it) {
    spn_try(spn_err_emit(&spn, read_archive_symbols(target->link.cc.whole_archives[it], &symbols, &seen)));
  }

  sp_io_file_writer_t writer = sp_zero;
  if (sp_io_file_writer_from_path(&writer, output) != SP_OK) {
    return spn_err_emit(&spn, (spn_err_union_t) { .kind = SPN_ERR_FS_WRITE, .fs.path = output });
  }

  switch (spn_cc_exports_format(target->kind, profile->os)) {
    case SPN_CC_EXPORTS_VERSION_SCRIPT: {
      spn_exports_render_version_script(&writer.base, symbols);
      break;
    }
    case SPN_CC_EXPORTS_SYMBOL_LIST:
    case SPN_CC_EXPORTS_WASM: {
      spn_exports_render_symbol_list(&writer.base, symbols);
      break;
    }
    case SPN_CC_EXPORTS_DEF: {
      spn_exports_render_def(&writer.base, target->info->name, symbols);
      break;
    }
  }
  sp_io_file_writer_close(&writer);

  return 0;
}

s32 spn_link_exports_run(spn_target_unit_t* target, sp_da(sp_str_t) objects, sp_str_t output) {
  spn_pkg_unit_announce_compile(target->pkg);

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  s32 result = spn_link_exports_exec(scratch.mem, target, objects, output);
  sp_mem_end_scratch(scratch);
  return result;
}

static spn_err_union_t read_export_symbols(sp_mem_t mem, sp_str_t path, sp_da(sp_str_t)* symbols) {
  sp_str_t content = sp_zero;
  if (sp_io_read_file(mem, path, &content)) {
    return (spn_err_union_t) { .kind = SPN_ERR_FS_READ, .fs.path = path };
  }

  sp_da(sp_str_t) lines = sp_str_split_c8(mem, content, '\n');
  sp_da_for(lines, it) {
    if (!sp_str_empty(lines[it])) {
      sp_da_push(*symbols, lines[it]);
    }
  }
  return spn_result(SPN_OK);
}

static s32 spn_link_target_exec(sp_mem_t scratch, spn_target_unit_t* target, sp_str_t output, sp_da(sp_str_t) objects, sp_str_t exports) {
  spn_cc_link_files_t files = {
    .output = output,
    .objects = objects,
  };
  switch (target->kind) {
    case SPN_CC_OUTPUT_REACTOR: {
      sp_da_init(scratch, files.exports.symbols);
      spn_try(spn_err_emit(&spn, read_export_symbols(scratch, exports, &files.exports.symbols)));
      break;
    }
    case SPN_CC_OUTPUT_SHARED_LIB: {
      files.exports.path = exports;
      break;
    }
    case SPN_CC_OUTPUT_EXE:
    case SPN_CC_OUTPUT_STATIC_LIB: {
      sp_assert(sp_str_empty(exports));
      break;
    }
    case SPN_CC_OUTPUT_OBJECT: {
      sp_unreachable_case();
    }
  }

  spn_invocation_t* invocation = sp_alloc_type(spn.mem, spn_invocation_t);
  spn_try(spn_err_emit(&spn, spn_build_link_invocation(spn.mem, target, &files, invocation)));

  spn_invocation_result_t run = spn_invocation_run(invocation);

  if (run.result.status.exit_code) {
    return emit_link_failed(target, invocation, run.result.status.exit_code, run.result.out, run.result.err);
  }

  return emit_link_passed(target, invocation, spn_target_output_path(spn.mem, target), run.result.out, run.elapsed);
}

s32 spn_link_target_run(spn_target_unit_t* target, sp_str_t output, sp_da(sp_str_t) objects, sp_str_t exports) {
  spn_pkg_unit_announce_compile(target->pkg);

  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_LINK_START,
    .pkg = target->pkg->info,
    .io = &target->logs,
    .link_start = {
      .target = target->info->name,
    }
  });

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  s32 result = spn_link_target_exec(scratch.mem, target, output, objects, exports);
  sp_mem_end_scratch(scratch);
  return result;
}
