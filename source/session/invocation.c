#include "ctx/types.h"
#include "session/types.h"
#include "unit/types.h"

#include "event/log.h"
#include "compiler/driver.h"
#include "external/cc.h"
#include "session/invocation.h"
#include "session/session.h"
#include "unit/compiler.h"

static sp_str_t resolve_pkg_path(sp_mem_t mem, spn_pkg_unit_t* pkg, sp_str_t path) {
  if (sp_fs_is_absolute(path)) {
    return path;
  }
  return sp_fs_join_path(mem, pkg->paths.source, path);
}

spn_err_union_t spn_session_build_invocations(spn_session_t* session) {
  sp_mem_t mem = session->mem;
  sp_om_for(session->units.objects, it) {
    spn_compile_unit_t* unit = sp_om_at(session->units.objects, it);
    if (!sp_str_empty(unit->invocation.program)) {
      continue;
    }

    spn_build_unit_t* build = unit->target->build;

    spn_cc_compile_t compile = {
      .lang = unit->lang,
      .source = unit->paths.file,
      .output = unit->paths.object,
      .cxx = unit->target->info->cxx,
      .pic = unit->target->info->kind == SPN_TARGET_LIB,
      .visibility = build->visibility,
    };
    sp_da_init(mem, compile.include);
    sp_da_init(mem, compile.define);
    sp_da_init(mem, compile.args);

    sp_da_for(build->include, it) {
      sp_da_push(compile.include, build->include[it]);
    }

    bool program = unit->target->info->kind == SPN_TARGET_CONFIGURE_PROGRAM ||
      unit->target->info->kind == SPN_TARGET_BUILD_PROGRAM;
    if (!program) {
      sp_da_for(unit->package->info->include, it) {
        sp_da_push(compile.include, resolve_pkg_path(mem, unit->package, unit->package->info->include[it]));
      }
      sp_da_for(unit->package->info->define, it) {
        sp_da_push(compile.define, unit->package->info->define[it]);
      }
    }

    sp_da_for(unit->target->info->include, it) {
      sp_da_push(compile.include, resolve_pkg_path(mem, unit->package, unit->target->info->include[it]));
    }
    sp_da_for(unit->target->info->define, it) {
      sp_da_push(compile.define, unit->target->info->define[it]);
    }
    sp_da_for(unit->target->info->flags, it) {
      sp_da_push(compile.args, unit->target->info->flags[it]);
    }

    if (!program) {
      sp_da(spn_pkg_dep_t) deps = spn_session_pkg_deps(session, unit->package);
      sp_da_for(deps, it) {
        if (!deps[it].unit) {
          continue;
        }
        if (!(build->dep_kinds & spn_dep_kind_bit(deps[it].kind))) {
          continue;
        }
        if (deps[it].kind == SPN_DEP_KIND_TEST && unit->target->info->kind != SPN_TARGET_TEST) {
          continue;
        }

        spn_pkg_unit_t* dependency = deps[it].unit;
        sp_da_push(compile.include, dependency->paths.include);
        sp_da_for(dependency->info->public_define, it) {
          sp_da_push(compile.define, dependency->info->public_define[it]);
        }
      }
    }

    if (program) {
      sp_da_for(unit->target->deps.package, it) {
        spn_pkg_unit_t* dependency = unit->target->deps.package[it];
        sp_da_push(compile.include, dependency->paths.include);
        sp_da_for(dependency->info->public_define, it) {
          sp_da_push(compile.define, dependency->info->public_define[it]);
        }
      }
    }

    if (!sp_da_empty(unit->target->info->embed)) {
      sp_da_push(compile.include, unit->target->paths.generated);
    }

    sp_ps_config_t ps = sp_zero_s(sp_ps_config_t);
    spn_cc_toolchain_t toolchain = spn_toolchain_unit_compiler(unit->target->build->toolchain);
    spn_err_union_t err = spn_cc_render_compile(mem, &toolchain, &unit->target->build->profile, &compile, &ps);
    if (err.kind) {
      return err;
    }

    unit->invocation = (spn_invocation_t) {
      .program = ps.command,
      .args = ps.dyn_args,
      .cwd = unit->target->paths.work,
    };
    sp_da_push(session->units.compile_commands, ((spn_compile_command_t) {
      .source = unit->paths.file,
      .output = unit->paths.object,
      .invocation = unit->invocation,
    }));
  }
  return spn_result(SPN_OK);
}

spn_err_t spn_session_write_compile_commands(spn_session_t* session, sp_str_t path) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();

  sp_io_dyn_mem_writer_t buf;
  sp_io_dyn_mem_writer_init(scratch.mem, &buf);
  sp_io_writer_t* io = &buf.base;

  sp_io_write_cstr(io, "[", SP_NULLPTR);
  u32 count = 0;
  sp_da_for(session->units.compile_commands, it) {
    spn_compile_command_t* command = &session->units.compile_commands[it];
    spn_invocation_t* invocation = &command->invocation;

    if (count++) {
      sp_io_write_c8(io, ',');
    }
    sp_io_write_cstr(io, "\n  { \"directory\": ", SP_NULLPTR);
    spn_json_write_str(io, invocation->cwd);
    sp_io_write_cstr(io, ", \"file\": ", SP_NULLPTR);
    spn_json_write_str(io, command->source);
    sp_io_write_cstr(io, ", \"output\": ", SP_NULLPTR);
    spn_json_write_str(io, command->output);
    sp_io_write_cstr(io, ", \"arguments\": [", SP_NULLPTR);
    spn_json_write_str(io, invocation->program);
    sp_da_for(invocation->args, arg) {
      sp_io_write_cstr(io, ", ", SP_NULLPTR);
      spn_json_write_str(io, invocation->args[arg]);
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
  return sp_fs_join_path(session->mem, session->paths.root, sp_str_lit("compile_commands.json"));
}

spn_invocation_result_t spn_invocation_run(spn_invocation_t* invocation) {
  sp_ps_config_t ps = {
    .command = invocation->program,
    .dyn_args = invocation->args,
    .cwd = invocation->cwd,
    .io = {
      .in.mode = SP_PS_IO_MODE_NULL,
      .err.mode = SP_PS_IO_MODE_REDIRECT,
    }
  };

  sp_tm_timer_t timer = sp_tm_start_timer();
  sp_ps_output_t result = sp_ps_run(spn.mem, ps);
  u64 elapsed = sp_tm_read_timer(&timer);

  return (spn_invocation_result_t) {
    .result = result,
    .elapsed = elapsed,
  };
}
