#include "ctx/types.h"
#include "event/event.h"
#include "session/types.h"
#include "sp/macro.h"
#include "unit/types.h"

#include "compiler/driver.h"
#include "paths/paths.h"
#include "session/invocation.h"
#include "session/session.h"
#include "graph/build.h"
#include "graph/nodes/nodes.h"
#include "unit/package.h"

s32 spn_compile_object_run(spn_compile_unit_t* unit, spn_path_t object, spn_path_t depfile) {
  spn_pkg_unit_t* pkg = unit->target->pkg;
  spn_session_t* session = pkg->session;

  spn_pkg_unit_announce_compile(pkg);

  spn_cc_compile_files_t files = {
    .source = unit->paths.file,
    .output = object,
    .depfile = depfile,
  };
  spn_invocation_t invocation = spn_cc_render_compile_command(spn.mem, &pkg->build->toolchain->cc, &pkg->build->profile, &unit->invocation, &files);
  sp_str_t source = spn_path_str(&spn.roots, spn.mem, files.source);
  sp_str_t output = spn_path_str(&spn.roots, spn.mem, files.output);
  spn_invocation_result_t run = spn_invocation_run(&invocation);
  sp_str_t command = spn_invocation_to_str(spn.mem, &invocation);

  if (run.result.status.exit_code) {
    spn_event_buffer_push(session->ctx->events, (spn_event_t) {
      .kind = SPN_EVENT_TARGET_BUILD_FAILED,
      .pkg = pkg->info->name,
      .target_failed = {
        .target = unit->target->info->name,
        .source_file = source,
        .object_file = output,
        .rc = run.result.status.exit_code,
        .out = run.result.out,
        .command = command,
        .time = run.elapsed,
      }
    });
  } else {
    spn_event_buffer_push(session->ctx->events, (spn_event_t) {
      .kind = SPN_EVENT_TARGET_BUILD_PASSED,
      .pkg = pkg->info->name,
      .target_passed = {
        .target = unit->target->info->name,
        .source_file = source,
        .object_file = output,
        .command = command,
        .out = run.result.out,
        .time = run.elapsed,
      }
    });
  }

  return run.result.status.exit_code;
}
