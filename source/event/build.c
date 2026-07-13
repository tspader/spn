#include "sp.h"
#include "event/build.h"

#include "sp/io.h"
#include "sp/macro.h"

void spn_event_log_build(sp_io_writer_t* out, spn_build_event_t* event) {
  spn_invocation_t* invocation = SP_NULLPTR;
  sp_str_t transcript = sp_zero;

  switch (event->kind) {
    case SPN_EVENT_TARGET_BUILD_PASSED: invocation = event->target.passed.invocation;      transcript = event->target.passed.out;      break;
    case SPN_EVENT_TARGET_BUILD_FAILED: invocation = event->target.failed.invocation;      transcript = event->target.failed.out;      break;
    case SPN_EVENT_LINK_PASSED:         invocation = event->target.link_passed.invocation; transcript = event->target.link_passed.out; break;
    case SPN_EVENT_LINK_FAILED:         invocation = event->target.link_failed.invocation; transcript = event->target.link_failed.out; break;
    default: return;
  }

  if (sp_str_empty(transcript)) {
    return;
  }

  if (invocation) {
    sp_io_write_str(out, invocation->program, SP_NULLPTR);
    sp_da_for(invocation->args, it) {
      sp_io_write_c8(out, ' ');
      sp_io_write_str(out, invocation->args[it], SP_NULLPTR);
    }
  }
  sp_io_write_new_line(out);
  sp_io_write_str(out, transcript, SP_NULLPTR);
  sp_io_write_new_line(out);
}
