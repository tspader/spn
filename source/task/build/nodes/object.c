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

s32 compile_object(spn_bg_cmd_t* cmd, void* user_data) {
  spn_compile_unit_t* unit = (spn_compile_unit_t*)user_data;
  spn_session_t* session = unit->session;

  spn_pkg_unit_announce_compile(unit->package);

  sp_fs_create_dir(sp_fs_parent_path(unit->paths.object));

  spn_invocation_result_t run = spn_invocation_run(&unit->invocation);


  // @spader
  // This is vestigial; we used to just assemble the invocation here, in the
  // build graph. This meant that when we had to communicate the command back
  // to the reporting thread, there was nothing to point to, so we just heap
  // allocated a big string of all the arguments.
  //
  // But now, we already *have* the invocation. There's no point in assembling
  // this string on the heap just so we can log it
  sp_io_dyn_mem_writer_t io;
  sp_io_dyn_mem_writer_init(spn.mem, &io);
  sp_io_write_str(&io.base, unit->invocation.program, SP_NULLPTR);
  sp_io_write_c8(&io.base, ' ');
  sp_da_for(unit->invocation.args, it) {
    sp_io_write_str(&io.base, unit->invocation.args[it], SP_NULLPTR);
    sp_io_write_c8(&io.base, ' ');
  }

  if (run.result.status.exit_code) {
    spn_event_buffer_push_ex(session->events, unit->package->info, &unit->target->logs, (spn_build_event_t) {
      .kind = SPN_EVENT_TARGET_BUILD_FAILED,
      .target.failed = {
        .source_file = unit->paths.file,
        .object_file = unit->paths.object,
        .rc = run.result.status.exit_code,
        .out = run.result.out,
        .args = sp_io_dyn_mem_writer_take_str(&io),
        .time = run.elapsed,
      }
    });
  } else {
    spn_event_buffer_push_ex(session->events, unit->package->info, &unit->target->logs, (spn_build_event_t) {
      .kind = SPN_EVENT_TARGET_BUILD_PASSED,
      .target.passed = {
        .source_file = unit->paths.file,
        .object_file = unit->paths.object,
        .args = args,
        .out = run.result.out,
        .time = run.elapsed,
      }
    });
  }

  return run.result.status.exit_code;
}
