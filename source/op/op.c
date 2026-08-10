#include "core/core.h"
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

void spn_op_cancel(spn_op_t* op) {
  sp_atomic_s32_store(&op->cancelled, 1, SP_ATOMIC_SEQ_CST);
}

bool spn_op_cancelled(spn_op_t* op) {
  return sp_atomic_s32_load(&op->cancelled, SP_ATOMIC_SEQ_CST) != 0;
}

static void exec(spn_op_t* op) {
  if (spn_op_cancelled(op)) {
    op->result.err = SPN_ERR_CANCELLED;
  }
  else {
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

  sp_atomic_u32_store(&op->done, 1, SP_ATOMIC_RELEASE);
  spn_wake_pulse(&op->ctx->wake);
}

static s32 op_thread(void* data) {
  spn_ctx_t* ctx = (spn_ctx_t*)data;

  while (true) {
    sp_queue_node_t* node = sp_queue_wait(&ctx->ops.queue);
    if (node) {
      exec((spn_op_t*)node);
    }
    else if (!sp_atomic_u32_load(&ctx->ops.running, SP_ATOMIC_ACQUIRE)) {
      break;
    }
  }

  spn_wasm_thread_exit();
  return 0;
}

void spn_op_thread_start(spn_ctx_t* ctx) {
  sp_queue_init(&ctx->ops.queue);
  sp_atomic_u32_store(&ctx->ops.running, 1, SP_ATOMIC_RELEASE);
  sp_thread_init(&ctx->ops.thread, op_thread, ctx);
}

void spn_op_thread_stop(spn_ctx_t* ctx) {
  sp_atomic_u32_store(&ctx->ops.running, 0, SP_ATOMIC_RELEASE);
  sp_queue_wake(&ctx->ops.queue);
  sp_thread_join(&ctx->ops.thread);
}

void spn_op_submit(spn_op_t* op) {
  sp_queue_push(&op->ctx->ops.queue, &op->node);
}

bool spn_op_done(spn_op_t* op) {
  return sp_atomic_u32_load(&op->done, SP_ATOMIC_ACQUIRE) != 0;
}

spn_op_result_t spn_op_result(spn_op_t* op) {
  sp_assert(spn_op_done(op));
  return op->result;
}

void spn_op_free(spn_op_t* op) {
}
