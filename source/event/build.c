#include "sp.h"
#include "event/build.h"

#include "sp/io.h"
#include "sp/macro.h"

void spn_event_log_build(sp_io_writer_t* out, spn_build_event_t* event) {
  sp_str_t command = sp_zero;
  sp_str_t transcript = sp_zero;

  switch (event->kind) {
    case SPN_EVENT_TARGET_BUILD_PASSED: command = event->target_passed.command; transcript = event->target_passed.out; break;
    case SPN_EVENT_TARGET_BUILD_FAILED: command = event->target_failed.command; transcript = event->target_failed.out; break;
    case SPN_EVENT_LINK_PASSED:         command = event->link_passed.command;   transcript = event->link_passed.out;   break;
    case SPN_EVENT_LINK_FAILED:         command = event->link_failed.command;   transcript = event->link_failed.out;   break;
    default: return;
  }

  if (sp_str_empty(transcript)) {
    return;
  }

  sp_io_write_str(out, command, SP_NULLPTR);
  sp_io_write_new_line(out);
  sp_io_write_str(out, transcript, SP_NULLPTR);
  sp_io_write_new_line(out);
}
