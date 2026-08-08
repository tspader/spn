#include "spn/host.h"

#include "ctx/types.h"
#include "op/op.h"
#include "session/types.h"

spn_op_t* spn_op_new(spn_ctx_t* ctx, spn_session_t* session, spn_op_kind_t kind) {
  if (session) {
    ctx = session->ctx;
  }
  spn_op_t* op = sp_alloc_type(ctx->heap, spn_op_t);
  *op = (spn_op_t) {
    .ctx = ctx,
    .session = session,
    .mem = ctx->heap,
    .result = { .kind = kind },
  };
  return op;
}

void spn_op_submit(spn_op_t* op) {
  switch (op->result.kind) {
    case SPN_OP_ADD:           op->result.err = spn_op_add(op); break;
    case SPN_OP_PUBLISH:       op->result.err = spn_op_publish(op); break;
    case SPN_OP_SYNC_INDEXES:  op->result.err = spn_op_sync_indexes(op); break;
    case SPN_OP_SCAFFOLD:      op->result.err = spn_op_scaffold(op); break;
    case SPN_OP_BUILD:         op->result.err = spn_op_build(op); break;
    case SPN_OP_TEST:          op->result.err = spn_op_test(op); break;
    case SPN_OP_RUN_TARGET:    op->result.err = spn_op_run_target(op); break;
    case SPN_OP_CLEAN:         op->result.err = spn_op_clean(op); break;
    case SPN_OP_CLEAN_PROFILE: op->result.err = spn_op_clean_profile(op); break;
  }

  op->done = true;
}

spn_err_t spn_op_wait(spn_op_t* op) {
  sp_assert(op->done);
  return op->result.err;
}

spn_op_result_t spn_op_result(spn_op_t* op) {
  sp_assert(op->done);
  return op->result;
}

void spn_op_free(spn_op_t* op) {
}
