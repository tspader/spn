#include "ctx/types.h"
#include "unit/types.h"
#include "session/types.h"
#include "target/types.h"

#include "external/cc.h"
#include "enum/enum.h"
#include "event/event.h"
#include "toolchain/toolchain.h"
#include "task/build/build.h"

typedef struct {
  sp_ps_output_t result;
  u64 elapsed;
  sp_str_t args;
} ar_result_t;

ar_result_t run_ar_exec(spn_target_unit_t* unit, sp_str_t output) {
  spn_toolchain_t* toolchain = &unit->session->toolchain;
  sp_ps_config_t ps = {
    .command = toolchain->archiver.program,
    .cwd = unit->paths.work,
    .io = {
      .in.mode = SP_PS_IO_MODE_NULL,
      .err.mode = SP_PS_IO_MODE_REDIRECT,
    },
  };
  sp_da_for(toolchain->archiver.args, ai) {
    sp_ps_config_add_arg(&ps, toolchain->archiver.args[ai]);
  }
  sp_ps_config_add_arg(&ps, sp_str_lit("rcs"));
  sp_ps_config_add_arg(&ps, output);

  sp_da_for(unit->objects, it) {
    sp_ps_config_add_arg(&ps, unit->objects[it]->paths.object);
  }

  sp_str_builder_t log = SP_ZERO_INITIALIZE();
  sp_str_builder_append(&log, ps.command);
  sp_str_builder_append_c8(&log, ' ');
  sp_da_for(ps.dyn_args, it) {
    sp_str_builder_append(&log, ps.dyn_args[it]);
    sp_str_builder_append_c8(&log, ' ');
  }

  sp_tm_timer_t timer = sp_tm_start_timer();
  sp_ps_output_t result = sp_ps_run(ps);
  u64 elapsed = sp_tm_read_timer(&timer);

  return (ar_result_t) {
    .result = result,
    .elapsed = elapsed,
    .args = sp_str_builder_to_str(&log),
  };
}

spn_err_t emit_success(spn_target_unit_t* unit, sp_str_t output, u64 elapsed) {
  spn_event_buffer_push_ex(spn.events, unit->pkg, &unit->logs, (spn_build_event_t) {
    .kind = SPN_EVENT_LINK_PASSED,
    .target.name = unit->info->name,
    .target.link_passed = {
      .output_path = output,
      .time = elapsed,
    }
  });
  return SPN_OK;
}

spn_err_t emit_failure(spn_target_unit_t* unit, sp_str_t linker, sp_str_t args, s32 rc, sp_str_t out, sp_str_t err) {
  spn_event_buffer_push_ex(spn.events, unit->pkg, &unit->logs, (spn_build_event_t) {
    .kind = SPN_EVENT_LINK_FAILED,
    .target.name = unit->info->name,
    .target.link_failed = {
      .exit_code = rc,
      .out = out,
      .err = err,
      .linker = linker,
      .args = args,
    }
  });
  return SPN_ERROR;
}


s32 link_target(spn_bg_cmd_t* cmd, void* user_data) {
  spn_target_unit_t* unit = (spn_target_unit_t*)user_data;
  spn_target_t* target = unit->info;

  sp_str_t output = get_target_output_path(unit);
  sp_str_t output_name = sp_fs_get_name(output);
  bool has_embeds = !sp_da_empty(target->embed);

  switch (target->kind) {
    case SPN_TARGET_STATIC_LIB: {
      ar_result_t run = run_ar_exec(unit, output);

      spn_event_buffer_push_ex(spn.events, unit->pkg, &unit->logs, (spn_build_event_t) {
        .kind = SPN_EVENT_LINK_START,
        .target.name = target->name,
        .target.link_start = {
          .kind = target->kind,
          .num_objects = sp_da_size(unit->objects),
          .output_path = output,
          .linker = unit->session->toolchain.archiver.program,
          .args = run.args,
          .has_embeds = has_embeds,
        }
      });

      if (run.result.status.exit_code) {
        emit_failure(unit, unit->session->toolchain.archiver.program, run.args, run.result.status.exit_code, run.result.out, run.result.err);
        return SPN_ERROR;
      }

      emit_success(unit, output, run.elapsed);
      return SPN_OK;
    }
    case SPN_TARGET_EXE:
    case SPN_TARGET_SHARED_LIB: {
      spn_cc_t* cc = sp_alloc_type(spn_cc_t);
      spn_cc_set_profile(cc, &unit->session->profile);
      spn_cc_set_output_dir(cc, sp_fs_parent_path(output));
      spn_cc_set_toolchain(cc, unit->session->toolchain);
      add_pkg_to_cc(cc, unit->pkg);

      spn_cc_target_t* cc_target = spn_cc_add_target(cc, target->kind, output_name);
      add_pkg_to_cc_target(cc_target, unit->pkg, target);
      add_deps_to_cc_target(cc_target, unit->pkg, target, unit->session);

      sp_da_for(unit->objects, it) {
        spn_cc_target_add_absolute_source(cc_target, unit->objects[it]->paths.object);
      }

      if (has_embeds) {
        spn_cc_target_add_absolute_source(cc_target, get_embed_object_path(unit));
      }

      spn_cc_run_t run = spn_cc_target_run(cc_target, unit->paths.work);
      s32 rc = run.result.status.exit_code;

      sp_str_t linker = spn_toolchain_get_linker_driver(&cc->toolchain.info).program;
      spn_event_buffer_push_ex(spn.events, unit->pkg, &unit->logs, (spn_build_event_t) {
        .kind = SPN_EVENT_LINK_START,
        .target.name = target->name,
        .target.link_start = {
          .kind = target->kind,
          .num_objects = sp_da_size(unit->objects),
          .output_path = output,
          .linker = linker,
          .args = run.args,
          .has_embeds = has_embeds,
        }
      });

      if (rc) return emit_failure(unit, linker, run.args, rc, run.result.out, run.result.err);
      else return emit_success(unit, output, run.elapsed);

      sp_unreachable_return(69);
    }
    case SPN_TARGET_NONE:
    case SPN_TARGET_JIT:
    case SPN_TARGET_OBJECT: {
      SP_UNREACHABLE_CASE();
    }
  }

  SP_UNREACHABLE_RETURN(SPN_ERROR);
}
