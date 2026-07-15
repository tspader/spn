#include "ctx/types.h"
#include "event/event.h"
#include "session/types.h"
#include "sp/macro.h"
#include "unit/types.h"

#include "session/invocation.h"
#include "session/session.h"
#include "task/build/build.h"
#include "task/build/nodes/nodes.h"
#include "unit/package.h"

s32 spn_compile_object_run(spn_compile_unit_t* unit, sp_str_t object, sp_str_t depfile) {
  spn_pkg_unit_t* pkg = unit->target->pkg;
  spn_session_t* session = pkg->session;

  spn_pkg_unit_announce_compile(pkg);

  spn_err_union_t render = spn_build_render_compile(spn.mem, unit, object, depfile, &unit->invocation);
  if (render.kind) {
    spn_event_buffer_push(spn.events, (spn_build_event_t) {
      .kind = SPN_EVENT_ERR,
      .err = render,
    });
    return 1;
  }

  spn_invocation_result_t run = spn_invocation_run(&unit->invocation);

  if (run.result.status.exit_code) {
    spn_event_buffer_push_ex(session->events, pkg->info, &unit->target->logs, (spn_build_event_t) {
      .kind = SPN_EVENT_TARGET_BUILD_FAILED,
      .target.failed = {
        .source_file = unit->paths.file,
        .object_file = object,
        .rc = run.result.status.exit_code,
        .out = run.result.out,
        .invocation = &unit->invocation,
        .time = run.elapsed,
      }
    });
  } else {
    spn_event_buffer_push_ex(session->events, pkg->info, &unit->target->logs, (spn_build_event_t) {
      .kind = SPN_EVENT_TARGET_BUILD_PASSED,
      .target.passed = {
        .source_file = unit->paths.file,
        .object_file = object,
        .invocation = &unit->invocation,
        .out = run.result.out,
        .time = run.elapsed,
      }
    });
  }

  return run.result.status.exit_code;
}

s32 compile_object(spn_bg_cmd_t* cmd, void* user_data) {
  spn_compile_unit_t* unit = (spn_compile_unit_t*)user_data;
  sp_fs_create_dir(sp_fs_parent_path(unit->paths.object));
  return spn_compile_object_run(unit, unit->paths.object, sp_str_lit(""));
}
