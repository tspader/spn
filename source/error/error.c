#include "error/error.h"

#include "ctx/types.h"
#include "event/event.h"

spn_err_union_t spn_err_emit(spn_err_union_t err) {
  if (err.kind && !err.reported) {
    spn_event_buffer_push(spn.events, (spn_build_event_t) {
      .kind = SPN_EVENT_ERR,
      .err = err,
    });
    err.reported = true;
  }
  return err;
}
