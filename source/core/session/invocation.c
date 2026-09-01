#include "ctx/types.h"
#include "session/types.h"
#include "unit/types.h"

#include "codegen/codegen.h"
#include "compiler/driver.h"
#include "external/cc.h"
#include "paths/paths.h"
#include "session/invocation.h"
#include "session/session.h"
#include "unit/unit.h"
#include "graph/build.h"

static spn_cc_compile_t compile_desc(sp_mem_t mem, spn_compile_unit_t* unit) {
  spn_pkg_unit_t* pkg = unit->target->pkg;
  spn_build_unit_t* build = pkg->build;

  spn_cc_compile_t compile = {
    .lang = unit->lang,
    .cxx = unit->target->info->cxx,
    .pic = unit->target->info->kind == SPN_TARGET_KIND_LIB && build->profile.os != SPN_OS_FREESTANDING,
  };
  if (build->profile.os == SPN_OS_MACOS) {
    compile.min_os = unit->target->link.cc.min_os;
  }
  sp_da_init(mem, compile.include);
  sp_da_init(mem, compile.define);
  sp_da_init(mem, compile.args);

  sp_da_for(build->include, it) {
    sp_da_push(compile.include, build->include[it]);
  }
  sp_da_for(build->define, it) {
    sp_da_push(compile.define, build->define[it]);
  }

  sp_da_for(pkg->info->include, it) {
    sp_da_push(compile.include, pkg->info->include[it]);
  }
  sp_da_for(pkg->info->define, it) {
    sp_da_push(compile.define, pkg->info->define[it]);
  }

  sp_da_for(unit->target->info->include, it) {
    sp_da_push(compile.include, unit->target->info->include[it]);
  }
  sp_da_for(unit->target->info->define, it) {
    sp_da_push(compile.define, unit->target->info->define[it]);
  }
  sp_da_for(unit->target->info->flags, it) {
    sp_da_push(compile.args, unit->target->info->flags[it]);
  }

  if (unit->target->info->kind == SPN_TARGET_KIND_EXAMPLE) {
    sp_da_push(compile.include, pkg->paths.include);
  }

  sp_da_for(pkg->deps, it) {
    if (!spn_dep_kind_applies(pkg->deps[it].kind, unit->target->info->kind)) {
      continue;
    }

    spn_pkg_unit_t* dependency = pkg->deps[it].unit;
    sp_da_push(compile.include, dependency->paths.include);
    sp_da_for(dependency->info->public_define, jt) {
      sp_da_push(compile.define, dependency->info->public_define[jt]);
    }
  }

  if (!sp_da_empty(unit->target->info->embed)) {
    sp_da_push(compile.include, spn_target_unit_object_dir(mem, unit->target));
  }

  return compile;
}

spn_err_t spn_build_render_compile(sp_mem_t mem, spn_compile_unit_t* unit, spn_invocation_t* invocation) {
  spn_pkg_unit_t* pkg = unit->target->pkg;
  spn_build_unit_t* build = pkg->build;

  spn_cc_compile_t compile = compile_desc(mem, unit);
  spn_try(spn_cc_render_compile(mem, &build->toolchain->cc, &build->profile, &compile, invocation));
  invocation->cwd = pkg->paths.work;
  return SPN_OK;
}

spn_err_t spn_session_write_compile_commands(spn_session_t* session, sp_str_t path) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();

  sp_io_dyn_mem_writer_t buf;
  sp_io_dyn_mem_writer_init(scratch.mem, &buf);
  sp_io_writer_t* io = &buf.base;

  sp_io_write_cstr(io, "[", SP_NULLPTR);
  const spn_path_roots_t* roots = &spn.roots;
  u32 count = 0;
  sp_om_for(session->units.objects, it) {
    spn_compile_unit_t* unit = sp_om_at(session->units.objects, it);
    spn_cc_compile_files_t files = {
      .source = unit->paths.file,
      .output = unit->paths.object,
    };
    spn_invocation_t invocation = spn_cc_render_compile_command(scratch.mem, &unit->target->pkg->build->toolchain->cc, &unit->target->pkg->build->profile, &unit->invocation, &files);
    sp_da(sp_str_t) args = spn_invocation_args(roots, scratch.mem, &invocation);

    if (count++) {
      sp_io_write_c8(io, ',');
    }
    sp_io_write_cstr(io, "\n  { \"directory\": ", SP_NULLPTR);
    spn_codegen_json_str(io, spn_path_str(roots, scratch.mem, invocation.cwd));
    sp_io_write_cstr(io, ", \"file\": ", SP_NULLPTR);
    spn_codegen_json_str(io, spn_path_str(roots, scratch.mem, files.source));
    sp_io_write_cstr(io, ", \"output\": ", SP_NULLPTR);
    spn_codegen_json_str(io, spn_path_str(roots, scratch.mem, files.output));
    sp_io_write_cstr(io, ", \"arguments\": [", SP_NULLPTR);
    spn_codegen_json_str(io, spn_arg_str(roots, scratch.mem, invocation.program));
    sp_da_for(args, arg) {
      sp_io_write_cstr(io, ", ", SP_NULLPTR);
      spn_codegen_json_str(io, args[arg]);
    }
    sp_io_write_cstr(io, "] }", SP_NULLPTR);
  }
  sp_io_write_cstr(io, "\n]\n", SP_NULLPTR);

  sp_str_t content = sp_io_dyn_mem_writer_take_str(&buf);

  sp_io_file_writer_t writer = sp_zero;
  if (sp_io_file_writer_from_path(&writer, path) != SP_OK) {
    sp_mem_end_scratch(scratch);
    return SPN_ERROR;
  }
  sp_io_write_str(&writer.base, content, SP_NULLPTR);
  sp_io_file_writer_close(&writer);

  sp_mem_end_scratch(scratch);
  return SPN_OK;
}

sp_str_t spn_session_compile_commands_path(spn_session_t* session) {
  spn_path_t path = spn_path_join(session->mem, session->paths.root, sp_str_lit("compile_commands.json"));
  return spn_path_str(&spn.roots, session->mem, path);
}

sp_da(sp_str_t) spn_invocation_args(const spn_path_roots_t* roots, sp_mem_t mem, const spn_invocation_t* invocation) {
  sp_da(sp_str_t) args = sp_da_new(mem, sp_str_t);
  sp_da_reserve(args, sp_da_size(invocation->args));
  sp_da_for(invocation->args, it) {
    sp_da_push(args, spn_arg_str(roots, mem, invocation->args[it]));
  }
  return args;
}

sp_str_t spn_invocation_to_str(sp_mem_t mem, const spn_invocation_t* invocation) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch_for(mem);
  sp_da(sp_str_t) parts = sp_da_new(scratch.mem, sp_str_t);
  sp_da_push(parts, spn_arg_str(&spn.roots, scratch.mem, invocation->program));
  sp_da_for(invocation->args, it) {
    sp_da_push(parts, spn_arg_str(&spn.roots, scratch.mem, invocation->args[it]));
  }

  sp_str_t command = sp_str_join_n(mem, parts, sp_da_size(parts), sp_str_lit(" "));
  sp_mem_end_scratch(scratch);
  return command;
}

spn_invocation_result_t spn_invocation_run(spn_invocation_t* invocation) {
  const spn_path_roots_t* roots = &spn.roots;
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();

  sp_str_t cwd = spn_path_str(roots, scratch.mem, invocation->cwd);
  sp_fs_create_dir(cwd);

  sp_ps_config_t ps = {
    .command = spn_arg_str(roots, scratch.mem, invocation->program),
    .dyn_args = spn_invocation_args(roots, scratch.mem, invocation),
    .cwd = cwd,
    .io = {
      .in.mode = SP_PS_IO_MODE_NULL,
      .err.mode = SP_PS_IO_MODE_REDIRECT,
    }
  };

  sp_tm_timer_t timer = sp_tm_start_timer();
  sp_ps_output_t result = sp_ps_run(spn.mem, ps);
  u64 elapsed = sp_tm_read_timer(&timer);
  sp_mem_end_scratch(scratch);

  return (spn_invocation_result_t) {
    .result = result,
    .elapsed = elapsed,
  };
}
