#include "ctx/types.h"
#include "session/types.h"
#include "unit/types.h"

#include "event/log.h"
#include "external/cc.h"
#include "session/invocation.h"
#include "session/session.h"

void spn_session_build_invocations(spn_session_t* session) {
  sp_mem_t mem = session->mem;
  sp_om_for(session->units.objects, it) {
    spn_compile_unit_t* unit = sp_om_at(session->units.objects, it);

    spn_cc_t cc;
    spn_cc_init(&cc, mem);
    spn_cc_set_profile(&cc, unit->package->build->profile);
    spn_cc_set_toolchain(&cc, unit->package->build->toolchain);
    spn_cc_add_pkg(&cc, unit->package);

    spn_cc_target_t* target = spn_cc_add_target(&cc, SPN_CC_OUTPUT_OBJECT, unit->paths.object);
    spn_cc_target_set_lang(target, unit->lang);
    spn_cc_target_add_info(target, unit->package, unit->target->info);
    if (unit->target->info->kind == SPN_TARGET_LIB) {
      spn_cc_target_add_flag(target, sp_str_lit("-fPIC"));
    }

    sp_da(spn_pkg_dep_t) deps = spn_session_pkg_deps(session, unit->package);
    sp_da_for(deps, it) {
      if (!deps[it].unit) {
        continue;
      }

      switch (deps[it].kind) {
        case SPN_DEP_KIND_PACKAGE: {
          break;
        }
        case SPN_DEP_KIND_TEST: {
          if (unit->target->info->kind != SPN_TARGET_TEST) {
            continue;
          }
          break;
        }
        case SPN_DEP_KIND_BUILD: {
          continue;
        }
      }

      spn_cc_target_add_absolute_include(target, deps[it].unit->paths.include);
      sp_da_for(deps[it].unit->info->public_define, dt) {
        spn_cc_target_add_define(target, deps[it].unit->info->public_define[dt]);
      }
    }

    if (!sp_da_empty(unit->target->info->embed)) {
      spn_cc_target_add_absolute_include(target, unit->target->paths.generated);
    }

    spn_cc_target_add_absolute_source(target, unit->paths.file);

    sp_ps_config_t ps = sp_zero_s(sp_ps_config_t);
    spn_cc_to_ps(mem, &cc, target, &ps);
    spn_cc_target_to_ps(mem, &cc, target, &ps);

    unit->invocation = (spn_invocation_t) {
      .program = ps.command,
      .args = ps.dyn_args,
      .cwd = unit->target->paths.work,
    };
  }
}

spn_err_t spn_session_write_compile_commands(spn_session_t* session, sp_str_t path) {
  sp_mem_arena_marker_t scratch = sp_mem_begin_scratch();

  sp_io_dyn_mem_writer_t buf;
  sp_io_dyn_mem_writer_init(scratch.mem, &buf);
  sp_io_writer_t* io = &buf.base;

  sp_io_write_cstr(io, "[", SP_NULLPTR);
  u32 count = 0;
  sp_om_for(session->units.objects, it) {
    spn_compile_unit_t* unit = sp_om_at(session->units.objects, it);
    spn_invocation_t* invocation = &unit->invocation;

    if (count++) {
      sp_io_write_c8(io, ',');
    }
    sp_io_write_cstr(io, "\n  { \"directory\": ", SP_NULLPTR);
    spn_json_write_str(io, invocation->cwd);
    sp_io_write_cstr(io, ", \"file\": ", SP_NULLPTR);
    spn_json_write_str(io, unit->paths.file);
    sp_io_write_cstr(io, ", \"output\": ", SP_NULLPTR);
    spn_json_write_str(io, unit->paths.object);
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
