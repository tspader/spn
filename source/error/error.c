#include "error/error.h"

#include "ctx/types.h"
#include "event/event.h"

spn_err_t spn_err_emit(spn_ctx_t* ctx, spn_err_union_t err) {
  if (err.kind) {
    if (!err.reported) {
      spn_event_buffer_push(ctx->events, (spn_build_event_t) {
        .kind = SPN_EVENT_ERR,
        .err = err,
      });
    }
    sp_atomic_s32_cas(&ctx->error, 0, err.kind, SP_ATOMIC_SEQ_CST);
  }
  return err.kind;
}
