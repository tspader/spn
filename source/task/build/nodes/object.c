#include "event/event.h"
#include "session/types.h"
#include "sp/macro.h"
#include "unit/types.h"

#include "external/cc.h"
#include "session/session.h"
#include "task/build/build.h"
#include "task/build/nodes/nodes.h"

void emit_result(spn_cc_run_t run) {
  switch (run.result.status.exit_code) {

  }
}

s32 compile_object(spn_bg_cmd_t* cmd, void* user_data) {
  spn_compile_unit_t* unit = (spn_compile_unit_t*)user_data;
  spn_session_t* session = unit->session;

  sp_str_t file = sp_fs_get_name(unit->paths.object);
  sp_str_t dir = sp_fs_parent_path(unit->paths.object);
  sp_fs_create_dir(dir);

  spn_cc_t* cc = sp_alloc_type(spn_cc_t);
  spn_cc_set_profile(cc, session->profile);
  spn_cc_set_output_dir(cc, dir);
  spn_cc_set_toolchain(cc, unit->session->units.toolchain);
  add_pkg_to_cc(cc, unit->package);

  spn_cc_target_t* target = spn_cc_add_target(cc, SPN_CC_OUTPUT_OBJECT, file);
  add_pkg_to_cc_target(target, unit->package, unit->target->info);

  // Dependencies publish their headers into their store; compile against them
  sp_da(spn_pkg_unit_t*) deps = spn_session_pkg_deps(session, unit->package);
  sp_da_for(deps, it) {
    if (!deps[it]) continue;
    spn_cc_target_add_absolute_include(target, deps[it]->paths.include);
  }

  if (!sp_da_empty(unit->target->info->embed)) {
    spn_cc_target_add_absolute_include(target, unit->target->paths.generated);
  }

  spn_cc_target_add_absolute_source(target, unit->paths.file);

  // Convert it into a subprocess
  sp_ps_config_t config = {
    .cwd = unit->target->paths.work,
    .io = {
      .in.mode = SP_PS_IO_MODE_NULL,
      .out.mode = SP_PS_IO_MODE_CREATE,
      .err.mode = SP_PS_IO_MODE_REDIRECT,
    }
  };
  spn_cc_to_ps(target->cc, &config);
  spn_cc_target_to_ps(target->cc, target, &config);

  sp_tm_timer_t timer = sp_tm_start_timer();
  sp_ps_output_t result = sp_ps_run(config);
  u64 elapsed = sp_tm_read_timer(&timer);

  if (result.status.exit_code) {
    spn_event_buffer_push_ex(session->events, unit->package->info, &unit->target->logs, (spn_build_event_t) {
      .kind = SPN_EVENT_TARGET_BUILD_FAILED,
      .target.failed = {
        .source_file = unit->paths.file,
        .object_file = unit->paths.object,
        .rc = result.status.exit_code,
        .out = result.err,
        .err = result.out,
        .time = elapsed,
      }
    });
  } else {
    spn_event_buffer_push_ex(session->events, unit->package->info, &unit->target->logs, (spn_build_event_t) {
      .kind = SPN_EVENT_TARGET_BUILD_PASSED,
      .target.passed = {
        .source_file = unit->paths.file,
        .object_file = unit->paths.object,
        .time = elapsed
      }
    });
  }

  return result.status.exit_code;
}

