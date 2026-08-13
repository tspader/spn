#include "ctx/types.h"
#include "event/event.h"
#include "session/types.h"
#include "sp/macro.h"
#include "unit/types.h"

#include "compiler/driver.h"
#include "session/invocation.h"
#include "session/session.h"
#include "graph/build.h"
#include "graph/nodes/nodes.h"
#include "unit/package.h"

s32 spn_compile_object_run(spn_compile_unit_t* unit, sp_str_t object, sp_str_t depfile) {
  spn_pkg_unit_t* pkg = unit->target->pkg;
  spn_session_t* session = pkg->session;

  spn_pkg_unit_announce_compile(pkg);

  spn_cc_compile_files_t files = {
    .source = unit->paths.file,
    .output = object,
    .depfile = depfile,
  };
  spn_invocation_t invocation = spn_cc_render_compile_command(spn.mem, &pkg->build->toolchain->cc, &unit->invocation, &files);
  spn_invocation_result_t run = spn_invocation_run(&invocation);
  sp_str_t command = spn_invocation_to_str(spn.mem, &invocation);

  if (run.result.status.exit_code) {
    spn_event_buffer_push(session->ctx->events, (spn_build_event_t) {
      .kind = SPN_EVENT_TARGET_BUILD_FAILED,
      .pkg = pkg->info,
      .target_failed = {
        .target = unit->target->info->name,
        .source_file = unit->paths.file,
        .object_file = object,
        .rc = run.result.status.exit_code,
        .out = run.result.out,
        .command = command,
        .time = run.elapsed,
      }
    });
  } else {
    spn_event_buffer_push(session->ctx->events, (spn_build_event_t) {
      .kind = SPN_EVENT_TARGET_BUILD_PASSED,
      .pkg = pkg->info,
      .target_passed = {
        .target = unit->target->info->name,
        .source_file = unit->paths.file,
        .object_file = object,
        .command = command,
        .out = run.result.out,
        .time = run.elapsed,
      }
    });
  }

  return run.result.status.exit_code;
}
