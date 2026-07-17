#include "ctx/types.h"
#include "error/types.h"
#include "sp/sp_graph.h"
#include "spn.h"
#include "unit/types.h"
#include "session/types.h"

#include "external/cc.h"
#include "compiler/driver.h"
#include "compiler/exports.h"
#include "compiler/toc.h"
#include "event/event.h"
#include "intern/intern.h"
#include "session/invocation.h"
#include "sp/io.h"
#include "sp/os.h"
#include "task/build/build.h"
#include "unit/package.h"

static spn_cc_link_t spn_build_link_desc(sp_mem_t mem, spn_target_unit_t* target) {
  spn_cc_link_t link = {
    .lang = target->link.lang,
    .kind = target->kind,
    .exports = target->link.exports,
    .system_libs = target->link.system_libs,
    .whole_archives = target->link.whole_archives,
    .private_libs = target->link.private_libs,
    .lib_dirs = target->link.lib_dirs,
    .frameworks = target->link.frameworks,
  };
  sp_da_init(mem, link.objects);
  sp_da_init(mem, link.args);
  sp_da_init(mem, link.libs);
  sp_da_init(mem, link.rpath);

  switch (target->pkg->build->profile.os) {
    case SPN_OS_LINUX: {
      sp_da_push(link.rpath, sp_str_lit("$ORIGIN"));
      break;
    }
    case SPN_OS_MACOS: {
      sp_da_push(link.rpath, sp_str_lit("@loader_path"));
      link.min_os = target->link.min_os;
      break;
    }
    case SPN_OS_WINDOWS: {
      link.subsystem = target->info->windows.subsystem;
      break;
    }
    case SPN_OS_WASI:
    case SPN_OS_NONE: {
      break;
    }
  }

  return link;
}

spn_err_union_t spn_build_render_target(sp_mem_t mem, spn_target_unit_t* target, sp_str_t output, sp_da(sp_str_t) objects, spn_invocation_t* invocation) {
  spn_profile_info_t* profile = &target->pkg->build->profile;
  spn_cc_toolchain_t* toolchain = &target->pkg->build->toolchain->cc;

  switch (target->kind) {
    case SPN_CC_OUTPUT_STATIC_LIB: {
      spn_cc_archive_t archive = {
        .output = output,
        .objects = objects,
      };
      sp_da_init(mem, archive.args);
      try_union(spn_cc_render_archive(mem, toolchain, profile, &archive, invocation));
      break;
    }
    case SPN_CC_OUTPUT_EXE:
    case SPN_CC_OUTPUT_SHARED_LIB:
    case SPN_CC_OUTPUT_REACTOR: {
      spn_cc_link_t link = spn_build_link_desc(mem, target);
      link.output = output;
      link.objects = objects;
      try_union(spn_cc_render_link(mem, toolchain, profile, &link, invocation));
      break;
    }
    case SPN_CC_OUTPUT_OBJECT: {
      sp_unreachable_case();
    }
  }

  invocation->cwd = target->pkg->paths.work;
  return spn_result(SPN_OK);
}

spn_err_union_t spn_build_validate_target(spn_target_unit_t* target) {
  spn_profile_info_t* profile = &target->pkg->build->profile;
  spn_cc_toolchain_t* toolchain = &target->pkg->build->toolchain->cc;

  if (!sp_da_empty(target->objects)) {
    try_union(spn_cc_validate_compile(toolchain, profile));
  }

  switch (target->kind) {
    case SPN_CC_OUTPUT_STATIC_LIB: {
      return spn_cc_validate_archive(toolchain, profile);
    }
    case SPN_CC_OUTPUT_SHARED_LIB:
    case SPN_CC_OUTPUT_REACTOR: {
      try_union(spn_cc_validate_archive(toolchain, profile));
      return spn_cc_validate_link(toolchain, profile, target->kind, !sp_da_empty(target->link.frameworks));
    }
    case SPN_CC_OUTPUT_EXE: {
      return spn_cc_validate_link(toolchain, profile, target->kind, !sp_da_empty(target->link.frameworks));
    }
    case SPN_CC_OUTPUT_OBJECT: {
      return spn_result(SPN_OK);
    }
  }
  sp_unreachable_return(spn_result(SPN_ERROR));
}

sp_da(sp_str_t) spn_build_target_objects(sp_mem_t mem, spn_target_unit_t* target) {
  sp_da(sp_str_t) objects = sp_da_new(mem, sp_str_t);
  sp_da_for(target->objects, it) {
    sp_da_push(objects, target->objects[it]->paths.object);
  }
  if (!sp_da_empty(target->info->embed)) {
    sp_da_push(objects, get_embed_object_path(mem, target));
  }
  return objects;
}

spn_err_t emit_link_passed(spn_target_unit_t* unit, sp_str_t output, sp_str_t out, u64 elapsed) {
  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_LINK_PASSED,
    .pkg = unit->pkg->info,
    .io = &unit->logs,
    .target.name = unit->info->name,
    .target.link_passed = {
      .output_path = output,
      .invocation = &unit->invocation,
      .out = out,
      .time = elapsed,
    }
  });
  return SPN_OK;
}

spn_err_t emit_link_failed(spn_target_unit_t* unit, s32 rc, sp_str_t out, sp_str_t err) {
  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_LINK_FAILED,
    .pkg = unit->pkg->info,
    .io = &unit->logs,
    .target.name = unit->info->name,
    .target.link_failed = {
      .exit_code = rc,
      .out = out,
      .err = err,
      .invocation = &unit->invocation,
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

static sp_str_t exports_file_name(sp_mem_t mem, spn_target_unit_t* target) {
  const c8* extension = "map";
  switch (target->pkg->build->profile.os) {
    case SPN_OS_MACOS: {
      extension = "exp";
      break;
    }
    case SPN_OS_WINDOWS: {
      extension = "def";
      break;
    }
    case SPN_OS_LINUX:
    case SPN_OS_WASI:
    case SPN_OS_NONE: {
      break;
    }
  }
  return sp_fmt(mem, "{}.{}", SP_FMT_STR(target->info->name), sp_fmt_cstr(extension)).value;
}

static s32 exports_fail(spn_err_union_t err) {
  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_ERR,
    .err = err,
  });
  return 1;
}

static s32 spn_build_target_exports(sp_mem_t scratch, spn_target_unit_t* target, sp_da(sp_str_t) objects) {
  sp_mem_t mem = spn.mem;
  spn_pkg_unit_t* pkg = target->pkg;
  spn_profile_info_t* profile = &pkg->build->profile;
  spn_cc_toolchain_t* toolchain = &pkg->build->toolchain->cc;

  // @linker Can we use the DAG for this?
  sp_str_t index_name = sp_fmt(mem, "{}.exports.a", SP_FMT_STR(target->info->name)).value;
  sp_str_t index_path = sp_fs_join_path(mem, pkg->paths.work, index_name);
  sp_fs_remove(index_path);

  spn_cc_archive_t archive = {
    .output = index_path,
    .objects = objects,
  };
  sp_da_init(mem, archive.args);
  try_emit(spn_cc_render_archive(mem, toolchain, profile, &archive, &target->invocation), spn.events);

  target->invocation.cwd = pkg->paths.work;
  spn_invocation_result_t run = spn_invocation_run(&target->invocation);
  if (run.result.status.exit_code) {
    return emit_link_failed(target, run.result.status.exit_code, run.result.out, run.result.err);
  }

  // @linker Why don't other platforms ever carry explicit symbols? Not sure if this is just what I punted on and we just couldn't punt for WASM without completely breaking it or if it's bad design
  spn_symbol_set_t seen;
  sp_str_ht_init(scratch, seen);
  sp_da(sp_str_t) symbols = sp_da_new(scratch, sp_str_t);
  try_emit(read_archive_symbols(index_path, &symbols, &seen), spn.events);

  sp_da_for(target->link.whole_archives, it) {
    try_emit(read_archive_symbols(target->link.whole_archives[it], &symbols, &seen), spn.events);
  }

  if (target->kind == SPN_CC_OUTPUT_REACTOR) {
    target->link.exports.symbols = symbols;
    return 0;
  }

  sp_str_t path = sp_fs_join_path(mem, pkg->paths.work, exports_file_name(mem, target));
  sp_io_file_writer_t writer = sp_zero;
  if (sp_io_file_writer_from_path(&writer, path) != SP_OK) {
    return exports_fail((spn_err_union_t) { .kind = SPN_ERR_FS_WRITE, .fs.path = path });
  }

  switch (profile->os) {
    case SPN_OS_MACOS: {
      spn_exports_render_symbol_list(&writer.base, symbols);
      break;
    }
    case SPN_OS_WINDOWS: {
      spn_exports_render_def(&writer.base, target->info->name, symbols);
      break;
    }
    case SPN_OS_LINUX:
    case SPN_OS_WASI:
    case SPN_OS_NONE: {
      spn_exports_render_version_script(&writer.base, symbols);
      break;
    }
  }
  sp_io_file_writer_close(&writer);

  target->link.exports.path = path;
  return 0;
}

static s32 spn_link_target_exec(sp_mem_t scratch, spn_target_unit_t* target, sp_str_t output, sp_da(sp_str_t) objects) {
  if (target->kind == SPN_CC_OUTPUT_SHARED_LIB || target->kind == SPN_CC_OUTPUT_REACTOR) {
    if (spn_build_target_exports(scratch, target, objects)) {
      return 1;
    }
  }

  spn_err_union_t render = spn_build_render_target(spn.mem, target, output, objects, &target->invocation);
  if (render.kind) {
    spn_event_buffer_push(spn.events, (spn_build_event_t) {
      .kind = SPN_EVENT_ERR,
      .err = render,
    });
    return 1;
  }

  spn_invocation_result_t run = spn_invocation_run(&target->invocation);

  if (run.result.status.exit_code) {
    return emit_link_failed(target, run.result.status.exit_code, run.result.out, run.result.err);
  }

  return emit_link_passed(target, get_target_output_path(spn.mem, target), run.result.out, run.elapsed);
}

s32 spn_link_target_run(spn_target_unit_t* target, sp_str_t output, sp_da(sp_str_t) objects) {
  spn_pkg_unit_announce_compile(target->pkg);

  spn_event_buffer_push(spn.events, (spn_build_event_t) {
    .kind = SPN_EVENT_LINK_START,
    .pkg = target->pkg->info,
    .io = &target->logs,
    .target.name = target->info->name,
    .target.link_start = {
      .target = target
    }
  });

  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();
  s32 result = spn_link_target_exec(scratch.mem, target, output, objects);
  sp_mem_end_scratch(scratch);
  return result;
}

s32 link_target(spn_bg_cmd_t* cmd, void* user_data) {
  spn_target_unit_t* target = (spn_target_unit_t*)user_data;

  if (sp_da_empty(target->objects)) return 0;

  sp_str_t output = get_target_output_path(spn.mem, target);
  return spn_link_target_run(target, output, spn_build_target_objects(spn.mem, target));
}
