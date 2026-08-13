#ifndef SPN_EVENT_TYPES_H
#define SPN_EVENT_TYPES_H

#include "sp.h"

#include "events.gen.h"
#include "log/lazy/types.h"

struct spn_event_buffer_t {
  sp_rb(spn_build_event_t) buffer;
  spn_build_event_t current;
  sp_mutex_t mutex;
  spn_lazy_log_t log;
  sp_io_dyn_mem_writer_t backlog;
  spn_wake_t* wake;
};

#endif
