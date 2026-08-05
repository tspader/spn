#include "event/flush.h"

#include "event/build.h"
#include "event/event.h"
#include "event/log.h"

sp_da(spn_build_event_t) spn_event_flush(sp_mem_t mem, spn_event_buffer_t* events) {
  sp_da(spn_build_event_t) drained = spn_event_buffer_drain(mem, events);

  sp_da_for(drained, it) {
    spn_build_event_t* event = &drained[it];
    if (event->io) {
      spn_event_log_jsonl(&event->io->jsonl.writer, event);
      spn_event_log_build(&event->io->build.writer, event);
    }
  }

  return drained;
}
