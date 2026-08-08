#include "spn/host.h"

#include "ctx/types.h"
#include "external/wasm/wasm.h"
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

static void exec(spn_op_t* op) {
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
}

static s32 op_thread(void* data) {
  spn_ctx_t* ctx = (spn_ctx_t*)data;

  while (true) {
    sp_mutex_lock(&ctx->ops.mutex);
    while (!ctx->ops.shutdown && ctx->ops.next == sp_da_size(ctx->ops.queue)) {
      sp_cv_wait(&ctx->ops.signal.submitted, &ctx->ops.mutex);
    }
    if (ctx->ops.next == sp_da_size(ctx->ops.queue)) {
      sp_mutex_unlock(&ctx->ops.mutex);
      spn_wasm_thread_exit();
      return 0;
    }
    spn_op_t* op = ctx->ops.queue[ctx->ops.next++];
    if (ctx->ops.next == sp_da_size(ctx->ops.queue)) {
      sp_da_clear(ctx->ops.queue);
      ctx->ops.next = 0;
    }
    sp_mutex_unlock(&ctx->ops.mutex);

    exec(op);

    sp_mutex_lock(&ctx->ops.mutex);
    sp_atomic_s32_store(&op->done, 1, SP_ATOMIC_SEQ_CST);
    sp_mutex_unlock(&ctx->ops.mutex);
    sp_cv_notify_all(&ctx->ops.signal.completed);
  }
}

void spn_op_submit(spn_op_t* op) {
  spn_ctx_t* ctx = op->ctx;
  sp_mutex_lock(&ctx->ops.mutex);
  if (!ctx->ops.started) {
    ctx->ops.started = true;
    sp_thread_init(&ctx->ops.thread, op_thread, ctx);
  }
  sp_da_push(ctx->ops.queue, op);
  sp_mutex_unlock(&ctx->ops.mutex);
  sp_cv_notify_one(&ctx->ops.signal.submitted);
}

bool spn_op_done(spn_op_t* op) {
  return sp_atomic_s32_load(&op->done, SP_ATOMIC_SEQ_CST) != 0;
}

spn_err_t spn_op_wait(spn_op_t* op) {
  spn_ctx_t* ctx = op->ctx;
  sp_mutex_lock(&ctx->ops.mutex);
  while (!sp_atomic_s32_load(&op->done, SP_ATOMIC_SEQ_CST)) {
    sp_cv_wait(&ctx->ops.signal.completed, &ctx->ops.mutex);
  }
  sp_mutex_unlock(&ctx->ops.mutex);
  return op->result.err;
}

spn_op_result_t spn_op_result(spn_op_t* op) {
  sp_assert(spn_op_done(op));
  return op->result;
}

void spn_op_free(spn_op_t* op) {
}
